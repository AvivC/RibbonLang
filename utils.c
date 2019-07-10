#include "utils.h"

uint16_t twoBytesToShort(uint8_t a, uint8_t b) {
	return (a << 8) + b;
}

void short_to_two_bytes(uint16_t num, uint8_t* bytes_out) {
	bytes_out[0] = (num >> 8) & 0xFF;
	bytes_out[1] = num & 0xFF;
}
