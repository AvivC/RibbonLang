#ifndef plane_pointerarray_h
#define plane_pointerarray_h

typedef struct {
    int count;
    int capacity;
    void** values;
    const char* alloc_string; /* Kind of ugly, purely for debugging */
} PointerArray;

void pointer_array_init(PointerArray* array, const char* alloc_string);
void pointer_array_write(PointerArray* array, void* value);
void pointer_array_free(PointerArray* array);
void** pointer_array_to_plain_array(PointerArray* array, const char* what);

#endif
