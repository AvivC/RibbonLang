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
    .object_instance_new = object_instance_new,
    .copy_cstring = copy_cstring,
    .table_set = table_set,
    .table_get = table_get,
    .object_table_new_empty = object_table_new_empty,
    .object_string_take = object_string_take,
    .object_string_copy_from_null_terminated = object_string_copy_from_null_terminated,
    .object_string_clone = object_string_clone,
    .EXTENSION_ALLOC_STRING_CSTRING = "extension cstring",
    .EXTENSION_ALLOC_STRING_GC_LEEFS = "extension gc leefs",
    .EXTENSION_ALLOC_STRING_MISC = "extension memory allocation"
};
