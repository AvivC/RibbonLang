#ifndef plane_object_h
#define plane_object_h

#include "common.h"
#include "table.h"

typedef enum {
    OBJECT_STRING
} ObjectType;

typedef struct Object {
    ObjectType type;
    Table attributes;
} Object;

typedef struct ObjectString {
    Object base;
    char* chars;
    int length;
} ObjectString;

ObjectString* copyString(const char* string, int length);
ObjectString* takeString(char* chars, int length);

bool stringsEqual(ObjectString* a, ObjectString* b);
bool cstringsEqual(const char* a, const char* b);

void freeObject(Object* object);

#define OBJECT_AS_STRING(o) ((ObjectString*) (o))

#endif
