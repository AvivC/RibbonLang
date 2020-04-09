gcc -g -fPIC myextension.c -o myextension.dll -I.. -D MYEXTENSION_EXPORTS -shared -Wl,--subsystem,windows

gcc -g -fPIC myextension.c -o my_second_extension.dll -I.. -D MYEXTENSION_EXPORTS -shared -Wl,--subsystem,windows
