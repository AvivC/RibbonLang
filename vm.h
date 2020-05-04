#ifndef plane_vm_h
#define plane_vm_h

#include "bytecode.h"
#include "table.h"
#include "cell_table.h"
#include "plane_object.h"
#include "value.h"

typedef enum {
    CALL_RESULT_SUCCESS,
    CALL_RESULT_INVALID_ARGUMENT_COUNT,
    CALL_RESULT_PLANE_CODE_EXECUTION_FAILED,
    CALL_RESULT_NATIVE_EXECUTION_FAILED,
    CALL_RESULT_CLASS_INIT_NOT_METHOD,
    CALL_RESULT_INVALID_CALLABLE,
    CALL_RESULT_NO_SUCH_ATTRIBUTE
} CallResult;

typedef enum {
    IMPORT_RESULT_SUCCESS,
    IMPORT_RESULT_OPEN_FAILED,
    IMPORT_RESULT_READ_FAILED,
    IMPORT_RESULT_CLOSE_FAILED,
    IMPORT_RESULT_EXTENSION_NO_INIT_FUNCTION,
    IMPORT_RESULT_MODULE_NOT_FOUND
} ImportResult;

#define CALL_STACK_MAX 255
#define EVAL_STACK_MAX 255

typedef struct {
	Object* objects;

    Value stack[EVAL_STACK_MAX];
    Value* stack_top;
    StackFrame call_stack[CALL_STACK_MAX];
    StackFrame* call_stack_top;

    uint8_t* ip;

    CellTable globals;
    CellTable imported_modules;
    CellTable builtin_modules;

    int num_objects;
    int max_objects;
    bool allow_gc;

    Table string_cache;

    /* Used as roots for locating different modules during imports, etc. */
    char* main_module_path;
    char* interpreter_dir_path;
} VM;

extern VM vm;

CallResult vm_call_object(Object* object, ValueArray args, Value* out);
CallResult vm_call_function(ObjectFunction* function, ValueArray args, Value* out);
CallResult vm_call_bound_method(ObjectBoundMethod* bound_method, ValueArray args, Value* out);
CallResult vm_instantiate_class(ObjectClass* klass, ValueArray args, Value* out);
CallResult vm_instantiate_class_no_args(ObjectClass* klass, Value* out);
CallResult vm_call_attribute(Object* object, ObjectString* name, ValueArray args, Value* out);
CallResult vm_call_attribute_cstring(Object* object, char* name, ValueArray args, Value* out);

ImportResult vm_import_module(ObjectString* module_name);
ImportResult vm_import_module_cstring(char* name);

ObjectModule* vm_get_module(ObjectString* name);
ObjectModule* vm_get_module_cstring(char* name);

void vm_push_object(Object* value);
Object* vm_pop_object(void);

void vm_gc(void);

void vm_init(void);
void vm_free(void);
bool vm_interpret_program(Bytecode* bytecode, char* main_module_path);

#endif
