#include "value_array.h"
#include "value.h"

IMPLEMENT_DYNAMIC_ARRAY(struct Value, ValueArray, value_array)

ValueArray value_array_make(int count, struct Value* values) {
    ValueArray array;
    value_array_init(&array);
    for (int i = 0; i < count; i++) {
        value_array_write(&array, &values[i]);
    }
    return array;
}