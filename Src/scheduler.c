/*
 *  scheduler.c
 *
 *  Created on: 19.07.2021
 *      Author: am
 */


#include "scheduler.h"
#include "safe/safe-circuits.h"
#include "safe/safe-speed.h"
#include "safe/safe-standstill.h"
#include "pdsv-parameters.h"
#include "pdsv-voltage.h"
#include "pdsv-ipk.h"
#include "pdsv-inputs.h"
#include "pdsv-fdcan.h"
#include "pdsv-diagnostics.h"
#include "pdsv-memorytest.h"
#include "pdsv-hardware.h"

/* Private prototypes */
static inline void vSchedulerAssignFlags(void);
static inline void vSchedulerDeassignFlags(void);

/****************************************************
 *
 * 			Scheduler: Execution configuration
 *
 ****************************************************/
const func pvSubroutines1ms[] = {

		vDiagnosticsHandleErrors,								// Execute Error Handling
		vAdcFetchValues,							// Read Adc
		vInputSample,								// Read Inputs
		vParameterCheckComplete,					// Check if parameters are complete

		vAdcCheckVoltage,							// SR0008, SR0009, SR0010 WARNING! Has to executed after vFetchAdcValues
		vAdcCheckWirebreakage,					// SR0015 WARNING! Has to executed after vFetchAdcValues
		vAdcCheckExtendedWirebreakage,			// SR0021
		vAdcCheckDigitalAnalogCoupling,				// SR00012 WARNING! Has to executed after vFetchAdcValues
		vIpkCheckTimeout,								// SR0006
		vSafetyCircuitCheckCrosscircuit,							// Sicherheitskreise Querschluss
		vSpeedExecute,								// Evaluation functions
		vEmkExecute,								// Evaluation functions
		vCanTransmitFrame,							// Exchange Information

		};

const func pvSubroutines10ms[] = {

		vSafetyCircuitExecute,						// Sicherheitskreise
		vTwohandCircuitExecute,					// Zweihand
		vRomCheckContinous,							// SR0024
		vRamCheckContinous							// SR0025
		};

const func pvSubroutines100ms[] = {

		vDiagnosticsCheckFirmwareVersion,					// SR0001
		vInputCheckPort,							// SR0005
		vCanCheckTimeout,						// SR0007
		vParameterCheckCopies,					// SR0004
		vParameterCheckExchangedChecksum		// SR0018

		};

// Scheduler paces
#define SCHEDULER_SUBROUTINE_COUNT0		SIZEOF(pvSubroutines1ms)
#define SCHEDULER_FLAG_PLACE0			1

#define SCHEDULER_SUBROUTINE_COUNT1		SIZEOF(pvSubroutines10ms)
#define SCHEDULER_FLAG_PLACE1			10

#define SCHEDULER_SUBROUTINE_COUNT2		SIZEOF(pvSubroutines100ms)
#define SCHEDULER_FLAG_PLACE2			100

#if defined(SCHEDULER_SUBROUTINE_COUNT0) && defined(SCHEDULER_SUBROUTINE_COUNT1) && defined(SCHEDULER_SUBROUTINE_COUNT2)
#define SCHEDULER_FLAG_COUNT			3
#elif defined(SCHEDULER_SUBROUTINE_COUNT0) && defined(SCHEDULER_SUBROUTINE_COUNT1)
	#define SCHEDULER_FLAG_COUNT			2

#elif defined(SCHEDULER_SUBROUTINE_COUNT0)
	#define SCHEDULER_FLAG_COUNT			1
#endif

#if SCHEDULER_FLAG_COUNT>4
#error "Error:Scheduler not implemented for the count"
#endif

/* Private datatypes */
typedef struct {
	uint8_t ui8ScheduleFlags;
	uint8_t ui8ScheduleCounters[SCHEDULER_FLAG_COUNT];
} SoftTimer_t;

typedef struct {
	const func *pvSubroutines[SCHEDULER_FLAG_COUNT];
	uint8_t ui8SubroutineCallCount[SCHEDULER_FLAG_COUNT];
} SubroutineBlock_t;

typedef struct {
	SoftTimer_t xSoftTimer;
	SubroutineBlock_t xSubroutineBlock;
} SchedulerBlock_t;

// Scheduler object
SchedulerBlock_t xSchedulerBlock;


