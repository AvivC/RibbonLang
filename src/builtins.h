#ifndef ribbon_builtins_h
#define ribbon_builtins_h

#include "value.h"

bool builtin_print(Object* self, ValueArray args, Value* out);
bool builtin_input(Object* self, ValueArray args, Value* out);
bool builtin_read_text_file(Object* self, ValueArray args, Value* out);
bool builtin_read_binary_file(Object* self, ValueArray args, Value* out);
bool builtin_write_text_file(Object* self, ValueArray args, Value* out);
bool builtin_write_binary_file(Object* self, ValueArray args, Value* out);
bool builtin_delete_file(Object* self, ValueArray args, Value* out);
bool builtin_file_exists(Object* self, ValueArray args, Value* out);
bool builtin_spawn(Object* self, ValueArray args, Value* out);
bool builtin_time(Object* self, ValueArray args, Value* out);
bool builtin_to_number(Object* self, ValueArray args, Value* out);
bool builtin_to_string(Object* self, ValueArray args, Value* out);
bool builtin_has_attr(Object* self, ValueArray args, Value* out);
bool builtin_random(Object* self, ValueArray args, Value* out);
bool builtin_get_main_file_path(Object* self, ValueArray args, Value* out);
bool builtin_get_main_file_directory(Object* self, ValueArray args, Value* out);
bool builtin_is_instance(Object* self, ValueArray args, Value* out);
bool builtin_get_type(Object* self, ValueArray args, Value* out);
bool builtin_super(Object* self, ValueArray args, Value* out);

#endif
