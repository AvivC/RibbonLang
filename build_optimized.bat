gcc -DNDEBUG -I%RIBBON_BUILD_INCLUDE% src/*.c -o src/ribbon.exe -O2 -Wall -Wno-unused -L%RIBBON_BUILD_LIB% -lShlwapi

