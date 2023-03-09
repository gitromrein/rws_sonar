/*
 *  pdsv-diagnostics.h
 *
 *  Created on: 26.10.2021
 *      Author: am
 */

#ifndef SAFE_DIAGNOSTICS_H_
#define SAFE_DIAGNOSTICS_H_

/* Public defines*/
// Eds
#define EDS_SIZE 16

// Hardware Errors
#define ERROR_HARDWARE_RAMTEST_INIT			0x0C
#define ERROR_HARDWARE_RAMTEST_RUN			0x0D
#define ERROR_HARDWARE_ROMTEST_INIT			0x0E
#define ERROR_HARDWARE_ROMTEST_RUN			0x0F


#define ERROR_HARDWARE_INTERRUPT			0xFF

#define ERROR_HARDWARE_MAIN					0x02
#define ERROR_HARDWARE_TIMER				0x03
#define ERROR_HARDWARE_WATCHDOG				0x04
#define ERROR_HARDWARE_CAPTURE				0x05
#define ERROR_HARDWARE_BROWNOUT				0x06
#define ERROR_HARDWARE_PGM_1MS				0x07
#define ERROR_HARDWARE_PGM_4MS				0x08
#define ERROR_HARDWARE_PGM_10MS				0x09
#define ERROR_HARDWARE_PGM_100MS			0x0A
#define ERROR_HARDWARE_UNEXPECTED_RESET		0x0B

/* Public Datatypes */
/*-------------------------------
 * 	Runtime state for the logic
 -------------------------------*/
#define FREQUENCY_LEVELS 	4
typedef struct {

	uint16_t ui16Inputs;

	// Captured
	struct {
		uint32_t ui32InterruptCapturedFrequency;
		uint8_t ui1InterruptNewCapturePending :1;
		uint8_t ui1InterruptDirectionCapture :1;
	} xCaptured;

	struct {
		uint8_t ui1ValidFrequency :1;
		uint8_t ui1WedgeCrossed :1;
		uint8_t ui1NewEvaluation :1;
	} xProcess;

	// Evaluated Speed&Standstill
	struct {
		uint32_t ui32Timeout100mHz;
		uint32_t ui32Timeout500mHz;

		uint32_t ui32MasterFrequency;
		uint32_t ui32SlaveFrequency;

		uint32_t ui32HysteresisTimeout;		// Index timeout and hysteresis timeout are the same

// Frequenzen
		uint32_t rui32FrequencyTimeouts[FREQUENCY_LEVELS];

// Mehrfachmessung
		uint8_t rui8MultiCaptureCounters[FREQUENCY_LEVELS];

// Abschaltverzögerung
		uint32_t rui32SwitchoffDelays[FREQUENCY_LEVELS];

	} xSpeed;

	// Direction
	struct {
		uint32_t ui32TotalNumberOfPhaseSamples;
		uint32_t ui32TotalNumberOfMasterLowSamples;
		uint32_t ui32SignalSimilarity;
		uint32_t ui32PercentageSimilarity;
		uint16_t ui16DirectionRight;
		uint8_t ui8SimilaryCheckRunning;
		uint8_t ui1Direction;
	} xDirection;

	struct {
		uint32_t ui32AccelerationDummy;
	} xAcceleration;

	// Voltage
	struct {
		uint32_t ui32VoltageAverageUW;
		uint32_t ui32VoltageMaximumUW;
		uint32_t ui32VoltageMinimumUW;

		uint32_t ui32VoltageEmkAverageUW;
		uint32_t ui32VoltageEmkMaximumUW;
		uint32_t ui32VoltageEmkMinimumUW;

		uint32_t ui32Voltage12;
		uint32_t ui32Voltage3v3;
		uint32_t ui32Voltage1v65;

	} xVoltage;

	struct {
		uint32_t ui32EmkMaxAverage;
		uint32_t ui32EmkMinAverage;
		uint32_t ui32EmkDelta;
		uint32_t ui32EmkDelay;
		uint8_t ui8EmkShiftingStatus;
		uint8_t ui8EmkSampleCount;
		uint8_t ui8EmkPeakCount;
	} xEmk;

	uint8_t ui8ParameterComplete;

} RuntimeState_t;

/*-------------------------------
 * 	Hardcore Errors
 -------------------------------*/
typedef union {
	struct {
		uint8_t ui8ErrorRomTest;
		uint8_t ui8ErrorRamTest;
		uint8_t ui8ErrorMainTest;
		uint8_t ui8ErrorTimerTest;
		uint8_t ui8ErrorAdcTest;
		uint8_t ui8ErrorPgmTest;
		uint8_t ui8ErrorUnexpectedInterrupt;
		uint8_t ui8ErrorBrownout;
		uint8_t ui8ErrorWatchdog;
		uint8_t ui8ErrorSubroutineCalls;
		uint8_t ui8ErrorCaptureDefect;

	};
} HardwareErrors_t;

