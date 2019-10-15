#include <string.h>
#include <math.h>

#include "common.h"
#include "object.h"
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

bool object_is_value_object_of_type(Value value, ObjectType type) {
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

    if (!object_is_value_object_of_type(self_value, OBJECT_STRING) || !object_is_value_object_of_type(other_value, OBJECT_STRING)) {
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

    if (!object_is_value_object_of_type(self_value, OBJECT_STRING)) {
    	FAIL("String length method called on none ObjectString.");
    }

    ObjectString* self_string = OBJECT_AS_STRING(self_value.as.object);

    *result = MAKE_VALUE_NUMBER(self_string->length);
    return true;
}

static bool object_table_length(ValueArray args, Value* result) {
	Value self_value = args.values[0];

    if (!object_is_value_object_of_type(self_value, OBJECT_TABLE)) {
    	FAIL("Table length method called on none ObjectTable.");
    }

    ObjectTable* self_table = (ObjectTable*) self_value.as.object;

    // TODO: Maybe much better way to do this?

    int length = 0;
	for (int i = 0; i < self_table->table.capacity; i++) {
		Entry* entry = &self_table->table.entries[i];
		if (entry->key != NULL) {
			length++;
		}
	}

    *result = MAKE_VALUE_NUMBER(length);
    return true;
}

static bool object_string_get_key(ValueArray args, Value* result) {
	// TODO: Proper error reporting mechanisms! not just a boolean which tells the user nothing.

	Value self_value = args.values[0];
	Value other_value = args.values[1];

    if (!object_is_value_object_of_type(self_value, OBJECT_STRING)) {
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
	Value other_value = args.values[1];

    if (!object_is_value_object_of_type(self_value, OBJECT_TABLE)) {
    	FAIL("Table @get_key called on none ObjectTable.");
    }

    if (!object_is_value_object_of_type(other_value, OBJECT_STRING))  {
    	*result = MAKE_VALUE_NIL();
    	return false;
    }

    ObjectTable* self_table = (ObjectTable*) self_value.as.object;
    ObjectString* key_string = (ObjectString*) other_value.as.object;

    Value value;
    if (table_get(&self_table->table, key_string, &value)) {
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

    if (!object_is_value_object_of_type(self_value, OBJECT_TABLE)) {
    	FAIL("Table @set_key called on none ObjectTable.");
    }

    if (!object_is_value_object_of_type(key_value, OBJECT_STRING)) {
    	return false;
    }

    ObjectTable* self_table = (ObjectTable*) self_value.as.object;
    ObjectString* key_string = (ObjectString*) key_value.as.object;

    table_set(&self_table->table, key_string, value_to_set);

    return true;
}

static ObjectString* object_string_new(char* chars, int length) {
    DEBUG_OBJECTS_PRINT("Allocating string object '%s' of length %d.", chars, length);
    
    ObjectString* string = (ObjectString*) allocate_object(sizeof(ObjectString), "ObjectString", OBJECT_STRING);
    
    string->chars = chars;
    string->length = length;

	set_object_native_method((Object*) string, "@add", (char*[]){"other"}, 1, object_string_add);
	set_object_native_method((Object*) string, "@get_key", (char*[]){"other"}, 1, object_string_get_key);
	set_object_native_method((Object*) string, "length", (char*[]){}, 0, object_string_length);
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

bool object_cstrings_equal(const char* a, const char* b) {
    // Assuming NULL-terminated strings
    return strcmp(a, b) == 0;
}

bool object_strings_equal(ObjectString* a, ObjectString* b) {
    return (a->length == b->length) && (object_cstrings_equal(a->chars, b->chars));
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

void object_function_set_name(ObjectFunction* function, char* name) {
	function->name = name;
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
	return cell;
}

ObjectModule* object_module_new(ObjectString* name, ObjectFunction* function) {
	ObjectModule* module = (ObjectModule*) allocate_object(sizeof(ObjectModule), "ObjectModule", OBJECT_MODULE);
	module->name = name;
	module->function = function;
//	module->source_code = source_code;
	return module;
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
			deallocate(module, sizeof(ObjectModule), "ObjectModule");
        	break;
        }
    }
    
    vm.num_objects--;
    DEBUG_OBJECTS_PRINT("Decremented numObjects to %d", vm.numObjects);
}

void object_print(Object* o) {
    switch (o->type) {
        case OBJECT_STRING: {
        	// TODO: Maybe differentiate string printing and value printing which happens to be a string
            printf("%s", OBJECT_AS_STRING(o)->chars);
            return;
        }
        case OBJECT_FUNCTION: {
        	if (OBJECT_AS_FUNCTION(o)->is_native) {
        		printf("<Native function at %p>", o);
        	} else {
        		printf("<Function at %p>", o);
        	}
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
			printf("<Module %s>", module->name->chars);
			return;
        }
    }
    
    FAIL("Unrecognized object type: %d", o->type);
}

bool object_compare(Object* a, Object* b) {
	if (a->type != b->type) {
		return false;
	}

	if (a->type == OBJECT_STRING) {
		return object_strings_equal(OBJECT_AS_STRING(a), OBJECT_AS_STRING(b));
	} else {
		return a == b;
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

	if (!object_is_value_object_of_type(method_value, OBJECT_FUNCTION)) {
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

