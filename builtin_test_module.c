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
    vm_call_function_directly(function);
    printf("I'm a native function\n");

    *out = MAKE_VALUE_NIL();
    return true;
}