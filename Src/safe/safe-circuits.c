/*
 *  safe-circuit.c
 *
 *  Created on: 24.10.2019
 *      Author: am
 */

#include "safe/safe-circuits.h"

#include "pdsv-parameters.h"
#include "pdsv-fdcan.h"
#include "pdsv-ipk.h"
#include "pdsv-hardware.h"
#include "pdsv-inputs.h"
#include "pdsv-diagnostics.h"

/* Private defines*/
#define SK_CHECK_DELAY   20     //entspricht 200ms Delay SKR Auswertung
#define Q_DELAY  		 50
#define SK_TIMEOUT       50
#define SK_CROSSCIRCUIT  3100                //31 entspricht 3,1Sek

/* Public variables */
SafetyCircuitManagement xgSafetyCircuitManagement;

/**-------------------------------------------------------------------------------------------------
 *    Description:  Reset Safety Circuit errors and counters
 *    Frequency:  on RTSK
 *    Parameters: None
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vSafetyCircuitResetManagement(void) {
	memset((uint8_t*) &xgSafetyCircuitManagement.xErrorCounters, 0, sizeof(xgSafetyCircuitManagement.xErrorCounters));
}

/**-------------------------------------------------------------------------------------------------
 *    Description:  Querschlusstest. Die Eingänge werden  4ms Entprellung nach Querschluss überprüft
 *    Frequency:  1ms
 *    Parameters: *    Parameters: pui8CheckinCounter - Scheduler Counter
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vSafetyCircuitCheckCrosscircuit(uint8_t *pui8CheckinCounter) {
	uint8_t ui8Error = 0;

	++(*pui8CheckinCounter);
	uint16_t ui16CurrentInput = ui16gSampledInputs;

	// Crosscircuit Skr1
	//******************************************************************
	if ((xgRuntimeParameters.ui1SafetyCircuitCrosscircuitsMode1 != 0) && (xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrState == 5)) {   //Querschlusserkennung erforderlich, SKR on Zustand
		if ((ui16CurrentInput & 0x03) == 0) //*********************00******************************--
				{
			if (xgRuntimeParameters.ui8SafetyCircuitsAntivalent & 0x01) {   //bei SKR antivalent ist low/low kein Abschaltgrund
				xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK1a = 0;
				xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK1b = 0;
			}
			//Zustand 00, über cnt verzögert absch.
			if (xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK1b < SK_CROSSCIRCUIT)
				xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK1b++;
			if (xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK1b >= SK_CROSSCIRCUIT)
				ui8Error = 1;

		} else {   //Eingänge sind nicht 00
			if ((ui16CurrentInput & 0x01) == 0) //******************--x0*********************************
					{
				xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK1a = 0;
			} else  //E1 == 1 ***************************************-x1*********************************
			{

				if (xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK1a < SK_CROSSCIRCUIT)
					xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK1a++;
				if (xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK1a >= SK_CROSSCIRCUIT)
					ui8Error = 1;

			}
			if ((ui16CurrentInput & 0x02) == 0) //*********************-0x******************************-
					{
				xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK1b = 0;
			} else //E2 == 1 ******************************************--1x******************************
			{

				if (xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK1b < SK_CROSSCIRCUIT)
					xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK1b++;
				if (xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK1b >= SK_CROSSCIRCUIT)
					ui8Error = 1;

			}
		}
		if (ui8Error != 0) {   // SKR abschalten
			memset(&xgSafetyCircuitManagement.xSafetyCircuit1, 0, sizeof(xgSafetyCircuitManagement.xSafetyCircuit1));
			xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrState = 1;
			xgIpkTx.xSafetyCircuitErrors.ui1ErrorCrosscircuit1 = TRUE;
		}
	} else    //Querschlusserkennung nicht erforderlich oder SKR im off Zustand
	{
		xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK1a = 0;
		xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK1b = 0;
	}

	// Crosscircuit Skr2
	//******************************************************************
	ui8Error = 0;
	//Querschlusserkennung erforderlich, SKR on Zustand
	if ((xgRuntimeParameters.ui1SafetyCircuitCrosscircuitsMode2 != 0) && (xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrState == 5)) {
		// 00
		if ((ui16CurrentInput & 0x0C) == 0) {
			if ((xgRuntimeParameters.ui8SafetyCircuitsAntivalent & 0x02)) {   //bei SKR antivalent ist low/low kein Abschaltgrund
				xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK2a = 0;
				xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK2b = 0;
			}
			//Zustand 00, über cnt verzögert abschalten
			if (xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK2b < SK_CROSSCIRCUIT)
				xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK2b++;
			if (xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK2b >= SK_CROSSCIRCUIT)
				ui8Error = 1;

		} else {
			if ((ui16CurrentInput & 0x04) == 0) //******************x0*********************************--
					{
				xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK2a = 0;
			} else  //E1 == 1 ************************************--x1*********************************--
			{

				if (xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK2a < SK_CROSSCIRCUIT)
					xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK2a++;
				if (xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK2a >= SK_CROSSCIRCUIT)
					ui8Error = 1;

			}
			if ((ui16CurrentInput & 0x08) == 0) //******************0x*********************************--
					{
				xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK2b = 0;
			} else //E2 == 1 ***************************************-01*********************************-
			{

				if (xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK2b < SK_CROSSCIRCUIT)
					xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK2b++;
				if (xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK2b >= SK_CROSSCIRCUIT)
					ui8Error = 1;

			}
		}
		if (ui8Error != 0) {   //SKR abschalten
			memset(&xgSafetyCircuitManagement.xSafetyCircuit2, 0, sizeof(xgSafetyCircuitManagement.xSafetyCircuit2));
			xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrState = 1;

			xgIpkTx.xSafetyCircuitErrors.ui1ErrorCrosscircuit2 = TRUE;
		}
	} else {    //Querschlusserkennung nicht erforderlich oder SKR im off Zustand
		xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK2a = 0;
		xgSafetyCircuitManagement.xErrorCounters.ui32ErrorCounterCrosscircuitSK2b = 0;
	}
}

/**-------------------------------------------------------------------------------------------------
 *    Description:  Sicherheitskreise
 *    Frequency:  10ms
 *    Parameters: pui8CheckinCounter - Scheduler Counter
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vSafetyCircuitExecute(uint8_t *pui8CheckinCounter) {
	uint8_t ui8TempInputs = 0;
	uint8_t ui8InputsSafetyCircuit = 0;

	++(*pui8CheckinCounter);

	// Active only system is okay!
	if ((xgImkRx.xHeader.ui1nSlok) || (!xgRuntimeState.ui8ParameterComplete)) {	//SKR Reset
		xgSafetyCircuitManagement.ui8SkrEvaluationDelayCounter = SK_CHECK_DELAY;

		// Reset states
		memset(&xgSafetyCircuitManagement.xSafetyCircuit1, 0, sizeof(xgSafetyCircuitManagement.xSafetyCircuit1));
		xgIpkTx.xSafetyCircuit.ui1Skr1 = FALSE;

		memset(&xgSafetyCircuitManagement.xSafetyCircuit2, 0, sizeof(xgSafetyCircuitManagement.xSafetyCircuit2));
		xgIpkTx.xSafetyCircuit.ui1Skr2 = FALSE;

		//kein Error und ParamKomplett
	} else {

		if (xgSafetyCircuitManagement.ui8SkrEvaluationDelayCounter != 0)
			xgSafetyCircuitManagement.ui8SkrEvaluationDelayCounter--;
		else {

			ui8InputsSafetyCircuit = (xgIpkTx.xInputs.ui4Inputs);

			// Safety Circuit 1
			//------------------------------------------------------------
			if (xgRuntimeParameters.ui1SafetyCircuitsActivation1) {
				if (xgRuntimeParameters.ui1SafetyCircuitAntivalent1)
					ui8TempInputs = (0x02 ^ ui8InputsSafetyCircuit) & 0x03;
				else
					ui8TempInputs = (ui8InputsSafetyCircuit) & 0x03;

				//-----SKR1	Quittierung----------------------------------------------------------------------------
				if (xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrQDelay != 0)
					xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrQDelay--;
				else
					xgSafetyCircuitManagement.xSafetyCircuit1.ui8Q = 0;
				if (xgImkRx.xZmvSafetyCircuitIn.ui1SkrQ1) {		//Can-Q == 1
					if (!xgRuntimeParameters.ui1SafetyCircuitsAcknowledgementDelay1)
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrQDelay = Q_DELAY;	//1=0ms, 0=500ms
					if (!xgRuntimeParameters.ui1SafetyCircuitsAcknowledgementLevel1) {	//quittieren bei high Pegel
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8Q = 1;
					} else //quittieren bei fallender Flanke
					{
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8Qpos = 1;
					}
				} else	//Can-Q == 0
				{
					if (xgSafetyCircuitManagement.xSafetyCircuit1.ui8Qpos == 1) {	//negative Quittier-Flanke
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8Q = 1;
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8Qpos = 0;
					}
				}
				//-----------------------------------------------------------------
				if (!xgRuntimeParameters.ui1SafetyCircuitsAcknowledgementMode1)
					xgSafetyCircuitManagement.xSafetyCircuit1.ui8Q = 1;	//automatisch quittieren
				//-----------------------------------------------------------------
				//bei gesetztem SKR Quittierflags löschen
				if (xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrQuitt == 1) {
					xgSafetyCircuitManagement.xSafetyCircuit1.ui8Qpos = 0;
					xgSafetyCircuitManagement.xSafetyCircuit1.ui8Q = 0;
					xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrQDelay = 0;
					//wenn cnt hier gelöscht wird, dann ist Nachprellen nicht erlaubt
					//cntTOSkr1 = 0;
				}

				//-----SKR1 Statemachine--------------------------------------------------------------------------
				switch (xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrState) {
				case 0:	//--------------------------------------------------------------
					//Schritt 0, Zustand nach SLOKoff Wechsel von 1 auf 0
					if (xgRuntimeParameters.ui1SafetyCircuitInitialStateCheck1) {	//Grundstellung 0/0 wird nicht verlangt
						switch (ui8TempInputs) {
						case 0b11:
							xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrOut = 1;
							//flQ1 = 1	//Quittierung nicht nach PowerOn setzen
							xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrState = 5;
							break;
						default: //0b00, 0b01, 0b10
							xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrState = 1;
							break;
						}
					} else //_flPowerOnGrSt1 == 0
					{	//Grundstellung 0/0 wird verlangt
						//wenn nicht 0/0, dann ErrGrundstellung
						switch (ui8TempInputs) {
						case 0b00:
							xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrState = 1;
							break;
						default: //01, 10, 11
							xgIpkTx.xSafetyCircuitErrors.ui1ErrorInitialState1 = TRUE;
							//ui8SKRstate1 erst incr. wenn in Grundstellung
							break;
						}
					}
					break;
				case 1:	//--------------------------------------------------------------
					//Schritt 1, _SKR aus, Abfrage Eingänge 00
					switch (ui8TempInputs) {
					case 0b00:
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrState = 2;
						break;
					case 0b11:
						// nach Querschluss würde auch Einkanalerror gesetzt werden
						// Eingänge sind 11, waren aber nicht vorher 00
						if (xgIpkTx.xSafetyCircuitErrors.ui1ErrorCrosscircuit1 == 0) {
							xgIpkTx.xSafetyCircuitErrors.ui1ErrorGeneral1 = TRUE;
						}
						break;
					default: //01, 10, 11
						//nicht tun
						break;
					}
					break;
				case 2:	//--------------------------------------------------------------
					//Schritt 2, Grundstellung, _SKR aus, Abfrage Eingänge 11,  10 oder 01
					switch (ui8TempInputs) {
					case 0b11:
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrTimeout = SK_TIMEOUT;
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrOut = 1;
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrState = 4;
						break;
					case 0b00:
						//nichts tun
						break;
					default: //01, 10
						//cntTOSkr1 = SK_TIMEOUT; //deaktiviert
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrOut = 0;
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrState = 3;
						break;
					}
					break;
				case 3:	//--------------------------------------------------------------
					//Schritt 3, Zustand 10 oder 01
					//bei 10 oder 01 soll der timeout noch nicht ablaufen
					switch (ui8TempInputs) {
					case 0b00:
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrOut = 0;
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrState = 1;
						break;
					case 0b11:
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrOut = 1;
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrTimeout = SK_TIMEOUT;
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrState = 4;
						break;
					default:
						//nichts tun
						break;
					}
					break;
				case 4:	//--------------------------------------------------------------
					//Schritt 4, Zustand 1/1, _SKR = 1, timeout läuft ab
					//einkanaliges Prellen führt noch nicht zum Abschalten
					if (xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrTimeout != 0) {
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrTimeout--;
						switch (ui8TempInputs) {
						case 0b00:
							xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrOut = 0;
							xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrState = 1;
							break;
						default:	//01, 10, 11
							//mindestens 1 Eingang auf 1, abwarten
							break;
						}
					} else	//cnt TOSkr1 == 0
					{
						switch (ui8TempInputs) {
						case 0b11:
							xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrOut = 1;
							xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrState = 5;
							break;
						default:	//00, 01, 10
							xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrOut = 0;
							xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrState = 1;
							break;
						}
					}
					break;
				case 5:	//--------------------------------------------------------------
					//Schritt 5, Zustand 1/1, _SKR = 1, ungleich 1/1 führt zum Abschalten
					switch (ui8TempInputs) {
					case 0b11:
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrOut = 1;
						//ok, abwarten
						break;
					default:
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrOut = 0;
						xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrState = 1;
						break;
					}
					break;
				}
				//-----------------------------------------------------------------
				if (xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrOut == 0)
					xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrQuitt = 0;
				if ((xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrOut == 1) && (xgSafetyCircuitManagement.xSafetyCircuit1.ui8Q == 1))
					xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrQuitt = 1;

				xgIpkTx.xSafetyCircuit.ui1Skr1 = FALSE;

				if (xgSafetyCircuitManagement.xSafetyCircuit1.ui8SkrQuitt == 1)
					xgIpkTx.xSafetyCircuit.ui1Skr1 = TRUE;

			}

			// Safety Circuit 2
			//------------------------------------------------------------
			if (xgRuntimeParameters.ui1SafetyCircuitsActivation2) {
				if (xgRuntimeParameters.ui1SafetyCircuitAntivalent2)
					ui8TempInputs = ((0x08 ^ ui8InputsSafetyCircuit) >> 2) & 0x03;
				else
					ui8TempInputs = (ui8InputsSafetyCircuit >> 2) & 0x03;

				//-----SKR2	Quittierung----------------------------------------------------------------------------
				if (xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrQDelay != 0)
					xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrQDelay--;
				else
					xgSafetyCircuitManagement.xSafetyCircuit2.ui8Q = 0;
				if (xgImkRx.xZmvSafetyCircuitIn.ui1SkrQ2) {		//Can-Q == 1
					if (!xgRuntimeParameters.ui1SafetyCircuitsAcknowledgementDelay2)
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrQDelay = Q_DELAY;	//1=0ms, 0=500ms
					if (!xgRuntimeParameters.ui1SafetyCircuitsAcknowledgementLevel2) {	//quittieren bei high Pegel
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8Q = 1;
					} else //quittieren bei fallender Flanke
					{
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8Qpos = 1;
					}
				} else	//Can-Q == 0
				{
					if (xgSafetyCircuitManagement.xSafetyCircuit2.ui8Qpos == 1) {	//negative Quittier-Flanke
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8Q = 1;
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8Qpos = 0;
					}
				}
				//-----------------------------------------------------------------
				if (!xgRuntimeParameters.ui1SafetyCircuitCrosscircuitsMode2)
					xgSafetyCircuitManagement.xSafetyCircuit2.ui8Q = 1;	//automatisch quittieren
				//-----------------------------------------------------------------
				//bei gesetztem SKR Quittierflags löschen
				if (xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrQuitt == 1) {
					xgSafetyCircuitManagement.xSafetyCircuit2.ui8Qpos = 0;
					xgSafetyCircuitManagement.xSafetyCircuit2.ui8Q = 0;
					xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrQDelay = 0;
					//wenn cnt hier gelöscht wird, dann ist Nachprellen nicht erlaubt
					//cntTOSkr2 = 0;
				}

				//-----SKR2 Statemachine--------------------------------------------------------------------------
				switch (xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrState) {
				case 0:	//--------------------------------------------------------------
					//Schritt 0, Zustand nach SLOKoff Wechsel von 1 auf 0
					if (xgRuntimeParameters.ui1SafetyCircuitInitialStateCheck2) {	//Grundstellung 0/0 wird nicht verlangt
						switch (ui8TempInputs) {
						case 0b11:
							xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrOut = 1;
							xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrState = 5;
							break;
						default: //0b00, 0b01, 0b10
							xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrState = 1;
							break;
						}
					} else //_flPowerOnGrSt2 == 0
					{	//Grundstellung 0/0 wird verlangt
						//wenn nicht 0/0, dann ErrGrundstellung
						switch (ui8TempInputs) {
						case 0b00:
							xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrState = 1;
							break;
						default: //01, 10, 11
							xgIpkTx.xSafetyCircuitErrors.ui1ErrorInitialState2 = TRUE;
							;
							//ui8SKRstate2 erst incr. wenn in Grundstellung
							break;
						}
					}
					break;
				case 1:	//--------------------------------------------------------------
					//Schritt 1, _SKR aus, Abfrage Eingänge 00
					switch (ui8TempInputs) {
					case 0b00:
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrState = 2;
						break;
					case 0b11:
						if (xgIpkTx.xSafetyCircuitErrors.ui1ErrorCrosscircuit2 == 0) {				//nach Querschluss würde auch Einkanalerror gesetzt werden
							//Eingänge sind 11, waren aber nicht vorher 00
							xgIpkTx.xSafetyCircuitErrors.ui1ErrorGeneral2 = TRUE;
						}
						break;
					default: //01, 10, 11
						//nicht tun
						break;
					}
					break;
				case 2:	//--------------------------------------------------------------
					//Schritt 2, Grundstellung, _SKR aus, Abfrage Eingänge 11,  10 oder 01
					switch (ui8TempInputs) {
					case 0b11:
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrTimeout = SK_TIMEOUT;
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrOut = 1;
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrState = 4;
						break;
					case 0b00:
						//nichts tun
						break;
					default: //01, 10
						//cntTOSkr2 = SK_TIMEOUT; //deaktiviert
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrOut = 0;
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrState = 3;
						break;
					}
					break;
				case 3:	//--------------------------------------------------------------
					//Schritt 3, Zustand 10 oder 01
					//bei 10 oder 01 soll der timeout noch nicht ablaufen
					switch (ui8TempInputs) {
					case 0b00:
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrOut = 0;
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrState = 1;
						break;
					case 0b11:
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrOut = 1;
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrTimeout = SK_TIMEOUT;
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrState = 4;
						break;
					default:
						//nichts tun
						break;
					}
					break;
				case 4:	//--------------------------------------------------------------
					//Schritt 4, Zustand 1/1, _SKR = 1, timeout läuft ab
					//einkanaliges Prellen führt noch nicht zum Abschalten
					if (xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrTimeout != 0) {
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrTimeout--;
						switch (ui8TempInputs) {
						case 0b00:
							xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrOut = 0;
							xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrState = 1;
							break;
						default:	//01, 10, 11
							//mindestens 1 Eingang auf 1, abwarten
							break;
						}
					} else	//cnt TOSkr2 == 0
					{
						switch (ui8TempInputs) {
						case 0b11:
							xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrOut = 1;
							xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrState = 5;
							break;
						default:	//00, 01, 10
							xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrOut = 0;
							xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrState = 1;
							break;
						}
					}
					break;
				case 5:	//--------------------------------------------------------------
					//Schritt 5, Zustand 1/1, _SKR = 1, ungleich 1/1 führt zum Abschalten
					switch (ui8TempInputs) {
					case 0b11:
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrOut = 1;
						//ok, abwarten
						break;
					default:
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrOut = 0;
						xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrState = 1;
						break;
					}
					break;
				}
				//-----------------------------------------------------------------
				if (xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrOut == 0)
					xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrQuitt = 0;
				if ((xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrOut == 1) && (xgSafetyCircuitManagement.xSafetyCircuit2.ui8Q == 1))
					xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrQuitt = 1;

				xgIpkTx.xSafetyCircuit.ui1Skr2 = FALSE;
				if (xgSafetyCircuitManagement.xSafetyCircuit2.ui8SkrQuitt == 1)
					xgIpkTx.xSafetyCircuit.ui1Skr2 = TRUE;
			}

		}
	}
}

/**-------------------------------------------------------------------------------------------------
 *    Description: Zweihandschaltung
 *    Frequency:  10ms
 *    Parameters:  *    Parameters: pui8CheckinCounter - Scheduler Counter
 *    Return: None
 *-------------------------------------------------------------------------------------------------*/
