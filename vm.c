#include <string.h>

#include "vm.h"
#include "common.h"
#include "chunk.h"
#include "value.h"
#include "object.h"
#include "memory.h"
#include "table.h"

VM vm;

static void push(Value value) {
    #if DEBUG
        if (vm.stackTop - vm.stack == STACK_MAX) {
            FAIL("STACK OVERFLOW!");
        }
    #endif
    *vm.stackTop = value;
    vm.stackTop++;
}

static Value pop() {
    #if DEBUG
        if (vm.stackTop == vm.stack) {
            FAIL("STACK UNDERFLOW!");
        }
    #endif
    vm.stackTop--;
    return *vm.stackTop;
}

void initVM(Chunk* chunk) {
    vm.ip = chunk->code;
    vm.chunk = chunk;
    vm.stackTop = vm.stack;
    initTable(&vm.globals);
}

void freeVM() {
    freeChunk(vm.chunk);
    freeTable(&vm.globals);
    while (vm.objects != NULL) {
        Object* next = vm.objects->next;
        freeObject(vm.objects);
        vm.objects = next;
    }
}

InterpretResult interpret() {
    #define READ_BYTE() (*vm.ip++)
    #define BINARY_OPERATION(op) do { \
        Value b = pop(); \
        Value a = pop(); \
        Value result; \
        \
        if (a.type == VALUE_NUMBER && b.type == VALUE_NUMBER) { \
            result = MAKE_VALUE_NUMBER(a.as.number op b.as.number); \
            \
        } else if (a.type == VALUE_OBJECT && b.type == VALUE_OBJECT) { \
            ObjectString* aString = OBJECT_AS_STRING(a.as.object); \
            ObjectString* bString = OBJECT_AS_STRING(b.as.object); \
            char* buffer = allocate(aString->length + bString->length + 1, "Concatenated string"); \
            memcpy(buffer, aString->chars, aString->length); \
            memcpy(buffer + aString->length, bString->chars, bString->length); \
            int stringLength = aString->length + bString->length; \
            buffer[stringLength] = '\0'; \
            ObjectString* objString = takeString(buffer, stringLength); \
            result = MAKE_VALUE_OBJECT(objString); \
             \
             \
        } else { \
            fprintf(stderr, "Connecting wrong stuff.\n"); \
        } \
        push(result); \
    } while(false)
    
    for (;;) {
        #if DEBUG_TRACE_EXECUTION
        for (Value* value = vm.stack; value - vm.stackTop; value++) {
            printf("[ ");
            printValue(*value);
            printf(" ]");
        }
        printf("\n");
        #endif
        
        uint8_t opcode = READ_BYTE();
        
        switch (opcode) {
            case OP_CONSTANT: {
                DEBUG_TRACE("OP_CONSTANT");
                int constantIndex = READ_BYTE();
                Value constant = vm.chunk->constants.values[constantIndex];
                push(constant);
                break;
            }
            
            case OP_ADD: {
                DEBUG_TRACE("OP_ADD");
                BINARY_OPERATION(+);
                break;
            }
            
            case OP_SUBTRACT: {
                DEBUG_TRACE("OP_SUBTRACT");
                BINARY_OPERATION(-);
                break;
            }
            
            case OP_MULTIPLY: {
                DEBUG_TRACE("OP_MULTIPLY");
                BINARY_OPERATION(*);
                break;
            }
            
            case OP_DIVIDE: {
                DEBUG_TRACE("OP_DIVIDE");
                BINARY_OPERATION(/);
                break;
            }
            
            case OP_RETURN: {
                DEBUG_TRACE("OP_RETURN");
                printValue(pop());
                printf("\n"); // Temporary?
                return INTERPRET_SUCCESS;
            }
            
            case OP_NEGATE: {
                DEBUG_TRACE("OP_NEGATE");
                Value operand = pop();
                Value result;
                result.type = VALUE_NUMBER;
                result.as.number = operand.as.number * -1;
                push(result);
                break;
            }
            
            case OP_LOAD_VARIABLE: {
                DEBUG_TRACE("OP_LOAD_VARIABLE");
                int constantIndex = READ_BYTE();
                ObjectString* name = OBJECT_AS_STRING(vm.chunk->constants.values[constantIndex].as.object);
                Value value;
                bool success = getTable(&vm.globals, name, &value);
                
                if (success) {
                    push(value);
                } else {
                    push(MAKE_VALUE_NIL()); // TODO: proper error handling
                }
                break;
            }
            
            case OP_SET_VARIABLE: {
                DEBUG_TRACE("OP_SET_VARIABLE");
                int constantIndex = READ_BYTE();
                ObjectString* name = OBJECT_AS_STRING(vm.chunk->constants.values[constantIndex].as.object);
                Value value = pop();
                setTable(&vm.globals, name, value);
                break;
            }
            
            case OP_CALL: {
                ObjectFunction* function = (ObjectFunction*) pop().as.object;
                // TODO: implement explicit call-stack
				
                uint8_t* currentIp = vm.ip;
				
                vm.ip = function->chunk.code;
                interpret();
                vm.ip = currentIp;
                break;
            }
        }
    }
    
    #undef READ_BYTE
    #undef BINARY_OPERATION
}