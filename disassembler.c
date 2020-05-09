#include "disassembler.h"

#include <stdio.h>
#include "plane_object.h"
#include "value.h"
#include "plane_utils.h"

// TODO: Add tests maybe?

static int single_operand_instruction(const char* name, Bytecode* chunk, int offset) {
    int operand = chunk->code[offset + 1];

    printf("%p %-28s %d\n", chunk->code + offset, name, operand);
    return offset + 2;
}

static int short_operand_instruction(const char* name, Bytecode* chunk, int offset) {
    uint16_t operand = two_bytes_to_short(chunk->code[offset + 1], chunk->code[offset + 2]);

    printf("%p %-28s %d\n", chunk->code + offset, name, operand);
    return offset + 3;
}

static Value read_constant_operand(Bytecode* chunk, int offset) {
	uint8_t constant_index_byte_1 = chunk->code[offset];
	uint8_t constant_index_byte_2 = chunk->code[offset + 1];
	uint16_t index = two_bytes_to_short(constant_index_byte_1, constant_index_byte_2);
	return chunk->constants.values[index];
}

static int constant_instruction(const char* name, Bytecode* chunk, int offset) {
	Value constant = read_constant_operand(chunk, offset + 1);

    // int constantIndex = chunk->code[offset + 1];
    // Value constant = chunk->constants.values[constantIndex];
    
    printf("%p %-28s ", chunk->code + offset, name);
    value_print(constant);
    printf("\n");
    
    return offset + 3;
}

static int constant_and_variable_length_constants_instruction(const char* name, Bytecode* chunk, int offset) {
    // int constantIndex = chunk->code[offset + 1];
    // Value constant = chunk->constants.values[constantIndex];

	Value constant = read_constant_operand(chunk, offset + 1);

    printf("%p %-28s ", chunk->code + offset, name);
    value_print(constant);

    uint16_t additional_operands_count = two_bytes_to_short(chunk->code[offset + 3], chunk->code[offset + 4]);

    for (int i = 0; i < additional_operands_count * 2; i += 2) {
		int additional_operand_offset = offset + 5 + i;
		Value operand = read_constant_operand(chunk, additional_operand_offset);
		// uint8_t additional_operand_index = chunk->code[additional_operand_offset];
		// Value operand = chunk->constants.values[additional_operand_index];
		printf(", ");
		value_print(operand);
	}

    printf("\n");

    return offset + 5 + (additional_operands_count * 2);
}

static int simple_instruction(const char* name, Bytecode* chunk, int offset) {
    printf("%p %s\n", chunk->code + offset, name);
    return offset + 1;
}

int disassembler_do_single_instruction(OP_CODE opcode, Bytecode* chunk, int offset) {
	printf("%-3d ", offset);

	switch (opcode) {
		case OP_CONSTANT: {
			return constant_instruction("OP_CONSTANT", chunk, offset);
		}
		case OP_ADD: {
			return simple_instruction("OP_ADD", chunk, offset);
		}
		case OP_SUBTRACT: {
			return simple_instruction("OP_SUBTRACT", chunk, offset);
		}
		case OP_DIVIDE: {
			return simple_instruction("OP_DIVIDE", chunk, offset);
		}
		case OP_MULTIPLY: {
			return simple_instruction("OP_MULTIPLY", chunk, offset);
		}
		case OP_LESS_THAN: {
			return simple_instruction("OP_LESS_THAN", chunk, offset);
		}
		case OP_GREATER_THAN: {
			return simple_instruction("OP_GREATER_THAN", chunk, offset);
		}
		case OP_LESS_EQUAL: {
			return simple_instruction("OP_LESS_EQUAL", chunk, offset);
		}
		case OP_GREATER_EQUAL: {
			return simple_instruction("OP_GREATER_EQUAL", chunk, offset);
		}
		case OP_EQUAL: {
			return simple_instruction("OP_EQUAL", chunk, offset);
		}
		case OP_AND: {
			return simple_instruction("OP_AND", chunk, offset);
		}
		case OP_OR: {
			return simple_instruction("OP_OR", chunk, offset);
		}
		case OP_NEGATE: {
			return simple_instruction("OP_NEGATE", chunk, offset);
		}
		case OP_LOAD_VARIABLE: {
			return constant_instruction("OP_LOAD_VARIABLE", chunk, offset);
		}
		case OP_SET_VARIABLE: {
			return constant_instruction("OP_SET_VARIABLE", chunk, offset);
		}
		case OP_CALL: {
			return single_operand_instruction("OP_CALL", chunk, offset);
		}
		case OP_ACCESS_KEY: {
			return simple_instruction("OP_ACCESS_KEY", chunk, offset);
		}
		case OP_SET_KEY: {
			return simple_instruction("OP_SET_KEY", chunk, offset);
		}
		case OP_IMPORT: {
			return constant_instruction("OP_IMPORT", chunk, offset);
		}
		case OP_GET_ATTRIBUTE: {
			return constant_instruction("OP_GET_ATTRIBUTE", chunk, offset);
		}
		case OP_SET_ATTRIBUTE: {
			return constant_instruction("OP_SET_ATTRIBUTE", chunk, offset);
		}
		case OP_POP: {
			return simple_instruction("OP_POP", chunk, offset);
		}
		case OP_JUMP_IF_FALSE: {
			return short_operand_instruction("OP_JUMP_IF_FALSE", chunk, offset);
		}
		case OP_JUMP: {
			return short_operand_instruction("OP_JUMP", chunk, offset);
		}
		case OP_MAKE_STRING: {
			return constant_instruction("OP_MAKE_STRING", chunk, offset);
		}
		case OP_MAKE_TABLE: {
			return single_operand_instruction("OP_MAKE_TABLE", chunk, offset);
		}
		case OP_MAKE_FUNCTION: {
			return constant_and_variable_length_constants_instruction("OP_MAKE_FUNCTION", chunk, offset);
		}
		case OP_MAKE_CLASS: {
			return constant_instruction("OP_MAKE_CLASS", chunk, offset);
		}
		case OP_NIL: {
			return simple_instruction("OP_NIL", chunk, offset);
		}
		case OP_RETURN: {
			return simple_instruction("OP_RETURN", chunk, offset);
		}
	}

	FAIL("Unknown opcode when disassembling: %d", opcode);
	return -1;
}

void disassembler_do_bytecode(Bytecode* chunk) {
    int offset = 0; 
    for (; offset < chunk->count; ) {
		offset = disassembler_do_single_instruction(chunk->code[offset], chunk, offset);
    }
    
    for (int i = 0; i < chunk->constants.count; i++) {
        Value constant = chunk->constants.values[i];
        if (constant.type == VALUE_OBJECT && constant.as.object->type == OBJECT_CODE) {
            printf("\nInner chunk [index %d]:\n", i);
            ObjectCode* inner_code_object = (ObjectCode*) constant.as.object;
            Bytecode inner_chunk = inner_code_object->bytecode;
            disassembler_do_bytecode(&inner_chunk);
        }
    }

    bytecode_print_constant_table(chunk);
}
