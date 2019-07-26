#ifndef plane_pointerarray_h
#define plane_pointerarray_h

typedef struct {
    int count;
    int capacity;
    void** values;
} PointerArray;

void init_pointer_array(PointerArray* array);
void write_pointer_array(PointerArray* array, void* value);
void freePointerArray(PointerArray* array);
void** pointerArrayToPlainArray(PointerArray* array, const char* what);

#endif
