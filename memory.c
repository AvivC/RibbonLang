#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "memory.h"
#include "common.h"

typedef struct {
    const char* name;
    size_t size;
    void* ptr;
    bool allocated;
} Allocation;

static size_t allocatedMemory = 0;
static Allocation* allocations = NULL;
static size_t allocsBufferCapacity = 0;
static size_t allocsBufferCount = 0;

size_t getAllocatedMemory() {
    return allocatedMemory;
}

size_t getAllocationsCount() {
    size_t count = 0;
    for (int i = 0; i < allocsBufferCount; i++) {
        if (allocations[i].allocated) {
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

bool sameAllocation(void* pointer, const char* what, Allocation allocation) {
    return allocation.ptr == pointer && (strcmp(allocation.name, what) == 0);
}

bool samePointerDifferentName(void* pointer, const char* what, Allocation allocation) {
    return allocation.ptr == pointer && (strcmp(allocation.name, what) != 0);
}

void* reallocate(void* pointer, size_t oldSize, size_t newSize, const char* what) {
    if (newSize == 0) {
        // Deallocation
        
        DEBUG_MEMORY("Attempting to deallocate %d bytes, for '%s' at '%p'.", oldSize, what, pointer);
        
        if (pointer != NULL) {
            bool found = false;
            for (int i = 0; i < allocsBufferCount; i++) {
                if (sameAllocation(pointer, what, allocations[i])) {
                    if (allocations[i].allocated) {
                        DEBUG_MEMORY("Marked '%s' as deallocated.", allocations[i].name);
                        allocations[i].allocated = false;
                        allocations[i].ptr = NULL;
                        found = true;
                        break;
                    } else {
                        FAIL("Found matching allocation, but it's already freed.");
                    }
                } else if (samePointerDifferentName(pointer, what, allocations[i])) {
                    FAIL("An existing allocation has a pointer matching the searched one, but different name ('%s' != '%s')"
                    		, what, allocations[i].name);
                }
            }
        
            if (!found) {
                FAIL("deallocate() didn't find an allocation marker to NULL-mark '%s', %p.", what, pointer);
            }
        } else {
            // NULL pointer. Continue as usual
            DEBUG_MEMORY("Freeing NULL pointer. Should be okay.");
        }
        
        DEBUG_MEMORY("Freeing '%s' ('%p') and %d bytes.", what, pointer, oldSize);
        free(pointer); // realloc() shouldn't be called with 0 size
        allocatedMemory -= oldSize;
        return NULL;
    }
    
    if (oldSize == 0) {
        // New allocation
        
        DEBUG_MEMORY("Attempting to allocate %d bytes for '%s'.", newSize, what);
        
        if (allocsBufferCount + 1 > allocsBufferCapacity) {
            allocsBufferCapacity = GROW_CAPACITY(allocsBufferCapacity);
            allocations = realloc(allocations, sizeof(Allocation) * allocsBufferCapacity);
            for (int i = allocsBufferCount; i < allocsBufferCapacity; i++) {
                allocations[i] = (Allocation) {.name = "", .size = 0, .ptr = NULL, .allocated = false};
            }
        }
        
        DEBUG_MEMORY("Allocating for '%s' %d bytes.", what, newSize);
        pointer = realloc(pointer, newSize); // realloc() with NULL is equal to malloc()
        allocations[allocsBufferCount++] = (Allocation) {.name = what, .size = newSize, .ptr = pointer, .allocated = true};
        allocatedMemory += newSize;
        return pointer;
    }
    
    // Reallocation
    
    DEBUG_MEMORY("Attempting to reallocate for '%s' %d bytes instead of %d bytes.", what, newSize, oldSize);
    void* newpointer = realloc(pointer, newSize);
    
    if (newpointer == NULL) {
        FAIL("Reallocation of '%s' failed!", what);
        return NULL;
    }
    
    for (int i = 0; i < allocsBufferCount; i++) {
        if (allocations[i].ptr == pointer) {
            if (strcmp(allocations[i].name, what) != 0) {
                FAIL("In leak-detector - two allocations with the same pointer but different names.");
            }
            
            if (!allocations[i].allocated) {
                FAIL("In leak-detector - relocated allocation seems to be not allocated.");
            }
            
            allocations[i].ptr = newpointer;
            allocations[i].size = newSize;
            break;
        }
    }
    
    allocatedMemory -= oldSize;
    allocatedMemory += newSize;
    
    return newpointer;
}

void printAllocationsBuffer() {  // for debugging
    DEBUG_IMPORTANT_PRINT("\nAllocations buffer:\n");

    for (int i = 0; i < allocsBufferCount; i++) {
        Allocation allocation = allocations[i];
        DEBUG_IMPORTANT_PRINT("[ %-3d: ", i);
        DEBUG_IMPORTANT_PRINT("%p ", allocation.ptr);
        DEBUG_IMPORTANT_PRINT(" | ");
        DEBUG_IMPORTANT_PRINT("%-25s", allocation.name);
        DEBUG_IMPORTANT_PRINT(" | ");
        DEBUG_IMPORTANT_PRINT("%-15s", allocation.allocated ? "Allocated" : "Not allocated");
        DEBUG_IMPORTANT_PRINT(" | ");
        DEBUG_IMPORTANT_PRINT("Last allocated size: %-4" PRIuPTR, allocation.size);
        DEBUG_IMPORTANT_PRINT("]\n");
    }
}
