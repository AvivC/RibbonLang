#ifndef plane_object_h
#define plane_object_h

#include "common.h"
#include "table.h"
#include "value.h"
#include "chunk.h"

typedef enum {
    OBJECT_STRING,
    OBJECT_FUNCTION,
	OBJECT_CODE
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

typedef struct ObjectCode {
    Object base;
    Chunk chunk;
} ObjectCode;

typedef bool (*NativeFunction)(ValueArray, Value*);

typedef struct ObjectFunction {
    Object base;
    char** parameters;
    int numParams;
    bool isNative;
    Object* self;
    union {
    	NativeFunction nativeFunction;
//    	Chunk chunk;
    	ObjectCode* code;
    };
} ObjectFunction;

char* copy_cstring(const char* string, int length, const char* what);
ObjectString* copyString(const char* string, int length);
ObjectString* takeString(char* chars, int length);
ObjectString** createCopiedStringsArray(const char** strings, int num, const char* allocDescription);

ObjectFunction* newUserObjectFunction(ObjectCode* code, char** parameters, int numParams, Object* self);
ObjectFunction* newNativeObjectFunction(NativeFunction nativeFunction, char** parameters, int numParams, Object* self);

ObjectCode* object_code_new(Chunk chunk);

bool compareObjects(Object* a, Object* b);

bool cstringsEqual(const char* a, const char* b);
bool stringsEqual(ObjectString* a, ObjectString* b);

void freeObject(Object* object);
void printObject(Object* o);
void printAllObjects(void);

#define OBJECT_AS_STRING(o) (objectAsString(o))
#define OBJECT_AS_FUNCTION(o) (objectAsFunction(o))

ObjectFunction* objectAsFunction(Object* o);
ObjectString* objectAsString(Object* o);

bool is_value_object_of_type(Value value, ObjectType type);

#endif
