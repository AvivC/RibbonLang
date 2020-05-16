#include <string.h>
#include <windows.h>

#include "plane_utils.h"
#include "memory.h"
#include "value.h"
#include "plane_object.h"

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
	/* TODO: This wasn't tested throughly after converting it from the regular hash_string.
	   General tests of course pass, but possibly this doesn't really hash correctly etc.
	   Possibly look into this in the future. */

	unsigned long hash = 5381;
	int c;

	for (const char* p = string; p - string < length; p++) {
		c = *p;
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}

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

bool cstrings_equal(const char* s1, int length1, const char* s2, int length2) {
	return length1 == length2 && (strncmp(s1, s2, length1) == 0);
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
	char* last_slash = NULL;

	char* last_back_slash = strrchr(path, '\\');
	char* last_forward_slash = strrchr(path, '/');

	if (last_back_slash == NULL && last_forward_slash != NULL) {
		last_slash = last_forward_slash;
	} else if (last_forward_slash == NULL && last_back_slash != NULL) {
		last_slash = last_back_slash;
	} else if (last_forward_slash == NULL && last_back_slash == NULL) {
		return NULL;
	} else {
		if (last_back_slash - path > last_forward_slash - path) {
			last_slash = last_back_slash;
		} else {
			last_slash = last_forward_slash;
		}
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

char* get_current_working_directory(void) {
	LPTSTR dir = allocate(MAX_PATH, "working directory path");
	DWORD result = GetCurrentDirectory(MAX_PATH, dir);
	if (result == 0 || result >= MAX_PATH - 1) {
		return NULL;
	}
	return reallocate(dir, MAX_PATH, strlen(dir) + 1, "working directory path");
}

char* concat_null_terminated_paths(char* p1, char* p2, char* alloc_string) {
	return concat_multi_null_terminated_cstrings(3, (char*[]) {p1, "\\", p2}, alloc_string);
}

static bool argument_matches(Value value, const char** c) {
	bool matching = true;

	int type_length = strcspn(*c, " |");

	if (**c == 'n') {
		if (value.type != VALUE_NUMBER) {
			matching = false;
		}
	} else if (**c == 'b') {
		if (value.type != VALUE_BOOLEAN) {
			matching = false;
		}
	} else if (**c == 'i') {
		if (value.type != VALUE_NIL) {
			matching = false;
		}
	} else if (**c == 'o') {
		if (value.type == VALUE_OBJECT) {
			Object* object = value.as.object;
			
			if (cstrings_equal((*c) + 1, type_length - 1, "String", strlen("String"))) {
				if (object->type != OBJECT_STRING) {
					matching = false;
				}
			} else if (cstrings_equal((*c) + 1, type_length - 1, "Table", strlen("Table"))) {
				if (object->type != OBJECT_TABLE) {
					matching = false;
				}
			} else if (cstrings_equal((*c) + 1, type_length - 1, "Function", strlen("Function"))) {
				if (object->type != OBJECT_FUNCTION) {
					matching = false;
				}
			} else if (cstrings_equal((*c) + 1, type_length - 1, "Module", strlen("Module"))) {
				if (object->type != OBJECT_MODULE) {
					matching = false;
				}
			} else if (cstrings_equal((*c) + 1, type_length - 1, "Class", strlen("Class"))) {
				if (object->type != OBJECT_CLASS) {
					matching = false;
				}
			} else {
				char* class_name = copy_cstring((*c) + 1, type_length - 1, "Validator class name string");
				if (!is_instance_of_class(object, class_name)) {
					matching = false;
				}
				deallocate(class_name, strlen(class_name) + 1, "Validator class name string");
			}
		} else {
			matching = false;
		}
	} else {
		return false; /* Illegal format string, might as well fail the caller */
	}

	*c += type_length;

	if (matching && **c == '|') {
		while (**c != ' ' && **c != '\0') {
			(*c)++;
		}
	}

	return matching;
}

static bool match_character(const char** c, char expected) {
	if (**c == expected) {
		(*c)++;
		return true;
	}
	return false;
}

bool arguments_valid(ValueArray args, const char* string) {
	const char* c = string;

	int i = 0;
	while (*c != '\0') {
		while (*c == ' ') {
			c++;
		}

		Value value = args.values[i];
		
		bool matching;
		do {
			matching = argument_matches(value, &c);
		}  while (!matching && match_character(&c, '|'));

		if (!matching) {
			return false;
		}

		i++;
	}

	if (i < args.count) { 
		return false;
	}

	return true;
}

IMPLEMENT_DYNAMIC_ARRAY(size_t, IntegerArray, integer_array)

IMPLEMENT_DYNAMIC_ARRAY(char, CharacterArray, character_array)
