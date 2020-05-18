#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "memory.h"
#include "common.h"
#include "table.h"

static Table allocations;
static size_t allocated_memory = 0;

void memory_init(void) {
    table_init_memory_infrastructure(&allocations);
    allocated_memory = 0; // For consistency
}

size_t get_allocated_memory() {
    return allocated_memory;
}

size_t get_allocations_count() {
    return allocations.num_entries;
}

static bool is_same_allocation(size_t size, const char* what, Allocation allocation) {
    return (allocation.size == size) && (strcmp(allocation.name, what) == 0);
}

void* allocate(size_t size, const char* what) {
    if (size <= 0) {
        FAIL("allocate :: size <= 0");
    }

    #if MEMORY_DIAGNOSTICS
    return reallocate(NULL, 0, size, what);

    #else

    return allocate_no_tracking(size);

    #endif
}

void deallocate(void* pointer, size_t oldSize, const char* what) {
    #if MEMORY_DIAGNOSTICS

    reallocate(pointer, oldSize, 0, what);

    #else

    return deallocate_no_tracking(pointer);

    #endif
}

static void* do_deallocation(void* pointer, size_t old_size, const char* what) {
    // We allow the pointer to be NULL, and if it is we do nothing

    if (pointer != NULL) {
        if (!table_delete(&allocations, MAKE_VALUE_ADDRESS(pointer))) {
            FAIL("Couldn't remove existing key in allocations table: %p. Allocation tag: '%s'", pointer, what);
        }
    }
    
    DEBUG_MEMORY("Freeing '%s' ('%p') and %" PRI_SIZET " bytes.", what, pointer, old_size);
    free(pointer); /* If pointer == NULL, free on NULL is a legal noop */
    allocated_memory -= old_size;

    return NULL;
}

static void* do_reallocation(void* pointer, size_t old_size, size_t new_size, const char* what) {
    DEBUG_MEMORY("Attempting to reallocate for '%s' %" PRI_SIZET " bytes instead of %" PRI_SIZET " bytes.", what, new_size, old_size);
    void* newpointer = realloc(pointer, new_size);
    
    if (newpointer == NULL) {
         // TODO: Should be a severe runtime error, not a FAIL?
        FAIL("Reallocation of '%s' failed! "
                "Pointer: %p, new_size: %" PRI_SIZET " . Total allocated memory: %" PRI_SIZET, what, pointer, new_size, get_allocated_memory());
        return NULL;
    }

    Value allocation_out;
    if (table_get(&allocations, MAKE_VALUE_ADDRESS(pointer), &allocation_out)) {
        Allocation existing = allocation_out.as.allocation;
        if (!is_same_allocation(old_size, what, existing)) {
            FAIL("When attempting relocation, table returned wrong allocation marker.");
        }

        if (table_delete(&allocations, MAKE_VALUE_ADDRESS(pointer))) {
            Value new_allocation_key = MAKE_VALUE_ADDRESS(newpointer);
            Value new_allocation = MAKE_VALUE_ALLOCATION(what, new_size);
            table_set(&allocations, new_allocation_key, new_allocation);
        } else {
            FAIL("In reallocation, couldn't remove entry which was found in the table.");
        }
    } else {
        FAIL("memory do_reallocation(): Couldn't find marker to replace.");
    }
    
    allocated_memory -= old_size;
    allocated_memory += new_size;
    
    return newpointer;
}

static void* do_allocation(void* pointer, size_t new_size, const char* what) {
    DEBUG_MEMORY("Allocating for '%s' %" PRI_SIZET " bytes.", what, new_size);

    pointer = realloc(pointer, new_size); // realloc() with NULL is equal to malloc()

    if (pointer == NULL) {
        FAIL("Couldn't allocate new memory."); // Temp
    }

    table_set(&allocations, MAKE_VALUE_ADDRESS(pointer), MAKE_VALUE_ALLOCATION(what, new_size));
    allocated_memory += new_size;

    return pointer;
}

void* reallocate(void* pointer, size_t old_size, size_t new_size, const char* what) {
    #if MEMORY_DIAGNOSTICS

    if (new_size == 0) {
        return do_deallocation(pointer, old_size, what);
    }
    
    if (old_size == 0) {
        return do_allocation(pointer, new_size, what);
    }
    
    return do_reallocation(pointer, old_size, new_size, what);

    #else

    if (new_size == 0) {
        deallocate_no_tracking(pointer);
        return NULL;
    }

    return reallocate_no_tracking(pointer, new_size);

    #endif
}

void* allocate_no_tracking(size_t size) {
    if (size <= 0) {
        FAIL("allocate_no_tracking :: size <= 0");
    }

    return malloc(size);
}

void deallocate_no_tracking(void* pointer) {
    free(pointer);
}

void* reallocate_no_tracking(void* pointer, size_t new_size) {
    if (new_size <= 0) {
        FAIL("reallocate_no_tracking :: new_size <= 0");
    }

    return realloc(pointer, new_size);
}

void memory_print_allocated_entries() {  // for debugging
    DEBUG_IMPORTANT_PRINT("Allocated memory entries:\n");

    printf("\n");
    // table_print_debug_as_buckets(&allocations, false);
    table_print_debug(&allocations);
    printf("\n");
}
