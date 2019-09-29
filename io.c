#include "io.h"
#include "memory.h"

static IOResult read_file_cleanup_and_fail(char* data, size_t size, IOResult result) {
	deallocate(data, size, "File content buffer");
	return result;
}

IOResult read_file(const char* file_name, char** text_out, size_t* text_length_out) {
	// TODO: Proper systematic error handling, instead of ad-hoc printing
	// TODO: Dedicated tests

    FILE* file = fopen(file_name, "rb");

    if (file == NULL) {
        fprintf(stderr, "Couldn't open file '%s'. Error:\n'", file_name);
        perror("fopen");
        fprintf(stderr, "'\n");
        return IO_OPEN_FILE_FAILURE;
    }

    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    size_t buffer_size = sizeof(char) * fileSize + 1;
    char* buffer = allocate(buffer_size, "File content buffer");
    size_t bytesRead = fread(buffer, 1, fileSize, file);

    if (bytesRead != fileSize) {
        fprintf(stderr, "Couldn't read entire file. File size: %d. Bytes read: %d.\n", fileSize, bytesRead);
        return read_file_cleanup_and_fail(buffer, buffer_size, IO_READ_FILE_FAILURE);
    }

    if (fclose(file) != 0) {
        fprintf(stderr, "Couldn't close file.\n");
        return read_file_cleanup_and_fail(buffer, buffer_size, IO_CLOSE_FILE_FAILURE);
    }

    buffer[fileSize] = '\0';

    *text_out = buffer;
    *text_length_out = buffer_size;
    return IO_SUCCESS;
}
