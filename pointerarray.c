#include <string.h>
#include <stdio.h>
#include "pointerarray.h"
#include "memory.h"

void init_pointer_array(PointerArray* array) {
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

void write_pointer_array(PointerArray* array, void* value) {
    if (array->count == array->capacity) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = reallocate(array->values, sizeof(void*) * oldCapacity, sizeof(void*) * array->capacity, "Pointer array buffer");
    }
    
    array->values[array->count++] = value;
}

void freePointerArray(PointerArray* array) {
    deallocate(array->values, sizeof(void*) * array->capacity, "Pointer array buffer");
    init_pointer_array(array);
}

void** pointerArrayToPlainArray(PointerArray* array, const char* what) {
	void** plainArray = allocate(sizeof(void*) * array->count, what);
	memcpy(plainArray, array->values, array->count * sizeof(void*));
	return plainArray;
}
