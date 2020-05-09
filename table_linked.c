// #include <string.h>

// #include "table.h"
// #include "plane_object.h"
// #include "memory.h"
// #include "common.h"
// #include "value.h"

// #if DEBUG_TABLE_STATS

// /* For debugging */
// static size_t times_called = 0;
// static size_t capacity_sum = 0;
// static size_t max_capacity = 0;
// static size_t bucket_sum = 0;
// static size_t avg_bucket_count = 0;
// static size_t entries_sum = 0;
// static size_t avg_entries_sum = 0;
// static double avg_capacity = 0;
// /* ..... */

// void table_debug_print_general_stats(void) {
//     printf("Times called: %" PRI_SIZET "\n", times_called);
//     printf("Sum capacity: %" PRI_SIZET "\n", capacity_sum);
//     printf("Avg capacity: %g\n", avg_capacity);
//     printf("Max capacity: %" PRI_SIZET "\n", max_capacity);
//     printf("Sum buckets: %" PRI_SIZET "\n", bucket_sum);
//     printf("Avg bucket count: %" PRI_SIZET "\n", avg_bucket_count);
//     printf("Entries sum: %" PRI_SIZET "\n", entries_sum);
//     printf("Avg entries: %" PRI_SIZET "\n", avg_entries_sum);
// }

// #endif

// #define MAX_LOAD_FACTOR 0.75

// static void* allocate_suitably(Table* table, size_t size, const char* what) {
//     if (!table->is_memory_infrastructure) {
//         return allocate(size, what);
//     }
//     return allocate_no_tracking(size);
// }

// static void deallocate_suitably(Table* table, void* pointer, size_t size, const char* what) {
//     if (!table->is_memory_infrastructure) {
//         deallocate(pointer, size, what);
//         return;
//     }
//     deallocate_no_tracking(pointer);
// }

// static bool keys_equal(Value v1, Value v2) {
//     int compare_result = -1;
//     bool compare_success = value_compare(v1, v2, &compare_result);
//     return compare_success && (compare_result == 0);
// }

// static void grow_table(Table* table) {
//     DEBUG_MEMORY("Growing table.");

//     if (table->is_growing) {
//         FAIL("grow_table() called while table is already growing."); // For current debugging, remove later
//     }

//     table->is_growing = true;
    
//     int old_capacity = table->capacity;
//     Node** old_entries = table->entries;
    
//     table->capacity = GROW_CAPACITY(table->capacity);
//     table->bucket_count = 0;
//     table->num_entries = 0;

//     table->entries = allocate_suitably(table, sizeof(Node*) * table->capacity, "Hash table array");

//     for (size_t i = 0; i < table->capacity; i++) {
//         table->entries[i] = NULL;
//     }

//     DEBUG_MEMORY("Old capacity: %d. New capacity: %d", old_capacity, table->capacity);
    
//     for (size_t i = 0; i < old_capacity; i++) {
//         Node* old_entry = old_entries[i];
        
//         while (old_entry != NULL) {
//             table_set(table, old_entry->key, old_entry->value);
//             Node* current = old_entry;
//             old_entry = old_entry->next;
//             deallocate_suitably(table, current, sizeof(Node), "Table linked list node");
//         }
//     }
    
//     DEBUG_MEMORY("Deallocating old table array.");
//     deallocate_suitably(table, old_entries, sizeof(Node*) * old_capacity, "Hash table array");

//     table->is_growing = false;
// }

// void table_init(Table* table) {
//     table->bucket_count = 0;
//     table->capacity = 0;
//     table->is_memory_infrastructure = false;
//     table->is_growing = false;
//     table->entries = NULL;
//     table->num_entries = 0;
// }

// void table_init_memory_infrastructure(Table* table) {
//     table_init(table);
//     table->is_memory_infrastructure = true;
// }

// void table_set_cstring_key(Table* table, const char* key, Value value) {
//     Value copied_key = MAKE_VALUE_OBJECT(object_string_copy_from_null_terminated(key)); // Yeah, not really efficient this stuff...
//     table_set(table, copied_key, value);
// }

// void table_set(Table* table, struct Value key, Value value) {
//     if (table->bucket_count + 1 > table->capacity * MAX_LOAD_FACTOR) {
//         grow_table(table);
//     }

//     unsigned long hash;
//     if (!value_hash(&key, &hash)) {
//     	// TODO: Report error
//         FAIL("Temporary FAIL: Couldn't hash in table_set.");
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
//         Node* new_node = allocate_suitably(table, sizeof(Node), "Table linked list node");
//         new_node->key = key;
//         new_node->value = value;

