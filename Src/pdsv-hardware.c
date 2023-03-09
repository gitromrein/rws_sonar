/*
 * File:   pdsv-hardware.c
 * Author: am
 * 10.01.2021
 */

#include "pdsv-hardware.h"
#include "pdsv-parameters.h"
#include "pdsv-voltage.h"
#include "pdsv-fdcan.h"
#include "pdsv-diagnostics.h"
#include "pdsv-ipk.h"
#include "pdsv-memorytest.h"
#include "pdsv-diagnostics.h"
#include "scheduler.h"

// #define ADDRESS 0x1FFF7800
// const uint32_t __attribute__((section(".flashoptr"))) UI32FLASH_SET_LOCKON = 0xFFFFFF00;// 0xFF01FF00;
// INFO! Do not forget to set the following option bytes: BOOT_LOCK = 1  to disable the boot pin PB8

// FIT 100
#define ERROR_MAIN_COUNTER 5000		// ~5-10 ms
// #define ERROR_MAIN_COUNTER 100
// FIT 1
// #define ERROR_TIMER_COUNTER 1
#define ERROR_TIMER_COUNTER 50		// 50 ms

volatile uint8_t ui8gHardwareTimerEvent = FALSE;			// 1ms
volatile uint8_t ui8gHardwareUartSwapRequested = FALSE;		// About each 20ms

// Macros
#define mWaitFor(condition) while(condition){}

