/*
 *  safe-speed.c
 *
 *  Created on: Aug 5, 2020
 *      Author: am
 */

#include "safe/safe-speed.h"
#include "pdsv-parameters.h"
#include "pdsv-hardware.h"
#include "pdsv-fdcan.h"

#include "pdsv-diagnostics.h"
#include "pdsv-ipk.h"

/* Private defines */
#define ERROR_1500HZ_COUNTER 50
#define ERROR_FREQUENCY_NOT_EQUAL_1MS	4000U		// 4 seconds
#define ERROR_QUITT_STUCKAT_1MS			10000U		// 10 seconds
#define ERROR_QUITT_RESET_1MS			1000

// Quitt States
#define ACK_STATE_INIT 0
#define ACK_STATE_ACTIVE_HIGH_FE 2
#define ACK_STATE_SET_BIT 3
#define ACK_STATE_WAIT_FOR_Q 4
#define ACK_STATE_ACKED	5
#define ACK_STATE_WAIT_FOR_MOTOR 6
#define ACK_STATE_WAIT_FOR_START 8

// Delays
#define MIN_DELAY_ACK 500
#define MIN_TIME_ON 150
#define MIN_TIME_OFF 100
#define MAX_TIME_Q_PASSIVE	(uint32_t)11100

#define MUTING_OVERRIDE_FREQUENCY_LOW 0
#define MUTING_OVERRIDE_HYSTERESIS_LOW 0
#define MUTING_OVERRIDE_TIMEOUT_LOW 0

#define MUTING_OVERRIDE_FREQUENCY_HIGH 0xFFFF
#define MUTING_OVERRIDE_HYSTERESIS_HIGH 0xFFFF
#define MUTING_OVERRIDE_TIMEOUT_HIGH 10

static uint8_t ui8AckModeState = ACK_STATE_INIT;
static uint8_t ui8OperatingMode = 0;
static uint32_t ui32AcknowledgeSoftTimer = 0;			//  TimeDelay used for delaying the states and validating the signal
static uint32_t ui32AckMinDelay = 0;

/* Private defines */
#define ERROR_DIR_SWITCH_COUNTER	30		// Direction frequent switching error tolerance
#define ERROR_DIR_SWITCH_STEP 		4
#define DIRECTION_HYSTERESIS 		3		// Direction switching tolerance

#define DIVIDER_10MS	10

#define mGetBypass (ui8OperatingMode & 0x08)

#define mCounterBasedSwitching(execcondition, resultbit, counter, Switchoffdelay) 	\
					{\
						if(!Switchoffdelay){\
							if(execcondition){\
								if(counter)\
									--counter;\
								if(!counter)\
								  resultbit = FALSE;\
							 } else {\
								resultbit = FALSE;\
							}\
						}\
					}\

#define mCounterBasedSwitchingSpecial(execcondition, resultbit, counter, Switchoffdelay) 	\
					{\
						if(!Switchoffdelay){\
							if(execcondition){\
								if(counter)\
									--counter;\
								if(!counter)\
								  resultbit = TRUE;\
							 } else {\
								resultbit = TRUE;\
							}\
						}\
					}\


/*-------------------------------------------------------------------------------------------------
 *    Description: Phase MonitoringToggling /SR0022
 *    Frequency:  1 ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vSpeedCheckPhase(void) {

	// Do the check only if in range
	if ((xgRuntimeState.xSpeed.ui32HysteresisTimeout && (xgRuntimeState.xSpeed.ui32MasterFrequency > FREQ_5HZ && xgRuntimeState.xSpeed.ui32MasterFrequency < FREQ_100HZ))) {

		// Test Running
		if (xgRuntimeState.xDirection.ui8SimilaryCheckRunning) {
			xgRuntimeState.xDirection.ui32TotalNumberOfPhaseSamples++;

			// The Frequency is too low, reset the error counter
			if (xgRuntimeState.xDirection.ui32TotalNumberOfPhaseSamples >= 800) {
				xgRuntimeState.xDirection.ui8SimilaryCheckRunning = FALSE;
				xgErrorCounters.ui16ErrorCounterPhaseShift = 0;
			} else {

				uint8_t ui8PulsePins = mHardwareGetStateDirection;
				uint8_t ui8SlavePin = ui8PulsePins >> 1;
				uint8_t ui8MasterPin = ui8PulsePins & 0x01;

				if (!(ui8MasterPin)) {
					++xgRuntimeState.xDirection.ui32TotalNumberOfPhaseSamples;
				}

				if (ui8MasterPin != ui8SlavePin) {
					xgRuntimeState.xDirection.ui32SignalSimilarity++;
				}
			}
		}

		// Start/Stop Test
		if (xgRuntimeState.xProcess.ui1NewEvaluation) {

			// If the Check was already underway we finish it and calculate the similarity
			if (xgRuntimeState.xDirection.ui8SimilaryCheckRunning && xgRuntimeState.xDirection.ui32TotalNumberOfPhaseSamples > 0) {
				xgRuntimeState.xDirection.ui32PercentageSimilarity = ((xgRuntimeState.xDirection.ui32SignalSimilarity) * 100 / xgRuntimeState.xDirection.ui32TotalNumberOfPhaseSamples);

				if ((xgRuntimeState.xDirection.ui32PercentageSimilarity < 50) || (xgRuntimeState.xDirection.ui32PercentageSimilarity > 79)
						|| (xgRuntimeState.xDirection.ui32TotalNumberOfMasterLowSamples == 0
								|| xgRuntimeState.xDirection.ui32TotalNumberOfMasterLowSamples == xgRuntimeState.xDirection.ui32TotalNumberOfPhaseSamples))
					xgErrorCounters.ui16ErrorCounterPhaseShift += 3;

				else if (xgErrorCounters.ui16ErrorCounterPhaseShift > 0)
					xgErrorCounters.ui16ErrorCounterPhaseShift--;

			}

			// Start
			xgRuntimeState.xDirection.ui32SignalSimilarity = 0;
			xgRuntimeState.xDirection.ui32TotalNumberOfPhaseSamples = 0;
			xgRuntimeState.xDirection.ui32TotalNumberOfMasterLowSamples = 0;
			xgRuntimeState.xDirection.ui8SimilaryCheckRunning = TRUE;
		}

		// Reset error
	} else {

		xgErrorCounters.ui16ErrorCounterPhaseShift = 0;
		xgRuntimeState.xDirection.ui8SimilaryCheckRunning = FALSE;
	}

	if (xgErrorCounters.ui16ErrorCounterPhaseShift > 17) {
		xgIpkTx.xErrors2.ui1ErrorPhase = TRUE;
	}

}

/*-------------------------------------------------------------------------------------------------
 *    Description: Check Direction Toggling /SR0023
 *    Frequency:  on new measurement
 *    Parameters: None
 *    Return: None
 *--------------------------------------------------------------------------------------------------*/
void vDirectionCheckRotationSwitch(void) {

	static uint16_t ui16PreviousDirectionRight = 0;

	if ((xgRuntimeState.xSpeed.ui32MasterFrequency > FREQ_5HZ) && (xgRuntimeState.xSpeed.ui32MasterFrequency < FREQ_100HZ)
			&& (ui16PreviousDirectionRight != xgRuntimeState.xDirection.ui16DirectionRight)) {
		if (xgErrorCounters.ui8ErrorCounterDirectionSwitch < ERROR_DIR_SWITCH_COUNTER) {
			xgErrorCounters.ui8ErrorCounterDirectionSwitch += ERROR_DIR_SWITCH_STEP;
		}
	} else if (xgErrorCounters.ui8ErrorCounterDirectionSwitch && xgErrorCounters.ui8ErrorCounterDirectionSwitch < ERROR_DIR_SWITCH_COUNTER) {
		--xgErrorCounters.ui8ErrorCounterDirectionSwitch;
	}

	if (!xgRuntimeState.xSpeed.ui32HysteresisTimeout) {
		xgErrorCounters.ui8ErrorRotationDirectionSwitchCounter = 0;
	}

	if (xgErrorCounters.ui8ErrorCounterDirectionSwitch >= ERROR_DIR_SWITCH_COUNTER) {
		xgIpkTx.xErrors2.ui1ErrorDirectionSwitch = TRUE;
	}

	ui16PreviousDirectionRight = xgRuntimeState.xDirection.ui16DirectionRight;
}

/*-------------------------------------------------------------------------------------------------
 *    Description: Compare frequencies /SR0013
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *--------------------------------------------------------------------------------------------------*/
void vSpeedCheckFrequenciesNotEqual(void) {
	uint32_t ui32FrequencyDeviation;
	xgRuntimeState.xSpeed.ui32SlaveFrequency = (((uint32_t) xgIpkRx.ui8FrequencyHighbyte) << 8) | (xgIpkRx.ui8FrequencyLowbyte);

	// If  bypass
	// if in standstill
	// if freq < 2
	//DEBUGCODE({xgIpkTx.xLogic.ui1Limit0= FALSE;})	// FIT
	//DEBUGCODE(xgRuntimeState.xSpeed.ui32MasterFrequency = 200;); // FIT
	if (mGetBypass || (xgIpkTx.xLogic.ui1Limit0 && xgIpkRx.xLogic.ui1Limit0)
			|| ((xgRuntimeState.xSpeed.ui32MasterFrequency < xgRuntimeParameters.ui16SpeedCompareWedge) && (xgRuntimeState.xSpeed.ui32SlaveFrequency < xgRuntimeParameters.ui16SpeedCompareWedge))) {
		xgErrorCounters.ui32ErrorCounterFrequencyCompare = 0;
	} else {
		// Avoid Devision by 0 by adding 1
		// Deviation should be around 1024 +- 20 %   which is 1229 and 819
		ui32FrequencyDeviation = ((xgRuntimeState.xSpeed.ui32MasterFrequency + 1) << 10) / (xgRuntimeState.xSpeed.ui32SlaveFrequency + 1);

		if (ui32FrequencyDeviation > 1229 || ui32FrequencyDeviation < 819) {
			++xgErrorCounters.ui32ErrorCounterFrequencyCompare;
		} else if (xgErrorCounters.ui32ErrorCounterFrequencyCompare > 0) {
			--xgErrorCounters.ui32ErrorCounterFrequencyCompare;
		}

	}

	if (xgErrorCounters.ui32ErrorCounterFrequencyCompare >= ERROR_FREQUENCY_NOT_EQUAL_1MS) {
		xgIpkTx.xErrors2.ui1ErrorFrequencyNotEqual = TRUE;
	}
}

