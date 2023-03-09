
/*
 *  helpers.c
 *
 *  Created on: 24.10.2019
 *      Author: am
 */

#include "helpers.h"

/*---------------------------------------------
 *
 *  Simple waiting function based on cycles
 *
 *---------------------------------------------*/
void vWaitCycles( uint16_t n ) {
#define CYCLES_PER_LOOP 3
    uint16_t l = n/CYCLES_PER_LOOP;
    asm volatile( "0:" "SUBS %[count], 1;" "BNE 0b;" :[count]"+r"(l) );
}

/*---------------------------------------------
 *
 *  Convert from little to big endian
 *
 *---------------------------------------------*/
uint32_t ui32ConvertToBigEndian(uint32_t ui32Param){
	return ((ui32Param << 24) & 0xFF000000) | ((ui32Param << 8) & 0x00FF0000) | ((ui32Param >> 8) & 0x00FF00) | ((ui32Param >> 24) & 0x00FF);
}

/*---------------------------------------------
 *
 *  Convert value to 10base representation
 *
 *---------------------------------------------*/
uint32_t ui32ConvertToBcd(uint16_t ui16Value){
	uint32_t ui32Result = 0;
	uint8_t ui8Index = 5;
	uint32_t ui32Divider = 10000;
	uint32_t ui32Temp = 0;

	for(; ui8Index>0;--ui8Index){
		ui32Temp = ui16Value/ui32Divider;
		ui32Result = ((ui32Result|(ui32Temp))<<4);
		ui16Value = ui16Value-(ui32Temp*ui32Divider);
		ui32Divider = ui32Divider/10;
	}

	return ui32Result>>4;
}

/********************************************--
 *
 *  Generate CRC16 given the Polynom
 *
 *******************************************--*/
inline uint16_t ui16GenerateCRC16(const uint8_t *pui8ParameterData, uint16_t ui16ParameterSize, const uint16_t ui16Polynom, const uint16_t ui16CrcInitial)
{


	uint32_t ui32Crc = ui16CrcInitial;
	uint16_t ui16Size = ui16ParameterSize;
	uint8_t ui8Bitcounter;

	for (; ui16Size>0; ui16Size--)               						/* Step through bytes in memory */
	  {
		ui32Crc = ui32Crc ^ (*pui8ParameterData++ << 8);      							/* Fetch byte from memory, XOR into CRC top byte*/
	  for (ui8Bitcounter=0; ui8Bitcounter<8; ui8Bitcounter++)               /* Prepare to rotate 8 bits */
	    {
		  ui32Crc = ui32Crc << 1;          		/* rotate */
	    if (ui32Crc & 0x10000)             		/* bit 15 was set (now bit 16)... */
	    	ui32Crc = (ui32Crc ^ ui16Polynom) & 0xFFFF; /* XOR with XMODEM polynomic */
	    							   	   	   /* and ensure CRC remains 16-bit value */
	    }                              		/* Loop for 8 bits */
	  }                                		/* Loop until num = 0 */

	  return(ui32Crc);                     /* Return updated CRC */
}

