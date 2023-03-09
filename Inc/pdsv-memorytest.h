/*
 * pdsv-memorytest.h
 *
 *  Created on: 27.10.2021
 *      Author: am
 */

#ifndef PDSV_MEMORYTEST_H_
#define PDSV_MEMORYTEST_H_

/* Public functions */
extern void vRamCheckContinous(uint8_t *pui8CheckinCounter);
extern void vRomCheckContinous(uint8_t *pui8CheckinCounter);
extern void vRamInitialize(void);
extern void vRomInitialize(void);

#endif /* PDSV_MEMORYTEST_H_ */
