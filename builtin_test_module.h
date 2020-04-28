#ifndef plane_builtin_test_module_h
#define plane_builtin_test_module_h

#include "value.h"
#include "plane_object.h"

bool builtin_test_demo_print(Object* self, ValueArray args, Value* out);
bool builtin_test_call_callback_with_args(Object* self, ValueArray args, Value* out);
bool builtin_test_get_value_directly_from_object_attributes(Object* self, ValueArray args, Value* out);
bool builtin_test_same_object(Object* self, ValueArray args, Value* out);
bool builtin_test_get_object_address(Object* self, ValueArray args, Value* out);
bool builtin_test_gc(Object* self, ValueArray args, Value* out);

#endif
