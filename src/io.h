#ifndef ribbon_io_h
#define ribbon_io_h

#include <windows.h>
#include "common.h"
#include "table.h"

typedef enum {
	IO_SUCCESS,
	IO_OPEN_FILE_FAILURE,
	IO_READ_FILE_FAILURE,
	IO_CLOSE_FILE_FAILURE,
	IO_WRITE_FILE_FAILURE,
	IO_DELETE_FILE_FAILURE
} IOResult;

IOResult io_read_text_file(const char* file_name, const char* alloc_string, char** text_out, size_t* text_length_out);
IOResult io_read_binary_file(const char* file_name, Table* data_out);
IOResult io_write_text_file(const char* file_name, const char* string);
IOResult io_write_binary_file(const char* file_name, Table* data);
IOResult io_delete_file(const char* file_name);
BOOL io_file_exists(LPCTSTR path);

#endif
