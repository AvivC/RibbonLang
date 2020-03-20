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
} VM;

extern VM vm;

void vm_init(void);
void vm_free(void);
InterpretResult vm_interpret_frame(StackFrame* frame);
InterpretResult vm_interpret_program(Bytecode* bytecode);
InterpretResult vm_call_function_directly(ObjectFunction* function, ValueArray args, Value* out);

void vm_spawn_thread(ObjectFunction* function);

void gc(void);

#endif
