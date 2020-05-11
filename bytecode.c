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
    if (chunk->count >= 65534) {
        // Because jump offsets are currently absolute shorts. We should change them to relative offsets, but it's fine for now.
        FAIL("One bytecode object cannot have more than 65533 bytes. This likely means you have at least a few thousands"
                "of lines of code in one file or function. Please split the file or function into multiple ones.");
    }

    if (chunk->count == chunk->capacity) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = reallocate(chunk->code, oldCapacity, chunk->capacity, "Chunk code buffer");
    }
    
    chunk->code[chunk->count++] = byte;
}

void bytecode_set(Bytecode* chunk, int position, uint8_t byte) {
    assert(position < chunk->count && position >= 0);

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
    if (chunk->constants.count >= 65534) {
        FAIL("Too many constants to one code object (>= 65534). Cannot fit the index into a short in the bytecode.");
    }

	value_array_write(&chunk->constants, constant);
    return chunk->constants.count - 1;
}

void bytecode_print_constant_table(Bytecode* chunk) { // For debugging
	printf("\nConstant table [size %d] of chunk pointing at '%p':\n", chunk->constants.count, chunk->code);
	for (int i = 0; i < chunk->constants.count; i++) {
		Value constant = chunk->constants.values[i];
		printf("%d: ", i);
		value_print(constant);
		printf(" [ type %d ]", constant.type);
		printf("\n");
	}
}
