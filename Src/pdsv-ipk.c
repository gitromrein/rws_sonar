/*
 * pdsv-ipk.c
 *
 *  Created on: 21.07.2021
 *      Author: am
 */

#include "pdsv-ipk.h"
#include <pdsv-diagnostics.h>
#include "pdsv-hardware.h"

#define ERROR_UART_1MS 250

/* Private defines */
#define FW_VERSION  0x01

/* Public variables */

Ipk_t xgIpkTx = { ui8TailPattern:0x55, ui8Version:FW_VERSION };
Ipk_t xgIpkRx = { ui8TailPattern:0xFF, ui8Version:0xFF };

volatile uint8_t rui8gHardwareIpkBufferOut[sizeof(Ipk_t)];
volatile uint8_t rui8gHardwareIpkBufferIn[sizeof(Ipk_t)];

uint8_t rui8gLatchedIpkBufferMaster[sizeof(Ipk_t)];
uint8_t rui8gLatchedIpkBufferSlave[sizeof(Ipk_t)];

/**-------------------------------------------------------------------------------------------------
 *    Description: Delete Latch information
 *    Frequency:  once
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vIpkClearLatchBuffers(void) {
	memset((uint8_t*) rui8gLatchedIpkBufferMaster, 0, sizeof(Ipk_t));
	memset((uint8_t*) rui8gLatchedIpkBufferSlave, 0, sizeof(Ipk_t));
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Copy Information from one buffer into another when DZ-bits turn off
 *    Frequency:  once
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vIpkSaveLatchBuffer(void) {
	xgIpkTx.xStatus.ui2Frametype = FRAMETYPE_LATCH_MASTER;
	memcpy((uint8_t*) rui8gLatchedIpkBufferMaster, (uint8_t*) &xgIpkTx, sizeof(Ipk_t));

}

/**-------------------------------------------------------------------------------------------------
 *    Description: Check if Ipk has timed out /SR0006
 *    Frequency:  1ms
 *    Parameters: pui8CheckinCounter	-	Scheduler Counter
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vIpkCheckTimeout(uint8_t *pui8CheckinCounter) {
	++(*pui8CheckinCounter);

	++xgErrorCounters.ui8ErrorCounterIpkTimeout;
	if (xgErrorCounters.ui8ErrorCounterIpkTimeout >= ERROR_UART_1MS) {		// Wenn die Interrupt Routine nicht aufgerufen wird
		xgIpkTx.xErrors0.ui1ErrorIpk = TRUE;
	}
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Copy Information from one buffer into another
 *    Frequency:  ~20ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vIpkSwapBuffers(void) {

	//Uart IR disable STM32
	NVIC_DisableIRQ(USART3_IRQn);  // built-in routines

	if (ui8gHardwareUartSwapRequested) {
		ui8gHardwareUartSwapRequested = FALSE;

		// TX Latch happened. Send it out once
		if (xgIpkTx.xStatus.ui2Frametype == FRAMETYPE_LATCH_MASTER) {
			memcpy((uint8_t*) rui8gHardwareIpkBufferOut, (uint8_t*) rui8gLatchedIpkBufferMaster, sizeof(Ipk_t));
			xgIpkTx.xStatus.ui2Frametype = FRAMETYPE_NORMAL;
		} else {
			memcpy((uint8_t*) rui8gHardwareIpkBufferOut, (uint8_t*) &xgIpkTx, sizeof(Ipk_t));
		}

		// RX Latch happened. Save it
		if (((Ipk_t*) rui8gHardwareIpkBufferIn)->xStatus.ui2Frametype == FRAMETYPE_LATCH_SLAVE) {
			memcpy((uint8_t*) rui8gLatchedIpkBufferSlave, (uint8_t*) rui8gHardwareIpkBufferIn, sizeof(Ipk_t));
		}

		// Copy and set as current received
		memcpy((uint8_t*) &xgIpkRx, (uint8_t*) rui8gHardwareIpkBufferIn, sizeof(Ipk_t));
	}

	NVIC_EnableIRQ(USART3_IRQn);  // built-in routines
}
