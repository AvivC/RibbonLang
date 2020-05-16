#include <stdio.h>
#include <time.h>

#include "vm.h"
#include "builtins.h"
#include "value.h"
#include "memory.h"
#include "ribbon_object.h"
#include "io.h"

bool builtin_print(Object* self, ValueArray args, Value* out) {
	value_print(args.values[0]);
	printf("\n");

	*out = MAKE_VALUE_NIL();
	return true;
}

bool builtin_input(Object* self, ValueArray args, Value* out) {
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
		case IO_WRITE_FILE_FAILURE: {
			FAIL("IO_WRITE_FILE_FAILURE returned from io_read_file - shouldn't happen.");
			return false;
		}
		case IO_DELETE_FILE_FAILURE: {
			FAIL("IO_DELETE_FILE_FAILURE returned from io_read_file - shouldn't happen.");
			return false;
		}
	}

	FAIL("Should be unreachable.");
	return false;
}

bool builtin_write_file(Object* self, ValueArray args, Value* out) {
	bool success = true;

	assert(args.count == 2); /* Assertion because the function calling system should have raised a runtime error if incorrect
	                            number of arguments */
	if (!object_value_is(args.values[0], OBJECT_STRING) || !object_value_is(args.values[1], OBJECT_STRING)) {
		*out = MAKE_VALUE_NIL();
		return false;
	}

	ObjectString* file_name = (ObjectString*) args.values[0].as.object;
	ObjectString* text = (ObjectString*) args.values[1].as.object;

	char* file_name_bounded = copy_cstring(file_name->chars, file_name->length, "File name bounded");
	char* text_bounded = copy_cstring(text->chars, text->length, "File text bounded");
	if (io_write_file(file_name_bounded, text_bounded) != IO_SUCCESS) {
		success = false;
		goto cleanup;
	}

	cleanup:
	deallocate(file_name_bounded, strlen(file_name_bounded) + 1, "File name bounded");
	deallocate(text_bounded, strlen(text_bounded) + 1, "File text bounded");
	
	*out = MAKE_VALUE_NIL();
	return success;
}

bool builtin_file_exists(Object* self, ValueArray args, Value* out) {
	if (!object_value_is(args.values[0], OBJECT_STRING)) {
		*out = MAKE_VALUE_NIL();
		return false;
	}

	ObjectString* file_name = (ObjectString*) args.values[0].as.object;
	char* file_name_bounded = copy_cstring(file_name->chars, file_name->length, "File name bounded");
	bool exists = io_file_exists(file_name_bounded);
	deallocate(file_name_bounded, strlen(file_name_bounded) + 1, "File name bounded");

	*out = MAKE_VALUE_BOOLEAN(exists);
	return true;
}

bool builtin_delete_file(Object* self, ValueArray args, Value* out) {
	if (!object_value_is(args.values[0], OBJECT_STRING)) {
		*out = MAKE_VALUE_NIL();
		return false;
	}

	bool success = true;

	ObjectString* file_name = (ObjectString*) args.values[0].as.object;
	char* file_name_bounded = copy_cstring(file_name->chars, file_name->length, "File name bounded");
	if (io_delete_file(file_name_bounded) != IO_SUCCESS) {
		success = false;
	}

	deallocate(file_name_bounded, strlen(file_name_bounded) + 1, "File name bounded");
	*out = MAKE_VALUE_NIL();
	return success;
}

bool builtin_time(Object* self, ValueArray args, Value* out) {
	/* Get the number of millisecond since the program was launched.
	   Might be inexact and possibly have some odd issues.  */
	   
	*out = MAKE_VALUE_NUMBER(clock() / (CLOCKS_PER_SEC / 1000));
	return true;
}

bool builtin_to_number(Object* self, ValueArray args, Value* out) {
	bool success = true;

	Value arg = args.values[0];
	double number;

	if (object_value_is(arg, OBJECT_STRING)) {
		ObjectString* string = (ObjectString*) arg.as.object;
		char* file_name_bounded = copy_cstring(string->chars, string->length, "File name bounded");
		if (sscanf(file_name_bounded, "%lf", &number) == EOF) {
			success = false;
		} 
		deallocate(file_name_bounded, strlen(file_name_bounded) + 1, "File name bounded");
	} else {
		success = false;
	}

	*out = success ? MAKE_VALUE_NUMBER(number) : MAKE_VALUE_NIL();
	return success;
}

bool builtin_to_string(Object* self, ValueArray args, Value* out) {
	bool success = true;

	char* buffer = NULL;

	Value arg = args.values[0];
	if (arg.type == VALUE_NUMBER) {
		double number = arg.as.number;
		size_t string_length = snprintf(NULL, 0, "%g", number);
		buffer = allocate(string_length + 1, "to_string buffer");
		int ret = snprintf(buffer, string_length + 1, "%g", number);
		success = ret >= 0 && ret < string_length + 1;
	} else {
		success = false;
	}

	*out = success ? MAKE_VALUE_OBJECT(object_string_take(buffer, strlen(buffer))) : MAKE_VALUE_NIL();
	return success;
}

