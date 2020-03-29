#ifndef plane_plane_utils_h
#define plane_plane_utils_h

#include "common.h"
#include "dynamic_array.h"

uint16_t two_bytes_to_short(uint8_t a, uint8_t b);

void short_to_two_bytes(uint16_t num, uint8_t* bytes_out);

char* copy_cstring(const char* string, int length, const char* what);

char* copy_null_terminated_cstring(const char* string, const char* what);

// TODO: void printStack(void);

unsigned long hash_string(const char* string);
unsigned long hash_string_bounded(const char* string, int length);
unsigned int hash_int(unsigned int x);

char* concat_cstrings(const char* str1, int str1_length, const char* str2, int str2_length, const char* alloc_string);
char* concat_null_terminated_cstrings(const char* str1, const char* str2, const char* alloc_string);
char* concat_multi_null_terminated_cstrings(int count, char** strings, const char* alloc_string);
char* concat_multi_cstrings(int count, char** strings, int lengths[], char* alloc_string);

char* find_interpreter_directory(void);
char* get_current_directory(void);
char* directory_from_path(char* path);

DECLARE_DYNAMIC_ARRAY(size_t, IntegerArray, integer_array)

DECLARE_DYNAMIC_ARRAY(char, CharacterArray, character_array)

#endif
