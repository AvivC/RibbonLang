#include <string.h>
#include <math.h>

#include "common.h"
#include "plane_utils.h"
#include "plane_object.h"
#include "vm.h"
#include "value.h"
#include "memory.h"
#include "table.h"

static Object* allocate_object(size_t size, const char* what, ObjectType type) {
	DEBUG_OBJECTS_PRINT("Allocating object '%s' of size %d and type %d.", what, size, type);

    Object* object = allocate(size, what);
    object->type = type;    
    object->is_reachable = false;
    object->next = vm.objects;
    vm.objects = object;
    cell_table_init(&object->attributes);
    
    vm.num_objects++;
    DEBUG_OBJECTS_PRINT("Incremented num_objects to %d", vm.numObjects);

    return object;
}

bool object_value_is(Value value, ObjectType type) {
	return value.type == VALUE_OBJECT && value.as.object->type == type;
}

static void set_object_native_method(Object* object, const char* method_name, char** params, int num_params, NativeFunction function) {
	int num_params_including_self = num_params + 1;
	char** copied_params = allocate(sizeof(char*) * num_params_including_self, "Parameters list cstrings");

	copied_params[0] = copy_null_terminated_cstring("self", "ObjectFunction param cstring");
	for (int i = 0; i < num_params; i++) {
		copied_params[i + 1] = copy_null_terminated_cstring(params[i], "ObjectFunction param cstring");
	}

	ObjectFunction* method = object_native_function_new(function, copied_params, num_params_including_self, object);
	cell_table_set_value_cstring_key(&object->attributes, method_name, MAKE_VALUE_OBJECT(method));
}

static bool object_string_add(ValueArray args, Value* result) {
	Value self_value = args.values[0];
	Value other_value = args.values[1];

    if (!object_value_is(self_value, OBJECT_STRING) || !object_value_is(other_value, OBJECT_STRING)) {
    	*result = MAKE_VALUE_NIL();
    	return false;
    }

    ObjectString* self_string = OBJECT_AS_STRING(self_value.as.object);
    ObjectString* other_string = OBJECT_AS_STRING(other_value.as.object);

    char* buffer = allocate(self_string->length + other_string->length + 1, "Object string buffer");
    memcpy(buffer, self_string->chars, self_string->length);
    memcpy(buffer + self_string->length, other_string->chars, other_string->length);
    int string_length = self_string->length + other_string->length;
    buffer[string_length] = '\0';
    ObjectString* object_string = object_string_take(buffer, string_length);

    *result = MAKE_VALUE_OBJECT(object_string);
    return true;
}

static bool object_string_length(ValueArray args, Value* result) {
	Value self_value = args.values[0];

    if (!object_value_is(self_value, OBJECT_STRING)) {
    	FAIL("String length method called on none ObjectString.");
    }

    ObjectString* self_string = OBJECT_AS_STRING(self_value.as.object);

    *result = MAKE_VALUE_NUMBER(self_string->length);
    return true;
}

static bool object_table_length(ValueArray args, Value* result) {
	Value self_value = args.values[0];

    if (!object_value_is(self_value, OBJECT_TABLE)) {
    	FAIL("Table length method called on none ObjectTable.");
    }

    ObjectTable* self_table = (ObjectTable*) self_value.as.object;

	PointerArray entries = table_iterate(&self_table->table, "table object length table_iterate buffer");
    *result = MAKE_VALUE_NUMBER(entries.count);
	pointer_array_free(&entries);

    return true;
}

static bool object_string_get_key(ValueArray args, Value* result) {
	// TODO: Proper error reporting mechanisms! not just a boolean which tells the user nothing.

	Value self_value = args.values[0];
	Value other_value = args.values[1];

    if (!object_value_is(self_value, OBJECT_STRING)) {
    	FAIL("String @get_key called on none ObjectString.");
    }

    if (other_value.type != VALUE_NUMBER) {
    	*result = MAKE_VALUE_NIL();
    	return false;
    }

    ObjectString* self_string = object_as_string(self_value.as.object);

    double other_as_number = other_value.as.number;
    bool number_is_integer = floor(other_as_number) == other_as_number;
    if (!number_is_integer) {
    	*result = MAKE_VALUE_NIL();
		return false;
    }

    if (other_as_number < 0 || other_as_number > self_string->length - 1) {
    	*result = MAKE_VALUE_NIL();
		return false;
    }

    char char_result = self_string->chars[(int) other_as_number];
    *result = MAKE_VALUE_OBJECT(object_string_copy(&char_result, 1));
    return true;
}

