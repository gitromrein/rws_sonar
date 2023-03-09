/*
 * safe-speed.h
 *
 *  Created on: 21.07.2021
 *      Author: am
 */

#ifndef SAFE_SPEED_H_
#define SAFE_SPEED_H_




/* Public defines*/
// Frequencies
#define FREQ_0_1HZ  1
#define FREQ_0_5HZ	5
#define FREQ_1_1HZ 11
#define FREQ_2HZ   20
#define FREQ_5HZ 50
#define FREQ_10HZ 100
#define FREQ_12HZ 120
#define FREQ_13HZ 130
#define FREQ_20HZ 200
#define FREQ_25HZ 250
#define FREQ_65HZ 650
#define FREQ_75HZ 750
#define FREQ_100HZ 1000
#define FREQ_150HZ 1500
#define FREQ_200HZ 2000
#define FREQ_1000HZ 10000
#define FREQ_1500HZ 15000


/* Public prototpypes */
void vSpeedCheckPhase(void);
void vDirectionCheckRotationSwitch(void);
void vSpeedCheckQuittStuckat(void);
void vSpeedCheck1500Hz(void);
void vSpeedCheckFrequenciesNotEqual(void);
void vSpeedExecute(uint8_t *pui8CheckinCounter);


#endif /* SAFE_SPEED_H_ */
