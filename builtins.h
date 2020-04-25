#ifndef plane_builtins_h
#define plane_builtins_h

#include "value.h"

bool builtin_print(Object* self, ValueArray args, Value* out);
bool builtin_input(Object* self, ValueArray args, Value* out);
bool builtin_read_file(Object* self, ValueArray args, Value* out);
bool builtin_write_file(Object* self, ValueArray args, Value* out);
bool builtin_delete_file(Object* self, ValueArray args, Value* out);
bool builtin_file_exists(Object* self, ValueArray args, Value* out);
bool builtin_spawn(Object* self, ValueArray args, Value* out);
bool builtin_time(Object* self, ValueArray args, Value* out);

#endif
