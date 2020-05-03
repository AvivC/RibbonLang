#include "sdl_extension.h"

/* Note: currently no argument validations here. TODO to write helpers in main code for it to be easier to do. */

static PlaneApi plane;
static ObjectModule* this;

static ObjectClass* texture_class;
static ObjectClass* window_class;
static ObjectClass* renderer_class;
static ObjectClass* rect_class;
static ObjectClass* event_class;

static size_t owned_string_counter = 0;
static ObjectTable* owned_strings;

static size_t cached_strings_counter = 0;
static ObjectTable* cached_strings;

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

typedef struct ObjectInstanceRect {
    ObjectInstance base;
    SDL_Rect rect;
} ObjectInstanceRect;

typedef struct ObjectInstanceEvent {
    ObjectInstance base;
    // SDL_Event event;
    int type;
    int scancode;
    int repeat;
} ObjectInstanceEvent;

static bool window_init(Object* self, ValueArray args, Value* out) {
    assert(plane.is_instance_of_class(self, "Window"));
    // if (!plane.is_instance_of_class(self, "Window")) {
    //     FAIL("Window self is not a Window instance.");
    // }

    ObjectInstanceWindow* instance = (ObjectInstanceWindow*) self;

    ObjectString* title_arg = (ObjectString*) (args.values[0].as.object);
    double x_arg = args.values[1].as.number;
    double y_arg = args.values[2].as.number;
    double w_arg = args.values[3].as.number;
    double h_arg = args.values[4].as.number;
    double flags_arg = args.values[5].as.number;

    char* title = plane.copy_cstring(title_arg->chars, title_arg->length, plane.EXTENSION_ALLOC_STRING_CSTRING);
    SDL_Window* window = SDL_CreateWindow(title, x_arg, y_arg, w_arg, h_arg, flags_arg);

    if (window == NULL) {
        plane.deallocate(title, strlen(title) + 1, plane.EXTENSION_ALLOC_STRING_CSTRING);
        *out = MAKE_VALUE_NIL();
        return false;
    }

    instance->window = window;
    instance->title = title;

    *out = MAKE_VALUE_NIL();
    return true;
}

static bool renderer_init(Object* self, ValueArray args, Value* out) {
    assert(plane.is_instance_of_class(self, "Renderer"));
    // if (!plane.is_instance_of_class(self, "Renderer")) {
    //     FAIL("Renderer self is not a Renderer instance.");
    // }

    ObjectInstanceRenderer* instance = (ObjectInstanceRenderer*) self;

    ObjectInstanceWindow* window = (ObjectInstanceWindow*) args.values[0].as.object;
    int index = args.values[1].as.number;
    Uint32 flags = args.values[2].as.number;

    /* Hopefully passing window->window is safe and makes sense? I think it is */
    SDL_Renderer* renderer = SDL_CreateRenderer(window->window, index, flags);

    if (renderer == NULL) {
        *out = MAKE_VALUE_NIL();
        return false;
    }

    instance->window = window;
    instance->renderer = renderer;

    *out = MAKE_VALUE_NIL();
    return true;
}

static bool rect_init(Object* self, ValueArray args, Value* out) {
    ObjectInstanceRect* instance = (ObjectInstanceRect*) self;
    
    double x = args.values[0].as.number;
    double y = args.values[1].as.number;
    double w = args.values[2].as.number;
    double h = args.values[3].as.number;

    SDL_Rect rect = {.x = x, .y = y, .w = w, .h = h};
    instance->rect = rect;

    *out = MAKE_VALUE_NIL(); /* Value is discarded anyway, but has to be pushed */
    return true;
}

