#ifndef plane_utils_h
#define plane_utils_h

#include "common.h"
#include "dynamic_array.h"

uint16_t two_bytes_to_short(uint8_t a, uint8_t b);

void short_to_two_bytes(uint16_t num, uint8_t* bytes_out);

// TODO: void printStack(void);

DEFINE_DYNAMIC_ARRAY(size_t, IntegerArray, integer_array)

#endif
