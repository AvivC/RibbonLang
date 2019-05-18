#include <stdio.h>
#include "debug.h"
#include "object.h"
#include "value.h"

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
    int constantIndex = chunk->code[offset + 1];
    Value constant = chunk->constants.values[constantIndex];
    
    printf("%-30s '", name);
    printValue(constant);
    printf("'\n");
    
    return offset + 2;
}

static int simpleInstruction(const char* name, int offset) {
    printf("%s\n", name);
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
                offset = simpleInstruction("OP_ADD", offset);
                break;
            }
            case OP_SUBTRACT: {
                offset = simpleInstruction("OP_SUBTRACT", offset);
                break;
            }
            case OP_DIVIDE: {
                offset = simpleInstruction("OP_DIVIDE", offset);
                break;
            }
            case OP_MULTIPLY: {
                offset = simpleInstruction("OP_MULTIPLY", offset);
                break;
            }
            case OP_NEGATE: {
                offset = simpleInstruction("OP_NEGATE", offset);
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
                offset = simpleInstruction("OP_CALL", offset);
                break;
            }
            case OP_RETURN: {
                offset = simpleInstruction("OP_RETURN", offset);
                break;
            }
            default:
                printf("Weird opcode.\n");
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
