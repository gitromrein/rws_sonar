/*
 * 	pdsv-voltage.h
 *
 *  Created on: 19.07.2021
 *      Author: am
 */

#ifndef PDSV_VOLTAGE_H_
#define PDSV_VOLTAGE_H_


/* Public datatypes */
/**********************************************
 *
 * 	ADC Structure for holding adc values
 *
 **********************************************/
#define MAX_CONVERSIONS 12
#define MAX_ADC_SOURCES 5

#define INPUT_OFFSET_L1   0
#define INPUT_OFFSET_12V  1
#define INPUT_OFFSET_3V3  2
#define INPUT_OFFSET_1V65 3
#define INPUT_OFFSET_EMK  4

typedef struct {
	uint32_t ui32ConversionReady;	// Is set if MAX_CONVERSION has been reached
	uint32_t ui32ConversionsCount;
	uint32_t rui32AdcConverted[MAX_ADC_SOURCES * MAX_CONVERSIONS];		// Number of conversions
} AdcData_t;


/* Public variables */
#define MAX_ADC_DATA_BLOCKS 2
extern AdcData_t rxgAdcData[MAX_ADC_DATA_BLOCKS];						// 2 swap buffers for software and the hardware
extern AdcData_t *pxgAdcSoftwareBuffer;				// is initialized in the timer interrupt, the timer isr swaps the buffers between dma adc and main execution
extern AdcData_t *pxgAdcHardwareBuffer;
extern uint32_t rui32gDmaBuffer[];


/* Public functions */
extern void vAdcCheckUnit(uint32_t ui32Flag);
extern void vAdcCheckWirebreakage(uint8_t *pui8CheckinCounter);
extern void vAdcCheckExtendedWirebreakage(uint8_t *pui8CheckinCounter);
extern void vAdcCheckDigitalAnalogCoupling(uint8_t *pui8CheckinCounter);
extern void vAdcCheckVoltage(uint8_t *pui8CheckinCounter);
extern void vAdcFetchValues(uint8_t *pui8CheckinCounter);
extern void vAdcInitializeDmaBuffers(void);
#endif /* PDSV_VOLTAGE_H_ */
