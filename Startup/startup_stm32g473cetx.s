/**
  ******************************************************************************
  * @file      startup_stm32g473xx.s
  * @author    MCD Application Team
  * @brief     STM32G473xx devices vector table GCC toolchain.
  *            This module performs:
  *                - Set the initial SP
  *                - Set the initial PC == Reset_Handler,
  *                - Set the vector table entries with the exceptions ISR address,
  *                - Configure the clock system
  *                - Branches to main in the C library (which eventually
  *                  calls main()).
  *            After Reset the Cortex-M4 processor is in Thread mode,
  *            priority is Privileged, and the Stack is set to Main.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

  .syntax unified
	.cpu cortex-m4
	.fpu softvfp
	.thumb

.global	g_pfnVectors
.global	Default_Handler

/* start address for the initialization values of the .data section.
defined in linker script */
.word	_sidata
/* start address for the .data section. defined in linker script */
.word	_sdata
/* end address for the .data section. defined in linker script */
.word	_edata
/* start address for the .bss section. defined in linker script */
.word	_sbss
/* end address for the .bss section. defined in linker script */
.word	_ebss

.equ  BootRAM,        0xF1E0F85F
/**
 * @brief  This is the code that gets called when the processor first
 *          starts execution following a reset event. Only the absolutely
 *          necessary set is performed, after which the application
 *          supplied main() routine is called.
 * @param  None
 * @retval : None
*/

    .section	.text.Reset_Handler
	.weak	Reset_Handler
	.type	Reset_Handler, %function
Reset_Handler:
  ldr   r0, =_estack
  mov   sp, r0          /* set stack pointer */

/* Copy the data segment initializers from flash to SRAM */
  ldr r0, =_sdata
  ldr r1, =_edata
  ldr r2, =_sidata
  movs r3, #0
  b	LoopCopyDataInit

CopyDataInit:
  ldr r4, [r2, r3]
  str r4, [r0, r3]
  adds r3, r3, #4

LoopCopyDataInit:
  adds r4, r0, r3
  cmp r4, r1
  bcc CopyDataInit
  
/* Zero fill the bss segment. */
  ldr r2, =_sbss
  ldr r4, =_ebss
  movs r3, #0
  b LoopFillZerobss

FillZerobss:
  str  r3, [r2]
  adds r2, r2, #4

LoopFillZerobss:
  cmp r2, r4
  bcc FillZerobss

/* Call the clock system intitialization function.*/
    bl  SystemInit
/* Call static constructors */
    bl __libc_init_array
/* Call the application's entry point.*/
	bl	main

LoopForever:
    b LoopForever

.size	Reset_Handler, .-Reset_Handler

/**
 * @brief  This is the code that gets called when the processor receives an
 *         unexpected interrupt.  This simply enters an infinite loop, preserving
 *         the system state for examination by a debugger.
 *
 * @param  None
 * @retval : None
*/
    .section	.text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
	b	Infinite_Loop
	.size	Default_Handler, .-Default_Handler
/******************************************************************************
*
* The minimal vector table for a Cortex-M4.  Note that the proper constructs
* must be placed on this to ensure that it ends up at physical address
* 0x0000.0000.
*
******************************************************************************/
 	.section	.isr_vector,"a",%progbits
	.type	g_pfnVectors, %object
	.size	g_pfnVectors, .-g_pfnVectors


