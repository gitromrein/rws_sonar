

/*****************************************************************************************
 * @author: Alexander Malyugin
 * @description:  Kern des Programs. Hier werden alle Laufzeitroutinen augerufen
 *
 *				#Fuses#
 *         		*[BOOTLCK:TRUE], damit fest aus dem Flash gebootet wird
 *         		*[BOR_LEVEL:4]  BOR bei ~2.8V Pegel
 *
 * @revision: V1.00	- 2020-07-01 Hardware komplett in Betrieb genommen, Inputs, Outputs, IPK und 2 SPIs
 * 				    - 2020-07-01 DMA für ADC eingestellt, Frequenzmessung läuft, Error-Struct definiert
 *
 *
 *    		  V1.10
 *       			- 2021-07-07 Umbenennungen und Erweiterungen
 *       			- 2021-07-21 Debouncing von Inputs vereinfacht und verallgemeinert
 *       			- 2021-07-22 Ipk Buffer Swap verfeinert, jetzt werden nicht 20 byte wegen einem byte kopiert
 *       						 und auf Uart-ISR findet das Kopieren auch nicht mehr statt
 *			  V1.x
 *       			- 2021-10-27 Viele Überarbeitungen + Bereinigung.
 *       			- 2022-02-01 Für ROM-Test als CRC-Lösung eingeführt. Sehr elegant und kompakt.
 *       			- 2022-02-09 Eingeführt eine Übersicht über die SRxxxx Funktionen
 *       			- 2022-02-16 Vorberechnung von EMK Schwellen, Validierung einzelner Parameter
 *       						 Austrudelüberwachung komplett entfernt
 *       			- 2022-02-17 Korrektur von Zeichen für Fenstermodus
 *       			- 2022-02-18 Abschaltverzögerung ist auch beim Timeout weiter gültig
 *								 Abschaltverzögerung für Modus 7 nicht wirksam. Dies wäre ein Parameterfehler
 *					- 2022-02-21 Richtungserkennung gefixt.
 *								 Sicherheitskreise zusammengefasst
 *								 Ipk-Struct auf Frametyp erweitert
 *					- 2022-02-22 CAN heisst jetzt Imk, SRS umsortiert, eingeführt Latch und Diagnose
 *								 Anlaufzeit gefixt
 *					- 2022-03-01 Initial Low für initiale Quittierung wurde entfernt
 *								 vCheckQuittStuckat überarbeitet
 *								 Emk-Stati übernommen in RuntimeState
 *					- 2022-03-03 Alle Drehzahlmodi funktionieren!
 *					- 2022-03-07 ADC-Mittelwert wird noch angepasst
 *								 Berechnungen für ADC wurde erweitert.
 *					- 2022-03-09 Performancemessungen mit vDebugTimerWCET1 und vDebugTimerWCET2 möglich
 *					- 2022-03-09 Sicherheitskreise wurden aufgesplittet
 *					- 2022-03-22 vExecuteMainMonitor und vExecuteTimerMonitor überarbeitet
 *					- 2022-03-23 Weitere Anpassungen der Bits für die Reset-Ursache und Überwachung von Main und Timer
 *								 Phasenüberwachung ist jetzt nur noch von einem Parameter abhängig
 *					- 2022-04-14 Neues Konzept für vEvaluateEmk
 *					- 2022-04-21 EMK-Verschärfung, 2 neue Parameter, AdcTimeout in xErrors1
 *					- 2022-04-25 ADC-Fehlertoleranz erhöht entweder 2 oder 3 Messungen. Beim startet ist die Inkrementierung auf 1 immer da,
 *								 bei mancher Hardware kommt es aber bis zu 2
 *					- 2022-04-27 Latch-Trigger angepasst für Modus 7. Ipk_t.xSpeedEmk optimiert
 ****************************************************************************************/
#include "contract.h"

#include "scheduler.h"
#include "pdsv-hardware.h"

#include "pdsv-fdcan.h"
#include "pdsv-parameters.h"
#include "pdsv-ipk.h"
#include "pdsv-memorytest.h"

