gcc -g -fPIC myextension.c -o myextension.dll -I.. -D MYEXTENSION_EXPORTS -shared -Wl,--subsystem,windows