/*-------------------------------------------------------------------------------------------------
 *    Description: Defines scheduler object
 *    Frequency:  once
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
inline void vSchedulerInitialize(void) {
	memset((uint8_t*) &xSchedulerBlock, 0, sizeof(SchedulerBlock_t));
#if SCHEDULER_FLAG_COUNT>0
	xSchedulerBlock.xSubroutineBlock.pvSubroutines[0] = pvSubroutines1ms;
	xSchedulerBlock.xSoftTimer.ui8ScheduleCounters[0] = 0;
#endif

#if SCHEDULER_FLAG_COUNT>1
	xSchedulerBlock.xSubroutineBlock.pvSubroutines[1] = pvSubroutines10ms;
	xSchedulerBlock.xSoftTimer.ui8ScheduleCounters[1] = 0;
#endif

#if SCHEDULER_FLAG_COUNT>2
	xSchedulerBlock.xSubroutineBlock.pvSubroutines[2] = pvSubroutines100ms;
	xSchedulerBlock.xSoftTimer.ui8ScheduleCounters[2] = 0;
#endif

#if SCHEDULER_FLAG_COUNT>3
	xSchedulerBlock.xSubroutineBlock.pvSubroutines[3] = pvSubroutines1ms;
	xSchedulerBlock.xSoftTimer.ui8ScheduleCounters[3] = 0;
#endif

}


/*-------------------------------------------------------------------------------------------------
 *    Description: Set Scheduler flags
 *    Frequency:  8us -~1ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
static inline void vSchedulerAssignFlags() {

	if (ui8gHardwareTimerEvent) {
		ui8gHardwareTimerEvent = FALSE;

#if SCHEDULER_FLAG_COUNT >0
		if (++xSchedulerBlock.xSoftTimer.ui8ScheduleCounters[0] >= SCHEDULER_FLAG_PLACE0) {
			xSchedulerBlock.xSoftTimer.ui8ScheduleCounters[0] = 0;
			xSchedulerBlock.xSoftTimer.ui8ScheduleFlags |= 0x01;
		}

#endif

#if SCHEDULER_FLAG_COUNT>1
		if (++xSchedulerBlock.xSoftTimer.ui8ScheduleCounters[1] >= SCHEDULER_FLAG_PLACE1) {
			xSchedulerBlock.xSoftTimer.ui8ScheduleCounters[1] = 0;
			xSchedulerBlock.xSoftTimer.ui8ScheduleFlags |= 0x02;
		}

#endif

#if SCHEDULER_FLAG_COUNT >2
		if (++xSchedulerBlock.xSoftTimer.ui8ScheduleCounters[2] >= SCHEDULER_FLAG_PLACE2) {
			xSchedulerBlock.xSoftTimer.ui8ScheduleCounters[2] = 0;
			xSchedulerBlock.xSoftTimer.ui8ScheduleFlags |= 0x04;
		}

#endif

#if SCHEDULER_FLAG_COUNT >3
		if (++xSchedulerBlock.xSoftTimer.ui8ScheduleCounters[3] >= SCHEDULER_FLAG_PLACE3){
			xSchedulerBlock.xSoftTimer.ui8ScheduleCounters[3] = 0;
			xSchedulerBlock.xSoftTimer.ui8ScheduleFlags |= 0x08;
		}

		#endif
	}
}


/**-------------------------------------------------------------------------------------------------
 *    Description: 	Execute Routines
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
inline void vSchedulerExecute(void) {

	uint8_t ui8Index;

	// Scheduling Tasks to predefined timeslots
	vSchedulerAssignFlags();

#if SCHEDULER_FLAG_COUNT >0
	if (xSchedulerBlock.xSoftTimer.ui8ScheduleFlags & 0x01) {
		for (ui8Index = 0; ui8Index < SCHEDULER_SUBROUTINE_COUNT0; ++ui8Index) {
			(xSchedulerBlock.xSubroutineBlock.pvSubroutines[0][ui8Index])(&xSchedulerBlock.xSubroutineBlock.ui8SubroutineCallCount[0]);
		}
	}
#endif

#if SCHEDULER_FLAG_COUNT >1
	if (xSchedulerBlock.xSoftTimer.ui8ScheduleFlags & 0x02) {
		for (ui8Index = 0; ui8Index < SCHEDULER_SUBROUTINE_COUNT1; ++ui8Index) {
			(xSchedulerBlock.xSubroutineBlock.pvSubroutines[1][ui8Index])(&xSchedulerBlock.xSubroutineBlock.ui8SubroutineCallCount[1]);
		}
	}
#endif

#if SCHEDULER_FLAG_COUNT >2
	if (xSchedulerBlock.xSoftTimer.ui8ScheduleFlags & 0x04) {
		for (ui8Index = 0; ui8Index < SCHEDULER_SUBROUTINE_COUNT2; ++ui8Index) {
			(xSchedulerBlock.xSubroutineBlock.pvSubroutines[2][ui8Index])(&xSchedulerBlock.xSubroutineBlock.ui8SubroutineCallCount[2]);
		}
	}
#endif

#if SCHEDULER_FLAG_COUNT >3
	if(xSchedulerBlock.xSoftTimer.ui8ScheduleFlags&0x08){
		for(ui8Index=0;ui8Index<SCHEDULER_SUBROUTINE_COUNT3;++ui8Index){
			(xSchedulerBlock.xSubroutineBlock.pvSubroutines[3][ui8Index])(&xSchedulerBlock.xSubroutineBlock.ui8SubroutineCallCount[3]);
		}
	}
	#endif
	// Validate execution of all functions
	vSchedulerDeassignFlags();

}

/*-------------------------------------------------------------------------------------------------
 *    Description: Deassigns Scheduler flags	/SR0029
 *    Frequency:  8us -~1ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
static inline void vSchedulerDeassignFlags() {
#if SCHEDULER_FLAG_COUNT>0
	if (xSchedulerBlock.xSoftTimer.ui8ScheduleFlags & 0x01) {
		// DEBUG_PIN_OFF; max execution measurement
		if (xSchedulerBlock.xSubroutineBlock.ui8SubroutineCallCount[0] != SCHEDULER_SUBROUTINE_COUNT0) {
			xgHardwareErrors.ui8ErrorSubroutineCalls = ERROR_HARDWARE_PGM_1MS;
		}
		xSchedulerBlock.xSoftTimer.ui8ScheduleCounters[0] = 0;
		xSchedulerBlock.xSubroutineBlock.ui8SubroutineCallCount[0] = 0;

	}
#endif

#if SCHEDULER_FLAG_COUNT>1
	if (xSchedulerBlock.xSoftTimer.ui8ScheduleFlags & 0x02) {
		if (xSchedulerBlock.xSubroutineBlock.ui8SubroutineCallCount[1] != SCHEDULER_SUBROUTINE_COUNT1) {
			xgHardwareErrors.ui8ErrorSubroutineCalls = ERROR_HARDWARE_PGM_10MS;
		}
		xSchedulerBlock.xSoftTimer.ui8ScheduleCounters[1] = 0;
		xSchedulerBlock.xSubroutineBlock.ui8SubroutineCallCount[1] = 0;

	}
#endif

#if SCHEDULER_FLAG_COUNT>2
	if (xSchedulerBlock.xSoftTimer.ui8ScheduleFlags & 0x04) {
		if (xSchedulerBlock.xSubroutineBlock.ui8SubroutineCallCount[2] != SCHEDULER_SUBROUTINE_COUNT2) {
			xgHardwareErrors.ui8ErrorSubroutineCalls = ERROR_HARDWARE_PGM_100MS;	// 100ms pace
		}
		xSchedulerBlock.xSoftTimer.ui8ScheduleCounters[2] = 0;
		xSchedulerBlock.xSubroutineBlock.ui8SubroutineCallCount[2] = 0;

	}
#endif

#if SCHEDULER_FLAG_COUNT>3
	if(xSchedulerBlock.xSoftTimer.ui8ScheduleFlags&0x08){
		if(xSchedulerBlock.xSubroutineBlock.ui8SubroutineCallCount[3] != SCHEDULER_SUBROUTINE_COUNT3){
			xgHardwareErrors.ui8ErrorSubroutineCalls = ERROR_HARDWARE_PGM_100MS;
		}
		xSchedulerBlock.xSoftTimer.ui8ScheduleCounters[3] = 0;
		xSchedulerBlock.xSubroutineBlock.ui8SubroutineCallCount[3] = 0;

	}
#endif

	xSchedulerBlock.xSoftTimer.ui8ScheduleFlags = 0;
	if (xgHardwareErrors.ui8ErrorSubroutineCalls) {
		vHardwareExecuteErrorLoop(xgHardwareErrors.ui8ErrorSubroutineCalls);
	}

}
