#include <stdio.h>

#include "myextension.h"

static PlaneApi plane;

typedef struct {
    ObjectInstance base;
    char* dynamic_memory;
} ObjectInstanceMyThingA;

typedef struct {
    ObjectInstance base;
    ObjectInstanceMyThingA* a;
} ObjectInstanceMyThingB;

static bool multiply(Object* self, ValueArray args, Value* out) {
    if (args.count != 2) {
        *out = MAKE_VALUE_NIL();
        return false;
    }

    if (args.values[0].type != VALUE_NUMBER || args.values[1].type != VALUE_NUMBER) {
        *out = MAKE_VALUE_NIL();
        return false;
    }

    double num1 = args.values[0].as.number;
    double num2 = args.values[1].as.number;
    *out = MAKE_VALUE_NUMBER(num1 * num2);
    return true;
}

static void my_thing_a_dealloc(ObjectInstance* instance) {
    ObjectInstanceMyThingA* my_thing_a = (ObjectInstanceMyThingA*) instance;
    plane.deallocate(my_thing_a->dynamic_memory, strlen(my_thing_a->dynamic_memory) + 1, plane.EXTENSION_ALLOC_STRING_CSTRING);
}

static void my_thing_b_dealloc(ObjectInstance* instance) {
    /* None. */
}

static bool my_thing_a_init(Object* self, ValueArray args, Value* out) {
    ObjectInstanceMyThingA* a = (ObjectInstanceMyThingA*) self;
    ObjectString* text = (ObjectString*) args.values[0].as.object;
    a->dynamic_memory = plane.copy_cstring(text->chars, text->length, plane.EXTENSION_ALLOC_STRING_CSTRING);
    *out = MAKE_VALUE_NIL();
    return true;
}

static bool my_thing_b_init(Object* self, ValueArray args, Value* out) {
    ObjectInstanceMyThingB* b = (ObjectInstanceMyThingB*) self;
    b->a = (ObjectInstanceMyThingA*) args.values[0].as.object;
    *out = MAKE_VALUE_NIL();
    return true;
}

static Object** my_thing_b_gc_mark(ObjectInstance* instance) {
    ObjectInstanceMyThingB* b = (ObjectInstanceMyThingB*) instance;
    Object** leefs = plane.allocate(sizeof(Object*) * 2, plane.EXTENSION_ALLOC_STRING_GC_LEEFS);
    leefs[0] = (Object*) b->a;
    leefs[1] = NULL;
    return leefs;
}

static bool my_thing_a_get_text(Object* self, ValueArray args, Value* out) {
    ObjectInstanceMyThingA* a = (ObjectInstanceMyThingA*) self;
    *out = MAKE_VALUE_OBJECT(plane.object_string_copy_from_null_terminated(a->dynamic_memory));
    return true;
}

static bool my_thing_b_get_text_multiplied(Object* self, ValueArray args, Value* out) {
    ObjectInstanceMyThingB* b = (ObjectInstanceMyThingB*) self;
    int times = args.values[0].as.number;

    ValueArray get_text_args;
    plane.value_array_init(&get_text_args);
    Value text_out;
    if (!my_thing_a_get_text((Object*) b->a, get_text_args, &text_out)) {
        *out = MAKE_VALUE_NIL();
        return false;
    }
    plane.value_array_free(&get_text_args);

    ObjectString* text = (ObjectString*) text_out.as.object;

    char* buffer = plane.allocate(text->length * times + 1, plane.EXTENSION_ALLOC_STRING_CSTRING);
    for (char* cursor = buffer; cursor < buffer + (text->length * times); cursor += text->length) {
        memcpy(cursor, text->chars, text->length);
    }
    
    buffer[text->length * times] = '\0';

    ObjectString* twice = plane.object_string_take(buffer, strlen(buffer));
    *out = MAKE_VALUE_OBJECT(twice);
    return true;
}

MYEXTENSIONAPI bool plane_module_init(PlaneApi api, ObjectModule* module) {
    plane = api;

    char** multiply_params = api.allocate(sizeof(char*) * 2, "Parameters list cstrings");
    multiply_params[0] = api.copy_null_terminated_cstring("x", "ObjectFunction param cstring");
    multiply_params[1] = api.copy_null_terminated_cstring("y", "ObjectFunction param cstring");

    ObjectFunction* multiply_function = api.object_native_function_new(multiply, multiply_params, 2);
    Value multiply_func_value = MAKE_VALUE_OBJECT(multiply_function);
    api.object_set_attribute_cstring_key((Object*) module, "multiply", multiply_func_value);   

    ObjectFunction* constructor_a = plane.make_native_function_with_params("@init", 1, (char*[]) {"text"}, my_thing_a_init);

    ObjectClass* my_thing_a_class 
            = plane.object_class_native_new("MyThingA", sizeof(ObjectInstanceMyThingA), my_thing_a_dealloc, NULL, constructor_a);

    plane.object_set_attribute_cstring_key((Object*) my_thing_a_class, "get_text", MAKE_VALUE_OBJECT(
        plane.make_native_function_with_params("get_text", 0, NULL, my_thing_a_get_text)
    ));

    ObjectFunction* constructor_b = plane.make_native_function_with_params("@init", 1, (char*[]) {"a"}, my_thing_b_init);
    ObjectClass* my_thing_b_class 
            = plane.object_class_native_new("MyThingB", sizeof(ObjectInstanceMyThingB), my_thing_b_dealloc, my_thing_b_gc_mark, constructor_b);

    plane.object_set_attribute_cstring_key((Object*) my_thing_b_class, "get_text_multiplied", MAKE_VALUE_OBJECT(
        plane.make_native_function_with_params("get_text_multiplied", 1, (char*[]) {"times"}, my_thing_b_get_text_multiplied)
    ));

    plane.object_set_attribute_cstring_key((Object*) module, "MyThingA", MAKE_VALUE_OBJECT(my_thing_a_class));
    plane.object_set_attribute_cstring_key((Object*) module, "MyThingB", MAKE_VALUE_OBJECT(my_thing_b_class));

    return true;
}