static bool rect_descriptor_get(Object* self, ValueArray args, Value* out) {
    Object* object = args.values[0].as.object;

    assert(plane.is_instance_of_class(object, "Rect"));
    // if (!plane.is_instance_of_class(object, "Rect")) {
    //     FAIL("Non Rect passed as first argument to Rect descriptor get");
    // }

    ObjectInstanceRect* rect = (ObjectInstanceRect*) object;

    assert(plane.object_value_is(args.values[1], OBJECT_STRING));
    // if (!plane.object_value_is(args.values[1], OBJECT_STRING)) {
    //     FAIL("Non string passed as second argument to Rect descriptor get");
    // }

    ObjectString* attr_name = (ObjectString*) args.values[1].as.object;

    char* attr_name_cstring = attr_name->chars;
    int attr_name_length = attr_name->length;

    if (plane.cstrings_equal(attr_name_cstring, attr_name_length, "x", 1)) {
        *out = MAKE_VALUE_NUMBER(rect->rect.x);
    } else if (plane.cstrings_equal(attr_name_cstring, attr_name_length, "y", 1)) {
        *out = MAKE_VALUE_NUMBER(rect->rect.y);
    } else if (plane.cstrings_equal(attr_name_cstring, attr_name_length, "w", 1)) {
        *out = MAKE_VALUE_NUMBER(rect->rect.w);
    } else if (plane.cstrings_equal(attr_name_cstring, attr_name_length, "h", 1)) {
        *out = MAKE_VALUE_NUMBER(rect->rect.h);
    } else {
        FAIL("Rect descriptor get method received attr name other than x, y, w, h");
    }

    return true;
}

static bool rect_descriptor_set(Object* self, ValueArray args, Value* out) {
    Object* object = args.values[0].as.object;

    assert(plane.is_instance_of_class(object, "Rect"));
    // if (!plane.is_instance_of_class(object, "Rect")) {
    //     FAIL("Non Rect passed as first argument to Rect descriptor get");
    // }

    ObjectInstanceRect* rect = (ObjectInstanceRect*) object;

    assert(plane.object_value_is(args.values[1], OBJECT_STRING));
    // if (!plane.object_value_is(args.values[1], OBJECT_STRING)) {
    //     FAIL("Non string passed as second argument to Rect descriptor get");
    // }

    assert(args.values[2].type == VALUE_NUMBER);
    // if (args.values[2].type != VALUE_NUMBER) {
    //     /* Temp fail */
    //     FAIL("Non number passed as third argument to Rect descriptor get");
    // }

    ObjectString* attr_name = (ObjectString*) args.values[1].as.object;
    double value = args.values[2].as.number;

    char* attr_name_cstring = attr_name->chars;
    int attr_name_length = attr_name->length;

    if (plane.cstrings_equal(attr_name_cstring, attr_name_length, "x", 1)) {
        rect->rect.x = value;
    } else if (plane.cstrings_equal(attr_name_cstring, attr_name_length, "y", 1)) {
        rect->rect.y = value;
    } else if (plane.cstrings_equal(attr_name_cstring, attr_name_length, "w", 1)) {
        rect->rect.w = value;
    } else if (plane.cstrings_equal(attr_name_cstring, attr_name_length, "h", 1)) {
        rect->rect.h = value;
    } else {
        FAIL("Rect descriptor set method received attr name other than x, y, w, h");
    }

    *out = MAKE_VALUE_NIL();
    return true;
}

static bool event_descriptor_get(Object* self, ValueArray args, Value* out) {
    assert(args.count == 2);

    Object* object = args.values[0].as.object;

    assert(plane.is_instance_of_class(object, "Event"));

    ObjectInstanceEvent* event = (ObjectInstanceEvent*) object;

    ObjectString* attr_name = (ObjectString*) args.values[1].as.object;

    char* attr_name_cstring = attr_name->chars;
    int attr_name_length = attr_name->length;

    if (plane.cstrings_equal(attr_name_cstring, attr_name_length, "type", 4)) {
        *out = MAKE_VALUE_NUMBER(event->type);
    } else if (plane.cstrings_equal(attr_name_cstring, attr_name_length, "scancode", 8)) {
        *out = MAKE_VALUE_NUMBER(event->scancode);
    } else if (plane.cstrings_equal(attr_name_cstring, attr_name_length, "repeat", 6)) {
        *out = MAKE_VALUE_NUMBER(event->repeat);
    } else {
        FAIL("Event descriptor @get method received attr name other than type or scancode");
    }

    return true;
}