/**-------------------------------------------------------------------------------------------------
 *    Description: Dient zur Aufdeckung vom hängendem mainaus dem Timer interrupt /SR0026
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
inline void vHardwareCheckTimerMonitor(void) {
	// Fehler Timer Timeout
	++xgErrorCounters.ui16ErrorCounterTimer;
	xgErrorCounters.ui16ErrorCounterMain = 0;

	if (xgErrorCounters.ui16ErrorCounterTimer >= ERROR_TIMER_COUNTER) {
		xgHardwareErrors.ui8ErrorTimerTest = TRUE;
		vHardwareExecuteErrorLoop(ERROR_HARDWARE_TIMER);
	}
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Dient zur Aufdeckung vom hängendem Interrupt, aus dem main /SR0027
 *    Frequency:  7us - ~1ms
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
inline void vHardwareCheckMainMonitor(void) {
	// Gegenseitiger check
	NVIC_DisableIRQ(TIM1_UP_TIM16_IRQn);
	++xgErrorCounters.ui16ErrorCounterMain;
	xgErrorCounters.ui16ErrorCounterTimer = 0;
	NVIC_EnableIRQ(TIM1_UP_TIM16_IRQn);

	// Fehler Main
	if (xgErrorCounters.ui16ErrorCounterMain >= ERROR_MAIN_COUNTER) {
		xgHardwareErrors.ui8ErrorMainTest = TRUE;
		vHardwareExecuteErrorLoop(ERROR_HARDWARE_MAIN);
	}

	// Feed the Watchdog
#ifndef DEBUG_DISABLE_WATCHDOG
	IWDG->KR = 0x0000AAAA;
#endif

}

/**-------------------------------------------------------------------------------------------------
 *    Description:  Überprüft die initialen Register flags /SR0034
 *    Frequency:  once
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
inline void vHardwareCheckCaptureUnit(uint32_t ui32MeasuredDifference) {
	if (ui32MeasuredDifference == 0) {
		++xgErrorCounters.ui8ErrorCounterCaptureDefect;
		if (xgErrorCounters.ui8ErrorCounterCaptureDefect >= 5) {
			xgHardwareErrors.ui8ErrorCaptureDefect = TRUE;
			vHardwareExecuteErrorLoop(ERROR_HARDWARE_CAPTURE);
		}
	} else {
		xgRuntimeState.xCaptured.ui32InterruptCapturedFrequency = CAPTURE_SAMPLE_FREQUENCY / ui32MeasuredDifference;
		if (xgErrorCounters.ui8ErrorCounterCaptureDefect > 0)
			--xgErrorCounters.ui8ErrorCounterCaptureDefect;
	}
}

/**-------------------------------------------------------------------------------------------------
 *    Description:  Überprüft die initialen Register flags SR0030
 *    Frequency:  on interrupt
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
inline void vHardwareCheckInitialFlags(uint32_t ui32Csr) {
#ifdef DEBUG
	uint32_t ui32Optr = FLASH->OPTR;
	uint8_t ui8Watchdog = 0;

	(void) ui32Optr;
	(void) ui8Watchdog;
#endif

	// Catch last reset flags. From most-significant to least-significant
	//-----------------------------------------------------------------
	// Low Power Reset  Brownout	SR0031
	if (ui32Csr & (RCC_CSR_LPWRRSTF)) {
		vHardwareExecuteErrorLoop(ERROR_HARDWARE_BROWNOUT);
	}

	// Window-Watchdog Reset. This is not used
	if (ui32Csr & RCC_CSR_WWDGRSTF) {
		vHardwareExecuteErrorLoop(ERROR_HARDWARE_WATCHDOG);
	}

	// Independent Watchdog Reset	SR0032
	if (ui32Csr & RCC_CSR_IWDGRSTF) {
		DEBUGCODE(ui8Watchdog = 1;)
		//vExecuteHardwareErrorLoop(ERROR_HARDWARE_WATCHDOG);
	}

	// Software Reset Flag			SR0034
	if (ui32Csr & RCC_CSR_SFTRSTF) {
		//vExecuteHardwareErrorLoop(ERROR_HARDWARE_UNEXPECTED_RESET);
	}

	// Brownout Reset Brownout		SR0031
	if (ui32Csr & RCC_CSR_BORRSTF) {
		//vExecuteHardwareErrorLoop(ERROR_HARDWARE_BROWNOUT);
	}

	// NRST	Reset
	if (ui32Csr & RCC_CSR_PINRSTF) {

	}

	// Reset during Option Byte loading
	if (ui32Csr & RCC_CSR_OBLRSTF) {

	}

}

/**-------------------------------------------------------------------------------------------------
 *    Description:  Überprüft die initialen Register flags SR0031 - SR0032
 *    Frequency:  once
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
inline void vHardwareCheckUnexpectedInterrupt(void) {
	vHardwareExecuteErrorLoop(ERROR_HARDWARE_INTERRUPT);
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Wird bei bestimmten Fehlern aufgerufen. Hier springt der Prozessor nicht mehr raus
 *    			   Dient zur Implementierung der sicheren Abschaltung im Hardware-Fehler-Fall
 *    Frequency:  on error
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vHardwareExecuteErrorLoop(uint8_t ui8HardError) {
	uint16_t i = 0;

	__disable_irq();
	while (1) {

		i = 0;
		USART1->TDR = ui8HardError;
		while (i <= 0xFFFFFF) {
			++i;

#ifndef DEBUG_DISABLE_WATCHDOG
			IWDG->KR = 0x0000AAAA;
#endif
		}
	}
}

/**-------------------------------------------------------------------------------------------------
 *    Description:  Initialize Hardware Components
 *    Frequency:  once during initialization
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vHardwareInitialize(void) {
	uint8_t ui8ModuleSlot = 0;
	uint32_t ui32Csr = 0;

	ui32Csr = RCC->CSR;			// Last reset status
	RCC->CSR = RCC_CSR_RMVF;

	//SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET; /* Vector Table Relocation in Internal FLASH */

	//------------------------------------------------------------
	// Flash
	//------------------------------------------------------------

	FLASH->ACR |= FLASH_ACR_PRFTEN;  			//Prefetch is enabled.
	FLASH->ACR |= FLASH_ACR_ICEN;    			//Instruction cache is enabled
	FLASH->ACR |= FLASH_ACR_DCEN;    			//Data cache is enabled
	FLASH->ACR |= (3 << FLASH_ACR_LATENCY_Pos); //3 Wait States
	//FLASH->OPTR|= FLASH_OPTR_nBOOT0
	// Flash->ACR  = ICRST = 1;

	//---------------------------------------------
	// PLL and Buses
	//---------------------------------------------
	RCC->CR |= RCC_CR_HSEON;

	// Warten bis HSE bereit ist
	mWaitFor(!(RCC->CR & RCC_CR_HSERDY));

	RCC->CFGR = 0;
	RCC->CFGR |= (uint16_t) (0 << 4);   // AHB presc. 1, 100MHz
	RCC->CFGR |= (uint16_t) (5 << 8);   // APB1 presc. 4, 25MHz // Peripheral Clock 1: Wichtig für CAN
	RCC->CFGR |= (uint16_t) (0 << 11);  //(4 << 11);  // APB2 presc. 2, 50MHz	 // Peripheral Clock 2: Wichtig für UART
	RCC->CFGR |= (uint16_t) (10 << 16); // RTCPRE = 10, 1MHz RTC		??
	RCC->CFGR |= (uint16_t) (5 << 24);  // MCOSEL: PLLR Clock
	RCC->CFGR |= (uint16_t) (4 << 28);  // MCO NO Prescaling

	// PLL Formula: PLLR wird durchgeschaltet
	// f(VCO clock) = f(PLL clock input) × (PLLN / PLLM)
	// f(PLL_P) = f(VCO clock) / PLLP        Max 170 Mhz ADC, But ADC could work off SYSCLK
	// f(PLL_Q) = f(VCO clock) / PLLQ		Max 170 Mhz Quad Spi and USB
	// f(PLL_R) = f(VCO clock) / PLLR		Max 170 Mhz

	// VCO Frequency is maxed out at 344
	RCC->PLLCFGR = 0;
	RCC->PLLCFGR |= (uint16_t) (3 << 0);    // PLLSRC HSE
	RCC->PLLCFGR |= (uint16_t) (4 << 4);    // PLLM = Div 5
	RCC->PLLCFGR |= (uint16_t) (100 << 8);  // PLLN = Faktor 100
	RCC->PLLCFGR |= (uint16_t) (0 << 25);   // PLLR = Div 2

	// Enable p, q and r clocks if needed
	RCC->PLLCFGR |= RCC_PLLCFGR_PLLREN;		// Enable the R clock
	RCC->CR |= RCC_CR_PLLON;

	// Warten bis Pll Clock ist bereit
	mWaitFor(!(RCC->CR & RCC_CR_PLLRDY));

	RCC->CFGR |= (3 << 0);  // 3=PLL select as system clock

	// Warten bis PLL = system clock
	mWaitFor((RCC->CFGR & (RCC_CFGR_SWS)) != (3 << 2));

	// Jetzt werden Befehle mit 100 Mhz abgearbeitet

	//----------------------------------------------------------------------------
	// Component Clocks
	//-----------------------------------------------------------------------------
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;		// Clocks
	//RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;		// Power Configuration, not needed unless charging batteries or using RTC

	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;		// Port b
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;		// Port a

	RCC->CCIPR = 0; 							// PCLK 50 Mhz for Uart
	RCC->CCIPR |= RCC_CCIPR_FDCANSEL_1;			// PCLK 25 Mhz for Can
	RCC->CCIPR |= RCC_CCIPR_ADC12SEL_1;			// System Clock as Input clock for ADC12

	// SYSCFG->CMPCR |= SYSCFG_CMPCR_CMP_PD;	 //Compensation cell eingeschaltet, für besseren Slew bei >= 50Hz.
	// Man könnte Gain compensation für ADC einstellen über GCOMPCOEFF, die gemessenen Spannung zu viel abweicht!
	// Man könnte auch OPAMP mit Verstärkung 1 verwenden, aber ist nicht notwendig!

	//-----------------------------------------------
	// DEBUG Pin: output, fast
	//-----------------------------------------------
	mDebugConfigurePin
	;

	//-----------------------------------------------
	// Direction Pin: input, pullup, fast
	//-----------------------------------------------
	GPIOA->MODER &= (~GPIO_MODER_MODER1);
	GPIOA->PUPDR |= (GPIO_PUPDR_PUPDR1);
	GPIOA->OSPEEDR |= (GPIO_OSPEEDR_OSPEED1);

	//-----------------------------------------------
	// Hysteresis switching pins: output, fast
	//-----------------------------------------------
	GPIOB->MODER &= (~GPIO_MODER_MODER10);  		// Hyst 1
	GPIOB->MODER |= GPIO_MODER_MODER10_0;
	GPIOB->OTYPER &= (~GPIO_OTYPER_OT10);
	GPIOB->OSPEEDR |= (GPIO_OSPEEDR_OSPEED10);

	GPIOB->MODER &= (~GPIO_MODER_MODER12);  		// Hyst 2
	GPIOB->MODER |= GPIO_MODER_MODER12_0;
	GPIOB->OTYPER &= (~GPIO_OTYPER_OT12);
	GPIOB->OSPEEDR |= (GPIO_OSPEEDR_OSPEED12);

	//-----------------------------------------------
	// Cap. Coup: output, fast
	//-----------------------------------------------
	GPIOB->MODER &= (~GPIO_MODER_MODER13);  		// Cap. Coup
	GPIOB->MODER |= GPIO_MODER_MODER13_0;
	GPIOB->OTYPER &= (~GPIO_OTYPER_OT13);
	GPIOB->OSPEEDR |= (GPIO_OSPEEDR_OSPEED13);

	//-----------------------------------------------
	// EMK gain: output, fast
	//-----------------------------------------------
	GPIOB->MODER &= (~GPIO_MODER_MODER14);  		// EMK gain
	GPIOB->MODER |= GPIO_MODER_MODER14_0;
	GPIOB->OTYPER &= (~GPIO_OTYPER_OT14);
	GPIOB->OSPEEDR |= (GPIO_OSPEEDR_OSPEED14);

	//-----------------------------------------------
	// MCO DEBUG Pin: output, fast
	//------------------------------------
	//GPIOA->MODER &= (~GPIO_MODER_MODER8);		// Clear the settings for A8
	//GPIOA->MODER |= GPIO_MODER_MODER8_1;		// Alternative
	//GPIOA->OTYPER &= (~GPIO_OTYPER_OT8);		// Pushpull
	//GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED8;	    // Maximum geschwindigkeit
	//GPIOA->AFR[1] &= (~GPIO_AFRH_AFSEL8);       // Alternate Function MCO

	//------------------------------------
	// I1 - I4 Inputs
	//------------------------------------
	// Eingänge, keine pull-up oder  pull-down, niedrige Geschwindigkeit
	mHardwareSetStateInputI1;
	GPIOB->PUPDR &= (~GPIO_PUPDR_PUPDR6);
	GPIOB->OSPEEDR &= (~GPIO_OSPEEDR_OSPEED6);

	mHardwareSetStateInputI2;
	GPIOB->PUPDR &= (~GPIO_PUPDR_PUPDR7);
	GPIOB->OSPEEDR &= (~GPIO_OSPEEDR_OSPEED7);

	mHardwareSetStateInputI3;
	GPIOB->PUPDR &= (~GPIO_PUPDR_PUPDR8);
	GPIOB->OSPEEDR &= (~GPIO_OSPEEDR_OSPEED8);

	mHardwareSetStateInputI4;
	GPIOB->PUPDR &= (~GPIO_PUPDR_PUPDR9);
	GPIOB->OSPEEDR &= (~GPIO_OSPEEDR_OSPEED9);

	//------------------------------------
	// Input test pin: output, fast
	//------------------------------------
	GPIOB->MODER &= (~GPIO_MODER_MODER15);  		// Input testing
	GPIOB->MODER |= GPIO_MODER_MODER15_0;
	GPIOB->OTYPER &= (~GPIO_OTYPER_OT15);
	GPIOB->OSPEEDR |= (GPIO_OSPEEDR_OSPEED15);

	//------------------------------------
	// Wire breakage: input, fast
	//------------------------------------
	GPIOB->MODER &= (~GPIO_MODER_MODER5); // DB UW for Detection of Wirebreakage			B5				Digital Input
	GPIOB->OTYPER &= (~GPIO_OTYPER_OT5);
	GPIOB->OSPEEDR |= (GPIO_OSPEEDR_OSPEED5);

	//---------------------------------------------------------------
	// USART
	//---------------------------------------------------------------
	GPIOA->MODER &= (~GPIO_MODER_MODER9);
	GPIOA->MODER |= GPIO_MODER_MODER9_1;		//alternative
	GPIOA->OTYPER &= (~GPIO_MODER_MODER9);		//pushpull
	GPIOA->OSPEEDR |= GPIO_MODER_MODER9_0;	//mittlere Geschwindigkeit
	GPIOA->AFR[1] &= (~GPIO_AFRH_AFSEL9);
	GPIOA->AFR[1] |= GPIO_AFRH_AFSEL9_0 | GPIO_AFRH_AFSEL9_1 | GPIO_AFRH_AFSEL9_2;     //USART1 TX

	GPIOA->MODER &= (~GPIO_MODER_MODER10);
	GPIOA->MODER |= GPIO_MODER_MODER10_1;		//alternative
	GPIOA->OTYPER &= (~GPIO_OTYPER_OT10);		//pushpull
	GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED10_0;	//mittlere Geschwindigkeit
	GPIOA->AFR[1] &= (~GPIO_AFRH_AFSEL10);
	GPIOA->AFR[1] |= GPIO_AFRH_AFSEL10_0 | GPIO_AFRH_AFSEL10_1 | GPIO_AFRH_AFSEL10_2;     //USART1 RX

	RCC->APB2ENR |= RCC_APB2ENR_USART1EN;		//Clock einschalten
	USART1->CR1 &= (~USART_CR1_M);
	USART1->CR1 |= (USART_CR1_M0);				//9 bit modus
	USART1->CR1 |= USART_CR1_UE;				//UART3 on
	USART1->CR1 &= (~USART_CR1_OVER8);			//16 fach oversampling
	USART1->CR1 |= USART_CR1_RXNEIE;			//IR enabled whenever ORE=1 or RXNE=1 in the USART->SR register
	USART1->CR2 &= (~USART_CR2_STOP);   		//1 Stop bit
	USART1->CR3 &= (~USART_CR3_ONEBIT);			//3 samples per bit
	USART1->CR3 &= (~USART_CR3_DMAR);			//DMA receive disabled
	USART1->CR3 &= (~USART_CR3_DMAR);			//DMA transmit disabled
	USART1->TDR = 0;
	USART1->ICR = 0xFFFFFFFF;

	USART1->BRR = 0x30C;			//0x186;				// Calculation of BAUD(128k): 50000000/128000

	//Status und Daten lesen, dann sind error und RXNE bits gelöscht
	USART1->CR1 |= USART_CR1_TE;     //Transmit freigeschaltet
	USART1->CR1 |= USART_CR1_RE;     //Receive freigeschaltet

	// SR0031-33
	vHardwareCheckInitialFlags(ui32Csr);

	//-----------------------------------------------
	// Warning! Should be initialized before ROM-Test. CRC Initialization for a of ROM-Test.
	//-----------------------------------------------
	RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;
	CRC->INIT = 0x00000000;
	CRC->CR = 0x01;			// Reset to  32-Bit Polynom	 0x04C1 1DB7

	//-----------------------------------------------------------------
	// Self Validation before continuing
	//-----------------------------------------------------------------
	vRamInitialize();
	vRomInitialize();

