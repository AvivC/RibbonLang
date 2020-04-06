#ifndef plane_builtin_test_module_h
#define plane_builtin_test_module_h

#include "value.h"
#include "plane_object.h"

bool builtin_test_demo_print(Object* self, ValueArray args, Value* out);
bool builtin_test_call_callback_with_args(Object* self, ValueArray args, Value* out);

#endif
