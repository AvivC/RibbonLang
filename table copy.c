// #include <string.h>

// #include "table.h"
// #include "object.h"
// #include "memory.h"
// #include "common.h"
// #include "value.h"

// // TODO: External functions should return a boolean for success or failure, because e.g. not everything can be used
// // as a key because not everything is hashable.
// // Currently these edge cases may result in a segmentation fault (because find_entry will return NULL which will later be dereferenced).

// #define MAX_LOAD_FACTOR 0.75

// static bool keys_equal(Value v1, Value v2) {
//     int compare_result = -1;
//     bool compare_success = value_compare(v1, v2, &compare_result);
//     return compare_success && compare_result == 0;
// }

// static Entry* find_entry(Table* table, Value* key, bool settingValue) {
//     if (table->capacity == 0) {
//     	// Outer functions should guard against that
//     	FAIL("Illegal state: table capacity is 0.");
//     }
    
//     unsigned long hash;
//     if (!value_hash(key, &hash)) {
//     	return NULL; // Value is unhashable
//     }

//     int slot = hash % table->capacity;
    
//     int nils = 0;
//     for (int i = 0; i < table->capacity; i++) {
//         if (table->entries[i].key.type == VALUE_NIL) {
//             nils++;
//         }
//     }
    
//     Entry* entry = &table->entries[slot];
//     while (entry->key.type != VALUE_NIL && !keys_equal(entry->key, *key)) {
//         if (settingValue) {
//             table->collisions_counter++;
//         }
        
//         entry = &table->entries[++slot % table->capacity];
//     }
    
//     return entry;
// }

// static void grow_table(Table* table) {
//     DEBUG_MEMORY("Growing table.");
    
//     int oldCapacity = table->capacity;
//     Entry* oldEntries = table->entries;
    
//     table->capacity = GROW_CAPACITY(table->capacity);
//     table->entries = allocate(sizeof(Entry) * table->capacity, "Hash table array");
    
//     DEBUG_MEMORY("Old capacity: %d. New capacity: %d", oldCapacity, table->capacity);
    
//     for (int i = 0; i < table->capacity; i++) {
//         table->entries[i] = (Entry) {.key = MAKE_VALUE_NIL(), .value = MAKE_VALUE_NIL()};
//     }
    
//     for (int i = 0; i < oldCapacity; i++) {
//         Entry* oldEntry = &oldEntries[i];
        
//         if (oldEntry->key.type == VALUE_NIL) {
//             continue;
//         }
        
//         Entry* newEntry = find_entry(table, &oldEntry->key, false); // TODO: Check for NULL result
//         newEntry->key = oldEntry->key;
//         newEntry->value = oldEntry->value;
//     }
    
//     DEBUG_MEMORY("Deallocating old table array.");
//     deallocate(oldEntries, sizeof(Entry) * oldCapacity, "Hash table array");
// }

// void table_init(Table* table) {
//     table->count = 0;
//     table->capacity = 0;
//     table->collisions_counter = 0;
//     table->entries = NULL;
// }

// void table_set_cstring_key(Table* table, const char* key, Value value) {
// 	Value copied_key = MAKE_VALUE_OBJECT(object_string_copy_from_null_terminated(key)); // Yeah, not really efficient this stuff...

//     if (table->count + 1 > table->capacity * MAX_LOAD_FACTOR) {
//         grow_table(table);
//     }
    
//     DEBUG_MEMORY("Finding entry '%s' in hash table.", key);
//     Entry* entry = find_entry(table, &copied_key, true);

//     if (entry->key.type == VALUE_NIL) {
//         table->count++;
//     }

//     entry->key = copied_key; // If it's not NULL (nil?), we're needlessly overriding the same key
//     entry->value = value;
// }

// // void table_set(Table* table, struct Value key, Value value) {
// // 	ObjectString* key_string = VALUE_AS_OBJECT(key, OBJECT_STRING, ObjectString);
// // 	table_set_cstring_key(table, key_string->chars, value);
// // }

