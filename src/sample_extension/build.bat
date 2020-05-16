gcc -g -fPIC myextension.c -o myextension.dll -I.. -D MYEXTENSION_EXPORTS -D EXTENSION_1 -shared -Wl,--subsystem,windows

gcc -g -fPIC myextension.c -o my_second_extension.dll -I.. -D MYEXTENSION_EXPORTS -D EXTENSION_2 -shared -Wl,--subsystem,windows
