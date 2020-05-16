#ifndef ribbon_dynamic_array_h
#define ribbon_dynamic_array_h

#include "common.h"
#include "memory.h"

#define DECLARE_DYNAMIC_ARRAY(TYPE, ARRAY_NAME, FUNCTIONS_PERFIX) \
	typedef struct { \
    	int count; \
    	int capacity; \
    	TYPE * values; \
	} ARRAY_NAME; \
	\
	void FUNCTIONS_PERFIX##_init(ARRAY_NAME* array); \
	\
	void FUNCTIONS_PERFIX##_write(ARRAY_NAME* array, TYPE* value); \
	\
	void FUNCTIONS_PERFIX##_free(ARRAY_NAME* array); \

#define IMPLEMENT_DYNAMIC_ARRAY(TYPE, ARRAY_NAME, FUNCTIONS_PERFIX) \
\
void FUNCTIONS_PERFIX##_init(ARRAY_NAME* array) {\
    array->values = NULL;\
    array->count = 0;\
    array->capacity = 0;\
}\
\
void FUNCTIONS_PERFIX##_write(ARRAY_NAME* array, TYPE* value) {\
    if (array->count == array->capacity) {\
        int oldCapacity = array->capacity;\
        array->capacity = GROW_CAPACITY(oldCapacity);\
        array->values = reallocate(array->values, sizeof(TYPE) * oldCapacity, sizeof(TYPE) * array->capacity, #ARRAY_NAME " buffer");\
    }\
\
    array->values[array->count++] = *value;\
}\
\
void FUNCTIONS_PERFIX##_free(ARRAY_NAME* array) {\
	if (array->values != NULL) {\
    	deallocate(array->values, array->capacity * sizeof(TYPE), #ARRAY_NAME " buffer");\
    }\
    FUNCTIONS_PERFIX##_init(array);\
}\

#endif