static bool table_get_key_function(ValueArray args, Value* result) {
	// TODO: Proper error reporting mechanisms! not just a boolean which tells the user nothing.

	Value self_value = args.values[0];
	Value key = args.values[1];

    if (!object_value_is(self_value, OBJECT_TABLE)) {
    	FAIL("Table @get_key called on none ObjectTable.");
    }

    ObjectTable* self_table = (ObjectTable*) self_value.as.object;

    Value value;
    if (table_get(&self_table->table, key, &value)) {
    	*result = value;
    } else {
    	*result = MAKE_VALUE_NIL();
    }

    return true;
}

static bool table_set_key_function(ValueArray args, Value* result) {
	if (args.count != 3) {
		FAIL("Table @set_key called with argument number other than 3: %d", args.count);
	}

	Value self_value = args.values[0];
	Value key_value = args.values[1];
	Value value_to_set = args.values[2];

    if (!object_value_is(self_value, OBJECT_TABLE)) {
    	FAIL("Table @set_key called on none ObjectTable.");
    }

    ObjectTable* self_table = (ObjectTable*) self_value.as.object;

    table_set(&self_table->table, key_value, value_to_set);

	*result = MAKE_VALUE_NIL();
    return true;
}

static ObjectString* object_string_new_partial(char* chars, int length) {
	ObjectString* string = (ObjectString*) allocate_object(sizeof(ObjectString), "ObjectString", OBJECT_STRING);
	string->chars = copy_null_terminated_cstring(chars, "Object string buffer");
	string->length = length;
	return string;
}

// static ObjectString* object_string_new(char* chars, int length) {
//     DEBUG_OBJECTS_PRINT("Allocating string object '%s' of length %d.", chars, length);

//     ObjectString* string = (ObjectString*) allocate_object(sizeof(ObjectString), "ObjectString", OBJECT_STRING);

//     string->chars = chars;
// 	string->length = length;

// 	ObjectFunction* string_add_method = NULL;

// 	if (string_add_method == NULL) {
// 		char* params[] =  {"other"};
// 		int num_params = 1;
// 		int num_params_including_self = num_params + 1;
// 		char** copied_params = allocate(sizeof(char*) * num_params_including_self, "Parameters list cstrings");

// 		copied_params[0] = copy_null_terminated_cstring("self", "ObjectFunction param cstring");
// 		for (int i = 0; i < num_params; i++) {
// 			copied_params[i + 1] = copy_null_terminated_cstring(params[i], "ObjectFunction param cstring");
// 		}

// 		string_add_method = object_native_function_new(object_string_add, copied_params, num_params_including_self, (Object*) string);
// 	}

// 	// static ObjectFunction* string_get_key_method = NULL;
// 	ObjectFunction* string_get_key_method = NULL;

// 	if (string_get_key_method == NULL) {
// 		char* params[] =  {"other"};
// 		int num_params = 1;
// 		int num_params_including_self = num_params + 1;
// 		char** copied_params = allocate(sizeof(char*) * num_params_including_self, "Parameters list cstrings");

// 		copied_params[0] = copy_null_terminated_cstring("self", "ObjectFunction param cstring");
// 		for (int i = 0; i < num_params; i++) {
// 			copied_params[i + 1] = copy_null_terminated_cstring(params[i], "ObjectFunction param cstring");
// 		}

// 		string_get_key_method = object_native_function_new(object_string_get_key, copied_params, num_params_including_self, (Object*) string);
// 	}

// 	// static ObjectFunction* string_length_method = NULL;
// 	ObjectFunction* string_length_method = NULL;

// 	if (string_length_method == NULL) {
// 		char* params[] =  {};
// 		int num_params = 0;
// 		int num_params_including_self = num_params + 1;
// 		char** copied_params = allocate(sizeof(char*) * num_params_including_self, "Parameters list cstrings");

// 		copied_params[0] = copy_null_terminated_cstring("self", "ObjectFunction param cstring");
// 		for (int i = 0; i < num_params; i++) {
// 			copied_params[i + 1] = copy_null_terminated_cstring(params[i], "ObjectFunction param cstring");
// 		}

