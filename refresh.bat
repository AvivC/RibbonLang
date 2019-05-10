@echo off
cls
echo Compiling...
call build.bat
if %errorlevel%==0 call run.bat
