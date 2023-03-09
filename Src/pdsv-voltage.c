/*
 *  pdsv-voltage.c
 *
 *  Created on: 25.08.2020
 *      Author: am
 */

#include "pdsv-voltage.h"
#include "pdsv-parameters.h"
#include "pdsv-hardware.h"
#include "safe/safe-speed.h"
#include "pdsv-diagnostics.h"
#include "pdsv-ipk.h"

/* Private defines*/
#define ERROR_WIREBREAKAGE_SINGAL_1MS	4000		// 4 seconds
#define ERROR_WIREBREAkAGE_COUNTER 		  10

#define MAX_VOLTAGE_3V3 563						// ((1024/2)*(110/100))		// Voltage test10 % drüber
#define MIN_VOLTAGE_3V3 460						// ((1024/2)*(90/100))		// Voltage test 10 % drunter

#define MAX_VOLTAGE_1V65 563					// 1024/2*1.1
#define MIN_VOLTAGE_1V65 460					// 1024/2*0.9


#define MAX_VOLTAGE_12V    824    				 //824 = 1023/3,3V*2,65V. Spannungsteiler: 14,7V/12,2K *2,2K = 2,65V
#define MIN_VOLTAGE_12V    559     				//559 = 1023/3,3V*1,803V. Spannungsteiler: 10V/12,2K *2,2K = 1,803V

#define MAX_VOLTAGE_ERROR 10
#define MAX_VOLTAGE_ADC_FREQUENCY_ERROR 30

#define VOLTAGE_1v3 		403			// 1024/3.3*1.3
#define VOLTAGE_1v4 		434			// 1024/3.3*1.4
#define VOLTAGE_1v5			465			// 1024/3.3*1.5
#define VOLTAGE_1v6 		496			// 1024/3.3*1.6
#define VOLTAGE_3v0			930			// 1024/3.3*3.0
#define VOLTAGE_3v3			1024		// 1024/3.3*3.3

#define MAX_ADC_TIMEOUT 	2			// Number of missed adc measurements that are recognized as an error
#define DMA_BUF_SIZE 		100

#define ANALOGPATH_VOLTAGE_FOR_HYST0_FORWARD (77U << 2)
#define ANALOGPATH_VOLTAGE_FOR_HYST1_FORWARD (51U << 2)
#define ANALOGPATH_VOLTAGE_FOR_HYST2_FORWARD (179U << 2)
#define ANALOGPATH_VOLTAGE_FOR_HYST3_FORWARD (128U << 2)

#define ANALOGPATH_VOLTAGE_FOR_HYST0_BACKWARD  (25U << 2)
#define ANALOGPATH_VOLTAGE_FOR_HYST1_BACKWARD  (10U << 2)
#define ANALOGPATH_VOLTAGE_FOR_HYST2_BACKWARD (102U << 2)
#define ANALOGPATH_VOLTAGE_FOR_HYST3_BACKWARD  (76U << 2)

/* Public variables */
AdcData_t __attribute__((section(".ramvars"))) rxgAdcData[MAX_ADC_DATA_BLOCKS] = {0};		// 2 swap buffers for software and the hardware
AdcData_t *pxgAdcSoftwareBuffer;				// is initialized in the timer interrupt, the timer isr swaps the buffers between dma adc and main execution
AdcData_t *pxgAdcHardwareBuffer;				//


/*-------------------------------------------------------------------
 *    Description: Initialiye rxgAdcBuffers
 *    Frequency:  once
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------*/
void vAdcInitializeDmaBuffers(void){
	memset(rxgAdcData, 0, sizeof(rxgAdcData));
	pxgAdcSoftwareBuffer = &rxgAdcData[0];
	pxgAdcHardwareBuffer = &rxgAdcData[1];
}


