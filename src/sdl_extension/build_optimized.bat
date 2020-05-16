gcc -DNDEBUG -fPIC -shared -o graphics.dll -Wl,--subsystem,windows -I../ -I%RIBBON_BUILD_INCLUDE%/SDL2 *.c -Wall -Wno-unused -O2 -L%RIBBON_BUILD_LIB% -lmingw32 -lSDL2main -lSDL2 -lSDL2_image
