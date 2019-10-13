#include <stdio.h>

#include "builtins.h"
#include "value.h"
#include "memory.h"
#include "object.h"

bool builtin_print(ValueArray args, Value* out) {
	value_print(args.values[0]);
	printf("\n");
	fflush(stdout);

	*out = MAKE_VALUE_NIL();
	return true;
}

bool builtin_input(ValueArray args, Value* out) {
	// TODO: Error handling
	// TODO: Write dedicated tests

	int character = 0;
	int length = 0;
	int capacity = 10;
	const char* alloc_string = "Object string buffer";

	char* user_input = allocate(sizeof(char) * capacity, alloc_string);

	if (user_input == NULL) {
		return false;
	}

	while ((character = getchar()) != '\n') {
		user_input[length++] = character;
		if (length == capacity - 1) {
			int old_capacity = capacity;
			capacity = GROW_CAPACITY(old_capacity);
			user_input = reallocate(user_input, sizeof(char) * old_capacity, sizeof(char) * capacity, alloc_string);
		}
	}

	user_input[length] = '\0';
	user_input = reallocate(user_input, sizeof(char) * capacity, sizeof(char) * length + 1, alloc_string);
	ObjectString* obj_string = object_string_take(user_input, length);

	*out = MAKE_VALUE_OBJECT(obj_string);
	return true;
}

bool builtin_read_file(ValueArray args, Value* out) {
	// TODO: Proper systematic error handling, instead of ad-hoc printing
	// TODO: Dedicated tests

	if (!object_is_value_object_of_type(args.values[0], OBJECT_STRING)) {
		return false;
	}

	ObjectString* path = OBJECT_AS_STRING(args.values[0].as.object);

    FILE* file = fopen(path->chars, "rb");

    if (file == NULL) {
        fprintf(stderr, "Couldn't open file '%s'. Error:\n'", path->chars);
        perror("fopen");
        fprintf(stderr, "'\n");
        return false;
    }

    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = allocate(sizeof(char) * fileSize + 1, "Object string buffer");
    size_t bytesRead = fread(buffer, 1, fileSize, file);

    if (bytesRead != fileSize) {
        fprintf(stderr, "Couldn't read entire file. File size: %d. Bytes read: %d.\n", fileSize, bytesRead);
        return false;
    }

    if (fclose(file) != 0) {
        fprintf(stderr, "Couldn't close file.\n");
        return false;
    }

    buffer[fileSize] = '\0';

    *out = MAKE_VALUE_OBJECT(object_string_take(buffer, fileSize));
    return true;
}
