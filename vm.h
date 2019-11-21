#ifndef plane_vm_h
#define plane_vm_h

#include "bytecode.h"
#include "table.h"
#include "cell_table.h"
#include "object.h"
#include "value.h"

typedef enum {
    INTERPRET_SUCCESS,
    INTERPRET_COMPILER_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

#define STACK_MAX 256
#define CALL_STACK_MAX 256

typedef struct {
	Object* objects;

    ObjectThread* threads;  /* Doubly linked list of threads generally running in the system */
    ObjectThread* current_thread; /* Current executing thread */

    CellTable globals;
    CellTable imported_modules;

    int num_objects;
    int max_objects;
    bool allow_gc;
} VM;

extern VM vm;

void vm_init(void);
void vm_free(void);
InterpretResult vm_interpret(Bytecode* chunk);

void vm_spawn_thread(ObjectFunction* function);

void gc(void);

#endif
