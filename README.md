# The Ribbon programming language

Ribbon is an interpreted dynamic programming language. It's inspired by the likes of Python, Javascript and a dash of Lua.

[Here you can find a 2D game written in Ribbon](src/game).

And here is a Fizzbuzz implementation to get a look and feel for Ribbon:

```Python
Numbers = class {
    @init = {
        self.n = 0
    }

    next = {
        n = self.n
        self.n += 1
        return n
    }
}

fizzbuzz = { | max |
  numbers = Numbers()

  n = numbers.next()
  while n < max {
    if n % 3 == 0 and n % 5 == 0{
      print("fizzbuzz")
    } elsif n % 3 == 0 {
      print("fizz")
    } elsif n % 5 == 0 {
      print("buzz")
    } else {
      print(n)
    }

    n = numbers.next()
  }
}

fizzbuzz(20)
```

### The main traits of Ribbon:

* Everything is an expression - including classes and functions. You can give them names by assigning them to variables
* Full support for closures
* Flexible module system, including support for native C extension modules
* Minimal syntax, no ceremony or boilerplate
* 2D graphics programming supported out of the box via a standard library module (wrapping the native SDL library)
* Comprehensive test suite for the interpreter

### How to install Ribbon:

In order to install Ribbon we first need to build it from source.

After building Ribbon, it can be easily distributed and installed as a self contained directory, containing the interpreter `ribbon.exe` with the standard library `stdlib/` next to it.

#### Requirements to run Ribbon:

* Ribbon currently only runs on Windows 64 bit

#### Requirements to build Ribbon:

* Mingw-w64 GCC for building the interpreter (other compilers may work as well)
* Python 3 for running the test suite of the interpreter. It should be visible on the `PATH` as `python`
* **Optional:** in order to build the included `graphics` standard library module, we need the SDL2 library installed. More on this later

#### Steps to build Ribbon:

1. Clone this repository

2. `CD` to the root directory of the repo

3. Set environment variables required for building the interpreter:
    * Point `RIBBON_BUILD_INCLUDE` to the directory of the C libraries header files (For example: `C:/msys64/mingw64/include`)
    * Point `RIBBON_BUILD_LIB` to the directory of the C libraries binaries (For example: `C:/msys64/mingw64/lib`)

4. Run `build_dev.bat` in order to build in development mode. This turns on several diagnostics, and is necessary for the tests to pass.

        build_dev.bat
  
5. Ribbon comes with a dummy C module which is used in the test suite for the extension system. Build it from the current directory by running

        > src\sample_extension\bdeploy.bat
          
6. Run the test suite for the Ribbon interpreter:

        > test.bat
        
    You should see all of the tests passing.
    
6. Now we can build Ribbon for release. After building, this copies `ribbon.exe` and the adjacent `stdlib` directory, from `src` to a specified output directory. If unspecified, the default is `release\ribbon`.

        > build_release.bat [optional installation directory]
        
7. Now that we have built the interpreter, we can run Ribbon programs like so:

        > ribbon your_program.rib
        
8. **Optional:** Ribbon comes with a standard library `graphics` for 2D graphics programming. It's a wrapper around the SDL2 C library. The steps to build it:
    1. Acquire the SDL2 official binaries: `SDL2.dll` and `SDL2_image.dll`, with their corresponding headers and static libraries.
    2. Place the SDL2 `.a` files under `%RIBBON_BUILD_LIB%` (among the other binaries of the C libraries on your system)
    3. Place the SDL2 header files under `%RIBBON_BUILD_INCLUDE%/SDL2`
    4. During runtime, our `graphics` module needs to find SDL2. Place the `SDL2.dll` and `SDL2_image.dll` under `<ribbon_installation_directory>\stdlib\`. For example, under: `release\ribbon\stdlib\`.
    4. Build the `graphics` module like so:
  
          > src\sdl_extension\bdeploy_optimized.bat
          
    5. You should now see the `graphics.dll` module in the `stdlib` directory.
