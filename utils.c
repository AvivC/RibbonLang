#include "utils.h"

uint16_t twoBytesToShort(uint8_t a, uint8_t b) {
	return (a << 8) + b;
}
