#ifndef plane_vm_h
#define plane_vm_h

#include "bytecode.h"
#include "table.h"
#include "cell_table.h"
#include "plane_object.h"
#include "value.h"

typedef enum {
    INTERPRET_SUCCESS,
    INTERPRET_COMPILER_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef enum {
    CALL_RESULT_SUCCESS,
    CALL_RESULT_INVALID_ARGUMENT_COUNT,
    CALL_RESULT_CODE_EXECUTION_FAILED,
    CALL_RESULT_NATIVE_EXECUTION_FAILED,
    CALL_RESULT_CLASS_INIT_NOT_METHOD,
    CALL_RESULT_INVALID_CALLABLE
} CallResult;

typedef struct {
	Object* objects;

    ObjectThread* threads;  /* Doubly linked list of threads generally running in the system */
    ObjectThread* current_thread; /* Currently executing thread */
    size_t thread_creation_counter; /* For running-number thread names, used for debugging */
    size_t thread_opcode_counter; /* For thread scheduling */

    CellTable globals;
    CellTable imported_modules;
    CellTable builtin_modules;

    int num_objects;
    int max_objects;
    bool allow_gc;

    /* Used as roots for locating different modules during imports, etc. */
    char* main_module_path;
    char* interpreter_dir_path;
} VM;

extern VM vm;

void vm_init(void);
void vm_free(void);
InterpretResult vm_interpret_frame(StackFrame* frame);
InterpretResult vm_interpret_program(Bytecode* bytecode, char* main_module_path);
InterpretResult vm_call_function_directly(ObjectFunction* function, Object* self, ValueArray args, Value* out);

void vm_spawn_thread(ObjectFunction* function);

void gc(void);

#endif
