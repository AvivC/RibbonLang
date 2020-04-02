#include "plane.h"

PlaneApi API = {
    .allocate = allocate,
    .deallocate = deallocate,
    .reallocate = reallocate,
    .copy_null_terminated_cstring = copy_null_terminated_cstring,
    .object_native_function_new = object_native_function_new,
    .object_set_attribute_cstring_key = object_set_attribute_cstring_key,
    .make_native_function_with_params = make_native_function_with_params,
    .object_class_native_new = object_class_native_new,
    .object_instance_new = object_instance_new
};
