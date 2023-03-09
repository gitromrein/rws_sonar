/*
 *  pdsv-fdcan.h
 *
 *  Created on: 19.07.2021
 *      Author: am
 */

#ifndef PDSV_FDCAN_H_
#define PDSV_FDCAN_H_


/* Public defines */
#define CAN_BUF_COUNT 		6					// Fifos for RXFIFO0
#define CAN_BUF_SIZE		8
#define CAN_CYCLE_10MS      30      			// 300ms

#define dCanRxBuffer_AckOff 0x00


#define SRAMCAN_FLS_NBR                  (28U)         /* Max. Filter List Standard Number      */
#define SRAMCAN_FLE_NBR                  ( 8U)         /* Max. Filter List Extended Number      */
#define SRAMCAN_RF0_NBR                  ( 3U)         /* RX FIFO 0 Elements Number             */
#define SRAMCAN_RF1_NBR                  ( 3U)         /* RX FIFO 1 Elements Number             */
#define SRAMCAN_TEF_NBR                  ( 3U)         /* TX Event FIFO Elements Number         */
#define SRAMCAN_TFQ_NBR                  ( 3U)         /* TX FIFO/Queue Elements Number         */

#define SRAMCAN_FLS_SIZE            ( 1U * 4U)         /* Filter Standard Element Size in bytes */
#define SRAMCAN_FLE_SIZE            ( 2U * 4U)         /* Filter Extended Element Size in bytes */
#define SRAMCAN_RF0_SIZE            (18U * 4U)         /* RX FIFO 0 Elements Size in bytes      */
#define SRAMCAN_RF1_SIZE            (18U * 4U)         /* RX FIFO 1 Elements Size in bytes      */
#define SRAMCAN_TEF_SIZE            ( 2U * 4U)         /* TX Event FIFO Elements Size in bytes  */
#define SRAMCAN_TFQ_SIZE            (18U * 4U)         /* TX FIFO/Queue Elements Size in bytes  */

/*
 #define SRAMCAN_FLSSA ((uint32_t)0)                                                       Filter List Standard Start Address
 #define SRAMCAN_FLESA ((uint32_t)(SRAMCAN_FLSSA + (SRAMCAN_FLS_NBR * SRAMCAN_FLS_SIZE)))  Filter List Extended Start Address
 #define SRAMCAN_RF0SA ((uint32_t)(SRAMCAN_FLESA + (SRAMCAN_FLE_NBR * SRAMCAN_FLE_SIZE)))  Rx FIFO 0 Start Address
 #define SRAMCAN_RF1SA ((uint32_t)(SRAMCAN_RF0SA + (SRAMCAN_RF0_NBR * SRAMCAN_RF0_SIZE)))  Rx FIFO 1 Start Address
 #define SRAMCAN_TEFSA ((uint32_t)(SRAMCAN_RF1SA + (SRAMCAN_RF1_NBR * SRAMCAN_RF1_SIZE)))  Tx Event FIFO Start Address
 #define SRAMCAN_TFQSA ((uint32_t)(SRAMCAN_TEFSA + (SRAMCAN_TEF_NBR * SRAMCAN_TEF_SIZE)))  Tx FIFO/Queue Start Address
 #define SRAMCAN_SIZE  ((uint32_t)(SRAMCAN_TFQSA + (SRAMCAN_TFQ_NBR * SRAMCAN_TFQ_SIZE)))  Message RAM size
 */



/* Public datatypes */
/**---------------------------------------
 *  FDCAN Events
 *---------------------------------------*/
typedef struct {
	uint32_t rui32Event[2];
} FdcanTxEvent_t;

/**---------------------------------------
 *  FDCAN Fifo Buffers
 *---------------------------------------*/
typedef struct {
	uint32_t ui32Control0;
	uint32_t ui32Control1;
	union {
		uint32_t rui32Fifo[16];
		uint8_t rui8Fifo[16 * 4];	// DATA, later configured to be always 8
	};
} FdcanFifo_t;

