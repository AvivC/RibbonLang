#include "chunk.h"
#include "common.h"
#include "value.h"
#include "memory.h"

void initChunk(Chunk* chunk) {
    chunk->capacity = 0;
    chunk->count = 0;
    chunk->code = NULL;
    initValueArray(&chunk->constants);
}

void writeChunk(Chunk* chunk, uint8_t byte) {
    if (chunk->count == chunk->capacity) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = reallocate(chunk->code, oldCapacity, chunk->capacity, "Chunk code array");
        // DEBUG_PRINT("Chunk grew. Old Capacity: %d. New capacity: %d.", oldCapacity , chunk->capacity);
    }
    
    chunk->code[chunk->count++] = byte;
}

void freeChunk(Chunk* chunk) {
    deallocate(chunk->code, chunk->capacity * sizeof(uint8_t), "Chunk code buffer"); // the sizeof is probably stupid
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

int addConstant(Chunk* chunk, Value constant) {
    writeValueArray(&chunk->constants, constant);
    return chunk->constants.count - 1;
}