// 		string_length_method = object_native_function_new(object_string_length, copied_params, num_params_including_self, (Object*) string);
// 	}

// 	ObjectString* add_attr_key = object_string_new_partial("@add", strlen("@add"));
// 	cell_table_set_value_directly(&string->base.attributes, MAKE_VALUE_OBJECT(add_attr_key), MAKE_VALUE_OBJECT(string_add_method));

// 	ObjectString* get_key_attr_key = object_string_new_partial("@get_key", strlen("@get_key"));
// 	cell_table_set_value_directly(&string->base.attributes, MAKE_VALUE_OBJECT(get_key_attr_key), MAKE_VALUE_OBJECT(string_get_key_method));

// 	ObjectString* length_attr_key = object_string_new_partial("length", strlen("length"));
// 	cell_table_set_value_directly(&string->base.attributes, MAKE_VALUE_OBJECT(length_attr_key), MAKE_VALUE_OBJECT(string_length_method));

//     return string;
// }


static ObjectString* object_string_new(char* chars, int length) {
    DEBUG_OBJECTS_PRINT("Allocating string object '%s' of length %d.", chars, length);

    ObjectString* string = (ObjectString*) allocate_object(sizeof(ObjectString), "ObjectString", OBJECT_STRING);

    string->chars = chars;
	string->length = length;

	// ObjectFunction* string_add_method = NULL;
	ObjectFunction* string_add_method = make_native_function_with_params("@add", 2, (char*[]) {"self", "other"}, object_string_add);
	ObjectBoundMethod* string_add_bound_method = object_bound_method_new(string_add_method, (Object*) string);

	// char* params[] =  {"other"};
	// int num_params = 1;
	// int num_params_including_self = num_params + 1;
	// char** copied_params = allocate(sizeof(char*) * num_params_including_self, "Parameters list cstrings");

	// copied_params[0] = copy_null_terminated_cstring("self", "ObjectFunction param cstring");
	// for (int i = 0; i < num_params; i++) {
	// 	copied_params[i + 1] = copy_null_terminated_cstring(params[i], "ObjectFunction param cstring");
	// }

	// string_add_method = object_native_function_new(object_string_add, copied_params, num_params_including_self, (Object*) string);

	// static ObjectFunction* string_get_key_method = NULL;
	ObjectFunction* string_get_key_method = NULL;

	if (string_get_key_method == NULL) {
		char* params[] =  {"other"};
		int num_params = 1;
		int num_params_including_self = num_params + 1;
		char** copied_params = allocate(sizeof(char*) * num_params_including_self, "Parameters list cstrings");

		copied_params[0] = copy_null_terminated_cstring("self", "ObjectFunction param cstring");
		for (int i = 0; i < num_params; i++) {
			copied_params[i + 1] = copy_null_terminated_cstring(params[i], "ObjectFunction param cstring");
		}

		string_get_key_method = object_native_function_new(object_string_get_key, copied_params, num_params_including_self, (Object*) string);
	}

	// static ObjectFunction* string_length_method = NULL;
	ObjectFunction* string_length_method = NULL;

	if (string_length_method == NULL) {
		char* params[] =  {};
		int num_params = 0;
		int num_params_including_self = num_params + 1;
		char** copied_params = allocate(sizeof(char*) * num_params_including_self, "Parameters list cstrings");

		copied_params[0] = copy_null_terminated_cstring("self", "ObjectFunction param cstring");
		for (int i = 0; i < num_params; i++) {
			copied_params[i + 1] = copy_null_terminated_cstring(params[i], "ObjectFunction param cstring");
		}

		string_length_method = object_native_function_new(object_string_length, copied_params, num_params_including_self, (Object*) string);
	}

	ObjectString* add_attr_key = object_string_new_partial("@add", strlen("@add"));
	// cell_table_set_value_directly(&string->base.attributes, MAKE_VALUE_OBJECT(add_attr_key), MAKE_VALUE_OBJECT(string_add_method));
	cell_table_set_value_directly(&string->base.attributes, MAKE_VALUE_OBJECT(add_attr_key), MAKE_VALUE_OBJECT(string_add_bound_method));

	ObjectString* get_key_attr_key = object_string_new_partial("@get_key", strlen("@get_key"));
	cell_table_set_value_directly(&string->base.attributes, MAKE_VALUE_OBJECT(get_key_attr_key), MAKE_VALUE_OBJECT(string_get_key_method));

	ObjectString* length_attr_key = object_string_new_partial("length", strlen("length"));
	cell_table_set_value_directly(&string->base.attributes, MAKE_VALUE_OBJECT(length_attr_key), MAKE_VALUE_OBJECT(string_length_method));

    return string;
}

