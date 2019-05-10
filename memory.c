#include <stdlib.h>
#include <stdio.h>

#include "memory.h"

static size_t allocatedMemory = 0;

size_t getAllocatedMemory() {
    return allocatedMemory;
}

void* allocate(size_t size, const char* what) {
    return reallocate(NULL, 0, size, what);
}

void deallocate(void* pointer, size_t oldSize, const char* what) {
    reallocate(pointer, oldSize, 0, what);
}

void* reallocate(void* pointer, size_t oldSize, size_t newSize, const char* what) {
    if (newSize == 0) {
        DEBUG_PRINT("reallocate() called on '%s' with newSize=0 - freeing pointer '%p' and %d bytes.", what, pointer, oldSize);
        free(pointer); // realloc() shouldn't be called with 0 size
        allocatedMemory -= oldSize;
        return NULL;
    }
    
    DEBUG_PRINT("Allocating for '%s' %d bytes instead of %d bytes.\n", what, newSize, oldSize);
    pointer = realloc(pointer, newSize); // realloc() with NULL is equal to malloc()
    
    if (pointer == NULL) {
        DEBUG_PRINT("Reallocation of '%s' failed!\n", what);
        exit(EXIT_FAILURE); // TODO: Temporary. Handle in a different way
        return NULL;
    }
    
    allocatedMemory -= oldSize;
    allocatedMemory += newSize;
    
    return pointer;
}
