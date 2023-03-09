/*
 * 	pdsv-ipk.h
 *
 *  Created on: 21.07.2021
 *      Author: am
 */

#ifndef PDSV_IPK_H_
#define PDSV_IPK_H_

/* Public defines */
#define FRAMETYPE_NORMAL 0x00
#define FRAMETYPE_LATCH_MASTER 0x01
#define FRAMETYPE_LATCH_SLAVE 0x02

/* Public datatypes */
typedef struct {
	struct {

		// byte 0
		union {
			struct {
				uint8_t ui2Frametype :2;
				uint8_t ui1Reserved0 :1;
				uint8_t ui1Timeout1s :1;
				uint8_t ui2Hysteresis :2;
				uint8_t ui1ParameterComplete :1;
				uint8_t ui1nSlok :1;
			} xStatus;
			uint8_t ui8Status;
		};

		// byte 1
		union {
			struct {
				uint8_t ui4Inputs :4;
				uint8_t ui4Reserved1 :4;
			} xInputs;
			uint8_t ui8Inputs;
		};

		// byte 2
		union {
			struct {
				uint8_t ui4Operatingstate :4;
				uint8_t ui2Validationbits :2;
				uint8_t ui2Reserved2 :2;
			} xModifiers;
			uint8_t ui8Modifiers;
		};

		// byte 3
		union {
			struct {
				uint8_t ui1Zh :1;
				uint8_t ui3Reserved3 :3;
				uint8_t ui1Skr1 :1;
				uint8_t ui1Skr2 :1;
				uint8_t ui2Reserved3d :2;
			} xSafetyCircuit;
			uint8_t ui8SafetyCircuit;
		};

		// byte 4
		union {

			struct {
				uint8_t ui1Limit0 :1;
				uint8_t ui1Limit1 :1;
				uint8_t ui1Limit2 :1;
				uint8_t ui1Limit3 :1;

				uint8_t ui1Qacknowledge :1;
				uint8_t ui1Direction :1;
				uint8_t ui1Acceleration :1;
				uint8_t ui1ReservedXX :1;
			} xLogic;
			uint8_t ui8Logic;
		};

		// byte 5
		union {
			union {
				struct {
					uint8_t ui1Q0 :1;
					uint8_t ui1Q1 :1;
					uint8_t ui1Q2 :1;
					uint8_t ui1Q3 :1;
					uint8_t ui2Reserved000 :2;
					uint8_t ui1Emk :1;
					uint8_t ui1EmkReady :1;
				};

				struct {
					uint8_t ui4Q :4;
				};

			} xSpeedEmk;

			uint8_t ui8SpeedEmk;
		};

		// byte 6
		uint8_t ui8FrequencyHighbyte;

		// byte 7
		uint8_t ui8FrequencyLowbyte;

		// byte 8
		uint8_t ui8EmkVoltage;

		// byte 9
		uint8_t ui8ReservedByte9;

		// byte 10
		uint8_t ui8ReservedByte10;

		// byte 11
		union {
			struct {
				uint8_t ui1ErrorVoltage3v3 :1;
				uint8_t ui1ErrorVoltage1v65 :1;
				uint8_t ui1ErrorVoltage12v :1;
				uint8_t ui1ErrorCanTimeout :1;
				uint8_t ui1ErrorIpk :1;
				uint8_t ui1ErrorParameterChecksum :1;
				uint8_t ui1ErrorChecksumExchange :1;
				uint8_t ui1ErrorVersion :1;
			} xErrors0;
			uint8_t ui8Errors0;
		};

		// byte 12
		union {
			struct {
				uint8_t ui1ErrorWireBreakageTestSignal :1;
				uint8_t ui1ErrorParameter :1;
				uint8_t ui1ErrorParameterCopy :1;
				uint8_t ui1ErrorEmkSingle :1;
				uint8_t ui1ErrorPortTest :1;
				uint8_t ui1ErrorDynamicInputTest :1;
				uint8_t ui1ErrorInputsNotEqual :1;
				uint8_t ui1ErrorAdcTimeout :1;

			} xErrors1;
			uint8_t ui8Errors1;
		};

		// byte 13
		union {
			struct {
				uint8_t ui1ErrorWireBreakage :1;
				uint8_t ui1ErrorWireBreakageExtended :1;
				uint8_t ui1ErrorPhase :1;
				uint8_t ui1ErrorDirectionSwitch :1;
				uint8_t ui1ErrorFrequency1500hz :1;
				uint8_t ui1ErrorFrequencyNotEqual :1;
				uint8_t ui1ErrorAdcFrequency :1;
				uint8_t ui1ErrorQuitt :1;

			} xErrors2;
			uint8_t ui8Errors2;
		};

		// byte 14
		union {
			struct {
				uint8_t ui1ErrorGeneral1 :1;
				uint8_t ui1ErrorGeneral2 :1;
				uint8_t ui1ErrorCrosscircuit1 :1;
				uint8_t ui1ErrorCrosscircuit2 :1;
				uint8_t ui1ErrorInitialState1 :1;
				uint8_t ui1ErrorInitialState2 :1;
				uint8_t ui2Reserved2222 :2;
			} xSafetyCircuitErrors;
			uint8_t ui8SafetyCircuitErrors;
		};

		// byte 15
		uint8_t ui8DncoIndex;

		// byte 17
		union {
			uint16_t ui16Checksum;
			uint8_t rui8Checksum[2];
		};

		// byte 18
		uint8_t ui8Version;

		// byte 19
		uint8_t ui8TailPattern;

	};

} Ipk_t;

/* Public Variables */
extern Ipk_t xgIpkRx;
extern Ipk_t xgIpkTx;

extern uint8_t rui8gLatchedIpkBufferMaster[sizeof(Ipk_t)];
extern uint8_t rui8gLatchedIpkBufferSlave[sizeof(Ipk_t)];

extern volatile uint8_t rui8gHardwareIpkBufferOut[sizeof(Ipk_t)];
extern volatile uint8_t rui8gHardwareIpkBufferIn[sizeof(Ipk_t)];
extern volatile uint8_t ui8gHardwareUartSwapRequested;

/* Public prototypes */
extern void vIpkClearLatchBuffers(void);
extern void vIpkSaveLatchBuffer(void);
extern void vIpkCheckTimeout(uint8_t *pui8CheckinCounter);
extern void vIpkSwapBuffers(void);

#endif /* PDSV_IPK_H_ */
