#include <stdio.h>
#include <math.h>
#include <windows.h>

#include "io.h"
#include "memory.h"
#include "ribbon_utils.h"
#include "vm.h"

IOResult io_read_text_file(const char* file_name, const char* alloc_string, char** text_out, size_t* text_length_out) {
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

IOResult io_read_binary_file(const char* file_name, Table* data_out) {
    FILE* file = fopen(file_name, "rb");

    if (file == NULL) {
        return IO_OPEN_FILE_FAILURE;
    }

    IntegerArray integer_array;
    integer_array_init(&integer_array);
    int byte;
    while ((byte = fgetc(file)) != EOF) {
        size_t cast = (size_t) byte;
    	integer_array_write(&integer_array, &cast);
    }

    IOResult result = IO_SUCCESS;

    if (!feof(file)) {
    	// EOF encountered because of an error
		result = IO_READ_FILE_FAILURE;
		goto cleanup;
    }

    *data_out = table_new_empty();

    for (size_t i = 0; i < integer_array.count; i++) {
        Value value = MAKE_VALUE_NUMBER(integer_array.values[i]);
        table_set(data_out, MAKE_VALUE_NUMBER(i), value);
    }

    RIBBON_ASSERT(data_out->num_entries == integer_array.count, "Data buffer from file and table have a different count");

    cleanup:
	integer_array_free(&integer_array);
	if (fclose(file) == EOF) {
		result = IO_CLOSE_FILE_FAILURE;
	}

    return result;
}

IOResult io_write_text_file(const char* file_name, const char* string) {
    IOResult result = IO_SUCCESS;

    FILE* file = fopen(file_name, "w");

    if (file == NULL) {
        result = IO_OPEN_FILE_FAILURE;
        goto cleanup;
    }

    int write_result = fputs(string, file);
    if (write_result < 0 || write_result == EOF) {
        result = IO_WRITE_FILE_FAILURE;
    }

    cleanup:
    fclose(file);
    return result;
}

IOResult io_write_binary_file(const char* file_name, Table* data) {
    IOResult result = IO_SUCCESS;

    FILE* file = fopen(file_name, "wb");

    if (file == NULL) {
        result = IO_OPEN_FILE_FAILURE;
        goto cleanup;
    }

    for (int i = 0; i < data->num_entries; i++) {
        Value value;

        if (!table_get(data, MAKE_VALUE_NUMBER(i), &value)) {
            result = IO_WRITE_FILE_FAILURE;
            goto cleanup;
        }

        if (value.as.number != floor(value.as.number)) {
            result = IO_WRITE_FILE_FAILURE;
            goto cleanup;
        }

        const int byte = (int) value.as.number;
        if (fputc(value.as.number, file) == EOF) {
            result = IO_WRITE_FILE_FAILURE;
            goto cleanup;
        }
    }

    cleanup:
    fclose(file);
    return result;
}

IOResult io_delete_file(const char* file_name) {
    if (remove(file_name) == 0) {
        return IO_SUCCESS;
    }
    return IO_DELETE_FILE_FAILURE;
}

BOOL io_file_exists(LPCTSTR path) {
	DWORD file_attributes = GetFileAttributes(path);
	return (file_attributes != INVALID_FILE_ATTRIBUTES && !(file_attributes & FILE_ATTRIBUTE_DIRECTORY));
}

