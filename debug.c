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

void disassembleChunk(Chunk* chunk) {
    int offset = 0; 
    for (; offset < chunk->count; ) {
        switch (chunk->code[offset]) {
            case OP_CONSTANT: {
                offset = constantInstruction("OP_CONSTANT", chunk, offset);
                break;
            }
            case OP_ADD: {
                offset = simpleInstruction("OP_ADD", chunk, offset);
                break;
            }
            case OP_SUBTRACT: {
                offset = simpleInstruction("OP_SUBTRACT", chunk, offset);
                break;
            }
            case OP_DIVIDE: {
                offset = simpleInstruction("OP_DIVIDE", chunk, offset);
                break;
            }
            case OP_MULTIPLY: {
                offset = simpleInstruction("OP_MULTIPLY", chunk, offset);
                break;
            }
            case OP_NEGATE: {
                offset = simpleInstruction("OP_NEGATE", chunk, offset);
                break;
            }
            case OP_LOAD_VARIABLE: {
                offset = constantInstruction("OP_LOAD_VARIABLE", chunk, offset);
                break;
            }
            case OP_SET_VARIABLE: {
                offset = constantInstruction("OP_SET_VARIABLE", chunk, offset);
                break;
            }
            case OP_CALL: {
                offset = singleOperandInstruction("OP_CALL", chunk, offset);
                break;
            }
            case OP_POP: {
				offset = simpleInstruction("OP_POP", chunk, offset);
				break;
			}
            case OP_NIL: {
				offset = simpleInstruction("OP_NIL", chunk, offset);
				break;
			}
            case OP_RETURN: {
                offset = simpleInstruction("OP_RETURN", chunk, offset);
                break;
            }
        }
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
