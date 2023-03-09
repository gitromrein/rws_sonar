@echo off
SETLOCAL EnableDelayedExpansion

pushd %~dp0
set TOOL_STL="C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
set TOOL_ELFGEN= C:\svn\softwaretools\python\elf-generator\
set TOOL_HEXTOOLS=C:\svn\softwaretools\hex_tools\software\trunk\bin

rem Select base folder
set BUILDS[0]=Debug
set BUILDS[1]=Debug-Noslokoff
set BUILDS[2]=Debug-Skipslot

set ACTIONS[0]=copy
set ACTIONS[1]=flash

set PARAM1=%~1
set PARAM2=%~2

:: Here we initializing an variable named len to calculate length of array
set len_builds=0
:Loop1

:: It will check if the element is defined or not
if defined BUILDS[%len_builds%] ( 
	set /a len_builds+=1
	GOTO :Loop1 
)

set len_acts=0
:Loop2

:: It will check if the element is defined or not
if defined ACTIONS[%len_acts%] ( 
	set /a len_acts+=1
	GOTO :Loop2 
)


if [!PARAM1!] LSS [%len_builds%] (
	set BASE_FOLDER=!BUILDS[%PARAM1%]!

	rem Locations and file names
	set INPUT_ELF=!BASE_FOLDER!/pdsv.elf
	set INPUT_HEX=!BASE_FOLDER!/pdsv.hex
	set OUTPUT_COPY_HEX=!BASE_FOLDER!/pdsv-Rom-Copy.hex
	set OUTPUT_COPY_ELF=!BASE_FOLDER!/pdsv-Rom-Copy.elf
	
	echo ## Selected "!INPUT_ELF!" 
) else ( goto END1 )


if [!PARAM2!] LSS [%len_acts%] (
	set CMD=!ACTIONS[%PARAM2%]!
	echo ## Command "!CMD!"
	
	if [!CMD!]==[!ACTIONS[0]!] (

	  rem Create Elf with copied Rom
	  python %TOOL_ELFGEN%\createElf.py !INPUT_ELF! !OUTPUT_COPY_ELF! 0x10000 0x8000000

	  rem Create hex with copied Rom
	  %TOOL_HEXTOOLS%\replicator_s19hex.exe !INPUT_HEX! !OUTPUT_COPY_HEX! 128 ff 8000000 0010000 8010000 0 0 1 16 1
	)

	if [!CMD!]==[!ACTIONS[1]!] (
	  set INPUT_FILE=!INPUT_ELF!
	  

	  rem Programming STM32F207RBT Using STM32CubeProgrammer: Progam and Verify
	  %TOOL_STL% -c port=SWD -w !INPUT_FILE! -v -rst

	  rem Hex Command Line
	  rem set hex=%OUTPUT_HEX%
	  rem %TOOL_STL% -c port=JTAG -w %hex% -v -rst
	)
) else ( goto END2 )

goto END

:END1
echo ## Available Builds:
for /L %%a IN (0, 1, %len_builds%) DO echo [%%a] !BUILDS[%%a]!


:END2
echo ## Available Actions:
for /L %%a IN (0, 1, %len_acts%) DO echo [%%a] !ACTIONS[%%a]!
goto END
:END
pause
