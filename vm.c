#include <string.h>
#include <math.h>
#include <windows.h>
#include <time.h>

#include "vm.h"
#include "plane.h"
#include "common.h"
#include "value.h"
#include "plane_object.h"
#include "memory.h"
#include "table.h"
#include "builtins.h"
#include "bytecode.h"
#include "disassembler.h"
#include "plane_utils.h"
#include "pointerarray.h"
#include "io.h"
#include "ast.h"
#include "parser.h"
#include "compiler.h"
#include "builtin_test_module.h"

#define INITIAL_GC_THRESHOLD 10

#define VM_STDLIB_RELATIVE_PATH "stdlib"

VM vm;

static StackFrame* current_frame(void) {
	return vm.call_stack_top - 1;
}

static Bytecode* current_bytecode(void) {
	return &current_frame()->function->code->bytecode;
}

static void push(Value value) {
	assert(vm.stack_top - vm.stack <= EVAL_STACK_MAX);

	*vm.stack_top = value;
    vm.stack_top++;
}

static Value pop(void) {
	assert(vm.stack_top > vm.stack);

	vm.stack_top--;
    return *vm.stack_top;
}

/* TODO: Consider having vm_push_object and vm_pop_object work against
   a different stack. This way in the memory diagnostics stage at the end, we can validate
   that the stack has nothing on it, i.e. that extensions didn't forget nothing on it and leaked memory. 
   For now, they work against the regular value stack and it should be fine for now.
   */

void vm_push_object(Object* object) {
	push(MAKE_VALUE_OBJECT(object));
}

Object* vm_pop_object() {
	Value value = pop();
	assert(value.type == VALUE_OBJECT);
	return value.as.object;
}

static Value peek_at(int offset) {
	return *(vm.stack_top - offset);
}

static Value peek(void) {
	return peek_at(1);
}

static void init_stack_frame(StackFrame* frame) {
	frame->return_address = NULL;
	frame->function = NULL;
	frame->base_entity = NULL;
	frame->is_entity_base = false;
	frame->is_native = false;
	frame->discard_return_value = false;
	cell_table_init(&frame->local_variables);
}

static StackFrame new_stack_frame(
		uint8_t* return_address, ObjectFunction* function, 
		Object* base_entity, bool is_entity_base, bool is_native, bool discard_return_value) {
	StackFrame frame;
	init_stack_frame(&frame);
	frame.return_address = return_address;
	frame.function = function;
	frame.base_entity = base_entity;
	frame.is_entity_base = is_entity_base;
	frame.is_native = is_native;
	frame.discard_return_value = discard_return_value;
	return frame;
}

static bool push_frame(StackFrame frame) {
	if (vm.call_stack_top - vm.call_stack == CALL_STACK_MAX) {
		return false;
	}

	*vm.call_stack_top = frame;
	vm.call_stack_top++;

	return true;
}

static void stack_frame_free(StackFrame* frame) {
	cell_table_free(&frame->local_variables);
	init_stack_frame(frame);
}

static StackFrame pop_frame(void) {
	if (vm.call_stack_top <= vm.call_stack) {
		FAIL("Stack underflow - should never happen.");
	}
	vm.call_stack_top--;
	return *vm.call_stack_top;
}

static StackFrame* peek_frame(int offset) {
	if ((vm.call_stack_top - vm.call_stack) < offset) {
		return NULL;
	}
	return vm.call_stack_top - offset;
}

StackFrame* vm_peek_current_frame(void) {
	return peek_frame(1);
}

StackFrame* vm_peek_previous_frame(void) {
	return peek_frame(2);
}

static CellTable* frame_locals_or_module_table(StackFrame* frame) {
	return frame->is_entity_base ? &frame->base_entity->attributes : &frame->local_variables;
}

/* The "correct" locals table depends on whether we are the base function of a module or class or not...
 * This design isn't great, should change it later. */
static CellTable* locals_or_module_table(void) {
	return frame_locals_or_module_table(current_frame());
}

static bool load_variable(ObjectString* name, Value* out) {
	Value value;

	CellTable* locals = &current_frame()->local_variables;
	CellTable* free_vars = &current_frame()->function->free_vars;
	CellTable* globals = &vm.globals;

	bool variable_found =
			cell_table_get_value(locals, name, &value)
			|| cell_table_get_value(free_vars, name, &value)
			|| (current_frame()->is_entity_base && object_load_attribute(current_frame()->base_entity, name, &value))
			|| cell_table_get_value(globals, name, &value);

	if (variable_found) {
		*out = value;
		return true;
	}

	return false;
}

static void gc_mark_object(Object* object);

static void gc_mark_table(Table* table) {
	/* Gross breaking of table.c encapsulation here, soley for improving proven performance problems. */

	Entry* entries = table->entries;
	size_t count = table->capacity;

	for (Entry* entry = entries; entry - entries < count; entry++) {
		if (entry->key.type != VALUE_NIL && entry->tombstone == 0) {
			if (entry->value.type == VALUE_OBJECT) {
				gc_mark_object(entry->value.as.object);
			}
			if (entry->key.type == VALUE_OBJECT) {
				gc_mark_object(entry->key.as.object);
			}
		}
	}
}

static void gc_mark_object_code(Object* object) {
	ObjectCode* code = (ObjectCode*) object;
	Bytecode* chunk = &code->bytecode;
	for (int i = 0; i < chunk->constants.count; i++) {
		Value* constant = &chunk->constants.values[i];
		if (constant->type == VALUE_OBJECT) {
			gc_mark_object(constant->as.object);
		}
	}
}

static void gc_mark_object_attributes(Object* object) {
	gc_mark_table(&object->attributes.table);
}

static void assert_is_probably_valid_object(Object* object) {
	assert((object->is_reachable == true) || (object->is_reachable == false));
}

static void gc_mark_function_free_vars(ObjectFunction* function) {
	gc_mark_table(&function->free_vars.table);
}

static void gc_mark_object_function(Object* object) {
	ObjectFunction* function = OBJECT_AS_FUNCTION(object);

	for (int i = 0; i < function->num_params; i++) {
		ObjectString* param = function->parameters[i];
		gc_mark_object((Object*) param);
	}

	if (!function->is_native) {
		ObjectCode* code_object = function->code;
		gc_mark_object((Object*) code_object);
		gc_mark_function_free_vars(function);
	}
}

static void gc_mark_object_table(Object* object) {
	gc_mark_table(&((ObjectTable*) object)->table);
}

static void gc_mark_object_cell(Object* object) {
	ObjectCell* cell = (ObjectCell*) object;
	if (cell->is_filled && cell->value.type == VALUE_OBJECT) {
		gc_mark_object(cell->value.as.object);
	}
}

static void gc_mark_object_module(Object* object) {
	ObjectModule* module = (ObjectModule*) object;

	gc_mark_object((Object*) module->name);
	if (module->function != NULL) { /* function can be NULL if module is native */
		gc_mark_object((Object*) module->function);
	}
}

static void gc_mark_object_class(Object* object) {
	ObjectClass* klass = (ObjectClass*) object;

	if (klass->base_function != NULL) {
		gc_mark_object((Object*) klass->base_function);
	}
	
	if (klass->superclass != NULL) {
		gc_mark_object((Object*) klass->superclass);
	}
}

static void gc_mark_object_instance(Object* object) {
	ObjectInstance* instance = (ObjectInstance*) object;
	ObjectClass* klass = instance->klass;

	gc_mark_object((Object*) klass);

	if (klass->instance_size > 0) {
		/* Native class */

		if (klass->gc_mark_func != NULL && instance->is_initialized) {
			Object** leefs = klass->gc_mark_func(instance);
			size_t count = 1; /* Include NULL terminator */
			for (Object** leef = leefs; *leef != NULL; leef++, count++) {
				gc_mark_object(*leef);
			}
			deallocate(leefs, sizeof(Object*) * count, API.EXTENSION_ALLOC_STRING_GC_LEEFS);
		}
	}
}

