#include "sdl_extension.h"

/* Note: currently no argument validations here. TODO to write helpers in main code for it to be easier to do. */

static PlaneApi plane;
static ObjectModule* this;

static ObjectClass* texture_class;
static ObjectClass* window_class;

typedef struct ObjectInstanceTexture {
    ObjectInstance base;
    SDL_Texture* texture;
} ObjectInstanceTexture;

typedef struct ObjectInstanceWindow {
    ObjectInstance base;
    SDL_Window* window;
    ObjectString* title;
} ObjectInstanceWindow;

static ObjectInstanceTexture* new_texture(SDL_Texture* texture) {
    ObjectInstanceTexture* instance = (ObjectInstanceTexture*) plane.object_instance_new(texture_class);
    instance->texture = texture;
    return instance;
}

static ObjectInstanceWindow* new_window(SDL_Window* window, ObjectString* title) {
    ObjectInstanceWindow* instance = (ObjectInstanceWindow*) plane.object_instance_new(window_class);
    instance->window = window;
    instance->title = title;
    return instance;
}

static bool sdl_init(ValueArray args, Value* out) {
    int flags = args.values[0].as.number;
    bool result = SDL_Init(flags);
    *out = MAKE_VALUE_BOOLEAN(result);
    return true;
}

static bool sdl_create_window(ValueArray args, Value* out) {
    ObjectString* title_arg = (ObjectString*) (args.values[0].as.object);
    double x_arg = args.values[1].as.number;
    double y_arg = args.values[2].as.number;
    double w_arg = args.values[3].as.number;
    double h_arg = args.values[4].as.number;
    double flags_arg = args.values[5].as.number;

    SDL_Window* window = SDL_CreateWindow("Shooter 01", x_arg, y_arg, w_arg, h_arg, flags_arg);
    ObjectInstanceWindow* instance = new_window(window, title_arg);

    *out = MAKE_VALUE_OBJECT(instance);
    return true;
}

__declspec(dllexport) bool plane_module_init(PlaneApi api, ObjectModule* module) {
    plane = api;
    this = module;

    texture_class = api.object_class_native_new("Texture", sizeof(ObjectInstanceTexture));
    window_class = api.object_class_native_new("Window", sizeof(ObjectInstanceWindow));

    Value sdl_init_attr = MAKE_VALUE_OBJECT(plane.make_native_function_with_params("init", 1, (char*[]) {"flags"}, sdl_init));
    plane.object_set_attribute_cstring_key((Object*) this, "init", sdl_init_attr);

    plane.object_set_attribute_cstring_key((Object*) this, "SDL_WINDOWPOS_UNDEFINED", MAKE_VALUE_NUMBER(SDL_WINDOWPOS_UNDEFINED));

    return true;
}
