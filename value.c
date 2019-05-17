#include <stdio.h>
#include "value.h"
#include "object.h"
#include "memory.h"

void initValueArray(ValueArray* array) {
    array->values = NULL;
    array->count = 0;
    array->capacity = 0;
}

void writeValueArray(ValueArray* array, Value value) {
    if (array->count == array->capacity) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        // Almost sure that conversion from void* to Value* and vice-versa is automatic
        array->values = reallocate(array->values, sizeof(Value) * oldCapacity, sizeof(Value) * array->capacity, "Value array buffer");
    }
    
    array->values[array->count++] = value;
}

void freeValueArray(ValueArray* array) {
    // Counting on array being an automatic variable (stack), meaning we do not free the pointer itself.
    deallocate(array->values, array->capacity * sizeof(Value), "Value array buffer");
    initValueArray(array);
}

void printValue(Value value) {
    switch (value.type) {
        case VALUE_NUMBER: {
            printf("%g", value.as.number);
            break;
        }
        case VALUE_OBJECT: {
            printObject(value.as.object);
            break;
        }
        case VALUE_NIL: {
            printf("nil");
            break;
        }
        default: printf("Unknown value type: %d.", value.type); break;
    }
}
