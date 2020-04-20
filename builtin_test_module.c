#include "builtin_test_module.h"
#include "common.h"
#include "plane_object.h"
#include "vm.h"

bool builtin_test_demo_print(Object* self, ValueArray args, Value* out) {
    ObjectFunction* function = NULL;
    if ((function = VALUE_AS_OBJECT(args.values[0], OBJECT_FUNCTION, ObjectFunction)) == NULL) {
        FAIL("builtin_test_demo_print recieved non function as argument.");
    }

    printf("I'm a native function\n");

    ValueArray func_args;
    value_array_init(&func_args);
    Value callback_out;
    CallResult func_exec_result = vm_call_object((Object*) function, func_args, &callback_out);
    value_array_free(&func_args);

    printf("I'm a native function\n");

    *out = MAKE_VALUE_NIL();
    return func_exec_result == CALL_RESULT_SUCCESS;
}

bool builtin_test_call_callback_with_args(Object* self, ValueArray args, Value* out) {
    if (args.count != 3) {
        FAIL("builtin_test_call_callback_with_args didn't receive 3 arguments (callback, arg, arg).");
    }

    ObjectFunction* callback = NULL;
    if ((callback = VALUE_AS_OBJECT(args.values[0], OBJECT_FUNCTION, ObjectFunction)) == NULL) {
        FAIL("builtin_test_demo_print_with_args recieved non function as first argument.");
    }

    Value arg1 = args.values[1];
    Value arg2 = args.values[2];

    printf("I'm a native function\n");

    ValueArray callback_args;
    value_array_init(&callback_args);
    value_array_write(&callback_args, &arg1);
    value_array_write(&callback_args, &arg2);

    Value callback_out;
    CallResult func_exec_result = vm_call_object((Object*) callback, callback_args, &callback_out);
    
    value_array_free(&callback_args);

    if (func_exec_result == CALL_RESULT_SUCCESS) {
        *out = callback_out;
        return true;
    }

    *out = MAKE_VALUE_NIL();
    return false;
}

bool builtin_test_get_value_directly_from_object_attributes(Object* self, ValueArray args, Value* out) {
    /* Bypass the attribute lookup mechanism (mainly classes and descriptors), and get
       a value directly from an object's attribute table. Used to test some internals of the system. */

    assert(args.count == 2);
    assert(args.values[0].type == VALUE_OBJECT);
    assert(object_value_is(args.values[1], OBJECT_STRING));

    Object* object = args.values[0].as.object;
    ObjectString* attr_name = (ObjectString*) args.values[1].as.object;

    if (!load_attribute_bypass_descriptors(object, attr_name, out) || is_value_instance_of_class(*out, "Descriptor")) {
        /* Not super elegant, but fine for now. The tests are based on stdout reading anyway. They look for this when appropriate. */
        printf("Attribute not found\n"); 
        *out = MAKE_VALUE_NIL();
        return true;
    }

    return true;
}