/*
 * File:   pdsv-parameters.c
 * Author: am
 * 10.01.2021
 */

#include "pdsv-parameters.h"
#include "safe/safe-speed.h"
#include "pdsv-diagnostics.h"
#include "pdsv-ipk.h"
#include "pdsv-fdcan.h"
#include "helpers.h"

/* Private defines */
#define ERROR_PARAMETER_COPY_100MS				 5				// 500 ms
#define ERROR_PARAMETER_CHECKSUM_EXCHANGE_100MS  10

/* Public variables*/
RuntimeParameters_t xgRuntimeParameters;
RuntimeParameters_t xgRuntimeParametersCopied;
ReceivedParameters_t xgReceivedParameters;

/* Private functions */
static void vParametersCalculateSpeedMode(uint8_t ui8SpeedMode);
static void vParametersCalculateEmkMode(uint8_t ui8EmkMode);

uint16_t ui16DebugCalculatedChecksum = 0;
/**-----------------------------------------------------------------
 *
 * 		Loads standard parameters for debugging
 *
 *-----------------------------------------------------------------*/
void vLoadDebugParameters(void) {

//#define DEBUG_ARAPMS_FROM_BUFFER
#ifdef DEBUG_ARAPMS_FROM_BUFFER

	 uint8_t rui8DebugParameterBuffer[] =
	 {
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x64, 0x00, 0xC8, 0x01, 0x2C, 0x01, 0x90, 0x00, 0xC8, 0x00, 0x64, 0x02, 0xBC, 0x00, 0x0A,
	 0x00, 0x80, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	 };
	 memcpy(xgReceivedParameters.rui8Buffer, rui8DebugParameterBuffer, sizeof(rui8DebugParameterBuffer));

#else

#define SKR_ACTIVE 0x07
#define DIR_N_ACTIVE 0
#define DIR_OR_STOP 0
#define DIR_REVERSED 0
#define CAN_UPDATE 	10
#define SS1_SPEED	100u
#define F11_SPEED	200u
#define F12_SPEED	300u
#define F13_SPEED	400u
#define SPEED_REP_CAPTURE_WEDGE 200
#define SPEED_RUNTIME_TIME	500
#define EMK_SENSITIVITY 200
#define EMK_DELAY	0
#define SPEED_Switchoff_DELAY	2
#define SPEED_REP_CAPTURE_COUNT 0
#define SPEED_HYSTERESIS_MODE 0
#define SPEED_SPINOFF_FACTOR 0
#define SPEED_PHASE_MONITORING 0
#define SPEED_MODE	1
#define EMK_MODE 1
#define ACKNOWLEDGEMENT 1

	xgReceivedParameters.rui8Buffer[8] = SKR_ACTIVE;
	xgReceivedParameters.rui8Buffer[9] = 0x00;
	xgReceivedParameters.rui8Buffer[10] = (DIR_N_ACTIVE << 7) | (DIR_OR_STOP << 6) | (DIR_REVERSED << 5);
	xgReceivedParameters.rui8Buffer[16] = CAN_UPDATE;
	xgReceivedParameters.rui8Buffer[32] = (SS1_SPEED) >> 8;
	xgReceivedParameters.rui8Buffer[33] = (uint8_t) (SS1_SPEED);
	xgReceivedParameters.rui8Buffer[34] = (F11_SPEED) >> 8;
	xgReceivedParameters.rui8Buffer[35] = (uint8_t) (F11_SPEED);
	xgReceivedParameters.rui8Buffer[36] = (F12_SPEED) >> 8;
	xgReceivedParameters.rui8Buffer[37] = (uint8_t) (F12_SPEED);
	xgReceivedParameters.rui8Buffer[38] = (F13_SPEED) >> 8;
	xgReceivedParameters.rui8Buffer[39] = (uint8_t) (F13_SPEED);

	xgReceivedParameters.rui8Buffer[40] = (SPEED_REP_CAPTURE_WEDGE) >> 8;
	xgReceivedParameters.rui8Buffer[41] = (uint8_t) (SPEED_REP_CAPTURE_WEDGE);

	xgReceivedParameters.rui8Buffer[42] = (SPEED_RUNTIME_TIME) >> 8;
	xgReceivedParameters.rui8Buffer[43] = (uint8_t) (SPEED_RUNTIME_TIME);

	xgReceivedParameters.rui8Buffer[44] = (EMK_SENSITIVITY) >> 8;
	xgReceivedParameters.rui8Buffer[45] = (uint8_t) (EMK_SENSITIVITY);

	xgReceivedParameters.rui8Buffer[46] = (EMK_DELAY) >> 8;
	xgReceivedParameters.rui8Buffer[47] = (uint8_t) (EMK_DELAY);

	xgReceivedParameters.rui8Buffer[48] = (SPEED_Switchoff_DELAY);
	xgReceivedParameters.rui8Buffer[49] = ((SPEED_PHASE_MONITORING & 0x01) << 7) | ((SPEED_SPINOFF_FACTOR & 0x07) << 4) | ((SPEED_HYSTERESIS_MODE & 0x03) << 2) | (SPEED_REP_CAPTURE_COUNT & 0x03);

	xgReceivedParameters.rui8Buffer[50] = ((ACKNOWLEDGEMENT) << 7) | ((EMK_MODE & 0x07) << 4) | ((SPEED_MODE & 0x0F));

	xgReceivedParameters.rui8Buffer[54] = 0;
	xgReceivedParameters.rui8Buffer[55] = 0;/**/

#endif
	vParameterCheckDesignerChecksum();
	vParameterCheckPlausibility();

	//Calculate checksum for comparison between channels
	uint16_t ui16TempChecksum = ui16GenerateCRC16((uint8_t*) &xgRuntimeParameters, 128, POLY_RUNTIME_PARAM, CRC_INITIAL);
	ui16TempChecksum = ui16GenerateCRC16((uint8_t*) &xgReceivedParameters.rui8ReceivedDnco, 256, POLY_RUNTIME_PARAM, ui16TempChecksum);

	xgIpkTx.xStatus.ui1ParameterComplete = TRUE;
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Parameter are complete only if boths sides agree
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vParameterCheckComplete(uint8_t *pui8CheckinCounter) {
	++(*pui8CheckinCounter);

	xgRuntimeState.ui8ParameterComplete = xgIpkTx.xStatus.ui1ParameterComplete & xgIpkRx.xStatus.ui1ParameterComplete;
#ifdef DEBUG_SKIP_SLOT_EXCHANGE
	xgRuntimeState.ui8ParameterComplete = TRUE;
#endif

}

/**-------------------------------------------------------------------------------------------------
 *    Description: Compare exchanged checksums between two channels /SR00018
 *    Frequency:  100ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vParameterCheckExchangedChecksum(uint8_t *pui8CheckinCounter) {
	++(*pui8CheckinCounter);

	// Compare checksums only if the flags have been set
	if (xgIpkTx.xStatus.ui1ParameterComplete) {
		if (xgIpkTx.ui16Checksum != xgIpkRx.ui16Checksum) {
			++xgErrorCounters.ui8ErrorCounterParamExchange;
		} else if (xgErrorCounters.ui8ErrorCounterParamExchange > 0) {
			--xgErrorCounters.ui8ErrorCounterParamExchange;
		}

		if (xgErrorCounters.ui8ErrorCounterParamExchange > ERROR_PARAMETER_CHECKSUM_EXCHANGE_100MS) {
			xgIpkTx.xErrors0.ui1ErrorChecksumExchange = TRUE;
		}
	}

#ifdef DEBUG_SKIP_SLOT_EXCHANGE
	DEBUGCODE(xgIpkTx.xErrors0.ui1ErrorChecksumExchange = FALSE;);
#endif
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Calculate and Validate Parameters /SR0002
 *    Frequency:  once
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vParameterCheckPlausibility(void) {
	uint32_t ui32Temp;

	// Inputs
	//------------------------------------------------------
	ui32Temp = xgReceivedParameters.rui8Buffer[0];
	xgRuntimeParameters.ui8GeneralInputDebounce = ui32Temp;

	// Skrs
	//------------------------------------------------------
	// Querschluss
	ui32Temp = xgReceivedParameters.rui8Buffer[2];
	xgRuntimeParameters.ui8SafetyCircuitsCrosscircuitMode = ui32Temp & 0x03;

	// Quittierung
	ui32Temp = xgReceivedParameters.rui8Buffer[3];
	xgRuntimeParameters.ui8SafetyCircuitsAcknowledgementMode = ui32Temp & 0x03;

	// Quittierungsverzögerung
	ui32Temp = xgReceivedParameters.rui8Buffer[4];
	xgRuntimeParameters.ui8SafetyCircuitsAcknowledgementDelay = ui32Temp & 0x03;

	// Antivalenz
	ui32Temp = xgReceivedParameters.rui8Buffer[5];
	xgRuntimeParameters.ui8SafetyCircuitsAntivalent = ui32Temp & 0x03;

	// Grundstellung
	ui32Temp = xgReceivedParameters.rui8Buffer[6];
	xgRuntimeParameters.ui8SafetyCircuitsInitialStateCheck = ui32Temp & 0x03;

	// Quittierungspegel
	ui32Temp = xgReceivedParameters.rui8Buffer[7];
	xgRuntimeParameters.ui8SafetyCircuitsAcknowledgementLevel = ui32Temp & 0x03;

	// Aktivierung
	ui32Temp = xgReceivedParameters.rui8Buffer[8];
	xgRuntimeParameters.ui8SafetyCircuitsActivation = ui32Temp & 0x07;

	// Kombinatorisches Byte
	// -----------------------------------------------
	ui32Temp = xgReceivedParameters.rui8Buffer[10];
	xgRuntimeParameters.ui8GeneralSpeedSettings = ui32Temp;

	// DNCO
	//-----------------------------------------------
	ui32Temp = xgReceivedParameters.rui8Buffer[12];
	xgRuntimeParameters.ui8DncoMode = ui32Temp;

	// CAN
	//---------------------------------------------
	ui32Temp = xgReceivedParameters.rui8Buffer[16];
	mValidateCanRefreshTime(xgIpkTx.xErrors1.ui1ErrorParameter, ui32Temp);
	xgRuntimeParameters.ui8GeneralCanRefreshTime = ui32Temp; // 10ms pace

	// Emk-Überwachungsparameter
	//--------------------------
	ui32Temp = (xgReceivedParameters.rui8Buffer[50] >> 4) & 0x07;
	xgRuntimeParameters.ui8EmkMode = ui32Temp;
	vParametersCalculateEmkMode(xgRuntimeParameters.ui8EmkMode);

	// Drehzahlüberwachungsparameter
	//-----------------------------------------
	ui32Temp = (((uint32_t) xgReceivedParameters.rui8Buffer[42]) << 8) | xgReceivedParameters.rui8Buffer[43];	// Anlaufzeit hat auswirkung auf den Modus
	switch (xgReceivedParameters.rui8Buffer[50] & 0x8F) {
	case 0x00:
		break;

		// Automatic Acknowledgement
	case 0x01:
		xgRuntimeParameters.ui8SpeedMode = 1;
		break;
	case 0x02:
		if (ui32Temp)
			xgRuntimeParameters.ui8SpeedMode = 4;
		else
			xgRuntimeParameters.ui8SpeedMode = 3;
		break;
		// Manual Acknowledgement
	case 0x81:
		xgRuntimeParameters.ui8SpeedMode = 2;
		break;
	case 0x82:
		if (ui32Temp)
			xgRuntimeParameters.ui8SpeedMode = 6;
		else
			xgRuntimeParameters.ui8SpeedMode = 5;
		break;

		// Bereich
	case 0x83:
	case 0x03:
		xgRuntimeParameters.ui8SpeedMode = 7;
		break;
	default:
		xgRuntimeParameters.ui8SpeedMode = 0;
		break;
	}

	vParametersCalculateSpeedMode(xgRuntimeParameters.ui8SpeedMode);

	// Create a copy of parameters and calculate CRC16 for comparison between channels
	//-----------------------------------------------------------------------------
	memcpy(&xgRuntimeParametersCopied, &xgRuntimeParameters, sizeof(xgRuntimeParameters));
	memcpy(xgReceivedParameters.rui8ReceivedDncoCopied, xgReceivedParameters.rui8ReceivedDnco, 256);

	uint16_t ui16TempChecksum = (uint32_t) ui16GenerateCRC16((uint8_t*) &xgRuntimeParameters, sizeof(xgRuntimeParameters), POLY_RUNTIME_PARAM, CRC_INITIAL);
	ui16TempChecksum = ui16GenerateCRC16((uint8_t*) &xgReceivedParameters.rui8ReceivedDnco, 256, POLY_RUNTIME_PARAM, ui16TempChecksum);

	xgIpkTx.rui8Checksum[0] = ui16TempChecksum >> 8;
	xgIpkTx.rui8Checksum[1] = (uint8_t) ui16TempChecksum;

}

/**-------------------------------------------------------------------------------------------------
 *    Description: Parameter Checksum /SR0003
 *    Frequency:  once
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vParameterCheckDesignerChecksum(void) {

	// Calculate chained crc
	uint16_t ui16TempChecksum = ui16GenerateCRC16((uint8_t*) &xgReceivedParameters, 54, POLY_RECEIVED_PARAM, CRC_INITIAL);
	//uint16_t ui16TempChecksum2 = ui16GenerateCRC16((uint8_t*) &xgReceivedParameters.rui8ReceivedDnco, 256, POLY_RECEIVED_PARAM, ui16TempChecksum);

	// Read received checksum
	uint16_t ui16RealChecksum = ((uint16_t) xgReceivedParameters.rui8Buffer[54]) << 8 | xgReceivedParameters.rui8Buffer[55];

	// FIT
	// ++ui16RealChecksum;

	if (ui16TempChecksum != ui16RealChecksum) {
		xgIpkTx.xErrors0.ui1ErrorParameterChecksum = TRUE;
	}

	//(DEBUGCODE(xgIpkTx.xErrors0.ui1ErrorParameterChecksum = FALSE;);
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Parameter Copies /SR0004
 *    Frequency:  100ms
 *    Parameters: pui8CheckinCounter	-	Scheduler Counter
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vParameterCheckCopies(uint8_t *pui8CheckinCounter) {
	++(*pui8CheckinCounter);
	if (!memcmp(xgReceivedParameters.rui8ReceivedDnco, xgReceivedParameters.rui8ReceivedDncoCopied, 256)
			&& !memcmp((void*)&xgRuntimeParameters, (void*)&xgRuntimeParametersCopied, sizeof(xgRuntimeParameters))) {

		if (xgErrorCounters.ui8ErrorCounterParamCopy > 0) {
			--xgErrorCounters.ui8ErrorCounterParamCopy;
		}

	} else {
		++xgErrorCounters.ui8ErrorCounterParamCopy;
	}

	if (xgErrorCounters.ui8ErrorCounterParamCopy >= ERROR_PARAMETER_COPY_100MS) {
		xgIpkTx.xErrors1.ui1ErrorParameterCopy = TRUE;
	}
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Recalculate Runtime Parameters in case dnco-index changed
 *    Frequency:  once
 *    Parameters: ui8ForceRecalculation - Force recalculation of the parameters, used for initialization
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vParameterCalculateDncoFrequencies(uint8_t ui8ForceRecalculation) {
	static uint16_t ui16DncoIndex0 = 0;		// DNCO index has 256 values

	if (xgRuntimeParameters.ui1DncoModeActivated && ((ui16DncoIndex0 != xgIpkTx.ui8DncoIndex) | ui8ForceRecalculation)) {
		ui16DncoIndex0 = xgIpkTx.ui8DncoIndex;

		if (xgRuntimeParameters.ui1DncoModeF3) {
			xgRuntimeParameters.rui16FrequencyLimits[3][0] = ((uint16_t) xgReceivedParameters.rui8ReceivedDnco[ui16DncoIndex0]) << 8
					| ((uint16_t) xgReceivedParameters.rui8ReceivedDnco[ui16DncoIndex0 + 1]);	// Frequenz 3
			uint32_t ui32BaseFrequency = xgRuntimeParameters.rui16FrequencyLimits[3][0];
			xgRuntimeParameters.rui16FrequencyHysteresis[3][0] = mCalculateMinus10Percent(ui32BaseFrequency);

			// Calculate Timeout
			if (ui32BaseFrequency < FREQ_13HZ) {
				xgRuntimeParameters.rui16FrequencyTimeouts[3][0] = (uint16_t) ((1300 / ui32BaseFrequency));
			} else {
				xgRuntimeParameters.rui16FrequencyTimeouts[3][0] = 10;					// 100 ms
			}
		} else {

			// Frequencies
			xgRuntimeParameters.rui16FrequencyLimits[0][0] = ((uint16_t) xgReceivedParameters.rui8ReceivedDnco[ui16DncoIndex0 + 96]) << 8
					| ((uint16_t) xgReceivedParameters.rui8ReceivedDnco[ui16DncoIndex0 + 97]);
			xgRuntimeParameters.rui16FrequencyLimits[1][0] = ((uint16_t) xgReceivedParameters.rui8ReceivedDnco[ui16DncoIndex0 + 64]) << 8
					| ((uint16_t) xgReceivedParameters.rui8ReceivedDnco[ui16DncoIndex0 + 65]);
			xgRuntimeParameters.rui16FrequencyLimits[2][0] = ((uint16_t) xgReceivedParameters.rui8ReceivedDnco[ui16DncoIndex0 + 32]) << 8
					| ((uint16_t) xgReceivedParameters.rui8ReceivedDnco[ui16DncoIndex0 + 33]);
			xgRuntimeParameters.rui16FrequencyLimits[3][0] = ((uint16_t) xgReceivedParameters.rui8ReceivedDnco[ui16DncoIndex0]) << 8
					| ((uint16_t) xgReceivedParameters.rui8ReceivedDnco[ui16DncoIndex0 + 1]);

			// Calculate everything for 0 %
			for (uint8_t i = 0; i < SIZEOF(xgRuntimeParameters.rui16FrequencyTimeouts); i++) {
				uint32_t ui32BaseFrequency = xgRuntimeParameters.rui16FrequencyLimits[i][0];

				xgRuntimeParameters.rui16FrequencyHysteresis[i][0] = mCalculateMinus10Percent(ui32BaseFrequency);

				// Calculate Timeout
				if (ui32BaseFrequency < FREQ_13HZ) {
					xgRuntimeParameters.rui16FrequencyTimeouts[i][0] = (uint16_t) ((1300 / ui32BaseFrequency));
				} else {
					xgRuntimeParameters.rui16FrequencyTimeouts[i][0] = 10;					// 100 ms
				}
			}
		}
	}
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Calculate emk runtime parameters
 *    Frequency:  once
 *    Parameters: ui8EmkMode - Received emk-mode
 *    Return: None
 *    Changes: 	2022-04-19	Changed  the Hiysteresis levels for On and Off Levels
 *-------------------------------------------------------------------------------------------------*/
static inline void vParametersCalculateEmkMode(uint8_t ui8EmkMode) {
	uint32_t ui32Temp;
	if (ui8EmkMode) {

		// Empfindlichkeit
		ui32Temp = (((uint16_t) xgReceivedParameters.rui8Buffer[44]) << 8) | xgReceivedParameters.rui8Buffer[45];

		// Falsche Einstellungen
		mValidateEmkSensitivity(xgIpkTx.xErrors1.ui1ErrorParameter, ui32Temp);
		xgRuntimeParameters.ui16EmkSensitivityVoltage = ui32Temp;

		// Einschaltverzögerung
		ui32Temp = (((uint16_t) xgReceivedParameters.rui8Buffer[46]) << 8) | xgReceivedParameters.rui8Buffer[47];
		mValidateEmkDelay(xgIpkTx.xErrors1.ui1ErrorParameter, ui32Temp);
		xgRuntimeParameters.ui16EmkActivationDelay = ui32Temp;

		// Calculate On and Off Hysteresis
		if (xgRuntimeParameters.ui16EmkSensitivityVoltage > EMK_GAIN_SWITCH_VOLTAGE) {
			ui32Temp = xgRuntimeParameters.ui16EmkSensitivityVoltage * EMK_GAIN_LOW * EMK_3V3_DIGITAL_REF / EMK_3V3_ANALOG_REF;
		} else {
			ui32Temp = xgRuntimeParameters.ui16EmkSensitivityVoltage * EMK_GAIN_HIGH * EMK_3V3_DIGITAL_REF / EMK_3V3_ANALOG_REF;
		}

		xgRuntimeParameters.ui16EmkHysteresisOn = (uint16_t) mCalculateMinus5Percent(ui32Temp);
		xgRuntimeParameters.ui16EmkHysteresisOff = (uint16_t) mCalculatePlus70Percent(xgRuntimeParameters.ui16EmkHysteresisOn);

		// Limit Off-Hysteresis to approx. 3.22V
		mLimitEmkHysteresisOff(xgRuntimeParameters.ui16EmkHysteresisOff);
	}
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Calculate speed runtime parameters
 *    Frequency:  once
 *    Parameters: ui8SpeedMode - Received speed-mode
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
static inline void vParametersCalculateSpeedMode(uint8_t ui8SpeedMode) {
	uint32_t ui32Frequency;
	uint32_t ui32Temp;

	if (ui8SpeedMode != 0) {
		// Mehrfachmessung
		ui32Temp = (((uint16_t) xgReceivedParameters.rui8Buffer[40]) << 8) | xgReceivedParameters.rui8Buffer[41];
		xgRuntimeParameters.ui16SpeedRepeatedCaptureWedge = (ui32Temp);

		// Anlaufzeit
		ui32Temp = (((uint16_t) xgReceivedParameters.rui8Buffer[42]) << 8) | xgReceivedParameters.rui8Buffer[43];
		xgRuntimeParameters.ui16SpeedMotorRunupTime = ui32Temp;

		// Abschaltverzögerung
		ui32Temp = xgReceivedParameters.rui8Buffer[48];
		mValidateSwitchoffDelay(xgIpkTx.xErrors1.ui1ErrorParameter, ui32Temp, ui8SpeedMode);
		xgRuntimeParameters.ui8SpeedSwitchoffDelay = ui32Temp;

		// Kombinatorisches Byte
		ui32Temp = xgReceivedParameters.rui8Buffer[49];
		xgRuntimeParameters.ui8SpeedRepeatedCaptureCount = (ui32Temp & 0x03) + 1;	// Es wurde vereinbart mit +1 auszuwerten
		xgRuntimeParameters.ui8SpeedHysteresisMode = (ui32Temp >> 2) & 0x03;
		xgRuntimeParameters.ui8SpeedPhaseShiftMonitoring = ((ui32Temp) >> 7);

		// Abschaltverzögerung++
		ui32Temp = xgReceivedParameters.rui8Buffer[51];
		mValidateSwitchoffDelayPP(xgIpkTx.xErrors1.ui1ErrorParameter, ui32Temp);
		xgRuntimeParameters.ui8SpeedSwitchoffPercent = (uint8_t)ui32Temp;

		ui32Temp = xgReceivedParameters.rui8Buffer[52]*10;
		mValidateFrequencyComparisonWedge(xgIpkTx.xErrors1.ui1ErrorParameter, ui32Temp);
		xgRuntimeParameters.ui16SpeedCompareWedge = (uint16_t) ui32Temp;
	}

	switch (ui8SpeedMode) {
	case 1:
	case 2:
		//----------------------------------------------------------------
		// Calculate runtime parameters for speed mode 1
		//----------------------------------------------------------------
		if (xgRuntimeParameters.ui1DncoModeActivated) {
			vParameterCalculateDncoFrequencies(1);
		} else {

			// Frequencies
			xgRuntimeParameters.rui16FrequencyLimits[0][0] = (((uint16_t) xgReceivedParameters.rui8Buffer[32]) << 8) | ((uint16_t) xgReceivedParameters.rui8Buffer[32 + 1]);
			xgRuntimeParameters.rui16FrequencyLimits[1][0] = (((uint16_t) xgReceivedParameters.rui8Buffer[32 + 2]) << 8) | ((uint16_t) xgReceivedParameters.rui8Buffer[32 + 3]);
			xgRuntimeParameters.rui16FrequencyLimits[2][0] = ((uint16_t) xgReceivedParameters.rui8Buffer[32 + 4]) << 8 | ((uint16_t) xgReceivedParameters.rui8Buffer[32 + 5]);
			xgRuntimeParameters.rui16FrequencyLimits[3][0] = (((uint16_t) xgReceivedParameters.rui8Buffer[32 + 6]) << 8) | ((uint16_t) xgReceivedParameters.rui8Buffer[32 + 7]);	// Frequenz 3

			// Exception from the validation rule! Precalculate runtime values for standstill frequency
			ui32Frequency = xgRuntimeParameters.rui16FrequencyLimits[0][0];
			xgRuntimeParameters.rui16FrequencyHysteresis[0][0] = mCalculateMinus10Percent(ui32Frequency);
			xgRuntimeParameters.rui16FrequencyTimeouts[0][0] = ui32Frequency < FREQ_13HZ ? (uint16_t) ((1300 / ui32Frequency)) : 10;

			// Calculate everything: frequencies for validation levels 0% 10% 15% 20%; 10% hysteresis with timeouts for every frequency.
			for (uint8_t i = 1; i < SIZEOF(xgRuntimeParameters.rui16FrequencyTimeouts); i++) {
				for (uint8_t j = 0; j < SIZEOF(xgRuntimeParameters.rui16FrequencyTimeouts[0]); j++) {
					ui32Frequency = xgRuntimeParameters.rui16FrequencyLimits[i][0];

					switch (j) {
					case 0:
						ui32Frequency = ui32Frequency; // @suppress("Assignment to itself")
						break;
					case 1:
						ui32Frequency = mCalculateMinus10Percent(ui32Frequency);
						break;
					case 2:
						ui32Frequency = mCalculateMinus15Percent(ui32Frequency);
						break;
					case 3:
						ui32Frequency = mCalculateMinus20Percent(ui32Frequency);
						break;
					default:
						break;
					}

					// Calculate Hysteresis and Timeout
					xgRuntimeParameters.rui16FrequencyLimits[i][j] = ui32Frequency;
					xgRuntimeParameters.rui16FrequencyHysteresis[i][j] = mCalculateMinus10Percent(ui32Frequency);
					xgRuntimeParameters.rui16FrequencyTimeouts[i][j] = ui32Frequency < FREQ_13HZ ? (uint16_t) ((1300 / ui32Frequency)) : 10;

				}
			}
		}

		xgRuntimeParameters.ui8SpeedMaxValidationLevel = VALIDATION_LEVEL_30P;
		break;
	case 3:
	case 4:
	case 5:
	case 6:
		//----------------------------------------------------------------
		// Calculate runtime parameters for speed mode 2
		//----------------------------------------------------------------
		xgRuntimeParameters.rui16FrequencyLimits[0][0] = (((uint16_t) xgReceivedParameters.rui8Buffer[32]) << 8) | ((uint16_t) xgReceivedParameters.rui8Buffer[32 + 1]);		// Stillstandfrequenz
		xgRuntimeParameters.rui16FrequencyLimits[1][0] = (((uint16_t) xgReceivedParameters.rui8Buffer[32 + 2]) << 8) | ((uint16_t) xgReceivedParameters.rui8Buffer[32 + 3]);	// Frequenz 1
		xgRuntimeParameters.rui16FrequencyLimits[2][0] = (((uint16_t) xgReceivedParameters.rui8Buffer[32 + 4]) << 8) | ((uint16_t) xgReceivedParameters.rui8Buffer[32 + 5]);	// Frequenz 2
		//xgRuntimeParameters.aui16FrequencyLimits[3][0] = (((uint16_t) xgReceivedParameters.rui8Buffer[32 + 6]) << 8) | ((uint16_t) xgReceivedParameters.rui8Buffer[32 + 7]);	// Frequenz 3

		// Exception from the validation rule! Precalculate runtime values for standstill frequency
		ui32Frequency = xgRuntimeParameters.rui16FrequencyLimits[0][0];
		xgRuntimeParameters.rui16FrequencyHysteresis[0][0] = mCalculateMinus10Percent(ui32Frequency);
		xgRuntimeParameters.rui16FrequencyTimeouts[0][0] = ui32Frequency < FREQ_13HZ ? (uint16_t) ((1300 / ui32Frequency)) : 10;

		// Calculate everything: frequencies for validation levels 0% 10% 15% 20%; 10% hysteresis with timeouts for every frequency. Last frequency is not used
		for (uint8_t i = 1; i < SIZEOF(xgRuntimeParameters.rui16FrequencyTimeouts) - 1; i++) {
			for (uint8_t j = 0; j < SIZEOF(xgRuntimeParameters.rui16FrequencyTimeouts[0]); j++) {
				uint32_t ui32Frequency = xgRuntimeParameters.rui16FrequencyLimits[i][0];

				// Hysteresis is increased upwards if frequency is "frequency min"
				switch (j) {
				case 0:
					ui32Frequency = ui32Frequency;  // @suppress("Assignment to itself")
					break;
				case 1:
					ui32Frequency = (i == 1) ? mCalculatePlus10Percent(ui32Frequency) : mCalculateMinus10Percent(ui32Frequency);
					break;
				case 2:
					ui32Frequency = (i == 1) ? mCalculatePlus15Percent(ui32Frequency) : mCalculateMinus15Percent(ui32Frequency);
					break;
				case 3:
					ui32Frequency = (i == 1) ? mCalculatePlus20Percent(ui32Frequency) : mCalculateMinus20Percent(ui32Frequency);
					break;
				default:
					break;
				}

				// Calculate hysteresis depending on mode
				xgRuntimeParameters.rui16FrequencyLimits[i][j] = ui32Frequency;

				// if limit1 then rise up
				xgRuntimeParameters.rui16FrequencyHysteresis[i][j] = (i == 1) ? mCalculatePlus10Percent(ui32Frequency) : mCalculateMinus10Percent(ui32Frequency);

				// Calculate Timeout
				if (ui32Frequency < FREQ_13HZ) {
					xgRuntimeParameters.rui16FrequencyTimeouts[i][j] = (uint16_t) ((1300 / ui32Frequency));
				} else {
					if (i == 1) {
						xgRuntimeParameters.rui16FrequencyTimeouts[i][j] = 20;	// Window mode 200 ms for lower limit "frequency min"
					} else {
						xgRuntimeParameters.rui16FrequencyTimeouts[i][j] = 10;
					}
				}

			}
		}

		// Abstand für Fenster validieren. Wenn größer 1200 und wenn der Abstand zwischen Hysteresenen kleiner als 10 % ist
		if ((xgRuntimeParameters.rui16FrequencyLimits[0][0] > 12000) || (xgRuntimeParameters.rui16FrequencyLimits[1][0] > 12000) || (xgRuntimeParameters.rui16FrequencyLimits[2][0] > 12000)
				|| (((((uint16_t) xgRuntimeParameters.rui16FrequencyHysteresis[1][0]) * 110) / 100) >= xgRuntimeParameters.rui16FrequencyHysteresis[2][0])) {
			xgIpkTx.xErrors1.ui1ErrorParameter = TRUE;
		}

		// Maximale Validierungsstufe ermitteln
		if (xgRuntimeParameters.rui16FrequencyHysteresis[2][3] > xgRuntimeParameters.rui16FrequencyHysteresis[1][3])
			xgRuntimeParameters.ui8SpeedMaxValidationLevel = VALIDATION_LEVEL_30P;
		else if (xgRuntimeParameters.rui16FrequencyHysteresis[2][2] > xgRuntimeParameters.rui16FrequencyHysteresis[1][2])
			xgRuntimeParameters.ui8SpeedMaxValidationLevel = VALIDATION_LEVEL_20P;
		else if (xgRuntimeParameters.rui16FrequencyHysteresis[2][1] > xgRuntimeParameters.rui16FrequencyHysteresis[1][2])
			xgRuntimeParameters.ui8SpeedMaxValidationLevel = VALIDATION_LEVEL_10P;
		else
			xgRuntimeParameters.ui8SpeedMaxValidationLevel = VALIDATION_LEVEL_NONE;
		break;
	case 7:
		//----------------------------------------------------------------
		// Calculate runtime parameters for speed mode 3
		//----------------------------------------------------------------
		xgRuntimeParameters.rui16FrequencyLimits[0][0] = (((uint16_t) xgReceivedParameters.rui8Buffer[32]) << 8) | ((uint16_t) xgReceivedParameters.rui8Buffer[32 + 1]);		// Stillstandfrequenz
		xgRuntimeParameters.rui16FrequencyLimits[1][0] = (((uint16_t) xgReceivedParameters.rui8Buffer[32 + 2]) << 8) | ((uint16_t) xgReceivedParameters.rui8Buffer[32 + 3]);		// Frequenz 1
		xgRuntimeParameters.rui16FrequencyLimits[2][0] = (((uint16_t) xgReceivedParameters.rui8Buffer[32 + 4]) << 8) | ((uint16_t) xgReceivedParameters.rui8Buffer[32 + 5]);		// Frequenz 2
		xgRuntimeParameters.rui16FrequencyLimits[3][0] = (((uint16_t) xgReceivedParameters.rui8Buffer[32 + 6]) << 8) | ((uint16_t) xgReceivedParameters.rui8Buffer[32 + 7]);		// Frequenz 3

		// Calculate everything: frequencies for validation levels 0%; 10% hysteresis with timeouts for every frequency.
		for (uint8_t i = 0; i < SIZEOF(xgRuntimeParameters.rui16FrequencyTimeouts); i++) {
			uint32_t ui32BaseFrequency = xgRuntimeParameters.rui16FrequencyLimits[i][0];

			xgRuntimeParameters.rui16FrequencyHysteresis[i][0] = mCalculateMinus10Percent(ui32BaseFrequency);

			// Calculate Timeout
			if (ui32BaseFrequency < FREQ_13HZ) {
				xgRuntimeParameters.rui16FrequencyTimeouts[i][0] = (uint16_t) ((1300 / ui32BaseFrequency));
			} else {
				xgRuntimeParameters.rui16FrequencyTimeouts[i][0] = 10;					// 100 ms
			}
		}

		xgRuntimeParameters.ui8SpeedMaxValidationLevel = VALIDATION_LEVEL_NONE;
		break;

	}
}


