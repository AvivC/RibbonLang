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

static ObjectString* newObjectString(char* chars, int length) {
    DEBUG_PRINT("Allocating string object '%s' of length %d.", chars, length);
    
    ObjectString* objString = (ObjectString*) allocateObject(sizeof(ObjectString), "ObjectString", OBJECT_STRING);
    
    objString->chars = chars;
    objString->length = length;
    return objString;
}

ObjectString* copyString(const char* string, int length) {
	// argument length should not include the null-terminator
    DEBUG_PRINT("Allocating string buffer '%.*s' of length %d.", length, string, length);
    
    char* chars = allocate(sizeof(char) * length + 1, "Object string buffer");
    memcpy(chars, string, length);
    chars[length] = '\0';
    
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

static ObjectFunction* newPartialObjectFunction(bool isNative, ObjectString** parameters, int numParams) {
    ObjectFunction* objFunc = (ObjectFunction*) allocateObject(sizeof(ObjectFunction), "ObjectFunction", OBJECT_FUNCTION);
    objFunc->isNative = isNative;
    objFunc->parameters = parameters;
    objFunc->numParams = numParams;
    return objFunc;
}

ObjectFunction* newUserObjectFunction(Chunk chunk, ObjectString** parameters, int numParams) {
    DEBUG_PRINT("Creating user function object.");
    ObjectFunction* objFunc = newPartialObjectFunction(false, parameters, numParams);
    objFunc->chunk = chunk;
    return objFunc;
}

ObjectFunction* newNativeObjectFunction(void (*nativeFunction)(ValueArray), ObjectString** parameters, int numParams) {
    DEBUG_PRINT("Creating native function object.");
    ObjectFunction* objFunc = newPartialObjectFunction(true, parameters, numParams);
    objFunc->nativeFunction = nativeFunction;
    return objFunc;
}

void freeObject(Object* o) {
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
            deallocate(func->parameters, sizeof(ObjectString*) * func->numParams, "Parameters list");
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



