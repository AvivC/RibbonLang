#ifndef plane_chunk_h
#define plane_chunk_h

#include "common.h"
#include "value_array.h"

typedef enum {
    OP_CONSTANT,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NEGATE,
    OP_GREATER_THAN,
    OP_LESS_THAN,
    OP_GREATER_EQUAL,
    OP_LESS_EQUAL,
    OP_EQUAL,
    OP_AND,
    OP_OR,
    OP_LOAD_VARIABLE,
    OP_SET_VARIABLE,
    OP_CALL,
	OP_GET_ATTRIBUTE,
	OP_SET_ATTRIBUTE,
	OP_POP,
	OP_JUMP_IF_FALSE,
	OP_JUMP,
	OP_MAKE_STRING,
	OP_MAKE_FUNCTION,
	OP_NIL,
    OP_RETURN
} OP_CODE;

typedef struct {
    uint8_t* code;
    ValueArray constants;
    int capacity;
    int count;
} Chunk;

void initChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte);
void setChunk(Chunk* chunk, int position, uint8_t byte);
void freeChunk(Chunk* chunk);
int addConstant(Chunk* chunk, struct Value* constant);

#endif
