@echo off
cls

echo Building...
call build.bat

if %errorlevel%==0 (
    echo Build successful. Running dry run.
    
    call dry_run.bat
) else (
    echo Build failed.
)