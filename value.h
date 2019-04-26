#ifndef plane_value_h
#define plane_value_h

typedef enum {
    VALUE_NUMBER
} ValueType;

typedef struct {
    ValueType type;
    union {
        double number;
    } as;
} Value;

typedef struct {
    int count;
    int capacity;
    Value* values;
} ValueArray;

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);

void printValue(Value value);

#endif