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
#include "value_array.h"

typedef struct {
    const char* EXTENSION_ALLOC_STRING_CSTRING;
    const char* EXTENSION_ALLOC_STRING_GC_LEEFS;
    const char* EXTENSION_ALLOC_STRING_MISC;

    void* (*allocate) (size_t size, const char* what);
    void (*deallocate) (void* pointer, size_t oldSize, const char* what);
    void* (*reallocate) (void* pointer, size_t oldSize, size_t newSize, const char* what);

    ObjectFunction* (*object_native_function_new) (NativeFunction nativeFunction, char** parameters, int numParams);
    ObjectFunction* (*make_native_function_with_params) (char* name, int num_params, char** params, NativeFunction function);

    ObjectClass* (*object_class_native_new) (char* name, size_t instance_size, DeallocationFunction dealloc_func,
                   GcMarkFunction gc_mark_func, ObjectFunction* constructor, void* descriptors[][2]);
    ObjectInstance* (*object_instance_new) (ObjectClass* klass);

    void (*object_set_attribute_cstring_key) (Object* object, const char* key, Value value);

    ObjectString* (*object_string_take) (char* chars, int length);
    ObjectString* (*object_string_copy_from_null_terminated) (const char* string);
    ObjectString* (*object_string_clone) (ObjectString* original);
    bool (*object_strings_equal) (ObjectString* a, ObjectString* b);
    bool (*cstrings_equal) (const char* s1, int length1, const char* s2, int length2);

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
    ValueArray (*value_array_make) (int count, struct Value* values);

    bool (*object_value_is) (Value value, ObjectType type);

    CallResult (*vm_call_object) (Object* object, ValueArray args, Value* out);
    CallResult (*vm_call_function) (ObjectFunction* function, ValueArray args, Value* out);
    CallResult (*vm_call_bound_method) (ObjectBoundMethod* bound_method, ValueArray args, Value* out);
    CallResult (*vm_instantiate_class) (ObjectClass* klass, ValueArray args, Value* out);
    CallResult (*vm_instantiate_class_no_args) (ObjectClass* klass, Value* out);
    CallResult (*vm_call_attribute) (Object* object, ObjectString* name, ValueArray args, Value* out);
    CallResult (*vm_call_attribute_cstring) (Object* object, char* name, ValueArray args, Value* out);

    ImportResult (*vm_import_module) (ObjectString* module_name);
    ImportResult (*vm_import_module_cstring) (char* module_name);
    ObjectModule* (*vm_get_module) (ObjectString* name);
    ObjectModule* (*vm_get_module_cstring) (char* name);

    /* The push and pop functions are exposed to extensions only for one purpose: 
       to mark an object as reachable during GC. Don't use them for any other purpose externally. And don't forget to pop(). */
    void (*vm_push_object) (Object* value);
    Object* (*vm_pop_object) (void);

    bool (*is_instance_of_class) (Object* object, char* klass_name);
    bool (*is_value_instance_of_class) (Value value, char* klass_name);

    ObjectFunction* (*object_make_constructor) (int num_params, char** params, NativeFunction function);

    ObjectInstance* (*object_descriptor_new) (ObjectFunction* get, ObjectFunction* set);
    ObjectInstance* (*object_descriptor_new_native) (NativeFunction get, NativeFunction set);
} PlaneApi;

extern PlaneApi API;

typedef bool (*ExtensionInitFunction) (PlaneApi, ObjectModule*);

#endif
