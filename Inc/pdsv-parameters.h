/*
 *  pdsv-parameters.h
 *
 *  Created on: 19.07.2021
 *      Author: am
 */

#ifndef PDSV_PARAMETERS_H_
#define PDSV_PARAMETERS_H_

// Timeouts
#define DELAY_1MS_10MS 10U
#define DELAY_1MS_0S1 100U
#define DELAY_1MS_1S 1000U
#define DELAY_1MS_3S 3000U
#define DELAY_1MS_11S 11000U

/* Public defines */
#define mCalculatePlus10Percent(frequency) frequency*110/100
#define mCalculatePlus15Percent(frequency) frequency*115/100
#define mCalculatePlus20Percent(frequency) frequency*120/100
#define mCalculatePlus30Percent(frequency) frequency*130/100
#define mCalculatePlus50Percent(value) value*3/2
#define mCalculatePlus70Percent(value) (value* 17/10)

#define mCalculateMinus5Percent(value) (value*955/1000)
#define mCalculateMinus10Percent(frequency) frequency*90/100
#define mCalculateMinus15Percent(frequency) frequency*85/100
#define mCalculateMinus20Percent(frequency) frequency*80/100
#define mCalculateMinus30Percent(frequency) frequency*70/100

// Parameter definitions
#define HYSTERESIS_MODE_00	0
#define HYSTERESIS_MODE_01	1
#define HYSTERESIS_MODE_02	2
#define HYSTERESIS_MODE_03	3

#define VALIDATION_LEVEL_30P 3		// 0 -3
#define VALIDATION_LEVEL_20P 2
#define VALIDATION_LEVEL_10P 1
#define VALIDATION_LEVEL_NONE 0

// EMK
#define EMK_GAIN_SWITCH_VOLTAGE 86
#define EMK_GAIN_LOW 	3
#define EMK_GAIN_HIGH 	23
#define EMK_3V3_ANALOG_REF 	 (3300)
#define EMK_3V3_DIGITAL_REF  (1023)
#define EMK_3V22_ANALOG		 (1000)

// Parameter Validation Macros
#define mValidateEmkSensitivity(flag, value) if(value<10 || value>700){ flag= TRUE; }
#define mValidateEmkDelay(flag, value) if(value>3000){ flag = TRUE; }
#define mValidateCanRefreshTime(flag, value) 	if (ui32Temp < 5 || ui32Temp > 50) { flag = TRUE; value = CAN_CYCLE_10MS;}
#define mValidateSwitchoffDelay(flag, value, mode) if(mode == 7 && value!= 0){ flag = TRUE;}
#define mValidateSwitchoffDelayPP(flag, value) if(value>100) {flag = TRUE;}
#define mValidateFrequencyComparisonWedge(flag, value) if(value<FREQ_2HZ){ flag = TRUE;}

#define mLimitEmkHysteresisOff(value) if(value>EMK_3V22_ANALOG){ value = EMK_3V22_ANALOG; }

#define mGetRecalculatedRunupTime (xgRuntimeParameters.ui16SpeedMotorRunupTime*10)
#define mGetRecalculatedSwitchoffDelay (xgRuntimeParameters.ui8SpeedSwitchoffDelay * DELAY_1MS_10MS)
#define mGetRecalculatedEmkDelay (xgRuntimeParameters.ui16EmkActivationDelay * DELAY_1MS_10MS)

/* Public datatypes */
typedef struct {
	uint8_t rui8Buffer[64];
	uint8_t rui8ReceivedDnco[256];
	uint8_t rui8ReceivedDncoCopied[256];
} ReceivedParameters_t;