static bool event_descriptor_set(Object* self, ValueArray args, Value* out) {
    assert(args.count == 3);

    Object* object = args.values[0].as.object;

    assert(plane.is_instance_of_class(object, "Event"));

    ObjectInstanceEvent* event = (ObjectInstanceEvent*) object;

    ObjectString* attr_name = (ObjectString*) args.values[1].as.object;
    double value = args.values[2].as.number;

    char* attr_name_cstring = attr_name->chars;
    int attr_name_length = attr_name->length;

    if (plane.cstrings_equal(attr_name_cstring, attr_name_length, "type", 4)) {
        event->type = value;
    } else if (plane.cstrings_equal(attr_name_cstring, attr_name_length, "scancode", 8)) {
        event->scancode = value;
    } else if (plane.cstrings_equal(attr_name_cstring, attr_name_length, "repeat", 6)) {
        *out = MAKE_VALUE_NUMBER(event->repeat);
    } else {
        FAIL("Event descriptor @set method received attr name other than type or scancode");
    }

    *out = MAKE_VALUE_NIL();
    return true;
}

static bool texture_init(Object* self, ValueArray args, Value* out) {
    assert(plane.is_instance_of_class(self, "Texture"));
    // if (!plane.is_instance_of_class(self, "Texture")) {
    //     FAIL("texture_init called with none Texture");
    // }

    ObjectInstanceTexture* texture_instance = (ObjectInstanceTexture*) self;

    ObjectInstanceRenderer* renderer = (ObjectInstanceRenderer*) args.values[0].as.object;
    ObjectString* filename = (ObjectString*) args.values[1].as.object;

    /* Cloning makes sure it's null delimited */
    ObjectString* file_null_delimited = plane.object_string_clone(filename);
    plane.table_set(&owned_strings->table, MAKE_VALUE_NUMBER(owned_string_counter++), MAKE_VALUE_OBJECT(file_null_delimited));

    SDL_Texture* texture = IMG_LoadTexture(renderer->renderer, file_null_delimited->chars);
    
    if (texture == NULL) {
        *out = MAKE_VALUE_NIL();
        return false;
    }

    texture_instance->texture = texture;

    *out = MAKE_VALUE_NIL();
    return true;
}

static bool event_init(Object* self, ValueArray args, Value* out) {
    // if (plane.vm_instantiate_class(event_class, args, out) != CALL_RESULT_SUCCESS) {
    //     *out = MAKE_VALUE_NIL();
    //     return true;
    // }

    assert(plane.is_instance_of_class(self, "Event"));

    ObjectInstanceEvent* instance = (ObjectInstanceEvent*) self;

    int type = args.values[0].as.number;
    int scancode = args.values[1].as.number;
    int repeat = args.values[2].as.number; /* Non-zero if it's a key repeat */

    instance->type = type;
    instance->scancode = scancode;
    instance->repeat = repeat;

    *out = MAKE_VALUE_NIL();
    return true;
}

static bool init(Object* self, ValueArray args, Value* out) {
    int flags = args.values[0].as.number;
    double result = SDL_Init(flags);
    *out = MAKE_VALUE_NUMBER(result);
    return true;
}

static bool create_window(Object* self, ValueArray args, Value* out) {
    if (plane.vm_instantiate_class(window_class, args, out) != CALL_RESULT_SUCCESS) {
        *out = MAKE_VALUE_NIL();
        return true;
    }

    return true;
}

