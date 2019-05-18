@echo off
cls

echo Starting Build-Test-Run cycle

echo Building...
call build.bat

if %errorlevel%==0 (
    echo Build successful. Running tests. 
    
    call test.bat
    
    if %errorlevel%==0 (
        echo Tests successful. Executing interpreter.
        call run.bat
    ) else (
        echo Tests failed.
    )
) else (
    echo Build failed.
)

