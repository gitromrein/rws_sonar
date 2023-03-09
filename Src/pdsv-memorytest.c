/*
 * File:   pdsv-memorytest.c
 * Author: am
 * 10.01.2021
 */


#include "pdsv-memorytest.h"
#include "pdsv-hardware.h"
#include <pdsv-diagnostics.h>



//----------------------------------
// Romtest
//-----------------------------------

// Select Romtest type
#define ROMTEST_TYPE_NONE 0
#define ROMTEST_TYPE_COPY 1
#define ROMTEST_TYPE_CRC 2



#ifndef DEBUG_DISABLE_ROMTEST
	#define ROMTEST_TYPE ROMTEST_TYPE_CRC
	extern uint8_t _startFlash[];
	extern uint8_t _endFlash[];
	extern uint8_t _copyFlash[];
	extern uint16_t _sizeFlash;

#else
	#define ROMTEST_TYPE ROMTEST_TYPE_NONE
#endif



#if ROMTEST_TYPE == ROMTEST_TYPE_COPY
	#define FLASH_SIZE 0x10000
	#define ORIGINAL_FLASH  ((uint32_t*)_startFlash)
	#define COPY_FLASH	((uint32_t*)_copyFlash)


	// Die Kopie mit 0xFF initialisieren
	const uint32_t __attribute__((section(".copy"))) crui32FlashCopy[FLASH_SIZE / sizeof(uint32_t)] = { [0 ... FLASH_SIZE / sizeof(uint32_t) - 1]=0xFFFFFFFF };

	uint32_t* pui32OriginalAddress = ORIGINAL_FLASH;
	uint32_t* pui32RomCopyAddress = COPY_FLASH;
#elif ROMTEST_TYPE == ROMTEST_TYPE_CRC
	#define ORIGINAL_FLASH  ((uint32_t*)_startFlash)
	#define END_FLASH ((uint32_t*)_endFlash)


	uint32_t *pui32OriginalAddress = ORIGINAL_FLASH;

	static uint32_t __attribute__((section(".copy"))) __ui32FlashCrc = 0x725e772a;
#endif



//----------------------------------
// Ramtest
//-----------------------------------

#ifndef DEBUG_DISABLE_RAMTEST
	extern uint8_t _startRam[];
	extern uint8_t _startRamVars[];


// Ram Variablen
//-----------------------------------------------------------
#define RAM_START ((uint32_t *)_startRam)
#define RAM_END ((uint32_t*)(_startRamVars))

// .ramvar Section wird nicht von Ramtest überprüft
uint32_t *__attribute__((section(".ramvars"))) pui32RamCell;
uint32_t *__attribute__((section(".ramvars"))) pui32BaseRamCell;
uint32_t __attribute__((section(".ramvars"))) ui32RamCellBackup;
uint32_t __attribute__((section(".ramvars"))) ui32RamCellBackupInverted;
uint32_t __attribute__((section(".ramvars"))) ui32RamCell2ndBackup;
uint32_t __attribute__((section(".ramvars"))) ui32RamCell2ndBackupInverted;
uint32_t __attribute__((section(".ramvars"))) ui32PatternSelector;
uint32_t __attribute__((section(".ramvars"))) ui32PatternIndex = 0;

#define PATTERNSIZE 64
const uint32_t carrui32RamPattern[PATTERNSIZE] = { 0x00000001, 0x00000002, 0x00000004, 0x00000008, 0x00000010, 0x00000020, 0x00000040, 0x00000080, 0x00000100, 0x00000200, 0x00000400, 0x00000800, 0x00001000, 0x00002000, 0x00004000, 0x00008000, 0x00010000, 0x00020000, 0x00040000, 0x00080000,
		0x00100000, 0x00200000, 0x00400000, 0x00800000, 0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000, 0x20000000, 0x40000000, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFB, 0xFFFFFFF7, 0xFFFFFFEF, 0xFFFFFFDF, 0xFFFFFFBF, 0xFFFFFF7F, 0xFFFFFEFF, 0xFFFFFDFF, 0xFFFFFBFF, 0xFFFF7FF,
		0xFFFFEFFF, 0xFFFFDFFF, 0xFFFFBFFF, 0xFFFF7FFF, 0xFFFEFFFF, 0xFFFDFFFF, 0xFFFBFFFF, 0xFFF7FFFF, 0xFFEFFFFF, 0xFFDFFFFF, 0xFFBFFFFF, 0xFF7FFFFF, 0xFEFFFFFF, 0xFDFFFFFF, 0xFBFFFFFF, 0xF7FFFFF, 0xEFFFFFFF, 0xDFFFFFFF, 0xBFFFFFFF, 0x7FFFFFFF };

