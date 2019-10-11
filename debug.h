#ifndef plane_debug_h
#define plane_debug_h

#include "bytecode.h"
#include "common.h"

void disassembleChunk(Bytecode* chunk);
int disassembleInstruction(OP_CODE opcode, Bytecode* chunk, int offset);

#endif
