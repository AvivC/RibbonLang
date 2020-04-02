#ifndef plane_plane_h
#define plane_plane_h

/* Interface for extension modules */

/* TODO: Need include all these? Review */
#include "common.h"
#include "memory.h"
#include "plane_object.h"
#include "value.h"
#include "table.h"

typedef struct {
    void* (*allocate) (size_t size, const char* what);
    void (*deallocate) (void* pointer, size_t oldSize, const char* what);
    void* (*reallocate) (void* pointer, size_t oldSize, size_t newSize, const char* what);
    ObjectFunction* (*object_native_function_new) (NativeFunction nativeFunction, char** parameters, int numParams, Object* self);
    void (*object_set_attribute_cstring_key) (Object* object, const char* key, Value value);
    char* (*copy_null_terminated_cstring) (const char* string, const char* what);
    ObjectFunction* (*make_native_function_with_params) (const char* name, int num_params, char** params, NativeFunction function);
    ObjectClass* (*object_class_native_new) (char* name, size_t instance_size);
    ObjectInstance* (*object_instance_new) (ObjectClass* klass);
} PlaneApi;

PlaneApi API;

typedef bool __cdecl (*ExtensionInitFunction) (PlaneApi, ObjectModule*);

#endif
