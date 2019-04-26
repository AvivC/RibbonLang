#include "vm.h"
#include "chunk.h"
#include "value.h"

#define STACK_MAX 256  // TODO: review this. maybe grow dynamically?

typedef struct {
    uint8_t* ip;
    Chunk* chunk;
    Value* stackTop;
    Value stack[STACK_MAX];
} VM;

static VM vm;

static void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

static Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

void initVM(Chunk* chunk) {
    vm.ip = chunk->code;
    vm.chunk = chunk;
    vm.stackTop = vm.stack;
}

InterpretResult interpret() {
    #define READ_BYTE() (*vm.ip++)
    #define BINARY_OPERATION(op) do { \
        Value b = pop(); \
        Value a = pop(); \
        Value result; \
        result.type = VALUE_NUMBER; \
        result.as.number = a.as.number op b.as.number; \
        push(result); \
    } while(false)
    
    for (;;) {
        uint8_t opcode = READ_BYTE();
        
        switch (opcode) {
            case OP_CONSTANT: {
                int constantIndex = READ_BYTE();
                Value constant = vm.chunk->constants.values[constantIndex];
                push(constant);
                break;
            }
            
            case OP_ADD: {
                BINARY_OPERATION(+);
                break;
            }
            
            case OP_SUBTRACT: {
                BINARY_OPERATION(-);
                break;
            }
            
            case OP_MULTIPLY: {
                BINARY_OPERATION(*);
                break;
            }
            
            case OP_DIVIDE: {
                BINARY_OPERATION(/);
                break;
            }
            
            case OP_RETURN: {
                printValue(pop());
                return INTERPRET_SUCCESS;
            }
        }
    }
    
    #undef READ_BYTE
    #undef BINARY_OPERATION
}