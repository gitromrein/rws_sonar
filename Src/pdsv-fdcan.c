/*
 * File:   pdsv-fdcan.c
 * Author: am
 * 10.01.2021
 */


#include "pdsv-parameters.h"
#include "pdsv-fdcan.h"
#include "pdsv-diagnostics.h"
#include "pdsv-ipk.h"
#include "helpers.h"

/* Private defines*/
#define IDENTIFIER  ((32)) 							// Module internal identifier  // DSV2 22	//29d DSIV		// DBIV 31 // PDSV 32
#define MAX_CAN_RATE_1MS   		10      			// 10ms
#define CAN_CYCLE_1MS			(CAN_CYCLE_10MS*10)	// 300ms
#define CAN_TIMEOUT_100MS    	30      			// 3 seconds

/* Public Variables */
FdcanMessageRam_t *pxgFdcanMessageRam = (FdcanMessageRam_t*) SRAMCAN_BASE;
CanMessage_t xgCanMessageRx[CAN_BUF_COUNT];

Imk_t xgImkTx;
Imk_t xgImkRx;

uint8_t ui8gCircularHeadIndex = 0;

/* Private variables */
static uint8_t ui8CircularTailIndex = 0;

/**-------------------------------------------------------------------------------------------------
 *    Description: Check if Can timed out. 008-CAN-Frame has to be received periodically /SR0007
 *    Frequency:  100ms
 *    Parameters: pui8CheckinCounter	-	Scheduler Counter
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vCanCheckTimeout(uint8_t *pui8CheckinCounter) {
	++(*pui8CheckinCounter);

	if (xgErrorCounters.ui8ErrorCounterCanTimeout < CAN_TIMEOUT_100MS) {
		++xgErrorCounters.ui8ErrorCounterCanTimeout;
	} else {
		xgIpkTx.xErrors0.ui1ErrorCanTimeout = TRUE;
	}

}

/**-------------------------------------------------------------------------------------------------
 *    Description: Receive buffered 008 CAN Frames
 *    Frequency:  highest priority
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vCanReceiveFrame(void) {

	if (ui8CircularTailIndex != ui8gCircularHeadIndex) {

		// Die zwischengespeicherten Messages vollends abarbeiten
		// DLC == 2, DNCO
		if (xgCanMessageRx[ui8CircularTailIndex].ui16Size == 2) {
			xgIpkTx.ui8DncoIndex = xgCanMessageRx[ui8CircularTailIndex].ui8Data[0];

			// Second Index is not used for pdsv
			(void) xgCanMessageRx[ui8CircularTailIndex].ui8Data[1];

			//DLC == 8, State Data
		} else if (xgCanMessageRx[ui8CircularTailIndex].ui16Size == 8) {

			// Set local data
			memcpy((uint8_t*) &xgImkRx, (uint8_t*) &xgCanMessageRx[ui8CircularTailIndex].ui8Data, 8);

			// Update operating mode
			xgIpkTx.xModifiers.ui4Operatingstate = xgImkRx.xZmvModifiersIn.ui4Operatingstate;
			xgIpkTx.xModifiers.ui2Validationbits = xgImkRx.xZmvValidationIn.ui2Validationbits;

			// Receive Slow Parameters here
			//---------------------------------------------------------------------
			if (xgImkRx.xHeader.ui1ReceptionInProgress && !xgIpkTx.xStatus.ui1ParameterComplete) {
				if (xgImkRx.ui8ParametersIndex <= 0x0B) {
					xgImkTx.xHeader.ui1ReceptionInProgress = TRUE;

					uint8_t ui8Index = (xgImkRx.ui8DiagnosticsIndex << 1);                   		// Pro indexwert 2 Parameterwerte
					xgReceivedParameters.rui8Buffer[ui8Index] = xgImkRx.ui8Parameters0;   			// CanRX-byte 6  in Parametertabelle 0 bis 24
					xgReceivedParameters.rui8Buffer[ui8Index + 1] = xgImkRx.ui8Parameters1;      	// CanRX-byte 7  in Parametertabelle

					// If it is the last byte
					if (xgImkRx.ui8ParametersIndex == 0x0B) {
						xgImkTx.xHeader.ui1ReceptionInProgress = FALSE;
						xgIpkTx.xStatus.ui1ParameterComplete = TRUE;

						// Validate and check Parameters
						// /SR0003 /SR0003
						vParameterCheckDesignerChecksum();
						vParameterCheckPlausibility();

					}
				}
			}
		}

		// Receiving a frame resets the counter
		xgErrorCounters.ui8ErrorCounterCanTimeout = 0;
		ui8CircularTailIndex = (ui8CircularTailIndex + 1) % CAN_BUF_COUNT;
	}
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Send out frames to ZMV
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vCanTransmitFrame(uint8_t *pui8CheckinCounter) {
	static uint32_t ui32LastRelevantState = 0;
	static uint8_t ui8CounterSlowdownCanTX0 = MAX_CAN_RATE_1MS;

	static uint16_t ui16TimerCanTX0 = CAN_CYCLE_1MS;
	static uint16_t ui16TimerCanTX1 = CAN_CYCLE_1MS;

	++(*pui8CheckinCounter);

	// Update Can Bytes according to local ipk
	xgImkTx.xHeader.ui1nSlok = xgIpkTx.xStatus.ui1nSlok;
	xgImkTx.xHeader.ui5Identifier = IDENTIFIER;


	xgImkTx.xInputsOut.ui4Inputs = xgIpkTx.xInputs.ui4Inputs;

#ifndef DEBUG_DISABLE_DUALCHANNEL_AND
	xgImkTx.xSpeedOut.ui4Q = xgIpkTx.xSpeedEmk.ui4Q & xgIpkRx.xSpeedEmk.ui4Q;
	xgImkTx.xSafetyCircuitOut.ui1Skr1 = xgIpkTx.xSafetyCircuit.ui1Skr1 & xgIpkRx.xSafetyCircuit.ui1Skr1;
	xgImkTx.xSafetyCircuitOut.ui1Skr2 = xgIpkTx.xSafetyCircuit.ui1Skr2 & xgIpkRx.xSafetyCircuit.ui1Skr2;
	xgImkTx.xSafetyCircuitOut.ui1Zh = xgIpkTx.xSafetyCircuit.ui1Zh & xgIpkRx.xSafetyCircuit.ui1Zh;
	xgImkTx.xEmkOut.ui1Emk = xgIpkTx.xSpeedEmk.ui1Emk & xgIpkRx.xSpeedEmk.ui1Emk & xgIpkTx.xSpeedEmk.ui1EmkReady & xgIpkRx.xSpeedEmk.ui1EmkReady;
	xgImkTx.xEmkOut.ui1Error = (xgIpkTx.ui8Errors2!=0);
#else
	xgImkTx.xSpeedOut.ui4Q = xgIpkTx.xSpeedEmk.ui4Q;
	xgImkTx.xSafetyCircuitOut.ui1Skr1 = xgIpkTx.xSafetyCircuit.ui1Skr1;
	xgImkTx.xSafetyCircuitOut.ui1Skr2 = xgIpkTx.xSafetyCircuit.ui1Skr2;
	xgImkTx.xSafetyCircuitOut.ui1Zh = xgIpkTx.xSafetyCircuit.ui1Zh;
	xgImkTx.xEmkOut.ui1Emk = xgIpkTx.xSpeedEmk.ui1Emk & xgIpkTx.xSpeedEmk.ui1EmkReady;
	xgImkTx.xEmkOut.ui1Error = (xgIpkTx.ui8Errors2!=0);
#endif

	//	# 1. Input has changed
	//	# 2. Something has changed, then wait at least 10 ms
	//	# 3. When nothing has changed, after every 300 ms
	// Max update rate is 10 ms
	if (ui8CounterSlowdownCanTX0 > 0)
		ui8CounterSlowdownCanTX0--;

	// Trigger, last state has changed
	if (ui8CounterSlowdownCanTX0 == 0 && (ui32LastRelevantState != *((uint32_t*) &xgImkTx))) {
		ui16TimerCanTX0 = 0;
	}

	// One of the blocks execute. If two events happen simultaneously
	// type 0 has priority and the next one is executed after 1 ms
	// Trigger 300 ms are over
	if (ui16TimerCanTX0 > 0) {
		ui16TimerCanTX0--;
	}

	if (xgIpkTx.xStatus.ui1ParameterComplete && ui16TimerCanTX1 > 0) {
		ui16TimerCanTX1--;
	}

	// Executes normally every 300ms
	if (!ui16TimerCanTX0) {
		ui8CounterSlowdownCanTX0 = MAX_CAN_RATE_1MS;		// Limit the max rate to 10 ms, to avoid thrashing the can bus
		ui16TimerCanTX0 = CAN_CYCLE_1MS;

		// Execute Diagnostics
		vDiagnosticsExport();

		// Update Last state
		ui32LastRelevantState = *((uint32_t*) &xgImkTx);

		pxgFdcanMessageRam->rxTxFifo[0].rui32Fifo[0] = *((uint32_t*) &xgImkTx);
		pxgFdcanMessageRam->rxTxFifo[0].rui32Fifo[1] = *(((uint32_t*) &xgImkTx) + 1);

		FDCAN1->TXBAR = 0x01;
	}

	// Executes normally every 300ms
	else if (!ui16TimerCanTX1) {
		ui16TimerCanTX1 = (xgRuntimeParameters.ui8GeneralCanRefreshTime * 10);	// Multipliziert wegen dem 10ms Zeitraster


		// Positionsüberwachung1 32 bits
		//-----------------------------------------------------------------------------------------
		DEBUGCODE({
		uint32_t ui32Temp = ui32ConvertToBigEndian(ui32ConvertToBcd(xgRuntimeState.xSpeed.ui32MasterFrequency));
		uint32_t ui32Temp2 = ui32ConvertToBigEndian(ui32ConvertToBcd(xgDebugStatistics.xTime1.ui32WCET));
		uint32_t ui32Temp3 = ui32ConvertToBigEndian(ui32ConvertToBcd(xgDebugStatistics.xTime1.ui32WCETLatest));

		uint32_t ui32Temp4 = (ui32Temp3)|(ui32Temp2>>16);


		uint32_t ui32EmkVolatage;
		if (xgRuntimeParameters.ui16EmkSensitivityVoltage > EMK_GAIN_SWITCH_VOLTAGE) {
			ui32EmkVolatage = ((uint32_t)xgIpkTx.ui8EmkVoltage<<2) * EMK_3V3_ANALOG_REF / (EMK_3V3_DIGITAL_REF*EMK_GAIN_LOW);
		} else {
			ui32EmkVolatage = ((uint32_t)xgIpkTx.ui8EmkVoltage<<2) * EMK_3V3_ANALOG_REF / (EMK_3V3_DIGITAL_REF*EMK_GAIN_HIGH);

		}
		uint32_t ui32EMKTemp = ui32ConvertToBigEndian((uint32_t)xgRuntimeParameters.ui16EmkHysteresisOn|((uint32_t)xgRuntimeParameters.ui16EmkHysteresisOff<<16));

		uint32_t ui32SpeedRx = ui32ConvertToBigEndian(ui32ConvertToBcd(xgIpkRx.ui8FrequencyHighbyte<<8|xgIpkRx.ui8FrequencyLowbyte));
		uint32_t ui32SpeedTx = ui32ConvertToBigEndian(ui32ConvertToBcd(xgIpkTx.ui8FrequencyHighbyte<<8|xgIpkTx.ui8FrequencyLowbyte));



		pxgFdcanMessageRam->rxTxFifo[1].rui32Fifo[0] = ui32EMKTemp;					//
		pxgFdcanMessageRam->rxTxFifo[1].rui32Fifo[1] = ui32ConvertToBigEndian(xgRuntimeState.xEmk.ui32EmkDelta);//ui32ConvertToBigEndian(xgRuntimeParameters.ui16EmkSensitivityVoltage);	//(((uint32_t) xgIpkTx.ui16Checksum)) << 16 | (xgIpkRx.ui16Checksum);		// Check if this is big endian

		});
		//Positionsüberwachung2 32 bits
		FDCAN1->TXBAR = 0x02;
	}
}
