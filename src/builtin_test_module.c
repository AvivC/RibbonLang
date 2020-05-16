#include "builtin_test_module.h"
#include "common.h"
#include "ribbon_object.h"
#include "vm.h"

/* Functions for the _testing module, only for use in tests. Consider not creating _testing module in release build. */

bool builtin_test_demo_print(Object* self, ValueArray args, Value* out) {
    assert(object_value_is(args.values[0], OBJECT_FUNCTION));
    ObjectFunction* function = (ObjectFunction*) args.values[0].as.object;

    printf("I'm a native function\n");

    ValueArray func_args;
    value_array_init(&func_args);
    Value callback_out;
    CallResult func_exec_result = vm_call_object((Object*) function, func_args, &callback_out);
    value_array_free(&func_args);

    printf("I'm a native function\n");

    *out = MAKE_VALUE_NIL();
    return func_exec_result == CALL_RESULT_SUCCESS;
}

bool builtin_test_call_callback_with_args(Object* self, ValueArray args, Value* out) {
    assert(args.count == 3);

    assert(object_value_is(args.values[0], OBJECT_FUNCTION));
    ObjectFunction* callback = (ObjectFunction*) args.values[0].as.object;

    Value arg1 = args.values[1];
    Value arg2 = args.values[2];

    printf("I'm a native function\n");

    ValueArray callback_args;
    value_array_init(&callback_args);
    value_array_write(&callback_args, &arg1);
    value_array_write(&callback_args, &arg2);

    Value callback_out;
    CallResult func_exec_result = vm_call_object((Object*) callback, callback_args, &callback_out);
    
    value_array_free(&callback_args);

    if (func_exec_result == CALL_RESULT_SUCCESS) {
        *out = callback_out;
        return true;
    }

    *out = MAKE_VALUE_NIL();
    return false;
}

bool builtin_test_get_value_directly_from_object_attributes(Object* self, ValueArray args, Value* out) {
    /* Bypass the attribute lookup mechanism (mainly classes and descriptors), and get
       a value directly from an object's attribute table. Used to test some internals of the system. */

    assert(args.count == 2);
    assert(args.values[0].type == VALUE_OBJECT);
    assert(object_value_is(args.values[1], OBJECT_STRING));

    Object* object = args.values[0].as.object;
    ObjectString* attr_name = (ObjectString*) args.values[1].as.object;

    if (!load_attribute_bypass_descriptors(object, attr_name, out) || is_value_instance_of_class(*out, "Descriptor")) {
        /* Not super elegant, but fine for now. The tests are based on stdout reading anyway. They look for this when appropriate. */
        printf("Attribute not found\n"); 
        *out = MAKE_VALUE_NIL();
        return true;
    }

    return true;
}

bool builtin_test_same_object(Object* self, ValueArray args, Value* out) {
    if (args.count != 2) {
        return false;
    }
    if (args.values[0].type != VALUE_OBJECT || args.values[1].type != VALUE_OBJECT) {
        return false;
    }
    *out = MAKE_VALUE_BOOLEAN(args.values[0].as.object == args.values[1].as.object);
    return true;
}

bool builtin_test_get_object_address(Object* self, ValueArray args, Value* out) {
    if (args.count != 1) {
        return false;
    }
    if (args.values[0].type != VALUE_OBJECT) {
        return false;
    }
    *out = MAKE_VALUE_NUMBER((uintptr_t) args.values[0].as.object);
    return true;
}

/* Used to invoke the gc in very specific tests using the _testing module - use for nothing else. */
bool builtin_test_gc(Object* self, ValueArray args, Value* out) {
    vm_gc();
    *out = MAKE_VALUE_NIL();
    return true;
}

bool builtin_test_table_details(Object* self, ValueArray args, Value* out) {
    if (!object_value_is(args.values[0], OBJECT_TABLE)) {
        return false;
    }

    Table table = ((ObjectTable*) args.values[0].as.object)->table;

    Table result = table_new_empty();
    table_set(&result, MAKE_VALUE_OBJECT(object_string_copy_from_null_terminated("num_entries")),
        MAKE_VALUE_NUMBER(table.num_entries));
    table_set(&result, MAKE_VALUE_OBJECT(object_string_copy_from_null_terminated("count")),
        MAKE_VALUE_NUMBER(table.count));
    table_set(&result, MAKE_VALUE_OBJECT(object_string_copy_from_null_terminated("capacity")),
        MAKE_VALUE_NUMBER(table.capacity));
    table_set(&result, MAKE_VALUE_OBJECT(object_string_copy_from_null_terminated("collision_count")),
        MAKE_VALUE_NUMBER(table.collision_count));

    *out = MAKE_VALUE_OBJECT(object_table_new(result));
    return true;
}

bool builtin_test_table_delete(Object* self, ValueArray args, Value* out) {
    if (!object_value_is(args.values[0], OBJECT_TABLE)) {
        return false;
    }

    ObjectTable* table = (ObjectTable*) args.values[0].as.object;
    Value key = args.values[1];

    table_delete(&table->table, key);

    *out = MAKE_VALUE_NIL();
    return true;
}