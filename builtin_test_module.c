#include "builtin_test_module.h"
#include "common.h"
#include "plane_object.h"
#include "vm.h"

bool builtin_test_demo_print(ValueArray args, Value* out) {
    ObjectFunction* function = NULL;
    if ((function = VALUE_AS_OBJECT(args.values[0], OBJECT_FUNCTION, ObjectFunction)) == NULL) {
        FAIL("builtin_test_demo_print recieved non function as argument.");
    }

    printf("I'm a native function\n");
    ValueArray func_args;
    value_array_init(&func_args);
    vm_call_function_directly(function, func_args);
    printf("I'm a native function\n");

    *out = MAKE_VALUE_NIL();
    return true;
}

bool builtin_test_call_callback_with_args(ValueArray args, Value* out) {
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

    vm_call_function_directly(callback, callback_args);

    *out = MAKE_VALUE_NIL();
    return true;
}