#include "sdl_extension.h"

/* Note: currently no argument validations here. TODO to write helpers in main code for it to be easier to do. */

static PlaneApi plane;
static ObjectModule* this;

static ObjectClass* texture_class;
static ObjectClass* window_class;
static ObjectClass* renderer_class;

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

typedef struct ObjectInstanceRenderer {
    ObjectInstance base;
    SDL_Renderer* renderer;
    ObjectInstanceWindow* window;
} ObjectInstanceRenderer;

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

static ObjectInstanceRenderer* new_renderer(SDL_Renderer* renderer, ObjectInstanceWindow* window) {
    ObjectInstanceRenderer* instance = (ObjectInstanceRenderer*) plane.object_instance_new(renderer_class);
    instance->renderer = renderer;
    instance->window = window;
    return instance;
}

static bool init(ValueArray args, Value* out) {
    int flags = args.values[0].as.number;
    double result = SDL_Init(flags);
    *out = MAKE_VALUE_NUMBER(result);
    return true;
}

static bool create_window(ValueArray args, Value* out) {
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

    // char* title = plane.copy_cstring(title_arg->chars, title_arg->length, "SDL_Window title");
    char* title = plane.copy_cstring(title_arg->chars, title_arg->length, plane.EXTENSION_ALLOC_STRING_CSTRING);
    SDL_Window* window = SDL_CreateWindow(title, x_arg, y_arg, w_arg, h_arg, flags_arg);

    if (window == NULL) {
        // plane.deallocate(title, strlen(title) + 1, "SDL_Window title");
        plane.deallocate(title, strlen(title) + 1, plane.EXTENSION_ALLOC_STRING_CSTRING);
        *out = MAKE_VALUE_NIL();
        return true; /* Right now we don't consider it a fatal failure. We're just a very thin wrapper. */
    }

    ObjectInstanceWindow* instance = new_window(window, title);
    *out = MAKE_VALUE_OBJECT(instance);
    return true;
}

