#include <string.h>

#include "vm.h"
#include "common.h"
#include "chunk.h"
#include "value.h"
#include "object.h"
#include "memory.h"
#include "table.h"
#include "builtins.h"
#include "debug.h"

#define INITIAL_GC_THRESHOLD 4

VM vm;

static StackFrame* currentFrame(void) {
	return vm.callStackTop - 1;
}

static Chunk* currentChunk(void) {
	return &currentFrame()->objFunc->chunk;
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

static void initStackFrame(StackFrame* frame) {
	frame->returnAddress = NULL;
	frame->objFunc = NULL;
	initTable(&frame->localVariables);
}

static StackFrame newStackFrame(uint8_t* returnAddress, ObjectFunction* objFunc) {
	StackFrame frame;
	initStackFrame(&frame);
	frame.returnAddress = returnAddress;
	frame.objFunc = objFunc;
	return frame;
}

static StackFrame makeBaseStackFrame(Chunk* baseChunk) {
	ObjectFunction* baseObjFunc = newUserObjectFunction(*baseChunk, NULL, 0);
	return newStackFrame(NULL, baseObjFunc);
}

static void pushFrame(StackFrame frame) {
	*vm.callStackTop = frame;
	vm.callStackTop++;
}

static void freeStackFrame(StackFrame* frame) {
	freeTable(&frame->localVariables);
	initStackFrame(frame);
}

static StackFrame popFrame(void) {
	vm.callStackTop--;
	return *vm.callStackTop;
}

static bool isInFrame(void) {
	return vm.callStackTop != vm.callStack;
}

static Value loadVariable(ObjectString* name) {
	Value value;

	for (StackFrame* frame = vm.callStackTop; frame > vm.callStack;) {
		frame--;

		if (getTable(&frame->localVariables, name, &value)) {
			return value;
		}
	}

	if (getTable(&vm.globals, name, &value)) {
		return value;
	}

	return MAKE_VALUE_NIL();
}

static void gcMarkObject(Object* object) {
	if (object->isReachable) {
		return;
	}

	// TODO: Scan attributes recursively

	object->isReachable = true;

	if (object->type == OBJECT_FUNCTION) {
		ObjectFunction* objFunc = OBJECT_AS_FUNCTION(object);

		if (!objFunc->isNative) {
			ValueArray constants = objFunc->chunk.constants;
			for (int i = 0; i < constants.count; i++) {
				if (constants.values[i].type == VALUE_OBJECT) {
					gcMarkObject(constants.values[i].as.object);
				}
			}
		}

		for (int i = 0; i < objFunc->numParams; i++) {
			gcMarkObject((Object*) objFunc->parameters[i]);
		}
	}
}

static void gcMark(void) {
	// TODO: Recursively free all object attributes of the freed objects

	for (Value* value = vm.evalStack; value != vm.stackTop; value++) {
		if (value->type == VALUE_OBJECT) {
			gcMarkObject(value->as.object);
		}
	}

	for (StackFrame* frame = vm.callStack; frame != vm.callStackTop; frame++) {
		gcMarkObject((Object*) frame->objFunc);
	}

	// TODO: Pretty naive and inefficient - we scan the whole table in memory even though
	// many entries are likely to be empty
	for (int i = 0; i < vm.globals.capacity; i++) {
		Entry* entry = &vm.globals.entries[i];
		if (entry->value.type == VALUE_OBJECT) {
			gcMarkObject(entry->value.as.object);
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
	int numParams = 1;
	ObjectString** printParams = createCopiedStringsArray((const char*[] ) {"text" }, numParams, "Parameters list");
	ObjectFunction* printFunction = newNativeObjectFunction(builtinPrint, printParams, numParams);
	setTableCStringKey(&vm.globals, "print", MAKE_VALUE_OBJECT(printFunction));
}

static void callUserFunction(ObjectFunction* function) {
	StackFrame frame = newStackFrame(vm.ip, function);
	for (int i = 0; i < function->numParams; i++) {
		ObjectString* paramName = function->parameters[i];
		Value argument = pop();
		setTable(&frame.localVariables, paramName, argument);
	}
	pushFrame(frame);
	vm.ip = function->chunk.code;
}

static void callNativeFunction(ObjectFunction* function) {
	ValueArray arguments;
	initValueArray(&arguments);
	for (int i = 0; i < function->numParams; i++) {
		writeValueArray(&arguments, pop());
	}
	function->nativeFunction(arguments);
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
	for (StackFrame* frame = vm.callStack; frame != vm.callStackTop; frame++) {
		freeStackFrame(frame);
	}
	resetStacks();
	freeTable(&vm.globals);

	gc(); // TODO: probably move upper

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
            result = MAKE_VALUE_NIL(); \
        } \
        push(result); \
    } while(false)

	// TODO: Implement an actual runtime error mechanism. This is a placeholder.
	#define RUNTIME_ERROR(...) do { \
		fprintf(stderr, "Runtime error: " __VA_ARGS__); \
		fprintf(stderr, "\n"); \
		runtimeErrorOccured = true; \
		runLoop = false; \
		/* Remember to break manually after using this macro! */ \
	} while(false)

	pushFrame(makeBaseStackFrame(baseChunk));

	vm.allowGC = true;
	DEBUG_OBJECTS("Set vm.allowGC = true.");

	vm.ip = baseChunk->code;
	gc(); // Cleanup unused objects the compiler created

	bool runLoop = true;
	bool runtimeErrorOccured = false;

	DEBUG_TRACE("Starting interpreter loop.");

    while (runLoop) {
		DEBUG_TRACE("\n--------------------------\n");
    	DEBUG_TRACE("IP: %p", vm.ip);
    	DEBUG_TRACE("numObjects: %d", vm.numObjects);
    	DEBUG_TRACE("maxObjects: %d", vm.maxObjects);

    	uint8_t opcode = READ_BYTE();
        
		#if DEBUG_TRACE_EXECUTION
			disassembleInstruction(opcode, currentChunk(), vm.ip - 1 - currentChunk()->code);
		#endif

        switch (opcode) {
            case OP_CONSTANT: {
                int constantIndex = READ_BYTE();
                Value constant = currentChunk()->constants.values[constantIndex];
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
            
            case OP_NIL: {
            	push(MAKE_VALUE_NIL());
            	break;
            }

            case OP_RETURN: {

                StackFrame frame = popFrame();
                bool atBaseFrame = frame.returnAddress == NULL;
                if (atBaseFrame) {
                	runLoop = false;
                } else {
                	vm.ip = frame.returnAddress;
                }

                freeStackFrame(&frame);

                break;
            }
            
            case OP_POP: {
            	pop();
            	break;
            }

            case OP_NEGATE: {
                Value operand = pop();
                Value result;
                result.type = VALUE_NUMBER;
                result.as.number = operand.as.number * -1;
                push(result);
                break;
            }
            
            case OP_LOAD_VARIABLE: {

                int constantIndex = READ_BYTE();
                ObjectString* name = OBJECT_AS_STRING(currentChunk()->constants.values[constantIndex].as.object);
                push(loadVariable(name));

                break;
            }
            
            case OP_SET_VARIABLE: {
                int constantIndex = READ_BYTE();
                ObjectString* name = OBJECT_AS_STRING(currentChunk()->constants.values[constantIndex].as.object);
                Value value = pop();
                setTable(&currentFrame()->localVariables, name, value);
                break;
            }
            
            case OP_CALL: {
            	int argCount = READ_BYTE();

                if (peek().type != VALUE_OBJECT || (peek().type == VALUE_OBJECT && peek().as.object->type != OBJECT_FUNCTION)) {
                	RUNTIME_ERROR("Illegal call target.");
                	break;
                }

                ObjectFunction* function = (ObjectFunction*) pop().as.object;

                if (argCount != function->numParams) {
                	RUNTIME_ERROR("Function called with %d arguments, needs %d.", argCount, function->numParams);
                	break;
                }

                if (function->isNative) {
                	callNativeFunction(function);
                } else {
                	callUserFunction(function);
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
			printf("\n\nLocal variables:\n");
			if (isInFrame()) {
				printTable(&currentFrame()->localVariables);
			} else {
				printf("No stack frames.");
			}
			printf("\n\n");
			#if DEBUG_MEMORY_EXECUTION
				printAllObjects();
			#endif
        #endif
    }

    DEBUG_TRACE("\n--------------------------\n");
	DEBUG_TRACE("Ended interpreter loop.");
    
    #undef READ_BYTE
    #undef BINARY_OPERATION

    return runtimeErrorOccured ? INTERPRET_RUNTIME_ERROR : INTERPRET_SUCCESS;
}
