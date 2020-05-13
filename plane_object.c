#include <string.h>
#include <math.h>

#include "common.h"
#include "plane_utils.h"
#include "plane_object.h"
#include "vm.h"
#include "value.h"
#include "memory.h"
#include "table.h"

static ObjectClass* descriptor_class = NULL;

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

static void set_object_native_method(Object* object, char* method_name, char** params, int num_params, NativeFunction function) {
	// char** copied_params = allocate(sizeof(char*) * num_params, "Parameters list cstrings");

	// for (int i = 0; i < num_params; i++) {
	// 	copied_params[i] = copy_null_terminated_cstring(params[i], "ObjectFunction param cstring");
	// }

	// ObjectFunction* method = object_native_function_new(function, copied_params, num_params);
	ObjectFunction* method = make_native_function_with_params(method_name, num_params, params, function);
	ObjectBoundMethod* bound_method = object_bound_method_new(method, object);
	object_set_attribute_cstring_key(object, method_name, MAKE_VALUE_OBJECT(bound_method));
}

static bool object_string_add(Object* self, ValueArray args, Value* result) {
	Value other_value = args.values[0];

	if (!object_value_is(other_value, OBJECT_STRING)) {
    	*result = MAKE_VALUE_NIL();
    	return false;
    }

	assert(self->type == OBJECT_STRING);

    ObjectString* self_string = OBJECT_AS_STRING(self);
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

static bool object_string_length(Object* self, ValueArray args, Value* result) {
	assert(args.count == 0);
	assert(self->type == OBJECT_STRING);

    ObjectString* self_string = OBJECT_AS_STRING(self);

    *result = MAKE_VALUE_NUMBER(self_string->length);
    return true;
}

static bool object_table_length(Object* self, ValueArray args, Value* result) {
	assert(args.count == 0);
	assert(self->type == OBJECT_TABLE);

    ObjectTable* self_table = (ObjectTable*) self;

	PointerArray entries = table_iterate(&self_table->table, "table object length table_iterate buffer");
    *result = MAKE_VALUE_NUMBER(entries.count);
	pointer_array_free(&entries);

    return true;
}

static bool object_table_add(Object* self, ValueArray args, Value* result) {
	assert(args.count == 1);
	assert(self->type == OBJECT_TABLE);

	bool success = true;

	ValueArray length_args = value_array_make(0, NULL);
	Value length;
	if (vm_call_attribute_cstring(self, "length", length_args, &length) != CALL_RESULT_SUCCESS) {
		success = false;
		goto cleanup;
	}

	ValueArray set_key_args = value_array_make(2, (Value[]) {length, args.values[0]});
	Value throwaway;
	if (vm_call_attribute_cstring(self, "@set_key", set_key_args, &throwaway) != CALL_RESULT_SUCCESS) {
		success = false;
	}

	value_array_free(&set_key_args);

	cleanup:
	value_array_free(&length_args);
	return success;
}

static bool object_table_pop(Object* self, ValueArray args, Value* result) {
	assert(args.count == 0);
	assert(self->type == OBJECT_TABLE);

	bool success = true;

	ValueArray length_args = value_array_make(0, NULL);
	Value length;
	if (vm_call_attribute_cstring(self, "length", length_args, &length) != CALL_RESULT_SUCCESS) {
		success = false;
		goto cleanup;
	}

	ValueArray delete_args = value_array_make(1, (Value[]) {MAKE_VALUE_NUMBER(length.as.number - 1)});
	Value throwaway;
	if (vm_call_attribute_cstring(self, "remove_key", delete_args, &throwaway) != CALL_RESULT_SUCCESS) {
		success = false;
	}

	value_array_free(&delete_args);

	cleanup:
	value_array_free(&length_args);
	return success;
}

static bool object_table_remove_key(Object* self, ValueArray args, Value* result) {
	assert(args.count == 1);
	assert(self->type == OBJECT_TABLE);

	ObjectTable* table = (ObjectTable*) self;
	Value key = args.values[0];

	*result = MAKE_VALUE_NIL();
	return table_delete(&table->table, key);
}

static bool object_string_get_key(Object* self, ValueArray args, Value* result) {
	assert(self->type == OBJECT_STRING);

	Value other_value = args.values[0];

    if (other_value.type != VALUE_NUMBER) {
    	*result = MAKE_VALUE_NIL();
    	return false;
    }

    ObjectString* self_string = object_as_string(self);

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

static bool object_table_get_key(Object* self, ValueArray args, Value* result) {
	assert(self->type == OBJECT_TABLE);

	Value key = args.values[0];

    ObjectTable* self_table = (ObjectTable*) self;

    Value value;
    if (table_get(&self_table->table, key, &value)) {
    	*result = value;
    } else {
		*result = MAKE_VALUE_NIL();
    	return false;
    }

    return true;
}

static bool object_table_has_key(Object* self, ValueArray args, Value* result) {
	assert(self->type == OBJECT_TABLE);

	Value key = args.values[0];

    ObjectTable* self_table = (ObjectTable*) self;

    Value throwaway;
    *result = MAKE_VALUE_BOOLEAN(table_get(&self_table->table, key, &throwaway));
    return true;
}

static bool object_table_set_key(Object* self, ValueArray args, Value* result) {
	assert(args.count == 2);
	assert(self->type == OBJECT_TABLE);

	Value key_value = args.values[0];
	Value value_to_set = args.values[1];

    ObjectTable* self_table = (ObjectTable*) self;

    table_set(&self_table->table, key_value, value_to_set);

	*result = MAKE_VALUE_NIL();
    return true;
}

static ObjectString* get_string_from_cache(const char* string, int length) {
	Value cached_string;
	unsigned long hash = hash_string_bounded(string, length);
	if (table_get(&vm.string_cache, MAKE_VALUE_RAW_STRING(string, length, hash), &cached_string)) {
		assert(object_value_is(cached_string, OBJECT_STRING));
		return (ObjectString*) cached_string.as.object;
	}
	return NULL;
}

static ObjectString* new_bare_string(char* chars, int length) {
    ObjectString* string = (ObjectString*) allocate_object(sizeof(ObjectString), "ObjectString", OBJECT_STRING);

	assert(strlen(chars) == length);

    string->chars = chars;
	string->length = length;
	string->hash = hash_string_bounded(chars, length);

	return string;
}

static ObjectString* object_string_new_partial(char* chars, int length) {
	ObjectString* cached = get_string_from_cache(chars, length);
	if (cached != NULL) {
		return cached;
	}

	ObjectString* string = new_bare_string(copy_null_terminated_cstring(chars, "Object string buffer"), length);

	table_set(&vm.string_cache, MAKE_VALUE_RAW_STRING(string->chars, string->length, string->hash), MAKE_VALUE_OBJECT(string));

	return string;
}

ObjectString* object_string_new_partial_from_null_terminated(char* chars) {
	return object_string_new_partial(chars, strlen(chars));
}

static ObjectString* object_string_new(char* chars, int length) {
    ObjectString* string = new_bare_string(chars, length);

	ObjectFunction* string_add_method = make_native_function_with_params("@add", 1, (char*[]) {"other"}, object_string_add);
	ObjectBoundMethod* string_add_bound_method = object_bound_method_new(string_add_method, (Object*) string);

	ObjectFunction* string_get_key_method = make_native_function_with_params("@get_key", 1, (char*[]) {"other"}, object_string_get_key);
	ObjectBoundMethod* string_get_key_bound_method = object_bound_method_new(string_get_key_method, (Object*) string);

	ObjectFunction* string_length_method = make_native_function_with_params("length", 0, NULL, object_string_length);
	ObjectBoundMethod* string_length_bound_method = object_bound_method_new(string_length_method, (Object*) string);

	/* TODO: Consider if setting the objects attributes should go through the object_set_attribute function */

	ObjectString* add_attr_key = object_string_new_partial("@add", strlen("@add"));
	cell_table_set_value(&string->base.attributes, add_attr_key, MAKE_VALUE_OBJECT(string_add_bound_method));

	ObjectString* get_key_attr_key = object_string_new_partial("@get_key", strlen("@get_key"));
	cell_table_set_value(&string->base.attributes, get_key_attr_key, MAKE_VALUE_OBJECT(string_get_key_bound_method));

	ObjectString* length_attr_key = object_string_new_partial("length", strlen("length"));
	cell_table_set_value(&string->base.attributes, length_attr_key, MAKE_VALUE_OBJECT(string_length_bound_method));

	table_set(&vm.string_cache, MAKE_VALUE_RAW_STRING(string->chars, string->length, string->hash), MAKE_VALUE_OBJECT(string));

    return string;
}

ObjectString* object_string_copy(const char* string, int length) {
	ObjectString* cached = get_string_from_cache(string, length);
	if (cached != NULL) {
		return cached;
	}

	// argument length should not include the null-terminator
    char* chars = copy_cstring(string, length, "Object string buffer");
    return object_string_new(chars, length);
}

ObjectString* object_string_copy_from_null_terminated(const char* string) {
	return object_string_copy(string, strlen(string));
}

ObjectString* object_string_take(char* chars, int length) {
	ObjectString* cached = get_string_from_cache(chars, length);
	if (cached != NULL) {
		deallocate(chars, length + 1, "Object string buffer");
		return cached;
	}

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

// static ObjectFunction* object_function_base_new(bool isNative, char** parameters, int numParams, CellTable free_vars) {
static ObjectFunction* object_function_base_new(bool isNative, ObjectString** parameters, int numParams, CellTable free_vars) {
    ObjectFunction* objFunc = (ObjectFunction*) allocate_object(sizeof(ObjectFunction), "ObjectFunction", OBJECT_FUNCTION);
    objFunc->name = copy_null_terminated_cstring("<Anonymous function>", "Function name");
    objFunc->is_native = isNative;
    objFunc->parameters = parameters;
    objFunc->num_params = numParams;
    objFunc->free_vars = free_vars;
    return objFunc;
}

// ObjectFunction* object_user_function_new(ObjectCode* code, char** parameters, int numParams, CellTable free_vars) {
ObjectFunction* object_user_function_new(ObjectCode* code, ObjectString** parameters, int numParams, CellTable free_vars) {
    DEBUG_OBJECTS_PRINT("Creating user function object.");
    ObjectFunction* objFunc = object_function_base_new(false, parameters, numParams, free_vars);
    objFunc->code = code;
    return objFunc;
}

// ObjectFunction* object_native_function_new(NativeFunction nativeFunction, char** parameters, int numParams) {
ObjectFunction* object_native_function_new(NativeFunction nativeFunction, ObjectString** parameters, int numParams) {
    DEBUG_OBJECTS_PRINT("Creating native function object.");
    ObjectFunction* objFunc = object_function_base_new(true, parameters, numParams, cell_table_new_empty());
    objFunc->native_function = nativeFunction;
    return objFunc;
}

ObjectFunction* make_native_function_with_params(char* name, int num_params, char** params, NativeFunction function) {
	/* name must be null terminated. It is copied and ObjectFunction takes ownership over the copy.
	Can simply be a literal. Otherwise caller must free it later. */

	// char** params_buffer = allocate(sizeof(char*) * num_params, "Parameters list cstrings");
	ObjectString** params_buffer = allocate(sizeof(ObjectString*) * num_params, "Parameters list strings");
	for (int i = 0; i < num_params; i++) {
		// params_buffer[i] = copy_cstring(params[i], strlen(params[i]), "ObjectFunction param cstring");
		// params_buffer[i] = object_string_copy_from_null_terminated(params[i]);
		params_buffer[i] = object_string_new_partial_from_null_terminated(params[i]);
	}
	ObjectFunction* func = object_native_function_new(function, params_buffer, num_params);
	name = copy_null_terminated_cstring(name, "Function name");
	object_function_set_name(func, name);
	return func;
}

void object_function_set_name(ObjectFunction* function, char* name) {
	assert(function->name != NULL);

	// if (function->name == NULL) {
	// 	FAIL("function->name == NULL in object_function_set_name");
	// }

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

static bool descriptor_init(Object* self, ValueArray args, Value* out) {
	/* TODO: One convenient argument-types-validator thing, to be used also here. */

	assert(is_instance_of_class(self, "Descriptor"));

	Value get_arg = args.values[0];
	Value set_arg = args.values[1];

	/* Assertions are fine as long as this is internal.
	   TODO to figure out better mechanism if later this is exposed to user code. */

	if (get_arg.type != VALUE_NIL) {
		assert(object_value_is(get_arg, OBJECT_FUNCTION));
		// if (!object_value_is(get_arg, OBJECT_FUNCTION)) {
		// 	FAIL("get argument passed to descriptor @init is not a function.");
		// }
		object_set_attribute_cstring_key((Object*) self, "@get", get_arg);
	}

	if (set_arg.type != VALUE_NIL) {
		assert(object_value_is(set_arg, OBJECT_FUNCTION));
		object_set_attribute_cstring_key((Object*) self, "@set", set_arg);
	}

	*out = MAKE_VALUE_NIL();
	return true;
}

ObjectInstance* object_descriptor_new(ObjectFunction* get, ObjectFunction* set) {
	if (descriptor_class == NULL) {
		ObjectFunction* init_func = object_make_constructor(2, (char*[]) {"get", "set"}, descriptor_init);
		descriptor_class = object_class_native_new("Descriptor", sizeof(ObjectInstance), NULL, NULL, init_func, NULL);
	}

	Value get_val = get == NULL ? MAKE_VALUE_NIL() : MAKE_VALUE_OBJECT(get);
	Value set_val = set == NULL ? MAKE_VALUE_NIL() : MAKE_VALUE_OBJECT(set);

	Value descriptor_val;
	ValueArray args = value_array_make(2, (Value[]) {get_val, set_val});
	if (vm_instantiate_class(descriptor_class, args, &descriptor_val) != CALL_RESULT_SUCCESS) {
		return NULL;
	}
	value_array_free(&args);

	assert(is_value_instance_of_class(descriptor_val, "Descriptor"));
	// if (!is_value_instance_of_class(descriptor_val, "Descriptor")) {
	// 	FAIL("Descriptor instantiated isn't an instance of the Descriptor class.");
	// }

	return (ObjectInstance*) descriptor_val.as.object;
}

ObjectInstance* object_descriptor_new_native(NativeFunction get, NativeFunction set) {
	ObjectFunction* get_object = make_native_function_with_params("@get", 2, (char*[]) {"object", "attribute"}, get);
	ObjectFunction* set_object = make_native_function_with_params("@set", 3, (char*[]) {"object", "attribute", "value"}, set);
	return object_descriptor_new(get_object, set_object);
}

ObjectCode* object_code_new(Bytecode chunk) {
	ObjectCode* obj_code = (ObjectCode*) allocate_object(sizeof(ObjectCode), "ObjectCode", OBJECT_CODE);
	obj_code->bytecode = chunk;
	return obj_code;
}

ObjectTable* object_table_new(Table table) {
	ObjectTable* object_table = (ObjectTable*) allocate_object(sizeof(ObjectTable), "ObjectTable", OBJECT_TABLE);
	object_table->table = table;

	set_object_native_method((Object*) object_table, "length", (char*[]){}, 0, object_table_length);
	set_object_native_method((Object*) object_table, "@get_key", (char*[]){"other"}, 1, object_table_get_key);
	set_object_native_method((Object*) object_table, "@set_key", (char*[]){"key", "value"}, 2, object_table_set_key);
	set_object_native_method((Object*) object_table, "has_key", (char*[]){"key"}, 1, object_table_has_key);
	set_object_native_method((Object*) object_table, "remove_key", (char*[]){"key"}, 1, object_table_remove_key);
	set_object_native_method((Object*) object_table, "add", (char*[]){"value"}, 1, object_table_add);
	set_object_native_method((Object*) object_table, "pop", NULL, 0, object_table_pop);

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

ObjectClass* object_class_native_new(
		char* name, size_t instance_size, DeallocationFunction dealloc_func, 
		GcMarkFunction gc_mark_func, ObjectFunction* constructor, void* descriptors[][2]) {
	assert(instance_size > 0);
	// if (instance_size <= 0) {
	// 	FAIL("instance_size <= passed for native class.");
	// }

	ObjectClass* klass = object_class_new_base(NULL, name, instance_size, dealloc_func, gc_mark_func);

	if (constructor != NULL) {
		object_set_attribute_cstring_key((Object*) klass, "@init", MAKE_VALUE_OBJECT(constructor));
	}

	if (descriptors != NULL) {
		for (int i = 0; descriptors[i][0] != NULL; i++) {
			void** descriptor = descriptors[i];
			char* attribute = (char*) descriptor[0];
			ObjectInstance* instance = descriptor[1];
			object_set_attribute_cstring_key((Object*) klass, attribute, MAKE_VALUE_OBJECT(instance));
		}
	}

	return klass;
}

ObjectInstance* object_instance_new(ObjectClass* klass) {
	ObjectInstance* instance = NULL;

	if (klass->instance_size > 0) { // Native class
		instance = (ObjectInstance*) allocate_object(klass->instance_size, "ObjectInstance", OBJECT_INSTANCE);
	} else { // User class
		instance = (ObjectInstance*) allocate_object(sizeof(ObjectInstance), "ObjectInstance", OBJECT_INSTANCE);
	}
	
	instance->klass = klass;
	instance->is_initialized = false;

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

void object_free(Object* o) {
	cell_table_free(&o->attributes);

	ObjectType type = o->type;

    switch (type) {
        case OBJECT_STRING: {
            ObjectString* string = (ObjectString*) o;
            DEBUG_OBJECTS_PRINT("Freeing ObjectString '%s'", string->chars);
			table_delete(&vm.string_cache, MAKE_VALUE_RAW_STRING(string->chars, string->length, string->hash));
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
            // for (int i = 0; i < func->num_params; i++) {
            // 	char* param = func->parameters[i];
			// 	deallocate(param, strlen(param) + 1, "ObjectFunction param cstring");
			// }
            if (func->num_params > 0) {
            	deallocate(func->parameters, sizeof(ObjectString*) * func->num_params, "Parameters list strings");
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
				if (instance->is_initialized && klass->dealloc_func != NULL) {
					klass->dealloc_func(instance);
				}
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
			printf("<%.*s instance at %p>", instance->klass->name_length, instance->klass->name, instance);
			return;
		}
		case OBJECT_BOUND_METHOD: {
			ObjectBoundMethod* bound_method = (ObjectBoundMethod*) o;
			ObjectFunction* method = bound_method->method;
			printf("<Bound method %s of object ", method->name);
			object_print(bound_method->self);
			printf(" at %p>", bound_method);
			return;
		}
    }
    
    FAIL("Unrecognized object type: %d", o->type);
}

bool object_compare(Object* a, Object* b) {
	return a == b; /* Strings are interned */
}

ObjectFunction* object_as_function(Object* o) {
	assert(o->type == OBJECT_FUNCTION);
	return (ObjectFunction*) o;
}

ObjectString* object_as_string(Object* o) {
	assert(o->type == OBJECT_STRING);
	return (ObjectString*) o;
}

MethodAccessResult object_get_method(Object* object, const char* method_name, ObjectBoundMethod** out) {
	Value attr_val;
	if (!object_load_attribute_cstring_key(object, method_name, &attr_val)) {
		return METHOD_ACCESS_NO_SUCH_ATTR;
	}

	if (!object_value_is(attr_val, OBJECT_BOUND_METHOD)) {
		return METHOD_ACCESS_ATTR_NOT_BOUND_METHOD;
	}

	*out = (ObjectBoundMethod*) attr_val.as.object;
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
			*result = string->hash;
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

/* This function should generally only be called by the attribute accessor external functions of this module.
   It should almost never be called directly. It's only external here for use in the builtin_test module,
   to test internals of the system. */
bool load_attribute_bypass_descriptors(Object* object, ObjectString* name, Value* out) {
	if (cell_table_get_value(&object->attributes, name, out)) {
		return true;
	}

	if (object->type == OBJECT_INSTANCE) {
		ObjectInstance* instance = (ObjectInstance*) object;
		ObjectClass* klass = instance->klass;

		if (cell_table_get_value(&instance->klass->base.attributes, name, out)) {
			if (object_value_is(*out, OBJECT_FUNCTION)) {
				ObjectFunction* method = (ObjectFunction*) out->as.object;
				ObjectBoundMethod* bound_method = object_bound_method_new(method, object);
				*out = MAKE_VALUE_OBJECT(bound_method);
			}

			return true;
		}
	}

	return false;
}

void object_set_attribute_cstring_key(Object* object, const char* key, Value value) {
	Value descriptor_value;
	if (load_attribute_bypass_descriptors(object, object_string_copy_from_null_terminated(key), &descriptor_value)) {
		if (is_value_instance_of_class(descriptor_value, "Descriptor")) {
			ObjectInstance* descriptor = (ObjectInstance*) descriptor_value.as.object;
			
			Value set_method_val;
			if (!object_load_attribute_cstring_key((Object*) descriptor, "@set", &set_method_val)) {
				FAIL("Descriptor found with no @set method."); /* Descriptors without one of @get or @set currently unsupported */
			}
			assert(object_value_is(set_method_val, OBJECT_FUNCTION) || object_value_is(set_method_val, OBJECT_BOUND_METHOD));

			ValueArray descriptor_args = value_array_make(3, (Value[]) {
								MAKE_VALUE_OBJECT(object),
								MAKE_VALUE_OBJECT(object_string_copy_from_null_terminated(key)),
								value});
			
			Value descriptor_return_val;
			if (vm_call_object(set_method_val.as.object, descriptor_args, &descriptor_return_val) != CALL_RESULT_SUCCESS) {
				/* Currently an assertion might be fine, because right now descriptors aren't exposed to user code, it's an internal thing that should work.
				   Except for extensions. If you're an extension developer, and your descriptor fails, yes right now it will crash everything.
				   Maybe fix later if and when we make a better error handling system */

				FAIL("Calling descriptor failed.");
			}

			assert(descriptor_return_val.type == VALUE_NIL);

			value_array_free(&descriptor_args);

			return;
		}
	}

	cell_table_set_value_cstring_key(&object->attributes, key, value);
}

static Value function_value_to_bound_method(Value func_value, Object* self) {
	assert(object_value_is(func_value, OBJECT_FUNCTION));
	// if (!object_value_is(func_value, OBJECT_FUNCTION)) {
	// 	FAIL("Function-to-BoundMethod called with non function.");
	// }

	ObjectFunction* function = (ObjectFunction*) func_value.as.object;
	ObjectBoundMethod* bound_method = object_bound_method_new(function, self);
	return MAKE_VALUE_OBJECT(bound_method);
}

bool object_load_attribute(Object* object, ObjectString* name, Value* out) {
	if (!load_attribute_bypass_descriptors(object, name, out)) {
		return false;
	}

	if (is_value_instance_of_class(*out, "Descriptor")) {
		assert(object_value_is(*out, OBJECT_INSTANCE));

		ObjectInstance* descriptor = (ObjectInstance*) out->as.object;
		Value get_method_val;
		if (!object_load_attribute_cstring_key((Object*) descriptor, "@get", &get_method_val)) {
			FAIL("Found a descriptor without a @get method - currently shouldn't be possible.");
		}

		assert(object_value_is(get_method_val, OBJECT_FUNCTION) || object_value_is(get_method_val, OBJECT_BOUND_METHOD));

		ValueArray get_args = value_array_make(2, (Value[]) {MAKE_VALUE_OBJECT(object), MAKE_VALUE_OBJECT(name)});
		if (vm_call_object(get_method_val.as.object, get_args, out) != CALL_RESULT_SUCCESS) {
			/* Currently failing for this. Later possibly find a better solution. Possibly not,
			   if we don't expose descriptors as a user feature */
			FAIL("Descriptor @get failed.");
		}
		value_array_free(&get_args);
	}

	return true;
}

bool object_load_attribute_cstring_key(Object* object, const char* name, Value* out) {
	return object_load_attribute(object, object_string_copy_from_null_terminated(name), out);
}

/* TODO: Rename is_instance functions to object_* namespace */

bool is_instance_of_class(Object* object, char* klass_name) {
    if (object->type != OBJECT_INSTANCE) {
        return false;
    }

    ObjectInstance* instance = (ObjectInstance*) object;

    ObjectClass* klass = instance->klass;
    return (strlen(klass_name) == klass->name_length) && (strncmp(klass_name, klass->name, klass->name_length) == 0);;
}

bool is_value_instance_of_class(Value value, char* klass_name) {
    if (!object_value_is(value, OBJECT_INSTANCE)) {
        return false;
    }

    return is_instance_of_class(value.as.object, klass_name);
}

ObjectFunction* object_make_constructor(int num_params, char** params, NativeFunction function) {
    return make_native_function_with_params("@init", num_params, params, function);
}

char* object_get_callable_name(Object* object) {
	switch (object->type) {
		case OBJECT_FUNCTION: {
			return ((ObjectFunction*) object)->name;
		}
		case OBJECT_BOUND_METHOD: {
			return ((ObjectBoundMethod*) object)->method->name;
		}
		case OBJECT_CLASS: {
			return ((ObjectClass*) object)->name;
		}
		default: {
			FAIL("get_callable_name called on a none callable, should never happen.");
		}
	}

	return NULL;
}