#ifdef DEBUG_SKIP_SLOT_EXCHANGE
	ui8ModuleSlot = 1;
#else
	//------------------------------------------------------------
	// Get module Slot from PIC, and acknowledge it
	//------------------------------------------------------------
	/**/
	uint8_t ui8UartData = 0x00;
	uint8_t ui8DebugUartStatus = 0x00;
	uint8_t ui8ErrorCount = 0x00;
	do {
		mWaitFor(!(USART1->ISR & USART_ISR_RXNE));
		//Auf RX warten

		ui8DebugUartStatus = USART1->ISR;	//Steckplatz lesen aus SR und DR register, dadurch wird RXNE gelöscht
		ui8ModuleSlot = USART1->RDR;

		USART1->TDR = 0x100 | ui8ModuleSlot;	//Steckplatz zurücksenden
		mWaitFor(!(USART1->ISR & USART_ISR_RXNE));	//wait for acknowledge 0xAA

		// lesen aus SR und DR register, dadurch wird RXNE gelöscht
		ui8DebugUartStatus = USART1->ISR;
		(void) ui8DebugUartStatus;

		ui8UartData = USART1->RDR;

		++ui8ErrorCount;
	} while (ui8UartData != 0xAA);
#endif

	USART1->ICR = 0xFFFFFFFF;				// Reset UART status flags
	USART1->TDR = 0xAA;						// 0xAA senden und somit bestätigen

	//------------------------------------------------------------
	// Zeitgeber und Testimpulsgenerierung für Drahtbruch/Eingänge
	//-------------------------------------------------------------
	RCC->APB2ENR |= RCC_APB2ENR_TIM1EN; // APB2 = 100MHz
	TIM1->PSC = 99;						// Prescaler=PSC+1
	TIM1->CR1 &= (~TIM_CR1_DIR);		// Count up
	TIM1->ARR = 1000;					// Counter*1/(100M)/(Prescaler) = 1ms
	TIM1->CNT = 0;
	TIM1->SR &= (~TIM_SR_UIF);			// update IR flag
	TIM1->DIER |= TIM_DIER_UIE;		    // update IR enable bit

	//-----------------------------------------------------------------------------
	// FDCAN
	//----------------------------------------------------------------------------
	// Other Options for testing FDCAN
	/* Set FDCAN Operating Mode:
	 | Normal | Restricted |    Bus     | Internal | External
	 |        | Operation  | Monitoring | LoopBack | LoopBack
	 CCCR.TEST |   0    |     0      |     0      |    1     |    1
	 CCCR.MON  |   0    |     0      |     1      |    1     |    0
	 TEST.LBCK |   0    |     0      |     0      |    1     |    1
	 CCCR.ASM  |   0    |     1      |     0      |    0     |    0
	 */

	// FDCAN1 RX
	GPIOA->MODER &= (~GPIO_MODER_MODER11);
	GPIOA->MODER |= GPIO_MODER_MODER11_1;		//alternative
	GPIOA->OTYPER &= (~GPIO_OTYPER_OT11);		//pushpull
	GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED11;
	GPIOA->AFR[1] |= GPIO_AFRH_AFSEL11_3 | GPIO_AFRH_AFSEL11_0;

	// FDCAN1 TX
	GPIOA->MODER &= (~GPIO_MODER_MODER12);
	GPIOA->MODER |= GPIO_MODER_MODER12_1;		//alternative
	GPIOA->OTYPER &= (~GPIO_OTYPER_OT12);		//pushpull
	GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED12;
	GPIOA->AFR[1] |= GPIO_AFRH_AFSEL12_3 | GPIO_AFRH_AFSEL12_0;

	RCC->APB1ENR1 |= RCC_APB1ENR1_FDCANEN;		// Clock einschalten

	// Configuration
	FDCAN1->CCCR = 0x0000 | FDCAN_CCCR_INIT;	 // Set init
												 // Enable Automatic retransmission
												 // disable monitoring mode, disable powerdown capabilities
												 // disable edge filtering, In Erra Edge Filter could cause faulty frames
												 // Enable standard can ISO11898-1, is it okay?

	// Wait for initialization bits to become set
	mWaitFor(!(FDCAN1->CCCR & (FDCAN_CCCR_INIT)));

	FDCAN1->TOCC &= (~FDCAN_TOCC_ETOC); 		// Timeout feature, when there is no communication. Disable it for now
	FDCAN1->CCCR |= FDCAN_CCCR_CCE | FDCAN_CCCR_TXP; // Enable writing to protected configuration registers while in init, also resets the configuration registers, Exception handling turned off. Pause on
													 // BAR and BCR work when CCE is not set

	FDCAN_CONFIG->CKDIV = 0;			// Clock APB1 Clok Prescaler 1

	// Configure Time quanta
	// 25MHz / 125kB = 200 = AnzahlTQ * (BPR+1)
	// 200/25=8  (25 TQs, BPR+1=8+1)	25 Mhz
	// Synchronisierbit entspricht      1 TQ

	// Nominal Timing for Standard CAN
	FDCAN1->NBTP = 0;
	FDCAN1->NBTP |= (7 << FDCAN_DBTP_DBRP_Pos);
	FDCAN1->NBTP |= (15 << FDCAN_NBTP_NTSEG1_Pos);
	FDCAN1->NBTP |= (7 << FDCAN_NBTP_NTSEG2_Pos);
	FDCAN1->NBTP |= 1 << FDCAN_NBTP_NSJW_Pos; 		 // Synchronization Jump: Extends DSEG1 Phase up to 1 tq's

	// Data Timing for FDCAN
	FDCAN1->DBTP = 0;
	FDCAN1->DBTP |= (7 << FDCAN_DBTP_DBRP_Pos);
	FDCAN1->DBTP |= (15 << FDCAN_DBTP_DTSEG1_Pos);
	FDCAN1->DBTP |= (7 << FDCAN_DBTP_DTSEG2_Pos);
	FDCAN1->DBTP |= 1 << FDCAN_DBTP_DSJW_Pos;  		 // Synchronization Jump: Extends DSEG1 Phase up to 1 tq's

	// Filter
	uint32_t ui32Identifier = ((uint16_t) (ui8ModuleSlot)) << 3;		// Identifier 0	 when moduleslot 1 address 8, Identifier = moduleslot (bit11-8)

	memset((uint8_t*) pxgFdcanMessageRam, 0, sizeof(FdcanMessageRam_t));		// Clear CAN Message Ram

	pxgFdcanMessageRam->ui32Filters[0] = (0x02 << 30) | (0x01 << 27) | (ui32Identifier << 16) | (0x07FF);		// FIFO 0 only 8
	pxgFdcanMessageRam->ui32Filters[1] = (0x00 << 30) | (0x02 << 27) | ((ui32Identifier + 1) << 16) | ((ui32Identifier + 4));		// FIFO 1 Range 9 - C

	// Number of Filters
	FDCAN1->RXGFC = 2 << FDCAN_RXGFC_LSS_Pos;

	// Use FIFOs for TX instead of queues
	FDCAN1->TXBC = 0;

	// TX Message Ram Configuration, Address should be set once
	pxgFdcanMessageRam->rxTxFifo[0].ui32Control0 = (0x400L | ui32Identifier) << 18;
	pxgFdcanMessageRam->rxTxFifo[0].ui32Control1 = (8L << 16);		// DLC

	pxgFdcanMessageRam->rxTxFifo[1].ui32Control0 = (0x401L | ui32Identifier) << 18;
	pxgFdcanMessageRam->rxTxFifo[1].ui32Control1 = (8L << 16);		// DLC

	pxgFdcanMessageRam->rxTxFifo[2].ui32Control0 = (0x402L | ui32Identifier) << 18;
	pxgFdcanMessageRam->rxTxFifo[2].ui32Control1 = (8L << 16);		// DLC

	// TDCR.TDCO Configure Transmission Delay Compensation, programmed in quanta
	// PSR.TDCV Status for Transmission Delay Compensation

	// Normal Mode
	// CAN can operate only after it reads 11 high bits(CAN Idle), so can has to be high for the time
	FDCAN1->CCCR &= (~FDCAN_CCCR_INIT);
	FDCAN1->ILE = 0 | FDCAN_ILE_EINT0;											// Enable Interrupt Line0
	FDCAN1->IE = 0 | FDCAN_IE_RF1NE | FDCAN_IE_RF0NE | FDCAN_IE_BOE | FDCAN_IE_EWE;	// New Messages, Bus Off and Errors and Warnings cause an interrupt

	//---------------------------------------------------
	// Frequency Sampler: Reference Manual S. 1306
	//---------------------------------------------------
	GPIOA->MODER &= (~GPIO_MODER_MODER0);		// Primary capturing pin(Master)
	GPIOA->MODER |= GPIO_MODER_MODER0_1;		//
	GPIOA->OTYPER &= (~GPIO_OTYPER_OT0);
	GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED0;
	GPIOA->AFR[0] &= (~GPIO_AFRL_AFSEL0);		//
	GPIOA->AFR[0] |= (GPIO_AFRL_AFSEL0_0);		//

	GPIOA->MODER &= (~GPIO_MODER_MODER1);		// Secondary capturing pin (Slave)

	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;						// Clock used is PCLK1
	TIM2->PSC = 39;												// 25000000 is pre-multiplied by x2 Hardware Prescaler when PCLK1 Prescaler ist not x1, so we get 50 Mhz and need 1,25 Mhz
	TIM2->CR1 = 0;												// No preloading
	TIM2->CR2 = 0;
	TIM2->TISEL = 0;											// Reset Input sources
	TIM2->CCMR1 = 0 | TIM_CCMR1_CC1S_0 | TIM_CCMR1_IC1F_0;		// Rising edge, Capture counter mapped to TIM2_CH2, 2 Events needed to confirm a rising edge, capture done every time
	TIM2->DIER = TIM_DIER_CC1IE;								// Enable Capture interrupt
	TIM2->SR = 0;
	TIM2->CR1 |= TIM_CR1_CEN;									// Enable Timer for measuring time between captures
	TIM2->EGR |= TIM_EGR_UG;
	TIM2->CCER = TIM_CCER_CC1E;									// Capture enable

	//-----------------------------------------------------------
	// Quadraturdecoder. Not needed for this project!
	// ----------------------------------------------------------
	//RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;	// TIM2 Quadratur
	//TIM2->PSC = 0;								//8100 Inkr.= 0,15ms, die Zeit, die gemessen wird
	//TIM2->ARR = 8100;
	//TIM2->CNT = 0;
	//TIM2->SR &= (~TIM_SR_UIF);
	//TIM2->DIER |= TIM_DIER_UIE;		//update IR enable bit
	//TIM7_CR1  = CEN = 1;       	// wird von anderer Funktion(Transistor-Test) freigeschaltet

	//----------------------------------------------------
	// DMA for ADC									 |
	//----------------------------------------------------
	RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMAMUX1EN;			// Enable DMA1 and DMAMUX
	DMA1_Channel1->CPAR = (uint32_t) (&ADC2->DR);						// source

	//WARNING! Destination buffer is dynamic and is set during "TIM1 Interrupt". No need to preset the memory register
	//DMA1_Channel1->CMAR = (uint32_t) rxgAdcData[0].rui32AdcConverted;

	DMA1_Channel1->CNDTR = 5;											// 5 Values
	DMA1_Channel1->CCR = 0 | DMA_CCR_CIRC | DMA_CCR_PL_0 | DMA_CCR_PSIZE_1 | DMA_CCR_MSIZE_1 | DMA_CCR_TCIE | DMA_CCR_MINC;	// Direction from peripheral to memory, Priority Medium, 32 bits, interrupt when transfer completes

	DMA1->IFCR = DMA_IFCR_CTCIF1 | DMA_IFCR_CGIF1;
	DMAMUX1_Channel0->CCR = 0x24;				// Select as DMA requester ADC2

	DMA1_Channel1->CCR |= DMA_CCR_EN;			// Enable DMA
	vAdcInitializeDmaBuffers();					// Initialize DMA Buffers

	//----------------------------------------------------
	// ADC for Voltage Sampling												 |
	//----------------------------------------------------
	/*
	 t_adc = 1/10000000
	 n_wait = 92.5
	 n_channels = 5
	 n_in_cycles = 10

	 t_in_single = (n_wait+ 12.5) * t_adc
	 t_in_cycle = t_in_single * n_channels
	 t_total = n_in_cycles * t_in_cycle
	 print(t_total)
	 */

	RCC->AHB2ENR |= RCC_AHB2ENR_ADC12EN;	// ADC12 clock enable, we need ADC 2

	GPIOB->MODER |= GPIO_MODER_MODER2;		// 12 Volt			B2											ADC2_IN12
	GPIOB->MODER |= GPIO_MODER_MODER11;		// 3v3 Volt			B11											ADC12_IN14
	GPIOA->MODER |= GPIO_MODER_MODER4;		// 1v65 volt		A4											ADC2_IN17
	GPIOA->MODER |= GPIO_MODER_MODER6;		// L1 UW for Plausibility and Wire breakage		A6				ADC2_IN3
	GPIOA->MODER |= GPIO_MODER_MODER7;		// L1 UW EMK		A7											ADC2_IN4

	// ADC Clock prescaler. 100MHz / x = 10MHz
	ADC12_COMMON->CCR = ADC_CCR_PRESC_2 | ADC_CCR_PRESC_0;
	ADC2->CR = 0 | ADC_CR_ADVREGEN;

	ADC2->CFGR = ADC_CFGR_DMAEN | (ADC_CFGR_RES_0);	// Need  scan mode conversion, 10- bit resolution
	ADC2->SQR1 = (4 << 0);				// 5 Conversions
	ADC2->SQR1 |= (3 << 6);				// PA6: L1 UW, 	   ADC2_IN3
	ADC2->SQR1 |= (12 << 12);			// PB2: 12 V,      ADC2_IN12
	ADC2->SQR1 |= (14 << 18);			// PB11: 3.3 V,    ADC12_IN14
	ADC2->SQR1 |= (17 << 24);			// PA4: 1.65V,     ADC2_IN17
	ADC2->SQR2 = (4 << 0);			    // PA7: L1 UW EMK, ADC2_IN4

	// Configure the sampling time for the inputs 3 12 14 and 17
	ADC2->SMPR1 = (ADC_SMPR1_SMP4_2 | ADC_SMPR1_SMP4_0);	// L1
	ADC2->SMPR1 |= (ADC_SMPR1_SMP3_2 | ADC_SMPR1_SMP3_0);	// 12V
	ADC2->SMPR2 = (ADC_SMPR2_SMP12_2 | ADC_SMPR2_SMP12_0);	// 3.3V
	ADC2->SMPR2 |= (ADC_SMPR2_SMP14_2 | ADC_SMPR2_SMP14_0);	// 1.65V
	ADC2->SMPR2 |= (ADC_SMPR2_SMP17_2 | ADC_SMPR2_SMP17_0);	// EMK

	//ADC2->IER = 0| ADC_IER_EOCIE;										// ADC Interrupt, not used because dma does the transfer
	//ADC_ISR_EOSIE; END of conversion of a sequence //ADC_ISR_EOSMP; 	// END of sampling

	ADC2->CR |= ADC_CR_ADEN;								// Enable ADC
	mWaitFor(!(ADC2->ISR & ADC_ISR_ADRDY))
	//ADC2->ISR = ADC_ISR_ADRDY;							// Clear all bits including the adc_RDY, don't know if needed

	//-----------------------------------------------
	// Debug Performance measuring timer
	//-----------------------------------------------
