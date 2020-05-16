#ifndef ribbon_value_array_h
#define ribbon_value_array_h

#include "dynamic_array.h"

DECLARE_DYNAMIC_ARRAY(struct Value, ValueArray, value_array)

ValueArray value_array_make(int count, struct Value* values);

#endif
