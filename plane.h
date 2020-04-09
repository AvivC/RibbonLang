#ifndef plane_plane_h
#define plane_plane_h

/* Declaring an interface passed in extension modules */

/* TODO: Need to include all these? Review */
#include "common.h"
#include "vm.h"
#include "memory.h"
#include "plane_object.h"
#include "value.h"
#include "table.h"

typedef struct {
    const char* EXTENSION_ALLOC_STRING_CSTRING;
    const char* EXTENSION_ALLOC_STRING_GC_LEEFS;
    const char* EXTENSION_ALLOC_STRING_MISC;

    void* (*allocate) (size_t size, const char* what);
    void (*deallocate) (void* pointer, size_t oldSize, const char* what);
    void* (*reallocate) (void* pointer, size_t oldSize, size_t newSize, const char* what);

    ObjectFunction* (*object_native_function_new) (NativeFunction nativeFunction, char** parameters, int numParams);
    ObjectFunction* (*make_native_function_with_params) (char* name, int num_params, char** params, NativeFunction function);

    ObjectClass* (*object_class_native_new) (
		char* name, size_t instance_size, DeallocationFunction dealloc_func, GcMarkFunction gc_mark_func, ObjectFunction* constructor);
    ObjectInstance* (*object_instance_new) (ObjectClass* klass);

    void (*object_set_attribute_cstring_key) (Object* object, const char* key, Value value);

    ObjectString* (*object_string_take) (char* chars, int length);
    ObjectString* (*object_string_copy_from_null_terminated) (const char* string);
    ObjectString* (*object_string_clone) (ObjectString* original);

    char* (*copy_null_terminated_cstring) (const char* string, const char* what);
    char* (*copy_cstring) (const char* string, int length, const char* what);

    void (*table_set) (Table* table, struct Value key, Value value);
    bool (*table_get) (Table* table, struct Value key, Value* out);
    void (*table_set_cstring_key) (Table* table, const char* key, Value value);
    bool (*table_get_cstring_key) (Table* table, const char* key, Value* out);
    ObjectTable* (*object_table_new_empty) (void);

    bool (*object_load_attribute) (Object* object, ObjectString* name, Value* out);
    bool (*object_load_attribute_cstring_key) (Object* object, const char* name, Value* out);

    void (*value_array_init) (ValueArray*);
    void (*value_array_write) (ValueArray*, Value*);
    void (*value_array_free) (ValueArray*);

    CallResult (*vm_call_object) (Object* object, ValueArray args, Value* out);
    CallResult (*vm_call_function) (ObjectFunction* function, ValueArray args, Value* out);
    CallResult (*vm_call_bound_method) (ObjectBoundMethod* bound_method, ValueArray args, Value* out);
    CallResult (*vm_instantiate_class) (ObjectClass* klass, ValueArray args, Value* out);

    ImportResult (*vm_import_module) (ObjectString* module_name);
    ImportResult (*vm_import_module_cstring) (char* module_name);
    ObjectModule* (*vm_get_module) (ObjectString* name);
    ObjectModule* (*vm_get_module_cstring) (char* name);
} PlaneApi;

extern PlaneApi API;

typedef bool (*ExtensionInitFunction) (PlaneApi, ObjectModule*);

#endif
