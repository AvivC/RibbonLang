@echo off
cls

setlocal EnableDelayedExpansion	

echo Starting Build and Run

echo Building...
call build.bat

if %errorlevel%==0 (
    echo Build successful. Running. 
    call run.bat
) else (
    echo Build failed.
)