#ifdef DEBUG
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM3EN; 	// APB2 = 50MHz
	TIM3->PSC = 49;							// Prescaler=PSC+1		1us = Increment
	TIM3->CR1 &= (~TIM_CR1_DIR);			// Count up
	TIM3->CNT = 0;
	TIM3->SR &= (~TIM_SR_UIF);				// update IR flag
	TIM3->DIER |= TIM_DIER_UIE;		    	// update IR enable bit
	TIM3->CR1 |= TIM_CR1_CEN;
#endif

	//-----------------------------------------------
	// NVIC Interrupts
	//-----------------------------------------------
	NVIC_SetPriority(DMA1_Channel1_IRQn, NVIC_INT_PRIORITY_LVL2);
	NVIC_ClearPendingIRQ(DMA1_Channel1_IRQn);
	NVIC_EnableIRQ(DMA1_Channel1_IRQn);

//  Nicht notwendig!
//	NVIC_SetPriority(ADC1_2_IRQn, NVIC_INT_PRIORITY_LVL3);
//	NVIC_ClearPendingIRQ(ADC1_2_IRQn);
//	NVIC_EnableIRQ(ADC1_2_IRQn);

	NVIC_SetPriority(FDCAN1_IT0_IRQn, NVIC_INT_PRIORITY_LVL3);	// Only 0 line
	NVIC_ClearPendingIRQ(FDCAN1_IT0_IRQn);
	NVIC_EnableIRQ(FDCAN1_IT0_IRQn);

	NVIC_SetPriority(TIM1_UP_TIM16_IRQn, NVIC_INT_PRIORITY_LVL1);
	NVIC_ClearPendingIRQ(TIM1_UP_TIM16_IRQn);
	NVIC_EnableIRQ(TIM1_UP_TIM16_IRQn);

	NVIC_SetPriority(TIM2_IRQn, NVIC_INT_PRIORITY_LVL2);
	NVIC_ClearPendingIRQ(TIM2_IRQn);
	NVIC_EnableIRQ(TIM2_IRQn);

	NVIC_SetPriority(USART1_IRQn, NVIC_INT_PRIORITY_LVL3);
	NVIC_ClearPendingIRQ(USART1_IRQn);
	NVIC_EnableIRQ(USART1_IRQn);



	//-----------------------------------------------
	// Watchdog:  runs on special 32 kHz Clock
	//----------------------------------------------------
