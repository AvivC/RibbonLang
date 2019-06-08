#include <string.h>

#include "vm.h"
#include "common.h"
#include "chunk.h"
#include "value.h"
#include "object.h"
#include "memory.h"
#include "table.h"
#include "builtins.h"

#define INITIAL_GC_THRESHOLD 4

VM vm;

static StackFrame currentFrame(void) {
	return *(vm.callStackTop - 1);
}

static Chunk* currentChunk(void) {
	return &currentFrame().objFunc->chunk;
}

static void pushFrame(StackFrame frame) {
	*vm.callStackTop = frame;
	vm.callStackTop++;
}

static StackFrame popFrame(void) {
	vm.callStackTop--;
	return *vm.callStackTop;
}

static void push(Value value) {
    #if DEBUG
        if (vm.stackTop - vm.evalStack == STACK_MAX) {
            FAIL("STACK OVERFLOW!");
        }
    #endif
    *vm.stackTop = value;
    vm.stackTop++;
}

static Value pop(void) {
    #if DEBUG
        if (vm.stackTop == vm.evalStack) {
            FAIL("STACK UNDERFLOW!");
        }
    #endif
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peek(void) {
	return *(vm.stackTop - 1);
}

static StackFrame newStackFrame(uint8_t* returnAddress, ObjectFunction* objFunc) {
	StackFrame frame;
	frame.returnAddress = returnAddress;
	frame.objFunc = objFunc;
	return frame;
}

static void gcMark(void) {
	// TODO: Recursively free all object attributes of the freed objects

	for (Value* value = vm.evalStack; value != vm.stackTop; value++) {
		if (value->type == VALUE_OBJECT) {
			value->as.object->isReachable = true;
		}
	}

	for (StackFrame* frame = vm.callStack; frame != vm.callStackTop; frame++) {
		frame->objFunc->base.isReachable = true;

		// TODO: Iterate constants every gc() - probably wasteful, constants being always constant
		ValueArray constants = frame->objFunc->chunk.constants;
		for (int i = 0; i < constants.count; i++) {
			if (constants.values[i].type == VALUE_OBJECT) {
				constants.values[i].as.object->isReachable = true;
			}
		}
	}

	// TODO: Pretty naive and inefficient - we scan the whole table in memory even though
	// many entries are likely to be empty
	for (int i = 0; i < vm.globals.capacity; i++) {
		Entry* entry = &vm.globals.entries[i];
		if (entry->value.type == VALUE_OBJECT) {
			entry->value.as.object->isReachable = true;
		}
	}

	// TODO: Scan more things which will be added later, such as local variable Tables
}

static void gcSweep(void) {
	Object** current = &vm.objects;
	while (*current != NULL) {
		if ((*current)->isReachable) {
			(*current)->isReachable = false;
			current = &((*current)->next);
		} else {
			Object* unreachable = *current;
			*current = unreachable->next;
			freeObject(unreachable);
		}
	}
}

void gc(void) {
	if (vm.allowGC) {
		DEBUG_OBJECTS("GC Running. numObjects: %d. maxObjects: %d", vm.numObjects, vm.maxObjects);

		gcMark();
		gcSweep();

		vm.maxObjects = vm.numObjects * 2;

		DEBUG_OBJECTS("GC Finished. numObjects: %d. maxObjects: %d", vm.numObjects, vm.maxObjects);
	} else {
		DEBUG_OBJECTS("GC should run, but vm.allowGC is still false.");
	}
}

static void resetStacks(void) {
	vm.stackTop = vm.evalStack;
	vm.callStackTop = vm.callStack;
}

static void setBuiltinGlobals(void) {
	ObjectFunction* printFunction = newNativeObjectFunction(builtinPrint);
	ObjectString* printFuncName = copyString("print", 5);
	setTable(&vm.globals, printFuncName, MAKE_VALUE_OBJECT(printFunction));
}

void initVM(void) {
    vm.ip = NULL;

	resetStacks();
    vm.numObjects = 0;
    vm.maxObjects = INITIAL_GC_THRESHOLD;
    vm.allowGC = false;

    initTable(&vm.globals);
    setBuiltinGlobals();
}

void freeVM(void) {
	resetStacks();
	freeTable(&vm.globals);
	gc();

    vm.ip = NULL;
    vm.numObjects = 0;
    vm.maxObjects = INITIAL_GC_THRESHOLD;
    vm.allowGC = false;
}

InterpretResult interpret(Chunk* baseChunk) {
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
            char* buffer = allocate(aString->length + bString->length + 1, "Object string buffer"); \
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

	ObjectFunction* baseObjFunc = newUserObjectFunction(*baseChunk);
	StackFrame baseFrame = newStackFrame(NULL, baseObjFunc);
	pushFrame(baseFrame);
	vm.allowGC = true;

	vm.ip = baseChunk->code;

    for (;;) {
    	DEBUG_TRACE("IP: %p", vm.ip);
    	DEBUG_TRACE("numObjects: %d", vm.numObjects);
    	DEBUG_TRACE("maxObjects: %d", vm.maxObjects);

    	uint8_t opcode = READ_BYTE();
        
        switch (opcode) {
            case OP_CONSTANT: {
                DEBUG_TRACE("OP_CONSTANT");
                int constantIndex = READ_BYTE();
                Value constant = currentChunk()->constants.values[constantIndex];
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

                StackFrame frame = popFrame();
                bool atBaseFrame = frame.returnAddress == NULL;
                if (atBaseFrame) {
                	return INTERPRET_SUCCESS;
                } else {
                	vm.ip = frame.returnAddress;
                }

                break;
            }
            
            case OP_POP: {
            	DEBUG_TRACE("OP_POP");
            	pop();
            	break;
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
                ObjectString* name = OBJECT_AS_STRING(currentChunk()->constants.values[constantIndex].as.object);
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
                ObjectString* name = OBJECT_AS_STRING(currentChunk()->constants.values[constantIndex].as.object);
                Value value = pop();
                setTable(&vm.globals, name, value);
                break;
            }
            
            case OP_CALL: {
            	DEBUG_TRACE("OP_CALL");

            	// peek() and pop() as separate steps, so the GC doesn't collect the ObjectFunction
            	// before we have managed to put it on the call stack.
            	// In the "current" implementation this shouldn't be possible because GC will run
            	// in the same thread, between opcode instructions, but this should make the code survive
            	// GC even if it runs in a different thread or something...
            	// Following this logic, the same approach should be taken everywhere in the interpreter loop, which it isn't...

                ObjectFunction* function = (ObjectFunction*) peek().as.object;
                if (function->isNative) {
                	function->nativeFunction();
                } else {
					pushFrame(newStackFrame(vm.ip, function));
					pop();
					vm.ip = function->chunk.code;
                }
                break;
            }

            default: {
            	FAIL("Unknown opcode: %d", opcode);
            }
        }

		#if DEBUG_TRACE_EXECUTION
        	printf("\n");
			bool stackEmpty = vm.evalStack == vm.stackTop;
			if (stackEmpty) {
				printf("[ ]");
			} else {
				for (Value* value = vm.evalStack; value < vm.stackTop; value++) {
					printf("[ ");
					printValue(*value);
					printf(" ]");
				}
			}
			printf("\n\n");
        #endif
    }
    
    #undef READ_BYTE
    #undef BINARY_OPERATION

    return INTERPRET_RUNTIME_ERROR;
}
