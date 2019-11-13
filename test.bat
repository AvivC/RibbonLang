@echo off
REM Intentionally no CLS so we can see compiler warnings at the top
python python\tester\runtests.py
exit /B !err!
