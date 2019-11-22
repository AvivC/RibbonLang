// #include <string.h>

// #include "table.h"
// #include "object.h"
// #include "memory.h"
// #include "common.h"
// #include "value.h"

// #define MAX_LOAD_FACTOR 0.75

// static bool keys_equal(Value v1, Value v2) {
//     int compare_result = -1;
//     bool compare_success = value_compare(v1, v2, &compare_result);
//     return compare_success && compare_result == 0;
// }

// static void grow_table(TableNew* table) {
//     DEBUG_MEMORY("Growing table.");
    
//     int old_capacity = table->capacity;
//     Node** old_entries = table->entries;
    
//     table->capacity = GROW_CAPACITY(table->capacity);
//     table->entries = allocate(sizeof(Node*) * table->capacity, "Hash table array");
//     table->bucket_count = 0;
    
//     for (int i = 0; i < table->capacity; i++) {
//         table->entries[i] = NULL;
//     }

//     DEBUG_MEMORY("Old capacity: %d. New capacity: %d", old_capacity, table->capacity);
    
//     for (int i = 0; i < old_capacity; i++) {
//         Node* old_entry = old_entries[i];
        
//         if (old_entry == NULL) {
//             continue;
//         }
        
//         table_set_value_directly(table, old_entry->key, old_entry->value);
//     }
    
//     DEBUG_MEMORY("Deallocating old table array.");
//     deallocate(old_entries, sizeof(Node*) * old_capacity, "Hash table array");
// }

// void table_init(TableNew* table) {
//     table->bucket_count = 0;
//     table->capacity = 0;
//     // table->collisions_counter = 0;
//     table->entries = NULL;
// }

// void table_set_cstring_key(TableNew* table, const char* key, Value value) {
// 	// Value copied_key = MAKE_VALUE_OBJECT(object_string_copy_from_null_terminated(key)); // Yeah, not really efficient this stuff...

//     // if (table->bucket_count + 1 > table->capacity * MAX_LOAD_FACTOR) {
//     //     grow_table(table);
//     // }
    
//     // DEBUG_MEMORY("Finding entry '%s' in hash table.", key);
//     // Entry* entry = find_entry(table, &copied_key, true);

//     // if (entry->key.type == VALUE_NIL) {
//     //     table->bucket_count++;
//     // }

//     // entry->key = copied_key; // If it's not NULL (nil?), we're needlessly overriding the same key
//     // entry->value = value;

//     Value copied_key = MAKE_VALUE_OBJECT(object_string_copy_from_null_terminated(key)); // Yeah, not really efficient this stuff...
//     table_set_value_directly(table, copied_key, value);
// }

// void table_set(TableNew* table, struct Value key, Value value) {
//     table_set_value_directly(table, key, value);
// }

// void table_set_value_directly(TableNew* table, struct Value key, Value value) {
//     if (table->bucket_count + 1 > table->capacity * MAX_LOAD_FACTOR) {
//         grow_table(table);
//     }

//     unsigned long hash;
//     if (!value_hash(&key, &hash)) {
//     	// TODO: Report error
//     }

//     int slot = hash % table->capacity;
//     Node* root_node = table->entries[slot];
//     Node* node = root_node;

//     if (root_node == NULL) {
//         table->bucket_count++;
//     }

//     while (node != NULL) {
//         if (keys_equal(node->key, key)) {
//             node->value = value;
//             break;
//         }

//         node = node->next;
//     }

//     if (node == NULL) {
//         Node* new_node = allocate(sizeof(Node), "Table linked list node");
//         new_node->key = key;
//         new_node->value = value;

//         new_node->next = root_node;
//         table->entries[slot] = new_node;
//     }
// }

// bool table_get_cstring_key(TableNew* table, const char* key, Value* out) {
//     // if (table->entries == NULL) {
//     //     return false;
//     // }
    
//     // Entry* entry = find_entry(table, &MAKE_VALUE_OBJECT(object_string_copy_from_null_terminated(key)), false);
//     // if (entry->key.type == VALUE_NIL) {
//     //     return false;
//     // }
//     // *out = entry->value;
    
//     Value value_key = MAKE_VALUE_OBJECT(object_string_copy_from_null_terminated(key));
//     return table_get_value_directly(table, value_key, out);
// }

// bool table_get(TableNew* table, struct Value key, Value* out) {
// 	return table_get_value_directly(table, key, out);
// }

// bool table_get_value_directly(TableNew* table, Value key, Value* out) {
//     unsigned long hash;
//     if (!value_hash(&key, &hash)) {
//     	// TODO: Report error
//     }

//     int slot = hash % table->capacity;
//     Node* root_node = table->entries[slot];
//     Node* node = root_node;

//     while (node != NULL) {
//         if (keys_equal(node->key, key)) {
//             *out = node->value;
//             return true;
//         }

//         node = node->next;
//     }

//     return false;
// }

// /* Get a PointerArray of Value* of all set entries in the table. */
// PointerArray table_iterate(TableNew* table) {
// 	PointerArray array;
// 	pointer_array_init(&array, "table_iterate pointer array buffer");

// 	// for (int i = 0; i < table->capacity; i++) {
// 	// 	Entry* entry = &table->entries[i];

// 	// 	if (entry->key.type != VALUE_NIL) {
// 	// 		pointer_array_write(&array, entry);
// 	// 	} else if (entry->value.type != VALUE_NIL) {
// 	// 		FAIL("Found entry in table where key is nil but value is non-nil. Its type: %d", entry->value.type);
// 	// 	}
// 	// }

//     for (int i = 0; i < table->capacity; i++) {
//         Node* node = table->entries[i];
//         while (node != NULL) {
//             pointer_array_write(&array, node);
//         }
//     }

// 	return array;
// }

// void table_print(TableNew* table) {
// 	PointerArray entries = table_iterate(table);

// 	printf("[");
// 	for (int i = 0; i < entries.count; i++) {
// 		Node* entry = entries.values[i];
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

// void table_print_debug(TableNew* table) {
//     // printf("Capacity: %d \nCount: %d \nCollisions: %d \n", table->capacity, table->count, table->collisions_counter);
//     printf("Bucket capacity: %d \nBucket count: %d \n", table->capacity, table->bucket_count);
    
//     PointerArray entries = table_iterate(table);
//     if (table->capacity > 0) {
//     	printf("Data: \n");
// 		for (int i = 0; i < entries.count; i ++) {
// 			Node* entry = entries.values[i];
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

// void table_free(TableNew* table) {
//     deallocate(table->entries, sizeof(Node*) * table->capacity, "Hash table array");
//     table_init(table);
// }

// TableNew table_new_empty(void) {
// 	TableNew table;
// 	table_init(&table);
// 	return table;
// }