static void gc_mark_object_bound_method(Object* object) {
	ObjectBoundMethod* bound_method = (ObjectBoundMethod*) object;
	gc_mark_object(bound_method->self);
	gc_mark_object((Object*) bound_method->method);
}

static void gc_mark_object(Object* object) {
	assert_is_probably_valid_object(object);

	if (object->is_reachable) {
		return;
	}
	object->is_reachable = true;

	gc_mark_object_attributes(object);

	switch (object->type) {
		case OBJECT_FUNCTION: {
			gc_mark_object_function(object);
			return;
		}
		case OBJECT_CODE: {
			gc_mark_object_code(object);
			return;
		}
		case OBJECT_TABLE: {
			gc_mark_object_table(object);
			return;
		}
		case OBJECT_CELL: {
			gc_mark_object_cell(object);
			return;
		}
		case OBJECT_MODULE: {
			gc_mark_object_module(object);
			return;
		}
		case OBJECT_STRING: {
			/* Nothing. Currently strings don't link to any other object except for their attributes, which were already marked. */
			return;
		}
		case OBJECT_CLASS: {
			gc_mark_object_class(object);
			return;
		}
		case OBJECT_INSTANCE: {
			gc_mark_object_instance(object);
			return;
		}
		case OBJECT_BOUND_METHOD: {
			gc_mark_object_bound_method(object);
			return;
		}
	}

	FAIL("GC couldn't mark object, unknown object type: %d", object->type);
}