ObjectString* object_string_copy(const char* string, int length) {
	// argument length should not include the null-terminator
    char* chars = copy_cstring(string, length, "Object string buffer");
    return object_string_new(chars, length);
}

ObjectString* object_string_copy_from_null_terminated(const char* string) {
	return object_string_copy(string, strlen(string));
}

ObjectString* object_string_take(char* chars, int length) {
    // Assume chars is already null-terminated
    return object_string_new(chars, length);
}

ObjectString* object_string_clone(ObjectString* original) {
	return object_string_copy(original->chars, original->length);
}

ObjectString** object_create_copied_strings_array(const char** strings, int num, const char* allocDescription) {
	ObjectString** array = allocate(sizeof(ObjectString*) * num, allocDescription);
	for (int i = 0; i < num; i++) {
		array[i] = object_string_copy(strings[i], strlen(strings[i]));
	}
	return array;
}

bool object_cstrings_equal(const char* a, const char* b, int length) {
    // return strcmp(a, b) == 0; // This assumes null terminated strings
    return strncmp(a, b, length) == 0;
}

bool object_strings_equal(ObjectString* a, ObjectString* b) {
    return (a->length == b->length) && (object_cstrings_equal(a->chars, b->chars, a->length));
}

static ObjectFunction* object_function_base_new(bool isNative, char** parameters, int numParams, Object* self, CellTable free_vars) {
    ObjectFunction* objFunc = (ObjectFunction*) allocate_object(sizeof(ObjectFunction), "ObjectFunction", OBJECT_FUNCTION);
    objFunc->name = copy_null_terminated_cstring("<Anonymous function>", "Function name");
    objFunc->is_native = isNative;
    objFunc->parameters = parameters;
    objFunc->num_params = numParams;
    objFunc->self = self;
    objFunc->free_vars = free_vars;
    return objFunc;
}

ObjectFunction* object_user_function_new(ObjectCode* code, char** parameters, int numParams, Object* self, CellTable free_vars) {
    DEBUG_OBJECTS_PRINT("Creating user function object.");
    ObjectFunction* objFunc = object_function_base_new(false, parameters, numParams, self, free_vars);
    objFunc->code = code;
    return objFunc;
}

ObjectFunction* object_native_function_new(NativeFunction nativeFunction, char** parameters, int numParams, Object* self) {
    DEBUG_OBJECTS_PRINT("Creating native function object.");
    ObjectFunction* objFunc = object_function_base_new(true, parameters, numParams, self, cell_table_new_empty());
    objFunc->native_function = nativeFunction;
    return objFunc;
}

ObjectFunction* make_native_function_with_params(char* name, int num_params, char** params, NativeFunction function) {
	/* name must be null terminated. It is copied and ObjectFunction takes ownership over the copy.
	Can simply be a literal. Otherwise caller must free it later. */

	char** params_buffer = allocate(sizeof(char*) * num_params, "Parameters list cstrings");
	for (int i = 0; i < num_params; i++) {
		params_buffer[i] = copy_cstring(params[i], strlen(params[i]), "ObjectFunction param cstring");
	}
	ObjectFunction* func = object_native_function_new(function, params_buffer, num_params, NULL);
	name = copy_null_terminated_cstring(name, "Function name");
	object_function_set_name(func, name);
	return func;
}

void object_function_set_name(ObjectFunction* function, char* name) {
	if (function->name == NULL) {
		FAIL("function->name == NULL in object_function_set_name");
	}

	deallocate(function->name, strlen(function->name) + 1, "Function name");
	function->name = name;
}

void object_class_set_name(ObjectClass* klass, char* name, int length) {
	klass->name = name;
	klass->name_length = length;
}

