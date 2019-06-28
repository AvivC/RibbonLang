#include <stdio.h>
#include "debug.h"
#include "object.h"
#include "value.h"

static int singleOperandInstruction(const char* name, Chunk* chunk, int offset) {
    int operand = chunk->code[offset + 1];

    printf("%p %-28s %d\n", chunk->code + offset, name, operand);
    return offset + 2;
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
    int constantIndex = chunk->code[offset + 1];
    Value constant = chunk->constants.values[constantIndex];
    
    printf("%p %-27s '", chunk->code + offset, name);
    printValue(constant);
    printf("'\n");
    
    return offset + 2;
}

static int simpleInstruction(const char* name, Chunk* chunk, int offset) {
    printf("%p %s\n", chunk->code + offset, name);
    return offset + 1;
}

int disassembleInstruction(OP_CODE opcode, Chunk* chunk, int offset) {
	switch (opcode) {
		case OP_CONSTANT: {
			return constantInstruction("OP_CONSTANT", chunk, offset);
			break;
		}
		case OP_ADD: {
			return simpleInstruction("OP_ADD", chunk, offset);
			break;
		}
		case OP_SUBTRACT: {
			return simpleInstruction("OP_SUBTRACT", chunk, offset);
			break;
		}
		case OP_DIVIDE: {
			return simpleInstruction("OP_DIVIDE", chunk, offset);
			break;
		}
		case OP_MULTIPLY: {
			return simpleInstruction("OP_MULTIPLY", chunk, offset);
			break;
		}
		case OP_LESS_THAN: {
			return simpleInstruction("OP_LESS_THAN", chunk, offset);
			break;
		}
		case OP_GREATER_THAN: {
			return simpleInstruction("OP_GREATER_THAN", chunk, offset);
			break;
		}
		case OP_LESS_EQUAL: {
			return simpleInstruction("OP_LESS_EQUAL", chunk, offset);
			break;
		}
		case OP_GREATER_EQUAL: {
			return simpleInstruction("OP_GREATER_EQUAL", chunk, offset);
			break;
		}
		case OP_EQUAL: {
			return simpleInstruction("OP_EQUAL", chunk, offset);
			break;
		}
		case OP_NEGATE: {
			return simpleInstruction("OP_NEGATE", chunk, offset);
			break;
		}
		case OP_LOAD_VARIABLE: {
			return constantInstruction("OP_LOAD_VARIABLE", chunk, offset);
			break;
		}
		case OP_SET_VARIABLE: {
			return constantInstruction("OP_SET_VARIABLE", chunk, offset);
			break;
		}
		case OP_CALL: {
			return singleOperandInstruction("OP_CALL", chunk, offset);
			break;
		}
		case OP_POP: {
			return simpleInstruction("OP_POP", chunk, offset);
			break;
		}
		case OP_NIL: {
			return simpleInstruction("OP_NIL", chunk, offset);
			break;
		}
		case OP_RETURN: {
			return simpleInstruction("OP_RETURN", chunk, offset);
			break;
		}
		default: {
			FAIL("Unknown opcode when disassembling: %d", opcode);
			return -1;
		}
	}
}

void disassembleChunk(Chunk* chunk) {
    int offset = 0; 
    for (; offset < chunk->count; ) {
		offset = disassembleInstruction(chunk->code[offset], chunk, offset);
    }
    
    for (int i = 0; i < chunk->constants.count; i++) {
        Value constant = chunk->constants.values[i];
        if (constant.type == VALUE_OBJECT && constant.as.object->type == OBJECT_FUNCTION) {
            printf("\nInner function [index %d]:\n", i);
            ObjectFunction* funcObj = (ObjectFunction*) constant.as.object;
            disassembleChunk(&funcObj->chunk);
        }
    }
}
