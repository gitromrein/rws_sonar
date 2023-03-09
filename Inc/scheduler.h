/*
 * 	scheduler.h
 *
 *  Created on: 19.07.2021
 *      Author: am
 */

#ifndef SCHEDULER_H_
#define SCHEDULER_H_

/* Public datatypes */
typedef void (*func)(uint8_t*);

/* Public variables */
extern volatile uint8_t ui8gHardwareTimerEvent;	// Exported to be set by timer

/* Public prototypes */
extern void vSchedulerInitialize(void);
extern void vSchedulerExecute(void);

#endif /* SCHEDULER_H_ */
