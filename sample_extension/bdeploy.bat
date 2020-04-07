@echo off

setlocal EnableDelayedExpansion

pushd %~dp0
call build.bat
if %errorlevel%==0 (
	echo Build successful
	copy myextension.dll ..\stdlib\myextension.dll
	move myextension.dll ..\myuserextension.dll
	popd
) else (
	echo Build failed
)