static void gc_mark(void) {
	gc_mark_table(&vm.globals.table);
	gc_mark_table(&vm.imported_modules.table);
	gc_mark_table(&vm.builtin_modules.table);

	for (Value* value = vm.stack; value != vm.stack_top; value++) {
		if (value->type == VALUE_OBJECT) {
			gc_mark_object(value->as.object);
		}
	}
	
	for (StackFrame* frame = vm.call_stack; frame != vm.call_stack_top; frame++) {
		gc_mark_object((Object*) frame->function);
		if (frame->base_entity != NULL) {
			gc_mark_object(frame->base_entity);
		}

		gc_mark_table(&frame_locals_or_module_table(frame)->table);
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

/* vm_gc is only exposed outside for use in the _testing module. Apart from that, never invoke
   directly without a good reason. */
void vm_gc(void) {
	#if DISABLE_GC

	DEBUG_GC_PRINT("GC would run, but is disabled");

	#else

	if (vm.allow_gc) {
		size_t memory_before_gc = get_allocated_memory();
		DEBUG_GC_PRINT("===== GC Running =====");
		DEBUG_GC_PRINT("num_objects: %d. max_objects: %d", vm.num_objects, vm.max_objects);
		DEBUG_GC_PRINT("Allocated memory: %" PRI_SIZET " bytes", memory_before_gc);
		DEBUG_GC_PRINT("=======================");

		gc_mark();
		gc_sweep();

		vm.max_objects = vm.num_objects * 2;

		DEBUG_GC_PRINT("===== GC Finished =====");
		DEBUG_GC_PRINT("numObjects: %d. maxObjects: %d", vm.num_objects, vm.max_objects);
		DEBUG_GC_PRINT("Allocated memory before GC: %" PRI_SIZET " bytes", memory_before_gc);
		DEBUG_GC_PRINT("Allocated memory after GC: %" PRI_SIZET " bytes", get_allocated_memory());
		DEBUG_GC_PRINT("=======================");
	} else {
		DEBUG_GC_PRINT("GC should run, but vm.allow_gc is still false.");
	}

	#endif
}

static ValueArray collect_values(int count) {
	ValueArray values;
	value_array_init(&values);
	for (int i = 0; i < count; i++) {
		Value value = pop();
		value_array_write(&values, &value);
	}
	return values;
}

static void register_builtin_function(char* name, int num_params, char** params, NativeFunction function) {
	ObjectFunction* obj_function = make_native_function_with_params(name, num_params, params, function);
	cell_table_set_value_cstring_key(&vm.globals, name, MAKE_VALUE_OBJECT(obj_function));
}

static void set_builtin_globals(void) {
	register_builtin_function("print", 1, (char*[]) {"text"}, builtin_print);
	register_builtin_function("input", 0, NULL, builtin_input);
	register_builtin_function("read_file", 1, (char*[]) {"path"}, builtin_read_file);
	register_builtin_function("write_file", 2, (char*[]) {"path", "text"}, builtin_write_file);
	register_builtin_function("delete_file", 1, (char*[]) {"path"}, builtin_delete_file);
	register_builtin_function("file_exists", 1, (char*[]) {"path"}, builtin_file_exists);
	register_builtin_function("to_number", 1, (char*[]) {"value"}, builtin_to_number);
	register_builtin_function("to_string", 1, (char*[]) {"value"}, builtin_to_string);
	register_builtin_function("time", 0, NULL, builtin_time);
	register_builtin_function("has_attribute", 2, (char*[]) {"object", "attribute"}, builtin_has_attr);
	register_builtin_function("random", 0, NULL, builtin_random);
	register_builtin_function("get_main_file_path", 0, NULL, builtin_get_main_file_path);
	register_builtin_function("get_main_file_directory", 0, NULL, builtin_get_main_file_directory);
	register_builtin_function("is_instance", 2, (char*[]) {"value", "type_name"}, builtin_is_instance);
	register_builtin_function("type", 1, (char*[]) {"value"}, builtin_get_type);
	register_builtin_function("super", 1, (char*[]) {"args_table"}, builtin_super);
}

static void register_function_on_module(ObjectModule* module, char* name, int num_params, char* params[], NativeFunction func) {
	ObjectFunction* func_object = make_native_function_with_params(name, num_params, params, func);
	object_set_attribute_cstring_key((Object*) module, name, MAKE_VALUE_OBJECT(func_object));
}

static void register_builtin_modules(void) {
	/* Now that we have stdlib extension modules, genernally we shouldn't have too many (if any) builtin modules.
	   Nonetheless, this stays here at least for now. */

	const char* test_module_name = "_testing";
	ObjectModule* test_module = object_module_native_new(object_string_copy_from_null_terminated(test_module_name), NULL);

	register_function_on_module(test_module, "demo_print", 1, (char*[]) {"function"}, builtin_test_demo_print);

	register_function_on_module(test_module, "call_callback_with_args", 3, (char*[]) {"callback", "arg1", "arg2"}, builtin_test_call_callback_with_args);

	register_function_on_module(test_module, "same_object", 2, (char*[]) {"object1", "object2"}, builtin_test_same_object);

	register_function_on_module(test_module, "get_object_address", 1, (char*[]) {"object"}, builtin_test_get_object_address);

	register_function_on_module(test_module, "get_value_directly_from_object_attributes", 2, (char*[]) {"object", "attribute"}, builtin_test_get_value_directly_from_object_attributes);

	register_function_on_module(test_module, "gc", 0, NULL, builtin_test_gc);

	register_function_on_module(test_module, "table_details", 1, (char*[]) {"table"}, builtin_test_table_details);
	
	register_function_on_module(test_module, "table_delete", 2, (char*[]) {"table", "key"}, builtin_test_table_delete);

	cell_table_set_value_cstring_key(&vm.builtin_modules, test_module_name, MAKE_VALUE_OBJECT(test_module));
}

static bool call_native_function(ObjectFunction* function, Object* self, ValueArray arguments, Value* out) {
	/* Push a native frame to keep the call stack in order.
	Specifically, for the use case where a native function calls a user function.
	Without this "native frame", OP_RETURN in the user function will make control jump
	back to the point where the caller _native_ function was called. */
	StackFrame native_frame = new_stack_frame(NULL, function, NULL, false, true, false);
	if (!push_frame(native_frame)) {
		return false;
	}

	/* If the native function will end up calling a user function, the instruction
	pointer will be moving forward. When we go back to this calling site, we have to
	also remember to restore the instruction pointer. */
	uint8_t* ip_before_call = vm.ip;

	Value result;
	bool func_success = function->native_function(self, arguments, &result);
	if (func_success) {
		*out = result;
	} else {
		/* TODO: Probably a bug! Some functions still push NIL even if returning false. Need to sort this
		in one way or the other. */
		*out = MAKE_VALUE_NIL();
	}

	/* Pop and free the native frame */
	StackFrame used_frame = pop_frame();
	stack_frame_free(&used_frame);

	/* restore ip position */
	vm.ip = ip_before_call;

	return func_success;
}

static bool call_native_function_args_from_stack(ObjectFunction* function, Object* self) {
	ValueArray arguments = collect_values(function->num_params);

	Value result;
	bool success = call_native_function(function, self, arguments, &result);
	value_array_free(&arguments);
	push(result);

	return success;
}

void vm_init(void) {
	vm.currently_handling_error = false;

	vm.stack_top = vm.stack;
	vm.call_stack_top = vm.call_stack;

    vm.num_objects = 0;
    vm.max_objects = INITIAL_GC_THRESHOLD;
    vm.allow_gc = false;
    vm.imported_modules = cell_table_new_empty();
    vm.builtin_modules = cell_table_new_empty();
	table_init(&vm.string_cache); /* Must appear before the rest of the function - because other functions
	                                 may create strings, and then table_init would lose hold of and leak them */
    cell_table_init(&vm.globals);
    set_builtin_globals();
	register_builtin_modules();

	vm.main_module_path = NULL;
	vm.interpreter_dir_path = find_interpreter_directory();

	srand(time(NULL)); /* Our builtin random() function needs the randomizer seeded */
}

void vm_free(void) {
	vm.currently_handling_error = false;

	#if DEBUG_TABLE_STATS
	table_debug_print_general_stats();
	#endif

	vm.stack_top = vm.stack;
	vm.call_stack_top = vm.call_stack;

	cell_table_free(&vm.globals);
	cell_table_free(&vm.imported_modules);
	cell_table_free(&vm.builtin_modules);
	table_free(&vm.string_cache);	

	vm_gc();

    vm.num_objects = 0;
    vm.max_objects = INITIAL_GC_THRESHOLD;
    vm.allow_gc = false;

	/* These two will be NULL if we are running in -dry mode. Maybe this isn't the best solution, but for now it's fine. */
	if (vm.main_module_path != NULL) {
		deallocate(vm.main_module_path, strlen(vm.main_module_path) + 1, "main module absolute path");
	}
	if (vm.interpreter_dir_path != NULL) {
		deallocate(vm.interpreter_dir_path, strlen(vm.interpreter_dir_path) + 1, "interpreter directory path");
	}
}

static void print_call_stack(void) {
	const int MAX_FRAMES_VISUALIZE = 40;
	StackFrame* stopping_point = vm.call_stack_top - vm.call_stack <= MAX_FRAMES_VISUALIZE ? vm.call_stack : vm.call_stack_top - MAX_FRAMES_VISUALIZE;
	for (StackFrame* frame = vm.call_stack_top - 1; frame >= stopping_point; frame--) {
		printf("    -> %s\n", frame->function->name);
	}
	if (stopping_point != vm.call_stack) {
		printf("    -> [... %d lower frames truncated ...]\n", (int) (stopping_point - vm.call_stack));
	}
}

static void print_stack_trace(void) {
	printf("An error has occured. Stack trace (most recent call on top):\n");
	print_call_stack();
}

static void set_function_name(const Value* function_value, ObjectString* name) {
	ObjectFunction* function = (ObjectFunction*) function_value->as.object;
	char* new_cstring_name = copy_null_terminated_cstring(name->chars, "Function name");
	object_function_set_name(function, new_cstring_name);
}

static void set_class_name(const Value* class_value, ObjectString* name) {
	ObjectClass* klass = (ObjectClass*) class_value->as.object;
	deallocate(klass->name, strlen(klass->name) + 1, "Class name");
	char* new_cstring_name = copy_null_terminated_cstring(name->chars, "Class name");
	object_class_set_name(klass, new_cstring_name);
}

static bool call_plane_function(
		ObjectFunction* function, Object* self, ValueArray args, Object* base_entity, Value* out);

static ImportResult load_text_module(ObjectString* module_name, const char* file_name_buffer) {
	char* source = NULL;
	size_t source_buffer_size = -1;
	IOResult file_read_result = io_read_file(file_name_buffer, "File content buffer", &source, &source_buffer_size);

	switch (file_read_result) {
		case IO_SUCCESS: {

			/* Parse text to AST */
			AstNode* module_ast = parser_parse(source);

			/* Compile AST to bytecode */
			Bytecode module_bytecode;
			bytecode_init(&module_bytecode);
			compiler_compile(module_ast, &module_bytecode);

			/* Free the no longer needed AST and source text */
			ast_free_tree(module_ast);
			deallocate(source, source_buffer_size, "File content buffer");

			/* Wrap the Bytecode in an ObjectCode, and wrap the ObjectCode in an ObjectFunction */
			ObjectCode* code_object = object_code_new(module_bytecode);
			ObjectFunction* module_base_function = object_user_function_new(code_object, NULL, 0, cell_table_new_empty());

			/* VERY ugly temporary hack ahead */
			set_function_name(&MAKE_VALUE_OBJECT(module_base_function), object_string_copy_from_null_terminated("<Module base function>"));

			/* Wrap the ObjectFunction in an ObjectModule */
			ObjectModule* module = object_module_new(module_name, module_base_function);

			/* Call the new module to initialize it */
			ValueArray args;
			value_array_init(&args);
			Value throwaway_result;
			call_plane_function(module_base_function, NULL, args, (Object*) module, &throwaway_result);
			value_array_free(&args);

			cell_table_set_value(locals_or_module_table(), module_name, MAKE_VALUE_OBJECT(module));

			/* Cache the new module in the global module cache */
			cell_table_set_value(&vm.imported_modules, module_name, MAKE_VALUE_OBJECT(module));

			return IMPORT_RESULT_SUCCESS;
		}
		case IO_CLOSE_FILE_FAILURE: {
			return IMPORT_RESULT_CLOSE_FAILED;
		}
		case IO_READ_FILE_FAILURE: {
			return IMPORT_RESULT_READ_FAILED;
		}
		case IO_OPEN_FILE_FAILURE: {
			return IMPORT_RESULT_OPEN_FAILED;
		}
		case IO_WRITE_FILE_FAILURE: {
			FAIL("During module import, IO_WRITE_FILE_FAILURE returned. Should never happen.");
			return IMPORT_RESULT_READ_FAILED;
		}
		case IO_DELETE_FILE_FAILURE: {
			FAIL("During module import, IO_DELETE_FILE_FAILURE returned. Should never happen.");
			return IMPORT_RESULT_READ_FAILED;
		}
	}

	FAIL("load_text_module - shouldn't reach here.");
	return IMPORT_RESULT_READ_FAILED;
}

static bool call_plane_function_leave_on_stack(
		ObjectFunction* function, Object* self, ValueArray args, Object* base_entity);

static CallResult call_function_leave_on_stack(ObjectFunction* function, ValueArray args) {
	if (args.count != function->num_params) {
		return CALL_RESULT_INVALID_ARGUMENT_COUNT;
	}

	if (function->is_native) {
		Value out;
		if (call_native_function(function, NULL, args, &out)) {
			push(out);
			return CALL_RESULT_SUCCESS;
		}
		/* Push nil here? We are very incosistent about it throughout the system. */
		return CALL_RESULT_NATIVE_EXECUTION_FAILED;
	}

	if (call_plane_function_leave_on_stack(function, NULL, args, NULL)) {
		return CALL_RESULT_SUCCESS;
	}
	return CALL_RESULT_PLANE_CODE_EXECUTION_FAILED;
}

CallResult vm_call_function(ObjectFunction* function, ValueArray args, Value* out) {
	CallResult result = call_function_leave_on_stack(function, args);
	if (result == CALL_RESULT_SUCCESS) {
		*out = pop();
	}
	return result;
}

static CallResult call_bound_method_leave_on_stack(ObjectBoundMethod* bound_method, ValueArray args) {
	ObjectFunction* method = bound_method->method;

	if (method->num_params != args.count) {
		return CALL_RESULT_INVALID_ARGUMENT_COUNT;
	}

	if (method->is_native) {
		Value out;
		if (call_native_function(method, bound_method->self, args, &out)) {
			push(out);
			return CALL_RESULT_SUCCESS;
		}
		return CALL_RESULT_NATIVE_EXECUTION_FAILED;
	}

	if (call_plane_function_leave_on_stack(method, bound_method->self, args, NULL)) {
		return CALL_RESULT_SUCCESS;
	}
	return CALL_RESULT_PLANE_CODE_EXECUTION_FAILED;
}

CallResult vm_call_bound_method(ObjectBoundMethod* bound_method, ValueArray args, Value* out) {
	CallResult result = call_bound_method_leave_on_stack(bound_method, args);
	if (result == CALL_RESULT_SUCCESS) {
		*out = pop();
	}
	return result;
}


static CallResult instantiate_class_leave_on_stack(ObjectClass* klass, ValueArray args) {
	ObjectInstance* instance = object_instance_new(klass);

	Value init_method_value;
	if (object_load_attribute_cstring_key((Object*) instance, "@init", &init_method_value)) {
		ObjectBoundMethod* init_bound_method = NULL;
		if ((init_bound_method = VALUE_AS_OBJECT(init_method_value, OBJECT_BOUND_METHOD, ObjectBoundMethod)) == NULL) {
			return CALL_RESULT_CLASS_INIT_NOT_METHOD;
		}

		Object* self = init_bound_method->self;
		assert(self == (Object*) instance);

		Value throwaway;
		CallResult call_result = vm_call_bound_method(init_bound_method, args, &throwaway);
		if (call_result != CALL_RESULT_SUCCESS) {
			return call_result;
		}
	} else if (args.count != 0) {
		return CALL_RESULT_INVALID_ARGUMENT_COUNT;
	}

	instance->is_initialized = true;
	push(MAKE_VALUE_OBJECT(instance));
	return CALL_RESULT_SUCCESS;
}

CallResult vm_instantiate_class(ObjectClass* klass, ValueArray args, Value* out) {
	/* TODO: Somehow out argument as Object**, not Value*? */
	CallResult result = instantiate_class_leave_on_stack(klass, args);
	if (result == CALL_RESULT_SUCCESS) {
		*out = pop();
	}
	return result;
}

CallResult vm_instantiate_class_no_args(ObjectClass* klass, Value* out) {
	ValueArray args = value_array_make(0, NULL);
	CallResult result = vm_instantiate_class(klass, args, out);
	value_array_free(&args);
	return result;
}

CallResult vm_call_object(Object* object, ValueArray args, Value* out) {
	switch (object->type) {
		case OBJECT_FUNCTION:
			return vm_call_function((ObjectFunction*) object, args, out);
		case OBJECT_BOUND_METHOD:
			return vm_call_bound_method((ObjectBoundMethod*) object, args, out);
		case OBJECT_CLASS:
			return vm_instantiate_class((ObjectClass*) object, args, out);
		default:
			return CALL_RESULT_INVALID_CALLABLE;	
	}
}

static CallResult call_object_leave_on_stack(Object* object, ValueArray args) {
	switch (object->type) {
		case OBJECT_FUNCTION:
			return call_function_leave_on_stack((ObjectFunction*) object, args);
		case OBJECT_BOUND_METHOD:
			return call_bound_method_leave_on_stack((ObjectBoundMethod*) object, args);
		case OBJECT_CLASS:
			return instantiate_class_leave_on_stack((ObjectClass*) object, args);
		default:
			return CALL_RESULT_INVALID_CALLABLE;	
	}
}

CallResult vm_call_attribute(Object* object, ObjectString* name, ValueArray args, Value* out) {
	Value attrval;
	if (!object_load_attribute(object, name, &attrval)) {
		return CALL_RESULT_NO_SUCH_ATTRIBUTE;
	}

	if (attrval.type != VALUE_OBJECT) {
		return CALL_RESULT_INVALID_CALLABLE;
	}

	Object* callee = attrval.as.object;

	return vm_call_object(callee, args, out);
}

CallResult vm_call_attribute_cstring(Object* object, char* name, ValueArray args, Value* out) {
	return vm_call_attribute(object, object_string_copy_from_null_terminated(name), args, out);
}

static ImportResult load_extension_module(ObjectString* module_name, char* path) {
	HMODULE handle = LoadLibraryExA(path, NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);

	if (handle == NULL) {
		return IMPORT_RESULT_OPEN_FAILED;
	}

	ExtensionInitFunction init_function = (ExtensionInitFunction) GetProcAddress(handle, "plane_module_init");

	if (init_function == NULL) {
		FreeLibrary(handle);
		return IMPORT_RESULT_EXTENSION_NO_INIT_FUNCTION;
	}

	ObjectModule* extension_module = object_module_native_new(module_name, handle);
	init_function(API, extension_module);

	cell_table_set_value(locals_or_module_table(), module_name, MAKE_VALUE_OBJECT(extension_module));

	cell_table_set_value(&vm.imported_modules, module_name, MAKE_VALUE_OBJECT(extension_module));

	return IMPORT_RESULT_SUCCESS;
}

ImportResult vm_import_module(ObjectString* module_name) {
	/*
	Note: this function attempts to do two things: load a module to the global module cache (unless it's there already),
	and also store it in the locals table.
	As we can see, it's also a public function as part of the extension API. So when it's called from a C function,
	the former side effect is meaningless, but (I *think*) harmless - the module gets placed in the native StackFrame locals
	table and that has no effect. 
	*/

	char* user_module_file_suffix = ".pln";
	char* extension_module_file_suffix = ".dll";

	char* main_module_dir = NULL;
	char* user_module_file_name = NULL;
	char* user_module_path = NULL;
	char* user_extension_module_path = NULL;
	char* stdlib_path = NULL;
	char* stdlib_module_path = NULL;
	char* user_extension_module_file_name = NULL;
	char* extension_module_file_name = NULL;
	char* stdlib_extension_path = NULL;

	ImportResult import_result = IMPORT_RESULT_SUCCESS;

	Value module_value;
	bool module_already_imported = cell_table_get_value(&vm.imported_modules, module_name, &module_value);
	if (module_already_imported) {
		assert(object_value_is(module_value, OBJECT_MODULE));
		
		cell_table_set_value(locals_or_module_table(), module_name, module_value);

		return IMPORT_RESULT_SUCCESS;
	}

	main_module_dir = directory_from_path(vm.main_module_path);
	user_module_file_name = concat_cstrings(
		module_name->chars, module_name->length, user_module_file_suffix, strlen(user_module_file_suffix), "user module file name");
	user_module_path = concat_null_terminated_paths(main_module_dir, user_module_file_name, "user module path");

	if (io_file_exists(user_module_path)) {
		import_result = load_text_module(module_name, user_module_path);
		goto import_module_cleanup;
	}

	user_extension_module_file_name = concat_cstrings(
		module_name->chars, module_name->length, 
		extension_module_file_suffix, strlen(extension_module_file_suffix), "user extension module file name");
	user_extension_module_path = concat_null_terminated_paths(
		main_module_dir, user_extension_module_file_name, "user extension module path");

	if (io_file_exists(user_extension_module_path)) {
		import_result = load_extension_module(module_name, user_extension_module_path);
		goto import_module_cleanup;
	}

	Value builtin_module_value;
	if (cell_table_get_value(&vm.builtin_modules, module_name, &builtin_module_value)) {
		assert(object_value_is(builtin_module_value, OBJECT_MODULE));
		ObjectModule* module = (ObjectModule*) builtin_module_value.as.object;

		cell_table_set_value(locals_or_module_table(), module_name, MAKE_VALUE_OBJECT(module));
		cell_table_set_value(&vm.imported_modules, module_name, MAKE_VALUE_OBJECT(module));

		goto import_module_cleanup;
	}

	stdlib_path = concat_null_terminated_paths(vm.interpreter_dir_path, VM_STDLIB_RELATIVE_PATH, "stdlib path");
	stdlib_module_path = concat_null_terminated_paths(stdlib_path, user_module_file_name, "stdlib module path");

	if (io_file_exists(stdlib_module_path)) {
		import_result = load_text_module(module_name, stdlib_module_path);
		goto import_module_cleanup;
	}

	extension_module_file_name = concat_cstrings(
		module_name->chars, module_name->length, extension_module_file_suffix, strlen(extension_module_file_suffix),
		"extension module file name");
	stdlib_extension_path = concat_null_terminated_paths(stdlib_path, extension_module_file_name, "stdlib extension path");

	if (io_file_exists(stdlib_extension_path)) {
		import_result = load_extension_module(module_name, stdlib_extension_path);
		goto import_module_cleanup;
	}

	import_result = IMPORT_RESULT_MODULE_NOT_FOUND;

	import_module_cleanup:
	if (main_module_dir != NULL) {
		deallocate(main_module_dir, strlen(main_module_dir) + 1, "directory path");
	}
	if (user_module_file_name != NULL) {
		deallocate(user_module_file_name, strlen(user_module_file_name) + 1, "user module file name");
	}
	if (user_module_path != NULL) {
		deallocate(user_module_path, strlen(user_module_path) + 1, "user module path");
	}
	if (user_extension_module_path != NULL) {
		deallocate(user_extension_module_path, strlen(user_extension_module_path) + 1, "user extension module path");
	}
	if (stdlib_path != NULL) {
		deallocate(stdlib_path, strlen(stdlib_path) + 1, "stdlib path");
	}
	if (stdlib_module_path != NULL) {
		deallocate(stdlib_module_path, strlen(stdlib_module_path) + 1, "stdlib module path");
	}
	if (user_extension_module_file_name != NULL) {
		deallocate(user_extension_module_file_name, strlen(user_extension_module_file_name) + 1, "user extension module file name");
	}
	if (extension_module_file_name != NULL) {
		deallocate(extension_module_file_name, strlen(extension_module_file_name) + 1, "extension module file name");
	}
	if (stdlib_extension_path != NULL) {
		deallocate(stdlib_extension_path, strlen(stdlib_extension_path) + 1, "stdlib extension path");
	}

	return import_result;
}

ImportResult vm_import_module_cstring(char* name) {
	return vm_import_module(object_string_copy_from_null_terminated(name));
}

ObjectModule* vm_get_module(ObjectString* name) {
	Value module_value;
	if (cell_table_get_value(&vm.imported_modules, name, &module_value)) {
		assert(object_value_is(module_value, OBJECT_MODULE));

		return (ObjectModule*) module_value.as.object;
	}

	return NULL;
}

ObjectModule* vm_get_module_cstring(char* name) {
	return vm_get_module(object_string_copy_from_null_terminated(name));
}

static CellTable find_free_vars_for_new_function(ObjectCode* func_code) {
	IntegerArray names_refd_in_created_func = func_code->bytecode.referenced_names_indices;
	int names_refd_count = names_refd_in_created_func.count;

	CellTable free_vars = cell_table_new_empty();

	for (int i = 0; i < names_refd_count; i++) {
		/* Get the name of the variable referenced inside the created function */
		size_t refd_name_index = names_refd_in_created_func.values[i];
		Value refd_name_value = func_code->bytecode.constants.values[refd_name_index];
		assert(object_value_is(refd_name_value, OBJECT_STRING));
		ObjectString* refd_name = (ObjectString*) refd_name_value.as.object;

		/* If the cell for the refd_name exists in our current local scope, we take that cell
		* and put it in the free_vars of the created function.
		* If it doesn't, we check to see if this name is assigned anywhere in the current running function,
		* even though it hasn't been assigned yet, because if we later assign it in this current function, we would like
		* the created closure to be able to see the new value.
		* If it is assigned anywhere in the current function, we create a new empty cell, and we put it in both our
		* own local scope and the free_vars of the newly created function.
		* If it isn't assigned in our currently running function, the last resort is to look it up in our own free_vars.
		* This should create the chain of nested closures.
		* We look in our free_vars as the "last resort", because we want more inner references to shadow more outer references
		* for the created closure.
		* If the cell does exist in the free_vars, we put it in the new free_vars.
		* If it doesn't, than the name mentioned in the closure is either a reference to a global or an error. We'll find out
		* when the closure runs. */

		ObjectCell* cell = NULL;

		if (cell_table_get_cell_cstring_key(locals_or_module_table(), refd_name->chars, &cell)) {
			cell_table_set_cell_cstring_key(&free_vars, refd_name->chars, cell);
		} else {
			Bytecode* current_bytecode = &current_frame()->function->code->bytecode;
			IntegerArray* assigned_names_indices = &current_bytecode->assigned_names_indices;

			for (int i = 0; i < assigned_names_indices->count; i++) {
				size_t assigned_name_index = assigned_names_indices->values[i];
				Value assigned_name_constant = current_bytecode->constants.values[assigned_name_index];
				assert(object_value_is(assigned_name_constant, OBJECT_STRING));
				ObjectString* assigned_name = (ObjectString*) assigned_name_constant.as.object;

				if (object_strings_equal(refd_name, assigned_name)) {
					cell = object_cell_new_empty();
					cell_table_set_cell_cstring_key(&free_vars, refd_name->chars, cell);
					cell_table_set_cell_cstring_key(locals_or_module_table(), refd_name->chars, cell);
					break;
				}
			}

			CellTable* current_func_free_vars = &current_frame()->function->free_vars;
			if (cell == NULL && cell_table_get_cell_cstring_key(current_func_free_vars, refd_name->chars, &cell)) {
				cell_table_set_cell_cstring_key(&free_vars, refd_name->chars, cell);
			}
		}
	}

	return free_vars;
}

#define READ_BYTE() (*vm.ip++)

static Value read_constant(void) {
	uint8_t index_byte1 = READ_BYTE();
	uint8_t index_byte2 = READ_BYTE();
	uint16_t index = two_bytes_to_short(index_byte1, index_byte2);
	return current_bytecode()->constants.values[index];
}

#define READ_CONSTANT() (read_constant())

static bool vm_interpret_frame(StackFrame* frame) {
	#define BINARY_MATH_OP(op) do { \
        Value b = pop(); \
        Value a = pop(); \
        push(MAKE_VALUE_NUMBER((a.as.number) op (b.as.number))); \
    } while(false)

	#define RUNTIME_ERROR(...) do { \
		if (!vm.currently_handling_error) { \
			print_stack_trace(); \
			fprintf(stdout, __VA_ARGS__); \
			fprintf(stdout, "\n"); \
			vm.currently_handling_error = true; \
		} \
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

	bool is_executing = true;
	bool runtime_error_occured = false;

	if (!push_frame(*frame)) {
		RUNTIME_ERROR("Stack overflow");
		stack_frame_free(frame);
		return false;
	}

	vm.ip = current_frame()->function->code->bytecode.code;;

	DEBUG_TRACE("Starting interpreter loop.");

    while (is_executing) {
		DEBUG_TRACE("--------------------------");
    	DEBUG_TRACE("num_objects: %d, max_objects: %d", vm.num_objects, vm.max_objects);

    	OP_CODE opcode = READ_BYTE();
        
		#if DEBUG_TRACE_EXECUTION
			disassembler_do_single_instruction(opcode, current_bytecode(), vm.ip - 1 - current_bytecode()->code);

			Value* eval_stack = vm.stack;
			Value* stack_top = vm.stack_top;

			printf("\n");
			bool stackEmpty = eval_stack == stack_top;
			if (stackEmpty) {
				printf("[ -- Empty Stack -- ]");
			} else {
				for (Value* value = eval_stack; value < stack_top; value++) {
					printf("[ ");
					value_print(*value);
					printf(" ]");
				}
			}
			printf("\n\nLocal variables:\n");
			table_print(&locals_or_module_table()->table); // TODO: No encapsulation, fix this
			printf("\n");

			bytecode_print_constant_table(current_bytecode());
			printf("\n");

			printf("Call stack:\n");
			print_call_stack();
			printf("\n");

			#if DEBUG_MEMORY_EXECUTION
				printAllObjects();
			#endif
		#endif

		// Possible that vm.num_objects > vm.max_objects if many objects were created during the compiling stage, where GC is disallowed
		if (vm.num_objects >= vm.max_objects) {
			vm_gc();
		}

		#if GC_STRESS_TEST
		vm_gc();
		#endif

        switch (opcode) {
            case OP_CONSTANT: {
                // int constantIndex = READ_BYTE();
                // Value constant = current_bytecode()->constants.values[constantIndex];
				Value constant = READ_CONSTANT();
                push(constant);
                break;
            }

            case OP_ADD: {
            	if (peek_at(2).type == VALUE_OBJECT) {
            		Value other = peek_at(1); /* This will be popped later */
            		Value subject_val = peek_at(2); /* Leave subject on stack for it to not be GC'd */

            		Object* subject = subject_val.as.object;
            		Value add_method;
            		if (!object_load_attribute_cstring_key(subject, "@add", &add_method)) {
            			RUNTIME_ERROR("Object doesn't support @add method.");
            			break;
            		}

					if (!object_value_is(add_method, OBJECT_BOUND_METHOD)) {
						RUNTIME_ERROR("Objects @add isn't a method.");
						break;
					}

					ObjectBoundMethod* add_bound_method = (ObjectBoundMethod*) add_method.as.object;
					Object* self = add_bound_method->self;

					assert(subject == self);

					ValueArray arguments = collect_values(add_bound_method->method->num_params);
					if (call_bound_method_leave_on_stack(add_bound_method, arguments) != CALL_RESULT_SUCCESS) {
						RUNTIME_ERROR("@add function failed.");
						goto op_add_cleanup;
					}

					op_add_cleanup:
					value_array_free(&arguments);
					Value result = pop();
					pop(); /* The subject */
					push(result);
            	} else if (peek_at(2).type == VALUE_NUMBER && peek_at(1).type == VALUE_NUMBER) {
            		BINARY_MATH_OP(+);
            	} else {
					RUNTIME_ERROR("Attempting to add types which do not support addition.");
				}
                break;
            }
            
            case OP_SUBTRACT: {
				if (peek_at(2).type != VALUE_NUMBER || peek_at(1).type != VALUE_NUMBER) {
					RUNTIME_ERROR("Attempting to subtract types which do not support subtraction.");
					break;
				}
                BINARY_MATH_OP(-);
                break;
            }
            
            case OP_MULTIPLY: {
				if (peek_at(2).type != VALUE_NUMBER || peek_at(1).type != VALUE_NUMBER) {
					RUNTIME_ERROR("Attempting to multiply types which do not support multiplication.");
					break;
				}
                BINARY_MATH_OP(*);
                break;
            }
            
            case OP_DIVIDE: {
				if (peek_at(2).type != VALUE_NUMBER || peek_at(1).type != VALUE_NUMBER) {
					RUNTIME_ERROR("Attempting to divide types which do not support division.");
					break;
				}
                BINARY_MATH_OP(/);
                break;
            }

			case OP_MODULO: {
				if (peek_at(2).type != VALUE_NUMBER || peek_at(1).type != VALUE_NUMBER) {
					RUNTIME_ERROR("Attempting to perform modulo on types which do not support it.");
					break;
				}

				Value b = pop();
				Value a = pop();

				if (a.as.number < 0 || b.as.number < 0) {
					RUNTIME_ERROR("Modulo with negative numbers not supported.");
					break;
				}

				push(MAKE_VALUE_NUMBER(fmod(a.as.number, b.as.number)));

                break;
            }

            case OP_LESS_THAN: {
        		Value b = pop();
        		Value a = pop();

        		int compare = 0;
        		if (!value_compare(a, b, &compare)) {
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
        		if (!value_compare(a, b, &compare)) {
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
        		if (!value_compare(a, b, &compare)) {
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
        		if (!value_compare(a, b, &compare)) {
        			RUNTIME_ERROR("Unable to compare two values >=. Types: %d, %d", a.type, b.type);
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
        		if (!value_compare(a, b, &compare)) {
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

            case OP_MAKE_STRING: {
            	Value constant = READ_CONSTANT();
				assert(object_value_is(constant, OBJECT_STRING));
            	push(constant);
            	break;
            }

			case OP_MAKE_CLASS: {
				ObjectCode* class_body_code = (ObjectCode*) READ_CONSTANT().as.object;
				CellTable base_func_free_vars = find_free_vars_for_new_function(class_body_code);

				ObjectFunction* class_base_function = object_user_function_new(class_body_code, NULL, 0, base_func_free_vars);
				set_function_name(&MAKE_VALUE_OBJECT(class_base_function), object_string_copy_from_null_terminated("<Class base function>"));

				Value superclass_value = peek();
				ObjectClass* superclass;
				if (superclass_value.type == VALUE_NIL) {
					superclass = NULL;
				} else if (object_value_is(superclass_value, OBJECT_CLASS)) {
					superclass = (ObjectClass*) superclass_value.as.object;
				} else {
					RUNTIME_ERROR("Cannot set non-class value as a superclass.");
					break;
				}

				ObjectClass* class = object_class_new(class_base_function, superclass, NULL);

				ValueArray args;
				value_array_init(&args);
				Value throwaway_result;
				if (!call_plane_function(class_base_function, NULL, args, (Object*) class, &throwaway_result)) {
					RUNTIME_ERROR("Error occured when running class initialization code.");
					goto op_make_class_cleanup;
				}

				pop(); /* Superclass value */
				push(MAKE_VALUE_OBJECT(class));

				op_make_class_cleanup:
				value_array_free(&args);

				break;
			}

            case OP_MAKE_FUNCTION: {
				ObjectCode* object_code = (ObjectCode*) READ_CONSTANT().as.object;

				uint8_t num_params_byte1 = READ_BYTE();
				uint8_t num_params_byte2 = READ_BYTE();
				uint16_t num_params = two_bytes_to_short(num_params_byte1, num_params_byte2);

				/* Build the params array for the created function */
				ObjectString** params = allocate(sizeof(ObjectString*) * num_params, "Parameters list strings");
				for (int i = 0; i < num_params; i++) {
					Value param_value = READ_CONSTANT();
					assert(object_value_is(param_value, OBJECT_STRING));
					ObjectString* param_object_string = (ObjectString*) param_value.as.object;
					params[i] = param_object_string;
				}

				CellTable new_function_free_vars = find_free_vars_for_new_function(object_code);
				ObjectFunction* function = object_user_function_new(object_code, params, num_params, new_function_free_vars);
				push(MAKE_VALUE_OBJECT(function));
				break;
			}

            case OP_MAKE_TABLE: {
            	Table table;
            	table_init(&table);

            	uint8_t num_entries = READ_BYTE();

            	for (int i = 0; i < num_entries; i++) {
					Value key = pop();
					Value value = pop();
					table_set(&table, key, value);
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
                StackFrame* frame = vm_peek_current_frame(); /* Staying on the stack because is popped and freed after interpreter loop */

				vm.ip = frame->return_address;
				is_executing = false;

                break;
            }
            
            case OP_POP: {
            	pop();
            	break;
            }

			case OP_DUP: {
				Value v = peek();
				push(v);
				break;
			}

			case OP_DUP_TWO: {
				Value lower = peek_at(2);
				Value upper = peek_at(1);
				push(lower);
				push(upper);
				break;
			}

			case OP_SWAP: {
				Value upper = pop();
				Value lower = pop();
				push(upper);
				push(lower);
				break;
			}

			case OP_SWAP_TOP_WITH_NEXT_TWO: {
				Value top = pop();
				Value second = pop();
				Value third = pop();
				push(top);
				push(third);
				push(second);
				break;
			}

			case OP_GET_OFFSET_FROM_TOP: {
				uint8_t byte1 = READ_BYTE();
            	uint8_t byte2 = READ_BYTE();
            	uint16_t offset = two_bytes_to_short(byte1, byte2);
				Value v = peek_at(offset);
				push(v);
				break;
			}

			case OP_SET_OFFSET_FROM_TOP: {
				uint8_t byte1 = READ_BYTE();
            	uint8_t byte2 = READ_BYTE();
            	uint16_t offset = two_bytes_to_short(byte1, byte2);
				*(vm.stack_top - offset) = pop();
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
                Value name_value = READ_CONSTANT();
                ASSERT_VALUE_TYPE(name_value, VALUE_OBJECT);
                ObjectString* name_string = object_as_string(name_value.as.object);
				Value value;
				if (load_variable(name_string, &value)) {
					push(value);
				} else {
					RUNTIME_ERROR("Variable %.*s not found.", name_string->length, name_string->chars);
				}

                break;
            }
            
            case OP_SET_VARIABLE: {
				Value name_val = READ_CONSTANT();
                ASSERT_VALUE_TYPE(name_val, VALUE_OBJECT);
                ObjectString* name = OBJECT_AS_STRING(name_val.as.object);

                Value value = pop();

                if (object_value_is(value, OBJECT_FUNCTION)) {
                	set_function_name(&value, name);
                }
				else if (object_value_is(value, OBJECT_CLASS)) {
					set_class_name(&value, name);
				}

                cell_table_set_value(locals_or_module_table(), name, value);
                break;
            }

			case OP_DECLARE_EXTERNAL: {
				Value name_val = READ_CONSTANT();
				assert(object_value_is(name_val, OBJECT_STRING));
				ObjectString* name = (ObjectString*) name_val.as.object;

				CellTable* free_vars = &current_frame()->function->free_vars;
				ObjectCell* cell = NULL;
				if (!cell_table_get_cell(free_vars, name, &cell)) {
					FAIL("In OP_DECLARE_GLOBAL, couldn't find the matching cell in the free variables. Shouldn't happen.");
				}
				cell_table_set_cell(locals_or_module_table(), name, cell);

				break;
			}
            
			case OP_CALL: {
				int arg_count = READ_BYTE();
				Value callee_value = pop();

				if (callee_value.type != VALUE_OBJECT) {
					RUNTIME_ERROR("Cannot call non callable.");
					break;
				}

				Object* callee = callee_value.as.object;

				ValueArray args = collect_values(arg_count);

				push(callee_value); /* We push the callee so the GC doesn't collect it in any chance */

				CallResult call_result = call_object_leave_on_stack(callee, args);
				value_array_free(&args);

				if (call_result == CALL_RESULT_SUCCESS) {
					Value returnvalue = pop();
					pop(); /* The callee */
					push(returnvalue);
					break;
				}

				pop(); /* The callee */

				char* callable_name = object_get_callable_name(callee);
				
				switch (call_result) {
					case CALL_RESULT_SUCCESS: {
						/* Just to silence the compiler warning */
						break;
					}
					case CALL_RESULT_INVALID_ARGUMENT_COUNT: {
						RUNTIME_ERROR("Function %s called with illegal number of arguments.", callable_name);
						break;
					}
					case CALL_RESULT_NATIVE_EXECUTION_FAILED: {
						RUNTIME_ERROR("Native function %s failed.", callable_name);
						break;
					}
					case CALL_RESULT_PLANE_CODE_EXECUTION_FAILED: {
						RUNTIME_ERROR("Function %s failed.", callable_name);
						break;
					}
					case CALL_RESULT_CLASS_INIT_NOT_METHOD: {
						RUNTIME_ERROR("Class @init attribute isn't a method.");
						break;
					}
					case CALL_RESULT_INVALID_CALLABLE: {
						RUNTIME_ERROR("Cannot invoke non-callable.");
						break;
					}
					case CALL_RESULT_NO_SUCH_ATTRIBUTE: {
						FAIL("CALL_RESULT_NO_SUCH_ATTRIBUTE should never happen from OP_CALL.");
						break;
					}
				}

				break;
			}

            case OP_GET_ATTRIBUTE: {
				Value name_val = READ_CONSTANT();
                ASSERT_VALUE_TYPE(name_val, VALUE_OBJECT);
                ObjectString* name = OBJECT_AS_STRING(name_val.as.object);

                Value obj_val = pop();
                if (obj_val.type != VALUE_OBJECT) {
                	RUNTIME_ERROR("Cannot access attribute on non-object.");
                	break;
                }

				Value attr_value;
                if (object_load_attribute(obj_val.as.object, name, &attr_value)) {
					push(attr_value);
					break;
				}

				RUNTIME_ERROR("Cannot find attribute %.*s of object.", name->length, name->chars);
                break;
            }

            case OP_SET_ATTRIBUTE: {
                // Value name_val = current_bytecode()->constants.values[READ_BYTE()];
				Value name_val = READ_CONSTANT();
                ASSERT_VALUE_TYPE(name_val, VALUE_OBJECT);
                ObjectString* name = OBJECT_AS_STRING(name_val.as.object);

                Value obj_value = pop();
                if (obj_value.type != VALUE_OBJECT) {
                	RUNTIME_ERROR("Cannot set attribute on non-object.");
                	break;
                }

                Object* object = obj_value.as.object;
                Value attribute_value = pop();

				if (object->type == OBJECT_STRING) {
					/* We have to treat strings specially because of string caching */
					RUNTIME_ERROR("Cannot set attribute on strings.");
					break;
				}

                object_set_attribute_cstring_key(object, name->chars, attribute_value);

                break;
            }

            case OP_ACCESS_KEY: {
            	Value subject_value = pop();
            	if (subject_value.type != VALUE_OBJECT) {
					RUNTIME_ERROR("Accessing key on none object. Actual value type: %d", subject_value.type);
					break;
            	}

            	Object* subject = subject_value.as.object;
				Value key = pop();
				push(subject_value);
				push(key);

				Value key_access_method_value;
				if (!object_load_attribute_cstring_key(subject, "@get_key", &key_access_method_value)) {
					RUNTIME_ERROR("Object doesn't support @get_key method.");
					break;
				}

				if (!object_value_is(key_access_method_value, OBJECT_BOUND_METHOD)) {
					RUNTIME_ERROR("Object's @get_key isn't a method.");
					break;
				}

				ObjectBoundMethod* bound_method = (ObjectBoundMethod*) key_access_method_value.as.object;
				Object* self = bound_method->self;

				assert(subject == self);
				assert(bound_method->method->num_params == 1);

				ValueArray arguments = collect_values(bound_method->method->num_params);
				if (call_bound_method_leave_on_stack(bound_method, arguments) != CALL_RESULT_SUCCESS) {
					RUNTIME_ERROR("@get_key function failed.");
					goto op_access_key_cleanup;
				}

				Value result = pop();
				pop(); /* The subject */
				push(result);

				op_access_key_cleanup:
				value_array_free(&arguments);				

            	break;
            }

            case OP_SET_KEY: {
            	Value subject_as_value = peek_at(1);
            	Value key = peek_at(2);
            	Value value = peek_at(3);

            	if (subject_as_value.type != VALUE_OBJECT) {
            		RUNTIME_ERROR("Cannot set key on non-object.");
            		break;
            	}

            	Object* subject = subject_as_value.as.object;

            	ObjectBoundMethod* set_method = NULL;
            	MethodAccessResult access_result = -1;
            	if ((access_result = object_get_method(subject, "@set_key", &set_method)) == METHOD_ACCESS_SUCCESS) {
					Object* self = set_method->self;

					assert(subject == self);

    				ValueArray arguments;
    				value_array_init(&arguments);
    				value_array_write(&arguments, &key);
    				value_array_write(&arguments, &value);

    				Value throwaway_result;
					if (call_bound_method_leave_on_stack(set_method, arguments) != CALL_RESULT_SUCCESS) {
						RUNTIME_ERROR("@set_key function failed.");
						goto op_access_key_cleanup;
					}

					pop(); /* The result, doesn't matter to us */
					pop(); /* The subject */
					pop(); /* The key */
					pop(); /* The value */

    				op_set_key_cleanup:
    				value_array_free(&arguments);
            	} else if (access_result == METHOD_ACCESS_NO_SUCH_ATTR) {
            		RUNTIME_ERROR("Object doesn't support @set_key method.");
            		break;
            	} else if (access_result == METHOD_ACCESS_ATTR_NOT_BOUND_METHOD) {
            		RUNTIME_ERROR("Object's @set_key isn't a bound method.");
            		break;
            	} else {
            		FAIL("Illegal value for access_result: %d", access_result);
            	}

            	break;
            }

            case OP_JUMP_IF_FALSE: {
            	uint8_t addr_byte1 = READ_BYTE();
            	uint8_t addr_byte2 = READ_BYTE();
            	uint16_t delta = two_bytes_to_short(addr_byte1, addr_byte2);
				Value condition = pop();

				ERROR_IF_NON_BOOLEAN(condition, "Expected boolean as condition");

				if (!condition.as.boolean) {
					vm.ip += delta;
				}

				break;
			}

			case OP_JUMP_IF_TRUE: {
            	uint8_t addr_byte1 = READ_BYTE();
            	uint8_t addr_byte2 = READ_BYTE();
            	uint16_t delta = two_bytes_to_short(addr_byte1, addr_byte2);
				Value condition = pop();

				ERROR_IF_NON_BOOLEAN(condition, "Expected boolean as condition");

				if (condition.as.boolean) {
					vm.ip += delta;
				}

				break;
			}

            case OP_JUMP_FORWARD: {
            	uint8_t addr_byte1 = READ_BYTE();
            	uint8_t addr_byte2 = READ_BYTE();
            	uint16_t delta = two_bytes_to_short(addr_byte1, addr_byte2);

            	vm.ip += delta;

            	break;
            }

			case OP_JUMP_BACKWARD: {
            	uint8_t addr_byte1 = READ_BYTE();
            	uint8_t addr_byte2 = READ_BYTE();
            	uint16_t delta = two_bytes_to_short(addr_byte1, addr_byte2);

            	vm.ip -= delta;

            	break;
            }

			case OP_IMPORT: {
				ObjectString* module_name = (ObjectString*) READ_CONSTANT().as.object;
				ImportResult import_result = vm_import_module(module_name);

				switch (import_result) {
					case IMPORT_RESULT_SUCCESS: {
						break;
					}
					case IMPORT_RESULT_OPEN_FAILED: {
						RUNTIME_ERROR("Couldn't open module %.*s.", module_name->length, module_name->chars);
						break;
					}
					case IMPORT_RESULT_READ_FAILED: {
						RUNTIME_ERROR("Couldn't read module %.*s.", module_name->length, module_name->chars);
						break;
					}
					case IMPORT_RESULT_CLOSE_FAILED: {
						RUNTIME_ERROR("Couldn't close module %.*s.", module_name->length, module_name->chars);
						break;
					}
					case IMPORT_RESULT_EXTENSION_NO_INIT_FUNCTION: {
						RUNTIME_ERROR("Extension module %.*s doesn't export an init function.", module_name->length, module_name->chars);
						break;
					}
					case IMPORT_RESULT_MODULE_NOT_FOUND: {
						RUNTIME_ERROR("Couldn't find module %.*s.", module_name->length, module_name->chars);
						break;
					}
				}
				
				break;
			}

            default: {
            	FAIL("Unknown opcode: %d. At ip: %p", opcode, vm.ip - 1);
            }
        }

		#if DEBUG_PAUSE_AFTER_OPCODES
			printf("\nPress ENTER to continue.\n");
			getchar();
		#endif
    }

	StackFrame finished_frame = pop_frame();
	stack_frame_free(&finished_frame);

    DEBUG_TRACE("\n--------------------------\n");
	DEBUG_TRACE("Ended interpreter loop.");

    #undef BINARY_MATH_OP

    return !runtime_error_occured;
}

static bool call_plane_function_leave_on_stack(
		ObjectFunction* function, Object* self, ValueArray args, Object* base_entity) {
	bool is_entity_base = base_entity != NULL;
	StackFrame frame = new_stack_frame(vm.ip, function, base_entity, is_entity_base, false, false);

	assert(args.count == function->num_params);

	for (int i = 0; i < function->num_params; i++) {
		ObjectString* param_name = function->parameters[i];
		Value argument = args.values[i];
		cell_table_set_value(&frame.local_variables, param_name, argument);
	}

	if (self != NULL) {
		cell_table_set_value_cstring_key(&frame.local_variables, "self", MAKE_VALUE_OBJECT(self));
	}

	if (vm_interpret_frame(&frame)) {
		return true;
	}
	return false;
}

static bool call_plane_function(
		ObjectFunction* function, Object* self, ValueArray args, Object* base_entity, Value* out) {
	if (call_plane_function_leave_on_stack(function, self, args, base_entity)) {
		*out = pop();
		return true;
	}
	return false;
}

bool vm_interpret_program(Bytecode* bytecode, char* main_module_path) {
	vm.main_module_path = main_module_path;

	ObjectCode* code = object_code_new(*bytecode);
	ObjectFunction* base_function = object_user_function_new(code, NULL, 0, cell_table_new_empty());

	/* We want to run the GC before starting the program in order to clear objects created
	   during compilation and get a fresh start.
	   Before we do that, we have to push() the base_function, and pop it afterwards.
	   The reason for that is because our bytecode struct already points to objects
	   created during compilation, such as string and code object constants.
	   If we run the GC without first pushing the Bytecode on the stack (wrapped by an ObjectCode,
	   wrapped by an ObjectFunction), the constants will get collected. */
	push(MAKE_VALUE_OBJECT((Object*) base_function));

	/* Cleanup unused objects created during compilation */
	DEBUG_OBJECTS_PRINT("Setting vm.allow_gc = true.");
	vm.allow_gc = true;
	vm_gc();

	/* Pop the base_function that was pushed before */
	pop();

	ObjectString* base_module_name = object_string_copy_from_null_terminated("<main>");
	ObjectModule* module = object_module_new(base_module_name, base_function);
	StackFrame base_frame = new_stack_frame(NULL, base_function, (Object*) module, true, false, false);

	DEBUG_TRACE("Starting interpreter loop.");
	return vm_interpret_frame(&base_frame);
}

#undef READ_BYTE
#undef READ_CONSTANT