/**---------------------------------------
 *  FDCAN Message Memory Structure
 *---------------------------------------*/
typedef struct {
	uint32_t ui32Filters[SRAMCAN_FLS_NBR];
	uint8_t ui8Extendedfiltersmemory[SRAMCAN_FLE_NBR * SRAMCAN_FLE_SIZE];
	FdcanFifo_t xRxfifo0[SRAMCAN_RF0_NBR];
	FdcanFifo_t xRxfifo1[SRAMCAN_RF1_NBR];
	FdcanTxEvent_t stcTxEvents[SRAMCAN_TEF_NBR];
	FdcanFifo_t rxTxFifo[SRAMCAN_TFQ_NBR];
} FdcanMessageRam_t;


/**--------------------------------------------
 * Structure for saving CAN Frames
 * Use case: max 8 bytes not 64
 *--------------------------------------------*/
typedef struct {
	uint16_t ui16Id;
	uint16_t ui16Size;
	union {
		uint8_t ui8Data[8];
		uint16_t ui16Data[4];
		uint32_t ui32Data[2];
	};
} CanMessage_t;



// Internal structure for can
typedef struct {
	union{
		struct {
			uint8_t ui5Identifier:6;
			uint8_t ui1ReceptionInProgress:1;
			uint8_t ui1nSlok:1;
		} xHeader;
		uint8_t ui8Byte0;
	};
	union{
		struct{
			uint8_t ui4Q:4;
			uint8_t ui4Reservedyy:4;
		} xSpeedOut;

		struct {
			uint8_t ui1SkrQ1:1;
			uint8_t ui1SkrQ2:1;
			uint8_t ui6Reserved000:6;

		} xZmvSafetyCircuitIn;

		uint8_t ui8Byte1;
	};
	union{
		struct {
			uint8_t ui1Zh:1;
			uint8_t ui3Reserved:3;
			uint8_t ui1Skr1:1;
			uint8_t ui1Skr2:2;
		} xSafetyCircuitOut;
		uint8_t ui8Byte2;
	};

	union{
		struct {
			uint8_t ui4Inputs :4;
			uint8_t ui4Reserved1 :4;
		} xInputsOut;

		struct {
			uint8_t ui4Operatingstate :4;
			uint8_t ui4Reserved1 :4;
		} xZmvModifiersIn;

		uint8_t ui8Byte3;
	};

	union {
		struct {
			uint8_t ui1Emk:1;
			uint8_t ui1Error:1;
			uint8_t ui6Reserved:6;
		} xEmkOut;

		struct {
			uint8_t ui4Reserved:4;
			uint8_t ui2Validationbits:2;
			uint8_t ui1RTDS:1;
			uint8_t ui1RTSK:1;
		} xZmvValidationIn;

		uint8_t ui8Byte4;
	};

	union {
		uint8_t ui8DiagnosticsIndex;
		uint8_t ui8ParametersIndex;
		uint8_t ui8Byte5;
	};

	union {
		uint8_t ui8Diagnostics0;
		uint8_t ui8Parameters0;
		uint8_t ui8Byte6;
	};

	union {
		uint8_t ui8Diagnostics1;
		uint8_t ui8Parameters1;
		uint8_t ui8Byte7;
	};

} Imk_t;


/* Public variables */
extern FdcanMessageRam_t * pxgFdcanMessageRam;
extern CanMessage_t xgCanMessageRx[CAN_BUF_COUNT];

extern Imk_t xgImkTx;
extern Imk_t xgImkRx;
extern uint8_t ui8gCircularHeadIndex;
extern uint8_t ui8gTriggerCanMail0, ui8gTriggerCanMail1;

/* Public functions */
extern void vCanCheckTimeout(uint8_t *pui8CheckinCounter);
extern void vCanReceiveFrame(void);
extern void vCanTransmitFrame(uint8_t *pui8CheckinCounter);

#endif /* PDSV_FDCAN_H_ */
