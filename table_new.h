// #ifndef plane_table_h
// #define plane_table_h

// #include "common.h"
// #include "value.h"
// #include "pointerarray.h"

// //typedef struct {
// //    char* key; // TODO: Not only strings
// //    Value value;
// //} Entry;

// typedef struct {
//     Value key;
//     Value value;
// } EntryNew;

// typedef struct Node {
//     struct Node* next;
//     Value key;
//     Value value;
// } Node;

// typedef struct {
//     int capacity;
//     int bucket_count;
//     Node** entries;
//     // int collisions_counter; // for debugging
// } TableNew;

// struct ObjectString;

// TableNew table_new_empty(void);

// void table_init(TableNew* table);

// //void table_set(Table* table, struct ObjectString* key, Value value);
// void table_set(TableNew* table, struct Value key, Value value); // TODO: report success or failure
// //bool table_get(Table* table, struct ObjectString* key, Value* out);
// bool table_get(TableNew* table, struct Value key, Value* out);

// void table_set_value_directly(TableNew* table, struct Value key, Value value);
// bool table_get_value_directly(TableNew* table, Value key, Value* out);

// void table_set_cstring_key(TableNew* table, const char* key, Value value); // TODO: report success or failure
// bool table_get_cstring_key(TableNew* table, const char* key, Value* out);

// void table_free(TableNew* table);

// PointerArray table_iterate(TableNew* table);

// void table_print(TableNew* table); /* For user display */
// void table_print_debug(TableNew* table); /* for trace execution and debugging */

// #endif