//         new_node->next = root_node;
//         table->entries[slot] = new_node;

//         table->num_entries++;
//     }
// }

// bool table_get_cstring_key(Table* table, const char* key, Value* out) {
//     Value value_key = MAKE_VALUE_OBJECT(object_string_copy_from_null_terminated(key));
//     return table_get(table, value_key, out);
// }

// bool table_get(Table* table, Value key, Value* out) {
//     if (table->capacity == 0) {
//         return false;
//     }

//     unsigned long hash;
//     if (!value_hash(&key, &hash)) {
//     	FAIL("Temporary FAIL: Couldn't hash in table_get.");
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

//     #if DEBUG_TABLE_STATS

//     times_called++;
//     capacity_sum += table->capacity;
//     avg_capacity = capacity_sum / times_called;
//     if (table->capacity > max_capacity) {
//         max_capacity = table->capacity;
//     }
//     bucket_sum += table->bucket_count;
//     avg_bucket_count = bucket_sum / times_called;
//     entries_sum += table->num_entries;
//     avg_entries_sum = entries_sum / times_called;

//     #endif

//     return false;
// }

// bool table_delete(Table* table, Value key) {
//     if (table->capacity == 0) {
//         return false;
//     }

//     unsigned long hash;
//     if (!value_hash(&key, &hash)) {
//     	return false;
//     }

//     int slot = hash % table->capacity;
//     Node* root_node = table->entries[slot];
//     Node* node = root_node;
//     Node* previous = NULL;

//     while (node != NULL) {
//         if (keys_equal(node->key, key)) {
//             if (previous != NULL) { 
//                 previous->next = node->next;
//                 deallocate_suitably(table, node, sizeof(Node), "Table linked list node");
//             } else {
//                 table->entries[slot] = node->next;
//                 deallocate_suitably(table, node, sizeof(Node), "Table linked list node");
//             }

//             if (table->num_entries <= 0) {
//                 FAIL("table->num_entries <= 0 while deleting entry.");
//             }

//             table->num_entries--;
//             return true;
//         } else {
//             previous = node;
//             node = node->next;
//         }
//     }

//     return false;
// }

// PointerArray table_iterate(Table* table, const char* alloc_string) {
// 	PointerArray array;
// 	pointer_array_init(&array, alloc_string);

//      // TODO: Is it correct that we're iterating on table->capacity instead of table->count?
//     for (size_t i = 0; i < table->capacity; i++) {
//         Node* node = table->entries[i];

//         while (node != NULL) {
//             pointer_array_write(&array, node);
//             node = node->next;
//         }
//     }

// 	return array;
// }

// void table_print(Table* table) {
// 	PointerArray entries = table_iterate(table, "table print table_iterate buffer");

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

// void table_print_debug_as_buckets(Table* table, bool show_empty_buckets) {
//     for (size_t i = 0; i < table->capacity; i++) {
//         Node* node = table->entries[i];

//         if (node != NULL || (node == NULL && show_empty_buckets)) {

//             printf("Bucket [%3" PRI_SIZET "Z]", i);

//             while (node != NULL) {
//                 printf("  --->  ");

//                 printf("<");
//                 value_print(node->key);
//                 printf(" : ");
//                 value_print(node->value);
//                 printf(">");
//                 node = node->next;
//             }

//             printf("\n");

//         }
//     }
// }

// void table_print_debug(Table* table) {
//     // printf("Capacity: %d \nCount: %d \nCollisions: %d \n", table->capacity, table->count, table->collisions_counter);
//     printf("Bucket capacity: %d \nBucket count: %d \n", table->capacity, table->bucket_count);
    
//     PointerArray entries = table_iterate(table, "table print debug table_iterate buffer");
//     if (table->capacity > 0) {
//     	printf("Data: \n");
// 		for (size_t i = 0; i < entries.count; i++) {
// 			Node* entry = entries.values[i];
// 			printf("%" PRI_SIZET " = [Key: ", i);
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
//     // TODO: Differentiate between inner iteration utility which exposes Node*s,
//     // and the API function which should expose Entry objects or something (without the next member)

//     PointerArray entries = table_iterate(table, "table free table_iterate buffer");
//     for (size_t i = 0; i < entries.count; i++) {
//         Node* node = entries.values[i];
//         deallocate_suitably(table, node, sizeof(Node), "Table linked list node");        
//     }
//     pointer_array_free(&entries);

//     deallocate_suitably(table, table->entries, sizeof(Node*) * table->capacity, "Hash table array");
//     table_init(table);
// }

// Table table_new_empty(void) {
// 	Table table;
// 	table_init(&table);
// 	return table;
// }