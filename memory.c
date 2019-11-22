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

static size_t allocated_memory = 0;
static Allocation* allocations = NULL;
static size_t allocs_buffer_capacity = 0;
static size_t allocs_buffer_count = 0;

size_t get_allocated_memory() {
    return allocated_memory;
}

size_t get_allocations_count() {
    size_t count = 0;
    for (int i = 0; i < allocs_buffer_count; i++) {
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

bool is_same_allocation(void* pointer, const char* what, Allocation allocation) {
    return allocation.ptr == pointer && (strcmp(allocation.name, what) == 0);
}

bool is_same_pointer_different_name(void* pointer, const char* what, Allocation allocation) {
    return allocation.ptr == pointer && (strcmp(allocation.name, what) != 0);
}

static void manage_allocations_buffer_size(void) {
	if (allocs_buffer_count % 5000 == 0) {
		int num_allocated = 0;
		Allocation* new_allocations = malloc(sizeof(Allocation) * allocs_buffer_count);
		for (int i = 0; i < allocs_buffer_count; i++) {
			if (allocations[i].allocated) {
				new_allocations[num_allocated++] = allocations[i];
			}
		}

		allocs_buffer_count = num_allocated;
		allocs_buffer_capacity = allocs_buffer_count;
		new_allocations = realloc(new_allocations, sizeof(Allocation) * allocs_buffer_capacity);
		free(allocations);
		allocations = new_allocations;
	}
}

void* reallocate(void* pointer, size_t old_size, size_t new_size, const char* what) {
    manage_allocations_buffer_size();

    if (new_size == 0) {
        // Deallocation
        
        DEBUG_MEMORY("Attempting to deallocate %d bytes, for '%s' at '%p'.", old_size, what, pointer);
        
        if (pointer != NULL) {
            bool found = false;
            for (int i = 0; i < allocs_buffer_count; i++) {
                if (is_same_allocation(pointer, what, allocations[i])) {
                    if (allocations[i].allocated) {
                        DEBUG_MEMORY("Marked '%s' as deallocated.", allocations[i].name);
                        allocations[i].allocated = false;
                        allocations[i].ptr = NULL;
                        found = true;
                        break;
                    } else {
                        FAIL("Found matching allocation, but it's already freed.");
                    }
                } else if (is_same_pointer_different_name(pointer, what, allocations[i])) {
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
        
        DEBUG_MEMORY("Freeing '%s' ('%p') and %d bytes.", what, pointer, old_size);
        free(pointer); // realloc() shouldn't be called with 0 size
        allocated_memory -= old_size;
        return NULL;
    }
    
    if (old_size == 0) {
        // New allocation
        
        DEBUG_MEMORY("Attempting to allocate %d bytes for '%s'.", new_size, what);
        
        if (allocs_buffer_count + 1 > allocs_buffer_capacity) {
            allocs_buffer_capacity = GROW_CAPACITY(allocs_buffer_capacity);
            allocations = realloc(allocations, sizeof(Allocation) * allocs_buffer_capacity);
            for (int i = allocs_buffer_count; i < allocs_buffer_capacity; i++) {
                allocations[i] = (Allocation) {.name = "", .size = 0, .ptr = NULL, .allocated = false};
            }
        }
        
        DEBUG_MEMORY("Allocating for '%s' %d bytes.", what, new_size);
        pointer = realloc(pointer, new_size); // realloc() with NULL is equal to malloc()
        allocations[allocs_buffer_count++] = (Allocation) {.name = what, .size = new_size, .ptr = pointer, .allocated = true};
        allocated_memory += new_size;
        return pointer;
    }
    
    // Reallocation
    
    DEBUG_MEMORY("Attempting to reallocate for '%s' %d bytes instead of %d bytes.", what, new_size, old_size);
    void* newpointer = realloc(pointer, new_size);
    
    if (newpointer == NULL) {
         // TODO: Should be a runtime error, not a FAIL?
        FAIL("Reallocation of '%s' failed! "
                "Pointer: %p, new_size: %" PRI_SIZET " . Total allocated memory: %" PRI_SIZET, what, pointer, new_size, get_allocated_memory());
        return NULL;
    }
    
    for (int i = 0; i < allocs_buffer_count; i++) {
        if (allocations[i].ptr == pointer) {
            if (strcmp(allocations[i].name, what) != 0) {
                FAIL("In leak-detector - two allocations with the same pointer but different names.");
            }
            
            if (!allocations[i].allocated) {
                FAIL("In leak-detector - relocated allocation seems to be not allocated.");
            }
            
            allocations[i].ptr = newpointer;
            allocations[i].size = new_size;
            break;
        }
    }
    
    allocated_memory -= old_size;
    allocated_memory += new_size;
    
    return newpointer;
}

void print_allocated_memory_entries() {  // for debugging
    DEBUG_IMPORTANT_PRINT("\nAllocated memory entries:\n");

    for (int i = 0; i < allocs_buffer_count; i++) {
        Allocation allocation = allocations[i];
        if (allocation.allocated) {
			DEBUG_IMPORTANT_PRINT("[ %-3d: ", i);
			DEBUG_IMPORTANT_PRINT("%p ", allocation.ptr);
			DEBUG_IMPORTANT_PRINT(" | ");
			DEBUG_IMPORTANT_PRINT("%-33s", allocation.name);
			DEBUG_IMPORTANT_PRINT(" | ");
			DEBUG_IMPORTANT_PRINT("Last allocated size: %-4" PRIuPTR, allocation.size);
			DEBUG_IMPORTANT_PRINT("]\n");
        }
    }
}
