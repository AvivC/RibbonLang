#ifndef plane_vm_h
#define plane_vm_h

#include "bytecode.h"
#include "table.h"
#include "object.h"
#include "value.h"

typedef enum {
    INTERPRET_SUCCESS,
    INTERPRET_COMPILER_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

#define STACK_MAX 256  // TODO: review this. maybe grow dynamically?
#define CALL_STACK_MAX 256  // TODO: review this. maybe grow dynamically?

typedef struct {
	uint8_t* returnAddress;
	ObjectFunction* function;
	Table local_variables;
} StackFrame;

typedef struct {
    uint8_t* ip;

    Value* stackTop;
    Value evalStack[STACK_MAX];

    StackFrame* call_stack_top;
    StackFrame callStack[CALL_STACK_MAX];

    Table globals;

    Object* objects;
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
