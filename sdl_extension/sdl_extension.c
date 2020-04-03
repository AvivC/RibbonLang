#include "sdl_extension.h"

/* Note: currently no argument validations here. TODO to write helpers in main code for it to be easier to do. */

static PlaneApi plane;
static ObjectModule* this;

static ObjectClass* texture_class;
static ObjectClass* window_class;

size_t owned_string_counter = 0;
static ObjectTable* owned_strings;

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
    double result = SDL_Init(flags);
    *out = MAKE_VALUE_NUMBER(result);
    return true;
}

static bool sdl_create_window(ValueArray args, Value* out) {
    ObjectString* title_arg = (ObjectString*) (args.values[0].as.object);
    double x_arg = args.values[1].as.number;
    double y_arg = args.values[2].as.number;
    double w_arg = args.values[3].as.number;
    double h_arg = args.values[4].as.number;
    double flags_arg = args.values[5].as.number;

    /* TODO: Is this really safe? We assume that copying the string and giving ownership
    to the ObjectInstanceWindow is safe - as long as it's alive, the string will be alive.
    However, who says internally SDL doesn't distribute this string to more things, assuming
    it's a literal?
    Anyway it seems unlikely, so for now leave it like this. Later should consider a different approach. */

    char* title = plane.copy_cstring(title_arg->chars, title_arg->length, "SDL_Window title");
    SDL_Window* window = SDL_CreateWindow(title, x_arg, y_arg, w_arg, h_arg, flags_arg);

    if (window == NULL) {
        plane.deallocate(title, strlen(title) + 1, "SDL_Window title");
        *out = MAKE_VALUE_NIL();
        return true; /* Right now we don't consider it a fatal failure. We're just a very thin wrapper. */
    }

    ObjectInstanceWindow* instance = new_window(window, title);
    *out = MAKE_VALUE_OBJECT(instance);
    return true;
}

static bool sdl_set_hint(ValueArray args, Value* out) {
    ObjectString* name_arg = (ObjectString*) args.values[0].as.object;
    ObjectString* value_arg = (ObjectString*) args.values[1].as.object;

    /* cloning makes them null delimited */
    ObjectString* name = plane.object_string_clone(name_arg);
    ObjectString* value = plane.object_string_clone(value_arg);

    plane.table_set(&owned_strings->table, MAKE_VALUE_NUMBER(owned_string_counter++), MAKE_VALUE_OBJECT(name));
    plane.table_set(&owned_strings->table, MAKE_VALUE_NUMBER(owned_string_counter++), MAKE_VALUE_OBJECT(value));

    bool result = SDL_SetHint(name->chars, value->chars);
    *out = MAKE_VALUE_BOOLEAN(result);
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

    /* Init internal stuff */
    
    owned_string_counter = 0;
    owned_strings = plane.object_table_new_empty();
    plane.object_set_attribute_cstring_key((Object*) this, "_owned_strings", MAKE_VALUE_OBJECT(owned_strings));

    /* Init and expose classes */

    texture_class = api.object_class_native_new("Texture", sizeof(ObjectInstanceTexture), texture_class_deallocate);
    plane.object_set_attribute_cstring_key((Object*) this, "Texture", MAKE_VALUE_OBJECT(texture_class));
    window_class = api.object_class_native_new("Window", sizeof(ObjectInstanceWindow), window_class_deallocate);
    plane.object_set_attribute_cstring_key((Object*) this, "Window", MAKE_VALUE_OBJECT(window_class));

    /* Init and explose function */

    Value sdl_init_attr = MAKE_VALUE_OBJECT(plane.make_native_function_with_params("init", 1, (char*[]) {"flags"}, sdl_init));
    plane.object_set_attribute_cstring_key((Object*) this, "init", sdl_init_attr);

    Value sdl_set_hint_attr = MAKE_VALUE_OBJECT(plane.make_native_function_with_params("set_hint", 2, (char*[]) {"name", "value"}, sdl_set_hint));
    plane.object_set_attribute_cstring_key((Object*) this, "set_hint", sdl_set_hint_attr);

    Value sdl_create_window_attr = MAKE_VALUE_OBJECT(
        plane.make_native_function_with_params("create_window", 6, (char*[]) {"title", "x", "y", "w", "h", "flags"}, sdl_create_window));
    plane.object_set_attribute_cstring_key((Object*) this, "create_window", sdl_create_window_attr);

    /* Init and expose constants */

    plane.object_set_attribute_cstring_key((Object*) this, "SDL_WINDOWPOS_UNDEFINED", MAKE_VALUE_NUMBER(SDL_WINDOWPOS_UNDEFINED));
    plane.object_set_attribute_cstring_key((Object*) this, "SDL_INIT_VIDEO", MAKE_VALUE_NUMBER(SDL_INIT_VIDEO));
    plane.object_set_attribute_cstring_key((Object*) this, 
                        "SDL_HINT_RENDER_SCALE_QUALITY",
                        MAKE_VALUE_OBJECT(plane.object_string_copy_from_null_terminated(SDL_HINT_RENDER_SCALE_QUALITY)));

    return true;
}
