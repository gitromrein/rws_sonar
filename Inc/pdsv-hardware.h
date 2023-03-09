/*
 *  pdsv-hardware.h
 *
 *  Created on: 19.07.2021
 *      Author: am
 */

#ifndef PDSV_HARDWARE_H_
#define PDSV_HARDWARE_H_

/* Public defines */
#define NOP asm("NOP")

#define NVIC_INT_PRIORITY_LVL3 	3
#define NVIC_INT_PRIORITY_LVL2	2
#define NVIC_INT_PRIORITY_LVL1	1

// INFO Debug.pins
//PA2 = Pin10
#define mDebugConfigurePin 	DEBUGCODE(	{		GPIOA->MODER &= (~GPIO_MODER_MODER2);\
												GPIOA->MODER |= GPIO_MODER_MODER2_0;\
												GPIOA->OTYPER &= (~GPIO_OTYPER_OT2);\
												GPIOA->OSPEEDR |= (GPIO_OSPEEDR_OSPEED2);}\
									  )
#define mDebugPinOn 		DEBUGCODE(			GPIOA->BSRR |= GPIO_BSRR_BS2;)
#define mDebugPinOff 		DEBUGCODE(			GPIOA->BSRR |= GPIO_BSRR_BR2;)
#define mDebugTogglePin(t) 	DEBUGCODE({\
												if(t){ mDebugPinOn;}\
												else { mDebugPinOff;} \
												t = ~t;}\
									  )

// Eingänge umschalten für Porttest
#define mHardwareSetStateInputI1 		(GPIOB->MODER &= (~GPIO_MODER_MODER6))
#define mHardwareSetStateInputI2 		(GPIOB->MODER &= (~GPIO_MODER_MODER7))
#define mHardwareSetStateInputI3 		(GPIOB->MODER &= (~GPIO_MODER_MODER8))
#define mHardwareSetStateInputI4 		(GPIOB->MODER &= (~GPIO_MODER_MODER9))
#define mHardwareSetStateInputI1I2I3I4  (GPIOB->MODER &= (~(GPIO_MODER_MODER6|GPIO_MODER_MODER7|GPIO_MODER_MODER8|GPIO_MODER_MODER9)))

#define mHardwareSetStateOuputI1 		(GPIOB->MODER |= (GPIO_MODER_MODER6_0))
#define mHardwareSetStateOuputI2 		(GPIOB->MODER |= (GPIO_MODER_MODER7_0))
#define mHardwareSetStateOuputI3 		(GPIOB->MODER |= (GPIO_MODER_MODER8_0))
#define mHardwareSetStateOuputI4 		(GPIOB->MODER |= (GPIO_MODER_MODER9_0))

#define mHardwareGetStateI1I2I3I4 		(((GPIOB->IDR) >> 6)&0x0000F)

// Control driver states of inputs
#define mHardwareSetStateHighI1   		(GPIOB->BSRR |= GPIO_BSRR_BS6)
#define mHardwareSetStateLowI1    		(GPIOB->BSRR |= GPIO_BSRR_BR6)
#define mHardwareSetStateHighI2   		(GPIOB->BSRR |= GPIO_BSRR_BS7)
#define mHardwareSetStateLowI2    		(GPIOB->BSRR |= GPIO_BSRR_BR7)
#define mHardwareSetStateHighI3   		(GPIOB->BSRR |= GPIO_BSRR_BS8)
#define mHardwareSetStateLowI3 	  		(GPIOB->BSRR |= GPIO_BSRR_BR8)
#define mHardwareSetStateHighI4   		(GPIOB->BSRR |= GPIO_BSRR_BS9)
#define mHardwareSetStateLowI4   		(GPIOB->BSRR |= GPIO_BSRR_BR9)

#define mHardwareSetStateLowI1I2I3I4 	(GPIOB->BSRR |= (GPIO_BSRR_BR6|GPIO_BSRR_BR7|GPIO_BSRR_BR8|GPIO_BSRR_BR9))

// Hyst
#define mHardwareSetHysteresis00		(GPIOB->BSRR |= GPIO_BSRR_BR10|GPIO_BSRR_BR12)
#define mHardwareSetHysteresis01		(GPIOB->BSRR |= GPIO_BSRR_BS10|GPIO_BSRR_BR12)
#define mHardwareSetHysteresis10		(GPIOB->BSRR |= GPIO_BSRR_BR10|GPIO_BSRR_BS12)
#define mHardwareSetHysteresis11		(GPIOB->BSRR |= GPIO_BSRR_BS10|GPIO_BSRR_BS12)

// Cap.coupling
#define mHardwareEnableCapaciveCoupling		(GPIOB->BSRR |= GPIO_BSRR_BR13)
#define mHardwareDisableCapacitiveCoupling 	(GPIOB->BSRR |= GPIO_BSRR_BS13)

// Direction
#define mHardwareGetStateDirection (GPIOA->IDR&0x03)

// Emk-Gain
#define mHardwareSetEmkHighGain (GPIOB->BSRR |= GPIO_BSRR_BS14)
#define mHardwareSetEmkLowGain  (GPIOB->BSRR |= GPIO_BSRR_BR14)

// Wirebreakage Testsignal
#define mHardwareGetSignal (GPIOB->IDR&0x020)

// Dynamic Input Test pin
#define mHardwareDisableOptoInputs	(GPIOB->BSRR |= GPIO_BSRR_BS15)
#define mHardwareEnableOptoInputs	(GPIOB->BSRR |= GPIO_BSRR_BR15)

// Sample frequency at which the Capture timer runs
#define CAPTURE_SAMPLE_FREQUENCY 12500000

/* Public variables */
extern volatile uint8_t ui8gHardwareTimerEvent;
extern volatile uint8_t ui8gHardwareUartSwapRequested;

/* Public prototypes */
extern void vHardwareCheckCaptureUnit(uint32_t ui32MeasuredDifference);
extern void vHardwareCheckUnexpectedInterrupt(void);
extern void vHardwareCheckInitialFlags(uint32_t ui32Csr);
extern void vHardwareCheckTimerMonitor(void);
extern void vHardwareCheckMainMonitor(void);
extern void vHardwareExecuteErrorLoop(uint8_t ui8HardError);
extern void vHardwareInitialize(void);

#endif /* PDSV_HARDWARE_H_ */
