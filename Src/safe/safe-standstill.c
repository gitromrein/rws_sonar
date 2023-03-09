/*
 *  safe-standstill.c
 *
 *  Created on: Aug 5, 2020
 *      Author: am
 */

#include "safe/safe-standstill.h"
#include "pdsv-parameters.h"
#include "pdsv-ipk.h"
#include "pdsv-diagnostics.h"
#include "pdsv-hardware.h"
#include "safe/safe-speed.h"

#define MIN_VOLTAGE_EMK 		(200*1023/3300)
#define MAX_VOLTAGE_EMK 		(3100*1023/3300)

#define ERROR_EMK_VOLTAGE_UPPER (3000*255/3300)
#define ERROR_EMK_VOLTAGE_LOWER (100*255/3300)

#define ERROR_EMK_SINGLE_1MS	3000
#define PARAM_EMK_DELAY_STEP 	250			// ms
#define PARAM_EMK_VOLTAGE_STEP	3649		// 5 mV

#define ADC_VOLTAGE_MAX 0x3FF
#define ADC_VOLTAGE_MIN 0x000
#define PERIOD_MASK 0x1F
#define PERIOD_COUNT 50

enum {
	EMK_STATE_PAUSE, EMK_STATE_EVALUATE, EMK_STATE_CAPTURE
};

/*-------------------------------------------------------------------------------------------------
 *    Description: Check EMK is okay /SR0017
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: 	  None
 *    Changes:	  2022-04-21 	Verschärfung auf 10 Hz
 *-------------------------------------------------------------------------------------------------*/
void vEmkCheckSingle(void) {

	if (xgRuntimeState.xSpeed.ui32MasterFrequency > FREQ_10HZ) {

		if (((xgIpkTx.ui8EmkVoltage >= ERROR_EMK_VOLTAGE_UPPER) && (xgIpkRx.ui8EmkVoltage <= ERROR_EMK_VOLTAGE_LOWER))
				|| ((xgIpkRx.ui8EmkVoltage >= ERROR_EMK_VOLTAGE_UPPER) && (xgIpkTx.ui8EmkVoltage <= ERROR_EMK_VOLTAGE_LOWER))) {
			if (xgErrorCounters.ui8ErrorCounterEmkSingle < ERROR_EMK_SINGLE_1MS) {
				++xgErrorCounters.ui8ErrorCounterEmkSingle;
			}
		} else if (xgErrorCounters.ui8ErrorCounterEmkSingle > 0) {
			--xgErrorCounters.ui8ErrorCounterEmkSingle;
		}

		if (xgErrorCounters.ui8ErrorCounterEmkSingle >= ERROR_EMK_SINGLE_1MS) {
			xgIpkTx.xErrors1.ui1ErrorEmkSingle = TRUE;
		}
	} else {
		xgErrorCounters.ui8ErrorCounterEmkSingle = 0;
	}
}

/*-------------------------------------------------------------------------------------------------
 *    Description: EMK sensing /SR0017
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: 	  None
 *    Changes:	  2022-04-03	Re-Implementing function based on the concept of
 *    							synchronizing to reference signal used for wirebreakage
 *    							Check "DNSL-PDSV.Drahtbruch-EMK-Deltafunction.xlsx"
 *-------------------------------------------------------------------------------------------------*/
