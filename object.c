#include <string.h>

#include "object.h"
#include "value.h"
#include "memory.h"

// typedef ObjectString_t ObjectString;
// typedef Object_t Object;

static ObjectString* newObjectString(char* chars, int length) {
    DEBUG_PRINT("Allocating string object '%s' of length %d.", chars, length);
    
    ObjectString* object = allocate(sizeof(ObjectString), "ObjectString");
    object->base.type = OBJECT_STRING;
    // object->base.attributes = OBJECT_STRING;
    object->chars = chars;
    object->length = length;
    return object;
}

ObjectString* copyString(const char* string, int length) {
    DEBUG_PRINT("Allocating string buffer '%.*s' of length %d.", length, string, length);
    
    char* chars = allocate(sizeof(char) * length + 1, "Destination string-copy buffer");
    memcpy(chars, string, length);
    chars[length] = '\0';
    
    return newObjectString(chars, length);
}

ObjectString* takeString(char* chars, int length) {
    // Assume chars is already null-terminated
    return newObjectString(chars, length);
}

bool stringsEqual(ObjectString* a, ObjectString* b) {
    return (a->length == b->length) && (strcmp(a->chars, b->chars) == 0);
}

// void freeObject(Object* o) {
    // deallocate(o, sizeof(Object), "Base object");
// }

void freeObject(Object* o) {
    switch (o->type) {
        case OBJECT_STRING: {
            ObjectString* string = (ObjectString*) o;
            deallocate(string->chars, string->length + 1, "Object string buffer");
            deallocate(string, sizeof(ObjectString), "ObjectString");
            return;
        }
    }
    
    fprintf(stderr, "Weird object type when freeing.\n");
}


