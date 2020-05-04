#ifndef plane_object_h
#define plane_object_h

#include <Windows.h>

#include "bytecode.h"
#include "common.h"
#include "table.h"
#include "value.h"
#include "cell_table.h"

typedef enum {
    OBJECT_STRING,
    OBJECT_FUNCTION,
	OBJECT_CODE,
	OBJECT_TABLE,
	OBJECT_CELL,
	OBJECT_MODULE,
	OBJECT_CLASS,
	OBJECT_INSTANCE,
	OBJECT_BOUND_METHOD
} ObjectType;

typedef enum {
	METHOD_ACCESS_SUCCESS,
	METHOD_ACCESS_NO_SUCH_ATTR,
	METHOD_ACCESS_ATTR_NOT_BOUND_METHOD
} MethodAccessResult;

typedef struct Object {
    ObjectType type;
    struct Object* next;
    CellTable attributes;
    bool is_reachable;
} Object;

typedef struct ObjectString {
    Object base;
    char* chars;
    int length;
	unsigned long hash;
} ObjectString;

typedef struct ObjectTable {
    Object base;
    Table table;
} ObjectTable;

typedef struct ObjectCode {
    Object base;
    Bytecode bytecode;
} ObjectCode;

typedef bool (*NativeFunction)(Object*, ValueArray, Value*);

typedef struct ObjectCell {
	Object base;
	Value value;
	bool is_filled;
} ObjectCell;

typedef struct ObjectFunction {
    Object base;
    char* name;
    // char** parameters;
	ObjectString** parameters;
    int num_params;
    bool is_native;
    CellTable free_vars;
    union {
    	NativeFunction native_function;
    	ObjectCode* code;
    };
} ObjectFunction;

typedef struct ObjectModule {
	Object base;
	ObjectString* name;
	ObjectFunction* function;
	HMODULE dll;
} ObjectModule;

typedef struct {
	uint8_t* return_address;
	ObjectFunction* function;
	Object* base_entity;
	CellTable local_variables;
	bool is_entity_base;
	bool is_native;
	bool discard_return_value;
} StackFrame;

typedef struct ObjectInstance ObjectInstance;
typedef void (*DeallocationFunction)(struct ObjectInstance *);
typedef Object** (*GcMarkFunction)(struct ObjectInstance *);

typedef struct ObjectClass {
	/* TODO: Make distinction between plane and native classes clearer. Different types? Flag? Union? */
	Object base;
	char* name;
	int name_length;
	ObjectFunction* base_function;
	size_t instance_size;
	DeallocationFunction dealloc_func;
	GcMarkFunction gc_mark_func;
} ObjectClass;

typedef struct ObjectInstance {
	Object base;
	ObjectClass* klass;
	bool is_initialized;
} ObjectInstance;

typedef struct ObjectBoundMethod {
	Object base;
	Object* self;
	ObjectFunction* method;
} ObjectBoundMethod;

ObjectString* object_string_copy(const char* string, int length);
ObjectString* object_string_take(char* chars, int length);
ObjectString* object_string_clone(ObjectString* original);
ObjectString** object_create_copied_strings_array(const char** strings, int num, const char* allocDescription);
ObjectString* object_string_copy_from_null_terminated(const char* string);
ObjectString* object_string_new_partial_from_null_terminated(char* chars);

// ObjectFunction* object_user_function_new(ObjectCode* code, char** parameters, int numParams, CellTable free_vars);
ObjectFunction* object_user_function_new(ObjectCode* code, ObjectString** parameters, int numParams, CellTable free_vars);
// ObjectFunction* object_native_function_new(NativeFunction nativeFunction, char** parameters, int numParams);
ObjectFunction* object_native_function_new(NativeFunction nativeFunction, ObjectString** parameters, int numParams);
void object_function_set_name(ObjectFunction* function, char* name);
ObjectFunction* make_native_function_with_params(char* name, int num_params, char** params, NativeFunction function);

ObjectCode* object_code_new(Bytecode chunk);

ObjectTable* object_table_new(Table table);
ObjectTable* object_table_new_empty(void);

ObjectCell* object_cell_new(Value value);
ObjectCell* object_cell_new_empty(void);

ObjectClass* object_class_new(ObjectFunction* base_function, char* name);
ObjectClass* object_class_native_new(
		char* name, size_t instance_size, DeallocationFunction dealloc_func,
		GcMarkFunction gc_mark_func, ObjectFunction* constructor, void* descriptors[][2]);
void object_class_set_name(ObjectClass* klass, char* name, int length);

ObjectInstance* object_instance_new(ObjectClass* klass);

ObjectModule* object_module_new(ObjectString* name, ObjectFunction* function);
ObjectModule* object_module_native_new(ObjectString* name, HMODULE dll);

ObjectBoundMethod* object_bound_method_new(ObjectFunction* method, Object* self);

bool object_compare(Object* a, Object* b);

bool object_strings_equal(ObjectString* a, ObjectString* b);

void object_free(Object* object);
void object_print(Object* o);
void object_print_all_objects(void);

bool object_hash(Object* object, unsigned long* result);

// MethodAccessResult object_get_method(Object* object, const char* method_name, ObjectFunction** out);
MethodAccessResult object_get_method(Object* object, const char* method_name, ObjectBoundMethod** out);

#define OBJECT_AS_STRING(o) (object_as_string(o))
#define OBJECT_AS_FUNCTION(o) (object_as_function(o))

ObjectString* object_as_string(Object* o);
ObjectFunction* object_as_function(Object* o);

bool object_value_is(Value value, ObjectType type);

void object_set_attribute_cstring_key(Object* object, const char* key, Value value);

bool object_load_attribute(Object* object, ObjectString* name, Value* out);
bool object_load_attribute_cstring_key(Object* object, const char* name, Value* out);
bool load_attribute_bypass_descriptors(Object* object, ObjectString* name, Value* out); /* Internal: only external to be used by some tests */

ObjectInstance* object_descriptor_new(ObjectFunction* get, ObjectFunction* set);
ObjectInstance* object_descriptor_new_native(NativeFunction get, NativeFunction set);

bool is_instance_of_class(Object* object, char* klass_name);
bool is_value_instance_of_class(Value value, char* klass_name);

ObjectFunction* object_make_constructor(int num_params, char** params, NativeFunction function);

#define VALUE_AS_OBJECT(value, object_type, cast) object_value_is(value, object_type) ? (cast*) value.as.object : NULL

#define ASSERT_VALUE_AS_OBJECT(variable, value, object_type, cast, error) \
	do { \
		variable = VALUE_AS_OBJECT((value), object_type, cast); \
		if (variable == NULL) { \
			FAIL(error); \
		} \
	} while (false);

#define ASSERT_VALUE_IS_OBJECT(value, object_type, error_message) \
		do { \
			if (!object_value_is(value, object_type)) { \
				FAIL(error_message); \
			} \
		} while (false); \

#endif
