#ifndef sdl_extension_sdl_extension_h
#define sdl_extension_sdl_extension_h

#include "plane.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_image.h"

__declspec(dllexport) bool plane_module_init(PlaneApi api, ObjectModule* module);

#endif