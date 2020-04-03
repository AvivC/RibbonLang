pushd %~dp0
call build.bat
copy graphics.dll ..\stdlib\graphics.dll
popd