/*-------------------------------------------------------------------------------------------------
 *    Description: Check 1500 Hz /SR0014
 *    Frequency:  on new measurement
 *    Parameters: None
 *    Return: None
 *--------------------------------------------------------------------------------------------------*/
void vSpeedCheck1500Hz(void) {

	// Check only if valid
	if (xgRuntimeState.xProcess.ui1NewEvaluation && xgRuntimeState.xProcess.ui1ValidFrequency) {
		if (xgRuntimeState.xSpeed.ui32MasterFrequency >= FREQ_1500HZ) {
			++xgErrorCounters.ui8ErrorCounterFrequencyOver1500Hz;
			if (xgErrorCounters.ui8ErrorCounterFrequencyOver1500Hz == ERROR_1500HZ_COUNTER) {
				xgIpkTx.xErrors2.ui1ErrorFrequency1500hz = TRUE;
			}
		} else {
			xgErrorCounters.ui8ErrorCounterFrequencyOver1500Hz = 0;
		}

	}
}

/*-------------------------------------------------------------------------------------------------
 *    Description: Check Quitt Stuckat /SR0016
 *    Frequency:  1ms
 *    Parameters: pui8CheckinCounter	-	Scheduler Counter
 *    Return: None
 *--------------------------------------------------------------------------------------------------*/
void vSpeedCheckQuittStuckat(void) {

	// WARNING! Should be checked only if the parameters are valid
	uint8_t ui8ManualAcknowledgement = (xgRuntimeParameters.ui8SpeedMode == 1) || (xgRuntimeParameters.ui8SpeedMode == 3) || (xgRuntimeParameters.ui8SpeedMode == 4);

	if (ui8ManualAcknowledgement && (xgImkRx.xZmvValidationIn.ui1RTDS)) {
		++xgErrorCounters.ui32ErrorCounterQuitt;
	} else if (xgErrorCounters.ui32ErrorCounterQuitt > 0) {
		--xgErrorCounters.ui32ErrorCounterQuitt;
	}

	// Check for stuck at
	// Error Can  be reset by RTSK
	//------------------------------------------------------------------
	if (xgErrorCounters.ui32ErrorCounterQuitt >= ERROR_QUITT_STUCKAT_1MS) {
		xgErrorCounters.ui32ErrorCounterQuitt = ERROR_QUITT_STUCKAT_1MS;	// Set error flag reset timeout
		xgIpkTx.xErrors2.ui1ErrorQuitt = TRUE;
	}

	// Immediate precautions: Reset Ack-State
	//--------------------------------------
	if (xgIpkTx.xErrors2.ui1ErrorQuitt) {
		ui8AckModeState = 0;
		xgIpkTx.xLogic.ui1Qacknowledge = FALSE;

		// Fallback
		if (xgErrorCounters.ui32ErrorCounterQuitt <= (ERROR_QUITT_STUCKAT_1MS - ERROR_QUITT_RESET_1MS)) {
			xgIpkTx.xErrors2.ui1ErrorQuitt = FALSE;
			xgErrorCounters.ui32ErrorCounterQuitt = 0;
		}
	}
}

/*-------------------------------------------------------------------------------------------------
 *    Description: Acknowledgement for Speed Mode 1. Manual
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *--------------------------------------------------------------------------------------------------*/
static inline void vSpeedAcknowledgeMode1(void) {

	uint8_t ui8SyncAcknowledge;
	uint8_t ui8SyncQout;

#ifndef DEBUG_DISABLE_DUALCHANNEL_AND
	ui8SyncAcknowledge = (xgIpkTx.xLogic.ui1Qacknowledge & xgIpkRx.xLogic.ui1Qacknowledge);
	ui8SyncQout = (xgIpkTx.xSpeedEmk.ui1Q1 & xgIpkRx.xSpeedEmk.ui1Q1);
#else

	ui8SyncAcknowledge = (xgIpkTx.xLogic.ui1Qacknowledge);
	ui8SyncQout = (xgIpkTx.xSpeedEmk.ui1Q1);
#endif

	// Always
	if (ui32AcknowledgeSoftTimer > 0)
		--ui32AcknowledgeSoftTimer;

	switch (ui8AckModeState) {
	case ACK_STATE_INIT:
		// can ack only if frequency is in range
		if (xgIpkTx.xLogic.ui1Limit1 && xgImkRx.xZmvValidationIn.ui1RTDS) {
			ui8AckModeState = ACK_STATE_ACTIVE_HIGH_FE;
			ui32AcknowledgeSoftTimer = MIN_TIME_ON;
		}
		break;
	case ACK_STATE_ACTIVE_HIGH_FE:
		if (ui32AcknowledgeSoftTimer == 0) {
			if (!xgImkRx.xZmvValidationIn.ui1RTDS) {	// Reagiert auf fallende Flanke
				ui8AckModeState = ACK_STATE_SET_BIT;
			}
		} else if (!xgImkRx.xZmvValidationIn.ui1RTDS) {
			ui32AcknowledgeSoftTimer = MIN_TIME_ON;
		}
		break;
	case ACK_STATE_SET_BIT:
		// Validate the bits
		if (xgIpkTx.xLogic.ui1Limit1) {
			xgIpkTx.xLogic.ui1Qacknowledge = TRUE;
			ui8AckModeState = ACK_STATE_WAIT_FOR_Q;
			ui32AcknowledgeSoftTimer = MAX_TIME_Q_PASSIVE;
		} else {
			ui8AckModeState = ACK_STATE_INIT;
		}
		break;
	case ACK_STATE_WAIT_FOR_Q:
		if (ui8SyncQout) {
			ui8AckModeState = ACK_STATE_ACKED;

			// When Q1 did not come
		} else if (!ui32AcknowledgeSoftTimer || !xgIpkTx.xLogic.ui1Limit1) {
			xgIpkTx.xLogic.ui1Qacknowledge = FALSE;
			ui8AckModeState = ACK_STATE_INIT;
		}
		break;
	case ACK_STATE_ACKED:
		if (!ui8SyncQout) {
			ui8AckModeState = ACK_STATE_INIT;
			xgIpkTx.xLogic.ui1Qacknowledge = FALSE;
		}
		break;

	}

	// Select upper limit according to priority
	//---------------------------------------------
	if (ui8SyncAcknowledge && xgIpkTx.xLogic.ui1Limit1) {
		xgIpkTx.xSpeedEmk.ui1Q1 = TRUE;
	} else {
		xgIpkTx.xSpeedEmk.ui1Q1 = FALSE;		// delete bit
	}

	// Check if Stillstand: there is no need for acknowledgment
	if (xgIpkTx.xLogic.ui1Limit0) {
		xgIpkTx.xSpeedEmk.ui1Q0 = TRUE;

	} else {
		xgIpkTx.xSpeedEmk.ui1Q0 = FALSE;
	}

}

/*-------------------------------------------------------------------------------------------------
 *    Description: Acknowledgement for Speed Mode 2. Automatic
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *--------------------------------------------------------------------------------------------------*/
static inline void vSpeedAcknowledgeMode2() {
	uint8_t ui8SyncAcknowledge;
	uint8_t ui8SyncQout;
#ifndef DEBUG_DISABLE_DUALCHANNEL_AND
	ui8SyncAcknowledge = (xgIpkTx.xLogic.ui1Qacknowledge & xgIpkRx.xLogic.ui1Qacknowledge);
	ui8SyncQout = (xgIpkTx.xSpeedEmk.ui1Q1 & xgIpkRx.xSpeedEmk.ui1Q1);
#else

	ui8SyncAcknowledge = (xgIpkTx.xLogic.ui1Qacknowledge);
	ui8SyncQout = (xgIpkTx.xSpeedEmk.ui1Q1);
#endif

	// Always
	if (ui32AcknowledgeSoftTimer > 0)
		--ui32AcknowledgeSoftTimer;

	switch (ui8AckModeState) {
	case ACK_STATE_INIT:
		// can ack only if frequency is in range
		if (xgIpkTx.xLogic.ui1Limit1 && xgImkRx.xZmvValidationIn.ui1RTDS) {
			ui8AckModeState = ACK_STATE_SET_BIT;
		}
		break;
	case ACK_STATE_SET_BIT:
		// Validate the bits
		if (xgIpkTx.xLogic.ui1Limit1) {
			xgIpkTx.xLogic.ui1Qacknowledge = TRUE;
			ui8AckModeState = ACK_STATE_WAIT_FOR_Q;
			ui32AcknowledgeSoftTimer = MAX_TIME_Q_PASSIVE;
		} else {
			ui8AckModeState = ACK_STATE_INIT;
		}
		break;
	case ACK_STATE_WAIT_FOR_Q:
		if (ui8SyncQout) {
			ui8AckModeState = ACK_STATE_ACKED;
		} else if (!ui32AcknowledgeSoftTimer || !(xgIpkTx.xLogic.ui1Limit1)) {
			xgIpkTx.xLogic.ui1Qacknowledge = FALSE;
			ui8AckModeState = ACK_STATE_INIT;
		}
		break;
	case ACK_STATE_ACKED:
		if (!ui8SyncQout) {
			ui8AckModeState = ACK_STATE_INIT;
			xgIpkTx.xLogic.ui1Qacknowledge = FALSE;
		}
		break;

	}

	// Select upper limit according to priority
	//---------------------------------------------
	if (ui8SyncAcknowledge && xgIpkTx.xLogic.ui1Limit1) {
		xgIpkTx.xSpeedEmk.ui1Q1 = TRUE;

	} else {
		xgIpkTx.xSpeedEmk.ui1Q1 = FALSE;		// delete bit

	}

	// Check if Stillstand: there is no need for acknowledgment
	if (xgIpkTx.xLogic.ui1Limit0) {
		xgIpkTx.xSpeedEmk.ui1Q0 = TRUE;

	} else {
		xgIpkTx.xSpeedEmk.ui1Q0 = FALSE;
	}
}

