#ifndef plane_memory_h
#define plane_memory_h

#include "common.h"

#define GROW_CAPACITY(capacity) (capacity) < 8 ? 8 : (capacity) * 2

void initMemoryManager();
size_t getAllocatedMemory();
size_t getAllocationsCount();
void* allocate(size_t size, const char* what);
void deallocate(void* pointer, size_t oldSize, const char* what);
void* reallocate(void* pointer, size_t oldSize, size_t newSize, const char* what);

#endif
