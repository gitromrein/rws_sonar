/*
 * config.h
 *
 *  Created on: 22.02.2022
 *      Author: am
 *
 *      This file included using included using compiler configuration of project and is the first one to be included in every file.
 *      -It contains debug switches to test and modify code without much impact on the runtime.
 *      -It contains datatypes used through the project.
 *      -It contains references from stdlib
 *
 */


#ifndef CONFIG_H_
#define CONFIG_H_

/* General Includes */
#include <stdint.h>
#include <stdio.h>
#include <stm32g4xx.h>
#include <string.h>

#define SIZEOF(a) (sizeof(a)/sizeof(*a))

#define TRUE 1
#define FALSE 0

/*	Switches used for debugging */
#ifdef DEBUG
// 	options
//	#define DEBUG_DISABLE_WATCHDOG
//	#define DEBUG_SKIP_SLOT_EXCHANGE
// 	#define DEBUG_DISABLE_SLOKOFF
	#define DEBUG_DISABLE_ROMTEST
	#define DEBUG_DISABLE_DUALCHANNEL_AND
// #define DEBUG_DISABLE_RAMTEST
	#define DEBUGCODE(somecode) { somecode ;}

#else
	#define DEBUGCODE(somecode) {;}
#endif

#endif /* CONFIG_H_ */
