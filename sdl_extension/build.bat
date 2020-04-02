gcc -shared -o graphics.dll -Wl,--subsystem,windows -I../ -IC:/msys64_new/mingw64/include/SDL2 *.c -g -Wall -Wno-unused -LC:/msys64_new/mingw64/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_image