#ifndef DEBUG_DISABLE_WATCHDOG
	IWDG->KR = 0x0000CCCC;		// Enable watch dog. Enable Watchdog for the Release Version
	IWDG->KR = 0x00005555;

	// Prescaler 32 -> Watchdog clock 1 Khz
	mWaitFor((IWDG->SR&IWDG_SR_PVU))
	IWDG->PR = 7;

	// mit prescaler genau  100 ms
	mWaitFor((IWDG->SR&IWDG_SR_RVU))
	IWDG->RLR = 100;
	mWaitFor(IWDG->SR)

	IWDG->KR = 0x0000AAAA;

#endif

	TIM1->CR1 |= TIM_CR1_CEN;       // Enable Scheduler clock

}

/**-------------------------------------------------------------------------------------------------
 *    Description: Scheduler Interrupt
 *    			 * Laufzeit: 25us ohne Daten
 *				  			 60 us mit Daten
 *    Frequency:  1ms
 *    Parameters: None
 *    Return: None
 *    Changes:		2022-04-25	Adc buffers are swapped without the use of index
 *-------------------------------------------------------------------------------------------------*/
void TIM1_UP_TIM16_IRQHandler() {
	AdcData_t *pxgAdcData;
	TIM1->SR &= (~TIM_SR_UIF);

	//mDebugTogglePin(xgDebugStatistics.ui8Toggler);

	// Softwarebuffer is exclusive to vFetchAdc
	// Hardwarebuffer is exclusive to DMA
	//-----------------------------------------------------------------
	pxgAdcData = pxgAdcSoftwareBuffer;
	pxgAdcSoftwareBuffer = pxgAdcHardwareBuffer;
	pxgAdcHardwareBuffer = pxgAdcData;


	// Start sampling
	vDebugTimer2Start();
	DMA1_Channel1->CMAR = (uint32_t) pxgAdcHardwareBuffer->rui32AdcConverted;
	ADC2->CR |= ADC_CR_ADSTART;

	ui8gHardwareTimerEvent = TRUE;	// Set the flag for scheduler
	vHardwareCheckTimerMonitor();			// Check main loop timing
}

