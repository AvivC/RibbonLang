#include "bytecode.h"
#include "common.h"
#include "memory.h"
#include "value.h"

void bytecode_init(Bytecode* chunk) {
    chunk->capacity = 0;
    chunk->count = 0;
    chunk->code = NULL;
    value_array_init(&chunk->constants);
    integer_array_init(&chunk->referenced_names_indices);
    integer_array_init(&chunk->assigned_names_indices);
}

void bytecode_write(Bytecode* chunk, uint8_t byte) {
    if (chunk->count == chunk->capacity) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = reallocate(chunk->code, oldCapacity, chunk->capacity, "Chunk code buffer");
    }
    
    chunk->code[chunk->count++] = byte;
}

void bytecode_set(Bytecode* chunk, int position, uint8_t byte) {
    assert(position < chunk->count && position >= 0);

	// if (position >= chunk->count || position < 0) {
	// 	FAIL("Position out of bounds for chunk writing.");
	// }

	chunk->code[position] = byte;
}

void bytecode_free(Bytecode* chunk) {
    deallocate(chunk->code, chunk->capacity * sizeof(uint8_t), "Chunk code buffer"); // the sizeof is probably stupid
    value_array_free(&chunk->constants);
    integer_array_free(&chunk->referenced_names_indices);
    integer_array_free(&chunk->assigned_names_indices);
    bytecode_init(chunk);
}

int bytecode_add_constant(Bytecode* chunk, struct Value* constant) {
	value_array_write(&chunk->constants, constant);
    return chunk->constants.count - 1;
}

void bytecode_print_constant_table(Bytecode* chunk) { // For debugging
	printf("\nConstant table of chunk pointing at '%p':\n", chunk->code);
	for (int i = 0; i < chunk->constants.count; i++) {
		Value constant = chunk->constants.values[i];
		printf("%d: ", i);
		value_print(constant);
		printf(" [ type %d ]", constant.type);
		printf("\n");
	}
}
