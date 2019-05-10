#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "table.h"
#include "object.h"

static size_t allocatedMemory = 0;
static long int allocationsCount = 0;
static Table allocations;

void initMemoryManager() {
    initTable(&allocations);
}

size_t getAllocatedMemory() {
    return allocatedMemory;
}

size_t getAllocationsCount() {
    return allocationsCount;
}

void* allocate(size_t size, const char* what) {
    // ObjectString* whatAsObj = copyString(what, strlen(what));
    // Value currentAllocsVal;
    // if (getTable(&allocations, whatAsObj, &currentAllocsVal)) {
        // double newAllocsNum = currentAllocsVal.as.number + 1;
        // setTable(&allocations, whatAsObj, MAKE_VALUE_NUMBER(newAllocsNum));
    // } else {
        // setTable(&allocations, whatAsObj, MAKE_VALUE_NUMBER(0));
    // }
    
    allocationsCount++;
    return reallocate(NULL, 0, size, what);
}

void deallocate(void* pointer, size_t oldSize, const char* what) {
    allocationsCount--;
    reallocate(pointer, oldSize, 0, what);
}

void* reallocate(void* pointer, size_t oldSize, size_t newSize, const char* what) {
    if (newSize == 0) {
        DEBUG_PRINT("Freeing '%s' ('%p') and %d bytes.", what, pointer, oldSize);
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