/**-------------------------------------------------------------------------------------------------
 *    Description:  Capture Interrupt for measuring time between rising edges
 * 					Calculate time as follows: 50000000/(40*252150) Mhz/(PSC * Measured value)
 *    Frequency:  on capture
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void TIM2_IRQHandler() {
	static uint32_t ui32CaptureCurrentMeasurement = 0;
	static uint32_t ui32CapturePreviousMeasurement = 0;
	uint32_t ui32MeasuredDifference;

	// Direction bit
	if ((mHardwareGetStateDirection >> 1)) {
		xgRuntimeState.xCaptured.ui1InterruptDirectionCapture = FALSE;
	} else {
		xgRuntimeState.xCaptured.ui1InterruptDirectionCapture = TRUE;
	}

	//mDebugTogglePin(xgDebugStatistics.ui8Toggler);

	// Timer Capture. Non-standard time measurement.
	ui32CapturePreviousMeasurement = ui32CaptureCurrentMeasurement;
	ui32CaptureCurrentMeasurement = TIM2->CCR1;

	// Calculate frequency SR0034
	ui32MeasuredDifference = ui32CaptureCurrentMeasurement - ui32CapturePreviousMeasurement;

	// FIT
	//ui32MeasuredDifference = 0;
	vHardwareCheckCaptureUnit(ui32MeasuredDifference);

mDebugPutIntoArray1(xgRuntimeState.xCaptured.ui32InterruptCapturedFrequency);

	xgRuntimeState.xCaptured.ui1InterruptNewCapturePending = TRUE;
	TIM2->SR &= (~(TIM_SR_CC1IF | TIM_SR_CC1OF | TIM_SR_UIF));
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Uart Interrupt for communication with 2nd processor
 *    Frequency:  on byte
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void USART1_IRQHandler() {
	static uint8_t ui8AsynchronousIndex = 0;
	uint16_t ui16Status;
	uint16_t ui16ReceivedData;

	ui16Status = USART1->ISR;

	// FE(Framing error) ORE(Overrun error)
	if ((ui16Status & (USART_ISR_FE | USART_ISR_ORE)) != 0) {
		//DEBUGCODE( while((ui16Status & (USART_ISR_FE | USART_ISR_ORE))){};)
		ui8AsynchronousIndex = 0;

	} else if (ui16Status & (USART_ISR_RXNE)) {
		ui16ReceivedData = USART1->RDR;

		// Receive Synchronization
		//------------------------
		// 9.bit should be set at index 0
		if ((ui16ReceivedData & 0x100) != 0) {
			ui8AsynchronousIndex = 0;
		} else {
			++ui8AsynchronousIndex;
		}

		// Receive
		if (ui8AsynchronousIndex < (sizeof(rui8gHardwareIpkBufferIn))) {
			rui8gHardwareIpkBufferIn[ui8AsynchronousIndex] = (uint8_t) ui16ReceivedData;

			// Decrement Ipk Timeout
			if (xgErrorCounters.ui8ErrorCounterIpkTimeout > 0) {
				--xgErrorCounters.ui8ErrorCounterIpkTimeout;
			}

		} else {
			ui8AsynchronousIndex = 0;
		}

	}

	// Transmit: New Concept for synchronization
	//------------------------------------------
	if (ui8AsynchronousIndex == 0) {
		USART1->TDR = (0x100 + rui8gHardwareIpkBufferOut[0]);
	} else if (ui8AsynchronousIndex < (sizeof(rui8gHardwareIpkBufferOut))) {
		USART1->TDR = rui8gHardwareIpkBufferOut[ui8AsynchronousIndex];

		// Exchange Buffers
		if (ui8AsynchronousIndex == (sizeof(Ipk_t) - 1)) {
			ui8gHardwareUartSwapRequested = TRUE;
			// NOTE! Buffers are swapped in
			//memcpy((uint8_t*) rui8HardwareIpkBufferOut, (uint8_t*) rui8IpkTxBuffer, sizeof(rui8IpkTxBuffer));
			//memcpy((uint8_t*) rui8IpkRxBuffer, (uint8_t*) rui8HardwareIpkBufferIn, sizeof(rui8HardwareIpkBufferIn));
		}
	}

	USART1->ICR = 0xFFFFFFFF;

}

/**-------------------------------------------------------------------------------------------------
 *    Description: Receives CAN frames and puts them in a queue
 *    Frequency:  on frame
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void FDCAN1_intr0_IRQHandler() {

	uint32_t ui32interruptsource = FDCAN1->IR;
	uint32_t ui32numberofframes = 0;
	uint32_t ui32counterparsedframes = 0;
	uint32_t ui32getindex = 0;
	uint32_t ui32offset = 0;

	// Bus Off Error
	if (ui32interruptsource & FDCAN_IR_BO) {
		FDCAN1->IR |= FDCAN_IR_BO;
	}

	// New Message in FIFO 0
	else if (ui32interruptsource & FDCAN_IR_RF0N) {

		DEBUGCODE(++xgDebugStatistics.ui32DebugCanRx0
		;
		)

		ui32numberofframes = (FDCAN1->RXF0S) & 0x07;
		ui32getindex = (FDCAN1->RXF0S >> 8) & 0x03;

		uint32_t ui32clearchain = (ui32getindex + ui32numberofframes - 1) % SRAMCAN_RF0_NBR;

		// Write data into buffers
		while (ui32counterparsedframes != ui32numberofframes) {

			xgCanMessageRx[ui8gCircularHeadIndex].ui16Id = (pxgFdcanMessageRam->xRxfifo0[ui32getindex].ui32Control0 >> 18) & 0x7FF;	// ID
			xgCanMessageRx[ui8gCircularHeadIndex].ui16Size = (pxgFdcanMessageRam->xRxfifo0[ui32getindex].ui32Control1 >> 16) & 0x0F;	// DLC
			xgCanMessageRx[ui8gCircularHeadIndex].ui32Data[0] = pxgFdcanMessageRam->xRxfifo0[ui32getindex].rui32Fifo[0];					// Dword 0
			xgCanMessageRx[ui8gCircularHeadIndex].ui32Data[1] = pxgFdcanMessageRam->xRxfifo0[ui32getindex].rui32Fifo[1];					// Dword 1

			++ui32counterparsedframes;
			ui32getindex = (ui32getindex + 1) % SRAMCAN_RF0_NBR;
			ui8gCircularHeadIndex = (ui8gCircularHeadIndex + 1) % CAN_BUF_COUNT;

		}

		FDCAN1->RXF0A = ui32clearchain;
		FDCAN1->IR |= FDCAN_IR_RF0N;	// Clear interrupt flag
	}

	// New Message in FIFO 1
	else if (ui32interruptsource & FDCAN_IR_RF1N) {
		DEBUGCODE( {
			xgDebugStatistics.ui32DebugCanRx1 = xgDebugStatistics.ui32DebugCanRx1 + 1
			;
		}
		)
		ui32numberofframes = (FDCAN1->RXF1S) & 0x07;
		ui32getindex = (FDCAN1->RXF1S >> 8) & 0x03;

		uint32_t ui32clearchain = (ui32getindex + ui32numberofframes - 1) % SRAMCAN_RF1_NBR;
		// Check buffers
		while (ui32counterparsedframes != ui32numberofframes) {
			uint32_t ui32identifier = (pxgFdcanMessageRam->xRxfifo1[ui32getindex].ui32Control0 >> 18) & 0x7FF;

			switch ((ui32identifier & 0x07)) {
			case 1:                                  // Appl.StandardID+1: Frequenzen
				for (uint16_t i = 0; i < 8; i++) {   // 8 byte vom Zwischenpuffer zum Param[32-39]
					xgReceivedParameters.rui8Buffer[32 + i] = pxgFdcanMessageRam->xRxfifo1[ui32getindex].rui8Fifo[i];
				}

				break;
			case 2:                                  // Appl.StandardID+2: Abschaltverzögerung und Anlaufüberbrückungszeit, eventuell Position
				for (uint16_t i = 0; i < 8; i++) {   // 8 byte vom Zwischenpuffer zum Param[40-47]
					xgReceivedParameters.rui8Buffer[40 + i] = pxgFdcanMessageRam->xRxfifo1[ui32getindex].rui8Fifo[i];
				}

				break;
			case 3:                                   // Appl.StandardID+3 nichts
				for (uint16_t i = 0; i < 8; i++) {    // 8 byte vom Zwischenpuffer zum Param[48-55]
					xgReceivedParameters.rui8Buffer[48 + i] = pxgFdcanMessageRam->xRxfifo1[ui32getindex].rui8Fifo[i];
				}

				break;
			case 4:							          // Appl.StandardID+4
				// Byte8 im Zwischenpuffer ist index-DNCO[], byte12-15 = 4 dnco-werte.
				// 64 Message x 4 byte = 256 Byte.
				ui32offset = pxgFdcanMessageRam->xRxfifo1[ui32getindex].rui8Fifo[0];
				for (uint16_t i = 0; i < 4; i++) {
					xgReceivedParameters.rui8ReceivedDnco[ui32offset + i] = pxgFdcanMessageRam->xRxfifo1[ui32getindex].rui8Fifo[i + 4];
				}
				break;
			case 5:                        //Appl.StandardID+5
				break;
			}

			++ui32counterparsedframes;
		}

		FDCAN1->RXF1A = ui32clearchain;
		FDCAN1->IR |= FDCAN_IR_RF1N;

		// New Error or warning
	} else if (ui32interruptsource & FDCAN_IR_EW) {

		FDCAN1->CCCR |= FDCAN_CCCR_INIT;
		while (!(FDCAN1->CCCR & FDCAN_CCCR_INIT)) {
		}

		FDCAN1->CCCR &= (~FDCAN_CCCR_INIT);
		while ((FDCAN1->CCCR & FDCAN_CCCR_INIT)) {
		}

		FDCAN1->IR |= FDCAN_IR_EW;

	}

}

// Lösung ohne DMA
/*
 void ADC1_2_IRQHandler() {
 //Proof of concept, not used
 uint16_t ui16adcvalue = 0;
 static uint8_t ui8counter = 0;
 static uint8_t ui8selector = 0;

 ui16adcvalue = ADC2->DR;

 ui16debugadcs[ui8selector][ui8counter] = ui16adcvalue;

 if (ADC2->ISR & ADC_ISR_EOS) {
 ADC2->CR |= ADC_CR_ADSTART;
 }

 //ADC2->ISR &=(~(ADC_ISR_EOC|ADC_ISR_EOSMP|ADC_ISR_EOS));

 ++ui8selector;

 if (ui8selector == 4) {
 ui8counter = (ui8counter + 1) % 100;
 ui8selector = 0;
 }
 }*/

