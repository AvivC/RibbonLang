#include <string.h>

#include "vm.h"
#include "common.h"
#include "value.h"
#include "object.h"
#include "memory.h"
#include "table.h"
#include "builtins.h"
#include "bytecode.h"
#include "disassembler.h"
#include "utils.h"
#include "pointerarray.h"
#include "io.h"

#define INITIAL_GC_THRESHOLD 1024 * 1024

VM vm;

static StackFrame* current_frame(void) {
	return vm.call_stack_top - 1;
}

static Bytecode* currentChunk(void) {
	return &current_frame()->function->code->bytecode;
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

static Value peek_at(int offset) {
	return *(vm.stackTop - offset);
}

static Value peek(void) {
	return peek_at(1);
}

static void init_stack_frame(StackFrame* frame) {
	frame->returnAddress = NULL;
	frame->function = NULL;
	cell_table_init(&frame->local_variables);
}

static StackFrame new_stack_frame(uint8_t* returnAddress, ObjectFunction* objFunc) {
	StackFrame frame;
	init_stack_frame(&frame);
	frame.returnAddress = returnAddress;
	frame.function = objFunc;
	return frame;
}

static StackFrame make_base_stack_frame(Bytecode* base_chunk) {
	ObjectCode* code = object_code_new(*base_chunk);
	ObjectFunction* baseObjFunc = object_user_function_new(code, NULL, 0, NULL, table_new_empty());
	return new_stack_frame(NULL, baseObjFunc);
}

static void push_frame(StackFrame frame) {
	*vm.call_stack_top = frame;
	vm.call_stack_top++;
}

static void freeStackFrame(StackFrame* frame) {
	cell_table_free(&frame->local_variables);
	init_stack_frame(frame);
}

static StackFrame popFrame(void) {
	vm.call_stack_top--;
	return *vm.call_stack_top;
}

static Value load_variable(ObjectString* name) {
	Value value;

	CellTable* locals = &current_frame()->local_variables;
	Table* free_vars = &current_frame()->function->free_vars;
	Table* globals = &vm.globals;

	bool variable_found = cell_table_get_value(locals, name, &value) || table_get(free_vars, name, &value) || table_get(globals, name, &value);

	if (variable_found) {
		return value;
	}

	// TODO: Nil when variable not found..? Should be an error
	return MAKE_VALUE_NIL();
}

static void gc_mark_object(Object* object);

static void gc_mark_code_constants(ObjectCode* code) {
	Bytecode* chunk = &code->bytecode;
	for (int i = 0; i < chunk->constants.count; i++) {
		Value* constant = &chunk->constants.values[i];
		if (constant->type == VALUE_OBJECT) {
			gc_mark_object(chunk->constants.values[i].as.object);
		}
	}

	// Note: not looking for bare VALUE_CHUNK here, because logically it should never happen.
	// Chunks should always be wrapped in ObjectCode in order to be managed "automatically" by the GC system.
}

static void gc_mark_object_attributes(Object* object) {
	// TODO: Pretty naive and inefficient - we scan the whole table in memory even though
	// many entries are likely to be empty
	for (int i = 0; i < object->attributes.capacity; i++) {
		Entry* entry = &object->attributes.entries[i];
		if (entry->value.type == VALUE_OBJECT) {
			gc_mark_object(entry->value.as.object);
		}
	}
}

static void assert_is_probably_valid_object(Object* object) {
	bool is_probably_valid_object = (object->is_reachable == true)
			|| (object->is_reachable == false);
	if (!is_probably_valid_object) {
		FAIL("Illegal object passed to gcMarkObject.");
	}
}

static void gc_mark_function_free_vars(ObjectFunction* function) {
	Table* free_vars = &function->free_vars; // Using a pointer locally for efficiency.. Is this common practice? Read on this.
	PointerArray free_vars_entries = table_iterate(free_vars);
	for (int i = 0; i < free_vars_entries.count; i++) {
		Entry* entry = free_vars_entries.values[i];
		if (entry->value.type == VALUE_OBJECT) {
			gc_mark_object(entry->value.as.object);
		}
	}
}

static void gc_mark_object_function(Object* object) {
	ObjectFunction* function = OBJECT_AS_FUNCTION(object);
	if (function->self != NULL) {
		gc_mark_object((Object*) function->self);
	}
	if (!function->is_native) {
		ObjectCode* code_object = function->code;
		gc_mark_object((Object*) code_object);
		gc_mark_code_constants(code_object);
		gc_mark_function_free_vars(function);
	}
}

static void gc_mark_object_table(Object* object) {
	Table* table = &((ObjectTable*) object)->table;
	PointerArray entries = table_iterate(table);

	for (int i = 0; i < entries.count; i++) {
		Entry* entry = entries.values[i];

		if (entry->key == NULL) {
			FAIL("Calling table_iterate returned an entry with a NULL key, shouldn't happen.");
		}

		Value* value = &entry->value;
		if (value->type == VALUE_OBJECT) {
			gc_mark_object(value->as.object);
		}
	}

	pointer_array_free(&entries);
}

static void gc_mark_object_cell(Object* object) {
	ObjectCell* cell = (ObjectCell*) object;
	if (cell->value.type == VALUE_OBJECT) {
		gc_mark_object(cell->value.as.object);
	}
}

static void gc_mark_object(Object* object) {
	assert_is_probably_valid_object(object);

	if (object->is_reachable) {
		return;
	}
	object->is_reachable = true;

	gc_mark_object_attributes(object);

	if (object->type == OBJECT_FUNCTION) {
		gc_mark_object_function(object);
	} else if (object->type == OBJECT_CODE) {
		gc_mark_code_constants((ObjectCode*) object);
	} else if (object->type == OBJECT_TABLE) {
		gc_mark_object_table(object);
	} else if (object->type == OBJECT_CELL) {
		gc_mark_object_cell(object);
	}
}

static void gc_mark(void) {
	for (Value* value = vm.evalStack; value != vm.stackTop; value++) {
		if (value->type == VALUE_OBJECT) {
			gc_mark_object(value->as.object);
		}
	}

	for (StackFrame* frame = vm.callStack; frame != vm.call_stack_top; frame++) {
		gc_mark_object((Object*) frame->function);

		// TODO: Pretty naive and inefficient - we scan the whole table in memory even though
		// many entries are likely to be empty
		for (int i = 0; i < frame->local_variables.table.capacity; i++) {
			Entry* entry = &frame->local_variables.table.entries[i];
			if (entry->key != NULL) {
				ObjectCell* cell = NULL;
				if ((cell = VALUE_AS_OBJECT(entry->value, OBJECT_CELL, ObjectCell)) == NULL) {
					FAIL("Found non ObjectCell when scanning CellTable for gc_mark.");
				}
				gc_mark_object((Object*) cell);
			}
			if (entry->value.type == VALUE_OBJECT) {
				gc_mark_object(entry->value.as.object);
			}
		}
	}

	// TODO: Pretty naive and inefficient - we scan the whole table in memory even though
	// many entries are likely to be empty
	for (int i = 0; i < vm.globals.capacity; i++) {
		Entry* entry = &vm.globals.entries[i];
		if (entry->value.type == VALUE_OBJECT) {
			gc_mark_object(entry->value.as.object);
		}
	}
}

static void gc_sweep(void) {
	Object** current = &vm.objects;
	while (*current != NULL) {
		if ((*current)->is_reachable) {
			(*current)->is_reachable = false;
			current = &((*current)->next);
		} else {
			Object* unreachable = *current;
			*current = unreachable->next;
			object_free(unreachable);
		}
	}
}

void gc(void) {
	#if DISABLE_GC

	DEBUG_GC_PRINT("GC would run, but is disabled");

	#else

	if (vm.allow_gc) {
		size_t memory_before_gc = get_allocated_memory();
		DEBUG_GC_PRINT("===== GC Running =====");
		DEBUG_GC_PRINT("num_objects: %d. max_objects: %d", vm.num_objects, vm.max_objects);
		DEBUG_GC_PRINT("Allocated memory: %d bytes", memory_before_gc);
		DEBUG_GC_PRINT("=======================");

		gc_mark();
		gc_sweep();

		vm.max_objects = vm.num_objects * 2;

		DEBUG_GC_PRINT("===== GC Finished =====");
		DEBUG_GC_PRINT("numObjects: %d. maxObjects: %d", vm.num_objects, vm.max_objects);
		DEBUG_GC_PRINT("Allocated memory before GC: %d bytes", memory_before_gc);
		DEBUG_GC_PRINT("Allocated memory after GC: %d bytes", get_allocated_memory());
		DEBUG_GC_PRINT("=======================");
	} else {
		DEBUG_GC_PRINT("GC should run, but vm.allow_gc is still false.");
	}

	#endif
}

static void reset_stacks(void) {
	vm.stackTop = vm.evalStack;
	vm.call_stack_top = vm.callStack;
}

static void register_builtin_function(const char* name, int num_params, char** params, NativeFunction function) {
	char** params_buffer = allocate(sizeof(char*) * num_params, "Parameters list cstrings");
	for (int i = 0; i < num_params; i++) {
		params_buffer[i] = copy_cstring(params[i], strlen(params[i]), "ObjectFunction param cstring");
	}
	ObjectFunction* obj_function = object_native_function_new(function, params_buffer, num_params, NULL);
	table_set_cstring_key(&vm.globals, name, MAKE_VALUE_OBJECT(obj_function));
}

static void set_builtin_globals(void) {
	register_builtin_function("print", 1, (char*[]) {"text"}, builtin_print);
	register_builtin_function("input", 0, NULL, builtin_input);
	register_builtin_function("read_file", 1, (char*[]) {"path"}, builtin_read_file);
}

static void call_user_function(ObjectFunction* function) {
	if (function->self != NULL) {
		push(MAKE_VALUE_OBJECT(function->self));
	}

	StackFrame frame = new_stack_frame(vm.ip, function);
	for (int i = 0; i < function->num_params; i++) {
		const char* paramName = function->parameters[i];
		Value argument = pop();
		cell_table_set_value_cstring_key(&frame.local_variables, paramName, argument);
	}
	push_frame(frame);
	vm.ip = function->code->bytecode.code;
}

static bool callNativeFunction(ObjectFunction* function) {
	if (function->self != NULL) {
		push(MAKE_VALUE_OBJECT(function->self));
	}

	ValueArray arguments;
	value_array_init(&arguments);
	for (int i = 0; i < function->num_params; i++) {
		Value value = pop();
		value_array_write(&arguments, &value);
	}

	Value result;
	bool func_success = function->native_function(arguments, &result);
	if (func_success) {
		push(result);
	} else {
		push(MAKE_VALUE_NIL());
	}

	value_array_free(&arguments);
	return func_success;
}

void vm_init(void) {
    vm.ip = NULL;

	reset_stacks();
    vm.num_objects = 0;
    vm.max_objects = INITIAL_GC_THRESHOLD;
    vm.allow_gc = false;

    table_init(&vm.globals);
    set_builtin_globals();
}

void vm_free(void) {
	for (StackFrame* frame = vm.callStack; frame != vm.call_stack_top; frame++) {
		freeStackFrame(frame);
	}
	reset_stacks();
	table_free(&vm.globals);

	gc(); // TODO: probably move upper

    vm.ip = NULL;
    vm.num_objects = 0;
    vm.max_objects = INITIAL_GC_THRESHOLD;
    vm.allow_gc = false;
}

static void print_stack_trace(void) {
	printf("Stack trace:\n");
	for (StackFrame* frame = vm.callStack; frame < vm.call_stack_top; frame++) {
		printf("    - %s\n", frame->function->name);
	}
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

InterpretResult vm_interpret(Bytecode* base_bytecode) {
    #define BINARY_MATH_OP(op) do { \
        Value b = pop(); \
        Value a = pop(); \
        Value result; \
        \
        if (a.type == VALUE_NUMBER && b.type == VALUE_NUMBER) { \
            result = MAKE_VALUE_NUMBER(a.as.number op b.as.number); \
            \
        } else { \
            result = MAKE_VALUE_NIL(); \
        } \
        push(result); \
    } while(false)

	// TODO: Implement an actual runtime error mechanism. This is a placeholder.
	#define RUNTIME_ERROR(...) do { \
		print_stack_trace(); \
		fprintf(stderr, "Runtime error: " __VA_ARGS__); \
		fprintf(stderr, "\n"); \
		runtime_error_occured = true; \
		is_executing = false; \
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

	push_frame(make_base_stack_frame(base_bytecode));

	vm.allow_gc = true;
	DEBUG_OBJECTS_PRINT("Set vm.allow_gc = true.");

	vm.ip = base_bytecode->code;
	gc(); // Cleanup unused objects the compiler created

	bool is_executing = true;
	bool runtime_error_occured = false;

	DEBUG_TRACE("Starting interpreter loop.");

    while (is_executing) {
		DEBUG_TRACE("\n--------------------------\n");
    	DEBUG_TRACE("IP: %p", vm.ip);
    	DEBUG_TRACE("numObjects: %d", vm.num_objects);
    	DEBUG_TRACE("maxObjects: %d", vm.max_objects);

    	OP_CODE opcode = READ_BYTE();
        
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
				printTable(&current_frame()->local_variables);
			} else {
				printf("No stack frames.");
			}

			chunk_print_constant_table(currentChunk());

			printf("\n\n");

			#if DEBUG_MEMORY_EXECUTION
				printAllObjects();
			#endif
		#endif

		// Possible that vm.num_objects > vm.max_objects if many objects were created during the compiling stage, where GC is disallowed
		if (vm.num_objects >= vm.max_objects) {
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
            	if (peek_at(2).type == VALUE_OBJECT) {
            		Value other = pop();
            		Value self_val = pop();

            		Object* self = self_val.as.object;
            		Value add_method;
            		if (!table_get_cstring_key(&self->attributes, "@add", &add_method)) {
            			RUNTIME_ERROR("Object doesn't support @add method.");
            			break;
            		}

					if (!object_is_value_object_of_type(add_method, OBJECT_FUNCTION)) {
						RUNTIME_ERROR("Objects @add isn't a function.");
						break;
					}

					ObjectFunction* add_method_as_func = (ObjectFunction*) add_method.as.object;

					ValueArray arguments;
					value_array_init(&arguments);
					value_array_write(&arguments, &self_val);
					value_array_write(&arguments, &other);

					Value result;
					if (add_method_as_func->is_native) {
						if (!add_method_as_func->native_function(arguments, &result)) {
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
            	ObjectString* obj_string = object_string_copy(string.data, string.length);
            	push(MAKE_VALUE_OBJECT(obj_string));
            	break;
            }

            case OP_MAKE_FUNCTION: {
				ObjectCode* object_code = READ_CONSTANT_AS_OBJECT(OBJECT_CODE, ObjectCode);

				uint8_t num_params_byte1 = READ_BYTE();
				uint8_t num_params_byte2 = READ_BYTE();
				uint16_t num_params = two_bytes_to_short(num_params_byte1, num_params_byte2);

				char** params_buffer = allocate(sizeof(char*) * num_params, "Parameters list cstrings");
				for (int i = 0; i < num_params; i++) {
					Value param_value = READ_CONSTANT();
					if (param_value.type != VALUE_RAW_STRING) {
						FAIL("Param constant expected to be VALUE_RAW_STRING, actual type: '%d'", param_value.type);
					}
					RawString param_raw_string = param_value.as.raw_string;
					params_buffer[i] = copy_cstring(param_raw_string.data, param_raw_string.length, "ObjectFunction param cstring");
				}

				IntegerArray referenced_names_indices = object_code->bytecode.referenced_names_indices;
				int free_vars_count = referenced_names_indices.count; // referenced_names_indices is the indices in the code's constant table
				Table free_vars;
				table_init(&free_vars);

				for (int i = 0; i < free_vars_count; i++) {
					size_t name_index = referenced_names_indices.values[i];
					Value name_value = object_code->bytecode.constants.values[name_index];

					if (!object_is_value_object_of_type(name_value, OBJECT_STRING)) {
						FAIL("Upvalue name must be an ObjectString*");
					}

					ObjectString* name_string = (ObjectString*) name_value.as.object;

					Value value;
					if (cell_table_get_value(&current_frame()->local_variables, name_string, &value)) {
						// Referenced name found in local variables of the enclosing function
						table_set(&free_vars, name_string, value);
					} else {
						Table* current_func_free_vars = &current_frame()->function->free_vars;
						if (table_get(current_func_free_vars, name_string, &value)) {
							table_set(&free_vars, name_string, value);
						}
						// If not found, the referenced variable is probably a local, or a runtime error later.
					}
				}

				ObjectFunction* obj_function = object_user_function_new(object_code, params_buffer, num_params, NULL, free_vars);
				push(MAKE_VALUE_OBJECT(obj_function));
				break;
			}

            case OP_MAKE_TABLE: {
            	Table table;
            	table_init(&table);

            	uint8_t num_entries = READ_BYTE();

            	for (int i = 0; i < num_entries; i++) {
					Value key = pop();
					if (!object_is_value_object_of_type(key, OBJECT_STRING)) {
						RUNTIME_ERROR("Currently only strings supported as table keys.");
						break;
					}

					ObjectString* string_key = (ObjectString*) key.as.object;

					Value value = pop();
					table_set(&table, string_key, value);
				}

            	ObjectTable* table_object = object_table_new(table);
            	push(MAKE_VALUE_OBJECT(table_object));

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
                	is_executing = false;
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
                Value name_value = currentChunk()->constants.values[READ_BYTE()];
                ASSERT_VALUE_TYPE(name_value, VALUE_OBJECT);
                ObjectString* name_string = object_as_string(name_value.as.object);
                push(load_variable(name_string));

                break;
            }
            
            case OP_SET_VARIABLE: {
                int constantIndex = READ_BYTE();
                Value name_val = currentChunk()->constants.values[constantIndex];
                ASSERT_VALUE_TYPE(name_val, VALUE_OBJECT);
                ObjectString* name = OBJECT_AS_STRING(name_val.as.object);

                Value value = pop();

                if (object_is_value_object_of_type(value, OBJECT_FUNCTION)) {
                	ObjectFunction* function = (ObjectFunction*) value.as.object;
                	deallocate(function->name, strlen(function->name) + 1, "Function name");
                	char* new_cstring_name = copy_null_terminated_cstring(name->chars, "Function name");
                	object_function_set_name(function, new_cstring_name); // TODO: Consider concatenating assigned names, or something.
                																		// In case a function has more than one name.
                																		// Or maybe a function should have a list of names?
                }

                cell_table_set_value(&current_frame()->local_variables, name, value);
                break;
            }
            
            case OP_CALL: {
            	int explicit_arg_count = READ_BYTE();

                if (peek().type != VALUE_OBJECT || (peek().type == VALUE_OBJECT && peek().as.object->type != OBJECT_FUNCTION)) {
                	RUNTIME_ERROR("Illegal call target. Target type: %d.", peek().type);
                	break;
                }

                ObjectFunction* function = OBJECT_AS_FUNCTION(pop().as.object);

                bool is_method = function->self != NULL;
                int actual_arg_count = explicit_arg_count + (is_method ? 1 : 0);
                if (actual_arg_count != function->num_params) {
                	RUNTIME_ERROR("Function called with %d arguments, needs %d.", explicit_arg_count, function->num_params);
                	break;
                }

                if (function->is_native) {
                	if (!callNativeFunction(function)) {
                		RUNTIME_ERROR("Native function failed.");
                		break;
                	}
                } else {
                	// TODO: Handle errors
                	call_user_function(function);
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
                if (table_get(&object->attributes, name, &attr_value)) {
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
                table_set(&object->attributes, name, attribute_value);

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
				if (!table_get_cstring_key(&subject->attributes, "@get_key", &key_access_method_value)) {
					RUNTIME_ERROR("Object doesn't support @get_key method.");
					break;
				}

				if (!object_is_value_object_of_type(key_access_method_value, OBJECT_FUNCTION)) {
					RUNTIME_ERROR("Object's @get_key isn't a function.");
					break;
				}

				ObjectFunction* method_as_func = (ObjectFunction*) key_access_method_value.as.object;

				ValueArray arguments;
				value_array_init(&arguments);
				value_array_write(&arguments, &subject_value);
				value_array_write(&arguments, &key);

				Value result;
				if (method_as_func->is_native) {
					if (!method_as_func->native_function(arguments, &result)) {
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

            case OP_SET_KEY: {
            	Value subject_as_value = pop();
            	Value key = pop();
            	Value value = pop();

            	if (subject_as_value.type != VALUE_OBJECT) {
            		RUNTIME_ERROR("Cannot set key on non-object.");
            		break;
            	}

            	Object* subject = subject_as_value.as.object;

            	ObjectFunction* set_method = NULL;
            	MethodAccessResult access_result = -1;
            	if ((access_result = object_get_method(subject, "@set_key", &set_method)) == METHOD_ACCESS_SUCCESS) {
    				ValueArray arguments;
    				value_array_init(&arguments);
    				value_array_write(&arguments, &subject_as_value);
    				value_array_write(&arguments, &key);
    				value_array_write(&arguments, &value);

    				Value result;
    				if (set_method->is_native) {
    					if (!set_method->native_function(arguments, &result)) {
    						RUNTIME_ERROR("@set_key function failed.");
    						goto op_set_key_cleanup;
    					}
    					push(result);
    				} else {
    					// TODO: user function
    				}

    				op_set_key_cleanup:
    				value_array_free(&arguments);
            	} else if (access_result == METHOD_ACCESS_NO_SUCH_ATTR) {
            		RUNTIME_ERROR("Object doesn't support @set_key method.");
            		break;
            	} else if (access_result == METHOD_ACCESS_ATTR_NOT_FUNCTION) {
            		RUNTIME_ERROR("Object's @set_key isn't a function.");
            		break;
            	} else {
            		FAIL("Illegal value for access_result: %d", access_result);
            	}

            	break;
            }

            case OP_JUMP_IF_FALSE: {
            	uint8_t addr_byte1 = READ_BYTE();
            	uint8_t addr_byte2 = READ_BYTE();
            	uint16_t address = two_bytes_to_short(addr_byte1, addr_byte2);
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
            	uint16_t address = two_bytes_to_short(addr_byte1, addr_byte2);

            	vm.ip = currentChunk()->code + address;

            	break;
            }

            case OP_IMPORT: {
            	ObjectString* module_name = READ_CONSTANT_AS_OBJECT(OBJECT_STRING, ObjectString);

            	const char* file_name_suffix = ".pln";
				const char* file_name_alloc_string = "File name buffer";
				size_t file_name_buffer_size = module_name->length + strlen(file_name_suffix) + 1;
				char* file_name_buffer = allocate(file_name_buffer_size, file_name_alloc_string);
				memcpy(file_name_buffer, module_name->chars, module_name->length);
				memcpy(file_name_buffer + module_name->length, file_name_suffix, strlen(file_name_suffix));
				file_name_buffer[module_name->length + strlen(file_name_suffix)] = '\0';

				char* source = NULL;
				size_t source_buffer_size = -1;
				int file_read_success = read_file(file_name_buffer, &source, &source_buffer_size); // TODO: Figure out how to manage this memory

				// TODO: This is the basis for the module system. Come back to this after we have closures.

//				if (file_read_success == IO_SUCCESS) {
////            	    initVM();
//					Chunk module_chunk;
//					initChunk(&module_chunk);
//					AstNode* module_ast = parse(source);
//					compile(module_ast, &module_chunk);
//
//					ImportFrame = new_import_frame(vm.ip, )
//					uint8_t* return_ip = vm.ip;
//					InterpretResult result = interpret(&module_chunk);
//					vm.ip = return_ip;
//
//
////					if (result == INTERPRET_SUCCESS) {
//
////					} else {
//
////					}
//
//					freeTree(module_ast);
////            	    freeVM();
////            	    free(source);

				deallocate(file_name_buffer, file_name_buffer_size, "File name buffer");
				deallocate(source, source_buffer_size, "File content buffer");
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

    return runtime_error_occured ? INTERPRET_RUNTIME_ERROR : INTERPRET_SUCCESS;
}

#undef READ_BYTE
#undef READ_CONSTANT