#include "pdsv-voltage.h"
#include "pdsv-diagnostics.h"
#include "pdsv-inputs.h"

#include "safe/safe-speed.h"
#include "safe/safe-standstill.h"




/* Tabelle mit SRS, die im Programm implementiert wurden
 * Einige Funktionen müssen unabhängig von Parametern laufen
 * damit Sicherheit gewährleistet werden kann.
 * */

const void* pvSafetyRequirementSpecifications[] = {
	vDiagnosticsCheckFirmwareVersion, 				// SR0001
	vParameterCheckPlausibility,					// SR0002
	vParameterCheckDesignerChecksum,				// SR0003
	vParameterCheckCopies,							// SR0004
	vInputCheckPort,								// SR0005
	vIpkCheckTimeout,								// SR0006
	vCanCheckTimeout,								// SR0007
	vAdcCheckVoltage,								// SR0008, SR0009, SR0010
	vInputCheckAsynchronous,						// SR0011
	vAdcCheckDigitalAnalogCoupling,					// SR0012
	vSpeedCheckFrequenciesNotEqual,					// SR0013
	vSpeedCheck1500Hz,								// SR0014
	vAdcCheckWirebreakage,							// SR0015, SR0020
	vSpeedCheckQuittStuckat,						// SR0016
	vEmkCheckSingle,								// SR0017
	vParameterCheckExchangedChecksum,				// SR0018
	vInputCheckDynamic,								// SR0019
	vAdcCheckExtendedWirebreakage,					// SR0021
	vSpeedCheckPhase,								// SR0022
	vDirectionCheckRotationSwitch,					// SR0023
	vRomCheckContinous,	vRomInitialize,				// SR0024
	vRamCheckContinous, vRamInitialize,				// SR0025
	vHardwareCheckTimerMonitor,						// SR0026
	vHardwareCheckMainMonitor,						// SR0027
	vAdcCheckUnit,									// SR0028
	vSchedulerExecute,								// SR0029
	vHardwareCheckUnexpectedInterrupt,				// SR0030
	vHardwareCheckInitialFlags,						// SR0031 - SR0033
	vHardwareCheckCaptureUnit						// SR0034

};

/*---------------------------------------------
 *
 * 			main
 *
 *---------------------------------------------*/
int main(void) {

	// NOTE! Variables are initialized in start.c .bss and .data so there is no need to set them 0
	ASSERT(sizeof(RuntimeParameters_t) == 132);
	ASSERT(sizeof(Ipk_t) == 20);
	ASSERT(sizeof(Imk_t) == 8);

	__disable_irq();							// Disable all interrupts during initialization

	vHardwareInitialize();						// Initialize Hardware Components
	vSchedulerInitialize();						// Initialize Scheduler


	mDebugPinOff;

#ifdef DEBUG_SKIP_SLOT_EXCHANGE
	DEBUGCODE(vLoadDebugParameters(););
#endif

	__enable_irq();								// Enable Interrupts


	//__asm("SVC #0");

	// alt: Gemessene Laufzeit ohne ADC, Port Test und Transistor Test ~100us
	// alt: Gemessene Laufzeit mit eingeschaltener Überwachung ~150us*((uint32_t*) rui8gCanTxBuffer)
	// 2021-08-10: Zeit nach der Umstellung auf neuen Scheduler ~400us ohne Capture und SKR
	// 2022-02-22: Parameterüberwachung und CRC zusammen schließen die Laufzeit über 1.76ms raus
	//-----------------------------------------------------------------------------------------
	while (1) {

		/**/
		// Debug: Measure time
		vDebugTimer1WCET();

		// Highest Priority: Receive Can Frame
		vCanReceiveFrame();

		// Highest Priority: Swap Ipk Buffers
		vIpkSwapBuffers();

		// Execute Assigned Subroutines
		vSchedulerExecute();

		// Check if interrupt is still working, also  the watchdog
		vHardwareCheckMainMonitor();

	}

	return 0;
}