/**-------------------------------------------------------------------------------------------------
 *    Description:  DMA ISR for transfering ADC sampled values into the memory
 *  				Execution time 4 us, 40 us between DMA Interrupts.
 *  				Could be optimized when more sequences are used!
 *    Frequency:  after each conversion
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void DMA1_Channel1_IRQHandler() {
	uint32_t ui32ConversionCount;

	DMA1->IFCR |= DMA_IFCR_CGIF1;

	++pxgAdcHardwareBuffer->ui32ConversionsCount;
	ui32ConversionCount = pxgAdcHardwareBuffer->ui32ConversionsCount;

	DMA1_Channel1->CMAR = (uint32_t) &pxgAdcHardwareBuffer->rui32AdcConverted[MAX_ADC_SOURCES * ui32ConversionCount];
	if (ui32ConversionCount < MAX_CONVERSIONS) {
		ADC2->CR |= ADC_CR_ADSTART;
	} else {
		vDebugTimer2Stop();
		pxgAdcHardwareBuffer->ui32ConversionReady = TRUE;
	}
}
/**-------------------------------------------------------------------------------------------------
 *
 // .weak Definitions are all set to Default HardwareErrorLoop Handler set
 // !!!Important!!! do not remove these default Handlers. Only if the interrupt is used, replace the body
 // /SR0030
 *
 *-------------------------------------------------------------------------------------------------*/