g_pfnVectors:
	.word	_estack
		.word	Reset_Handler
	.word	NMI_Handler
	.word	HardFault_Handler
	.word	MemManage_Handler
	.word	BusFault_Handler
	.word	UsageFault_Handler
	.word	0
	.word	0
	.word	0
	.word	0
	.word	SVC_Handler
	.word	DebugMon_Handler
	.word	0
	.word	PendSV_Handler
	.word	SysTick_Handler
	.word	WWDG_IRQHandler
	.word	PVD_PVM_IRQHandler
	.word	RTC_TAMP_LSECSS_IRQHandler
	.word	RTC_WKUP_IRQHandler
	.word	FLASH_IRQHandler
	.word	RCC_IRQHandler
	.word	EXTI0_IRQHandler
	.word	EXTI1_IRQHandler
	.word	EXTI2_IRQHandler
	.word	EXTI3_IRQHandler
	.word	EXTI4_IRQHandler
	.word	DMA1_Channel1_IRQHandler
	.word	DMA1_Channel2_IRQHandler
	.word	DMA1_Channel3_IRQHandler
	.word	DMA1_Channel4_IRQHandler
	.word	DMA1_Channel5_IRQHandler
	.word	DMA1_Channel6_IRQHandler
	.word	DMA1_Channel7_IRQHandler
	.word	ADC1_2_IRQHandler
	.word	USB_HP_IRQHandler
	.word	USB_LP_IRQHandler
	.word	FDCAN1_intr0_IRQHandler
	.word	FDCAN1_intr1_IRQHandler
	.word	EXTI9_5_IRQHandler
	.word	TIM1_BRK_TIM15_IRQHandler
	.word	TIM1_UP_TIM16_IRQHandler
	.word	TIM1_TRG_COM_TIM17_IRQHandler
	.word	TIM1_CC_IRQHandler
	.word	TIM2_IRQHandler
	.word	TIM3_IRQHandler
	.word	TIM4_IRQHandler
	.word	I2C1_EV_IRQHandler
	.word	I2C1_ER_IRQHandler
	.word	I2C2_EV_IRQHandler
	.word	I2C2_ER_IRQHandler
	.word	SPI1_IRQHandler
	.word	SPI2_IRQHandler
	.word	USART1_IRQHandler
	.word	USART2_IRQHandler
	.word	USART3_IRQHandler
	.word	EXTI15_10_IRQHandler
	.word	RTC_Alarm_IRQHandler
	.word	USBWakeUp_IRQHandler
	.word	TIM8_BRK_IRQHandler
	.word	TIM8_UP_IRQHandler
	.word	TIM8_TRG_COM_IRQHandler
	.word	TIM8_CC_IRQHandler
	.word	ADC3_IRQHandler
	.word	FMC_IRQHandler
	.word	LPTIM1_IRQHandler
	.word	TIM5_IRQHandler
	.word	SPI3_IRQHandler
	.word	UART4_IRQHandler
	.word	UART5_IRQHandler
	.word	TIM6_DAC_IRQHandler
	.word	TIM7_DAC_IRQHandler
	.word	DMA2_Channel1_IRQHandler
	.word	DMA2_Channel2_IRQHandler
	.word	DMA2_Channel3_IRQHandler
	.word	DMA2_Channel4_IRQHandler
	.word	DMA2_Channel5_IRQHandler
	.word	ADC4_IRQHandler
	.word	ADC5_IRQHandler
	.word	UCPD1_IRQHandler
	.word	COMP1_2_3_IRQHandler
	.word	COMP4_5_6_IRQHandler
	.word	COMP7_IRQHandler
	.word	HRTIM_Master_IRQhandler
	.word	HRTIM_TIMA_IRQhandler
	.word	HRTIM_TIMB_IRQhandler
	.word	HRTIM_TIMC_IRQhandler
	.word	HRTIM_TIMD_IRQhandler
	.word	HRTIM_TIME_IRQhandler
	.word	HRTIM_TIM_FLT_IRQhandler
	.word	HRTIM_TIMF_IRQhandler
	.word	CRS_IRQHandler
	.word	SAI1_IRQHandler
	.word	TIM20_BRK_IRQHandler
	.word	TIM20_UP_IRQHandler
	.word	TIM20_TRG_COM_IRQHandler
	.word	TIM20_CC_IRQHandler
	.word	FPU_IRQHandler
	.word	I2C4_EV_IRQHandler
	.word	I2C4_ER_IRQHandler
	.word	SPI4_IRQHandler
	.word	AES_IRQHandler
	.word	FDCAN2_intr0_IRQHandler
	.word	FDCAN2_intr1_IRQHandler
	.word	FDCAN3_intr0_IRQHandler
	.word	FDCAN3_intr1_IRQHandler
	.word	RNG_IRQHandler
	.word	LPUART1_IRQHandler
	.word	I2C3_EV_IRQHandler
	.word	I2C3_ER_IRQHandler
	.word	DMAMUX_OVR_IRQHandler
	.word	QUADSPI_IRQHandler
	.word	DMA1_Channel8_IRQHandler
	.word	DMA2_Channel6_IRQHandler
	.word	DMA2_Channel7_IRQHandler
	.word	DMA2_Channel8_IRQHandler
	.word	CORDIC_IRQHandler
	.word	FMAC_IRQHandler