/*-------------------------------
 * 	Error Counters
 -------------------------------*/
typedef struct {
	uint16_t ui16ErrorCounterMain;
	uint16_t ui16ErrorCounterTimer;

	// Voltage
	uint8_t ui8ErrorCounterVoltage3v3;
	uint8_t ui8ErrorCounterVoltage1v65;
	uint8_t ui8ErrorCounterVoltage12v;

	// Others
	uint8_t ui8ErrorCounterEmkSingle;

	uint8_t ui8ErrorCounterIpkTimeout;

	uint8_t ui8ErrorCounterPort;
	uint16_t ui16ErrorCounterInput;

	uint8_t ui8ErrorCounterCanTimeout;

	uint8_t ui8ErrorCounterWireBreakage;
	uint16_t ui16ErrorCounterWireBreakageSignal;

	uint8_t ui8ErrorRotationDirectionSwitchCounter;

	uint8_t ui8ErrorCounterAdcFrequency;
	uint8_t ui8ErrorCounterAdcTimeout;
	uint8_t ui8ErrorCounterDynamicInputTest;

	uint8_t ui8ErrorCounterParamCopy;
	uint8_t ui8ErrorCounterParamExchange;

	// Speed
	uint8_t ui8ErrorCounterFrequencyOver1500Hz;
	uint32_t ui32ErrorCounterFrequencyCompare;
	uint8_t ui8ErrorCounterVersion;
	uint32_t ui32ErrorCounterQuitt;
	uint8_t ui8ErrorCounterCaptureDefect;

	// Direction
	uint8_t ui8ErrorCounterDirectionSwitch;
	uint16_t ui16ErrorCounterPhaseShift;
} ErrorCounters_t;

/*-------------------------------
 * 	Debug datatype
 -------------------------------*/
#ifdef DEBUG
#define DIAG_ARRAY_SIZE 100
typedef struct {
	uint32_t ui32WCET;
	uint32_t ui32WCETLatest;
	uint32_t ui32TimerSum;
	uint32_t ui32TimerAverage;
	uint32_t ui32TimerP;
	uint32_t ui32TimerC;
	uint32_t ui32TimerDifference;
	uint32_t ui32TimerCount;

} Time_t;

typedef struct {
	Time_t xTime1;
	Time_t xTime2;

	uint32_t rui32Array0[DIAG_ARRAY_SIZE];
	uint32_t rui32Array1[DIAG_ARRAY_SIZE];
	uint32_t ui32Index0;
	uint32_t ui32Index1;


	uint32_t ui32DebugCanRx0;
	uint32_t ui32DebugCanRx1;

	uint8_t ui8Toggler;
	uint8_t ui8InputXor;
	uint32_t ui32AdcExecutions;
} DebugStatistics_t;

/* Public Variables */
extern DebugStatistics_t xgDebugStatistics;	// Debug

#define mDebugPutIntoArray0(value) DEBUGCODE({xgDebugStatistics.rui32Array0[xgDebugStatistics.ui32Index0] = value;\
											xgDebugStatistics.ui32Index0 = (xgDebugStatistics.ui32Index0+1)%DIAG_ARRAY_SIZE;});

#define mDebugPutIntoArray1(value) DEBUGCODE({xgDebugStatistics.rui32Array1[xgDebugStatistics.ui32Index1] = value;\
											xgDebugStatistics.ui32Index1 = (xgDebugStatistics.ui32Index1+1)%DIAG_ARRAY_SIZE;});

#else
#define mDebugPutIntoArray0(value)
#define mDebugPutIntoArray1(value)
#endif




	extern const uint8_t rui8gEds[EDS_SIZE];
	extern RuntimeState_t xgRuntimeState;
	extern ErrorCounters_t xgErrorCounters;
	extern HardwareErrors_t xgHardwareErrors;

	/* Public prototypes */
	extern void vDiagnosticsResetErrors(void);
	extern void vDiagnosticsHandleErrors(uint8_t *pui8CheckinCounter);
	extern void vDiagnosticsCheckFirmwareVersion(uint8_t *pui8CheckinCounter);
	extern void vDiagnosticsExport(void);

	extern void vDebugTimer1WCET(void);
	extern void vDebugTimer2Start(void);
	extern void vDebugTimer2Stop(void);


#endif /* SAFE_DIAGNOSTICS_H_ */
