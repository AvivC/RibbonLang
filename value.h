#ifndef plane_value_h
#define plane_value_h

#include "common.h"
#include "dynamic_array.h"

typedef enum {
    VALUE_NUMBER,
    VALUE_BOOLEAN,
    VALUE_NIL,
	VALUE_RAW_STRING,
	VALUE_CODE,
    VALUE_OBJECT
} ValueType;

typedef struct {
	const char* data;
	int length;
} RawString;

typedef struct {
	uint8_t* bytes;
	int length;
	char** params;
	int num_params;
} RawCode;

typedef struct {
    ValueType type;
    union {
        double number;
        bool boolean;
        RawString raw_string;
        RawCode code;
        struct Object* object;
    } as;
} Value;

DEFINE_DYNAMIC_ARRAY(Value, ValueArray, value_array)

#define MAKE_VALUE_NUMBER(n) (Value){.type = VALUE_NUMBER, .as.number = (n)}
#define MAKE_VALUE_BOOLEAN(val) (Value){.type = VALUE_BOOLEAN, .as.boolean = (val)}
#define MAKE_VALUE_NIL() (Value){.type = VALUE_NIL, .as.number = -1}
#define MAKE_VALUE_RAW_STRING(cstring, the_length) (Value){.type = VALUE_RAW_STRING, \
														.as.raw_string = (RawString) {.data = (cstring), .length = (the_length)}}
#define MAKE_VALUE_CODE(the_code, the_length, the_params, the_num_params) (Value){.type = VALUE_CODE, \
														.as.code = (RawCode) {.bytes = the_code, .length = the_length, \
																				.params = the_params, .num_params = the_num_params}}
#define MAKE_VALUE_OBJECT(o) (Value){.type = VALUE_OBJECT, .as.object = (struct Object*)(o)}

void printValue(Value value);
bool compareValues(Value a, Value b, int* output);

#endif
