#ifndef plane_memory_h
#define plane_memory_h

#include "common.h"

#define GROW_CAPACITY(capacity) (capacity) < 8 ? 8 : (capacity) * 2

void* allocate(size_t size);
void deallocate(void* pointer, size_t oldSize);
void* reallocate(void* pointer, size_t oldSize, size_t newSize);

#endif