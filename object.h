#ifndef plane_object_h
#define plane_object_h

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
	OBJECT_CELL
} ObjectType;

typedef enum {
	METHOD_ACCESS_SUCCESS,
	METHOD_ACCESS_NO_SUCH_ATTR,
	METHOD_ACCESS_ATTR_NOT_FUNCTION
} MethodAccessResult;

typedef struct Object {
    ObjectType type;
    struct Object* next;
    Table attributes;
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

typedef bool (*NativeFunction)(ValueArray, Value*);

typedef struct ObjectCell {
	Object base;
	Value value;
} ObjectCell;

typedef struct ObjectFunction {
    Object base;
    char* name;
    char** parameters;
    int num_params;
    bool is_native;
    Object* self;
//    ValueArray upvalues;
//    Table free_vars;
    CellTable free_vars;
    union {
    	NativeFunction native_function;
    	ObjectCode* code;
    };
} ObjectFunction;

ObjectString* object_string_copy(const char* string, int length);
ObjectString* object_string_take(char* chars, int length);
ObjectString** object_create_copied_strings_array(const char** strings, int num, const char* allocDescription);
ObjectString* object_string_copy_from_null_terminated(const char* string);

ObjectFunction* object_user_function_new(ObjectCode* code, char** parameters, int numParams, Object* self, CellTable free_vars);
ObjectFunction* object_native_function_new(NativeFunction nativeFunction, char** parameters, int numParams, Object* self);
void object_function_set_name(ObjectFunction* function, char* name);

ObjectCode* object_code_new(Bytecode chunk);

ObjectTable* object_table_new(Table table);
ObjectTable* object_table_new_empty(void);

ObjectCell* object_cell_new(Value value);

bool object_compare(Object* a, Object* b);

bool object_cstrings_equal(const char* a, const char* b);
bool object_strings_equal(ObjectString* a, ObjectString* b);

void object_free(Object* object);
void object_print(Object* o);
void object_print_all_objects(void);

MethodAccessResult object_get_method(Object* object, const char* method_name, ObjectFunction** out);

#define OBJECT_AS_STRING(o) (object_as_string(o))
#define OBJECT_AS_FUNCTION(o) (object_as_function(o))

ObjectString* object_as_string(Object* o);
ObjectFunction* object_as_function(Object* o);

bool object_is_value_object_of_type(Value value, ObjectType type);

#define VALUE_AS_OBJECT(value, object_type, cast) object_is_value_object_of_type(value, object_type) ? (cast*) value.as.object : NULL

#endif
