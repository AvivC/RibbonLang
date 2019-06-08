#include <string.h>

#include "object.h"
#include "vm.h"
#include "value.h"
#include "memory.h"

static Object* allocateObject(size_t size, const char* what, ObjectType type) {
    DEBUG_PRINT("Allocating object '%s' of length %d and type %d.", chars, length, type);
    
    Object* object = allocate(size, what);
    object->type = type;    
    object->next = vm.objects;
    object->reachable = false;
    vm.objects = object;
    initTable(&object->attributes);
    
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

bool cstringsEqual(const char* a, const char* b) {
    // Assuming NULL-terminated strings
    return strcmp(a, b) == 0;
}

bool stringsEqual(ObjectString* a, ObjectString* b) {
    return (a->length == b->length) && (cstringsEqual(a->chars, b->chars));
}

ObjectFunction* newObjectFunction(Chunk chunk) {
    DEBUG_PRINT("Creating function object.");
    ObjectFunction* objFunc = (ObjectFunction*) allocateObject(sizeof(ObjectFunction), "ObjectFunction", OBJECT_FUNCTION);
    objFunc->chunk = chunk;
    return objFunc;
}

void freeObject(Object* o) {
    switch (o->type) {
        case OBJECT_STRING: {
            ObjectString* string = (ObjectString*) o;
            deallocate(string->chars, string->length + 1, "Object string buffer");
            deallocate(string, sizeof(ObjectString), "ObjectString");
            return;
        }
        case OBJECT_FUNCTION: {
            ObjectFunction* func = (ObjectFunction*) o;
            freeChunk(&func->chunk);
            deallocate(func, sizeof(ObjectFunction), "ObjectFunction");
            // TODO: deallocate parameters and such
            return;
        }
    }
    
    fprintf(stderr, "Weird object type when freeing.\n");
}

void printObject(Object* o) {
    switch (o->type) {
        case OBJECT_STRING: {
            printf("%s", OBJECT_AS_STRING(o)->chars);
            break;
        }
        case OBJECT_FUNCTION: {
            // Currently functions hold no metadata about them, so just print the pointer
            printf("<Function at %p>", o);
            break;
        }
    }
    
}


