#ifndef plane_debug_h
#define plane_debug_h

#include "common.h"
#include "chunk.h"

void disassembleChunk(Chunk* chunk);
int disassembleInstruction(OP_CODE opcode, Chunk* chunk, int offset);

#endif
