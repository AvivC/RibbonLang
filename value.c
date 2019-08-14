#include <stdio.h>
#include "value.h"
#include "object.h"
#include "memory.h"

IMPLEMENT_DYNAMIC_ARRAY(Value, ValueArray, value_array)

void printValue(Value value) {
    switch (value.type) {
        case VALUE_NUMBER: {
            printf("%g", value.as.number);
            break;
        }
        case VALUE_OBJECT: {
            printObject(value.as.object);
            break;
        }
        case VALUE_BOOLEAN: {
            printf(value.as.boolean ? "true" : "false");
            break;
        }
        case VALUE_NIL: {
            printf("nil");
            break;
        }
        default: printf("Unknown value type: %d.", value.type); break;
    }
}

bool compareValues(Value a, Value b, int* output) {
	if (a.type != b.type) {
		return false;
	}

	if (a.type == VALUE_NUMBER) {
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

	} else if (a.type == VALUE_BOOLEAN) {
		bool b1 = a.as.boolean;
		bool b2 = b.as.boolean;
		if (b1 == b2) {
			*output = 0;
		} else {
			*output = -1;
		}
		return true;

	} else if (a.type == VALUE_OBJECT) {
		bool objectsEqual = compareObjects(a.as.object, b.as.object);
		if (objectsEqual) {
			*output = 0;
		} else {
			*output = -1;
		}
		return true;
	}

	FAIL("Couldn't compare values.");
	return false;
}
