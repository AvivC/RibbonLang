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
    InterpretResult func_exec_result = vm_call_function_directly(function, NULL, func_args, &callback_out);
    value_array_free(&func_args);

    printf("I'm a native function\n");

    *out = MAKE_VALUE_NIL();
    return func_exec_result == INTERPRET_SUCCESS;
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
    InterpretResult func_exec_result = vm_call_function_directly(callback, NULL, callback_args, &callback_out);
    
    value_array_free(&callback_args);

    if (func_exec_result == INTERPRET_SUCCESS) {
        *out = callback_out;
        return true;
    }

    *out = MAKE_VALUE_NIL();
    return false;
}