void vTwohandCircuitExecute(uint8_t *pui8CheckinCounter) {
	uint8_t ui8Input;
	static uint8_t ui8TwohandState = 0;
	static uint8_t ui8CounterTwoHandTimeout = 0;
	++(*pui8CheckinCounter);

	// Active only system is okay!
	if (!xgImkRx.xHeader.ui1nSlok && xgRuntimeParameters.ui1SafetyTwohandActivation1) {
		ui8Input = xgIpkTx.xInputs.ui4Inputs;         //selected Input
		if (ui8TwohandState > 2)
			ui8TwohandState = 0;
		switch (ui8TwohandState) {
		case 0:
			if (ui8Input == 0x05) {
				xgIpkTx.xSafetyCircuit.ui1Zh = FALSE;
				ui8TwohandState = 1;
			} else {
				// Wait for initial state
				xgIpkTx.xSafetyCircuit.ui1Zh = FALSE;
			}
			break;
		case 1:
			if (ui8Input == 0x05) {
				xgIpkTx.xSafetyCircuit.ui1Zh = FALSE;
			} else if (ui8Input == 0x0A) {
				// Twohand has been pushed
				xgIpkTx.xSafetyCircuit.ui1Zh = TRUE;        //ZH bit0 setzen
				ui8CounterTwoHandTimeout = 0;
				ui8TwohandState = 2;
			} else {
				// Initial state has been left, wait for timeout
				ui8CounterTwoHandTimeout++;
				if (ui8CounterTwoHandTimeout > 51) {
					ui8TwohandState = 0;
					ui8CounterTwoHandTimeout = 0;
					xgIpkTx.xSafetyCircuit.ui1Zh = FALSE;
				}
			}
			break;
		case 2:
			if (ui8Input == 0x0A) {
				; //ok warten
			} else {
				// != 0x0A go back to initial state
				xgIpkTx.xSafetyCircuit.ui1Zh = FALSE;
				ui8TwohandState = 0;
				ui8CounterTwoHandTimeout = 0;
			}
			break;
		}

	} else {
		xgIpkTx.xSafetyCircuit.ui1Zh = FALSE;
		ui8TwohandState = 0;
		ui8CounterTwoHandTimeout = 0;
	}
}

