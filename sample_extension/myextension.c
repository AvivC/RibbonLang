#include <stdio.h>
#include <assert.h>

#include "myextension.h"

// // temp
// #define EXTENSION_1

static PlaneApi plane;
static ObjectModule* this;

typedef struct {
    ObjectInstance base;
    char* dynamic_memory;
    int x;
} ObjectInstanceMyThingA;

typedef struct {
    ObjectInstance base;
    ObjectInstanceMyThingA* a;
} ObjectInstanceMyThingB;

#ifdef EXTENSION_1
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
#endif

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
    a->x = 0;
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

static bool my_thing_a_descriptor_get(Object* self, ValueArray args, Value* out) {
    assert(self == NULL); /* In this case, @get isn't a bound method, just a function tied to an object */
    assert(args.count == 2);
    assert(plane.is_value_instance_of_class(args.values[0], "MyThingA"));
    assert(plane.object_value_is(args.values[1], OBJECT_STRING));

    ObjectInstanceMyThingA* thing = (ObjectInstanceMyThingA*) args.values[0].as.object;
    ObjectString* attr_name = (ObjectString*) args.values[1].as.object;

    assert(plane.cstrings_equal(attr_name->chars, attr_name->length, "x", 1));

    printf("Running MyThingA descriptor @get for attribute x. Value: %d\n", thing->x);
    *out = MAKE_VALUE_NUMBER(thing->x);
    return true;
}

static bool my_thing_a_descriptor_set(Object* self, ValueArray args, Value* out) {
    assert(self == NULL);
    assert(args.count == 3);
    assert(plane.is_value_instance_of_class(args.values[0], "MyThingA"));
    assert(plane.object_value_is(args.values[1], OBJECT_STRING));
    assert(args.values[2].type == VALUE_NUMBER); /* Not the way to do this in real code, but good for testing right now at least */


    ObjectInstanceMyThingA* thing = (ObjectInstanceMyThingA*) args.values[0].as.object;
    ObjectString* attr_name = (ObjectString*) args.values[1].as.object;
    double value = args.values[2].as.number;

    assert(plane.cstrings_equal(attr_name->chars, attr_name->length, "x", 1));

    printf("Running MyThingA descriptor @set for attribute x. Current value: %d. New value: %g\n", thing->x, value);
    thing->x = value;

    *out = MAKE_VALUE_NIL();
    return true;
}

#ifdef EXTENSION_2
static bool square(Object* self, ValueArray args, Value* out) {
    Value number = args.values[0];

    /* First just validating that we don't also have a "multiply" attribute or something,
    to make sure that we actually reach into another extension and nothing funky actually happens here.  */
    Value throwaway;
    bool we_have_multiply_also = plane.object_load_attribute_cstring_key((Object*) this, "multiply", &throwaway);
    if (we_have_multiply_also) {
        printf("We also have the multiply() function, that shouldn't happen.\n");
        *out = MAKE_VALUE_NIL();
        return false;
    }

    printf("Getting other extension.\n");
    ObjectModule* other_extension = plane.vm_get_module_cstring("myuserextension");
    if (other_extension == NULL) {
        printf("Other extension not imported yet. Importing it.\n");
        if (plane.vm_import_module_cstring("myuserextension") == IMPORT_RESULT_SUCCESS) {
            printf("Imported successfully.\n");
        } else {
            printf("Import failed.\n");
            *out = MAKE_VALUE_NIL();
            return false;
        }
        
        printf("Now getting the extension.\n");
        other_extension = plane.vm_get_module_cstring("myuserextension");

        if (other_extension == NULL) {
            printf("The extension is still NULL, and that's really weird and shouldn't happen.\n");
            *out = MAKE_VALUE_NIL();
            return false;
        } else {
            printf("Got the extension successfully.\n");
        }
    }

    printf("Getting the multiply() method from the extension.\n");
    Value multiply_method_val;
    if (!plane.object_load_attribute_cstring_key((Object*) other_extension, "multiply", &multiply_method_val)) {
        printf("Couldn'd get the method. Weird.\n");
        *out = MAKE_VALUE_NIL();
        return false;
    }

    printf("Calling multiply().\n");
    ValueArray multiply_args;
    plane.value_array_init(&multiply_args);
    plane.value_array_write(&multiply_args, &number);
    plane.value_array_write(&multiply_args, &number);
    Value sqr_result;
    plane.vm_call_function((ObjectFunction*) multiply_method_val.as.object, multiply_args, &sqr_result);
    plane.value_array_free(&multiply_args);

    printf("Returned from multiply(). Returning the value.\n");

    *out = sqr_result;
    return true;
}

