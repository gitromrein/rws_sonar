/*
 * contract.c
 *
 *  Created on: 31.01.2022
 *      Author: am
 */


/*-------------------------------------------------------------------------------------------------
 *    Description: Asserts the condition. Helpful for finding bugs
 *    Frequency:  atmost once
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void onAssert__(char const *file, unsigned line){
	while(1){
		IWDG->KR = 0xAAAA;
	}
}
