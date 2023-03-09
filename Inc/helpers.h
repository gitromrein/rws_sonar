/*
 * helpers.h
 *
 *  Created on: 28.01.2022
 *      Author: am
 */

#ifndef HELPERS_H_
#define HELPERS_H_

/* Public defines  */
#define POLY_RUNTIME_PARAM 	0x1021U
#define POLY_RECEIVED_PARAM 0x1021U
#define CRC_INITIAL 0x0000U

/* Public functions  */
extern uint32_t ui32ConvertToBigEndian(uint32_t ui32Value);
extern uint32_t ui32ConvertToBcd(uint16_t ui16Value);
extern uint16_t ui16GenerateCRC16(const uint8_t *pui8ParameterData, uint16_t ui16ParameterSize, uint16_t ui16Polynom, uint16_t ui16CrcInitial);
#endif /* HELPERS_H_ */
