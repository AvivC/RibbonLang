#ifndef plane_memory_h
#define plane_memory_h

#include "common.h"

#define GROW_CAPACITY(capacity) (capacity) < 8 ? 8 : (capacity) * 2

size_t get_allocated_memory();
size_t get_allocations_count();

void* allocate(size_t size, const char* what);
void deallocate(void* pointer, size_t oldSize, const char* what);
void* reallocate(void* pointer, size_t oldSize, size_t newSize, const char* what);

void print_allocated_memory_entries(); // for debugging

#endif
