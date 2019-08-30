#include "chunk.h"
#include "common.h"
#include "memory.h"
#include "value.h"

void initChunk(Chunk* chunk) {
    chunk->capacity = 0;
    chunk->count = 0;
    chunk->code = NULL;
    value_array_init(&chunk->constants);
    integer_array_init(&chunk->referenced_names_indices);
}

void writeChunk(Chunk* chunk, uint8_t byte) {
    if (chunk->count == chunk->capacity) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = reallocate(chunk->code, oldCapacity, chunk->capacity, "Chunk code buffer");
    }
    
    chunk->code[chunk->count++] = byte;
}

void setChunk(Chunk* chunk, int position, uint8_t byte) {
	if (position >= chunk->count || position < 0) {
		FAIL("Position out of bounds for chunk writing.");
	}

	chunk->code[position] = byte;
}

void freeChunk(Chunk* chunk) {
    deallocate(chunk->code, chunk->capacity * sizeof(uint8_t), "Chunk code buffer"); // the sizeof is probably stupid
    value_array_free(&chunk->constants);
    integer_array_free(&chunk->referenced_names_indices);
    initChunk(chunk);
}

int addConstant(Chunk* chunk, struct Value* constant) {
	value_array_write(&chunk->constants, constant);
    return chunk->constants.count - 1;
}

void chunk_print_constant_table(Chunk* chunk) { // For debugging
	printf("\nConstant table of chunk pointing at '%p':\n", chunk->code);
	for (int i = 0; i < chunk->constants.count; i++) {
		Value constant = chunk->constants.values[i];
		printf("%d: ", i);
		printValue(constant);
		printf(" [ type %d ]", constant.type);
		printf("\n");
	}
}
