#include <string.h>
#include <math.h>

#include "common.h"
#include "object.h"
#include "vm.h"
#include "value.h"
#include "memory.h"

static Object* allocateObject(size_t size, const char* what, ObjectType type) {
	DEBUG_OBJECTS_PRINT("Allocating object '%s' of size %d and type %d.", what, size, type);

    Object* object = allocate(size, what);
    object->type = type;    
    object->isReachable = false;
    object->next = vm.objects;
    vm.objects = object;
    initTable(&object->attributes);
    
    vm.numObjects++;
    DEBUG_OBJECTS_PRINT("Incremented numObjects to %d", vm.numObjects);

    return object;
}

bool is_value_object_of_type(Value value, ObjectType type) {
	return value.type == VALUE_OBJECT && value.as.object->type == type;
}

static bool object_string_add(ValueArray args, Value* result) {
	Value self_value = args.values[0];
	Value other_value = args.values[1];

    if (!is_value_object_of_type(self_value, OBJECT_STRING) || !is_value_object_of_type(other_value, OBJECT_STRING)) {
    	*result = MAKE_VALUE_NIL();
    	return false;
    }

    ObjectString* self_string = OBJECT_AS_STRING(self_value.as.object);
    ObjectString* other_string = OBJECT_AS_STRING(other_value.as.object);

    char* buffer = allocate(self_string->length + other_string->length + 1, "Object string buffer");
    memcpy(buffer, self_string->chars, self_string->length);
    memcpy(buffer + self_string->length, other_string->chars, other_string->length);
    int stringLength = self_string->length + other_string->length;
    buffer[stringLength] = '\0';
    ObjectString* objString = takeString(buffer, stringLength);

    *result = MAKE_VALUE_OBJECT(objString);
    return true;
}

static bool object_string_length(ValueArray args, Value* result) {
	Value self_value = args.values[0];

    if (!is_value_object_of_type(self_value, OBJECT_STRING)) {
    	FAIL("String length method called on none ObjectString.");
    }

    ObjectString* self_string = OBJECT_AS_STRING(self_value.as.object);

    *result = MAKE_VALUE_NUMBER(self_string->length);
    return true;
}

static bool object_string_get_key(ValueArray args, Value* result) {
	// TODO: Proper error reporting mechanisms! not just a boolean which tells the user nothing.

	Value self_value = args.values[0];
	Value other_value = args.values[1];

    if (!is_value_object_of_type(self_value, OBJECT_STRING)) {
    	FAIL("String @get_key called on none ObjectString.");
    }

    if (other_value.type != VALUE_NUMBER) {
    	*result = MAKE_VALUE_NIL();
    	return false;
    }

    ObjectString* self_string = objectAsString(self_value.as.object);

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
    *result = MAKE_VALUE_OBJECT(copyString(&char_result, 1));
    return true;
}

void set_string_add_method(ObjectString* objString) {
	int add_func_num_params = 2;
	char** params = allocate(sizeof(char*) * add_func_num_params, "Parameters list cstrings");
	// TODO: Should "result" be listed in the internal parameter list?
	params[0] = copy_cstring("self", 4, "ObjectFunction param cstring");
	params[1] = copy_cstring("other", 5, "ObjectFunction param cstring");
	ObjectFunction* string_add_function = object_native_function_new(object_string_add, params, add_func_num_params, (Object*) objString);
	setTableCStringKey(&objString->base.attributes, "@add", MAKE_VALUE_OBJECT(string_add_function));
}

void set_string_get_key_method(ObjectString* objString) {
	int num_params = 2;
	char** params = allocate(sizeof(char*) * num_params, "Parameters list cstrings");
	params[0] = copy_cstring("self", 4, "ObjectFunction param cstring");
	params[1] = copy_cstring("other", 5, "ObjectFunction param cstring");
	ObjectFunction* string_add_function = object_native_function_new(object_string_get_key, params, num_params, (Object*) objString);
	setTableCStringKey(&objString->base.attributes, "@get_key", MAKE_VALUE_OBJECT(string_add_function));
}

void set_string_length_method(ObjectString* objString) {
	int num_params = 1;
	char** params = allocate(sizeof(char*) * num_params, "Parameters list cstrings");
	params[0] = copy_cstring("self", 4, "ObjectFunction param cstring");
	ObjectFunction* string_length_function = object_native_function_new(object_string_length, params, num_params, (Object*) objString);
	setTableCStringKey(&objString->base.attributes, "length", MAKE_VALUE_OBJECT(string_length_function));
}

static ObjectString* newObjectString(char* chars, int length) {
    DEBUG_OBJECTS_PRINT("Allocating string object '%s' of length %d.", chars, length);
    
    ObjectString* objString = (ObjectString*) allocateObject(sizeof(ObjectString), "ObjectString", OBJECT_STRING);
    
    objString->chars = chars;
    objString->length = length;

	set_string_add_method(objString);
	set_string_get_key_method(objString);
	set_string_length_method(objString);
    return objString;
}

char* copy_cstring(const char* string, int length, const char* what) {
	// argument length should not include the null-terminator
	DEBUG_OBJECTS_PRINT("Allocating string buffer '%.*s' of length %d.", length, string, length);

    char* chars = allocate(sizeof(char) * length + 1, what);
    memcpy(chars, string, length);
    chars[length] = '\0';

    return chars;
}

char* copy_null_terminated_cstring(const char* string, const char* what) {
	return copy_cstring(string, strlen(string), what);
}

ObjectString* copyString(const char* string, int length) {
	// argument length should not include the null-terminator
    char* chars = copy_cstring(string, length, "Object string buffer");
    return newObjectString(chars, length);
}