static bool set_hint(ValueArray args, Value* out) {
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

static bool create_renderer(ValueArray args, Value* out) {
    ObjectInstanceWindow* window = (ObjectInstanceWindow*) args.values[0].as.object;
    int index = args.values[1].as.number;
    Uint32 flags = args.values[2].as.number;

    /* Hopefully passing window->window is safe and makes sense? I think it is */
    SDL_Renderer* renderer = SDL_CreateRenderer(window->window, index, flags);

    if (renderer == NULL) {
        *out = MAKE_VALUE_NIL();
        return true; /* Like we said, true on purpose because right now we're just an unopinionated thin layer */
    }

    *out = MAKE_VALUE_OBJECT(new_renderer(renderer, window));
    return true;
}

static bool img_init(ValueArray args, Value* out) {
    int flags = args.values[0].as.number;
    int result = IMG_Init(flags);
    *out = MAKE_VALUE_NUMBER(result);
    return true;
}

static bool show_cursor(ValueArray args, Value* out) {
    int toggle = args.values[0].as.number;
    int result = SDL_ShowCursor(toggle);
    *out = MAKE_VALUE_NUMBER(result);
    return true;
}

static bool destroy_renderer(ValueArray args, Value* out) {
    ObjectInstanceRenderer* renderer = (ObjectInstanceRenderer*) args.values[0].as.object;
    if (renderer->renderer != NULL) {
        SDL_DestroyRenderer(renderer->renderer);
        renderer->renderer = NULL;
    }
    *out = MAKE_VALUE_NIL();
    return true;
}

static bool destroy_window(ValueArray args, Value* out) {
    ObjectInstanceWindow* window = (ObjectInstanceWindow*) args.values[0].as.object;
    if (window->window != NULL) {
        SDL_DestroyWindow(window->window);
        window->window = NULL;
    }
    *out = MAKE_VALUE_NIL();
    return true;
}

static bool quit(ValueArray args, Value* out) {
    SDL_Quit();
    *out = MAKE_VALUE_NIL();
    return true;
}

static void texture_class_deallocate(ObjectInstance* instance) {
    /* object_free in the interpreter takes care of freeing the ObjectInstance using it's klass->instance_size,
    so everything else will be correctly freed. Here we only free the things special for ObjectInstanceTexture. */
    ObjectInstanceTexture* texture = (ObjectInstanceTexture*) instance;
    if (texture->texture != NULL) {
        SDL_DestroyTexture(texture->texture);
        texture->texture = NULL;
    }
}

static Object** renderer_class_gc_mark(ObjectInstance* instance) {
    ObjectInstanceRenderer* renderer = (ObjectInstanceRenderer*) instance;
    // Object** leefs = plane.allocate(sizeof(Object*) * 2, "Native ObjectInstance gc mark leefs");
    Object** leefs = plane.allocate(sizeof(Object*) * 2, plane.EXTENSION_ALLOC_STRING_GC_LEEFS);
    leefs[0] = (Object*) renderer->window;
    leefs[1] = NULL;
    return leefs;
}

static void window_class_deallocate(ObjectInstance* instance) {
    ObjectInstanceWindow* window = (ObjectInstanceWindow*) instance;
    if (window->window != NULL) {
        SDL_DestroyWindow(window->window);
        window->window = NULL;
    }
    // plane.deallocate(window->title, strlen(window->title) + 1, "SDL_Window title");
    plane.deallocate(window->title, strlen(window->title) + 1, plane.EXTENSION_ALLOC_STRING_CSTRING);
}

static void renderer_class_deallocate(ObjectInstance* instance) {
    ObjectInstanceRenderer* renderer = (ObjectInstanceRenderer*) instance;
    if (renderer->renderer != NULL) {
        SDL_DestroyRenderer(renderer->renderer);
        renderer->renderer = NULL;
    }
}

static void expose_function(char* name, int num_params, char** params, NativeFunction function) {
    Value value = MAKE_VALUE_OBJECT(plane.make_native_function_with_params(name, num_params, params, function));
    plane.object_set_attribute_cstring_key((Object*) this, name, value);
}

__declspec(dllexport) bool plane_module_init(PlaneApi api, ObjectModule* module) {
    plane = api;
    this = module;

    /* Init internal stuff */
    
    owned_string_counter = 0;
    owned_strings = plane.object_table_new_empty();
    plane.object_set_attribute_cstring_key((Object*) this, "_owned_strings", MAKE_VALUE_OBJECT(owned_strings));

    /* Init and expose classes */

    texture_class = api.object_class_native_new("Texture", sizeof(ObjectInstanceTexture), texture_class_deallocate, NULL);
    plane.object_set_attribute_cstring_key((Object*) this, "Texture", MAKE_VALUE_OBJECT(texture_class));

    window_class = api.object_class_native_new("Window", sizeof(ObjectInstanceWindow), window_class_deallocate, NULL);
    plane.object_set_attribute_cstring_key((Object*) this, "Window", MAKE_VALUE_OBJECT(window_class));

    renderer_class = api.object_class_native_new("Renderer", sizeof(ObjectInstanceRenderer), renderer_class_deallocate, renderer_class_gc_mark);
    plane.object_set_attribute_cstring_key((Object*) this, "Renderer", MAKE_VALUE_OBJECT(renderer_class));

    /* Init and explose function */

    expose_function("init", 1, (char*[]) {"flags"}, init);
    expose_function("set_hint", 2, (char*[]) {"name", "value"}, set_hint);
    expose_function("create_renderer", 3, (char*[]) {"window", "index", "flags"}, create_renderer);
    expose_function("create_window", 6, (char*[]) {"title", "x", "y", "w", "h", "flags"}, create_window);
    expose_function("img_init", 1, (char*[]) {"flags"}, img_init);
    expose_function("show_cursor", 1, (char*[]) {"toggle"}, show_cursor);
    expose_function("destroy_renderer", 1, (char*[]) {"renderer"}, destroy_renderer);
    expose_function("destroy_window", 1, (char*[]) {"window"}, destroy_window);
    expose_function("quit", 0, NULL, quit);

    /* Init and expose constants */

    plane.object_set_attribute_cstring_key((Object*) this, "SDL_WINDOWPOS_UNDEFINED", MAKE_VALUE_NUMBER(SDL_WINDOWPOS_UNDEFINED));
    plane.object_set_attribute_cstring_key((Object*) this, "SDL_INIT_VIDEO", MAKE_VALUE_NUMBER(SDL_INIT_VIDEO));
    plane.object_set_attribute_cstring_key((Object*) this, 
                        "SDL_HINT_RENDER_SCALE_QUALITY",
                        MAKE_VALUE_OBJECT(plane.object_string_copy_from_null_terminated(SDL_HINT_RENDER_SCALE_QUALITY)));
    plane.object_set_attribute_cstring_key((Object*) this, "SDL_RENDERER_ACCELERATED", MAKE_VALUE_NUMBER(SDL_RENDERER_ACCELERATED));
    plane.object_set_attribute_cstring_key((Object*) this, "IMG_INIT_PNG", MAKE_VALUE_NUMBER(IMG_INIT_PNG));
    plane.object_set_attribute_cstring_key((Object*) this, "IMG_INIT_JPG", MAKE_VALUE_NUMBER(IMG_INIT_JPG));

    return true;
}
