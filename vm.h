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

// typedef struct {
// 	uint8_t* return_address;
// 	ObjectFunction* function;
// 	ObjectModule* module;
// 	CellTable local_variables;
// 	bool is_module_base;
// } StackFrame;

typedef struct {
    // uint8_t* ip;

	Object* objects;

    // ThreadArray threads;
    ObjectThread* threads;
    ObjectThread* current_thread;

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

void gc(void);

#endif
