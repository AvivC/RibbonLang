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
#include "utils.h"

#define INITIAL_GC_THRESHOLD 1024 * 1024

VM vm;

static StackFrame* currentFrame(void) {
	return vm.callStackTop - 1;
}

static Chunk* currentChunk(void) {
	return &currentFrame()->objFunc->code->chunk;
}

static void push(Value value) {
    #if DEBUG_IMPORTANT
        if (vm.stackTop - vm.evalStack >= STACK_MAX) {
            FAIL("STACK OVERFLOW!");
        }
    #endif
    *vm.stackTop = value;
    vm.stackTop++;
}

static Value pop(void) {
    #if DEBUG_IMPORTANT
        if (vm.stackTop <= vm.evalStack) {
            FAIL("STACK UNDERFLOW!");
        }
    #endif
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peekAt(int offset) {
	return *(vm.stackTop - offset);
}

static Value peek(void) {
	return peekAt(1);
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

static StackFrame makeBaseStackFrame(Chunk* base_chunk) {
	ObjectCode* code = object_code_new(*base_chunk);
	ObjectFunction* baseObjFunc = newUserObjectFunction(code, NULL, 0, NULL);
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
	// TODO: Fix this weird semantics thing

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

static void gcMarkObject(Object* object);

static void gcMarkCodeConstants(ObjectCode* code) {
	Chunk* chunk = &code->chunk;
	for (int i = 0; i < chunk->constants.count; i++) {
		Value* constant = &chunk->constants.values[i];
		if (constant->type == VALUE_OBJECT) {
			gcMarkObject(chunk->constants.values[i].as.object);
		}
	}

	// Note: not looking for bare VALUE_CHUNK here, because logically it should never happen.
	// Chunks should always be wrapped in ObjectCode in order to be managed "automatically" by the GC system.
}

static void gcMarkObject(Object* object) {
	if (object->isReachable) {
		return;
	}

	object->isReachable = true;

	// TODO: Pretty naive and inefficient - we scan the whole table in memory even though
	// many entries are likely to be empty
	for (int i = 0; i < object->attributes.capacity; i++) {
		Entry* entry = &object->attributes.entries[i];
		if (entry->value.type == VALUE_OBJECT) {
			gcMarkObject(entry->value.as.object);
		}
	}

	if (object->type == OBJECT_FUNCTION) {
		ObjectFunction* objFunc = OBJECT_AS_FUNCTION(object);

		if (!objFunc->isNative) {
			ObjectCode* code_object = objFunc->code;
			gcMarkObject((Object*) code_object);
			gcMarkCodeConstants(code_object);
		}
	} else if (object->type == OBJECT_CODE) { // Code objects can also be by themselves sometimes, on the eval stack for example
		gcMarkCodeConstants((ObjectCode*) object);
	}
}

//static void gcMark(void) {
//	for (Value* value = vm.evalStack; value != vm.stackTop; value++) {
//		if (value->type == VALUE_OBJECT) {
//			gcMarkObject(value->as.object);
//		} else if (value->type == VALUE_CHUNK) {
//			gcMarkChunkConstants(&value->as.chunk);
//		}
//	}
//
//	for (StackFrame* frame = vm.callStack; frame != vm.callStackTop; frame++) {
//		gcMarkObject((Object*) frame->objFunc);
//		gcMarkChunkConstants(&frame->objFunc->code->chunk);
//
//		// TODO: Pretty naive and inefficient - we scan the whole table in memory even though
//		// many entries are likely to be empty
//		for (int i = 0; i < frame->localVariables.capacity; i++) {
//			Entry* entry = &frame->localVariables.entries[i];
//			if (entry->value.type == VALUE_OBJECT) {
//				gcMarkObject(entry->value.as.object);
//			} else if (entry->value.type == VALUE_CHUNK) {
//				gcMarkChunkConstants(&entry->value.as.chunk);
//			}
//		}
//	}
//
//	// TODO: Pretty naive and inefficient - we scan the whole table in memory even though
//	// many entries are likely to be empty
//	for (int i = 0; i < vm.globals.capacity; i++) {
//		Entry* entry = &vm.globals.entries[i];
//		if (entry->value.type == VALUE_OBJECT) {
//			gcMarkObject(entry->value.as.object);
//		} else if (entry->value.type == VALUE_CHUNK) {
//			gcMarkChunkConstants(&entry->value.as.chunk);
//		}
//	}
//}

static void gcMark(void) {
	for (Value* value = vm.evalStack; value != vm.stackTop; value++) {
		if (value->type == VALUE_OBJECT) {
			gcMarkObject(value->as.object);
		}
	}

	for (StackFrame* frame = vm.callStack; frame != vm.callStackTop; frame++) {
		gcMarkObject((Object*) frame->objFunc);

		// TODO: Pretty naive and inefficient - we scan the whole table in memory even though
		// many entries are likely to be empty
		for (int i = 0; i < frame->localVariables.capacity; i++) {
			Entry* entry = &frame->localVariables.entries[i];
			if (entry->value.type == VALUE_OBJECT) {
				gcMarkObject(entry->value.as.object);
			}
		}
	}

	// TODO: Pretty naive and inefficient - we scan the whole table in memory even though
	// many entries are likely to be empty
	for (int i = 0; i < vm.globals.capacity; i++) {
		Entry* entry = &vm.globals.entries[i];
		if (entry->value.type == VALUE_OBJECT) {
			gcMarkObject(entry->value.as.object);
		}
	}
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
	#if DISABLE_GC

	DEBUG_GC_PRINT("GC would run, but is disabled");

	#else

	if (vm.allowGC) {
		size_t memory_before_gc = getAllocatedMemory();
		DEBUG_GC_PRINT("===== GC Running =====");
		DEBUG_GC_PRINT("numObjects: %d. maxObjects: %d", vm.numObjects, vm.maxObjects);
		DEBUG_GC_PRINT("Allocated memory: %d bytes", memory_before_gc);
		DEBUG_GC_PRINT("=======================");

		gcMark();
		gcSweep();

		vm.maxObjects = vm.numObjects * 2;

		DEBUG_GC_PRINT("===== GC Finished =====");
		DEBUG_GC_PRINT("numObjects: %d. maxObjects: %d", vm.numObjects, vm.maxObjects);
		DEBUG_GC_PRINT("Allocated memory before GC: %d bytes", memory_before_gc);
		DEBUG_GC_PRINT("Allocated memory after GC: %d bytes", getAllocatedMemory());
		DEBUG_GC_PRINT("=======================");
	} else {
		DEBUG_GC_PRINT("GC should run, but vm.allowGC is still false.");
	}

	#endif
}

static void resetStacks(void) {
	vm.stackTop = vm.evalStack;
	vm.callStackTop = vm.callStack;
}

static void register_builtin_function(const char* name, int num_params, char** params, NativeFunction function) {
	char** params_buffer = allocate(sizeof(char*) * num_params, "Parameters list cstrings");
	for (int i = 0; i < num_params; i++) {
		params_buffer[i] = copy_cstring(params[i], strlen(params[i]), "ObjectFunction param cstring");
	}
	ObjectFunction* obj_function = newNativeObjectFunction(function, params_buffer, num_params, NULL);
	setTableCStringKey(&vm.globals, name, MAKE_VALUE_OBJECT(obj_function));
}

static void setBuiltinGlobals(void) {
	register_builtin_function("print", 1, (char*[]) {"text"}, builtin_print);
	register_builtin_function("input", 0, NULL, builtin_input);
	register_builtin_function("read_file", 1, (char*[]) {"path"}, builtin_read_file);
}

static void callUserFunction(ObjectFunction* function) {
	if (function->self != NULL) {
		push(MAKE_VALUE_OBJECT(function->self));
	}

	StackFrame frame = newStackFrame(vm.ip, function);
	for (int i = 0; i < function->numParams; i++) {
		const char* paramName = function->parameters[i];
		Value argument = pop();
		setTableCStringKey(&frame.localVariables, paramName, argument);
	}
	pushFrame(frame);
	vm.ip = function->code->chunk.code;
}

static bool callNativeFunction(ObjectFunction* function) {
	if (function->self != NULL) {
		push(MAKE_VALUE_OBJECT(function->self));
	}

	ValueArray arguments;
	value_array_init(&arguments);
	for (int i = 0; i < function->numParams; i++) {
		Value value = pop();
		value_array_write(&arguments, &value);
	}

	Value result;
	bool func_success = function->nativeFunction(arguments, &result);
	if (func_success) {
		push(result);
	} else {
		push(MAKE_VALUE_NIL());
	}

	value_array_free(&arguments);
	return func_success;
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

#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (currentChunk()->constants.values[READ_BYTE()])

static Object* read_constant_as_object(ObjectType type) {
	Value constant = READ_CONSTANT();
	ASSERT_VALUE_TYPE(constant, VALUE_OBJECT);
	Object* object = constant.as.object;
	if (object->type != type) {
		FAIL("Object at '%p' is of type %d, expected %d", object, object->type, type);
	}
	return object;
}

#define READ_CONSTANT_AS_OBJECT(type, cast) (cast*) read_constant_as_object(type)

InterpretResult interpret(Chunk* baseChunk) {
    #define BINARY_MATH_OP(op) do { \
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

	#define ERROR_IF_WRONG_TYPE(value, value_type, message) do { \
		if (value.type != value_type	) { \
			RUNTIME_ERROR(message); \
		} \
	} while (false)

	#define ERROR_IF_NON_BOOLEAN(value, message) do { \
		ERROR_IF_WRONG_TYPE(value, VALUE_BOOLEAN, message); \
	} while (false)

	#define ERROR_IF_NON_NUMBER(value, message) do { \
		ERROR_IF_WRONG_TYPE(value, VALUE_NUMBER, message); \
	} while (false)

	#define ERROR_IF_NON_OBJECT(value, message) do { \
		ERROR_IF_WRONG_TYPE(value, VALUE_OBJECT, message); \
	} while (false)

	#define ERROR_IF_NOT_NIL(value, message) do { \
		ERROR_IF_WRONG_TYPE(value, VALUE_NIL, message); \
	} while (false)

	pushFrame(makeBaseStackFrame(baseChunk));

	vm.allowGC = true;
	DEBUG_OBJECTS_PRINT("Set vm.allowGC = true.");

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

			printf("\n");
			bool stackEmpty = vm.evalStack == vm.stackTop;
			if (stackEmpty) {
				printf("[ -- Empty Stack -- ]");
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

			chunk_print_constant_table(currentChunk());

			printf("\n\n");

			#if DEBUG_MEMORY_EXECUTION
				printAllObjects();
			#endif
		#endif

		// Possible that vm.numObjects > vm.maxObjects if many objects were created during the compiling stage, where GC is disallowed
		if (vm.numObjects >= vm.maxObjects) {
			gc();
		}

        switch (opcode) {
            case OP_CONSTANT: {
                int constantIndex = READ_BYTE();
                Value constant = currentChunk()->constants.values[constantIndex];
                push(constant);
                break;
            }
            
            case OP_ADD: {
            	if (peekAt(2).type == VALUE_OBJECT) {
            		Value other = pop();
            		Value self_val = pop();

            		Object* self = self_val.as.object;
            		Value add_method;
            		if (!getTableCStringKey(&self->attributes, "@add", &add_method)) {
            			RUNTIME_ERROR("Object doesn't support @add method.");
            			break;
            		}

					if (!is_value_object_of_type(add_method, OBJECT_FUNCTION)) {
						RUNTIME_ERROR("Objects @add isn't a function.");
						break;
					}

					ObjectFunction* add_method_as_func = (ObjectFunction*) add_method.as.object;

					ValueArray arguments;
					value_array_init(&arguments);
					value_array_write(&arguments, &self_val);
					value_array_write(&arguments, &other);

					Value result;
					if (add_method_as_func->isNative) {
						if (!add_method_as_func->nativeFunction(arguments, &result)) {
							RUNTIME_ERROR("@add function failed.");
							goto cleanup;
						}
						push(result);
					} else {
						// TODO: user function
					}

					cleanup:
					value_array_free(&arguments);

            	} else {
            		BINARY_MATH_OP(+);
            	}
                break;
            }
            
            case OP_SUBTRACT: {
                BINARY_MATH_OP(-);
                break;
            }
            
            case OP_MULTIPLY: {
                BINARY_MATH_OP(*);
                break;
            }
            
            case OP_DIVIDE: {
                BINARY_MATH_OP(/);
                break;
            }
            
            case OP_LESS_THAN: {
        		Value b = pop();
        		Value a = pop();

        		int compare = 0;
        		if (!compareValues(a, b, &compare)) {
        			RUNTIME_ERROR("Unable to compare two values <.");
        			break;
        		}
        		if (compare == -1) {
        			push(MAKE_VALUE_BOOLEAN(true));
        		} else {
        			push(MAKE_VALUE_BOOLEAN(false));
        		}

            	break;
            }

            case OP_GREATER_THAN: {
        		Value b = pop();
        		Value a = pop();

        		int compare = 0;
        		if (!compareValues(a, b, &compare)) {
        			RUNTIME_ERROR("Unable to compare two values >.");
        			break;
        		}
        		if (compare == 1) {
        			push(MAKE_VALUE_BOOLEAN(true));
        		} else {
        			push(MAKE_VALUE_BOOLEAN(false));
        		}

            	break;
            }

            case OP_LESS_EQUAL: {
        		Value b = pop();
        		Value a = pop();

        		int compare = 0;
        		if (!compareValues(a, b, &compare)) {
        			RUNTIME_ERROR("Unable to compare two values <=.");
        			break;
        		}
        		if (compare == -1 || compare == 0) {
        			push(MAKE_VALUE_BOOLEAN(true));
        		} else {
        			push(MAKE_VALUE_BOOLEAN(false));
        		}

            	break;
            }

            case OP_GREATER_EQUAL: {
        		Value b = pop();
        		Value a = pop();

        		int compare = 0;
        		if (!compareValues(a, b, &compare)) {
        			RUNTIME_ERROR("Unable to compare two values >=.");
        			break;
        		}
        		if (compare == 1 || compare == 0) {
        			push(MAKE_VALUE_BOOLEAN(true));
        		} else {
        			push(MAKE_VALUE_BOOLEAN(false));
        		}

            	break;
            }

            case OP_EQUAL: {
        		Value b = pop();
        		Value a = pop();

        		int compare = 0;
        		if (!compareValues(a, b, &compare)) {
        			RUNTIME_ERROR("Unable to compare two values ==.");
        			break;
        		}
        		if (compare == 0) {
        			push(MAKE_VALUE_BOOLEAN(true));
        		} else {
        			push(MAKE_VALUE_BOOLEAN(false));
        		}

            	break;
            }

            case OP_AND: {
            	// TODO: AND should be short-circuiting

        		Value b = pop();
        		Value a = pop();

        		ERROR_IF_NON_BOOLEAN(a, "Right side non-boolean in AND expression.");
        		ERROR_IF_NON_BOOLEAN(b, "Left side non-boolean in AND expression.");

        		if (a.as.boolean && b.as.boolean) {
        			push(MAKE_VALUE_BOOLEAN(true));
        		} else {
        			push(MAKE_VALUE_BOOLEAN(false));
        		}

            	break;
            }

            case OP_OR: {
        		Value b = pop();
        		Value a = pop();

        		ERROR_IF_NON_BOOLEAN(a, "Right side non-boolean in OR expression.");
        		ERROR_IF_NON_BOOLEAN(b, "Left side non-boolean in OR expression.");

        		if (a.as.boolean || b.as.boolean) {
        			push(MAKE_VALUE_BOOLEAN(true));
        		} else {
        			push(MAKE_VALUE_BOOLEAN(false));
        		}

            	break;
            }

            case OP_MAKE_STRING: {
            	RawString string = READ_CONSTANT().as.raw_string;
            	ObjectString* obj_string = copyString(string.data, string.length);
            	push(MAKE_VALUE_OBJECT(obj_string));
            	break;
            }

            case OP_MAKE_FUNCTION: {
				ObjectCode* obj_code = READ_CONSTANT_AS_OBJECT(OBJECT_CODE, ObjectCode);

				uint8_t num_params_byte1 = READ_BYTE();
				uint8_t num_params_byte2 = READ_BYTE();
				uint16_t num_params = twoBytesToShort(num_params_byte1, num_params_byte2);

				char** params_buffer = allocate(sizeof(char*) * num_params, "Parameters list cstrings");
				for (int i = 0; i < num_params; i++) {
					Value param_value = READ_CONSTANT();
					if (param_value.type != VALUE_RAW_STRING) {
						FAIL("Param constant expected to be VALUE_RAW_STRING, actual type: '%d'", param_value.type);
					}
					RawString param_raw_string = param_value.as.raw_string;
					params_buffer[i] = copy_cstring(param_raw_string.data, param_raw_string.length, "ObjectFunction param cstring");
				}

				ObjectFunction* obj_function = newUserObjectFunction(obj_code, params_buffer, num_params, NULL);
				push(MAKE_VALUE_OBJECT(obj_function));
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
                if (operand.type == VALUE_NUMBER) {
                	push(MAKE_VALUE_NUMBER(operand.as.number * -1));
                } else if (operand.type == VALUE_BOOLEAN) {
                	push(MAKE_VALUE_BOOLEAN(!operand.as.boolean));
                } else {
                	RUNTIME_ERROR("Illegal value to negate.");
                }
                break;
            }
            
            case OP_LOAD_VARIABLE: {
                int constantIndex = READ_BYTE();
                Value name_val = currentChunk()->constants.values[constantIndex];
                ASSERT_VALUE_TYPE(name_val, VALUE_OBJECT);
                ObjectString* name_string = objectAsString(name_val.as.object);
                push(loadVariable(name_string));

                break;
            }
            
            case OP_SET_VARIABLE: {
                int constantIndex = READ_BYTE();
                Value name_val = currentChunk()->constants.values[constantIndex];
                ASSERT_VALUE_TYPE(name_val, VALUE_OBJECT);
                ObjectString* name = OBJECT_AS_STRING(name_val.as.object);

                Value value = pop();
                setTable(&currentFrame()->localVariables, name, value);
                break;
            }
            
            case OP_CALL: {
            	int explicit_arg_count = READ_BYTE();

                if (peek().type != VALUE_OBJECT || (peek().type == VALUE_OBJECT && peek().as.object->type != OBJECT_FUNCTION)) {
                	printValue(peek());
                	RUNTIME_ERROR("Illegal call target. Target type: %d.", peek().type);
                	break;
                }

                ObjectFunction* function = OBJECT_AS_FUNCTION(pop().as.object);

                bool is_method = function->self != NULL;
                int actual_arg_count = explicit_arg_count + (is_method ? 1 : 0);
                if (actual_arg_count != function->numParams) {
                	RUNTIME_ERROR("Function called with %d arguments, needs %d.", explicit_arg_count, function->numParams);
                	break;
                }

                if (function->isNative) {
                	if (!callNativeFunction(function)) {
                		RUNTIME_ERROR("Native function failed.");
                		break;
                	}
                } else {
                	// TODO: Handle errors
                	callUserFunction(function);
                }

                break;
            }

            case OP_GET_ATTRIBUTE: {
                int constant_index = READ_BYTE();
                Value name_val = currentChunk()->constants.values[constant_index];
                ASSERT_VALUE_TYPE(name_val, VALUE_OBJECT);
                ObjectString* name = OBJECT_AS_STRING(name_val.as.object);

                Value obj_val = pop();
                if (obj_val.type != VALUE_OBJECT) {
                	RUNTIME_ERROR("Cannot access attribute on non-object.");
                	break;
                }

                Object* object = obj_val.as.object;
                Value attr_value;
                if (getTable(&object->attributes, name, &attr_value)) {
                	push(attr_value);
                } else {
                	push(MAKE_VALUE_NIL());
                }

                break;
            }

            case OP_SET_ATTRIBUTE: {
                Value name_val = currentChunk()->constants.values[READ_BYTE()];
                ASSERT_VALUE_TYPE(name_val, VALUE_OBJECT);
                ObjectString* name = OBJECT_AS_STRING(name_val.as.object);

                Value obj_value = pop();
                if (obj_value.type != VALUE_OBJECT) {
                	RUNTIME_ERROR("Cannot set attribute on non-object.");
                	break;
                }

                Object* object = obj_value.as.object;
                Value attribute_value = pop();
                setTable(&object->attributes, name, attribute_value);

                break;
            }

            case OP_ACCESS_KEY: {
            	Value subject_value = pop();
            	if (subject_value.type != VALUE_OBJECT) {
					RUNTIME_ERROR("Accessing key on none object.");
					break;
            	}

            	Object* subject = subject_value.as.object;

            	Value key = pop();

				Value key_access_method_value;
				if (!getTableCStringKey(&subject->attributes, "@get_key", &key_access_method_value)) {
					RUNTIME_ERROR("Object doesn't support @get_key method.");
					break;
				}

				if (!is_value_object_of_type(key_access_method_value, OBJECT_FUNCTION)) {
					RUNTIME_ERROR("Objects @get_key isn't a function.");
					break;
				}

				ObjectFunction* method_as_func = (ObjectFunction*) key_access_method_value.as.object;

				ValueArray arguments;
				value_array_init(&arguments);
				value_array_write(&arguments, &subject_value);
				value_array_write(&arguments, &key);

				Value result;
				if (method_as_func->isNative) {
					if (!method_as_func->nativeFunction(arguments, &result)) {
						RUNTIME_ERROR("@get_key function failed.");
						goto op_access_key_cleanup;
					}
					push(result);
				} else {
					// TODO: user function
				}

				op_access_key_cleanup:
				value_array_free(&arguments);

            	break;
            }

            case OP_JUMP_IF_FALSE: {
            	uint8_t addr_byte1 = READ_BYTE();
            	uint8_t addr_byte2 = READ_BYTE();
            	uint16_t address = twoBytesToShort(addr_byte1, addr_byte2);
				Value condition = pop();

				ERROR_IF_NON_BOOLEAN(condition, "Expected boolean as condition");

				if (!condition.as.boolean) {
					vm.ip = currentChunk()->code + address;
				}

				break;
			}

            case OP_JUMP: {
            	uint8_t addr_byte1 = READ_BYTE();
            	uint8_t addr_byte2 = READ_BYTE();
            	uint16_t address = twoBytesToShort(addr_byte1, addr_byte2);

            	vm.ip = currentChunk()->code + address;

            	break;
            }

            default: {
            	FAIL("Unknown opcode: %d", opcode);
            }
        }
    }

    DEBUG_TRACE("\n--------------------------\n");
	DEBUG_TRACE("Ended interpreter loop.");
    
    #undef BINARY_MATH_OP

    return runtimeErrorOccured ? INTERPRET_RUNTIME_ERROR : INTERPRET_SUCCESS;
}

#undef READ_BYTE
#undef READ_CONSTANT
