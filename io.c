#include <stdio.h>
#include <windows.h>

#include "io.h"
#include "memory.h"
#include "utils.h"

static IOResult read_file_cleanup_and_fail(char* data, size_t size, IOResult result, const char* alloc_string) {
	deallocate(data, size, "File content buffer");
	return result;
}

//IOResult io_read_file(const char* file_name, const char* alloc_string, char** text_out, size_t* text_length_out) {
//	/* TODO: Proper systematic error handling, instead of ad-hoc printing
//	 * TODO: Dedicated tests */
//
//    FILE* file = fopen(file_name, "rb");
////    FILE* file = fopen(file_name, "r");
//
//    if (file == NULL) {
//        return IO_OPEN_FILE_FAILURE;
//    }
//
//    fseek(file, 0, SEEK_END);
//    size_t file_size = ftell(file);
//    fseek(file, 0, SEEK_SET);
//
//    size_t buffer_size = sizeof(char) * file_size + 1;
//    char* buffer = allocate(buffer_size, alloc_string);
//    size_t bytes_read = fread(buffer, 1, file_size, file);
//
//    if (bytes_read != file_size) {
//        return read_file_cleanup_and_fail(buffer, buffer_size, IO_READ_FILE_FAILURE, alloc_string);
//    }
//
//    if (fclose(file) != 0) {
//        return read_file_cleanup_and_fail(buffer, buffer_size, IO_CLOSE_FILE_FAILURE, alloc_string);
//    }
//
//    buffer[file_size] = '\0';
//
//    *text_out = buffer;
//    *text_length_out = buffer_size;
//    return IO_SUCCESS;
//}

IOResult io_read_file(const char* file_name, const char* alloc_string, char** text_out, size_t* text_length_out) {
	/* TODO: Proper systematic error handling, instead of ad-hoc printing
	 * TODO: Dedicated tests */

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
		printf("\nB\n");
		goto cleanup;
    }


    char nullbyte = '\0'; // Work around C not allowing passing pointers to literals...
    character_array_write(&char_array, &nullbyte);

    *text_out = copy_null_terminated_cstring(char_array.values, alloc_string);
    *text_length_out = char_array.count;

    cleanup:
	character_array_free(&char_array);
	if (fclose(file) == EOF) {
		printf("\nC\n");
		result = IO_CLOSE_FILE_FAILURE;
	}

    return result;
}

BOOL io_file_exists(LPCTSTR path) {
	DWORD file_attributes = GetFileAttributes(path);
	return (file_attributes != INVALID_FILE_ATTRIBUTES && !(file_attributes & FILE_ATTRIBUTE_DIRECTORY));
}

