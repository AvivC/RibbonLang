@echo off
cls

setlocal EnableDelayedExpansion	

echo Starting Build and Test

echo Building...
call build.bat

if %errorlevel%==0 (
    echo Build successful. Running tests. 
    
    call test.bat
    
    if !errorlevel!==0 (
		echo Tests successful.
	) else (
		echo Tests failed.
	)
) else (
    echo Build failed.
)

