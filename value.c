#include <stdio.h>
#include <string.h>
#include <math.h>

#include "value.h"
#include "plane_object.h"
#include "memory.h"

void value_print(Value value) {
    switch (value.type) {
        case VALUE_NUMBER: {
            printf("%g", value.as.number);
            return;
        }
        case VALUE_OBJECT: {
            object_print(value.as.object);
            return;
        }
        case VALUE_BOOLEAN: {
            printf(value.as.boolean ? "true" : "false");
            return;
        }
        case VALUE_RAW_STRING: {
        	RawString string = value.as.raw_string;
			printf("\"%.*s\"", string.length, string.data);
			return;
		}
        case VALUE_CHUNK: {
        	Bytecode chunk = value.as.chunk;
        	printf("< Chunk of size %d pointing at '%p' >", chunk.count, chunk.code);
        	return;
        }
        case VALUE_NIL: {
            printf("nil");
            return;
        }
		case VALUE_ADDRESS: {
			printf("%" PRIxPTR , value.as.address);
			return; 
		}
		case VALUE_ALLOCATION: {
			Allocation allocation = value.as.allocation;
			printf("<Internal: allocation marker of '\%s' size %" PRI_SIZET ">", allocation.name, allocation.size);
			return;
		}
    }

    FAIL("Unrecognized VALUE_TYPE: %d", value.type);
}

bool value_compare(Value a, Value b, int* output) {
	if (a.type != b.type) {
		return false;
	}

	switch (a.type) {
		case VALUE_NUMBER: {
			double n1 = a.as.number;
			double n2 = b.as.number;

			if (n1 == n2) {
				*output = 0;
			} else if (n1 > n2) {
				*output = 1;
			} else {
				*output = -1;
			}
			return true;
		}

		case VALUE_BOOLEAN: {
			bool b1 = a.as.boolean;
			bool b2 = b.as.boolean;
			if (b1 == b2) {
				*output = 0;
			} else {
				*output = -1;
			}
			return true;
		}

		case VALUE_OBJECT: {
			bool objectsEqual = object_compare(a.as.object, b.as.object);
			if (objectsEqual) {
				*output = 0;
			} else {
				*output = -1;
			}
			return true;
		}

		case VALUE_NIL: {
			*output = 0;
			return true;
		}

		case VALUE_ADDRESS: {
			uintptr_t addr1 = a.as.address;
			uintptr_t addr2 = b.as.address;
						
			if (addr1 == addr2) {
				*output = 0;
			} else if (addr1 < addr2) {
				*output = -1;
			} else {
				*output = 1;
			}
			
			return true;
		}

		case VALUE_ALLOCATION: {
			Allocation alloc1 = a.as.allocation;
			Allocation alloc2 = b.as.allocation;

			*output = (alloc1.size == alloc2.size) && (strcmp(alloc1.name, alloc2.name) == 0);
			return true;
		}

		case VALUE_CHUNK:
		case VALUE_RAW_STRING: {
			FAIL("Attempting to compare chunks or raw_strings, shouldn't happen.");
			return false;
		}
	}

	FAIL("Couldn't compare values. Type A: %d, type B: %d", a.type, b.type);
	return false;
}

bool value_hash(Value* value, unsigned long* result) {
	switch (value->type) {
		case VALUE_OBJECT: {
			unsigned long hash;
			if (object_hash(value->as.object, &hash)) {
				*result = hash;
				return true;
			}
			return false;
		}
		case VALUE_CHUNK: {
			FAIL("Since Bytecode values aren't supposed to be reachable directly from user code, this should never happen.");
			return false;
		}
		case VALUE_BOOLEAN: {
			*result = value->as.boolean ? 0 : 1;
			return true;
		}
		case VALUE_NUMBER: {
			*result = hash_int(floor(value->as.number)); // TODO: Not good at all, redo this
			return true;
		}
		case VALUE_NIL: {
			*result = 0;
			return true;
		}
		case VALUE_RAW_STRING: {
			FAIL("Hashing a RAW_STRING shouldn't really happen ever.");
			return false;
		}
		case VALUE_ADDRESS: {
			*result = hash_int(value->as.address); // Not good at all, but should logically work
			return true;
		}
		case VALUE_ALLOCATION: {
			return false;
		}
	}

	FAIL("value.c:hash_value - shouldn't get here.");
	return false;
}