/*-------------------------------------------------------------------------------------------------
 *    Description: Acknowledgement for Speed Mode 3 Manual
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *--------------------------------------------------------------------------------------------------*/
static inline void vSpeedAcknowledgeMode3() {
	uint8_t ui8SyncAcknowledge;
	uint8_t ui8SyncQout;
#ifndef DEBUG_DISABLE_DUALCHANNEL_AND
	ui8SyncAcknowledge = (xgIpkTx.xLogic.ui1Qacknowledge & xgIpkRx.xLogic.ui1Qacknowledge);
	ui8SyncQout = (xgIpkTx.xSpeedEmk.ui1Q1 & xgIpkRx.xSpeedEmk.ui1Q1);
#else

	ui8SyncAcknowledge = (xgIpkTx.xLogic.ui1Qacknowledge);
	ui8SyncQout = (xgIpkTx.xSpeedEmk.ui1Q1);
#endif

	uint8_t ui8ConditionInWindow = (!xgIpkTx.xLogic.ui1Limit1 && xgIpkTx.xLogic.ui1Limit2);

	// Always
	if (ui32AcknowledgeSoftTimer > 0)
		--ui32AcknowledgeSoftTimer;

	switch (ui8AckModeState) {
	case ACK_STATE_INIT:
		// Ack only in range
		if (ui8ConditionInWindow && xgImkRx.xZmvValidationIn.ui1RTDS) {
			ui8AckModeState = ACK_STATE_ACTIVE_HIGH_FE;
			ui32AcknowledgeSoftTimer = MIN_TIME_ON;
		}
		break;
	case ACK_STATE_ACTIVE_HIGH_FE:
		if (ui32AcknowledgeSoftTimer == 0) {
			if (!xgImkRx.xZmvValidationIn.ui1RTDS) {
				ui8AckModeState = ACK_STATE_SET_BIT;
			}
		} else if (!xgImkRx.xZmvValidationIn.ui1RTDS) {
			ui32AcknowledgeSoftTimer = MIN_TIME_ON;
		}
		break;
	case ACK_STATE_SET_BIT:
		// Validate the bits
		if (ui8ConditionInWindow) {
			xgIpkTx.xLogic.ui1Qacknowledge = TRUE;
			ui8AckModeState = ACK_STATE_WAIT_FOR_Q;
			ui32AcknowledgeSoftTimer = MAX_TIME_Q_PASSIVE;
		} else {
			ui8AckModeState = ACK_STATE_INIT;
		}
		break;
	case ACK_STATE_WAIT_FOR_Q:
		if (ui8SyncQout) {
			ui8AckModeState = ACK_STATE_ACKED;
		} else if (!ui32AcknowledgeSoftTimer || !(ui8ConditionInWindow)) {
			xgIpkTx.xLogic.ui1Qacknowledge = FALSE;
			ui8AckModeState = ACK_STATE_INIT;
		}
		break;
	case ACK_STATE_ACKED:
		if (!ui8SyncQout) {
			ui8AckModeState = ACK_STATE_INIT;
			xgIpkTx.xLogic.ui1Qacknowledge = FALSE;
		}
		break;
	}

	// Q2
	// Frequency is in the window
	//---------------------------
	if (ui8SyncAcknowledge && ui8ConditionInWindow) {
		xgIpkTx.xSpeedEmk.ui1Q1 = TRUE;
	} else {
		xgIpkTx.xSpeedEmk.ui1Q1 = FALSE;		// delete bit

	}

	// Q1
	//-------------------------
	if (xgIpkTx.xLogic.ui1Limit0) {
		xgIpkTx.xSpeedEmk.ui1Q0 = TRUE;
	} else {
		xgIpkTx.xSpeedEmk.ui1Q0 = FALSE;
	}
}

/*-------------------------------------------------------------------------------------------------
 *    Description: Acknowledgement for Speed Mode 4 Manual + Runup
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *--------------------------------------------------------------------------------------------------*/
static inline void vSpeedAcknowledgeMode4() {
	uint8_t ui8SyncAcknowledge;
	uint8_t ui8SyncQout;
#ifndef DEBUG_DISABLE_DUALCHANNEL_AND
	ui8SyncAcknowledge = (xgIpkTx.xLogic.ui1Qacknowledge & xgIpkRx.xLogic.ui1Qacknowledge);
	ui8SyncQout = (xgIpkTx.xSpeedEmk.ui1Q1 & xgIpkRx.xSpeedEmk.ui1Q1);
#else

	ui8SyncAcknowledge = (xgIpkTx.xLogic.ui1Qacknowledge);
	ui8SyncQout = (xgIpkTx.xSpeedEmk.ui1Q1);
#endif

	uint8_t ui8ConditionInWindow = (!xgIpkTx.xLogic.ui1Limit1 && xgIpkTx.xLogic.ui1Limit2);

	// Always
	if (ui32AcknowledgeSoftTimer > 0)
		--ui32AcknowledgeSoftTimer;

	switch (ui8AckModeState) {
	case ACK_STATE_INIT:
		if (xgIpkTx.xLogic.ui1Limit2 && xgImkRx.xZmvValidationIn.ui1RTDS) {
			ui8AckModeState = ACK_STATE_ACTIVE_HIGH_FE;
			ui32AcknowledgeSoftTimer = MIN_TIME_ON;
		}
		break;
	case ACK_STATE_ACTIVE_HIGH_FE:
		if (ui32AcknowledgeSoftTimer == 0) {
			if (!xgImkRx.xZmvValidationIn.ui1RTDS) {
				ui8AckModeState = ACK_STATE_SET_BIT;
			}
		} else if (!xgImkRx.xZmvValidationIn.ui1RTDS) {
			ui32AcknowledgeSoftTimer = MIN_TIME_ON;
		}
		break;
	case ACK_STATE_SET_BIT:

		// Under the upper limit or muted
		if ((xgIpkTx.xLogic.ui1Limit2)) {
			xgIpkTx.xLogic.ui1Qacknowledge = TRUE;
			ui8AckModeState = ACK_STATE_WAIT_FOR_Q;
			ui32AcknowledgeSoftTimer = MAX_TIME_Q_PASSIVE;
		} else {
			ui8AckModeState = ACK_STATE_INIT;
		}
		break;
	case ACK_STATE_WAIT_FOR_Q:
		if (ui8SyncQout) {
			ui8AckModeState = ACK_STATE_WAIT_FOR_MOTOR;
			ui32AcknowledgeSoftTimer = mGetRecalculatedRunupTime;

			// Outside of Upper Limit
		} else if (!ui32AcknowledgeSoftTimer || !(xgIpkTx.xLogic.ui1Limit2)) {
			xgIpkTx.xLogic.ui1Qacknowledge = FALSE;
			ui8AckModeState = ACK_STATE_INIT;
		}
		break;
	case ACK_STATE_WAIT_FOR_MOTOR:
		if (ui8ConditionInWindow) {
			ui8AckModeState = ACK_STATE_ACKED;
			ui32AcknowledgeSoftTimer = 0;

			// Outside of Upper Limit
		} else if (!ui32AcknowledgeSoftTimer || !(xgIpkTx.xLogic.ui1Limit2)) {
			xgIpkTx.xLogic.ui1Qacknowledge = FALSE;
			ui8AckModeState = ACK_STATE_INIT;
		}

		break;
	case ACK_STATE_ACKED:		// When the condition is not upheld
		if (!ui8ConditionInWindow) {
			ui8AckModeState = ACK_STATE_INIT;
			xgIpkTx.xLogic.ui1Qacknowledge = FALSE;
		}
		break;
	}

	// Q2
	// Frequency is in the window
	//---------------------------
	if (ui8SyncAcknowledge && (xgIpkTx.xLogic.ui1Limit2)) {
		xgIpkTx.xSpeedEmk.ui1Q1 = TRUE;
	} else {
		xgIpkTx.xSpeedEmk.ui1Q1 = FALSE;		// delete bit

	}

	// Q1
	//-------------------------
	if (xgIpkTx.xLogic.ui1Limit0) {
		xgIpkTx.xSpeedEmk.ui1Q0 = TRUE;
	} else {
		xgIpkTx.xSpeedEmk.ui1Q0 = FALSE;
	}

}

/*-------------------------------------------------------------------------------------------------
 *    Description: Acknowledgement for Speed Mode 5 Automatic
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *--------------------------------------------------------------------------------------------------*/
static inline void vSpeedAcknowledgeMode5() {
	uint8_t ui8SyncAcknowledge;
	uint8_t ui8SyncQout;

#ifndef DEBUG_DISABLE_DUALCHANNEL_AND
	ui8SyncAcknowledge = (xgIpkTx.xLogic.ui1Qacknowledge & xgIpkRx.xLogic.ui1Qacknowledge);
	ui8SyncQout = (xgIpkTx.xSpeedEmk.ui1Q1 & xgIpkRx.xSpeedEmk.ui1Q1);
#else

	ui8SyncAcknowledge = (xgIpkTx.xLogic.ui1Qacknowledge);
	ui8SyncQout = (xgIpkTx.xSpeedEmk.ui1Q1);
#endif

	uint8_t ui8ConditionInWindow = (!xgIpkTx.xLogic.ui1Limit1 && xgIpkTx.xLogic.ui1Limit2);

	// Always
	if (ui32AcknowledgeSoftTimer > 0)
		--ui32AcknowledgeSoftTimer;

	switch (ui8AckModeState) {
	case ACK_STATE_INIT:
		if (ui8ConditionInWindow && xgImkRx.xZmvValidationIn.ui1RTDS) {
			ui8AckModeState = ACK_STATE_SET_BIT;
		}
		break;
	case ACK_STATE_SET_BIT:
		// Validate the bits
		if (ui8ConditionInWindow) {
			xgIpkTx.xLogic.ui1Qacknowledge = TRUE;
			ui8AckModeState = ACK_STATE_WAIT_FOR_Q;
			ui32AcknowledgeSoftTimer = MAX_TIME_Q_PASSIVE;
		} else {
			ui8AckModeState = ACK_STATE_INIT;
		}
		break;
	case ACK_STATE_WAIT_FOR_Q:
		if (ui8SyncQout) {
			ui8AckModeState = ACK_STATE_ACKED;
		} else if (!ui32AcknowledgeSoftTimer || !(ui8ConditionInWindow)) {
			xgIpkTx.xLogic.ui1Qacknowledge = FALSE;
			ui8AckModeState = ACK_STATE_INIT;
		}
		break;
	case ACK_STATE_ACKED:
		if (!ui8SyncQout) {
			ui8AckModeState = ACK_STATE_INIT;
			xgIpkTx.xLogic.ui1Qacknowledge = FALSE;
		}
		break;
	}

	// Q2
	// Frequency is in the window
	//---------------------------
	if (ui8SyncAcknowledge && (ui8ConditionInWindow)) {
		xgIpkTx.xSpeedEmk.ui1Q1 = TRUE;
	} else {

		xgIpkTx.xSpeedEmk.ui1Q1 = FALSE;		// delete bit
	}

	// Q1
	//-------------------------
	if (xgIpkTx.xLogic.ui1Limit0) {
		xgIpkTx.xSpeedEmk.ui1Q0 = TRUE;
	} else {
		xgIpkTx.xSpeedEmk.ui1Q0 = FALSE;
	}
}

