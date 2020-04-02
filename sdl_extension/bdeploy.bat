pushd %~dp0
call build.bat
copy graphics.dll ..\graphics.dll
popd
