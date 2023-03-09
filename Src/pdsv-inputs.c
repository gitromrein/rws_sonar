/*
 * pdsv-inputs.c
 *
 *  Created on: 25.08.2020
 *      Author: am
 */


#include "pdsv-parameters.h"
#include "pdsv-hardware.h"
#include "pdsv-inputs.h"
#include <pdsv-diagnostics.h>
#include "pdsv-ipk.h"

/* Private Data structures */
typedef struct {
	uint16_t ui16DebounceCounterA;
	uint16_t ui16DebounceCounterB;
	uint16_t ui16DebouncedInputState;
} Inputs_t;

/* Private defines */
#define ERROR_INPUT_1MS 2550		// 2,55s
#define ERROR_PORTTEST_COUNTER 10
#define ERROR_DYNAMIC_INPUT_COUNTER 10

#define mWaitForPin(texondition,timeout){timeout=0; while(texondition && (++timeout)<=200){};}
#define mWaitTimeout(timeout) {timeout=0; while(timeout<10){++timeout;};}

/* Public variables */
uint16_t ui16gSampledInputs;

/* Private functions */
static void vInputDebouce(uint16_t ui16SampledInputs, Inputs_t *pxInputs);

/*-------------------------------------------------------------------------------------------------
 *    Description: Check for crossconnected pins /SR0005
 *    Frequency:  100ms
 *    Parameters: pui8CheckinCounter	-	Scheduler Counter
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vInputCheckPort(uint8_t *pui8CheckinCounter) {

	uint16_t ui16PortIn;
	uint16_t ui16PortOut;
	static uint16_t ui16PortError = 0;

	uint8_t ui8PinTimeout;
	static uint8_t ui8PortTestState = 0;

	++(*pui8CheckinCounter);

	// Check if the pins are connected, which would mean they have an error
	ui16PortIn = mHardwareGetStateI1I2I3I4;


	// Go through all the pins
	switch (ui8PortTestState) {
	case 0:
		if (ui16PortIn & 0x01) {

			mHardwareSetStateOuputI1;
			mHardwareSetStateLowI1;
			mWaitForPin((mHardwareGetStateI1I2I3I4 & 0x01), ui8PinTimeout)
			ui16PortOut = mHardwareGetStateI1I2I3I4;
			ui16PortError = ((ui16PortIn ^ ui16PortOut) & 0xFE);
			mHardwareSetStateInputI1;
		}
		break;
	case 1:
		if (ui16PortIn & 0x02) {
			mHardwareSetStateOuputI2;
			mHardwareSetStateLowI2;
			mWaitForPin((mHardwareGetStateI1I2I3I4 & 0x02), ui8PinTimeout)

			ui16PortOut = mHardwareGetStateI1I2I3I4;
			ui16PortError = ((ui16PortIn ^ ui16PortOut) & 0xFD);
			mHardwareSetStateInputI2;
		}
		break;
	case 2:
		if (ui16PortIn & 0x04) {
			mHardwareSetStateOuputI3;
			mHardwareSetStateLowI3;
			mWaitForPin((mHardwareGetStateI1I2I3I4 & 0x04), ui8PinTimeout)

			ui16PortOut = mHardwareGetStateI1I2I3I4;
			ui16PortError = ((ui16PortIn ^ ui16PortOut) & 0xFB);
			mHardwareSetStateInputI3;
		}
		break;
	case 3:
		if (ui16PortIn & 0x08) {
			mHardwareSetStateOuputI4;
			mHardwareSetStateLowI4;
			mWaitForPin((mHardwareGetStateI1I2I3I4 & 0x08), ui8PinTimeout)
			ui16PortOut = mHardwareGetStateI1I2I3I4;
			ui16PortError = ((ui16PortIn ^ ui16PortOut) & 0xF7);
			mHardwareSetStateInputI4;
		}
		break;
	}

	// Durchlauf komplett, gab es einen Error?
	if (ui8PortTestState == 3) {
		if (ui16PortError == 0) {
			xgErrorCounters.ui8ErrorCounterPort = 0;
		} else {
			if (xgErrorCounters.ui8ErrorCounterPort < ERROR_PORTTEST_COUNTER)
				xgErrorCounters.ui8ErrorCounterPort++;

			if (xgErrorCounters.ui8ErrorCounterPort >= ERROR_PORTTEST_COUNTER) {
				xgIpkTx.xErrors1.ui1ErrorPortTest = TRUE;
			}
		}

		ui16PortError = 0;
	}

	ui8PortTestState = 3;
	//ui8PortTestState = (ui8PortTestState + 1) % 4;
	mHardwareSetStateInputI1I2I3I4;
}



/*-------------------------------------------------------------------------------------------------
 *    Description: Check if 2-channel-inputs are not the same Check /SR0011
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
inline void vInputCheckAsynchronous(void) {

	if (xgIpkTx.xInputs.ui4Inputs != xgIpkRx.xInputs.ui4Inputs) {
		xgErrorCounters.ui16ErrorCounterInput++;
		if (xgErrorCounters.ui16ErrorCounterInput == ERROR_INPUT_1MS) {
			xgIpkTx.xErrors1.ui1ErrorInputsNotEqual = 1;
			DEBUGCODE(xgDebugStatistics.ui8InputXor = (xgIpkTx.xInputs.ui4Inputs ^ xgIpkRx.xInputs.ui4Inputs););
		}
	} else {
		xgErrorCounters.ui16ErrorCounterInput = 0;
	}
}

/*-------------------------------------------------------------------------------------------------
 *    Description: Dynamic Input test. Must be executed after the input has been
 *    					   read in by the vDebouceInput4ms routine /SR0019
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
inline void vInputCheckDynamic(void) {
	static uint16_t ui16InputBeforeTest = 0;
	static uint8_t ui8TestCounter = 0;
	uint16_t ui16InputsAfterTest = 0;

	++ui8TestCounter;

	// After 100ms run the check for 1 ms
	if (ui8TestCounter == 100) {
		ui16InputBeforeTest = mHardwareGetStateI1I2I3I4
		;

		// Are the inputs on
		if (ui16InputBeforeTest != 0) {
			mHardwareDisableOptoInputs;
		} else {
			ui8TestCounter = 0;
		}

	} else if (ui8TestCounter == 101) {
		ui16InputsAfterTest = mHardwareGetStateI1I2I3I4
		;

		// were the inputs turned on
		if (ui16InputsAfterTest != 0) {
			++xgErrorCounters.ui8ErrorCounterDynamicInputTest;
		} else if (xgErrorCounters.ui8ErrorCounterDynamicInputTest > 0) {
			--xgErrorCounters.ui8ErrorCounterDynamicInputTest;
		}

		mHardwareEnableOptoInputs;
		ui8TestCounter = 0;
	}

	if (xgErrorCounters.ui8ErrorCounterDynamicInputTest >= ERROR_DYNAMIC_INPUT_COUNTER) {
		xgIpkTx.xErrors1.ui1ErrorDynamicInputTest = TRUE;
	}

}

/*-------------------------------------------------------------------------------------------------
 *    Description: Debounce routine for 4 cycles
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
static void vInputDebouce(uint16_t ui16SampledInputs, Inputs_t *pxInputs) {
	uint16_t ui16CounterA;
	uint16_t ui16CounterB;

	uint16_t ui16Delta;
	uint16_t ui16Combined;

	// Load Variables
	ui16CounterA = pxInputs->ui16DebounceCounterA;
	ui16CounterB = pxInputs->ui16DebounceCounterB;
	ui16Delta = (ui16SampledInputs ^ pxInputs->ui16DebouncedInputState);

	// Increment Vertical Counter
	ui16CounterA = (ui16CounterA ^ ui16CounterB);
	ui16CounterB = (~ui16CounterB);

	// Reset Vertical Counter if there is no Change or increment it further, eventually 0
	ui16CounterB = (ui16Delta & ui16CounterB);
	ui16CounterA = (ui16Delta & ui16CounterA);

	ui16Combined = (ui16CounterA | ui16CounterB);	// Not 0 if there is a change, will be eventually 0, either when there is no change or the change stayed for 4 cycles

	// Update variables
	pxInputs->ui16DebouncedInputState = ((~ui16Combined & ui16SampledInputs) | (ui16Combined & pxInputs->ui16DebouncedInputState));
	pxInputs->ui16DebounceCounterB = ui16CounterB;
	pxInputs->ui16DebounceCounterA = ui16CounterA;

}

/*-------------------------------------------------------------------------------------------------
 *    Description: Input sample routine
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vInputSample(uint8_t *pui8CheckinCounter) {
	// Inputs: 0= 4ms 1= 16ms
	static Inputs_t xInputs[2] = { { 0, 0, 0 }, { 0, 0, 0 } };
	static uint8_t ui8InputStateMachine = 0;

	++(*pui8CheckinCounter);

	// Read Inputs
	ui16gSampledInputs = mHardwareGetStateI1I2I3I4;

	// Debounce 4ms
	vInputDebouce(ui16gSampledInputs, &xInputs[0]);
	if (++ui8InputStateMachine == 4) {

		// Debounce 16 ms
		ui8InputStateMachine = 0;
		vInputDebouce(xInputs[0].ui16DebouncedInputState, &xInputs[1]);
	}

	// Set the debounced inputs
	xgIpkTx.xInputs.ui4Inputs = (((xgRuntimeParameters.ui8GeneralInputDebounce & xInputs[0].ui16DebouncedInputState)
			| ((~xgRuntimeParameters.ui8GeneralInputDebounce) & xInputs[1].ui16DebouncedInputState))) & 0x0F;

	vInputCheckAsynchronous();		// SR0011 WARNING! Has to be executed after the inputs have been decided on, but before the Safety Precautions
	vInputCheckDynamic();				// SR0019


	// SRS Errors: Safety Precautions
	//-------------------------------------
	if (xgIpkTx.xErrors1.ui1ErrorPortTest || xgIpkTx.xErrors1.ui1ErrorDynamicInputTest || xgIpkTx.xErrors1.ui1ErrorInputsNotEqual) {
		xgIpkTx.xInputs.ui4Inputs = 0x00;
	}

}
