/*
 * safetycircuits.h
 *
 *  Created on: 19.07.2021
 *      Author: am
 */

#ifndef SAFE_CIRCUITS_H_
#define SAFE_CIRCUITS_H_

/* Public datatypes */


typedef struct {
	uint8_t ui8SkrState;
	uint8_t ui8SkrQDelay;
	uint8_t ui8SkrTimeout;
	uint8_t ui8Qpos;
	uint8_t ui8Q;
	uint8_t ui8SkrOut;
	uint8_t ui8SkrQuitt;
} SafetyCircuit_t;

/*-------------------------------
 * 	Safety Circuit Errors
 *------------------------------*/
typedef struct {
	struct {
		uint32_t ui32ErrorCounterCrosscircuitSK1a;
		uint32_t ui32ErrorCounterCrosscircuitSK1b;
		uint32_t ui32ErrorCounterCrosscircuitSK2a;
		uint32_t ui32ErrorCounterCrosscircuitSK2b;
	} xErrorCounters;

	SafetyCircuit_t xSafetyCircuit1;
	SafetyCircuit_t xSafetyCircuit2;

	uint8_t ui8SkrEvaluationDelayCounter;
} SafetyCircuitManagement;

/* Public variables */
extern SafetyCircuitManagement xgSafetyCircuitManagement;

/* Public Functions */
void vSafetyCircuitResetManagement(void);
void vSafetyCircuitCheckCrosscircuit(uint8_t *pui8CheckinCounter);
void vSafetyCircuitExecute(uint8_t *pui8CheckinCounter);
void vTwohandCircuitExecute(uint8_t *pui8CheckinCounter);

#endif
