#include <string.h>

#include "common.h"
#include "object.h"
#include "vm.h"
#include "value.h"
#include "memory.h"

static Object* allocateObject(size_t size, const char* what, ObjectType type) {
	DEBUG_OBJECTS("Allocating object '%s' of size %d and type %d.", what, size, type);

	// Possible that vm.numObjects > vm.maxObjects if many objects were created during the compiling stage, where GC is disallowed
    if (vm.numObjects >= vm.maxObjects) {
    	gc();
    }

    Object* object = allocate(size, what);
    object->type = type;    
    object->isReachable = false;
    object->next = vm.objects;
    vm.objects = object;
    initTable(&object->attributes);
    
    vm.numObjects++;
    DEBUG_OBJECTS("Incremented numObjects to %d", vm.numObjects);

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

static ObjectString* newObjectString(char* chars, int length) {
    DEBUG_PRINT("Allocating string object '%s' of length %d.", chars, length);
    
    ObjectString* objString = (ObjectString*) allocateObject(sizeof(ObjectString), "ObjectString", OBJECT_STRING);
    
    objString->chars = chars;
    objString->length = length;

    // TODO: Factor out all of this, it repeats in more places
    int add_func_num_params = 2;
    char** params = allocate(sizeof(char*) * add_func_num_params, "Parameters list cstrings");
    // Should "result" be listed in the internal parameter list?
    params[0] = copy_cstring("self", 4, "ObjectFunction param cstring");;
    params[1] = copy_cstring("other", 5, "ObjectFunction param cstring");;
    ObjectFunction* string_add_function = newNativeObjectFunction(object_string_add, params, add_func_num_params);
    setTableCStringKey(&objString->base.attributes, "@add", MAKE_VALUE_OBJECT(string_add_function));

    return objString;
}

char* copy_cstring(const char* string, int length, const char* what) {
	// argument length should not include the null-terminator
	DEBUG_PRINT("Allocating string buffer '%.*s' of length %d.", length, string, length);

    char* chars = allocate(sizeof(char) * length + 1, what);
    memcpy(chars, string, length);
    chars[length] = '\0';

    return chars;
}

ObjectString* copyString(const char* string, int length) {
	// argument length should not include the null-terminator
    DEBUG_PRINT("Allocating string buffer '%.*s' of length %d.", length, string, length);
    
    char* chars = copy_cstring(string, length, "Object string buffer");
    return newObjectString(chars, length);
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

static ObjectFunction* newPartialObjectFunction(bool isNative, char** parameters, int numParams) {
    ObjectFunction* objFunc = (ObjectFunction*) allocateObject(sizeof(ObjectFunction), "ObjectFunction", OBJECT_FUNCTION);
    objFunc->isNative = isNative;
    objFunc->parameters = parameters;
    objFunc->numParams = numParams;
    return objFunc;
}

ObjectFunction* newUserObjectFunction(Chunk chunk, char** parameters, int numParams) {
    DEBUG_PRINT("Creating user function object.");
    ObjectFunction* objFunc = newPartialObjectFunction(false, parameters, numParams);
    objFunc->chunk = chunk;
    return objFunc;
}

ObjectFunction* newNativeObjectFunction(NativeFunction nativeFunction, char** parameters, int numParams) {
    DEBUG_PRINT("Creating native function object.");
    ObjectFunction* objFunc = newPartialObjectFunction(true, parameters, numParams);
    objFunc->nativeFunction = nativeFunction;
    return objFunc;
}

void freeObject(Object* o) {
	freeTable(&o->attributes);

	ObjectType type = o->type;

    switch (type) {
        case OBJECT_STRING: {
            ObjectString* string = (ObjectString*) o;
            DEBUG_OBJECTS("Freeing ObjectString '%s'", string->chars);
            deallocate(string->chars, string->length + 1, "Object string buffer");
            deallocate(string, sizeof(ObjectString), "ObjectString");
            break;
        }
        case OBJECT_FUNCTION: {
            ObjectFunction* func = (ObjectFunction*) o;
            if (func->isNative) {
				DEBUG_OBJECTS("Freeing native ObjectFunction");
			} else {
				DEBUG_OBJECTS("Freeing user ObjectFunction");
				freeChunk(&func->chunk);
			}
            for (int i = 0; i < func->numParams; i++) {
            	char* param = func->parameters[i];
				deallocate(param, strlen(param) + 1, "ObjectFunction param cstring");
			}
            if (func->numParams > 0) {
            	deallocate(func->parameters, sizeof(char*) * func->numParams, "Parameters list cstrings");
            }
            deallocate(func, sizeof(ObjectFunction), "ObjectFunction");
            break;
        }
    }
    
    vm.numObjects--;
    DEBUG_OBJECTS("Decremented numObjects to %d", vm.numObjects);
}

void printObject(Object* o) {
    switch (o->type) {
        case OBJECT_STRING: {
            printf("%s", OBJECT_AS_STRING(o)->chars);
            break;
        }
        case OBJECT_FUNCTION: {
        	if (OBJECT_AS_FUNCTION(o)->isNative) {
        		printf("<Native function at %p>", o);
        	} else {
        		printf("<Function at %p>", o);
        	}
            break;
        }
    }
    
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
		FAIL("Illegal object conversion to function.");
	}
	return (ObjectFunction*) o;
}

ObjectString* objectAsString(Object* o) {
	if (o->type != OBJECT_STRING) {
		FAIL("Illegal object conversion to string.");
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

