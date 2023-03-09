/*
 * File:   pdsv-diagnostics.c
 * Author: am
 * 01.09.2021
 */


#include "pdsv-diagnostics.h"

#include "pdsv-fdcan.h"
#include "pdsv-ipk.h"
#include "safe/safe-circuits.h"
#include "pdsv-parameters.h"

/* Private defines */
#define SLOCKOFF_ERROR_COUNT 9
#define ERROR_FW_VERSION_100MS		3				// 300 ms

//Typenschild Informationen
#define HARDWARE_VER        0x22
#define FW_VER_1            'V'
#define FW_VER_2            '1'
#define FW_VER_3            '.'
#define FW_VER_4            '1'

#define ARTICLE_1    		'P'
#define ARTICLE_2    		'V'
#define ARTICLE_3    		'0'
#define ARTICLE_4    		'1'


#define TIMER_INCREMENT_1000us	1000

/* Public Variables */
#ifdef DEBUG
DebugStatistics_t xgDebugStatistics;
#endif

// Typenschild
const uint8_t rui8gEds[EDS_SIZE] = { HARDWARE_VER, FW_VER_1, FW_VER_2, FW_VER_3, FW_VER_4, ARTICLE_1, ARTICLE_2, ARTICLE_3, ARTICLE_4, 0, 0, 0, 0, 0, 0, 0 };

// Zustand
RuntimeState_t xgRuntimeState;
ErrorCounters_t xgErrorCounters;
HardwareErrors_t xgHardwareErrors;



// WARNING! This should be disabled for production, because the timer is being stopped.
/*-----------------------------------------------------------------------------------------
 *    Description: Measures time relative to the last call
 *    Frequency:  as fast as possible
 *    Parameters: None
 *    Return: None
 *----------------------------------------------------------------------------------------*/
inline void vDebugTimer1WCET(void) {
DEBUGCODE({

	xgDebugStatistics.xTime1.ui32TimerC = TIM3->CNT;
	xgDebugStatistics.xTime1.ui32TimerDifference = (xgDebugStatistics.xTime1.ui32TimerC - xgDebugStatistics.xTime1.ui32TimerP)&0xFFFF;
	xgDebugStatistics.xTime1.ui32TimerP = xgDebugStatistics.xTime1.ui32TimerC;

	// WCET
	//-----------------------------------------------------------------------------------
	if (xgDebugStatistics.xTime1.ui32TimerDifference > xgDebugStatistics.xTime1.ui32WCET)
		xgDebugStatistics.xTime1.ui32WCET = xgDebugStatistics.xTime1.ui32TimerDifference;
	if (xgDebugStatistics.xTime1.ui32TimerDifference > xgDebugStatistics.xTime1.ui32WCETLatest) {
		xgDebugStatistics.xTime1.ui32WCETLatest = xgDebugStatistics.xTime1.ui32TimerDifference;
	}
	if (xgDebugStatistics.xTime1.ui32TimerDifference > 0) {
		++xgDebugStatistics.xTime1.ui32TimerCount;
		xgDebugStatistics.xTime1.ui32TimerSum += xgDebugStatistics.xTime1.ui32TimerDifference;
		if (xgDebugStatistics.xTime1.ui32TimerSum > 0x00000FFFF) {
			xgDebugStatistics.xTime1.ui32TimerSum = 0;
			xgDebugStatistics.xTime1.ui32TimerCount = 0;
			xgDebugStatistics.xTime1.ui32WCETLatest = 0;
		} else {
			xgDebugStatistics.xTime1.ui32TimerAverage = xgDebugStatistics.xTime1.ui32TimerSum / xgDebugStatistics.xTime1.ui32TimerCount;
		}
	}
});
}


/*-----------------------------------------------------------------------------------------
 *    Description: Measures time relative to timer interrupt.
 *    Frequency:  after 1ms
 *    Parameters: None
 *    Return: None
 *----------------------------------------------------------------------------------------*/
