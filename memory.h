#ifndef plane_memory_h
#define plane_memory_h

#include "common.h"

typedef struct {
    const char* name;
    size_t size;
} Allocation;

#define GROW_CAPACITY(capacity) (capacity) < 8 ? 8 : (capacity) * 2

size_t get_allocated_memory();
size_t get_allocations_count();

void memory_init(void);

void* allocate(size_t size, const char* what);
void deallocate(void* pointer, size_t oldSize, const char* what);
void* reallocate(void* pointer, size_t oldSize, size_t newSize, const char* what);

void* allocate_no_tracking(size_t size);
void deallocate_no_tracking(void* pointer);
void* reallocate_no_tracking(void* pointer, size_t new_size);

void memory_print_allocated_entries(); // for debugging

#endif
