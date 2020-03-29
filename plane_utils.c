#include <string.h>

#include "plane_utils.h"
#include "memory.h"
#include <windows.h>
// #include <dbghelp.h>

uint16_t two_bytes_to_short(uint8_t a, uint8_t b) {
	return (a << 8) + b;
}

void short_to_two_bytes(uint16_t num, uint8_t* bytes_out) {
	bytes_out[0] = (num >> 8) & 0xFF;
	bytes_out[1] = num & 0xFF;
}

char* copy_cstring(const char* string, int length, const char* what) {
	// argument length should not include the null-terminator
	DEBUG_OBJECTS_PRINT("Allocating string buffer '%.*s' of length %d.", length, string, length);

    char* chars = allocate(sizeof(char) * length + 1, what);
    memcpy(chars, string, length);
    chars[length] = '\0';

    return chars;
}

char* copy_null_terminated_cstring(const char* string, const char* what) {
	return copy_cstring(string, strlen(string), what);
}

unsigned long hash_string(const char* string) {
	unsigned long hash = 5381;
	int c;

	while ((c = *string++)) {
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}

	return hash;
}

unsigned long hash_string_bounded(const char* string, int length) {
	// Inefficient approach, but fine for now.
	char* alloc_string = "Bounded string for hashing";
	char* null_terminated = copy_cstring(string, length, alloc_string);
	unsigned long hash = hash_string(null_terminated);
	deallocate(null_terminated, sizeof(char) * strlen(null_terminated) + 1, alloc_string);
	return hash;
}

unsigned int hash_int(unsigned int x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

char* concat_cstrings(const char* str1, int str1_length, const char* str2, int str2_length, const char* alloc_string) {
	size_t file_name_buffer_size = str1_length + str2_length + 1;
	char* result = allocate(file_name_buffer_size, alloc_string);
	memcpy(result, str1, str1_length);
	memcpy(result + str1_length, str2, str2_length);
	result[str1_length + str2_length] = '\0';

	return result;
}

char* concat_null_terminated_cstrings(const char* str1, const char* str2, const char* alloc_string) {
	return concat_cstrings(str1, strlen(str1), str2, strlen(str2), alloc_string);
}

char* concat_multi_null_terminated_cstrings(int count, char** strings, const char* alloc_string) {
	char* result = allocate(strlen(strings[0]) + 1, alloc_string);
	result[strlen(strings[0])] == '\0';
	strcpy(result, strings[0]);

	for (int i = 1; i < count; i++) {
		char* string = strings[i];
		char* old_result = result;
		result = concat_null_terminated_cstrings(result, string, alloc_string);
		deallocate(old_result, strlen(old_result) + 1, alloc_string);
	}

	return result;
}

char* concat_multi_cstrings(int count, char** strings, int lengths[], char* alloc_string) {
	char* result = allocate(lengths[0] + 1, alloc_string);
	memcpy(result, strings[0], lengths[0]);
	result[lengths[0]] == '\0';

	int length_sum = lengths[0];

	for (int i = 1; i < count; i++) {
		char* string = strings[i];
		int length = lengths[i];

		char* old_result = result;
		result = concat_cstrings(result, length_sum, string, length, alloc_string);
		deallocate(old_result, length_sum + 1, alloc_string);

		length_sum += length;
	}

	return result;
}

char* find_interpreter_directory(void) {
	char* dir_path = NULL;

	/* TODO: Use Windows MAX_PATH instead */
	DWORD MAX_LENGTH = 500;
	char* exec_path = allocate(MAX_LENGTH, "interpreter executable path");

	DWORD get_module_name_result = GetModuleFileNameA(NULL, exec_path, MAX_LENGTH);
	if (get_module_name_result == 0 || get_module_name_result == MAX_LENGTH) {
		goto cleanup;
	}

	char* last_slash;
	if ((last_slash = strrchr(exec_path, '\\')) == NULL) {
		goto cleanup;
	}

	int directory_length = last_slash - exec_path;
	dir_path = allocate(directory_length + 1, "interpreter directory path");
	memcpy(dir_path, exec_path, directory_length);
	dir_path[directory_length] = '\0';

	cleanup:
	deallocate(exec_path, MAX_LENGTH, "interpreter executable path");

	return dir_path;
}

char* directory_from_path(char* path) {
	char* last_slash;
	if ((last_slash = strrchr(path, '\\')) == NULL) {
		return NULL;
	}

	int directory_length = last_slash - path;
	char* dir_path = allocate(directory_length + 1, "directory path");

	if (dir_path == NULL) {
		return NULL;
	}

	memcpy(dir_path, path, directory_length);
	dir_path[directory_length] = '\0';

	return dir_path;
}

char* get_current_directory(void) {
	LPTSTR dir = allocate(MAX_PATH, "working directory path");
	DWORD result = GetCurrentDirectory(MAX_PATH, dir);
	if (result == 0 || result >= MAX_PATH - 1) {
		return NULL;
	}
	return reallocate(dir, MAX_PATH, strlen(dir) + 1, "working directory path");
}

IMPLEMENT_DYNAMIC_ARRAY(size_t, IntegerArray, integer_array)

IMPLEMENT_DYNAMIC_ARRAY(char, CharacterArray, character_array)

// TODO: Make this work.

// void printStack( void )
// {
//     unsigned int   i;
//     void         * stack[ 100 ];
//     unsigned short frames;
//     SYMBOL_INFO  * symbol;
//     HANDLE         process;

//     process = GetCurrentProcess();

//     SymInitialize( process, NULL, TRUE );

//     frames               = CaptureStackBackTrace( 0, 100, stack, NULL );
//     symbol               = ( SYMBOL_INFO * )calloc( sizeof( SYMBOL_INFO ) + 256 * sizeof( char ), 1 );
//     symbol->MaxNameLen   = 255;
//     symbol->SizeOfStruct = sizeof( SYMBOL_INFO );

//     for( i = 0; i < frames; i++ )
//     {
//         SymFromAddr( process, ( DWORD64 )( stack[ i ] ), 0, symbol );

//         printf( "%i: %s - 0x%0X\n", frames - i - 1, symbol->Name, symbol->Address );
//     }

//     free( symbol );
// }
