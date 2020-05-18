@echo off

setlocal EnableDelayedExpansion

echo Building...

gcc -DNDEBUG -DGC_STRESS_TEST=0 -DMEMORY_DIAGNOSTICS=1 -DDEBUG_IMPORTANT=1 -I%RIBBON_BUILD_INCLUDE% src/*.c -o src/ribbon.exe -O2 -Wall -Wno-unused -L%RIBBON_BUILD_LIB% -lShlwapi

SETLOCAL

if "%~1"=="" (
	set outdir=release\ribbon
) else (
	set outdir=%~1
)

if %errorlevel%==0 (
	echo Build successfull. Copying ribbon.exe and standard library to release directory
	if not exist "%outdir%" mkdir %outdir%
	copy src\ribbon.exe %outdir%\ribbon.exe
	echo D | xcopy src\stdlib %outdir%\stdlib /s /e /y
) else (
	echo Build failed.
)

ENDLOCAL