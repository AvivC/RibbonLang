#include <stdio.h>
#include <string.h>
#include <math.h>

#include "value.h"
#include "ribbon_object.h"
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
		*output = -1;
		return true;
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

			*output = (alloc1.size == alloc2.size) && (strcmp(alloc1.name, alloc2.name) == 0); /* BUG ??? */
			return true;
		}

		case VALUE_RAW_STRING: {
			RawString s1 = a.as.raw_string;
			RawString s2 = b.as.raw_string;
			if (s1.hash == s2.hash && cstrings_equal(s1.data, s1.length, s2.data, s2.length)) {
				*output = 0;
			} else {
				*output = -1;
			}
			return true;	
		}
	}

	FAIL("Couldn't compare values. Type A: %d, type B: %d", a.type, b.type);
	return false;
}

bool value_hash(Value* value, unsigned long* result) {
	ValueType type = value->type;
	switch (type) {
		case VALUE_OBJECT: {
			unsigned long hash;
			if (object_hash(value->as.object, &hash)) {
				*result = hash;
				return true;
			}
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
			RawString string = value->as.raw_string;
			*result = string.hash;
			return true;
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

const char* value_get_type(Value value) {
	switch (value.type) {
		case VALUE_NUMBER: return "Number";
		case VALUE_BOOLEAN: return "Boolean";
		case VALUE_NIL: return "Nil";
		case VALUE_OBJECT: return object_get_type_name(value.as.object);
		case VALUE_ALLOCATION: FAIL("Shouldn't ever get type of Allocation value.");
		case VALUE_ADDRESS: FAIL("Shouldn't ever get type of Address value.");
		case VALUE_RAW_STRING: FAIL("Shouldn't ever get type of RawString value.");
	}

	FAIL("Illegal value type passed in value_get_type(): %d", value.type);
	return NULL;
}