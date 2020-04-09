#include <stdio.h>

#include "vm.h"
#include "builtins.h"
#include "value.h"
#include "memory.h"
#include "plane_object.h"
#include "io.h"

bool builtin_print(Object* self, ValueArray args, Value* out) {
	value_print(args.values[0]);
	printf("\n");
	fflush(stdout);

	*out = MAKE_VALUE_NIL();
	return true;
}

bool builtin_input(Object* self, ValueArray args, Value* out) {
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

bool builtin_read_file(Object* self, ValueArray args, Value* out) {
	if (!object_value_is(args.values[0], OBJECT_STRING)) {
		return false;
	}

	ObjectString* path = OBJECT_AS_STRING(args.values[0].as.object);

	char* file_data = NULL;
	size_t file_size = 0;

	// TODO: Is path->chars in this case guaranteed to be null terminated...?
	IOResult result = io_read_file(path->chars, "Object string buffer", &file_data, &file_size);

	switch (result) {
		case IO_SUCCESS: {
			*out = MAKE_VALUE_OBJECT(object_string_take(file_data, file_size - 1)); // file_size already includes the null byte, so we decrement it
			return true;
		}
		case IO_OPEN_FILE_FAILURE: {
			*out = MAKE_VALUE_NIL();
			return false;
		}
		case IO_READ_FILE_FAILURE: {
			*out = MAKE_VALUE_NIL();
			return false;
		}
		case IO_CLOSE_FILE_FAILURE: {
			*out = MAKE_VALUE_NIL();
			return false;
		}
	}

	FAIL("Should be unreachable.");
	return false;
}
