#ifndef plane_vm_h
#define plane_vm_h

#include "chunk.h"
#include "table.h"
#include "object.h"
#include "value.h"

typedef enum {
    INTERPRET_SUCCESS,
    INTERPRET_COMPILER_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

#define STACK_MAX 256  // TODO: review this. maybe grow dynamically?

typedef struct {

} StackFrame;

typedef struct {
    uint8_t* ip;
    Chunk* chunk;
    Value* stackTop;
    Value evalStack[STACK_MAX];
    Table globals;
    Object* objects;
} VM;

extern VM vm;

void initVM(Chunk* chunk);
void freeVM();
InterpretResult interpret();

#endif
