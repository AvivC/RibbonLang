#include <string.h>
#include <windows.h>

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

static ObjectThread* current_thread(void) {
	return vm.current_thread;
}

static void set_current_thread(ObjectThread* thread) {
	vm.current_thread = thread;
}

static StackFrame* current_frame(void) {
	return current_thread()->call_stack_top - 1;
}

static Bytecode* current_bytecode(void) {
	return &current_frame()->function->code->bytecode;
}

static void push(Value value) {
	object_thread_push_eval_stack(current_thread(), value);
}

static Value pop(void) {
	return object_thread_pop_eval_stack(current_thread());
}

static Value peek_at(int offset) {
	return *(current_thread()->eval_stack_top - offset);
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

static StackFrame make_base_stack_frame(Bytecode* base_chunk) {
	ObjectCode* code = object_code_new(*base_chunk);
	ObjectFunction* base_function = object_user_function_new(code, NULL, 0, cell_table_new_empty());
	ObjectString* base_module_name = object_string_copy_from_null_terminated("<main>");
	ObjectModule* module = object_module_new(base_module_name, base_function);
	return new_stack_frame(NULL, base_function, (Object*) module, true, false, false);
}

InterpretResult vm_call_function_directly(ObjectFunction* function, ValueArray args, Value* out) {
	ObjectThread* thread = current_thread();
	StackFrame frame = new_stack_frame(thread->ip, function, NULL, false, false, false);

	if (args.count != function->num_params) {
		FAIL("User function called with unmatching params number."); /* TODO: Legit error? */
	}

	for (int i = 0; i < function->num_params; i++) {
		const char* param_name = function->parameters[i];
		Value argument = args.values[i];
		cell_table_set_value_cstring_key(&frame.local_variables, param_name, argument);
	}

	InterpretResult func_exec_result = vm_interpret_frame(&frame);
	
	if (func_exec_result == INTERPRET_SUCCESS) {
		Value return_value = pop();
		*out = return_value;
	}
	
	return func_exec_result;
}

/* Add a thread to the vm list of threads */
static void add_thread(ObjectThread* thread) {
	thread->previous_thread = NULL;
	thread->next_thread = vm.threads;
	if (vm.threads != NULL) {
		vm.threads->previous_thread = thread;
	}
	vm.threads = thread;
}

static void switch_to_new_thread(ObjectThread* thread) {
	// Maybe not the best scheduler-wise?
	add_thread(thread);
	set_current_thread(thread);
}

void vm_spawn_thread(ObjectFunction* function) {
	int name_length = snprintf(NULL, 0, "%" PRI_SIZET, vm.thread_creation_counter);
	char* temp_name_buffer = allocate(name_length + 1, "Temp thread name buffer");
	snprintf(temp_name_buffer, name_length + 1, "%" PRI_SIZET, vm.thread_creation_counter++);

	ObjectThread* thread = object_thread_new(function, temp_name_buffer);
	deallocate(temp_name_buffer, name_length + 1, "Temp thread name buffer");

	*thread->call_stack_top = new_stack_frame(NULL, function, NULL, false, false, false);
	thread->call_stack_top++;

	add_thread(thread);
}

static void push_frame(StackFrame frame) {
	/* TODO: Why do we take a StackFrame and not StackFrame*? */
	object_thread_push_frame(current_thread(), frame);
}

static void stack_frame_free(StackFrame* frame) {
	cell_table_free(&frame->local_variables);
	init_stack_frame(frame);
}

static StackFrame pop_frame(void) {
	return object_thread_pop_frame(current_thread());
}

static StackFrame* peek_current_frame(void) {
	return object_thread_peek_frame(current_thread(), 1);
}

static StackFrame* peek_previous_frame(void) {
	return object_thread_peek_frame(current_thread(), 2);
}

static CellTable* frame_locals_or_module_table(StackFrame* frame) {
	return frame->is_entity_base ? &frame->base_entity->attributes : &frame->local_variables;
}

/* The "correct" locals table depends on whether we are the base function of a module or class or not...
 * This design isn't great, should change it later. */
static CellTable* locals_or_module_table(void) {
	return frame_locals_or_module_table(current_frame());
}

static Value load_variable(ObjectString* name) {
	Value value;

	CellTable* locals = &current_frame()->local_variables;
	CellTable* free_vars = &current_frame()->function->free_vars;
	CellTable* globals = &vm.globals;

	bool variable_found =
			cell_table_get_value_cstring_key(locals, name->chars, &value)
			|| cell_table_get_value_cstring_key(free_vars, name->chars, &value)
			|| (current_frame()->is_entity_base && object_load_attribute_cstring_key(current_frame()->base_entity, name->chars, &value))
			|| cell_table_get_value_cstring_key(globals, name->chars, &value);

	if (variable_found) {
		return value;
	}

	// TODO: Nil when variable not found..? Should be an error
	return MAKE_VALUE_NIL();
}

static void gc_mark_object(Object* object);

static void gc_mark_table(Table* table) {
	PointerArray entries = table_iterate(table, "gc mark table_iterate buffer");

	for (int i = 0; i < entries.count; i++) {
		Node* entry = entries.values[i];

		if (entry->value.type == VALUE_OBJECT) {
			gc_mark_object(entry->value.as.object);
		}
		if (entry->key.type == VALUE_OBJECT) {
			gc_mark_object(entry->key.as.object);
		}
	}

	pointer_array_free(&entries);
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
	bool is_probably_valid_object = (object->is_reachable == true) || (object->is_reachable == false);
	if (!is_probably_valid_object) {
		FAIL("Illegal object passed to gc_mark_object.");
	}
}

static void gc_mark_function_free_vars(ObjectFunction* function) {
	gc_mark_table(&function->free_vars.table);
}

static void gc_mark_object_function(Object* object) {
	ObjectFunction* function = OBJECT_AS_FUNCTION(object);
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

static void gc_mark_object_thread(Object* object) {
	ObjectThread* thread = (ObjectThread*) object;

	gc_mark_object((Object*) thread->base_function);

	/* Mark everything on the evaluation stack */
	for (Value* value = thread->eval_stack; value != thread->eval_stack_top; value++) {
		if (value->type == VALUE_OBJECT) {
			gc_mark_object(value->as.object);
		}
	}

	/* Mark everything on the call stack */
	for (StackFrame* frame = thread->call_stack; frame != thread->call_stack_top; frame++) {
		gc_mark_object((Object*) frame->function);
		// if (frame->module != NULL) {
		// 	gc_mark_object((Object*) frame->module);
		// }
		if (frame->base_entity != NULL) {
			gc_mark_object(frame->base_entity);
		}

		/* Mark the frame locals */
		gc_mark_table(&frame_locals_or_module_table(frame)->table);
	}
}

static void gc_mark_object_instance(Object* object) {
	ObjectInstance* instance = (ObjectInstance*) object;
	ObjectClass* klass = instance->klass;

	gc_mark_object((Object*) klass);

	if (klass->instance_size > 0) {
		/* Native class */

		if (klass->gc_mark_func != NULL) {
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
		case OBJECT_THREAD: {
			gc_mark_object_thread(object);
			return;
		}
		case OBJECT_STRING: {
			/* Nothing. Currently strings don't link to any other object except for their attributes, which were already marked. */
			return;
		}
		case OBJECT_CLASS: {
			/* Nothing - currently classes don't link to any other object except for their attributes. */
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
	ObjectThread* thread = vm.threads;
	while (thread != NULL) {
		gc_mark_object((Object*) thread);
		thread = thread->next_thread;
	}

	gc_mark_table(&vm.globals.table);
	gc_mark_table(&vm.imported_modules.table);
	gc_mark_table(&vm.builtin_modules.table);
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

static void register_builtin_function(char* name, int num_params, char** params, NativeFunction function) {
	ObjectFunction* obj_function = make_native_function_with_params(name, num_params, params, function);
	cell_table_set_value_cstring_key(&vm.globals, name, MAKE_VALUE_OBJECT(obj_function));
}

static void set_builtin_globals(void) {
	register_builtin_function("print", 1, (char*[]) {"text"}, builtin_print);
	register_builtin_function("input", 0, NULL, builtin_input);
	register_builtin_function("read_file", 1, (char*[]) {"path"}, builtin_read_file);
	register_builtin_function("spawn", 1, (char*[]) {"function"}, builtin_spawn);
}

static void register_builtin_modules(void) {
	const char* test_module_name = "_testing";
	ObjectModule* test_module = object_module_native_new(object_string_copy_from_null_terminated(test_module_name), NULL);

	ObjectFunction* demo_print_func = make_native_function_with_params("demo_print", 1, (char*[]) {"function"}, builtin_test_demo_print);
	object_set_attribute_cstring_key((Object*) test_module, "demo_print", MAKE_VALUE_OBJECT(demo_print_func));

	ObjectFunction* call_callback_with_args_func = make_native_function_with_params(
							"call_callback_with_args", 3, (char*[]) {"callback", "arg1", "arg2"}, builtin_test_call_callback_with_args);
	object_set_attribute_cstring_key((Object*) test_module, "call_callback_with_args", MAKE_VALUE_OBJECT(call_callback_with_args_func));

	cell_table_set_value_cstring_key(&vm.builtin_modules, test_module_name, MAKE_VALUE_OBJECT(test_module));
}

static void call_user_function_custom_frame(ObjectFunction* function, Object* self, Object* base_entity, bool discard_return_value) {
	ObjectThread* thread = current_thread();

	bool is_entity_base = base_entity != NULL;
	StackFrame frame = new_stack_frame(thread->ip, function, base_entity, is_entity_base, false, discard_return_value);

	for (int i = 0; i < function->num_params; i++) {
		const char* param_name = function->parameters[i];
		Value argument = pop();
		cell_table_set_value_cstring_key(&frame.local_variables, param_name, argument);
	}

	if (self != NULL) {
		cell_table_set_value_cstring_key(&frame.local_variables, "self", MAKE_VALUE_OBJECT(self));
	}

	push_frame(frame);
	thread->ip = function->code->bytecode.code;
}

static void call_user_function(ObjectFunction* function, Object* self) {
	call_user_function_custom_frame(function, self, NULL, false);
}

static bool call_native_function(ObjectFunction* function, Object* self) {
	ValueArray arguments;
	value_array_init(&arguments);
	for (int i = 0; i < function->num_params; i++) {
		Value value = pop();
		value_array_write(&arguments, &value);
	}

	/* Push a native frame to keep the call stack in order.
	Specifically, for the use case where a native function calls a user function.
	Without this "native frame", OP_RETURN in the user function will make control jump
	back to the point where the caller _native_ function was called. */
	StackFrame native_frame = new_stack_frame(NULL, function, NULL, false, true, false);
	push_frame(native_frame);

	/* If the native function will end up calling a user function, the instruction
	pointer will be moving forward. When we go back to this calling site, we have to
	also remember to restore the instruction pointer. */
	uint8_t* ip_before_call = current_thread()->ip;

	Value result;
	bool func_success = function->native_function(self, arguments, &result);
	if (func_success) {
		push(result);
	} else {
		/* TODO: Probably a bug! Some functions still push NIL even if returning false. Need to sort this
		in one way or the other. */
		push(MAKE_VALUE_NIL());
	}

	pop_frame(); /* Pop the native frame */
	current_thread()->ip = ip_before_call; /* restore ip position */

	value_array_free(&arguments);
	return func_success;
}

void vm_init(void) {
	vm.threads = NULL;
	vm.current_thread = NULL;
	vm.thread_creation_counter = 0;
	vm.thread_opcode_counter = 0;

    vm.num_objects = 0;
    vm.max_objects = INITIAL_GC_THRESHOLD;
    vm.allow_gc = false;
    vm.imported_modules = cell_table_new_empty();
    vm.builtin_modules = cell_table_new_empty();
    cell_table_init(&vm.globals);
    set_builtin_globals();
	register_builtin_modules();

	vm.main_module_path = NULL;
	vm.interpreter_dir_path = find_interpreter_directory();
}

void vm_free(void) {
	cell_table_free(&vm.globals);
	cell_table_free(&vm.imported_modules);
	cell_table_free(&vm.builtin_modules);
	vm.threads = NULL;
	vm.current_thread = NULL;

	gc(); // TODO: probably move upper

    vm.num_objects = 0;
    vm.max_objects = INITIAL_GC_THRESHOLD;
    vm.allow_gc = false;

	deallocate(vm.main_module_path, strlen(vm.main_module_path) + 1, "main module absolute path");
	deallocate(vm.interpreter_dir_path, strlen(vm.interpreter_dir_path) + 1, "interpreter directory path");
}

static void print_call_stack(void) {
	ObjectThread* thread = current_thread();
	for (StackFrame* frame = thread->call_stack_top - 1; frame >= thread->call_stack; frame--) {
		printf("    -> %s\n", frame->function->name);
	}
}

static void print_stack_trace(void) {
	printf("An error has occured. Stack trace (most recent call on top):\n");
	print_call_stack();
}

#define READ_BYTE() (*current_thread()->ip++)
#define READ_CONSTANT() (current_bytecode()->constants.values[READ_BYTE()])

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

static void set_function_name(const Value* function_value, ObjectString* name) {
	ObjectFunction* function = (ObjectFunction*) function_value->as.object;
	char* new_cstring_name = copy_null_terminated_cstring(name->chars, "Function name");
	object_function_set_name(function, new_cstring_name);
}

static void set_class_name(const Value* class_value, ObjectString* name) {
	ObjectClass* klass = (ObjectClass*) class_value->as.object;
	deallocate(klass->name, klass->name_length + 1, "Class name");
	char* new_cstring_name = copy_null_terminated_cstring(name->chars, "Class name");
	object_class_set_name(klass, new_cstring_name, strlen(new_cstring_name));
}

static void print_all_threads(void) {
	ObjectThread* thread = vm.threads;
	while (thread != NULL) {
		object_thread_print_diagnostic(thread);		
		printf("\n");

		thread = thread->next_thread;
	}
}

static bool load_text_module(ObjectString* module_name, const char* file_name_buffer, char** error_out) {
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

			/* ObjectModule has to be on the stack at the end of the import */
			push(MAKE_VALUE_OBJECT(module));

			/* "Call" the new module to start executing it in the next iteration */
			call_user_function_custom_frame(module_base_function, NULL, (Object*) module, false);

			/* Cache the new module in the global module cache */
			cell_table_set_value_cstring_key(&vm.imported_modules, module_name->chars, MAKE_VALUE_OBJECT(module));

			return true;
		}
		case IO_CLOSE_FILE_FAILURE: {
			*error_out = "Failed to close file while loading module.";
			return false;
		}
		case IO_READ_FILE_FAILURE: {
			*error_out = "Failed to read file while loading module.";
			return false;
		}
		case IO_OPEN_FILE_FAILURE: {
			*error_out = "Failed to open file while loading module.";
			return false;
		}
	}

	FAIL("load_text_module - shouldn't reach here.");
	return false;
}

#define LOAD_EXTENSION_SUCCESS 0
#define LOAD_EXTENSION_OPEN_FAILURE 1
#define LOAD_EXTENSION_NO_INIT_FUNCTION 2

static int load_extension_module(ObjectString* module_name, char* path) {
	HMODULE handle = LoadLibraryExA(path, NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);

	if (handle == NULL) {
		return LOAD_EXTENSION_OPEN_FAILURE;
	}

	ExtensionInitFunction init_function = (ExtensionInitFunction) GetProcAddress(handle, "plane_module_init");

	if (init_function == NULL) {
		FreeLibrary(handle);
		return LOAD_EXTENSION_NO_INIT_FUNCTION;
	}

	ObjectModule* extension_module = object_module_native_new(module_name, handle);
	init_function(API, extension_module);

	push(MAKE_VALUE_OBJECT(extension_module));
	push(MAKE_VALUE_NIL()); /* Temporary patch, see related notes above */

	cell_table_set_value_cstring_key(&vm.imported_modules, module_name->chars, MAKE_VALUE_OBJECT(extension_module));

	return LOAD_EXTENSION_SUCCESS;
}

static CellTable find_free_vars_for_new_function(ObjectCode* func_code) {
	IntegerArray names_refd_in_created_func = func_code->bytecode.referenced_names_indices;
	int names_refd_count = names_refd_in_created_func.count;

	CellTable free_vars = cell_table_new_empty();

	for (int i = 0; i < names_refd_count; i++) {
		/* Get the name of the variable referenced inside the created function */
		size_t refd_name_index = names_refd_in_created_func.values[i];
		Value refd_name_value = func_code->bytecode.constants.values[refd_name_index];
		if (!object_value_is(refd_name_value, OBJECT_STRING)) {
			FAIL("Referenced name constant must be an ObjectString*");
		}
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
				ObjectString* assigned_name = NULL;
				ASSERT_VALUE_AS_OBJECT(assigned_name, assigned_name_constant, OBJECT_STRING, ObjectString, "Expected ObjectString* as assigned name.")

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

/* TODO: Leave public or make private? */
InterpretResult vm_interpret_frame(StackFrame* frame) {
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
		fprintf(stdout, "Runtime error: " __VA_ARGS__); \
		fprintf(stdout, "\n"); \
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

	#define THREAD_SWITCH_INTERVAL 16

	push_frame(*frame);

	current_thread()->ip = current_frame()->function->code->bytecode.code;

	bool is_executing = true;
	bool runtime_error_occured = false;

	DEBUG_TRACE("Starting interpreter loop.");

    while (is_executing) {
		DEBUG_TRACE("--------------------------");
    	DEBUG_TRACE("num_objects: %d, max_objects: %d", vm.num_objects, vm.max_objects);

    	OP_CODE opcode = READ_BYTE();
        
		#if DEBUG_TRACE_EXECUTION
			disassembler_do_single_instruction(opcode, current_bytecode(), current_thread()->ip - 1 - current_bytecode()->code);

			printf("\n");
			bool stackEmpty = current_thread()->eval_stack == current_thread()->eval_stack_top;
			if (stackEmpty) {
				printf("[ -- Empty Stack -- ]");
			} else {
				for (Value* value = current_thread()->eval_stack; value < current_thread()->eval_stack_top; value++) {
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

			// printf("Memory allocations:");
			// print_allocated_memory_entries();
			// printf("\n");

			#if DEBUG_MEMORY_EXECUTION
				printAllObjects();
			#endif
		#endif

		#if DEBUG_THREADING
			#if !DEBUG_TRACE_EXECUTION
				printf("--------------------------\n");	
				disassembler_do_single_instruction(opcode, current_bytecode(), current_thread()->ip - 1 - current_bytecode()->code);
			#endif

			DEBUG_THREADING_PRINT("Current thread:\n        ");
			object_thread_print(current_thread());
			printf("\n");

			printf("\n");
			printf("All running threads:\n");
			print_all_threads();
			printf("\n");
		#endif

		// Possible that vm.num_objects > vm.max_objects if many objects were created during the compiling stage, where GC is disallowed
		if (vm.num_objects >= vm.max_objects) {
			gc();
		}

        switch (opcode) {
            case OP_CONSTANT: {
                int constantIndex = READ_BYTE();
                Value constant = current_bytecode()->constants.values[constantIndex];
                push(constant);
                break;
            }
            
            case OP_ADD: {
            	if (peek_at(2).type == VALUE_OBJECT) {
            		Value other = peek_at(1);
            		Value subject_val = peek_at(2);

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

					if (subject != self) {
						FAIL("Before calling @add, subject is different than the bound method's self attribute.");
					}

					if (add_bound_method->method->is_native) {
						if (!call_native_function(add_bound_method->method, add_bound_method->self)) {
							RUNTIME_ERROR("@add function failed.");
							break;
						}
					} else {
						// TODO: user function
					}
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
            	ObjectString* prototype = NULL;
            	Value constant = READ_CONSTANT();
            	if ((prototype = VALUE_AS_OBJECT(constant, OBJECT_STRING, ObjectString)) == NULL) {
            		FAIL("Expected operand for OP_MAKE_STRING to be an ObjectString* for cloning.");
            	}
            	ObjectString* new_string = object_string_clone(prototype);
            	push(MAKE_VALUE_OBJECT(new_string));
            	break;
            }

			case OP_MAKE_CLASS: {
				ObjectCode* class_body_code = READ_CONSTANT_AS_OBJECT(OBJECT_CODE, ObjectCode);
				CellTable base_func_free_vars = find_free_vars_for_new_function(class_body_code);

				ObjectFunction* class_base_function = object_user_function_new(class_body_code, NULL, 0, base_func_free_vars);
				set_function_name(&MAKE_VALUE_OBJECT(class_base_function), object_string_copy_from_null_terminated("<Class base function>"));

				ObjectClass* class = object_class_new(class_base_function, NULL);

				call_user_function_custom_frame(class_base_function, NULL, (Object*) class, false);

				push(MAKE_VALUE_OBJECT(class));

				break;
			}

            case OP_MAKE_FUNCTION: {
				ObjectCode* object_code = READ_CONSTANT_AS_OBJECT(OBJECT_CODE, ObjectCode);

				uint8_t num_params_byte1 = READ_BYTE();
				uint8_t num_params_byte2 = READ_BYTE();
				uint16_t num_params = two_bytes_to_short(num_params_byte1, num_params_byte2);

				/* Build the params array for the created function */
				char** params = allocate(sizeof(char*) * num_params, "Parameters list cstrings");
				for (int i = 0; i < num_params; i++) {
					Value param_value = READ_CONSTANT();
					ObjectString* param_object_string = NULL;
					if ((param_object_string = VALUE_AS_OBJECT(param_value, OBJECT_STRING, ObjectString)) == NULL) {
						FAIL("Param constant expected to be ObjectString*, actual value type: '%d'", param_value.type);
					}
					params[i] = copy_cstring(param_object_string->chars, param_object_string->length, "ObjectFunction param cstring");
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

				/* Order of peek_previous_frame and pop_frame matters, because behavior of the former
				depends on the state of the call stack */
				StackFrame* previous_frame = peek_previous_frame();
                StackFrame frame = pop_frame();

				if (frame.discard_return_value) {
					pop();
				}

				if (previous_frame != NULL && previous_frame->is_native) {
					is_executing = false;
					goto op_return_cleanup;
				}

                bool is_base_frame = frame.return_address == NULL;
                if (is_base_frame) {

					ObjectThread* running_thread = current_thread();
					ObjectThread* previous_thread = running_thread->previous_thread;
					ObjectThread* next_thread = running_thread->next_thread;

					bool all_threads_finished = previous_thread == NULL && next_thread == NULL;
					if (all_threads_finished) {
						is_executing = false;
						vm.threads = NULL;
						vm.current_thread = NULL;
						goto op_return_cleanup;
					}

					if (previous_thread != NULL) {
						previous_thread->next_thread = next_thread;
					} else {
						vm.threads = next_thread;
					}

					if (next_thread != NULL) {
						next_thread->previous_thread = previous_thread;
						set_current_thread(next_thread);
					} else {
						set_current_thread(vm.threads);
					}

                } else {
                	current_thread()->ip = frame.return_address;
                }

				op_return_cleanup:
                stack_frame_free(&frame);
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
                Value name_value = current_bytecode()->constants.values[READ_BYTE()];
                ASSERT_VALUE_TYPE(name_value, VALUE_OBJECT);
                ObjectString* name_string = object_as_string(name_value.as.object);
                push(load_variable(name_string));

                break;
            }
            
            case OP_SET_VARIABLE: {
                int constant_index = READ_BYTE();
                Value name_val = current_bytecode()->constants.values[constant_index];
                ASSERT_VALUE_TYPE(name_val, VALUE_OBJECT);
                ObjectString* name = OBJECT_AS_STRING(name_val.as.object);

                Value value = pop();

                if (object_value_is(value, OBJECT_FUNCTION)) {
                	set_function_name(&value, name);
                }
				else if (object_value_is(value, OBJECT_CLASS)) {
					set_class_name(&value, name);
				}

                cell_table_set_value_cstring_key(locals_or_module_table(), name->chars, value);
                break;
            }
            
            case OP_CALL: {
            	int arg_count = READ_BYTE();

				if (peek().type != VALUE_OBJECT) {
					RUNTIME_ERROR("Cannot call non-object.");
					break;
				}

				if (peek().as.object->type == OBJECT_FUNCTION) {
					ObjectFunction* function = OBJECT_AS_FUNCTION(pop().as.object);

					if (arg_count != function->num_params) {
						RUNTIME_ERROR("Function called with %d arguments, needs %d.", arg_count, function->num_params);
						break;
					}

					if (function->is_native) {
						if (!call_native_function(function, NULL)) {
							RUNTIME_ERROR("Native function failed.");
							break;
						}
					} else {
						call_user_function(function, NULL);
					}
				}

				else if (peek().as.object->type == OBJECT_BOUND_METHOD) {
					ObjectBoundMethod* bound_method = (ObjectBoundMethod*) pop().as.object;
					ObjectFunction* method = bound_method->method;

					if (arg_count != method->num_params) {
						RUNTIME_ERROR("Function called with %d arguments, needs %d.", arg_count, method->num_params);
						break;
					}
					
					if (method->is_native) {
						if (!call_native_function(method, (Object*) bound_method->self)) {
							RUNTIME_ERROR("Native @init method failed.");
							break;
						}
					} else {
						call_user_function(method, bound_method->self);
					}
				}

				else if (peek().as.object->type == OBJECT_CLASS) {
					ObjectClass* klass = (ObjectClass*) pop().as.object;
					ObjectInstance* instance = object_instance_new(klass);

					Value init_method_value;
					if (object_load_attribute_cstring_key((Object*) instance, "@init", &init_method_value)) {
						ObjectBoundMethod* init_bound_method = NULL;
						if ((init_bound_method = VALUE_AS_OBJECT(init_method_value, OBJECT_BOUND_METHOD, ObjectBoundMethod)) == NULL) {
							RUNTIME_ERROR("@init attribute of class is not a method.");
							break;
						}

						if (init_bound_method->self != (Object*) instance) {
							FAIL("When instantiating class, bound method's self and subject instance are different.");
						}

						ObjectFunction* init_method = init_bound_method->method;

						if (arg_count != init_method->num_params) {
							RUNTIME_ERROR("@init called with %d arguments, needs %d.", arg_count, init_method->num_params);
							break;
						}
						
						if (init_method->is_native) {
							if (!call_native_function(init_method, (Object*) instance)) {
								RUNTIME_ERROR("Native @init method failed.");
								break;
							}
						} else {
							call_user_function_custom_frame(init_method, init_bound_method->self, NULL, true);
						}
					} else if (arg_count != 0) {
						RUNTIME_ERROR("@init function of class %.*s doesn't take parameters.", klass->name_length, klass->name);
						break;
					}

					push(MAKE_VALUE_OBJECT(instance));
				}

				else {
					RUNTIME_ERROR("Cannot call non function or class.");
					break;
				}

                break;
            }

            case OP_GET_ATTRIBUTE: {
                int constant_index = READ_BYTE();
                Value name_val = current_bytecode()->constants.values[constant_index];
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

				/* TODO: Runtime error instead of nil */
				push(MAKE_VALUE_NIL());
                break;
            }

            case OP_SET_ATTRIBUTE: {
                Value name_val = current_bytecode()->constants.values[READ_BYTE()];
                ASSERT_VALUE_TYPE(name_val, VALUE_OBJECT);
                ObjectString* name = OBJECT_AS_STRING(name_val.as.object);

                Value obj_value = pop();
                if (obj_value.type != VALUE_OBJECT) {
                	RUNTIME_ERROR("Cannot set attribute on non-object.");
                	break;
                }

                Object* object = obj_value.as.object;
                Value attribute_value = pop();
                cell_table_set_value_cstring_key(&object->attributes, name->chars, attribute_value);

                break;
            }

            case OP_ACCESS_KEY: {
            	Value subject_value = pop();
            	if (subject_value.type != VALUE_OBJECT) {
					RUNTIME_ERROR("Accessing key on none object. Actual value type: %d", subject_value.type);
					break;
            	}

            	Object* subject = subject_value.as.object;
            	Value key = peek();

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

				if (subject != self) {
					FAIL("Before calling @get_key, subject was different than bound method's self attribute.");
				}

				Value result;
				if (bound_method->method->is_native) {
					if (!call_native_function(bound_method->method, self)) {
						RUNTIME_ERROR("@get_key function failed.");
						break;
					}
				} else {
					// TODO: user function
				}

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

            	ObjectBoundMethod* set_method = NULL;
            	MethodAccessResult access_result = -1;
            	if ((access_result = object_get_method(subject, "@set_key", &set_method)) == METHOD_ACCESS_SUCCESS) {
					Object* self = set_method->self;

					if (self != subject) {
						FAIL("Before calling @set_key, subject was different from bound method's self attribute.");
					}

    				ValueArray arguments;
    				value_array_init(&arguments);
    				value_array_write(&arguments, &key);
    				value_array_write(&arguments, &value);

    				Value throwaway_result;
    				if (set_method->method->is_native) {
    					if (!set_method->method->native_function(self, arguments, &throwaway_result)) {
    						RUNTIME_ERROR("@set_key function failed.");
    						goto op_set_key_cleanup;
    					}
    				} else {
    					// TODO: user function
    				}

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
            	uint16_t address = two_bytes_to_short(addr_byte1, addr_byte2);
				Value condition = pop();

				ERROR_IF_NON_BOOLEAN(condition, "Expected boolean as condition");

				if (!condition.as.boolean) {
					current_thread()->ip = current_bytecode()->code + address;
				}

				break;
			}

            case OP_JUMP: {
            	uint8_t addr_byte1 = READ_BYTE();
            	uint8_t addr_byte2 = READ_BYTE();
            	uint16_t address = two_bytes_to_short(addr_byte1, addr_byte2);

            	current_thread()->ip = current_bytecode()->code + address;

            	break;
            }

			case OP_IMPORT: {
				/* TODO: Precise automated tests for import resolution order. It's one thing which isn't totally
				tested because of lack of functionality in the test runner right now */

				ObjectString* module_name = READ_CONSTANT_AS_OBJECT(OBJECT_STRING, ObjectString);

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

				Value module_value;
            	bool module_already_imported = cell_table_get_value_cstring_key(&vm.imported_modules, module_name->chars, &module_value);
            	if (module_already_imported) {
            		push(module_value);
            		push(MAKE_VALUE_NIL()); /* This is a patch because currently the compiler puts a OP_NIL OP_RETURN
            								 at the end of the module, just the same as with functions.
            								 And so after we import something, the bytecode does OP_POP to get rid of the NIL.
            								 So if we don't import the module because it's already cached, we should push the NIL ourselves.
            								 Remove this ugly patch after we fixed the compiler regarding this stuff. */
            		break;
            	}

				main_module_dir = directory_from_path(vm.main_module_path);
				user_module_file_name = concat_cstrings(
					module_name->chars, module_name->length, user_module_file_suffix, strlen(user_module_file_suffix), "user module file name");
				user_module_path = concat_null_terminated_paths(main_module_dir, user_module_file_name, "user module path");

				if (io_file_exists(user_module_path)) {
					char* load_module_error = NULL;
					if (!load_text_module(module_name, user_module_path, &load_module_error)) {
						RUNTIME_ERROR("%s", load_module_error);
					}
					goto op_import_cleanup;
				}

				user_extension_module_file_name = concat_cstrings(
					module_name->chars, module_name->length, 
					extension_module_file_suffix, strlen(extension_module_file_suffix), "user extension module file name");
				user_extension_module_path = concat_null_terminated_paths(
					main_module_dir, user_extension_module_file_name, "user extension module path");

				if (io_file_exists(user_extension_module_path)) {
					int result = load_extension_module(module_name, user_extension_module_path);
					if (result == LOAD_EXTENSION_OPEN_FAILURE) {
						DWORD error = GetLastError();
						RUNTIME_ERROR("Unable to open extension module %s. Error code: %ld", user_extension_module_file_name, error);
						goto op_import_cleanup;
					}
					if (result == LOAD_EXTENSION_NO_INIT_FUNCTION) {
						RUNTIME_ERROR("Unable to get plane_module_init function from extension module %s", user_extension_module_file_name);
						goto op_import_cleanup;
					}
					goto op_import_cleanup;
				}

				Value builtin_module_value;
				if (cell_table_get_value_cstring_key(&vm.builtin_modules, module_name->chars, &builtin_module_value)) {
					ObjectModule* module = NULL;
					if ((module = VALUE_AS_OBJECT(builtin_module_value, OBJECT_MODULE, ObjectModule)) == NULL) {
						FAIL("Found non ObjectModule* in builtin modules table.");
					}

					push(MAKE_VALUE_OBJECT(module));

 					/* Ugly hack to handle current situation in the compiler, see note earlier in OP_IMPORT case */
					push(MAKE_VALUE_NIL());

					cell_table_set_value_cstring_key(&vm.imported_modules, module_name->chars, MAKE_VALUE_OBJECT(module));

					goto op_import_cleanup;
				}

				stdlib_path = concat_null_terminated_paths(vm.interpreter_dir_path, VM_STDLIB_RELATIVE_PATH, "stdlib path");
				stdlib_module_path = concat_null_terminated_paths(stdlib_path, user_module_file_name, "stdlib module path");

				if (io_file_exists(stdlib_module_path)) {
					char* load_module_error = NULL;
					if (!load_text_module(module_name, stdlib_module_path, &load_module_error)) {
						RUNTIME_ERROR("%s", load_module_error);
					}
					goto op_import_cleanup;
				}

				extension_module_file_name = concat_cstrings(
					module_name->chars, module_name->length, extension_module_file_suffix, strlen(extension_module_file_suffix),
					"extension module file name");
				stdlib_extension_path = concat_null_terminated_paths(stdlib_path, extension_module_file_name, "stdlib extension path");

				if (io_file_exists(stdlib_extension_path)) {
					int result = load_extension_module(module_name, stdlib_extension_path);
					if (result == LOAD_EXTENSION_OPEN_FAILURE) {
						DWORD error = GetLastError();
						RUNTIME_ERROR("Unable to open extension module %s. Error code: %ld", extension_module_file_name, error);
						goto op_import_cleanup;
					}
					if (result == LOAD_EXTENSION_NO_INIT_FUNCTION) {
						RUNTIME_ERROR("Unable to get plane_module_init function from extension module %s", extension_module_file_name);
						goto op_import_cleanup;
					}
					goto op_import_cleanup;
				}

				RUNTIME_ERROR("Couldn't find module %s", module_name->chars);

				op_import_cleanup:
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

				break;
			}

            default: {
            	FAIL("Unknown opcode: %d. At ip: %p", opcode, current_thread()->ip - 1);
            }
        }

		vm.thread_opcode_counter = (vm.thread_opcode_counter + 1) % THREAD_SWITCH_INTERVAL;
		DEBUG_THREADING_PRINT("thread_opcode_counter: %d\n", thread_opcode_counter);

		/* TODO: Potential bug here? is_executing now means "is running current eval loop",
		but in this case it's assumed to mean "is VM executing". This can kinda screw up thread scheduling, address this. */

		if (is_executing && vm.thread_opcode_counter == 0) {
			ObjectThread* old_thread = vm.current_thread;
			ObjectThread* new_thread = vm.current_thread->next_thread != NULL ? vm.current_thread->next_thread : vm.threads;
			
			vm.current_thread = new_thread;

			DEBUG_THREADING_PRINT("Switching threads: \n");
			if (DEBUG_THREADING) {
				printf("From: ");
				object_thread_print(old_thread);
				printf("\nTo: ");
				object_thread_print(new_thread);
				printf("\n");
			}
		}

		if (DEBUG_PAUSE_AFTER_OPCODES) {
			printf("\nPress ENTER to continue.\n");
			getchar();
		}
    }

	if (vm.threads != NULL) {
		ObjectThread* thread = current_thread();
		while (thread->call_stack_top > thread->call_stack && !peek_current_frame()->is_native) {
			StackFrame frame = pop_frame();
			bool is_native = frame.is_native;

			stack_frame_free(&frame);
		}
	}

    DEBUG_TRACE("\n--------------------------\n");
	DEBUG_TRACE("Ended interpreter loop.");

    #undef BINARY_MATH_OP

    return runtime_error_occured ? INTERPRET_RUNTIME_ERROR : INTERPRET_SUCCESS;
}

InterpretResult vm_interpret_program(Bytecode* bytecode, char* main_module_path) {
	vm.main_module_path = main_module_path;

	ObjectCode* code = object_code_new(*bytecode);
	ObjectFunction* base_function = object_user_function_new(code, NULL, 0, cell_table_new_empty());
	ObjectThread* main_thread = object_thread_new(base_function, "<main thread>");
	switch_to_new_thread(main_thread);

	/* Cleanup unused objects created during compilation */
	DEBUG_OBJECTS_PRINT("Setting vm.allow_gc = true.");
	vm.allow_gc = true;
	gc();

	ObjectString* base_module_name = object_string_copy_from_null_terminated("<main>");
	ObjectModule* module = object_module_new(base_module_name, base_function);
	StackFrame base_frame = new_stack_frame(NULL, base_function, (Object*) module, true, false, false);

	DEBUG_TRACE("Starting interpreter loop.");
	return vm_interpret_frame(&base_frame);
}

#undef READ_BYTE
#undef READ_CONSTANT
