#include "plane.h"

PlaneApi API = {
    .EXTENSION_ALLOC_STRING_CSTRING = "extension cstring",
    .EXTENSION_ALLOC_STRING_GC_LEEFS = "extension gc leefs",
    .EXTENSION_ALLOC_STRING_MISC = "extension memory allocation",
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
    .table_set_cstring_key = table_set_cstring_key,
    .table_get_cstring_key = table_get_cstring_key,
    .object_table_new_empty = object_table_new_empty,
    .object_load_attribute = object_load_attribute,
    .object_load_attribute_cstring_key = object_load_attribute_cstring_key,
    .object_string_take = object_string_take,
    .object_string_copy_from_null_terminated = object_string_copy_from_null_terminated,
    .object_string_clone = object_string_clone,
    .object_string_new_partial_from_null_terminated = object_string_new_partial_from_null_terminated,
    .object_strings_equal = object_strings_equal,
    .cstrings_equal = cstrings_equal,
    .value_array_init = value_array_init,
    .value_array_write = value_array_write,
    .value_array_free = value_array_free,
    .value_array_make = value_array_make,
    .object_value_is = object_value_is,
    .vm_call_object = vm_call_object,
    .vm_call_function = vm_call_function,
    .vm_call_bound_method = vm_call_bound_method,
    .vm_instantiate_class = vm_instantiate_class,
    .vm_instantiate_class_no_args = vm_instantiate_class_no_args,
    .vm_call_attribute = vm_call_attribute,
    .vm_call_attribute_cstring = vm_call_attribute_cstring,
    .vm_import_module = vm_import_module,
    .vm_import_module_cstring = vm_import_module_cstring,
    .vm_get_module = vm_get_module,
    .vm_get_module_cstring = vm_get_module_cstring,
    .vm_push_object = vm_push_object,
    .vm_pop_object = vm_pop_object,
    .is_instance_of_class = is_instance_of_class,
    .is_value_instance_of_class = is_value_instance_of_class,
    .object_make_constructor = object_make_constructor,
    .object_descriptor_new = object_descriptor_new,
    .object_descriptor_new_native = object_descriptor_new_native
};
