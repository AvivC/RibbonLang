#ifndef plane_value_h
#define plane_value_h

typedef enum {
    VALUE_NUMBER,
    VALUE_OBJECT,
    VALUE_NIL
} ValueType;

typedef struct {
    ValueType type;
    union {
        double number;
        struct Object* object;
    } as;
} Value;

typedef struct {
    int count;
    int capacity;
    Value* values;
} ValueArray;

#define MAKE_VALUE_NUMBER(n) (Value){.type = VALUE_NUMBER, .as.number = (n)}
#define MAKE_VALUE_OBJECT(o) (Value){.type = VALUE_OBJECT, .as.object = (struct Object*)(o)}
#define MAKE_VALUE_NIL() (Value){.type = VALUE_NIL, .as.number = -1}

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);

void printValue(Value value);

#endif