/*-------------------------------------------------------------------
 *    Description: Test if the Wire is still connected /SR0015 SR0020
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------*/
void vAdcCheckWirebreakage(uint8_t *pui8CheckinCounter) {
	static uint8_t ui8State = 0;
	static uint8_t ui8Delay = 0;
	static uint8_t ui8CaptureTime = 0;
	static uint32_t ui32FirstWireVoltageAverage = 0;
	static uint32_t ui32SecondWireVoltageAverage = 0;
	uint8_t ui8ConnectionStatus = FALSE;
	uint8_t ui8SignalHigh = FALSE;

	++(*pui8CheckinCounter);

	// Check Signal
	if (mHardwareGetSignal) {
		ui8SignalHigh = TRUE;
	} else {
		ui8SignalHigh = FALSE;
	}

	// Analyze the signal
	switch (ui8State) {
	// Standard Low
	case 0:
		if (FALSE == ui8SignalHigh) {
			++ui8State;
		}
		break;

		// Rising Edge
	case 1:
		if (TRUE == ui8SignalHigh) {
			++ui8State;
			ui8Delay = 200;		// Wait 200 ms
		}
		break;
	case 2:
		--ui8Delay;
		if (!ui8Delay) {				// First part of the signal starts now: High part
			ui8CaptureTime = 25;	// Add values together for 25 ms
			ui32FirstWireVoltageAverage = 0;
			++ui8State;
		}
		break;
	case 3:
		if (ui8CaptureTime) {			// Capture 25 Averages
			--ui8CaptureTime;
			ui32FirstWireVoltageAverage += xgRuntimeState.xVoltage.ui32VoltageAverageUW;
		}

		// All done
		if (!ui8CaptureTime) {
			ui32FirstWireVoltageAverage = ui32FirstWireVoltageAverage / 25;
			++ui8State;
		}
		break;
	case 4:
		if (FALSE == ui8SignalHigh) {
			ui8Delay = 40;			// wait 40 ms before capturing the second part of the signal
			++ui8State;
		}
		break;
	case 5:
		--ui8Delay;
		if (!ui8Delay) {
			ui8CaptureTime = 25;	// Capture for another 25 ms
			ui32SecondWireVoltageAverage = 0;
			++ui8State;
		}
		break;
	case 6:
		if (ui8CaptureTime) {
			--ui8CaptureTime;
			ui32SecondWireVoltageAverage += xgRuntimeState.xVoltage.ui32VoltageAverageUW;		// Capture the second part of the signal: Low part, symmetrical 1.4 -> 1.6 Volt
		}

		if (!ui8CaptureTime) {
			ui32SecondWireVoltageAverage = ui32SecondWireVoltageAverage / 25;
			++ui8State;
		}
		break;
	case 7:

		// Wiretesting works only till 65 Hz, so we test it only till 65 Hz
		if (FREQ_65HZ > xgRuntimeState.xCaptured.ui32InterruptCapturedFrequency) {

			// Reference voltage should about 1.4V
			if (ui32FirstWireVoltageAverage > VOLTAGE_3v0) {

				// See if the test signal is there, then nothing is connected or the wire is broken
				// First part: higher than 3V
				// secondpart: between 1.3V to 1.5
				// maybe: 1.4 to 1.6 would be better.
				if (ui32FirstWireVoltageAverage > VOLTAGE_3v0 && ui32SecondWireVoltageAverage > VOLTAGE_1v3 && ui32SecondWireVoltageAverage < VOLTAGE_1v6) {

					ui8ConnectionStatus = FALSE;
				} else {
					ui8ConnectionStatus = TRUE;
				}
			} else {
				ui8ConnectionStatus = TRUE;
			}

		} else {
			ui8ConnectionStatus = TRUE;
		}

		// Evaluate the connection stawtus
		if (ui8ConnectionStatus == FALSE) {

			// Wire is not connected for 10 cycles
			if (xgErrorCounters.ui8ErrorCounterWireBreakage < ERROR_WIREBREAkAGE_COUNTER) {
				++xgErrorCounters.ui8ErrorCounterWireBreakage;
			}
		} else if (xgErrorCounters.ui8ErrorCounterWireBreakage > 0) {
			--xgErrorCounters.ui8ErrorCounterWireBreakage;
		}

		// Reset the timer if the signal is okay
		if (xgIpkTx.xErrors1.ui1ErrorWireBreakageTestSignal == FALSE) {
			xgErrorCounters.ui16ErrorCounterWireBreakageSignal = 0;
		}

		ui8State = 0;
		break;
	default:
		break;

	}

	// Pulse did not come: SR0020
	if (++xgErrorCounters.ui16ErrorCounterWireBreakageSignal >= ERROR_WIREBREAKAGE_SINGAL_1MS) {
		xgIpkTx.xErrors1.ui1ErrorWireBreakageTestSignal = TRUE;
	}

	// Wirebreakage: SR0015
	if (xgErrorCounters.ui8ErrorCounterWireBreakage >= ERROR_WIREBREAkAGE_COUNTER) {
		xgIpkTx.xErrors2.ui1ErrorWireBreakage = TRUE;
	}
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Test if the Differential-Opamp is still working /SR0031
 *    Frequency:  100ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vAdcCheckExtendedWirebreakage(uint8_t *pui8CheckinCounter) {
	++(*pui8CheckinCounter);
	// TODO add extended wirebreakage in the next hardware version!
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Test if the Schmitttrigger is still working /SR0012
 *    Frequency:  100ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vAdcCheckDigitalAnalogCoupling(uint8_t *pui8CheckinCounter) {
#define ACTIVATION_WEDGE_1MS 800
	static uint32_t ui32MinValue = 0xFFFFFFFF, ui32MaxValue = 0;
	static uint16_t ui16SamplesCount = 0;

	uint32_t ui32VoltageDifference = 0;
	uint8_t ui8VoltageNoFrequency = FALSE;
	uint8_t ui8FrequencyNoVoltage = FALSE;

	uint8_t ui8Hysteresis = 0xFF;

	++(*pui8CheckinCounter);
	ui16SamplesCount++;

	// Set the new Mins and Maxes
	if (xgRuntimeState.xVoltage.ui32VoltageMinimumUW < ui32MinValue)
		ui32MinValue = xgRuntimeState.xVoltage.ui32VoltageMinimumUW;

	if (xgRuntimeState.xVoltage.ui32VoltageMaximumUW > ui32MaxValue)
		ui32MaxValue = xgRuntimeState.xVoltage.ui32VoltageMaximumUW;

	// Check every 200 ms
	if (ui16SamplesCount >= 200) {
		ui8Hysteresis = xgIpkTx.xStatus.ui2Hysteresis;

		ui32VoltageDifference = ui32MaxValue - ui32MinValue;		// Build the difference

		if (!xgRuntimeState.xSpeed.ui32HysteresisTimeout) {
			switch (ui8Hysteresis) {
			case 1: // 0,5 Volt
				if (ui32VoltageDifference >= ANALOGPATH_VOLTAGE_FOR_HYST1_FORWARD) {		// Took the old values and converted them to 12 bit
					ui8VoltageNoFrequency = TRUE;
				}
				break;
			case 0:	// 1 Volt
				if (ui32VoltageDifference >= ANALOGPATH_VOLTAGE_FOR_HYST0_FORWARD) {
					ui8VoltageNoFrequency = TRUE;
				}
				break;
			case 3: // 2 Volt
				if (ui32VoltageDifference >= ANALOGPATH_VOLTAGE_FOR_HYST3_FORWARD) {
					ui8VoltageNoFrequency = TRUE;
				}
				break;
			case 2: // 3 Volt
				if (ui32VoltageDifference >= ANALOGPATH_VOLTAGE_FOR_HYST2_FORWARD) {
					ui8VoltageNoFrequency = TRUE;
				}
				break;
			default:
				break;
			}
		} else if ((xgRuntimeState.xSpeed.ui32HysteresisTimeout > ACTIVATION_WEDGE_1MS)
				&& (xgRuntimeState.xSpeed.ui32MasterFrequency > FREQ_20HZ && xgRuntimeState.xSpeed.ui32MasterFrequency < FREQ_1000HZ)) {
			switch (ui8Hysteresis) {

			case 1:	// 0,5 Volt
				if (ui32VoltageDifference < ANALOGPATH_VOLTAGE_FOR_HYST1_BACKWARD) {
					ui8FrequencyNoVoltage = TRUE;
				}
				break;
			case 0: // 1 Volt
				if (ui32VoltageDifference < ANALOGPATH_VOLTAGE_FOR_HYST0_BACKWARD) {
					ui8FrequencyNoVoltage = TRUE;
				}
				break;
			case 3: // 2 Volt
				if (ui32VoltageDifference < ANALOGPATH_VOLTAGE_FOR_HYST3_BACKWARD) {
					ui8FrequencyNoVoltage = TRUE;
				}
				break;
			case 2: // 3 Volt
				if (ui32VoltageDifference < ANALOGPATH_VOLTAGE_FOR_HYST2_BACKWARD) {
					ui8FrequencyNoVoltage = TRUE;
				}
				break;
			default:
				break;
			}
		}

		if (ui8VoltageNoFrequency || ui8FrequencyNoVoltage) {
			xgErrorCounters.ui8ErrorCounterAdcFrequency += 2;
		} else if (xgErrorCounters.ui8ErrorCounterAdcFrequency > 0) {
			--xgErrorCounters.ui8ErrorCounterAdcFrequency;
		}

		// Set ADC Frequency Error SR0028
		if ((xgErrorCounters.ui8ErrorCounterAdcFrequency >= MAX_VOLTAGE_ADC_FREQUENCY_ERROR)) {
			xgIpkTx.xErrors2.ui1ErrorAdcFrequency = TRUE;
		}

		ui16SamplesCount = 0;
		ui32MaxValue = 0;
		ui32MinValue = 0xFFFFFFFF;

	}
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Test if adc unit is still working at the right pace /SR0028
 *    Frequency:  1ms
 *    Parameters: ui32Flag - Condition to check
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
inline void vAdcCheckUnit(uint32_t ui32Flag) {
	if (ui32Flag) {
		if (xgErrorCounters.ui8ErrorCounterAdcTimeout > 0) {
			--xgErrorCounters.ui8ErrorCounterAdcTimeout;
		}
	} else {
		++xgErrorCounters.ui8ErrorCounterAdcTimeout;
	}

	// Adc did not deliver in the expected time
	//-----------------------------------------
	if (xgErrorCounters.ui8ErrorCounterAdcTimeout >= MAX_ADC_TIMEOUT) {
		xgIpkTx.xErrors1.ui1ErrorAdcTimeout = TRUE;
	}
}

/*-------------------------------------------------------------------------------------------------
 *    Description: Calculate Voltage Averages, Mins and Maxes. Increment Voltage Error Counters
 *    Frequency:  1ms
 *    Parameters: pui8CheckinCounter - SchedulerCounter
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vAdcFetchValues(uint8_t *pui8CheckinCounter) {
	uint32_t ui32Offset;
	uint32_t ui32Temp;
	uint32_t ui32ConversionReady;

	++(*pui8CheckinCounter);

	// Check pace
	ui32ConversionReady = pxgAdcSoftwareBuffer->ui32ConversionReady;
	vAdcCheckUnit(ui32ConversionReady);

	// Process newly captured adc data
	if (ui32ConversionReady) {
		xgRuntimeState.xVoltage.ui32VoltageAverageUW = 0;
		xgRuntimeState.xVoltage.ui32VoltageMaximumUW = 0;
		xgRuntimeState.xVoltage.ui32VoltageMinimumUW = 0xFFFFFFFF;

		xgRuntimeState.xVoltage.ui32VoltageEmkAverageUW = 0;
		xgRuntimeState.xVoltage.ui32VoltageEmkMaximumUW = 0;
		xgRuntimeState.xVoltage.ui32VoltageEmkMinimumUW = 0xFFFFFFFF;

		// Calculate averages, min and max of L1
		for (uint8_t ui8idx = 0; ui8idx < MAX_CONVERSIONS; ui8idx++) {
			ui32Offset = ui8idx * MAX_ADC_SOURCES;

			// Sequence:
			// [0]L1 UW, [1]12 V, [2]3.3 V, [3]1.65V, [4]EMK
			//------------------------------------
			// Adc Freq.
			ui32Temp = pxgAdcSoftwareBuffer->rui32AdcConverted[ui32Offset + INPUT_OFFSET_L1];
			xgRuntimeState.xVoltage.ui32VoltageAverageUW += ui32Temp;

			// if value is bigger than current max
			if (ui32Temp > xgRuntimeState.xVoltage.ui32VoltageMaximumUW) {
				xgRuntimeState.xVoltage.ui32VoltageMaximumUW = ui32Temp;
			}

			// if value is smaller than the current min
			if (ui32Temp < xgRuntimeState.xVoltage.ui32VoltageMinimumUW) {
				xgRuntimeState.xVoltage.ui32VoltageMinimumUW = ui32Temp;
			}

			// Emk
			ui32Temp = pxgAdcSoftwareBuffer->rui32AdcConverted[ui32Offset + INPUT_OFFSET_EMK];
			xgRuntimeState.xVoltage.ui32VoltageEmkAverageUW += ui32Temp;

			// if value is bigger than current max
			if (ui32Temp > xgRuntimeState.xVoltage.ui32VoltageEmkMaximumUW) {
				xgRuntimeState.xVoltage.ui32VoltageEmkMaximumUW = ui32Temp;
			}

			// if value is smaller than the current min
			if (ui32Temp < xgRuntimeState.xVoltage.ui32VoltageEmkMinimumUW) {
				xgRuntimeState.xVoltage.ui32VoltageEmkMinimumUW = ui32Temp;
			}
		}

		// L1 average
		xgRuntimeState.xVoltage.ui32VoltageAverageUW = xgRuntimeState.xVoltage.ui32VoltageAverageUW / MAX_CONVERSIONS;

		// L1 emk average
		xgRuntimeState.xVoltage.ui32VoltageEmkAverageUW = xgRuntimeState.xVoltage.ui32VoltageEmkAverageUW / MAX_CONVERSIONS;

		// Set voltages: No Average for these voltages. Take the last samples
		xgRuntimeState.xVoltage.ui32Voltage12 = pxgAdcSoftwareBuffer->rui32AdcConverted[ui32Offset + INPUT_OFFSET_12V];
		xgRuntimeState.xVoltage.ui32Voltage3v3 = pxgAdcSoftwareBuffer->rui32AdcConverted[ui32Offset + INPUT_OFFSET_3V3];
		xgRuntimeState.xVoltage.ui32Voltage1v65 = pxgAdcSoftwareBuffer->rui32AdcConverted[ui32Offset + INPUT_OFFSET_1V65];

		pxgAdcSoftwareBuffer->ui32ConversionReady = FALSE;
		pxgAdcSoftwareBuffer->ui32ConversionsCount = 0;

		DEBUGCODE(++xgDebugStatistics.ui32AdcExecutions; mDebugPutIntoArray0(xgRuntimeState.xVoltage.ui32VoltageEmkAverageUW);)
		//memset(pxgAdcSoftwareBuffer->rui32AdcConverted, 0, sizeof(pxgAdcSoftwareBuffer->rui32AdcConverted)); // clear the values
	}
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Check if Voltage is okay /SR0008 /SR0009 /SR0010
 *    Frequency:  1ms
 *    Parameters: pui8CheckinCounter	-	Scheduler Counter
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vAdcCheckVoltage(uint8_t *pui8CheckinCounter) {
	++(*pui8CheckinCounter);

// Compare Voltages
//-------------------
// 3v3
	if ((xgRuntimeState.xVoltage.ui32Voltage3v3 > MAX_VOLTAGE_3V3) || (xgRuntimeState.xVoltage.ui32Voltage3v3 < MIN_VOLTAGE_3V3)) {
		if (xgErrorCounters.ui8ErrorCounterVoltage3v3 < MAX_VOLTAGE_ERROR) {
			++xgErrorCounters.ui8ErrorCounterVoltage3v3;
		}
	} else if (xgErrorCounters.ui8ErrorCounterVoltage3v3 > 0) {
		--xgErrorCounters.ui8ErrorCounterVoltage3v3;
	}

// 1v65
	if ((xgRuntimeState.xVoltage.ui32Voltage1v65 > MAX_VOLTAGE_1V65) || (xgRuntimeState.xVoltage.ui32Voltage1v65 < MIN_VOLTAGE_1V65)) {
		if (xgErrorCounters.ui8ErrorCounterVoltage1v65 < MAX_VOLTAGE_ERROR) {
			++xgErrorCounters.ui8ErrorCounterVoltage1v65;
		}
	} else if (xgErrorCounters.ui8ErrorCounterVoltage1v65 > 0) {
		--xgErrorCounters.ui8ErrorCounterVoltage1v65;
	}

// 12v
	if ((xgRuntimeState.xVoltage.ui32Voltage12 > MAX_VOLTAGE_12V) || (xgRuntimeState.xVoltage.ui32Voltage12 < MIN_VOLTAGE_12V)) {
		if (xgErrorCounters.ui8ErrorCounterVoltage12v < MAX_VOLTAGE_ERROR) {
			++xgErrorCounters.ui8ErrorCounterVoltage12v;
		}
	} else if (xgErrorCounters.ui8ErrorCounterVoltage12v > 0) {
		--xgErrorCounters.ui8ErrorCounterVoltage12v;
	}

// Voltage counters
//---------------------------------------------------------------------
	if (xgErrorCounters.ui8ErrorCounterVoltage3v3 >= MAX_VOLTAGE_ERROR) {
		xgIpkTx.xErrors0.ui1ErrorVoltage3v3 = TRUE;
	}

	if (xgErrorCounters.ui8ErrorCounterVoltage1v65 >= MAX_VOLTAGE_ERROR) {
		xgIpkTx.xErrors0.ui1ErrorVoltage1v65 = TRUE;
	}

	if (xgErrorCounters.ui8ErrorCounterVoltage12v >= MAX_VOLTAGE_ERROR) {
		xgIpkTx.xErrors0.ui1ErrorVoltage12v = TRUE;
	}

}