// void table_set(Table* table, struct Value key, Value value) {
//     table_set_value_directly(table, key, value);
// }

// void table_set_value_directly(Table* table, struct Value key, Value value) {
//     if (table->count + 1 > table->capacity * MAX_LOAD_FACTOR) {
//         grow_table(table);
//     }

//     DEBUG_MEMORY("Finding entry '%s' in hash table.", key);
//     Entry* entry = find_entry(table, &key, true);

//     if (entry->key.type == VALUE_NIL) {
//         table->count++;
//     }

//     entry->key = key; // If it's not NULL (nil?), we're needlessly overriding the same key
//     entry->value = value;
// }

// bool table_get_cstring_key(Table* table, const char* key, Value* out) {
//     if (table->entries == NULL) {
//         return false;
//     }
    
//     Entry* entry = find_entry(table, &MAKE_VALUE_OBJECT(object_string_copy_from_null_terminated(key)), false);
//     if (entry->key.type == VALUE_NIL) {
//         return false;
//     }
//     *out = entry->value;
//     return true;
// }

// // bool table_get(Table* table, struct Value key, Value* out) {
// // 	ObjectString* key_string = VALUE_AS_OBJECT(key, OBJECT_STRING, ObjectString);
// //     return table_get_cstring_key(table, key_string->chars, out);
// // }

// bool table_get(Table* table, struct Value key, Value* out) {
// 	return table_get_value_directly(table, key, out);
// }

// bool table_get_value_directly(Table* table, Value key, Value* out) {
//     if (table->entries == NULL) {
//         return false;
//     }

//     Entry* entry = find_entry(table, &key, false);
//     if (entry->key.type == VALUE_NIL) {
//         return false;
//     }
//     *out = entry->value;
//     return true;
// }

// /* Get a PointerArray of Value* of all set entries in the table. */
// PointerArray table_iterate(Table* table) {
// 	PointerArray array;
// 	pointer_array_init(&array, "table_iterate pointer array buffer");

// 	for (int i = 0; i < table->capacity; i++) {
// 		Entry* entry = &table->entries[i];

// 		if (entry->key.type != VALUE_NIL) {
// 			pointer_array_write(&array, entry);
// 		} else if (entry->value.type != VALUE_NIL) {
// 			FAIL("Found entry in table where key is nil but value is non-nil. Its type: %d", entry->value.type);
// 		}
// 	}

// 	return array;
// }

// void table_print(Table* table) {
// 	PointerArray entries = table_iterate(table);

// 	printf("[");
// 	for (int i = 0; i < entries.count; i++) {
// 		Entry* entry = entries.values[i];
// 		Value value = entry->value;
// 		value_print(entry->key);
// 		printf(": ");
// 		value_print(value);

// 		if (i < entries.count - 1) {
// 			printf(", ");
// 		}
// 	}
// 	printf("]");

// 	pointer_array_free(&entries);
// }

// void table_print_debug(Table* table) {
//     printf("Capacity: %d \nCount: %d \nCollisions: %d \n", table->capacity, table->count, table->collisions_counter);
    
//     if (table->capacity > 0) {
//     	printf("Data: \n");
// 		for (int i = 0; i < table->capacity; i ++) {
// 			Entry* entry = &table->entries[i];
// 			printf("%d = [Key: ", i);
// 			value_print(entry->key);
// 			printf(", Value: ");
// 			value_print(entry->value);
// 			printf("]\n");
// 		}
//     } else {
//     	printf("Table is empty.\n");
//     }
// }

// void table_free(Table* table) {
//     deallocate(table->entries, sizeof(Entry) * table->capacity, "Hash table array");
//     table_init(table);
// }

// Table table_new_empty(void) {
// 	Table table;
// 	table_init(&table);
// 	return table;
// }
