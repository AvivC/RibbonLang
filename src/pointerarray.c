#include <string.h>
#include <stdio.h>
#include "pointerarray.h"
#include "memory.h"

void pointer_array_init(PointerArray* array, const char* alloc_string) {
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
    array->alloc_string = alloc_string;
}

void pointer_array_write(PointerArray* array, void* value) {
    if (array->count == array->capacity) {
        int old_capacity = array->capacity;
        array->capacity = GROW_CAPACITY(old_capacity);
        array->values = reallocate(array->values, sizeof(void*) * old_capacity, sizeof(void*) * array->capacity, array->alloc_string);
    }

    array->values[array->count++] = value;
}

void pointer_array_free(PointerArray* array) {
    deallocate(array->values, sizeof(void*) * array->capacity, array->alloc_string);
    pointer_array_init(array, array->alloc_string);
}

void** pointer_array_to_plain_array(PointerArray* array, const char* what) {
	void** plain_array = allocate(sizeof(void*) * array->count, what);
	memcpy(plain_array, array->values, array->count * sizeof(void*));
	return plain_array;
}