ObjectBoundMethod* object_bound_method_new(ObjectFunction* method, Object* self) {
	ObjectBoundMethod* bound_method 
		= (ObjectBoundMethod*) allocate_object(sizeof(ObjectBoundMethod), "ObjectBoundMethod", OBJECT_BOUND_METHOD);
	bound_method->method = method;
	bound_method->self = self;
	return bound_method;
}

ObjectCode* object_code_new(Bytecode chunk) {
	ObjectCode* obj_code = (ObjectCode*) allocate_object(sizeof(ObjectCode), "ObjectCode", OBJECT_CODE);
	obj_code->bytecode = chunk;
	return obj_code;
}

ObjectTable* object_table_new(Table table) {
	ObjectTable* object_table = (ObjectTable*) allocate_object(sizeof(ObjectTable), "ObjectTable", OBJECT_TABLE);
	object_table->table = table;

	/* TODO: The method system changed. At time of writing tables and strings still use this kind of thing, we
	need to change it at some point. */
	set_object_native_method((Object*) object_table, "length", (char*[]){}, 0, object_table_length);
	set_object_native_method((Object*) object_table, "@get_key", (char*[]){"other"}, 1, table_get_key_function);
	set_object_native_method((Object*) object_table, "@set_key", (char*[]){"key", "value"}, 2, table_set_key_function);

	return object_table;
}

ObjectTable* object_table_new_empty(void) {
	return object_table_new(table_new_empty());
}

ObjectCell* object_cell_new(Value value) {
	ObjectCell* cell = (ObjectCell*) allocate_object(sizeof(ObjectCell), "ObjectCell", OBJECT_CELL);
	cell->value = value;
	cell->is_filled = true;
	return cell;
}

ObjectCell* object_cell_new_empty(void) {
	ObjectCell* cell = (ObjectCell*) allocate_object(sizeof(ObjectCell), "ObjectCell", OBJECT_CELL);
	cell->value = MAKE_VALUE_NIL(); // Just for kicks
	cell->is_filled = false;
	return cell;
}

static ObjectClass* object_class_new_base(
		ObjectFunction* base_function, char* name, size_t instance_size, DeallocationFunction dealloc_func, GcMarkFunction gc_mark_func) {

	ObjectClass* klass = (ObjectClass*) allocate_object(sizeof(ObjectClass), "ObjectClass", OBJECT_CLASS);
	name = name == NULL ? "<Anonymous class>" : name;
	klass->name = copy_null_terminated_cstring(name, "Class name");
	klass->name_length = strlen(name);
	klass->base_function = base_function;
	klass->instance_size = instance_size;
	klass->dealloc_func = dealloc_func;
	klass->gc_mark_func = gc_mark_func;

	return klass;
}

ObjectClass* object_class_new(ObjectFunction* base_function, char* name) {
	return object_class_new_base(base_function, name, 0, NULL, NULL);
}

ObjectClass* object_class_native_new(char* name, size_t instance_size, DeallocationFunction dealloc_func, GcMarkFunction gc_mark_func) {
	if (dealloc_func == NULL) {
		FAIL("NULL dealloc_func passed for native class.");
	}
	if (instance_size <= 0) {
		FAIL("instance_size <= passed for native class.");
	}

	return object_class_new_base(NULL, name, instance_size, dealloc_func, gc_mark_func);
}

ObjectInstance* object_instance_new(ObjectClass* klass) {
	ObjectInstance* instance = NULL;

	if (klass->instance_size > 0) { // Native class
		instance = (ObjectInstance*) allocate_object(klass->instance_size, "ObjectInstance", OBJECT_INSTANCE);
	} else { // User class
		instance = (ObjectInstance*) allocate_object(sizeof(ObjectInstance), "ObjectInstance", OBJECT_INSTANCE);
	}
	
	instance->klass = klass;
	return instance;
}

ObjectModule* object_module_new(ObjectString* name, ObjectFunction* function) {
	ObjectModule* module = (ObjectModule*) allocate_object(sizeof(ObjectModule), "ObjectModule", OBJECT_MODULE);
	module->name = name;
	module->function = function;
	module->dll = NULL;
	return module;
}

ObjectModule* object_module_native_new(ObjectString* name, HMODULE dll) {
	ObjectModule* module = object_module_new(name, NULL);
	module->dll = dll;
	return module;
}

