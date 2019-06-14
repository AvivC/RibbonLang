#ifndef plane_pointerarray_h
#define plane_pointerarray_h

typedef struct {
    int count;
    int capacity;
    void** values;
} PointerArray;

void initPointerArray(PointerArray* array);
void writePointerArray(PointerArray* array, void* value);
void freePointerArray(PointerArray* array);
void** pointerArrayToPlainArray(PointerArray* array, const char* what);

#endif
