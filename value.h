#ifndef plane_value_h
#define plane_value_h

#include "bytecode.h"
#include "common.h"
#include "dynamic_array.h"

typedef enum {
    VALUE_NUMBER,
    VALUE_BOOLEAN,
    VALUE_NIL,
	VALUE_RAW_STRING,
	VALUE_CHUNK,
    VALUE_OBJECT
} ValueType;

typedef struct {
	const char* data;
	int length;
} RawString;

typedef struct Value {
    ValueType type;
    union {
        double number;
        bool boolean;
        RawString raw_string;
        Bytecode chunk;
        struct Object* object;
    } as;
} Value;

#define MAKE_VALUE_NUMBER(n) (Value){.type = VALUE_NUMBER, .as.number = (n)}
#define MAKE_VALUE_BOOLEAN(val) (Value){.type = VALUE_BOOLEAN, .as.boolean = (val)}
#define MAKE_VALUE_NIL() (Value){.type = VALUE_NIL, .as.number = -1}
#define MAKE_VALUE_RAW_STRING(cstring, the_length) (Value){.type = VALUE_RAW_STRING, \
														.as.raw_string = (RawString) {.data = (cstring), .length = (the_length)}}
#define MAKE_VALUE_OBJECT(o) (Value){.type = VALUE_OBJECT, .as.object = (struct Object*)(o)}
#define MAKE_VALUE_CHUNK(the_chunk) (Value){.type = VALUE_CHUNK, .as.chunk = (the_chunk)}

#define ASSERT_VALUE_TYPE(value, expected_type) \
	do { \
		if (value.type != expected_type) { \
			FAIL("Expected value type: %d, found: %d", expected_type, value.type); \
		} \
	} while (false)

void value_print(Value value);
bool value_compare(Value a, Value b, int* output);

bool value_hash(Value* value, unsigned long* result);

#endif