void vEmkExecute(uint8_t *pui8CheckinCounter) {
	static uint32_t ui32EmkReferenceCounter = 0;
	static uint8_t ui8SignaleLevel = 0;

	uint8_t ui8EmkState;

	++(*pui8CheckinCounter);

	// Active only if the mode is set
	if (xgRuntimeParameters.ui8EmkMode && xgRuntimeState.ui8ParameterComplete) {

		// Switch Gain depending on the parameters
		// Persistent pin control!
		//---------------------------------------------
		if (xgRuntimeParameters.ui16EmkSensitivityVoltage > EMK_GAIN_SWITCH_VOLTAGE) {
			mHardwareSetEmkLowGain;
		} else {
			mHardwareSetEmkHighGain;
		}

		// Rising edge resets the counter
		//---------------------------------------------
		if (mHardwareGetSignal) {
			ui8SignaleLevel = (ui8SignaleLevel << 1) | 0x01;
		} else {
			ui8SignaleLevel = (ui8SignaleLevel << 1);
		}

		if ((ui8SignaleLevel & 0x03) == 0x01) {
			ui32EmkReferenceCounter = 0;
		} else {
			++ui32EmkReferenceCounter;
		}

		// Messungszeiten
		// Z0:  6  - 55
		// Z1: 106 - 155
		// Z2: 206 - 255
		if ((ui32EmkReferenceCounter >= 6 && ui32EmkReferenceCounter <= 55) || (ui32EmkReferenceCounter >= 106 && ui32EmkReferenceCounter <= 155)
				|| (ui32EmkReferenceCounter >= 206 && ui32EmkReferenceCounter <= 255)) {

			ui8EmkState = EMK_STATE_CAPTURE;

			// Auswertungszeiten
			// A0:56 - 105
			// A1:156 - 205
			// A2:276 - 325
		} else if ((ui32EmkReferenceCounter >= 56 && ui32EmkReferenceCounter <= 105) || (ui32EmkReferenceCounter >= 156 && ui32EmkReferenceCounter <= 205)
				|| (ui32EmkReferenceCounter >= 276 && ui32EmkReferenceCounter <= 325)) {

			ui8EmkState = EMK_STATE_EVALUATE;

		} else {
			ui8EmkState = EMK_STATE_PAUSE;
		}

		// Zustandsmaschine:
		//------------------------------------------------------------
		switch (ui8EmkState) {
		case EMK_STATE_PAUSE:
			xgRuntimeState.xEmk.ui32EmkMinAverage = ADC_VOLTAGE_MAX;
			xgRuntimeState.xEmk.ui32EmkMaxAverage = ADC_VOLTAGE_MIN;
			break;
		case EMK_STATE_EVALUATE:
		case EMK_STATE_CAPTURE:
			++xgRuntimeState.xEmk.ui8EmkSampleCount;
			// Min max voltage

			if (xgRuntimeState.xVoltage.ui32VoltageEmkAverageUW > xgRuntimeState.xEmk.ui32EmkMaxAverage) {
				xgRuntimeState.xEmk.ui32EmkMaxAverage = xgRuntimeState.xVoltage.ui32VoltageEmkAverageUW;
			}

			if (xgRuntimeState.xVoltage.ui32VoltageEmkAverageUW < xgRuntimeState.xEmk.ui32EmkMinAverage) {
				xgRuntimeState.xEmk.ui32EmkMinAverage = xgRuntimeState.xVoltage.ui32VoltageEmkAverageUW;
			}

			// Messung
			//--------------------------
			if (xgRuntimeState.xEmk.ui8EmkSampleCount >= PERIOD_COUNT) {
				xgRuntimeState.xEmk.ui32EmkDelta = (xgRuntimeState.xEmk.ui32EmkMaxAverage - xgRuntimeState.xEmk.ui32EmkMinAverage);

				// Bigger than off voltage
				if (xgRuntimeState.xEmk.ui32EmkDelta > xgRuntimeParameters.ui16EmkHysteresisOff) {
					xgRuntimeState.xEmk.ui8EmkShiftingStatus = (xgRuntimeState.xEmk.ui8EmkShiftingStatus << 1);
				}

				// Smaller than on voltage
				if (xgRuntimeState.xEmk.ui32EmkDelta < xgRuntimeParameters.ui16EmkHysteresisOn) {
					xgRuntimeState.xEmk.ui8EmkShiftingStatus = (xgRuntimeState.xEmk.ui8EmkShiftingStatus << 1) + 1;
				}

				xgRuntimeState.xEmk.ui8EmkSampleCount = 0;
				xgRuntimeState.xEmk.ui32EmkMinAverage = ADC_VOLTAGE_MAX;
				xgRuntimeState.xEmk.ui32EmkMaxAverage = ADC_VOLTAGE_MIN;
			}
			break;
		default:
			break;
		}

		// /SR0017
		vEmkCheckSingle();

		// Peaks for higher frequencies
		//-----------------------------------------------
		if ((xgRuntimeState.xVoltage.ui32VoltageEmkAverageUW < MIN_VOLTAGE_EMK || xgRuntimeState.xVoltage.ui32VoltageEmkAverageUW > MAX_VOLTAGE_EMK)
				&& xgRuntimeState.xEmk.ui8EmkPeakCount < PERIOD_COUNT) {
			++xgRuntimeState.xEmk.ui8EmkPeakCount;
		} else if (xgRuntimeState.xEmk.ui8EmkPeakCount > 0) {
			--xgRuntimeState.xEmk.ui8EmkPeakCount;
		}

		// New sampling is not evaluated, if peak to peak has been reached
		if (xgRuntimeState.xEmk.ui8EmkPeakCount >= PERIOD_COUNT) {
			xgIpkTx.ui8EmkVoltage = 0xFF;
			xgRuntimeState.xEmk.ui8EmkShiftingStatus = 0;
		}

		// Shift-Register Status. Works symmetrically
		//----------------------------------------------
		if ((xgRuntimeState.xEmk.ui8EmkShiftingStatus & PERIOD_MASK) == PERIOD_MASK) {
			xgIpkTx.xSpeedEmk.ui1Emk = TRUE;
		} else if ((xgRuntimeState.xEmk.ui8EmkShiftingStatus & PERIOD_MASK) == 0x00) {
			xgIpkTx.xSpeedEmk.ui1Emk = FALSE;
		}

		// EMK Verzögerung
		//-------------------------------------------
		if (xgIpkTx.xSpeedEmk.ui1Emk) {
			if (xgRuntimeState.xEmk.ui32EmkDelay) {
				--xgRuntimeState.xEmk.ui32EmkDelay;
			}

			// Ready-Flag wird mit Emk verundet
			if (!xgRuntimeState.xEmk.ui32EmkDelay) {
				xgIpkTx.xSpeedEmk.ui1EmkReady = TRUE;
			}
		} else {
			xgRuntimeState.xEmk.ui32EmkDelay = mGetRecalculatedEmkDelay;
			xgIpkTx.xSpeedEmk.ui1EmkReady = FALSE;
		}

		// Massnahmen EMK SRS-Errors
		//--------------------------
		if (xgIpkTx.xErrors1.ui1ErrorEmkSingle || xgIpkTx.xErrors2.ui1ErrorWireBreakage || xgIpkTx.xErrors2.ui1ErrorWireBreakageExtended) {
			xgIpkTx.xSpeedEmk.ui1Emk = FALSE;
		}

		// Diagnose: EMK Delta
		//--------------------
		xgIpkTx.ui8EmkVoltage = (xgRuntimeState.xEmk.ui32EmkDelta >> 2);

	} else {

		xgRuntimeState.xEmk.ui32EmkMinAverage = ADC_VOLTAGE_MAX;
		xgRuntimeState.xEmk.ui32EmkMaxAverage = ADC_VOLTAGE_MIN;
		xgRuntimeState.xEmk.ui32EmkDelta = 0x000;

		// Reset diagnostics
		xgIpkTx.ui8EmkVoltage = 0;
		xgRuntimeState.xEmk.ui8EmkShiftingStatus = 0;

	}
}