// WARNING! Diese Struktur muss bei beiden Controllern gleich bleiben
// Fürst Stillstandsfrequenz werden die Stellen 1 - 3 nicht benutzt, da es keine Validierung stattfindet
typedef struct {
	union {
		struct {
			uint16_t rui16FrequencyLimits[4][4];
			uint16_t rui16FrequencyHysteresis[4][4];
			uint16_t rui16FrequencyTimeouts[4][4];
		};

		uint8_t rui8UnionBuffer[2 * 4 * 4 * 3];
	};
	uint16_t ui16SpeedRepeatedCaptureWedge;
	uint16_t ui16SpeedMotorRunupTime;
	uint16_t ui16EmkActivationDelay;
	uint16_t ui16EmkSensitivityVoltage;

	uint16_t ui16EmkHysteresisOn;
	uint16_t ui16EmkHysteresisOff;

	uint8_t ui8SpeedSwitchoffDelay;
	uint8_t ui8SpeedMaxValidationLevel;
	uint8_t ui8SpeedHysteresisMode;
	uint8_t ui8SpeedRepeatedCaptureCount;
	uint8_t ui8GeneralInputDebounce;

	union {
		struct {
			uint8_t ui1SafetyCircuitCrosscircuitsMode1 :1;
			uint8_t ui1SafetyCircuitCrosscircuitsMode2 :1;
			uint8_t ui6SafetyCircuitCrosscircuitReserved :6;

		};
		uint8_t ui8SafetyCircuitsCrosscircuitMode;
	};

	union {
		struct {
			uint8_t ui1SafetyCircuitsAcknowledgementMode1 :1;
			uint8_t ui1SafetyCircuitsAcknowledgementMode2 :1;
			uint8_t ui6SafetyCircuitsAcknowledgementModeReserved :6;
		};
		uint8_t ui8SafetyCircuitsAcknowledgementMode;
	};

	union {
		struct {
			uint8_t ui1SafetyCircuitsAcknowledgementDelay1 :1;
			uint8_t ui1SafetyCircuitsAcknowledgementDelay2 :1;
			uint8_t ui6SafetyCircuitsAcknowledgementDelayReserved :6;
		};
		uint8_t ui8SafetyCircuitsAcknowledgementDelay;
	};

	union {
		struct {
			uint8_t ui1SafetyCircuitAntivalent1 :1;
			uint8_t ui1SafetyCircuitAntivalent2 :1;
			uint8_t ui6SafetyCircuitAntivalentReserved :6;
		};

		uint8_t ui8SafetyCircuitsAntivalent;
	};

	union {
		struct {
			uint8_t ui1SafetyCircuitInitialStateCheck1 :1;
			uint8_t ui1SafetyCircuitInitialStateCheck2 :1;
			uint8_t ui6SafetyCircuitInitialStateCheckReserved :6;
		};
		uint8_t ui8SafetyCircuitsInitialStateCheck;
	};

	union {
		struct {
			uint8_t ui1SafetyCircuitsAcknowledgementLevel1 :1;
			uint8_t ui1SafetyCircuitsAcknowledgementLevel2 :1;
			uint8_t ui6SafetyCircuitsAcknowledgementLevelReserved :6;
		};
		uint8_t ui8SafetyCircuitsAcknowledgementLevel;
	};

	union {
		struct {
			uint8_t ui1SafetyCircuitsActivation1 :1;
			uint8_t ui1SafetyCircuitsActivation2 :1;
			uint8_t ui1SafetyTwohandActivation1 :1;
			uint8_t ui6SafetyCircuitsActivationReserved :5;
		};
		uint8_t ui8SafetyCircuitsActivation;
	};

	uint8_t ui8GeneralCanRefreshTime;

	union {
		struct {
			uint8_t ui2GeneralSpeedSettingsReserved02 :2;
			uint8_t ui1GeneralSpeedSettingsStandstillSwitchoffDelay :1;
			uint8_t ui1GeneralSpeedSettingsSafeStop :1;
			uint8_t ui1GeneralSpeedSettingsMuteWithoutSensors :1;
			uint8_t ui1GeneralSpeedSettingsDirection :1;
			uint8_t ui1GeneralSpeedSettingsDirectionStop :1;
			uint8_t ui1GeneralSpeedSettingsNDirectionActivation :1;
		};
		uint8_t ui8GeneralSpeedSettings;
	};

	uint8_t ui8SpeedPhaseShiftMonitoring;

	uint8_t ui8SpeedMode;
	uint8_t ui8EmkMode;

	union {
		struct {
			uint8_t ui1DncoModeActivated :1;
			uint8_t ui3DncoModeReservedaaa :3;
			uint8_t ui1DncoModeF3 :1;
			uint8_t ui3DncoModereservedkopdksa :3;
		};
		uint8_t ui8DncoMode;
	};
	uint16_t ui16SpeedCompareWedge;
	uint8_t ui8SpeedSwitchoffPercent;
	uint8_t ui8Dummy0;
	uint8_t ui8Dummy1;
	uint8_t ui8Dummy2;

} RuntimeParameters_t;

/* Public Variables */
extern RuntimeParameters_t xgRuntimeParameters;
extern RuntimeParameters_t xgRuntimeParametersCopied;
extern ReceivedParameters_t xgReceivedParameters;

/* Public prototypes */
extern void vParameterCheckDesignerChecksum(void);
extern void vParameterCheckPlausibility(void);
extern void vParameterCheckExchangedChecksum(uint8_t *pui8CheckinCounter);
extern void vParameterCheckCopies(uint8_t *pui8CheckinCounter);

extern void vLoadDebugParameters(void);
extern void vParameterCheckComplete(uint8_t *pui8CheckinCounter);
extern void vParameterCalculateDncoFrequencies(uint8_t ui8ForceRecalculation);

#endif /* PDSV_PARAMETERS_H_ */
