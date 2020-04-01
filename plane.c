#include "plane.h"

PlaneApi API = {
    .allocate = allocate,
    .deallocate = deallocate,
    .reallocate = reallocate,
    .copy_null_terminated_cstring = copy_null_terminated_cstring,
    .object_native_function_new = object_native_function_new,
    .object_set_attribute_cstring_key = object_set_attribute_cstring_key
};

// PlaneApi plane_api_instance(void) {
//     PlaneApi api;

//     api.allocate = allocate;
//     api.deallocate = deallocate;
//     api.reallocate = reallocate;
//     api.copy_null_terminated_cstring = copy_null_terminated_cstring;
//     api.object_native_function_new = object_native_function_new;
//     api.object_set_attribute_cstring_key = object_set_attribute_cstring_key;

//     return api;
// }