ObjectThread* object_thread_new(ObjectFunction* function, char* name) {
	ObjectThread* thread = (ObjectThread*) allocate_object(sizeof(ObjectThread), "ObjectThread", OBJECT_THREAD);
	thread->name = copy_null_terminated_cstring(name, "Thread name");
	thread->previous_thread = NULL;
	thread->next_thread = NULL;
	thread->base_function = function;
	thread->ip = function->code->bytecode.code;
	thread->eval_stack_top = thread->eval_stack;
	thread->call_stack_top = thread->call_stack;
	return thread;
}

void object_free(Object* o) {
	cell_table_free(&o->attributes);

	ObjectType type = o->type;

    switch (type) {
        case OBJECT_STRING: {
            ObjectString* string = (ObjectString*) o;
            DEBUG_OBJECTS_PRINT("Freeing ObjectString '%s'", string->chars);
            deallocate(string->chars, string->length + 1, "Object string buffer");
            deallocate(string, sizeof(ObjectString), "ObjectString");
            break;
        }
        case OBJECT_FUNCTION: {
            ObjectFunction* func = (ObjectFunction*) o;
            if (func->is_native) {
            	DEBUG_OBJECTS_PRINT("Freeing native ObjectFunction");
			} else {
				DEBUG_OBJECTS_PRINT("Freeing user ObjectFunction");
			}
            for (int i = 0; i < func->num_params; i++) {
            	char* param = func->parameters[i];
				deallocate(param, strlen(param) + 1, "ObjectFunction param cstring");
			}
            if (func->num_params > 0) {
            	deallocate(func->parameters, sizeof(char*) * func->num_params, "Parameters list cstrings");
            }
            cell_table_free(&func->free_vars);
            deallocate(func->name, strlen(func->name) + 1, "Function name");
            deallocate(func, sizeof(ObjectFunction), "ObjectFunction");
            break;
        }
        case OBJECT_CODE: {
        	ObjectCode* code = (ObjectCode*) o;
        	DEBUG_OBJECTS_PRINT("Freeing ObjectCode at '%p'", code);
        	bytecode_free(&code->bytecode);
        	deallocate(code, sizeof(ObjectCode), "ObjectCode");
        	break;
        }
		case OBJECT_THREAD: {
        	ObjectThread* thread = (ObjectThread*) o;
        	DEBUG_OBJECTS_PRINT("Freeing ObjectThread at '%p'", thread);
			deallocate(thread->name, strlen(thread->name) + 1, "Thread name");
        	deallocate(thread, sizeof(ObjectThread), "ObjectThread");
        	break;
        }
        case OBJECT_TABLE: {
        	ObjectTable* table = (ObjectTable*) o;
        	DEBUG_OBJECTS_PRINT("Freeing ObjectTable at '%p'", table);
        	table_free(&table->table);
        	deallocate(table, sizeof(ObjectTable), "ObjectTable");
        	break;
        }
        case OBJECT_CELL: {
        	ObjectCell* cell = (ObjectCell*) o;
        	DEBUG_OBJECTS_PRINT("Freeing ObjectCell at '%p'", cell);
        	deallocate(cell, sizeof(ObjectCell), "ObjectCell");
        	break;
        }
        case OBJECT_MODULE: {
        	ObjectModule* module = (ObjectModule*) o;
			DEBUG_OBJECTS_PRINT("Freeing ObjectModule at '%p'", cell);
			if (module->dll != NULL) {
				FreeLibrary(module->dll);
			}
			deallocate(module, sizeof(ObjectModule), "ObjectModule");
        	break;
        }
		case OBJECT_CLASS: {
			ObjectClass* class = (ObjectClass*) o;
			DEBUG_OBJECTS_PRINT("Freeing ObjectClass at '%p'", class);
			deallocate(class->name, class->name_length + 1, "Class name");
			deallocate(class, sizeof(ObjectClass), "ObjectClass");
        	break;
		}
		case OBJECT_INSTANCE: {
			ObjectInstance* instance = (ObjectInstance*) o;
			DEBUG_OBJECTS_PRINT("Freeing ObjectInstance at '%p'", instance);

			ObjectClass* klass = instance->klass;
			if (klass->instance_size > 0) {
				/* Native class */
				klass->dealloc_func(instance);
				deallocate(instance, klass->instance_size, "ObjectInstance");
			} else {
				/* Plane class */
				deallocate(instance, sizeof(ObjectInstance), "ObjectInstance");
			}

			break;
		}
		case OBJECT_BOUND_METHOD: {
			ObjectBoundMethod* bound_method = (ObjectBoundMethod*) o;
			DEBUG_OBJECTS_PRINT("Freeing ObjectBoundMethod at '%p'", instance);
			deallocate(bound_method, sizeof(ObjectBoundMethod), "ObjectBoundMethod");
			break;
		}
    }
    
    vm.num_objects--;
    DEBUG_OBJECTS_PRINT("Decremented numObjects to %d", vm.numObjects);
}

