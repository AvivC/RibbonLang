#ifndef plane_vm_h
#define plane_vm_h

#include "chunk.h"

typedef enum {
    INTERPRET_SUCCESS,
    INTERPRET_COMPILER_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM(Chunk* chunk);
void freeVM();
InterpretResult interpret();

#endif