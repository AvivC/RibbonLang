#include <stdio.h>

#include "builtins.h"
#include "value.h"
#include "memory.h"
#include "object.h"

bool builtin_print(ValueArray args, Value* out) {
	printValue(args.values[0]);
	printf("\n");
	fflush(stdout);

	*out = MAKE_VALUE_NIL();
	return true;
}

bool builtin_input(ValueArray args, Value* out) {
	// TODO: Error handling

	int character = 0;
	int length = 0;
	int capacity = 10;
	const char* alloc_string = "User input buffer";

	char* user_input = allocate(sizeof(char) * capacity, alloc_string);

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
	ObjectString* obj_string = takeString(user_input, length);

	*out = MAKE_VALUE_OBJECT(obj_string);
	return true;
}