ObjectString* object_string_copy_from_null_terminated(const char* string) {
	return copyString(string, strlen(string));
}

ObjectString* takeString(char* chars, int length) {
    // Assume chars is already null-terminated
    return newObjectString(chars, length);
}

ObjectString** createCopiedStringsArray(const char** strings, int num, const char* allocDescription) {
	ObjectString** array = allocate(sizeof(ObjectString*) * num, allocDescription);
	for (int i = 0; i < num; i++) {
		array[i] = copyString(strings[i], strlen(strings[i]));
	}
	return array;
}

bool cstringsEqual(const char* a, const char* b) {
    // Assuming NULL-terminated strings
    return strcmp(a, b) == 0;
}

bool stringsEqual(ObjectString* a, ObjectString* b) {
    return (a->length == b->length) && (cstringsEqual(a->chars, b->chars));
}

static ObjectFunction* object_function_base_new(bool isNative, char** parameters, int numParams, Object* self) {
    ObjectFunction* objFunc = (ObjectFunction*) allocateObject(sizeof(ObjectFunction), "ObjectFunction", OBJECT_FUNCTION);
    objFunc->name = copy_null_terminated_cstring("<Anonymous function>", "Function name");
    objFunc->isNative = isNative;
    objFunc->parameters = parameters;
    objFunc->numParams = numParams;
    objFunc->self = self;
    return objFunc;
}

ObjectFunction* object_user_function_new(ObjectCode* code, char** parameters, int numParams, Object* self) {
    DEBUG_OBJECTS_PRINT("Creating user function object.");
    ObjectFunction* objFunc = object_function_base_new(false, parameters, numParams, self);
    objFunc->code = code;
    return objFunc;
}

ObjectFunction* object_native_function_new(NativeFunction nativeFunction, char** parameters, int numParams, Object* self) {
    DEBUG_OBJECTS_PRINT("Creating native function object.");
    ObjectFunction* objFunc = object_function_base_new(true, parameters, numParams, self);
    objFunc->nativeFunction = nativeFunction;
    return objFunc;
}

void object_function_set_name(ObjectFunction* function, char* name) {
	function->name = name;
}

ObjectCode* object_code_new(Chunk chunk) {
	ObjectCode* obj_code = (ObjectCode*) allocateObject(sizeof(ObjectCode), "ObjectCode", OBJECT_CODE);
	obj_code->chunk = chunk;
	return obj_code;
}

ObjectTable* object_table_new(void) {
	ObjectTable* obj_table = (ObjectTable*) allocateObject(sizeof(ObjectTable), "ObjectTable", OBJECT_TABLE);
	return obj_table;
}

void freeObject(Object* o) {
	freeTable(&o->attributes);

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
            if (func->isNative) {
            	DEBUG_OBJECTS_PRINT("Freeing native ObjectFunction");
			} else {
				DEBUG_OBJECTS_PRINT("Freeing user ObjectFunction");
			}
            for (int i = 0; i < func->numParams; i++) {
            	char* param = func->parameters[i];
				deallocate(param, strlen(param) + 1, "ObjectFunction param cstring");
			}
            if (func->numParams > 0) {
            	deallocate(func->parameters, sizeof(char*) * func->numParams, "Parameters list cstrings");
            }
            deallocate(func->name, strlen(func->name) + 1, "Function name");
            deallocate(func, sizeof(ObjectFunction), "ObjectFunction");
            break;
        }
        case OBJECT_CODE: {
        	ObjectCode* code = (ObjectCode*) o;
        	DEBUG_OBJECTS_PRINT("Freeing ObjectCode at '%p'", code);
        	freeChunk(&code->chunk);
        	deallocate(code, sizeof(ObjectCode), "ObjectCode");
        	break;
        }
    }
    
    vm.numObjects--;
    DEBUG_OBJECTS_PRINT("Decremented numObjects to %d", vm.numObjects);
}

void printObject(Object* o) {
    switch (o->type) {
        case OBJECT_STRING: {
        	// TODO: Maybe differentiate string printing and value printing which happens to be a string
            printf("%s", OBJECT_AS_STRING(o)->chars);
            return;
        }
        case OBJECT_FUNCTION: {
        	if (OBJECT_AS_FUNCTION(o)->isNative) {
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
    }
    
    FAIL("Unrecognized object type: %d", o->type);
}

bool compareObjects(Object* a, Object* b) {
	if (a->type != b->type) {
		return false;
	}

	if (a->type == OBJECT_STRING) {
		return stringsEqual(OBJECT_AS_STRING(a), OBJECT_AS_STRING(b));
	} else {
		return a == b;
	}
}

ObjectFunction* objectAsFunction(Object* o) {
	if (o->type != OBJECT_FUNCTION) {
		FAIL("Object is not a function. Actual type: %d", o->type);
	}
	return (ObjectFunction*) o;
}

ObjectString* objectAsString(Object* o) {
	if (o->type != OBJECT_STRING) {
		FAIL("Object is not string. Actual type: %d", o->type);
	}
	return (ObjectString*) o;
}

// For debugging
void printAllObjects(void) {
	if (vm.objects != NULL) {
		printf("All live objects:\n");
		int counter = 0;
		for (Object* object = vm.objects; object != NULL; object = object->next) {
			printf("%-2d | Type: %d | isReachable: %d | Print: ", counter, object->type, object->isReachable);
			printObject(object);
			printf("\n");
			counter++;
		}
	} else {
		printf("No live objects.\n");
	}
}

