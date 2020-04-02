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
	OBJECT_THREAD,
	OBJECT_CLASS,
	OBJECT_INSTANCE,
	OBJECT_BOUND_METHOD
} ObjectType;

typedef enum {
	METHOD_ACCESS_SUCCESS,
	METHOD_ACCESS_NO_SUCH_ATTR,
	METHOD_ACCESS_ATTR_NOT_FUNCTION
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
} ObjectString;

typedef struct ObjectTable {
    Object base;
    Table table;
} ObjectTable;

typedef struct ObjectCode {
    Object base;
    Bytecode bytecode;
} ObjectCode;

typedef bool __cdecl (*NativeFunction)(ValueArray, Value*);

typedef struct ObjectCell {
	Object base;
	Value value;
	bool is_filled;
} ObjectCell;

typedef struct ObjectFunction {
    Object base;
    char* name;
    char** parameters;
    int num_params;
    bool is_native;
    Object* self;
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

#define THREAD_EVAL_STACK_MAX 255
#define THREAD_CALL_STACK_MAX 255

typedef struct {
	uint8_t* return_address;
	ObjectFunction* function;
	Object* base_entity;
	CellTable local_variables;
	bool is_entity_base;
	bool is_native;
	bool discard_return_value;
} StackFrame;

typedef struct ObjectThread {
	Object base;

	char* name;

	struct ObjectThread* previous_thread;
	struct ObjectThread* next_thread;

	uint8_t* ip;
	ObjectFunction* base_function;

    Value* eval_stack_top;
    Value eval_stack[THREAD_EVAL_STACK_MAX];

    StackFrame* call_stack_top;
    StackFrame call_stack[THREAD_CALL_STACK_MAX];
} ObjectThread;

typedef struct ObjectClass {
	Object base;
	char* name;
	int name_length;
	ObjectFunction* base_function;
	size_t instance_size;
} ObjectClass;

typedef struct ObjectInstance {
	Object base;
	ObjectClass* klass;
} ObjectInstance;

typedef struct ObjectBoundMethod {
	Object base;
	Object* self;
	ObjectFunction* method;
} ObjectBoundMethod;

typedef bool (*NativeFunction) (ValueArray, Value*);

DECLARE_DYNAMIC_ARRAY(ObjectThread*, ThreadArray, thread_array)

ObjectString* object_string_copy(const char* string, int length);
ObjectString* object_string_take(char* chars, int length);
ObjectString* object_string_clone(ObjectString* original);
ObjectString** object_create_copied_strings_array(const char** strings, int num, const char* allocDescription);
ObjectString* object_string_copy_from_null_terminated(const char* string);

ObjectFunction* object_user_function_new(ObjectCode* code, char** parameters, int numParams, Object* self, CellTable free_vars);
ObjectFunction* object_native_function_new(NativeFunction nativeFunction, char** parameters, int numParams, Object* self);
void object_function_set_name(ObjectFunction* function, char* name);
ObjectFunction* make_native_function_with_params(const char* name, int num_params, char** params, NativeFunction function);

ObjectCode* object_code_new(Bytecode chunk);

ObjectTable* object_table_new(Table table);
ObjectTable* object_table_new_empty(void);

ObjectCell* object_cell_new(Value value);
ObjectCell* object_cell_new_empty(void);

ObjectClass* object_class_new(ObjectFunction* base_function, char* name);
ObjectClass* object_class_native_new(char* name, size_t instance_size);
void object_class_set_name(ObjectClass* klass, char* name, int length);

ObjectInstance* object_instance_new(ObjectClass* klass);

ObjectModule* object_module_new(ObjectString* name, ObjectFunction* function);
ObjectModule* object_module_native_new(ObjectString* name, HMODULE dll);

ObjectBoundMethod* object_bound_method_new(ObjectFunction* method, Object* self);

ObjectThread* object_thread_new(ObjectFunction* function, char* name);
void object_thread_push_eval_stack(ObjectThread* thread, Value value);
Value object_thread_pop_eval_stack(ObjectThread* thread);
void object_thread_push_frame(ObjectThread* thread, StackFrame frame);
StackFrame object_thread_pop_frame(ObjectThread* thread);
StackFrame* object_thread_peek_frame(ObjectThread* thread, int offset);

bool object_compare(Object* a, Object* b);

bool object_cstrings_equal(const char* a, const char* b);
bool object_strings_equal(ObjectString* a, ObjectString* b);

void object_free(Object* object);
void object_print(Object* o);
void object_print_all_objects(void);

void object_thread_print(ObjectThread* thread);
void object_thread_print_diagnostic(ObjectThread* thread);

bool object_hash(Object* object, unsigned long* result);

MethodAccessResult object_get_method(Object* object, const char* method_name, ObjectFunction** out);

#define OBJECT_AS_STRING(o) (object_as_string(o))
#define OBJECT_AS_FUNCTION(o) (object_as_function(o))

ObjectString* object_as_string(Object* o);
ObjectFunction* object_as_function(Object* o);

bool object_value_is(Value value, ObjectType type);

void object_set_attribute_cstring_key(Object* object, const char* key, Value value);

bool object_load_attribute(Object* object, ObjectString* name, Value* out);
bool object_load_attribute_cstring_key(Object* object, const char* name, Value* out);

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
