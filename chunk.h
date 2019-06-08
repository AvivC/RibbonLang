#ifndef plane_chunk_h
#define plane_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NEGATE,
    OP_LOAD_VARIABLE,
    OP_SET_VARIABLE,
    OP_CALL,
	OP_POP,
    OP_RETURN
} OP_CODE;

typedef struct {
    uint8_t* code;
    ValueArray constants;
    int capacity;
    int count;
    // TODO: add line numbers
} Chunk;

void initChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte);
void freeChunk(Chunk* chunk);
int addConstant(Chunk* chunk, Value constant);

#endif
