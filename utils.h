#ifndef plane_utils_h
#define plane_utils_h

#include "common.h"

uint16_t twoBytesToShort(uint8_t a, uint8_t b);
void short_to_two_bytes(uint16_t num, uint8_t* bytes_out);

#endif
