#ifndef plane_value_h
#define plane_value_h

#include "common.h"
#include "dynamic_array.h"

typedef enum {
    VALUE_NUMBER,
    VALUE_BOOLEAN,
    VALUE_OBJECT,
    VALUE_NIL
} ValueType;

typedef struct {
    ValueType type;
    union {
        double number;
        struct Object* object;
        bool boolean;
    } as;
} Value;

DEFINE_DYNAMIC_ARRAY(Value, ValueArray, value_array)

#define MAKE_VALUE_NUMBER(n) (Value){.type = VALUE_NUMBER, .as.number = (n)}
#define MAKE_VALUE_BOOLEAN(val) (Value){.type = VALUE_BOOLEAN, .as.boolean = (val)}
#define MAKE_VALUE_OBJECT(o) (Value){.type = VALUE_OBJECT, .as.object = (struct Object*)(o)}
#define MAKE_VALUE_NIL() (Value){.type = VALUE_NIL, .as.number = -1}

void printValue(Value value);
bool compareValues(Value a, Value b, int* output);

#endif
