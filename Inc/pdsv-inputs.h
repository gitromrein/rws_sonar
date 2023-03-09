/*
 *  pdsv-inputs.h
 *
 *  Created on: 19.07.2021
 *      Author: am
 */

#ifndef PDSV_INPUTS_H_
#define PDSV_INPUTS_H_

/* Public variables */
extern uint16_t ui16gSampledInputs;

/* Public functions */
extern void vInputCheckDynamic(void);
extern void vInputCheckAsynchronous(void);
extern void vInputCheckPort(uint8_t *pui8CheckinCounter);
extern void vInputSample(uint8_t *pui8CheckinCounter);

#endif /* PDSV_INPUTS_H_ */