inline void vDebugTimer2Start(){
	DEBUGCODE({
	xgDebugStatistics.xTime2.ui32TimerC = TIM3->CNT;
	});
}

/*-----------------------------------------------------------------------------------------
 *    Description: Measures time relative to timer interrupt.
 *    Frequency:  after 1ms
 *    Parameters: None
 *    Return: None
 *----------------------------------------------------------------------------------------*/
inline void vDebugTimer2Stop(){
	DEBUGCODE({
	xgDebugStatistics.xTime2.ui32TimerDifference = (TIM3->CNT - xgDebugStatistics.xTime2.ui32TimerC)&0xFFFF;

	if(xgDebugStatistics.xTime2.ui32TimerDifference>xgDebugStatistics.xTime2.ui32WCET){
		xgDebugStatistics.xTime2.ui32WCET = xgDebugStatistics.xTime2.ui32TimerDifference;
	}

	});
}

/*-----------------------------------------------------------------------------------------
 *    Description: Reset all possible errors, counters and latch information.
 *    Frequency:  RTSK quitt and during initialization
 *    Parameters: pui8CheckinCounter	-	Scheduler Counter
 *    Return: None
 *----------------------------------------------------------------------------------------*/
void vDiagnosticsResetErrors(void) {
	memset((uint8_t*) &xgErrorCounters, 0, sizeof(xgErrorCounters));

	// Delete normal errors
	xgIpkTx.ui8Errors0 = 0;
	xgIpkTx.ui8Errors1 = 0;
	xgIpkTx.ui8Errors2 = 0;
	xgIpkTx.ui8SafetyCircuitErrors = 0;

	// Reset Safety Circuit
	vSafetyCircuitResetManagement();
	vIpkClearLatchBuffers();

	// Slok
	//xgIpkTx.xStatus.ui1SlockOff = FALSE;	// NOTE! CAN General-Error-Flag zurücksetzen

}


/*-------------------------------------------------------------------------------------------
 *    Description: Defines the result of a detected error or acknowledged error
 *    Frequency:  1ms
 *    Parameters: pui8CheckinCounter	-	Scheduler Counter
 *    Return: None
 *-------------------------------------------------------------------------------------------*/
void vDiagnosticsHandleErrors(uint8_t *pui8CheckinCounter) {
	static uint8_t ui8StateAcknowledgeErrors = 0;

	++(*pui8CheckinCounter);
	switch (ui8StateAcknowledgeErrors) {
	case 0:
		vDiagnosticsResetErrors();
		ui8StateAcknowledgeErrors = 1;
		break;
	case 1:
		// Wait for rising edge RTSK
		if (xgImkRx.xZmvValidationIn.ui1RTSK)
			ui8StateAcknowledgeErrors = 2;
		break;
	case 2:
		// Wait for falling edge RTSK
		if (!xgImkRx.xZmvValidationIn.ui1RTSK)
			ui8StateAcknowledgeErrors = 3;
		break;
	case 3:
		ui8StateAcknowledgeErrors = 1;
		vDiagnosticsResetErrors();
		break;
	}

	// SLOK Off Errors
	// SRS Errors
#ifdef DEBUG_DISABLE_SLOKOFF
	xgIpkTx.xStatus.ui1nSlok = FALSE;
#else

	if (xgIpkTx.xErrors0.ui1ErrorVersion || xgIpkTx.xErrors1.ui1ErrorParameter || xgIpkTx.xErrors0.ui1ErrorParameterChecksum || xgIpkTx.xErrors1.ui1ErrorParameterCopy || xgIpkTx.xErrors0.ui1ErrorIpk
			|| xgIpkTx.xErrors0.ui1ErrorCanTimeout || xgIpkTx.xErrors0.ui1ErrorVoltage3v3 || xgIpkRx.xErrors0.ui1ErrorVoltage3v3 || xgIpkTx.xErrors0.ui1ErrorVoltage1v65
			|| xgIpkTx.xErrors0.ui1ErrorChecksumExchange || xgIpkTx.xErrors1.ui1ErrorWireBreakageTestSignal || xgIpkTx.xErrors1.ui1ErrorAdcTimeout||xgIpkRx.xStatus.ui1nSlok) {
		xgIpkTx.xStatus.ui1nSlok = TRUE;
	}
#endif
}