/*-------------------------------------------------------------------------------------------------
 *    Description: Acknowledgement for Speed Mode 6 Automatic + Runup
 *    			   Windowed Automnatic Run-up, starts counting when Standstill is reached
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *--------------------------------------------------------------------------------------------------*/
static inline void vSpeedAcknowledgeMode6() {
	uint8_t ui8SyncAcknowledge;
	uint8_t ui8SyncQout;

#ifndef DEBUG_DISABLE_DUALCHANNEL_AND
	ui8SyncAcknowledge = (xgIpkTx.xLogic.ui1Qacknowledge & xgIpkRx.xLogic.ui1Qacknowledge);
	ui8SyncQout = (xgIpkTx.xSpeedEmk.ui1Q1 & xgIpkRx.xSpeedEmk.ui1Q1);
#else

	ui8SyncAcknowledge = (xgIpkTx.xLogic.ui1Qacknowledge);
	ui8SyncQout = (xgIpkTx.xSpeedEmk.ui1Q1);
#endif

	uint8_t ui8ConditionInWindow = (!xgIpkTx.xLogic.ui1Limit1 && xgIpkTx.xLogic.ui1Limit2);

	// Always
	if (ui32AcknowledgeSoftTimer > 0)
		--ui32AcknowledgeSoftTimer;

	if (ui32AckMinDelay > 0)
		--ui32AckMinDelay;

	//xgImkRx.xZmvValidationIn.ui1RTDS = 1;

	switch (ui8AckModeState) {
	case ACK_STATE_INIT:
		if (xgIpkTx.xLogic.ui1Limit2 && xgImkRx.xZmvValidationIn.ui1RTDS) {
			ui8AckModeState = ACK_STATE_SET_BIT;
		}
		break;

		// has to stay Low at least 100 ms
	case ACK_STATE_SET_BIT:
		if (!ui32AckMinDelay && xgIpkTx.xLogic.ui1Limit2) {

			// if standstill, already in the window

			if (xgIpkTx.xLogic.ui1Limit0) {
				ui8AckModeState = ACK_STATE_WAIT_FOR_START;
				ui32AcknowledgeSoftTimer = MAX_TIME_Q_PASSIVE;
				xgIpkTx.xLogic.ui1Qacknowledge = TRUE;

			} else if (ui8ConditionInWindow) {
				ui8AckModeState = ACK_STATE_WAIT_FOR_Q;
				ui32AcknowledgeSoftTimer = MAX_TIME_Q_PASSIVE;
				xgIpkTx.xLogic.ui1Qacknowledge = TRUE;
			}

		} else {
			ui8AckModeState = ACK_STATE_INIT;
		}

		break;
	case ACK_STATE_WAIT_FOR_START:	// Wait for frequency to go outside of standstill
		if (!xgIpkTx.xLogic.ui1Limit0) {
			ui32AcknowledgeSoftTimer = mGetRecalculatedRunupTime;
			ui8AckModeState = ACK_STATE_WAIT_FOR_Q;
		}
		break;
	case ACK_STATE_WAIT_FOR_Q:
		if (ui8SyncQout) {
			ui8AckModeState = ACK_STATE_WAIT_FOR_MOTOR;
		} else if (!ui32AcknowledgeSoftTimer || !(xgIpkTx.xLogic.ui1Limit2)) {
			xgIpkTx.xLogic.ui1Qacknowledge = FALSE;
			ui8AckModeState = ACK_STATE_INIT;
		}
		break;
	case ACK_STATE_WAIT_FOR_MOTOR:
		if (ui8ConditionInWindow) {
			ui8AckModeState = ACK_STATE_ACKED;
			ui32AcknowledgeSoftTimer = 0;

		} else if (!ui32AcknowledgeSoftTimer || !(xgIpkTx.xLogic.ui1Limit2)) {
			xgIpkTx.xLogic.ui1Qacknowledge = FALSE;
			ui8AckModeState = ACK_STATE_INIT;
			ui32AckMinDelay = MIN_DELAY_ACK;
		}

		break;
	case ACK_STATE_ACKED:		// When the condition is not upheld
		if (!ui8ConditionInWindow) {
			ui8AckModeState = ACK_STATE_INIT;
			xgIpkTx.xLogic.ui1Qacknowledge = FALSE;
			ui32AckMinDelay = MIN_DELAY_ACK;
		}
		break;
	}

	// Q2
	// Frequency is inside from above, lower limit is checked together with the acknowledgment
	//---------------------------
	if (ui8SyncAcknowledge && (xgIpkTx.xLogic.ui1Limit2)) {
		xgIpkTx.xSpeedEmk.ui1Q1 = TRUE;

	} else {
		xgIpkTx.xSpeedEmk.ui1Q1 = FALSE;		// delete bit
	}

	// Q1
	//-------------------------
	if (xgIpkTx.xLogic.ui1Limit0) {
		xgIpkTx.xSpeedEmk.ui1Q0 = TRUE;
	} else {
		xgIpkTx.xSpeedEmk.ui1Q0 = FALSE;
	}
}

