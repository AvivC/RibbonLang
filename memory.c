#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "common.h"

static size_t allocatedMemory = 0;
static const char** allocations = NULL;
static size_t allocsBufferCapacity = 0;
static size_t allocsBufferCount = 0;

size_t getAllocatedMemory() {
    return allocatedMemory;
}

size_t getAllocationsCount() {
    size_t count = 0;
    for (int i = 0; i < allocsBufferCount; i++) {
        if (allocations[i] != NULL) {
            count++;
        }
    }
    
    return count;
}

void* allocate(size_t size, const char* what) {
    return reallocate(NULL, 0, size, what);
}

void deallocate(void* pointer, size_t oldSize, const char* what) {
    reallocate(pointer, oldSize, 0, what);
}

void* reallocate(void* pointer, size_t oldSize, size_t newSize, const char* what) {
    if (newSize == 0) {
        // Deallocation
        
        bool found = false;
        for (int i = 0; i < allocsBufferCount; i++) {
            if ((allocations[i] != NULL) && (strcmp(allocations[i], what) == 0)) {
                DEBUG_MEMORY_PRINT("Marked '%s' as deallocated.\n", allocations[i]);
                allocations[i] = NULL;
                found = true;
                break;
            }
        }
    
        if (!found) {
            FAIL("deallocate() didn't find an allocation marker to NULL-mark '%s'.", what);
        }
        
        DEBUG_MEMORY_PRINT("Freeing '%s' ('%p') and %d bytes.\n", what, pointer, oldSize);
        free(pointer); // realloc() shouldn't be called with 0 size
        allocatedMemory -= oldSize;
        return NULL;
    }
    
    if (oldSize == 0) {
        // New allocation
        if (allocsBufferCount + 1 > allocsBufferCapacity) {
            allocsBufferCapacity = GROW_CAPACITY(allocsBufferCapacity);
            allocations = realloc(allocations, sizeof(void*) * allocsBufferCapacity);
        }
        
        allocations[allocsBufferCount++] = what;
    }
    
    DEBUG_MEMORY_PRINT("Allocating for '%s' %d bytes instead of %d bytes.\n", what, newSize, oldSize);
    pointer = realloc(pointer, newSize); // realloc() with NULL is equal to malloc()
    
    if (pointer == NULL) {
        DEBUG_IMPORTANT_PRINT("Reallocation of '%s' failed!\n", what);
        exit(EXIT_FAILURE); // TODO: Temporary. Handle in a different way
        return NULL;
    }
    
    allocatedMemory -= oldSize;
    allocatedMemory += newSize;
    
    return pointer;
}

void printAllocationsBuffer() {  // for debugging
    DEBUG_MEMORY_PRINT("Allocations buffer:\n");

    for (int i = 0; i < allocsBufferCount; i++) {
        DEBUG_MEMORY_PRINT("[ %-4d: ", i);
        DEBUG_MEMORY_PRINT(allocations[i] == NULL ? "NULL" : allocations[i]);
        DEBUG_MEMORY_PRINT("]\n");
    }
}