static void print_function(ObjectFunction* function) {
	if (function->is_native) {
		printf("<Native function %s at %p>", function->name, function);
	} else {
		printf("<Function %s at %p>", function->name, function);
	}
}

void object_thread_push_eval_stack(ObjectThread* thread, Value value) {
	if (thread->eval_stack_top - thread->eval_stack >= THREAD_EVAL_STACK_MAX) {
		FAIL("Evaluation stack overflow");
	}
    *thread->eval_stack_top = value;
    thread->eval_stack_top++;
}

void object_thread_push_frame(ObjectThread* thread, StackFrame frame) {
	if (thread->call_stack_top - thread->call_stack == THREAD_CALL_STACK_MAX) {
		FAIL("Stack overflow.");
	}

	*thread->call_stack_top = frame;
	thread->call_stack_top++;
}

StackFrame object_thread_pop_frame(ObjectThread* thread) {
	/* TODO: Stack underflow protection or at least assertion */
	thread->call_stack_top--;
	return *thread->call_stack_top;
}

StackFrame* object_thread_peek_frame(ObjectThread* thread, int offset) {
	if ((thread->call_stack_top - thread->call_stack) < offset) {
		return NULL;
	}
	return thread->call_stack_top - offset;
}

Value object_thread_pop_eval_stack(ObjectThread* thread) {
	if (thread->eval_stack_top <= thread->eval_stack) {
		FAIL("Evaluation stack underflow");
	}
    thread->eval_stack_top--;
    return *thread->eval_stack_top;
}

void object_thread_print(ObjectThread* thread) {
	printf("<Thread %s at %p wrapping ", thread->name, thread);
	object_print((Object*) thread->base_function);
	printf(">");
}

void object_print(Object* o) {
    switch (o->type) {
        case OBJECT_STRING: {
        	// TODO: Maybe differentiate string printing and value printing which happens to be a string
            printf("%s", OBJECT_AS_STRING(o)->chars);
            return;
        }
        case OBJECT_FUNCTION: {
			ObjectFunction* function = (ObjectFunction*) o;
        	print_function(function);
            return;
        }
        case OBJECT_CODE: {
        	printf("<Code object at %p>", o);
            return;
        }
		case OBJECT_THREAD: {
			ObjectThread* thread = ((ObjectThread*) o);
			object_thread_print(thread);
            return;
        }
        case OBJECT_TABLE: {
        	ObjectTable* table = (ObjectTable*) o;
        	table_print(&table->table);
            return;
        }
        case OBJECT_CELL: {
        	ObjectCell* cell = (ObjectCell*) o;
        	printf("<Cell object at %p wrapping ", cell);
        	value_print(cell->value);
        	printf(">");
        	return;
        }
        case OBJECT_MODULE: {
        	ObjectModule* module = (ObjectModule*) o;
			if (module->dll != NULL) {
				printf("<Extension module %s>", module->name->chars);
			} else {
				printf("<Module %s>", module->name->chars);
			}
			return;
        }
		case OBJECT_CLASS: {
			ObjectClass* klass = (ObjectClass*) o;
			const char* name = klass->name;
			int name_length = klass->name_length;
			printf("<Class %.*s>", name_length, name);
			return;
		}
		case OBJECT_INSTANCE: {
			ObjectInstance* instance = (ObjectInstance*) o;
			printf("<%.*s instance>", instance->klass->name_length, instance->klass->name);
			return;
		}
		case OBJECT_BOUND_METHOD: {
			ObjectBoundMethod* bound_method = (ObjectBoundMethod*) o;
			ObjectFunction* method = bound_method->method;
			printf("<Bound method %s of object ", method->name);
			object_print(bound_method->self);
			printf(">");
			return;
		}
    }
    
    FAIL("Unrecognized object type: %d", o->type);
}