static bool more_talk_with_other_extension(Object* self, ValueArray args, Value* out) {
    plane.vm_import_module_cstring("myuserextension");
    ObjectModule* other_extension = plane.vm_get_module_cstring("myuserextension");

    assert(other_extension != NULL);
    // if (other_extension == NULL) {
    //     FAIL("Couldn't import myuserextension for some weird reason.");
    // }

    Value mythinga_val;
    if (!plane.object_load_attribute_cstring_key((Object*) other_extension, "MyThingA", &mythinga_val)) {
        FAIL("myuserextension doesn't have a MyThingA attribute.");
    }

    if (!plane.object_value_is(mythinga_val, OBJECT_CLASS)) {
        FAIL("MyThingA of myuserextension isn't a class.");
    }

    ObjectClass* my_thing_a = (ObjectClass*) mythinga_val.as.object;
    ValueArray mythinga_args;
    plane.value_array_init(&mythinga_args);
    plane.value_array_write(&mythinga_args, &MAKE_VALUE_OBJECT(plane.object_string_copy_from_null_terminated("Hyelloooo!")));
    Value instance_val;
    plane.vm_instantiate_class(my_thing_a, mythinga_args, &instance_val);
    plane.value_array_free(&mythinga_args);

    assert(plane.object_value_is(instance_val, OBJECT_INSTANCE));
    // if (!plane.object_value_is(instance_val, OBJECT_INSTANCE)) {
    //     FAIL("Instantiating MyThingA of myuserextension didn't return an instance or something.");
    // }

    ObjectInstance* instance = (ObjectInstance*) instance_val.as.object;

    assert(strncmp(instance->klass->name, "MyThingA", instance->klass->name_length) == 0);
    // if (strncmp(instance->klass->name, "MyThingA", instance->klass->name_length) != 0) {
    //     FAIL("Class of instance is not MyThingA. Actual class name: %.*s", instance->klass->name_length, instance->klass->name);
    // }

    Value get_text_val;
    plane.object_load_attribute_cstring_key((Object*) instance, "get_text", &get_text_val);

    assert(plane.object_value_is(get_text_val, OBJECT_BOUND_METHOD));
    // if (!plane.object_value_is(get_text_val, OBJECT_BOUND_METHOD)) {
    //     FAIL("get_text from MyThingA of myuserextension isn't a BoundMethod.");
    // }

    ObjectBoundMethod* get_text = (ObjectBoundMethod*) get_text_val.as.object;

    ValueArray get_text_args;
    plane.value_array_init(&get_text_args);
    Value get_text_result;
    plane.vm_call_bound_method(get_text, get_text_args, &get_text_result); 
    plane.value_array_free(&get_text_args);
    
    ObjectString* text = (ObjectString*) get_text_result.as.object;
    size_t string_length = snprintf(NULL, 0, "Text received from MyThingA::get_text(): %.*s", text->length, text->chars);
    char* buffer = plane.allocate(string_length + 1, plane.EXTENSION_ALLOC_STRING_CSTRING);
    snprintf(buffer, string_length + 1, "Text received from MyThingA::get_text(): %.*s", text->length, text->chars);
    ObjectString* result_text = plane.object_string_take(buffer, string_length);

    plane.vm_push_object((Object*) result_text);

    ValueArray multiply_args;
    plane.value_array_init(&multiply_args);
    plane.value_array_write(&multiply_args, &MAKE_VALUE_NUMBER(100));
    plane.value_array_write(&multiply_args, &MAKE_VALUE_NUMBER(3.5));
    
    Value multiply_result;
    CallResult multiply_call_result = plane.vm_call_attribute_cstring((Object*) other_extension, "multiply", multiply_args, &multiply_result);
    assert(multiply_call_result == CALL_RESULT_SUCCESS);
    // if (multiply_call_result != CALL_RESULT_SUCCESS) {
    //     FAIL("Calling myuserextension::multiply failed for some reason. Failure code: %d", multiply_call_result);
    // }
    plane.value_array_free(&multiply_args);

    assert(multiply_result.type == VALUE_NUMBER);
    // if (multiply_result.type != VALUE_NUMBER) {
    //     FAIL("multiply() function of myuserextension returned something other than VALUE_NUMBER, and that's weird.");
    // }

    ObjectModule* math_module = plane.vm_get_module_cstring("math"); // The stdlib module written in Plane
    if (math_module == NULL) {
        if (plane.vm_import_module_cstring("math") != IMPORT_RESULT_SUCCESS) {
            FAIL("Importing module math failed.");
        }
        
        math_module = plane.vm_get_module_cstring("math");

        assert(math_module != NULL);
        // if (math_module == NULL) {
        //     FAIL("Module math is still NULL even though we imported it.");
        // }
    } else {
        FAIL("Module math isn't imported yet, should return NULL.");
    }

    ValueArray max_args;
    plane.value_array_init(&max_args);
    plane.value_array_write(&max_args, &MAKE_VALUE_NUMBER(8));
    plane.value_array_write(&max_args, &MAKE_VALUE_NUMBER(5));
    Value max_out;
    plane.vm_call_attribute_cstring((Object*) math_module, "max", max_args, &max_out);
    plane.value_array_free(&max_args);

    ObjectTable* result_table = plane.object_table_new_empty();
    plane.table_set(&result_table->table, 
            MAKE_VALUE_OBJECT(plane.object_string_copy_from_null_terminated("Text")), MAKE_VALUE_OBJECT(result_text));
    plane.table_set(&result_table->table, 
            MAKE_VALUE_OBJECT(plane.object_string_copy_from_null_terminated("Number")), multiply_result);
    plane.table_set(&result_table->table, 
            MAKE_VALUE_OBJECT(plane.object_string_copy_from_null_terminated("MaxResult")), max_out);

    *out = MAKE_VALUE_OBJECT(result_table);

    plane.vm_pop_object();

    return true;
}
#endif

