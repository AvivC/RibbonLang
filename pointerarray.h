#ifndef plane_pointerarray_h
#define plane_pointerarray_h

typedef struct {
    int count;
    int capacity;
    void** values;
} PointerArray;

void pointer_array_init(PointerArray* array);
void pointer_array_write(PointerArray* array, void* value);
void pointer_array_free(PointerArray* array);
void** pointer_array_to_plain_array(PointerArray* array, const char* what);

#endif
