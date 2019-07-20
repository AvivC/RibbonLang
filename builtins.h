#ifndef plane_builtins_h
#define plane_builtins_h

#include "value.h"

bool builtin_print(ValueArray args, Value* out);
bool builtin_input(ValueArray args, Value* out);
bool builtin_read_file(ValueArray args, Value* out);

#endif
