#ifndef plane_io_h
#define plane_io_h

#include <windows.h>
#include "common.h"

typedef enum {
	IO_SUCCESS,
	IO_OPEN_FILE_FAILURE,
	IO_READ_FILE_FAILURE,
	IO_CLOSE_FILE_FAILURE
} IOResult;

IOResult io_read_file(const char* file_name, const char* alloc_string, char** text_out, size_t* text_length_out);
BOOL io_file_exists(LPCTSTR path);

#endif
