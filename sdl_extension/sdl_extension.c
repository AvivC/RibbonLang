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
    char* title;
} ObjectInstanceWindow;

static ObjectInstanceTexture* new_texture(SDL_Texture* texture) {
    ObjectInstanceTexture* instance = (ObjectInstanceTexture*) plane.object_instance_new(texture_class);
    instance->texture = texture;
    return instance;
}

static ObjectInstanceWindow* new_window(SDL_Window* window, char* title) {
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

    char* title = plane.copy_cstring(title_arg->chars, title_arg->length, "SDL_Window title");
    SDL_Window* window = SDL_CreateWindow(title, x_arg, y_arg, w_arg, h_arg, flags_arg);
    // SDL_Window* window = SDL_CreateWindow("xxx", x_arg, y_arg, w_arg, h_arg, flags_arg);
    // ObjectInstanceWindow* instance = new_window(window, title);
    ObjectInstanceWindow* instance = new_window(window, title);

    *out = MAKE_VALUE_OBJECT(instance);
    return true;
}

static void texture_class_deallocate(ObjectInstance* instance) {
    /* object_free in the interpreter takes care of freeing the ObjectInstance using it's klass->instance_size,
    so everything else will be correctly freed. Here we only free the things special for ObjectInstanceTexture. */
    ObjectInstanceTexture* texture = (ObjectInstanceTexture*) instance;
    SDL_DestroyTexture(texture->texture);
}

static void window_class_deallocate(ObjectInstance* instance) {
    ObjectInstanceWindow* window = (ObjectInstanceWindow*) instance;
    SDL_DestroyWindow(window->window);
    plane.deallocate(window->title, strlen(window->title) + 1, "SDL_Window title");
}

__declspec(dllexport) bool plane_module_init(PlaneApi api, ObjectModule* module) {
    plane = api;
    this = module;

    texture_class = api.object_class_native_new("Texture", sizeof(ObjectInstanceTexture), texture_class_deallocate);
    plane.object_set_attribute_cstring_key((Object*) this, "Texture", MAKE_VALUE_OBJECT(texture_class));
    window_class = api.object_class_native_new("Window", sizeof(ObjectInstanceWindow), window_class_deallocate);
    plane.object_set_attribute_cstring_key((Object*) this, "Window", MAKE_VALUE_OBJECT(window_class));

    Value sdl_init_attr = MAKE_VALUE_OBJECT(plane.make_native_function_with_params("init", 1, (char*[]) {"flags"}, sdl_init));
    plane.object_set_attribute_cstring_key((Object*) this, "init", sdl_init_attr);

    Value sdl_create_window_attr = MAKE_VALUE_OBJECT(
        plane.make_native_function_with_params("create_window", 6, (char*[]) {"title", "x", "y", "w", "h", "flags"}, sdl_create_window));
    plane.object_set_attribute_cstring_key((Object*) this, "create_window", sdl_create_window_attr);

    plane.object_set_attribute_cstring_key((Object*) this, "SDL_WINDOWPOS_UNDEFINED", MAKE_VALUE_NUMBER(SDL_WINDOWPOS_UNDEFINED));
    plane.object_set_attribute_cstring_key((Object*) this, "SDL_INIT_VIDEO", MAKE_VALUE_NUMBER(SDL_INIT_VIDEO));

    return true;
}