/*-------------------------------------------------------------------------------------------------
 *    Description: Region, Acknoledgemnent is not required
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *--------------------------------------------------------------------------------------------------*/
static inline void vSpeedAcknowledgeMode7() {
	uint8_t ui8SyncAcknowledge = TRUE;		// Not quitt is needed

	// Select upper limit according to priority and set outs
	// Q4
	if (ui8SyncAcknowledge && xgIpkTx.xLogic.ui1Limit3 && !xgIpkTx.xLogic.ui1Limit2) {
		xgIpkTx.xSpeedEmk.ui4Q = 0x08;
		// Q3
	} else if (ui8SyncAcknowledge && xgIpkTx.xLogic.ui1Limit2 && !xgIpkTx.xLogic.ui1Limit1) {
		xgIpkTx.xSpeedEmk.ui4Q = 0x04;
		// Q2
	} else if (ui8SyncAcknowledge && xgIpkTx.xLogic.ui1Limit1 && !xgIpkTx.xLogic.ui1Limit0) {
		xgIpkTx.xSpeedEmk.ui4Q = 0x02;
		// Q1
	} else if (ui8SyncAcknowledge && xgIpkTx.xLogic.ui1Limit0) {
		xgIpkTx.xSpeedEmk.ui4Q = 0x01;
	} else {
		xgIpkTx.xSpeedEmk.ui4Q = 0;
	}
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Mode 1
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
static inline void vSpeedProcessLimitsStandard(void) {
	uint8_t ui8Level = xgIpkTx.xModifiers.ui2Validationbits;
	uint8_t ui8OperatingIndex;

	// Select Frequency based on Operating Mode
	// F3
	if (ui8OperatingMode & 0x04) {
		ui8OperatingIndex = 3;
		// F2
	} else if (ui8OperatingMode & 0x02) {
		ui8OperatingIndex = 2;
		// F1
	} else if (ui8OperatingMode & 0x01) {
		ui8OperatingIndex = 1;

		// Muting oder Stillstand
	} else {
		ui8OperatingIndex = 0;
	}

	uint32_t ui32CurrentStandstillFrequency = xgRuntimeParameters.rui16FrequencyLimits[0][0];
	uint32_t ui32CurrentStandstillHysteresis = xgRuntimeParameters.rui16FrequencyHysteresis[0][0];
	uint32_t ui32CurrenttStandstillTimeout = xgRuntimeParameters.rui16FrequencyTimeouts[0][0];

	// Set Frequency
	uint32_t ui32CurrentUpperFrequency;
	uint32_t ui32CurrentUpperHysteresis;
	uint32_t ui32CurrentUpperTimeout;

	uint32_t ui32CurrentSwitchoffUpperFrequency;

	// Override when muted
	if (ui8OperatingMode & 0x08) {
		ui32CurrentUpperFrequency = MUTING_OVERRIDE_FREQUENCY_HIGH;
		ui32CurrentUpperHysteresis = MUTING_OVERRIDE_HYSTERESIS_HIGH;
		ui32CurrentUpperTimeout = MUTING_OVERRIDE_TIMEOUT_HIGH;
	} else {
		ui32CurrentUpperFrequency = xgRuntimeParameters.rui16FrequencyLimits[ui8OperatingIndex][ui8Level];
		ui32CurrentUpperHysteresis = xgRuntimeParameters.rui16FrequencyHysteresis[ui8OperatingIndex][ui8Level];
		ui32CurrentUpperTimeout = xgRuntimeParameters.rui16FrequencyTimeouts[ui8OperatingIndex][ui8Level];
	}

	// Abschaltverzögerungsschwelle einstellen
	if (xgRuntimeParameters.ui8SpeedSwitchoffPercent != 0) {
		ui32CurrentSwitchoffUpperFrequency = ui32CurrentUpperFrequency * (100 + xgRuntimeParameters.ui8SpeedSwitchoffPercent) / 100;
	} else {
		ui32CurrentSwitchoffUpperFrequency = MUTING_OVERRIDE_FREQUENCY_HIGH;
	}

	if (xgRuntimeState.xProcess.ui1NewEvaluation && xgRuntimeState.xProcess.ui1ValidFrequency) {

		// Upper Limit: New Frequency
		//----------------------------
		if ((xgIpkTx.xLogic.ui1Limit1)) {

			// When frequency is higher than switchoff frequency then override timer
			if (xgRuntimeState.xSpeed.ui32MasterFrequency >= ui32CurrentSwitchoffUpperFrequency) {
				xgRuntimeState.xSpeed.rui32SwitchoffDelays[2] = 0;
			}

			if (xgRuntimeState.xSpeed.ui32MasterFrequency >= ui32CurrentUpperFrequency) {
				mCounterBasedSwitching(xgRuntimeState.xProcess.ui1WedgeCrossed, xgIpkTx.xLogic.ui1Limit1, xgRuntimeState.xSpeed.rui8MultiCaptureCounters[1],
						xgRuntimeState.xSpeed.rui32SwitchoffDelays[1]);
			} else {
				xgRuntimeState.xSpeed.rui32SwitchoffDelays[1] = xgRuntimeParameters.ui8SpeedSwitchoffDelay * DELAY_1MS_10MS;
				xgRuntimeState.xSpeed.rui8MultiCaptureCounters[1] = xgRuntimeParameters.ui8SpeedRepeatedCaptureCount;
			}

		} else if ((!xgIpkTx.xLogic.ui1Limit1 && xgRuntimeState.xSpeed.ui32MasterFrequency <= ui32CurrentUpperHysteresis)) {
			xgIpkTx.xLogic.ui1Limit1 = TRUE;
		}

		// Standstill Limit: New Frequency
		//----------------------------
		if ((xgIpkTx.xLogic.ui1Limit0)) {

			if (xgRuntimeState.xSpeed.ui32MasterFrequency >= ui32CurrentStandstillFrequency) {
				mCounterBasedSwitching(xgRuntimeState.xProcess.ui1WedgeCrossed, xgIpkTx.xLogic.ui1Limit0, xgRuntimeState.xSpeed.rui8MultiCaptureCounters[0],
						xgRuntimeState.xSpeed.rui32SwitchoffDelays[0]);
			} else {
				xgRuntimeState.xSpeed.rui32SwitchoffDelays[0] = (xgRuntimeParameters.ui1GeneralSpeedSettingsStandstillSwitchoffDelay) ? xgRuntimeParameters.ui8SpeedSwitchoffDelay * DELAY_1MS_10MS : 0;
				xgRuntimeState.xSpeed.rui8MultiCaptureCounters[0] = xgRuntimeParameters.ui8SpeedRepeatedCaptureCount;
			}
		} else if ((!xgIpkTx.xLogic.ui1Limit0) && xgRuntimeState.xSpeed.ui32MasterFrequency <= ui32CurrentStandstillHysteresis) {
			xgIpkTx.xLogic.ui1Limit0 = TRUE;
		}

	} else {
		// Upper Limit: Timeout case
		//----------------------------
		if (xgRuntimeState.xSpeed.rui32FrequencyTimeouts[1] >= ui32CurrentUpperTimeout) {
			if (xgRuntimeState.xSpeed.rui32SwitchoffDelays[1] == 0) {
				xgRuntimeState.xSpeed.rui32SwitchoffDelays[1] = xgRuntimeParameters.ui8SpeedSwitchoffDelay * DELAY_1MS_10MS;
				xgRuntimeState.xSpeed.rui8MultiCaptureCounters[1] = xgRuntimeParameters.ui8SpeedRepeatedCaptureCount;
				xgIpkTx.xLogic.ui1Limit1 = TRUE;
			}
		}

		// Standstill Limit: Timeout case
		//----------------------------
		if (xgRuntimeState.xSpeed.rui32FrequencyTimeouts[0] >= ui32CurrenttStandstillTimeout) {
			if (xgRuntimeState.xSpeed.rui32SwitchoffDelays[0] == 0) {
				xgRuntimeState.xSpeed.rui32SwitchoffDelays[0] = (xgRuntimeParameters.ui1GeneralSpeedSettingsStandstillSwitchoffDelay) ? xgRuntimeParameters.ui8SpeedSwitchoffDelay * DELAY_1MS_10MS : 0;
				xgRuntimeState.xSpeed.rui8MultiCaptureCounters[0] = xgRuntimeParameters.ui8SpeedRepeatedCaptureCount;
				xgIpkTx.xLogic.ui1Limit0 = TRUE;
			}
		}
	}
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Mode 2
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
static inline void vSpeedProcessLimitsWindow(void) {
	uint8_t ui8Level = xgIpkTx.xModifiers.ui2Validationbits;

	uint32_t ui32CurrentStandstillFrequency = xgRuntimeParameters.rui16FrequencyLimits[0][0];
	uint32_t ui32CurrentStandstillHysteresis = xgRuntimeParameters.rui16FrequencyHysteresis[0][0];
	uint32_t ui32CurrenttStandstillTimeout = xgRuntimeParameters.rui16FrequencyTimeouts[0][0];

	uint32_t ui32CurrentLowerFrequency;
	uint32_t ui32CurrentLowerHysteresis;
	uint32_t ui32CurrentLowerTimeout;

	uint32_t ui32CurrentUpperFrequency;
	uint32_t ui32CurrentUpperHysteresis;
	uint32_t ui32CurrentUpperTimeout;

	uint32_t ui32CurrentSwitchoffUpperFrequency;

	// Override when muted
	if (ui8OperatingMode & 0x08) {
		ui32CurrentLowerFrequency = MUTING_OVERRIDE_FREQUENCY_LOW;
		ui32CurrentLowerHysteresis = MUTING_OVERRIDE_HYSTERESIS_LOW;
		ui32CurrentLowerTimeout = MUTING_OVERRIDE_TIMEOUT_LOW;
		ui32CurrentUpperFrequency = MUTING_OVERRIDE_FREQUENCY_HIGH;
		ui32CurrentUpperHysteresis = MUTING_OVERRIDE_HYSTERESIS_HIGH;
		ui32CurrentUpperTimeout = MUTING_OVERRIDE_TIMEOUT_HIGH;

	} else {
		ui32CurrentLowerFrequency = xgRuntimeParameters.rui16FrequencyLimits[1][ui8Level];
		ui32CurrentLowerHysteresis = xgRuntimeParameters.rui16FrequencyHysteresis[1][ui8Level];
		ui32CurrentLowerTimeout = xgRuntimeParameters.rui16FrequencyTimeouts[1][ui8Level];
		ui32CurrentUpperFrequency = xgRuntimeParameters.rui16FrequencyLimits[2][ui8Level];
		ui32CurrentUpperHysteresis = xgRuntimeParameters.rui16FrequencyHysteresis[2][ui8Level];
		ui32CurrentUpperTimeout = xgRuntimeParameters.rui16FrequencyTimeouts[2][ui8Level];
	}

	// Abschaltverzögerungsschwelle einstellen
	if (xgRuntimeParameters.ui8SpeedSwitchoffPercent != 0) {
		ui32CurrentSwitchoffUpperFrequency = ui32CurrentUpperFrequency * (100 + xgRuntimeParameters.ui8SpeedSwitchoffPercent) / 100;
	} else {
		ui32CurrentSwitchoffUpperFrequency = MUTING_OVERRIDE_FREQUENCY_HIGH;
	}

	// Check frequency only if there is a new capture
	if (xgRuntimeState.xProcess.ui1NewEvaluation && xgRuntimeState.xProcess.ui1ValidFrequency) {

		// Upper Limit: New Frequency
		//----------------------------
		if ((xgIpkTx.xLogic.ui1Limit2)) {
			if (xgRuntimeState.xSpeed.ui32MasterFrequency >= ui32CurrentUpperFrequency) {

				// When frequency is higher than switchoff frequency then override timer
				if (xgRuntimeState.xSpeed.ui32MasterFrequency >= ui32CurrentSwitchoffUpperFrequency) {
					xgRuntimeState.xSpeed.rui32SwitchoffDelays[2] = 0;
				}
				mCounterBasedSwitching(xgRuntimeState.xProcess.ui1WedgeCrossed, xgIpkTx.xLogic.ui1Limit2, xgRuntimeState.xSpeed.rui8MultiCaptureCounters[2],
						xgRuntimeState.xSpeed.rui32SwitchoffDelays[2]);
			} else {
				xgRuntimeState.xSpeed.rui32SwitchoffDelays[2] = mGetRecalculatedSwitchoffDelay;
				xgRuntimeState.xSpeed.rui8MultiCaptureCounters[2] = xgRuntimeParameters.ui8SpeedRepeatedCaptureCount;
			}

		} else if ((!xgIpkTx.xLogic.ui1Limit2) && xgRuntimeState.xSpeed.ui32MasterFrequency <= ui32CurrentUpperHysteresis) {
			xgIpkTx.xLogic.ui1Limit2 = TRUE;
		}

		// Lower Limit: New Frequency
		//----------------------------
		if (!(xgIpkTx.xLogic.ui1Limit1)) {
			if (xgRuntimeState.xSpeed.ui32MasterFrequency <= ui32CurrentLowerFrequency) {
				mCounterBasedSwitchingSpecial(xgRuntimeState.xProcess.ui1WedgeCrossed, xgIpkTx.xLogic.ui1Limit1, xgRuntimeState.xSpeed.rui8MultiCaptureCounters[1],
						xgRuntimeState.xSpeed.rui32SwitchoffDelays[1]);
			} else {
				xgRuntimeState.xSpeed.rui32SwitchoffDelays[1] = mGetRecalculatedSwitchoffDelay;
				xgRuntimeState.xSpeed.rui8MultiCaptureCounters[1] = xgRuntimeParameters.ui8SpeedRepeatedCaptureCount;
			}

		} else if ((xgIpkTx.xLogic.ui1Limit1) && xgRuntimeState.xSpeed.ui32MasterFrequency >= ui32CurrentLowerHysteresis) {
			xgIpkTx.xLogic.ui1Limit1 = FALSE;
		}

		// Standstill Limit: New Frequency
		//----------------------------
		if ((xgIpkTx.xLogic.ui1Limit0)) {
			if (xgRuntimeState.xSpeed.ui32MasterFrequency >= ui32CurrentStandstillFrequency) {
				mCounterBasedSwitching(xgRuntimeState.xProcess.ui1WedgeCrossed, xgIpkTx.xLogic.ui1Limit0, xgRuntimeState.xSpeed.rui8MultiCaptureCounters[0],
						xgRuntimeState.xSpeed.rui32SwitchoffDelays[0]);
			} else {
				xgRuntimeState.xSpeed.rui32SwitchoffDelays[0] = (xgRuntimeParameters.ui1GeneralSpeedSettingsStandstillSwitchoffDelay) ? mGetRecalculatedSwitchoffDelay : 0;
				xgRuntimeState.xSpeed.rui8MultiCaptureCounters[0] = xgRuntimeParameters.ui8SpeedRepeatedCaptureCount;
			}
		} else if ((!xgIpkTx.xLogic.ui1Limit0) && xgRuntimeState.xSpeed.ui32MasterFrequency <= ui32CurrentStandstillHysteresis) {
			xgIpkTx.xLogic.ui1Limit0 = TRUE;
		}

	} else {
		// Upper Limit: Timeout case
		//----------------------------
		if (xgRuntimeState.xSpeed.rui32FrequencyTimeouts[2] >= ui32CurrentUpperTimeout) {
			if (xgRuntimeState.xSpeed.rui32SwitchoffDelays[2] == 0) {
				xgRuntimeState.xSpeed.rui32SwitchoffDelays[2] = mGetRecalculatedSwitchoffDelay;
				xgRuntimeState.xSpeed.rui8MultiCaptureCounters[2] = xgRuntimeParameters.ui8SpeedRepeatedCaptureCount;
				xgIpkTx.xLogic.ui1Limit2 = TRUE;
			}
		}

		//Lower  Limit: Timeout case
		//----------------------------
		if (xgRuntimeState.xSpeed.rui32FrequencyTimeouts[1] >= ui32CurrentLowerTimeout) {
			if (xgRuntimeState.xSpeed.rui32SwitchoffDelays[1] == 0) {
				xgRuntimeState.xSpeed.rui32SwitchoffDelays[1] = mGetRecalculatedSwitchoffDelay;
				xgRuntimeState.xSpeed.rui8MultiCaptureCounters[1] = xgRuntimeParameters.ui8SpeedRepeatedCaptureCount;
				xgIpkTx.xLogic.ui1Limit1 = TRUE;
			}
		}

		// Standstill Limit: Timeout case
		//----------------------------
		if (xgRuntimeState.xSpeed.rui32FrequencyTimeouts[0] >= ui32CurrenttStandstillTimeout) {
			if (xgRuntimeState.xSpeed.rui32SwitchoffDelays[0] == 0) {
				xgRuntimeState.xSpeed.rui32SwitchoffDelays[0] = (xgRuntimeParameters.ui1GeneralSpeedSettingsStandstillSwitchoffDelay) ? mGetRecalculatedSwitchoffDelay : 0;
				xgRuntimeState.xSpeed.rui8MultiCaptureCounters[0] = xgRuntimeParameters.ui8SpeedRepeatedCaptureCount;
				xgIpkTx.xLogic.ui1Limit0 = TRUE;
			}
		}
	}

}

/**-------------------------------------------------------------------------------------------------
 *    Description: Mode 3
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
static inline void vSpeedProcessLimitsScanner(void) {

	// Permanently 0 so there is no validation for this mode
	const uint8_t ui8Level = 0;

	uint32_t ui32CurrentFrequency3 = xgRuntimeParameters.rui16FrequencyLimits[3][ui8Level];
	uint32_t ui32CurrentrHysteresis3 = xgRuntimeParameters.rui16FrequencyHysteresis[3][ui8Level];
	uint32_t ui32CurrentTimeout3 = xgRuntimeParameters.rui16FrequencyTimeouts[3][ui8Level];

	uint32_t ui32CurrentFrequency2 = xgRuntimeParameters.rui16FrequencyLimits[2][ui8Level];
	uint32_t ui32CurrentrHysteresis2 = xgRuntimeParameters.rui16FrequencyHysteresis[2][ui8Level];
	uint32_t ui32CurrentTimeout2 = xgRuntimeParameters.rui16FrequencyTimeouts[2][ui8Level];

	uint32_t ui32CurrentFrequency1 = xgRuntimeParameters.rui16FrequencyLimits[1][ui8Level];
	uint32_t ui32CurrentrHysteresis1 = xgRuntimeParameters.rui16FrequencyHysteresis[1][ui8Level];
	uint32_t ui32CurrentTimeout1 = xgRuntimeParameters.rui16FrequencyTimeouts[1][ui8Level];

	uint32_t ui32CurrentFrequency0 = xgRuntimeParameters.rui16FrequencyLimits[0][ui8Level];
	uint32_t ui32CurrentrHysteresis0 = xgRuntimeParameters.rui16FrequencyHysteresis[0][ui8Level];
	uint32_t ui32CurrentTimeout0 = xgRuntimeParameters.rui16FrequencyTimeouts[0][ui8Level];

	// Check frequency only if there is a new capture, see if it is higher than the wedge and then check the status
	if (xgRuntimeState.xProcess.ui1NewEvaluation && xgRuntimeState.xProcess.ui1ValidFrequency) {

		// Frequency 4: New Frequency
		//----------------------------
		if ((xgIpkTx.xLogic.ui1Limit3)) {
			if (xgRuntimeState.xSpeed.ui32MasterFrequency >= ui32CurrentFrequency3) {
				mCounterBasedSwitching(xgRuntimeState.xProcess.ui1WedgeCrossed, xgIpkTx.xLogic.ui1Limit3, xgRuntimeState.xSpeed.rui8MultiCaptureCounters[3],
						xgRuntimeState.xSpeed.rui32SwitchoffDelays[3]);
			} else {
				xgRuntimeState.xSpeed.rui32SwitchoffDelays[3] = 0;	//xgRuntimeParameters.ui8SpeedSwitchoffDelay * DELAY_1MS_10MS;
				xgRuntimeState.xSpeed.rui8MultiCaptureCounters[3] = xgRuntimeParameters.ui8SpeedRepeatedCaptureCount;
			}
		} else if ((!xgIpkTx.xLogic.ui1Limit3) && xgRuntimeState.xSpeed.ui32MasterFrequency <= ui32CurrentrHysteresis3) {
			xgIpkTx.xLogic.ui1Limit3 = TRUE;
		}

		// Frequency 3: New Frequency
		//----------------------------
		if ((xgIpkTx.xLogic.ui1Limit2)) {
			if (xgRuntimeState.xSpeed.ui32MasterFrequency >= ui32CurrentFrequency2) {
				mCounterBasedSwitching(xgRuntimeState.xProcess.ui1WedgeCrossed, xgIpkTx.xLogic.ui1Limit2, xgRuntimeState.xSpeed.rui8MultiCaptureCounters[2],
						xgRuntimeState.xSpeed.rui32SwitchoffDelays[2]);
			} else {
				xgRuntimeState.xSpeed.rui32SwitchoffDelays[2] = 0;	//xgRuntimeParameters.ui8SpeedSwitchoffDelay * DELAY_1MS_10MS;
				xgRuntimeState.xSpeed.rui8MultiCaptureCounters[2] = xgRuntimeParameters.ui8SpeedRepeatedCaptureCount;
			}
		} else if ((!xgIpkTx.xLogic.ui1Limit2) && xgRuntimeState.xSpeed.ui32MasterFrequency <= ui32CurrentrHysteresis2) {
			xgIpkTx.xLogic.ui1Limit2 = TRUE;
		}

		// Frequency 2: New Frequency
		//----------------------------
		if ((xgIpkTx.xLogic.ui1Limit1)) {
			if (xgRuntimeState.xSpeed.ui32MasterFrequency >= ui32CurrentFrequency1) {
				mCounterBasedSwitching(xgRuntimeState.xProcess.ui1WedgeCrossed, xgIpkTx.xLogic.ui1Limit1, xgRuntimeState.xSpeed.rui8MultiCaptureCounters[1],
						xgRuntimeState.xSpeed.rui32SwitchoffDelays[1]);
			} else {
				xgRuntimeState.xSpeed.rui32SwitchoffDelays[1] = 0;	//xgRuntimeParameters.ui8SpeedSwitchoffDelay * DELAY_1MS_10MS;
				xgRuntimeState.xSpeed.rui8MultiCaptureCounters[1] = xgRuntimeParameters.ui8SpeedRepeatedCaptureCount;
			}
		} else if ((!xgIpkTx.xLogic.ui1Limit1) && xgRuntimeState.xSpeed.ui32MasterFrequency <= ui32CurrentrHysteresis1) {
			xgIpkTx.xLogic.ui1Limit1 = TRUE;
		}

		// Frequency 1: New Frequency
		//----------------------------
		if ((xgIpkTx.xLogic.ui1Limit0)) {
			if (xgRuntimeState.xSpeed.ui32MasterFrequency >= ui32CurrentFrequency0) {
				mCounterBasedSwitching(xgRuntimeState.xProcess.ui1WedgeCrossed, xgIpkTx.xLogic.ui1Limit0, xgRuntimeState.xSpeed.rui8MultiCaptureCounters[0],
						xgRuntimeState.xSpeed.rui32SwitchoffDelays[0]);
			} else {
				xgRuntimeState.xSpeed.rui32SwitchoffDelays[0] = 0;		//sxgRuntimeParameters.ui8SpeedSwitchoffDelay * DELAY_1MS_10MS;
				xgRuntimeState.xSpeed.rui8MultiCaptureCounters[0] = xgRuntimeParameters.ui8SpeedRepeatedCaptureCount;
			}
		} else if ((!xgIpkTx.xLogic.ui1Limit0) && xgRuntimeState.xSpeed.ui32MasterFrequency <= ui32CurrentrHysteresis0) {
			xgIpkTx.xLogic.ui1Limit0 = TRUE;
		}

	} else {

		// Frequency 3 : Timeout case
		//----------------------------
		if (xgRuntimeState.xSpeed.rui32FrequencyTimeouts[3] >= ui32CurrentTimeout3) {
			xgRuntimeState.xSpeed.rui32SwitchoffDelays[3] = 0;		//xgRuntimeParameters.ui8SpeedSwitchoffDelay * DELAY_1MS_10MS;
			xgRuntimeState.xSpeed.rui8MultiCaptureCounters[3] = xgRuntimeParameters.ui8SpeedRepeatedCaptureCount;
			xgIpkTx.xLogic.ui1Limit3 = TRUE;
		}

		// Frequency 2 : Timeout case
		//----------------------------
		if (xgRuntimeState.xSpeed.rui32FrequencyTimeouts[2] >= ui32CurrentTimeout2) {
			xgRuntimeState.xSpeed.rui32SwitchoffDelays[2] = 0;		//xgRuntimeParameters.ui8SpeedSwitchoffDelay * DELAY_1MS_10MS;
			xgRuntimeState.xSpeed.rui8MultiCaptureCounters[2] = xgRuntimeParameters.ui8SpeedRepeatedCaptureCount;
			xgIpkTx.xLogic.ui1Limit2 = TRUE;
		}

		// Frequency 1 : Timeout case
		//----------------------------
		if (xgRuntimeState.xSpeed.rui32FrequencyTimeouts[1] >= ui32CurrentTimeout1) {
			xgRuntimeState.xSpeed.rui32SwitchoffDelays[1] = 0;		//xgRuntimeParameters.ui8SpeedSwitchoffDelay * DELAY_1MS_10MS;
			xgRuntimeState.xSpeed.rui8MultiCaptureCounters[1] = xgRuntimeParameters.ui8SpeedRepeatedCaptureCount;
			xgIpkTx.xLogic.ui1Limit1 = TRUE;
		}

		// Frequency 0 : Timeout case
		//----------------------------
		if (xgRuntimeState.xSpeed.rui32FrequencyTimeouts[0] >= ui32CurrentTimeout0) {
			xgRuntimeState.xSpeed.rui32SwitchoffDelays[0] = 0;		//xgRuntimeParameters.ui8SpeedSwitchoffDelay * DELAY_1MS_10MS;
			xgRuntimeState.xSpeed.rui8MultiCaptureCounters[0] = xgRuntimeParameters.ui8SpeedRepeatedCaptureCount;
			xgIpkTx.xLogic.ui1Limit0 = TRUE;
		}
	}
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Process and Evaluate Acceleration
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vAccelerationProcess(void) {

}

/**-------------------------------------------------------------------------------------------------
 *    Description: Process and Evaluate Direction
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vDirectionProcess(void) {

	// Execute only if the direction is activated
	if (!xgRuntimeParameters.ui1GeneralSpeedSettingsNDirectionActivation) {

		// Direction either LR or RL
		xgIpkTx.xLogic.ui1Direction = xgRuntimeState.xDirection.ui1Direction ^ xgRuntimeParameters.ui1GeneralSpeedSettingsDirection;

		// Direction is parsed only between 5 and 100 Hz. Hysteresis timeout causes the direction to switch to right
		// Direction tolerance sets the amount of periods to be processed in both directions
		//----------------------------------------------------------------------------------------------------------
		if (!xgRuntimeState.xSpeed.ui32HysteresisTimeout || xgRuntimeState.xSpeed.ui32MasterFrequency < FREQ_5HZ) {
			xgRuntimeState.xDirection.ui16DirectionRight = DIRECTION_HYSTERESIS;

		} else if (xgRuntimeState.xSpeed.ui32MasterFrequency < FREQ_100HZ) {

			// Increment the counter only if direction bit is set
			if (xgIpkTx.xLogic.ui1Direction) {
				if (xgRuntimeState.xDirection.ui16DirectionRight < DIRECTION_HYSTERESIS) {
					xgRuntimeState.xDirection.ui16DirectionRight += 1;
				}
			} else if (xgRuntimeState.xDirection.ui16DirectionRight > 0) {
				xgRuntimeState.xDirection.ui16DirectionRight -= 1;
			}
		}

		// The hysteresis controls the the direction bit, so it wouldn't toggle
		if (xgRuntimeState.xDirection.ui16DirectionRight == DIRECTION_HYSTERESIS) {
			xgIpkTx.xSpeedEmk.ui1Q2 = TRUE;
		} else if (xgRuntimeState.xDirection.ui16DirectionRight == 0) {
			xgIpkTx.xSpeedEmk.ui1Q2 = FALSE;
		}

		//mDebugPutIntoArray0(xgRuntimeState.xDirection.ui16DirectionRight);

		// Richtungsbit ist abhängig von Stop
		xgIpkTx.xSpeedEmk.ui1Q2 |= (xgRuntimeParameters.ui1GeneralSpeedSettingsDirectionStop & xgIpkTx.xLogic.ui1Limit0);

		// Check if the switching is too fast /SR0023
		vDirectionCheckRotationSwitch();

	} else {
		xgIpkTx.xSpeedEmk.ui1Q2 = FALSE;
	}
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Evaluates speed logic
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
static inline void vSpeedProcess(void) {
	ui8OperatingMode = (xgIpkTx.xModifiers.ui4Operatingstate);	//  & xgIpkRx.xModifiers.ui4Operatingstate); // Keine Verundung, da Q's verundet sind!

	// Multi-Capture: Flag is set or unset
	//-------------------------------------------------------------------------------
	if (xgRuntimeState.xSpeed.ui32MasterFrequency >= xgRuntimeParameters.ui16SpeedRepeatedCaptureWedge) {
		xgRuntimeState.xProcess.ui1WedgeCrossed = TRUE;
	} else {
		xgRuntimeState.xProcess.ui1WedgeCrossed = FALSE;
	}




	// Higher Mode has priority over others
	// Limits are in ProcessLimitsMode functions
	// Qs are set in AcknoledgementMode functions
	//---------------------------------
	switch (xgRuntimeParameters.ui8SpeedMode) {
	case 1:
		vParameterCalculateDncoFrequencies(0);
		vAccelerationProcess();
		vDirectionProcess();
		vSpeedProcessLimitsStandard();
		vSpeedAcknowledgeMode1();
		break;
	case 2:
		vParameterCalculateDncoFrequencies(0);
		vAccelerationProcess();
		vDirectionProcess();
		vSpeedProcessLimitsStandard();
		vSpeedAcknowledgeMode2();
		break;
	case 3:
		vAccelerationProcess();
		vDirectionProcess();
		vSpeedProcessLimitsWindow();
		vSpeedAcknowledgeMode3();

		break;
	case 4:
		vAccelerationProcess();
		vDirectionProcess();
		vSpeedProcessLimitsWindow();
		vSpeedAcknowledgeMode4();
		break;
	case 5:
		vAccelerationProcess();
		vDirectionProcess();
		vSpeedProcessLimitsWindow();
		vSpeedAcknowledgeMode5();
		break;
	case 6:
		vAccelerationProcess();
		vDirectionProcess();
		vSpeedProcessLimitsWindow();
		vSpeedAcknowledgeMode6();
		break;
	case 7:
		vSpeedProcessLimitsScanner();
		vSpeedAcknowledgeMode7();
		break;
	default:
		xgIpkTx.xSpeedEmk.ui4Q = 0;				// Do nothing
		break;
	}

	// /SR0022 Active phaseshift monitoring.
	// Phaseshiftmonitoring can run without direction logic
	if (xgRuntimeParameters.ui8SpeedPhaseShiftMonitoring) {
		vSpeedCheckPhase();
	}

	// /SR0014 Check if frequency is over 1500 h.
	vSpeedCheck1500Hz();

	// /SR0013 Compare frequencies
	vSpeedCheckFrequenciesNotEqual();

	// /SR0016 Check for stuckat
	vSpeedCheckQuittStuckat();

	// Sicherer Stillstand
	//---------------------------------

	// SRS Errors: Safety Precautions.
	// Muting without sensors
	// Safe Stop
	//-------------------------------------
	if (xgIpkTx.ui8Errors2) {
		if (!(xgRuntimeParameters.ui1GeneralSpeedSettingsMuteWithoutSensors && mGetBypass)) {
			if (xgRuntimeParameters.ui1GeneralSpeedSettingsSafeStop) {
				xgIpkTx.xSpeedEmk.ui1Q0 = FALSE;
			}

			xgIpkTx.xSpeedEmk.ui1Q1 = FALSE; // TODO
			xgIpkTx.xSpeedEmk.ui1Q2 = FALSE; // TODO
			xgIpkTx.xSpeedEmk.ui1Q3 = FALSE; // TODO

		}
	}
}

/**-------------------------------------------------------------------------------------------------
 *    Description:  Process captured time period between rising edges
 *					This function contains the logic for speed monitoring and direction monitoring
 *					Changing Schmitttrigger hysteresis
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vSpeedExecute(uint8_t *pui8CheckinCounter) {

	static uint8_t ui8TimeDivider10ms = 0;
	static uint32_t ui32FrequencyRegion = 0;
	static uint32_t ui32MoveFrequencyRegionUp = 0;
	static uint32_t ui32MovefrequencyRegionDown = 0;
	uint8_t ui8SpeedCurrentQ;

#ifndef DEBUG_DISABLE_DUALCHANNEL_AND
	uint8_t ui8SpeedPreviousQ = xgIpkTx.xSpeedEmk.ui4Q & xgIpkRx.xSpeedEmk.ui4Q;
#else
	uint8_t ui8SpeedPreviousQ = xgIpkTx.xSpeedEmk.ui4Q;
#endif

	++(*pui8CheckinCounter);

	// Create a local copy
	//----------------------------------------------------------------
	uint8_t ui8CurrentHysteresisState = xgIpkTx.xStatus.ui2Hysteresis;
	xgRuntimeState.xProcess.ui1NewEvaluation = FALSE;

	//	Update timeouts
	if (xgRuntimeState.xCaptured.ui1InterruptNewCapturePending) {
		ui8TimeDivider10ms = 0;

		__disable_irq();
		xgRuntimeState.xCaptured.ui1InterruptNewCapturePending = FALSE;
		uint32_t ui32TempFrequency = xgRuntimeState.xCaptured.ui32InterruptCapturedFrequency;
		uint8_t ui8TempDirection = xgRuntimeState.xCaptured.ui1InterruptDirectionCapture;
		__enable_irq();

		xgRuntimeState.xProcess.ui1NewEvaluation = TRUE;
		xgRuntimeState.xSpeed.ui32Timeout100mHz = DELAY_1MS_11S;
		xgRuntimeState.xSpeed.ui32Timeout500mHz = DELAY_1MS_3S;
		xgRuntimeState.xSpeed.ui32HysteresisTimeout = DELAY_1MS_1S;

		// Process captured information only if it is valid.
		//-------------------------------------------------
		if (xgRuntimeState.xProcess.ui1ValidFrequency) {

			// Update only when there is something new: Frequency and Direction
			//------------------------------------------
			xgRuntimeState.xSpeed.ui32MasterFrequency = ui32TempFrequency;
			xgRuntimeState.xDirection.ui1Direction = ((ui8TempDirection));

			// Timeout reset
			//-----------------------------------------
			xgRuntimeState.xSpeed.rui32FrequencyTimeouts[0] = 0;
			xgRuntimeState.xSpeed.rui32FrequencyTimeouts[1] = 0;
			xgRuntimeState.xSpeed.rui32FrequencyTimeouts[2] = 0;
			xgRuntimeState.xSpeed.rui32FrequencyTimeouts[3] = 0;

			//////////////////////////////////////////////////////////////
			// Region switching for Hysteresis control
			//--------------------------------------------------------------------
			switch (ui32FrequencyRegion) {
			case 0:
				if (xgRuntimeState.xSpeed.ui32MasterFrequency >= FREQ_10HZ) {
					ui32MoveFrequencyRegionUp++;
					ui32MovefrequencyRegionDown = 0;
				}
				break;
			case 1:
				if (xgRuntimeState.xSpeed.ui32MasterFrequency >= FREQ_25HZ) {
					ui32MoveFrequencyRegionUp++;
					ui32MovefrequencyRegionDown = 0;
				} else if (xgRuntimeState.xSpeed.ui32MasterFrequency < FREQ_10HZ) {
					ui32MoveFrequencyRegionUp = 0;
					ui32MovefrequencyRegionDown++;
				}
				break;
			case 2:
				if (xgRuntimeState.xSpeed.ui32MasterFrequency >= FREQ_75HZ) {
					ui32MoveFrequencyRegionUp++;
					ui32MovefrequencyRegionDown = 0;
				} else if (xgRuntimeState.xSpeed.ui32MasterFrequency < FREQ_25HZ) {
					ui32MoveFrequencyRegionUp = 0;
					ui32MovefrequencyRegionDown++;
				}
				break;
			case 3:
				if (xgRuntimeState.xSpeed.ui32MasterFrequency >= FREQ_100HZ) {
					ui32MoveFrequencyRegionUp++;
					ui32MovefrequencyRegionDown = 0;
				} else if (xgRuntimeState.xSpeed.ui32MasterFrequency < FREQ_75HZ) {
					ui32MoveFrequencyRegionUp = 0;
					ui32MovefrequencyRegionDown++;
				}
				break;
			case 4:
				if (xgRuntimeState.xSpeed.ui32MasterFrequency >= FREQ_150HZ) {
					ui32MoveFrequencyRegionUp++;
					ui32MovefrequencyRegionDown = 0;
				} else if (xgRuntimeState.xSpeed.ui32MasterFrequency < FREQ_100HZ) {
					ui32MoveFrequencyRegionUp = 0;
					ui32MovefrequencyRegionDown++;
				}
				break;
			case 5:
				if (xgRuntimeState.xSpeed.ui32MasterFrequency >= FREQ_200HZ) {
					ui32MoveFrequencyRegionUp++;
					ui32MovefrequencyRegionDown = 0;
				} else if (xgRuntimeState.xSpeed.ui32MasterFrequency < FREQ_150HZ) {
					ui32MoveFrequencyRegionUp = 0;
					ui32MovefrequencyRegionDown++;
				}
				break;
			case 6:
				if (xgRuntimeState.xSpeed.ui32MasterFrequency < FREQ_200HZ) {
					ui32MoveFrequencyRegionUp = 0;
					ui32MovefrequencyRegionDown++;
				}
				break;
			};

			// Zwei Perioden oberhalb der Schranke  bewirken Hochschaltung
			if (ui32MoveFrequencyRegionUp == 2) {
				ui32MoveFrequencyRegionUp = 0;
				if (ui32FrequencyRegion != 6) {
					ui32FrequencyRegion++;
				}
			}
			// Zwei Perioden unterhalb der Schranke  bewirken Hochschaltung
			else if (ui32MovefrequencyRegionDown == 2) {
				ui32MovefrequencyRegionDown = 0;
				if (ui32FrequencyRegion != 0) {
					ui32FrequencyRegion--;
				}
			}

		} else {

			xgRuntimeState.xProcess.ui1ValidFrequency = TRUE;
		}

	} else {

		// 0,1 Hz timeout
		if (xgRuntimeState.xSpeed.ui32Timeout100mHz > 0) {
			--xgRuntimeState.xSpeed.ui32Timeout100mHz;
		}

		// 0,5 Hz timeout
		if (xgRuntimeState.xSpeed.ui32Timeout500mHz > 0) {
			--xgRuntimeState.xSpeed.ui32Timeout500mHz;
		}

		// Hysteresis timeout
		if (xgRuntimeState.xSpeed.ui32HysteresisTimeout > 0) {
			--xgRuntimeState.xSpeed.ui32HysteresisTimeout;
		}

		// Frequency timeouts timeout
		// The timeouts are used by the Mode functions
		if (++ui8TimeDivider10ms >= DIVIDER_10MS) {
			ui8TimeDivider10ms = 0;
			++xgRuntimeState.xSpeed.rui32FrequencyTimeouts[0];
			++xgRuntimeState.xSpeed.rui32FrequencyTimeouts[1];
			++xgRuntimeState.xSpeed.rui32FrequencyTimeouts[2];
			++xgRuntimeState.xSpeed.rui32FrequencyTimeouts[3];
		}

		// Reset measured frequency to 0,5 Hz
		if (!xgRuntimeState.xSpeed.ui32Timeout500mHz) {
			xgRuntimeState.xProcess.ui1ValidFrequency = FALSE;
			xgRuntimeState.xSpeed.ui32MasterFrequency = FREQ_0_1HZ;
		}

		// Reset Hysteresis to the biggest one
		if (!xgRuntimeState.xSpeed.ui32HysteresisTimeout) {
			ui32FrequencyRegion = 0;
			ui32MoveFrequencyRegionUp = 0;
			ui32MovefrequencyRegionDown = 0;
			xgRuntimeState.xDirection.ui1Direction = TRUE;
			xgRuntimeState.xDirection.ui16DirectionRight = 0xFF;
		}
	}

	// Evaluate the frequency only if the parameters are all there and set
	// Update Limits, Qs
	//--------------------------------------------------------------------
	if (xgRuntimeState.ui8ParameterComplete) {

		// Switch Hysteresis according to Hyst configuration and Measured Frequency
		//--------------------------------------------------------------------------------------
		switch (ui32FrequencyRegion) {
		case 0:
			// Hysterese
			if (xgRuntimeParameters.ui8SpeedHysteresisMode == HYSTERESIS_MODE_00) {
				ui8CurrentHysteresisState = 0x02;
			} else if (xgRuntimeParameters.ui8SpeedHysteresisMode == HYSTERESIS_MODE_01) {
				ui8CurrentHysteresisState = 0x03;
			} else if (xgRuntimeParameters.ui8SpeedHysteresisMode == HYSTERESIS_MODE_02) {
				ui8CurrentHysteresisState = 0x00;
			} else if (xgRuntimeParameters.ui8SpeedHysteresisMode == HYSTERESIS_MODE_03) {
				ui8CurrentHysteresisState = 0x01;
			}
			break;
		case 1:
			// Hysterese graue Zone, nichts umschalten
			break;
		case 2:
		case 3:
		case 4:
			// Hysterese
			if (xgRuntimeParameters.ui8SpeedHysteresisMode == HYSTERESIS_MODE_03) {
				ui8CurrentHysteresisState = 0x01;
			} else {
				ui8CurrentHysteresisState = 0x00;
			}
			break;
		case 5:
			// Hysterese graue Zone, nichts umschalten
			// Kap.Kopp
			break;
		case 6:
			// Hysterese
			ui8CurrentHysteresisState = 0x01;
			break;
		}

		// Process captured frequency only if there are parameters
		vSpeedProcess();
	} else {
		ui32FrequencyRegion = 6;
		ui8CurrentHysteresisState = 0x01;
		xgIpkTx.xSpeedEmk.ui4Q = 0;		// Enforce 0x00
	}

	// Save IPK State into the latch buffer
	uint8_t ui8TempSpeedMode = xgRuntimeParameters.ui8SpeedMode;
#ifndef DEBUG_DISABLE_DUALCHANNEL_AND
	ui8SpeedCurrentQ = xgIpkTx.xSpeedEmk.ui4Q & xgIpkRx.xSpeedEmk.ui4Q;
#else
	ui8SpeedCurrentQ = xgIpkTx.xSpeedEmk.ui4Q;
#endif
	if (ui8TempSpeedMode > 0) {

		// For mode 7
		if ((ui8TempSpeedMode == 7)) {
			if (ui8SpeedPreviousQ && !ui8SpeedCurrentQ) {
				vIpkSaveLatchBuffer();
			}
			// For other modes
		} else if ((ui8SpeedPreviousQ & ~ui8SpeedCurrentQ) & 0x02) {
			vIpkSaveLatchBuffer();
		}
	}

	// Switchoff-Delays
	//-----------------------------------------
	for (uint8_t ui8Index = 0; ui8Index < FREQUENCY_LEVELS; ++ui8Index) {
		if (xgRuntimeState.xSpeed.rui32SwitchoffDelays[ui8Index]) {
			--xgRuntimeState.xSpeed.rui32SwitchoffDelays[ui8Index];
		}
	}

	// Set Timeout flag
	//-----------------------------------------
	if (!xgRuntimeState.xSpeed.ui32HysteresisTimeout) {
		xgIpkTx.xStatus.ui1Timeout1s = TRUE;
	} else {
		xgIpkTx.xStatus.ui1Timeout1s = FALSE;
	}

	// Update Hysteresis
	//---------------------------------------------
	switch (ui8CurrentHysteresisState) {
	case 0x00:
		mHardwareSetHysteresis00;
		break;
	case 0x01:
		mHardwareSetHysteresis01;
		break;
	case 0x02:
		mHardwareSetHysteresis10;
		break;
	case 0x03:
		mHardwareSetHysteresis11;
		break;
	}

	// Extras
	//----------------------------------
	mHardwareEnableCapaciveCoupling;					// MOSFET is OFF, so the capacitor is working

	// Update IPK
	//-----------------------------------------------
	xgIpkTx.xStatus.ui2Hysteresis = ui8CurrentHysteresisState;
	xgIpkTx.ui8FrequencyHighbyte = (xgRuntimeState.xSpeed.ui32MasterFrequency) >> 8;
	xgIpkTx.ui8FrequencyLowbyte = (xgRuntimeState.xSpeed.ui32MasterFrequency);

}