/*******************************************************************************
*
* Provide weak aliases for each Exception handler to the Default_Handler.
* As they are weak aliases, any function with the same name will override
* this definition.
*
*******************************************************************************/

	.weak	NMI_Handler
	.weak	HardFault_Handler
	.weak	MemManage_Handler
	.weak	BusFault_Handler
	.weak	UsageFault_Handler
	.weak	SVC_Handler
	.weak	DebugMon_Handler
	.weak	PendSV_Handler
	.weak	SysTick_Handler
	.weak	WWDG_IRQHandler
	.weak	PVD_PVM_IRQHandler
	.weak	RTC_TAMP_LSECSS_IRQHandler
	.weak	RTC_WKUP_IRQHandler
	.weak	FLASH_IRQHandler
	.weak	RCC_IRQHandler
	.weak	EXTI0_IRQHandler
	.weak	EXTI1_IRQHandler
	.weak	EXTI2_IRQHandler
	.weak	EXTI3_IRQHandler
	.weak	EXTI4_IRQHandler
	.weak	DMA1_Channel1_IRQHandler
	.weak	DMA1_Channel2_IRQHandler
	.weak	DMA1_Channel3_IRQHandler
	.weak	DMA1_Channel4_IRQHandler
	.weak	DMA1_Channel5_IRQHandler
	.weak	DMA1_Channel6_IRQHandler
	.weak	DMA1_Channel7_IRQHandler
	.weak	ADC1_2_IRQHandler
	.weak	USB_HP_IRQHandler
	.weak	USB_LP_IRQHandler
	.weak	FDCAN1_intr0_IRQHandler
	.weak	FDCAN1_intr1_IRQHandler
	.weak	EXTI9_5_IRQHandler
	.weak	TIM1_BRK_TIM15_IRQHandler
	.weak	TIM1_UP_TIM16_IRQHandler
	.weak	TIM1_TRG_COM_TIM17_IRQHandler
	.weak	TIM1_CC_IRQHandler
	.weak	TIM2_IRQHandler
	.weak	TIM3_IRQHandler
	.weak	TIM4_IRQHandler
	.weak	I2C1_EV_IRQHandler
	.weak	I2C1_ER_IRQHandler
	.weak	I2C2_EV_IRQHandler
	.weak	I2C2_ER_IRQHandler
	.weak	SPI1_IRQHandler
	.weak	SPI2_IRQHandler
	.weak	USART1_IRQHandler
	.weak	USART2_IRQHandler
	.weak	USART3_IRQHandler
	.weak	EXTI15_10_IRQHandler
	.weak	RTC_Alarm_IRQHandler
	.weak	USBWakeUp_IRQHandler
	.weak	TIM8_BRK_IRQHandler
	.weak	TIM8_UP_IRQHandler
	.weak	TIM8_TRG_COM_IRQHandler
	.weak	TIM8_CC_IRQHandler
	.weak	ADC3_IRQHandler
	.weak	FMC_IRQHandler
	.weak	LPTIM1_IRQHandler
	.weak	TIM5_IRQHandler
	.weak	SPI3_IRQHandler
	.weak	UART4_IRQHandler
	.weak	UART5_IRQHandler
	.weak	TIM6_DAC_IRQHandler
	.weak	TIM7_DAC_IRQHandler
	.weak	DMA2_Channel1_IRQHandler
	.weak	DMA2_Channel2_IRQHandler
	.weak	DMA2_Channel3_IRQHandler
	.weak	DMA2_Channel4_IRQHandler
	.weak	DMA2_Channel5_IRQHandler
	.weak	ADC4_IRQHandler
	.weak	ADC5_IRQHandler
	.weak	UCPD1_IRQHandler
	.weak	COMP1_2_3_IRQHandler
	.weak	COMP4_5_6_IRQHandler
	.weak	COMP7_IRQHandler
	.weak	HRTIM_Master_IRQhandler
	.weak	HRTIM_TIMA_IRQhandler
	.weak	HRTIM_TIMB_IRQhandler
	.weak	HRTIM_TIMC_IRQhandler
	.weak	HRTIM_TIMD_IRQhandler
	.weak	HRTIM_TIME_IRQhandler
	.weak	HRTIM_TIM_FLT_IRQhandler
	.weak	HRTIM_TIMF_IRQhandler
	.weak	CRS_IRQHandler
	.weak	SAI1_IRQHandler
	.weak	TIM20_BRK_IRQHandler
	.weak	TIM20_UP_IRQHandler
	.weak	TIM20_TRG_COM_IRQHandler
	.weak	TIM20_CC_IRQHandler
	.weak	FPU_IRQHandler
	.weak	I2C4_EV_IRQHandler
	.weak	I2C4_ER_IRQHandler
	.weak	SPI4_IRQHandler
	.weak	AES_IRQHandler
	.weak	FDCAN2_intr0_IRQHandler
	.weak	FDCAN2_intr1_IRQHandler
	.weak	FDCAN3_intr0_IRQHandler
	.weak	FDCAN3_intr1_IRQHandler
	.weak	RNG_IRQHandler
	.weak	LPUART1_IRQHandler
	.weak	I2C3_EV_IRQHandler
	.weak	I2C3_ER_IRQHandler
	.weak	DMAMUX_OVR_IRQHandler
	.weak	QUADSPI_IRQHandler
	.weak	DMA1_Channel8_IRQHandler
	.weak	DMA2_Channel6_IRQHandler
	.weak	DMA2_Channel7_IRQHandler
	.weak	DMA2_Channel8_IRQHandler
	.weak	CORDIC_IRQHandler
	.weak	FMAC_IRQHandler
	.weak    SystemInit

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