MYEXTENSIONAPI bool plane_module_init(PlaneApi api, ObjectModule* module) {
    plane = api;
    this = module;

    #ifdef EXTENSION_1
    char** multiply_params = api.allocate(sizeof(char*) * 2, "Parameters list cstrings");
    multiply_params[0] = api.copy_null_terminated_cstring("x", "ObjectFunction param cstring");
    multiply_params[1] = api.copy_null_terminated_cstring("y", "ObjectFunction param cstring");
    
    ObjectFunction* multiply_function = api.object_native_function_new(multiply, multiply_params, 2);
    Value multiply_func_value = MAKE_VALUE_OBJECT(multiply_function);
    api.object_set_attribute_cstring_key((Object*) module, "multiply", multiply_func_value);   

    ObjectFunction* constructor_a = plane.make_native_function_with_params("@init", 1, (char*[]) {"text"}, my_thing_a_init);

    ObjectInstance* my_thing_a_descriptor = plane.object_descriptor_new_native(my_thing_a_descriptor_get, my_thing_a_descriptor_set);
    ObjectClass* my_thing_a_class = plane.object_class_native_new(
            "MyThingA", sizeof(ObjectInstanceMyThingA), my_thing_a_dealloc, NULL, constructor_a,
            (void*[][2]) {{"x", my_thing_a_descriptor}, {NULL, NULL}});

    plane.object_set_attribute_cstring_key((Object*) my_thing_a_class, "get_text", MAKE_VALUE_OBJECT(
        plane.make_native_function_with_params("get_text", 0, NULL, my_thing_a_get_text)
    ));
    
    plane.object_set_attribute_cstring_key((Object*) module, "MyThingA", MAKE_VALUE_OBJECT(my_thing_a_class));
    #endif

    #ifdef EXTENSION_2
    ObjectFunction* more_talk_func = api.object_native_function_new(more_talk_with_other_extension, NULL, 0);
    Value more_talk_func_value = MAKE_VALUE_OBJECT(more_talk_func);
    api.object_set_attribute_cstring_key((Object*) module, "more_talk_with_other_extension", more_talk_func_value);   

    char** squared_params = api.allocate(sizeof(char*) * 1, "Parameters list cstrings");
    squared_params[0] = api.copy_null_terminated_cstring("number", "ObjectFunction param cstring");

    ObjectFunction* sqr_function = api.object_native_function_new(square, squared_params, 1);
    Value sqr_func_value = MAKE_VALUE_OBJECT(sqr_function);
    api.object_set_attribute_cstring_key((Object*) module, "square", sqr_func_value);   

    ObjectFunction* constructor_b = plane.make_native_function_with_params("@init", 1, (char*[]) {"a"}, my_thing_b_init);
    ObjectClass* my_thing_b_class 
            = plane.object_class_native_new("MyThingB", sizeof(ObjectInstanceMyThingB), my_thing_b_dealloc, my_thing_b_gc_mark, constructor_b, NULL);

    plane.object_set_attribute_cstring_key((Object*) my_thing_b_class, "get_text_multiplied", MAKE_VALUE_OBJECT(
        plane.make_native_function_with_params("get_text_multiplied", 1, (char*[]) {"times"}, my_thing_b_get_text_multiplied)
    ));

    plane.object_set_attribute_cstring_key((Object*) module, "MyThingB", MAKE_VALUE_OBJECT(my_thing_b_class));
    #endif

    
    return true;
}
