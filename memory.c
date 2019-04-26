#include <stdlib.h>
#include <stdio.h>

#include "memory.h"

void* allocate(size_t size) {
    return reallocate(NULL, 0, size);
}

void deallocate(void* pointer, size_t oldSize) {
    reallocate(pointer, oldSize, 0);
}

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        DEBUG_PRINT("reallocate() called with newSize=0 - freeing pointer '%p' and %d bytes.", pointer, oldSize);
        free(pointer); // realloc() shouldn't be called with 0 size
        return NULL;
    }
    
    DEBUG_PRINT("Allocating %d bytes insted of %d bytes.\n", newSize, oldSize);
    pointer = realloc(pointer, newSize); // realloc() with NULL is equal to malloc()
    
    if (pointer == NULL) {
        DEBUG_PRINT("Reallocation failed!\n");
        return NULL;
    }
    
    return pointer;
}
