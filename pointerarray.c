#include <stdio.h>
#include "pointerarray.h"
#include "memory.h"

void initPointerArray(PointerArray* array) {
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

void writePointerArray(PointerArray* array, void* value) {
    if (array->count == array->capacity) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = reallocate(array->values, sizeof(void*) * oldCapacity, sizeof(void*) * array->capacity, "Pointer array buffer");
    }
    
    array->values[array->count++] = value;
}

void freePointerArray(PointerArray* array) {
    deallocate(array->values, sizeof(void*) * array->capacity, "Pointer array buffer");
    initPointerArray(array);
}
