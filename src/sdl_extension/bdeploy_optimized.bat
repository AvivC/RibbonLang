@echo off

setlocal EnableDelayedExpansion

pushd %~dp0
call build_optimized.bat
if %errorlevel%==0 (
	echo Build successful
	move graphics.dll ..\stdlib\graphics.dll
	popd
) else (
	echo Build failed
	exit /b -1
)