const uint32_t carrui32RamInitPattern[PATTERNSIZE] = { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
		0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
#endif


/*-------------------------------------------------------------------------------------------------
 *    Description: Überprüfung des flüchtigen Speichers indem ein Muster reingeschrieben wird  /SR0025
 *    				Biespiel: Durchlaufzeit zusammen mit RomTest 8 us
 * 							  0x1000/4*0x1000/4*64*10 ms =     ugenfähr 7,7 Tage
 *    Frequency:  10ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vRamCheckContinous(uint8_t *pui8CheckinCounter) {

	++(*pui8CheckinCounter);

#ifndef DEBUG_DISABLE_RAMTEST
	__disable_irq();
	// äußere Schleife, Überlauf von 0x1000 bis < 0x3000
	if (pui32RamCell < RAM_END) {
		if (pui32BaseRamCell != pui32RamCell) {

			// innere Schleife
			if (ui32PatternIndex < PATTERNSIZE) { 				//	for(ui32patternIndex=0; ui32patternIndex <32; ui32patternIndex ++)	// Bit Schleife
				ui32RamCellBackup = *pui32BaseRamCell;
				ui32RamCellBackupInverted = ~(*pui32BaseRamCell);

				ui32RamCell2ndBackup = *pui32RamCell;
				ui32RamCell2ndBackupInverted = ~(*pui32RamCell);

				*pui32BaseRamCell = carrui32RamInitPattern[ui32PatternIndex];
				*pui32RamCell = carrui32RamPattern[ui32PatternIndex];

				// Test written
				if ((*pui32BaseRamCell != carrui32RamInitPattern[ui32PatternIndex]) || (*pui32RamCell != carrui32RamPattern[ui32PatternIndex])) {
					xgHardwareErrors.ui8ErrorRamTest = TRUE;
					vHardwareExecuteErrorLoop(ERROR_HARDWARE_RAMTEST_RUN);
				}

				// Test read
				if (ui32RamCellBackup == (~ui32RamCellBackupInverted) && (ui32RamCell2ndBackup == (~ui32RamCell2ndBackupInverted))) {
					*pui32BaseRamCell = ui32RamCellBackup;
					*pui32RamCell = ui32RamCell2ndBackup;
				} else {
					xgHardwareErrors.ui8ErrorRamTest = TRUE;
					vHardwareExecuteErrorLoop(ERROR_HARDWARE_RAMTEST_RUN);
				}

				ui32PatternIndex++;
			} else {

				ui32PatternIndex = 0;
				if (pui32BaseRamCell < (RAM_END - 1)) {
					pui32BaseRamCell++;
				} else {
					pui32BaseRamCell = RAM_START;
				}
			}
		} else {
			++pui32RamCell;

			// Weiter inkrementieren muss 32 bit Abstand vorhanden sein
			if (pui32RamCell < (RAM_END - 1)) {
				pui32BaseRamCell = pui32RamCell;
				++pui32BaseRamCell;
			} else {
				pui32BaseRamCell = RAM_START;
			}
		}
	} else {
		pui32RamCell = RAM_START;
		pui32BaseRamCell = RAM_START + 1;
		//break;
	}
	__enable_irq();
#endif
}


/*-------------------------------------------------------------------------------------------------
 *    Description: Überprüfung der CRC des FLASHES zur Laufzeit o.Überprüfung des /SR0024
 *    			   festen Speichers indem ein die Kopien überprüft werden
 *    			   (Original und Kopie müssen auf 4 bytes ausgerichtet sein)
 *    Frequency:  10ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vRomCheckContinous(uint8_t *pui8CheckinCounter) {
	++(*pui8CheckinCounter);

#if ROMTEST_TYPE == ROMTEST_TYPE_COPY
if (pui32OriginalAddress != COPY_FLASH) {
	if (*pui32OriginalAddress != *pui32RomCopyAddress) {
		xgHardwareErrors.ui8ErrorRomTest = TRUE;
		vHardwareExecuteErrorLoop(ERROR_HARDWARE_ROMTEST_RUN);
	} else {
		++pui32OriginalAddress;
		++pui32RomCopyAddress;
	}
} else {
	pui32OriginalAddress = ORIGINAL_FLASH;
	pui32RomCopyAddress = COPY_FLASH;
}

#elif ROMTEST_TYPE == ROMTEST_TYPE_CRC


	// 256 executions per run 256 * 32-bit = 1024 bytes per run. Approx 32 execs required in total, 3 seconds
	uint8_t ui8Index = 0xFF;
	while(--ui8Index){

		if(pui32OriginalAddress < END_FLASH){
			CRC->DR = *((uint16_t*) (pui32OriginalAddress));
			++pui32OriginalAddress;
		} else{

			// Compare calculated Crc to the saved
			if (CRC->DR != __ui32FlashCrc) {
				vHardwareExecuteErrorLoop(ERROR_HARDWARE_ROMTEST_RUN);
			}

			CRC->CR = CRC_CR_RESET;
			CRC->INIT = 0x0000;
			pui32OriginalAddress = ORIGINAL_FLASH;

		}
	}
#endif

}


/*-------------------------------------------------------------------------------------------------
 *    Description:Überprüfung des flüchtigen Speichers indem ein Muster reingeschrieben wird /SR0025
 *    Frequency:  during initialization
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vRamInitialize(void) {

#ifndef DEBUG_DISABLE_RAMTEST
	uint16_t i = 0;
	pui32RamCell = RAM_START;
	while (pui32RamCell < RAM_END) {

		ui32RamCellBackup = *pui32RamCell;
		ui32RamCellBackupInverted = ~(*pui32RamCell);

		for (i = 0; i < PATTERNSIZE; i++) {
			*pui32RamCell = carrui32RamPattern[i];

			// Fehlereinbau: RAM-Test
			//if(pui32RamCell == (RAM_START+((RAM_END-RAM_START)>>1))){
			//	*pui32RamCell=0xFF;
			//}

			if (*pui32RamCell != carrui32RamPattern[i]) {
				xgHardwareErrors.ui8ErrorRamTest = TRUE;
				vHardwareExecuteErrorLoop(ERROR_HARDWARE_RAMTEST_INIT);
			}
		}
		if (ui32RamCellBackup == (~ui32RamCellBackupInverted)) {
			*pui32RamCell = ui32RamCellBackup;
		} else {
			xgHardwareErrors.ui8ErrorRamTest = TRUE;
			vHardwareExecuteErrorLoop(ERROR_HARDWARE_RAMTEST_INIT);
		}

		++pui32RamCell;
	}
	pui32RamCell = RAM_START;
	pui32BaseRamCell = RAM_START + 1;
#endif
}


/*-------------------------------------------------------------------------------------------------
 *    Description: Überprüfung der CRC zur Initialisierungszeit oder der Kopie /SR0024
 *    Frequency:  during initialization
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vRomInitialize(void) {

#if ROMTEST_TYPE == ROMTEST_TYPE_COPY
	while (1) {
		if (pui32OriginalAddress < (COPY_FLASH)) {
			if (*pui32OriginalAddress != *pui32RomCopyAddress) {
				xgHardwareErrors.ui8ErrorRomTest = TRUE;
				vHardwareExecuteErrorLoop(ERROR_HARDWARE_ROMTEST_INIT);
			} else {
				++pui32OriginalAddress;
				++pui32RomCopyAddress;
			}
		} else {
			pui32OriginalAddress = ORIGINAL_FLASH;
			pui32RomCopyAddress = COPY_FLASH;
			break;
		}
	}

#elif ROMTEST_TYPE == ROMTEST_TYPE_CRC
	uint32_t ui32CrcCalculated;
	CRC->CR = CRC_CR_RESET;
	CRC->INIT = 0x0000;
	pui32OriginalAddress = ORIGINAL_FLASH;

	// Calculate over the first half
	while (pui32OriginalAddress < (END_FLASH)) {
		CRC->DR = *((uint32_t*) (pui32OriginalAddress));
		 ++pui32OriginalAddress;
	}

	ui32CrcCalculated = CRC->DR;

	// Compare
	if (ui32CrcCalculated != (__ui32FlashCrc)) {
		vHardwareExecuteErrorLoop(ERROR_HARDWARE_ROMTEST_INIT);
	}

#endif
}