static bool set_hint(Object* self, ValueArray args, Value* out) {
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

static bool create_renderer(Object* self, ValueArray args, Value* out) {
    if (plane.vm_instantiate_class(renderer_class, args, out) != CALL_RESULT_SUCCESS) {
        *out = MAKE_VALUE_NIL();
        return true;
    }

    return true;
}

static bool img_init(Object* self, ValueArray args, Value* out) {
    int flags = args.values[0].as.number;
    int result = IMG_Init(flags);
    *out = MAKE_VALUE_NUMBER(result);
    return true;
}

static bool show_cursor(Object* self, ValueArray args, Value* out) {
    int toggle = args.values[0].as.number;
    int result = SDL_ShowCursor(toggle);
    *out = MAKE_VALUE_NUMBER(result);
    return true;
}

static bool destroy_renderer(Object* self, ValueArray args, Value* out) {
    ObjectInstanceRenderer* renderer = (ObjectInstanceRenderer*) args.values[0].as.object;
    if (renderer->renderer != NULL) {
        SDL_DestroyRenderer(renderer->renderer);
        renderer->renderer = NULL;
    }
    *out = MAKE_VALUE_NIL();
    return true;
}

static bool destroy_window(Object* self, ValueArray args, Value* out) {
    ObjectInstanceWindow* window = (ObjectInstanceWindow*) args.values[0].as.object;
    if (window->window != NULL) {
        SDL_DestroyWindow(window->window);
        window->window = NULL;
    }
    *out = MAKE_VALUE_NIL();
    return true;
}

static bool quit(Object* self, ValueArray args, Value* out) {
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

static void rect_class_deallocate(ObjectInstance* instance) {
    /* Currently nothing to do here */
}

static void event_class_deallocate(ObjectInstance* instance) {
    /* Currently nothing to do here */
}

static Object** renderer_class_gc_mark(ObjectInstance* instance) {
    ObjectInstanceRenderer* renderer = (ObjectInstanceRenderer*) instance;
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

static bool log_message(Object* self, ValueArray args, Value* out) {
    int category = args.values[0].as.number;
    int priority = args.values[1].as.number;
    ObjectString* message = (ObjectString*) args.values[2].as.object;

    ObjectString* message_to_print;

    Value cached_val;
    if (plane.table_get(&cached_strings->table, MAKE_VALUE_OBJECT(message), &cached_val)) {
        message_to_print = (ObjectString*) cached_val.as.object;
    } else {
        ObjectString* null_delimited_message = plane.object_string_clone(message);
        message_to_print = null_delimited_message;
        plane.table_set(&cached_strings->table, MAKE_VALUE_OBJECT(null_delimited_message), MAKE_VALUE_OBJECT(null_delimited_message));
    }

    SDL_LogMessage(category, priority, message_to_print->chars);
    *out = MAKE_VALUE_NIL();
    return true;
}

static bool query_texture(Object* self, ValueArray args, Value* out) {
    ObjectInstanceTexture* texture = (ObjectInstanceTexture*) args.values[0].as.object;
    ObjectTable* out_table = (ObjectTable*) args.values[1].as.object;

    Uint32 format;
    int access, width, height;
    int result = SDL_QueryTexture(texture->texture, &format, &access, &width, &height);

    plane.table_set_cstring_key(&out_table->table, "format", MAKE_VALUE_NUMBER(format));
    plane.table_set_cstring_key(&out_table->table, "access", MAKE_VALUE_NUMBER(access));
    plane.table_set_cstring_key(&out_table->table, "w", MAKE_VALUE_NUMBER(width));
    plane.table_set_cstring_key(&out_table->table, "h", MAKE_VALUE_NUMBER(height));

    *out = MAKE_VALUE_NUMBER(result);
    return true;   
}

static bool set_render_draw_color(Object* self, ValueArray args, Value* out) {
    ObjectInstanceRenderer* renderer = (ObjectInstanceRenderer*) args.values[0].as.object;
    int r = args.values[1].as.number;
    int g = args.values[2].as.number;
    int b = args.values[3].as.number;
    int a = args.values[4].as.number;

    *out = MAKE_VALUE_NUMBER(SDL_SetRenderDrawColor(renderer->renderer, r, g, b, a));
    return true;   
}

static bool render_clear(Object* self, ValueArray args, Value* out) {
    ObjectInstanceRenderer* renderer = (ObjectInstanceRenderer*) args.values[0].as.object;
    *out = MAKE_VALUE_NUMBER(SDL_RenderClear(renderer->renderer));
    return true;   
}

static bool render_present(Object* self, ValueArray args, Value* out) {
    ObjectInstanceRenderer* renderer = (ObjectInstanceRenderer*) args.values[0].as.object;
    SDL_RenderPresent(renderer->renderer);
    *out = MAKE_VALUE_NIL();
    return true;   
}

static bool render_copy(Object* self, ValueArray args, Value* out) {
    ObjectInstanceRenderer* renderer = (ObjectInstanceRenderer*) args.values[0].as.object;
    ObjectInstanceTexture* texture = (ObjectInstanceTexture*) args.values[1].as.object;

    SDL_Rect* src_rect;
    if (args.values[2].type == VALUE_NIL) {
        src_rect = NULL;
    } else {
        ObjectInstanceRect* src_rect_instance = (ObjectInstanceRect*) args.values[2].as.object;
        src_rect = &src_rect_instance->rect;
    }

    SDL_Rect* dst_rect;
    if (args.values[3].type == VALUE_NIL) {
        dst_rect = NULL;
    } else {
        ObjectInstanceRect* dst_rect_instance = (ObjectInstanceRect*) args.values[3].as.object;
        dst_rect = &dst_rect_instance->rect;
    }

    *out = MAKE_VALUE_NUMBER(SDL_RenderCopy(renderer->renderer, texture->texture, src_rect, dst_rect));
    return true;   
}

static bool img_load_texture(Object* self, ValueArray args, Value* out) {
    if (plane.vm_instantiate_class(texture_class, args, out) != CALL_RESULT_SUCCESS) {
        FAIL("Temp fail: couldn't instantiate texture class");
    }

    return true;
}

static bool poll_event(Object* self, ValueArray args, Value* out) {
    SDL_Event event;
    int result = SDL_PollEvent(&event);

    assert(result == 0 || result == 1);
    // if (result != 0 && result != 1) {
    //     /* Ummmm.... should we? */
    //     FAIL("SDL_PollEvent returned a non 1 or 0 value: %d", result);
    // }

    int scancode = -1;
    int repeat = -1;
    if (event.type == SDL_KEYUP || event.type == SDL_KEYDOWN) {
        scancode = event.key.keysym.scancode;
        repeat = event.key.repeat;
    }

    if (result == 1) {
        ValueArray event_args = 
            plane.value_array_make(3, (Value[]) {MAKE_VALUE_NUMBER(event.type), MAKE_VALUE_NUMBER(scancode), MAKE_VALUE_NUMBER(repeat)});
        if (plane.vm_instantiate_class(event_class, event_args, out) != CALL_RESULT_SUCCESS) {
            FAIL("Failed to instantiate event class for some reason.");
        }
        plane.value_array_free(&event_args);
    } else {
        *out = MAKE_VALUE_NIL();
    }
    
    return true;
}

static bool get_ticks(Object* self, ValueArray args, Value* out) {
    *out = MAKE_VALUE_NUMBER(SDL_GetTicks());
    return true;
}

static bool delay(Object* self, ValueArray args, Value* out) {
    int ms = args.values[0].as.number;
    SDL_Delay(ms);
    *out = MAKE_VALUE_NIL();
    return true;
}

static bool render_draw_line(Object* self, ValueArray args, Value* out) {
    ObjectInstanceRenderer* renderer = (ObjectInstanceRenderer*) args.values[0].as.object;
    int x1 = args.values[1].as.number;
    int y1 = args.values[2].as.number;
    int x2 = args.values[3].as.number;
    int y2 = args.values[4].as.number;

    *out = MAKE_VALUE_NUMBER(SDL_RenderDrawLine(renderer->renderer, x1, y1, x2, y2));
    return true;
}

static bool set_render_draw_blend_mode(Object* self, ValueArray args, Value* out) {
    ObjectInstanceRenderer* renderer = (ObjectInstanceRenderer*) args.values[0].as.object;
    int blend_mode = args.values[1].as.number;

    *out = MAKE_VALUE_NUMBER(SDL_SetRenderDrawBlendMode(renderer->renderer, blend_mode));
    return true;
}

static bool set_texture_blend_mode(Object* self, ValueArray args, Value* out) {
    ObjectInstanceTexture* texture = (ObjectInstanceTexture*) args.values[0].as.object;
    int blend_mode = args.values[1].as.number;

    *out = MAKE_VALUE_NUMBER(SDL_SetTextureBlendMode(texture->texture, blend_mode));
    return true;
}

static bool set_texture_color_mod(Object* self, ValueArray args, Value* out) {
    ObjectInstanceTexture* texture = (ObjectInstanceTexture*) args.values[0].as.object;
    int r = args.values[1].as.number;
    int g = args.values[2].as.number;
    int b = args.values[3].as.number;

    *out = MAKE_VALUE_NUMBER(SDL_SetTextureColorMod(texture->texture, r, g, b));
    return true;
}

static bool set_texture_alpha_mod(Object* self, ValueArray args, Value* out) {
    ObjectInstanceTexture* texture = (ObjectInstanceTexture*) args.values[0].as.object;
    int alpha = args.values[1].as.number;

    *out = MAKE_VALUE_NUMBER(SDL_SetTextureAlphaMod(texture->texture, alpha));
    return true;
}

static void expose_function(char* name, int num_params, char** params, NativeFunction function) {
    Value value = MAKE_VALUE_OBJECT(plane.make_native_function_with_params(name, num_params, params, function));
    plane.object_set_attribute_cstring_key((Object*) this, name, value);
}


static ObjectClass* expose_class(
    char* name, size_t instance_size, DeallocationFunction dealloc_func,
    GcMarkFunction gc_mark_func, ObjectFunction* init_func, void* descriptors[][2]) {

    ObjectClass* klass = plane.object_class_native_new(name, instance_size, dealloc_func, gc_mark_func, init_func, descriptors);
    plane.object_set_attribute_cstring_key((Object*) this, name, MAKE_VALUE_OBJECT(klass));
    return klass;
}

__declspec(dllexport) bool plane_module_init(PlaneApi api, ObjectModule* module) {
    plane = api;
    this = module;

    /* Init internal stuff */
    
    owned_string_counter = 0;
    owned_strings = plane.object_table_new_empty();
    plane.object_set_attribute_cstring_key((Object*) this, "_owned_strings", MAKE_VALUE_OBJECT(owned_strings));

    cached_strings_counter = 0;
    cached_strings = plane.object_table_new_empty();
    plane.object_set_attribute_cstring_key((Object*) this, "_cached_strings", MAKE_VALUE_OBJECT(cached_strings));

    /* Init and expose classes */

    texture_class = expose_class("Texture", sizeof(ObjectInstanceTexture), texture_class_deallocate, NULL,
                plane.object_make_constructor(2, (char*[]) {"renderer", "filename"}, texture_init), NULL);
    window_class = expose_class("Window", sizeof(ObjectInstanceWindow), window_class_deallocate, NULL, 
                plane.object_make_constructor(6, (char*[]) {"title", "x", "y", "w", "h", "flags"}, window_init), NULL);
    renderer_class = expose_class("Renderer", sizeof(ObjectInstanceRenderer), renderer_class_deallocate, renderer_class_gc_mark,
                plane.object_make_constructor(3, (char*[]) {"window", "index", "flags"}, renderer_init), NULL);

    ObjectInstance* rect_descriptor = plane.object_descriptor_new_native(rect_descriptor_get, rect_descriptor_set);
    void* rect_descriptors[][2] = {
        {"x", rect_descriptor}, {"y", rect_descriptor}, {"w", rect_descriptor}, {"h", rect_descriptor}, {NULL, NULL}
    };

    rect_class = expose_class("Rect", sizeof(ObjectInstanceRect), rect_class_deallocate, NULL, 
                plane.object_make_constructor(4, (char*[]) {"x", "y", "w", "h"}, rect_init), rect_descriptors);

    ObjectInstance* event_descriptor = plane.object_descriptor_new_native(event_descriptor_get, event_descriptor_set);
    void* event_descriptors[][2] = {
        {"type", event_descriptor}, {"scancode", event_descriptor}, {"repeat", event_descriptor}, {NULL, NULL}
    };

    event_class = expose_class("Event", sizeof(ObjectInstanceEvent), event_class_deallocate,
        NULL, plane.object_make_constructor(3, (char*[]) {"type", "scancode", "repeat"}, event_init), event_descriptors);

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
    expose_function("log_message", 3, (char*[]) {"category", "priority", "message"}, log_message);
    expose_function("img_load_texture", 2, (char*[]) {"renderer", "filename"}, img_load_texture);
    expose_function("query_texture", 2, (char*[]) {"texture", "out_table"}, query_texture);
    expose_function("set_render_draw_color", 5, (char*[]) {"renderer", "r", "g", "b", "a"}, set_render_draw_color);
    expose_function("render_clear", 1, (char*[]) {"renderer"}, render_clear);
    expose_function("render_present", 1, (char*[]) {"renderer"}, render_present);
    expose_function("render_copy", 4, (char*[]) {"renderer", "texture", "src_rect", "dst_rect"}, render_copy);
    expose_function("poll_event", 0, NULL, poll_event);
    expose_function("get_ticks", 0, NULL, get_ticks);
    expose_function("delay", 1, (char*[]) {"ms"}, delay);
    expose_function("render_draw_line", 5, (char*[]) {"renderer", "x1", "y1", "x2", "y2"}, render_draw_line);
    expose_function("set_render_draw_blend_mode", 2, (char*[]) {"renderer", "blend_mode"}, set_render_draw_blend_mode);
    expose_function("set_texture_blend_mode", 2, (char*[]) {"texture", "blend_mode"}, set_texture_blend_mode);
    expose_function("set_texture_color_mod", 4, (char*[]) {"texture", "r", "g", "b"}, set_texture_color_mod);
    expose_function("set_texture_alpha_mod", 2, (char*[]) {"texture", "alpha"}, set_texture_alpha_mod);

    /* Init and expose constants */

    plane.object_set_attribute_cstring_key((Object*) this, "SDL_WINDOWPOS_UNDEFINED", MAKE_VALUE_NUMBER(SDL_WINDOWPOS_UNDEFINED));
    plane.object_set_attribute_cstring_key((Object*) this, "SDL_INIT_VIDEO", MAKE_VALUE_NUMBER(SDL_INIT_VIDEO));
    plane.object_set_attribute_cstring_key((Object*) this, 
                        "SDL_HINT_RENDER_SCALE_QUALITY",
                        MAKE_VALUE_OBJECT(plane.object_string_copy_from_null_terminated(SDL_HINT_RENDER_SCALE_QUALITY)));
    plane.object_set_attribute_cstring_key((Object*) this, "SDL_RENDERER_ACCELERATED", MAKE_VALUE_NUMBER(SDL_RENDERER_ACCELERATED));
    plane.object_set_attribute_cstring_key((Object*) this, "IMG_INIT_PNG", MAKE_VALUE_NUMBER(IMG_INIT_PNG));
    plane.object_set_attribute_cstring_key((Object*) this, "IMG_INIT_JPG", MAKE_VALUE_NUMBER(IMG_INIT_JPG));
    plane.object_set_attribute_cstring_key((Object*) this, "SDL_LOG_CATEGORY_APPLICATION", MAKE_VALUE_NUMBER(SDL_LOG_CATEGORY_APPLICATION));
    plane.object_set_attribute_cstring_key((Object*) this, "SDL_LOG_PRIORITY_INFO", MAKE_VALUE_NUMBER(SDL_LOG_PRIORITY_INFO));

    plane.object_set_attribute_cstring_key((Object*) this, "SDL_QUIT", MAKE_VALUE_NUMBER(SDL_QUIT));
    plane.object_set_attribute_cstring_key((Object*) this, "SDL_KEYUP", MAKE_VALUE_NUMBER(SDL_KEYUP));
    plane.object_set_attribute_cstring_key((Object*) this, "SDL_KEYDOWN", MAKE_VALUE_NUMBER(SDL_KEYDOWN));
    plane.object_set_attribute_cstring_key((Object*) this, "SDL_SCANCODE_UP", MAKE_VALUE_NUMBER(SDL_SCANCODE_UP));
    plane.object_set_attribute_cstring_key((Object*) this, "SDL_SCANCODE_DOWN", MAKE_VALUE_NUMBER(SDL_SCANCODE_DOWN));
    plane.object_set_attribute_cstring_key((Object*) this, "SDL_SCANCODE_LEFT", MAKE_VALUE_NUMBER(SDL_SCANCODE_LEFT));
    plane.object_set_attribute_cstring_key((Object*) this, "SDL_SCANCODE_RIGHT", MAKE_VALUE_NUMBER(SDL_SCANCODE_RIGHT));
    plane.object_set_attribute_cstring_key((Object*) this, "SDL_SCANCODE_SPACE", MAKE_VALUE_NUMBER(SDL_SCANCODE_SPACE));

    return true;
}