void NMI_Handler() {
	vHardwareCheckUnexpectedInterrupt();
}
void HardFault_Handler() {
	vHardwareCheckUnexpectedInterrupt();
}
void MemManage_Handler() {
	vHardwareCheckUnexpectedInterrupt();
}
void BusFault_Handler() {
	vHardwareCheckUnexpectedInterrupt();
}
void UsageFault_Handler() {
	vHardwareCheckUnexpectedInterrupt();
}
void SVC_Handler() {
	vHardwareCheckUnexpectedInterrupt();
}
void DebugMon_Handler() {
	vHardwareCheckUnexpectedInterrupt();
}
void PendSV_Handler() {
	vHardwareCheckUnexpectedInterrupt();
}
void SysTick_Handler() {
	vHardwareCheckUnexpectedInterrupt();
}
void WWDG_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void PVD_PVM_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void RTC_TAMP_LSECSS_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void RTC_WKUP_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void FLASH_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void RCC_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void EXTI0_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void EXTI1_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void EXTI2_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void EXTI3_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void EXTI4_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void DMA1_Channel2_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void DMA1_Channel3_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void DMA1_Channel4_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void DMA1_Channel5_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void DMA1_Channel6_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void DMA1_Channel7_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void ADC1_2_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void USB_HP_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void USB_LP_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void FDCAN1_intr1_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void EXTI9_5_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void TIM1_BRK_TIM15_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void TIM1_TRG_COM_TIM17_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void TIM1_CC_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void TIM3_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void TIM4_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void I2C1_EV_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void I2C1_ER_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void I2C2_EV_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void I2C2_ER_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void SPI1_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void SPI2_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void USART2_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void USART3_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void EXTI15_10_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void RTC_Alarm_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void USBWakeUp_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void TIM8_BRK_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void TIM8_UP_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void TIM8_TRG_COM_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void TIM8_CC_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void ADC3_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void FMC_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void LPTIM1_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void TIM5_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void SPI3_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void UART4_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void UART5_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void TIM6_DAC_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void TIM7_DAC_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void DMA2_Channel1_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void DMA2_Channel2_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void DMA2_Channel3_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void DMA2_Channel4_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void DMA2_Channel5_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void ADC4_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void ADC5_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void UCPD1_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void COMP1_2_3_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void COMP4_5_6_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void COMP7_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void HRTIM_Master_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void HRTIM_TIMA_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void HRTIM_TIMB_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void HRTIM_TIMC_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void HRTIM_TIMD_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void HRTIM_TIME_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void HRTIM_TIM_FLT_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void HRTIM_TIMF_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void CRS_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void SAI1_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void TIM20_BRK_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void TIM20_UP_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void TIM20_TRG_COM_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void TIM20_CC_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void FPU_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void I2C4_EV_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void I2C4_ER_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void SPI4_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void AES_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void FDCAN2_intr0_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void FDCAN2_intr1_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void FDCAN3_intr0_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void FDCAN3_intr1_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void RNG_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void LPUART1_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void I2C3_EV_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void I2C3_ER_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void DMAMUX_OVR_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void QUADSPI_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void DMA1_Channel8_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void DMA2_Channel6_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void DMA2_Channel7_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void DMA2_Channel8_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void CORDIC_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
void FMAC_IRQHandler() {
	vHardwareCheckUnexpectedInterrupt();
}