/* For debugging */
void object_thread_print_diagnostic(ObjectThread* thread) {
	object_print((Object*) thread);
	printf("\n        - Current ip: %p\n        - Current function: ", thread->ip);
	print_function(thread->base_function);
}

bool object_compare(Object* a, Object* b) {
	if (a->type != b->type) {
		return false;
	}

	if (a->type == OBJECT_STRING) {
		return object_strings_equal(OBJECT_AS_STRING(a), OBJECT_AS_STRING(b));
	} else {
		return a == b; // TODO: Not good enough
	}
}

ObjectFunction* object_as_function(Object* o) {
	if (o->type != OBJECT_FUNCTION) {
		FAIL("Object is not a function. Actual type: %d", o->type);
	}
	return (ObjectFunction*) o;
}

ObjectString* object_as_string(Object* o) {
	if (o->type != OBJECT_STRING) {
		FAIL("Object is not string. Actual type: %d", o->type);
	}
	return (ObjectString*) o;
}

MethodAccessResult object_get_method(Object* object, const char* method_name, ObjectFunction** out) {
	Value method_value;
	if (!cell_table_get_value_cstring_key(&object->attributes, method_name, &method_value)) {
		return METHOD_ACCESS_NO_SUCH_ATTR;
	}

	if (!object_value_is(method_value, OBJECT_FUNCTION)) {
		return METHOD_ACCESS_ATTR_NOT_FUNCTION;
	}

	*out = (ObjectFunction*) method_value.as.object;
	return METHOD_ACCESS_SUCCESS;
}

// For debugging
void object_print_all_objects(void) {
	if (vm.objects != NULL) {
		printf("All live objects:\n");
		int counter = 0;
		for (Object* object = vm.objects; object != NULL; object = object->next) {
			printf("%-2d | Type: %d | isReachable: %d | Print: ", counter, object->type, object->is_reachable);
			object_print(object);
			printf("\n");
			counter++;
		}
	} else {
		printf("No live objects.\n");
	}
}

bool object_hash(Object* object, unsigned long* result) {
	/* Currently only possible to hash strings. Plug in implementations here as needed. */

	switch (object->type) {
		case OBJECT_STRING: {
			ObjectString* string = (ObjectString*) object;
			*result = hash_string_bounded(string->chars, string->length);
			return true;
		}
		case OBJECT_CELL: {
			return false;
		}
		case OBJECT_CODE: {
			return false;
		}
		case OBJECT_FUNCTION: {
			return false;
		}
		case OBJECT_MODULE: {
			return false;
		}
		case OBJECT_TABLE: {
			return false;
		}
		case OBJECT_THREAD: {
			return false;
		}
		case OBJECT_CLASS: {
			return false;
		}
		case OBJECT_INSTANCE: {
			return false;
		}
		case OBJECT_BOUND_METHOD: {
			return false;
		}
	}

	// TODO: Look for @hash function on the object or something similar and delegate to it.

	FAIL("Shouldn't get here.");
	return false;
}

void object_set_attribute_cstring_key(Object* object, const char* key, Value value) {
	cell_table_set_value_cstring_key(&object->attributes, key, value);
}

bool object_load_attribute(Object* object, ObjectString* name, Value* out) {
	Value attr_value;
	if (cell_table_get_value_cstring_key(&object->attributes, name->chars, &attr_value)) {
		*out = attr_value;
		return true;
	}

	if (object->type == OBJECT_INSTANCE) {
		ObjectInstance* instance = (ObjectInstance*) object;
		ObjectClass* klass = instance->klass;
		if (cell_table_get_value_cstring_key(&instance->klass->base.attributes, name->chars, &attr_value)) {
			if (object_value_is(attr_value, OBJECT_FUNCTION)) {
				ObjectFunction* method = (ObjectFunction*) attr_value.as.object;
				ObjectBoundMethod* bound_method = object_bound_method_new(method, object);
				attr_value = MAKE_VALUE_OBJECT(bound_method);
			}

			*out = attr_value;
			return true;
		}
	}

	return false;
}

bool object_load_attribute_cstring_key(Object* object, const char* name, Value* out) {
	return object_load_attribute(object, object_string_copy_from_null_terminated(name), out);
}