/*-------------------------------------------------------------------------------------------
 *    Description: FW Versions Check /SR0001
 *    Frequency:  100ms
 *    Parameters: pui8CheckinCounter	-	Scheduler Counter
 *    Return: None
 *-------------------------------------------------------------------------------------------*/
void vDiagnosticsCheckFirmwareVersion(uint8_t *pui8CheckinCounter) {
	++(*pui8CheckinCounter);

	if (xgIpkTx.ui8Version != xgIpkRx.ui8Version) {
		++xgErrorCounters.ui8ErrorCounterVersion;
	} else if (xgErrorCounters.ui8ErrorCounterVersion > 0) {
		--xgErrorCounters.ui8ErrorCounterVersion;
	}

	if (xgErrorCounters.ui8ErrorCounterVersion > ERROR_FW_VERSION_100MS) {
		xgIpkTx.xErrors0.ui1ErrorVersion = TRUE;
	}
}

/*-------------------------------------------------------------------------------------------
 *    Description: Diagnostics information for Designer
 *    Frequency:  300ms
 *    Parameters:
 *    Return: None
 *-------------------------------------------------------------------------------------------*/
void vDiagnosticsExport(void) {
	static uint8_t* pui8CurrentIpkBuffer;
	static uint8_t ui8IndexEds = 0;
	static uint8_t ui8BufferSelector = 0;
	uint8_t ui8IpkIndex;

	++xgImkTx.ui8DiagnosticsIndex;

	if (xgImkTx.ui8DiagnosticsIndex >= 12) {
		xgImkTx.ui8DiagnosticsIndex = 0;

		// Checking 3 types
		ui8BufferSelector = (ui8BufferSelector+1)%3;

		// Case1 Is: there a latched frame from master? next otherwise
		if(ui8BufferSelector == FRAMETYPE_LATCH_SLAVE){
			if(rui8gLatchedIpkBufferSlave[0] & FRAMETYPE_LATCH_SLAVE){
				pui8CurrentIpkBuffer = rui8gLatchedIpkBufferSlave;
			} else {
				ui8BufferSelector = 2;
			}
		}

		// Case 2: Is there a latched frame from master? next otherwise
		if(ui8BufferSelector == FRAMETYPE_LATCH_MASTER){
			if(rui8gLatchedIpkBufferMaster[0] & FRAMETYPE_LATCH_MASTER){
				pui8CurrentIpkBuffer = rui8gLatchedIpkBufferMaster;
			} else {
				ui8BufferSelector = 0;
			}
		}

		// Case 3:Switch to current Ipk State
		if(!ui8BufferSelector){
			pui8CurrentIpkBuffer = (uint8_t*)(&xgIpkTx);
		}
	}

	// Ipk Diagnostics
	if (xgImkTx.ui8DiagnosticsIndex != 10) {
		ui8IpkIndex = xgImkTx.ui8DiagnosticsIndex;

		xgImkTx.ui8Diagnostics0 = *(pui8CurrentIpkBuffer + (ui8IpkIndex<<1));
		xgImkTx.ui8Diagnostics1 = *(pui8CurrentIpkBuffer + (ui8IpkIndex<<1)+1);

		// EDS
	} else {
		xgImkTx.ui8Diagnostics0 = ui8IndexEds;
		xgImkTx.ui8Diagnostics1 = rui8gEds[ui8IndexEds];

		ui8IndexEds = (ui8IndexEds+1)%EDS_SIZE;

	}
}
