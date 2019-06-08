#ifndef plane_object_h
#define plane_object_h

#include "common.h"
#include "table.h"
#include "chunk.h"

typedef enum {
    OBJECT_STRING,
    OBJECT_FUNCTION
} ObjectType;

typedef struct Object {
    ObjectType type;
    struct Object* next;
    Table attributes;
    bool isReachable;
} Object;

typedef struct ObjectString {
    Object base;
    char* chars;
    int length;
} ObjectString;

typedef struct ObjectFunction {
    Object base;
    Chunk chunk;
    // TODO: parameters and such
} ObjectFunction;

ObjectString* copyString(const char* string, int length);
ObjectString* takeString(char* chars, int length);

ObjectFunction* newObjectFunction(Chunk chunk);

bool cstringsEqual(const char* a, const char* b);
bool stringsEqual(ObjectString* a, ObjectString* b);

void freeObject(Object* object);
void printObject(Object* o);

#define OBJECT_AS_STRING(o) ((ObjectString*) (o))
#define OBJECT_AS_FUNCTION(o) ((ObjectFunction*) (o))

#endif
