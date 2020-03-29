#include <stdio.h>
#include <windows.h>

#include "io.h"
#include "memory.h"
#include "plane_utils.h"

IOResult io_read_file(const char* file_name, const char* alloc_string, char** text_out, size_t* text_length_out) {
    FILE* file = fopen(file_name, "r");

    if (file == NULL) {
        return IO_OPEN_FILE_FAILURE;
    }

    CharacterArray char_array;
    character_array_init(&char_array);
    int ch;
    while ((ch = fgetc(file)) != EOF) {
    	char cast_char = (char) ch;
    	character_array_write(&char_array, &cast_char);
    }

    IOResult result = IO_SUCCESS;

    if (!feof(file)) {
    	// EOF encountered because of an error
		result = IO_READ_FILE_FAILURE;
		goto cleanup;
    }


    char nullbyte = '\0'; // Work around C not allowing passing pointers to literals...
    character_array_write(&char_array, &nullbyte);

    *text_out = copy_null_terminated_cstring(char_array.values, alloc_string);
    *text_length_out = char_array.count;

    cleanup:
	character_array_free(&char_array);
	if (fclose(file) == EOF) {
		result = IO_CLOSE_FILE_FAILURE;
	}

    return result;
}

BOOL io_file_exists(LPCTSTR path) {
	DWORD file_attributes = GetFileAttributes(path);
	return (file_attributes != INVALID_FILE_ATTRIBUTES && !(file_attributes & FILE_ATTRIBUTE_DIRECTORY));
}

