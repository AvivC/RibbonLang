#ifndef sdl_extension_sdl_extension_h
#define sdl_extension_sdl_extension_h

#include "ribbon_api.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_image.h"

__declspec(dllexport) bool ribbon_module_init(RibbonApi api, ObjectModule* module);

#endif