bool builtin_has_attr(Object* self, ValueArray args, Value* out) {
	if (args.count != 2 || !(args.values[0].type == VALUE_OBJECT && object_value_is(args.values[1], OBJECT_STRING))) {
		return false;
	}

	Object* object = args.values[0].as.object;
	ObjectString* attr = (ObjectString*) args.values[1].as.object;
	Value throwaway;
	*out = MAKE_VALUE_BOOLEAN(object_load_attribute(object, attr, &throwaway));
	return true;
}

bool builtin_random(Object* self, ValueArray args, Value* out) {
	*out = MAKE_VALUE_NUMBER(rand());
	return true;
}

bool builtin_get_main_file_path(Object* self, ValueArray args, Value* out) {
	*out = MAKE_VALUE_OBJECT(object_string_copy_from_null_terminated(vm.main_module_path));
	return true;
}

bool builtin_get_main_file_directory(Object* self, ValueArray args, Value* out) {
	char* last_slash = strrchr(vm.main_module_path, '\\');

	if (last_slash == NULL) {
		/* Seems we're working with the file path directly, thus the current directory is the directory */
		*out = MAKE_VALUE_OBJECT(object_string_copy_from_null_terminated("."));
		return true;
	}

	*out = MAKE_VALUE_OBJECT(object_string_copy(vm.main_module_path, last_slash - vm.main_module_path));
	return true;
}

bool builtin_is_instance(Object* self, ValueArray args, Value* out) {
	if (!object_value_is(args.values[1], OBJECT_STRING)) {
		return false;
	}

	Value value = args.values[0];
	ObjectString* type_name = (ObjectString*) args.values[1].as.object;

	if (value.type == VALUE_OBJECT && is_instance_of_class(value.as.object, type_name->chars)) {
		*out = MAKE_VALUE_BOOLEAN(true);
		return true;
	}

	*out = MAKE_VALUE_BOOLEAN(strcmp(value_get_type(value), type_name->chars) == 0);
	return true;
}

bool builtin_get_type(Object* self, ValueArray args, Value* out) {
	*out = MAKE_VALUE_OBJECT(object_string_copy_from_null_terminated(value_get_type(args.values[0])));
	return true;
}

bool builtin_super(Object* self, ValueArray args, Value* out) {
	if (!object_value_is(args.values[0], OBJECT_TABLE)) {
		return false;
	}

	ObjectTable* args_table = (ObjectTable*) args.values[0].as.object;

	StackFrame* current_frame = vm_peek_previous_frame();
	if (current_frame->is_native) {
		/* Currently calling super() in native methods unsupported. */
		return false;
	}

	Value object_val;
	if (!cell_table_get_value_cstring_key(&current_frame->local_variables, "self", &object_val)) {
		return false;
	}

	assert(object_value_is(object_val, OBJECT_INSTANCE));

	ObjectInstance* object = (ObjectInstance*) object_val.as.object;

	const char* function_name = current_frame->function->name;

	ObjectClass* superclass = object->klass->superclass;
	if (superclass == NULL) {
		return false;
	}

	Value superclass_function_val;
	if (!object_load_attribute_cstring_key((Object*) superclass, function_name, &superclass_function_val)) {
		return false;
	}

	if (!object_value_is(superclass_function_val, OBJECT_FUNCTION)) {
		return false;
	}

	ObjectFunction* superclass_function = (ObjectFunction*) superclass_function_val.as.object;

	ObjectBoundMethod* bound_method = object_bound_method_new(superclass_function, (Object*) object);

	bool success = true;

	ValueArray length_args = value_array_make(0, NULL);
	Value length_val;
	if (vm_call_attribute_cstring((Object*) args_table, "length", length_args, &length_val) != CALL_RESULT_SUCCESS) {
		success = false;
		goto cleanup;
	}

	assert(length_val.type == VALUE_NUMBER);

	ValueArray args_for_super_function;
	value_array_init(&args_for_super_function);

	for (int i = 0; i < length_val.as.number; i++) {
		ValueArray get_key_args = value_array_make(1, (Value[]) {MAKE_VALUE_NUMBER(i)});
		Value arg;
		if (vm_call_attribute_cstring((Object*) args_table, "@get_key", get_key_args, &arg) != CALL_RESULT_SUCCESS) {
			success = false;
		}
		value_array_free(&get_key_args);

		if (!success) {
			goto cleanup;
		}

		value_array_write(&args_for_super_function, &arg);
	}

	CallResult result = vm_call_bound_method(bound_method, args_for_super_function, out);
	success = result == CALL_RESULT_SUCCESS;

	cleanup:
	value_array_free(&args_for_super_function);
	value_array_free(&length_args);

	return success;
}
