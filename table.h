#ifndef plane_table_h
#define plane_table_h

#include "common.h"
#include "value.h"
#include "pointerarray.h"

//typedef struct {
//    char* key; // TODO: Not only strings
//    Value value;
//} Entry;

typedef struct {
    Value key;
    Value value;
} EntryNew;

typedef struct Node {
    struct Node* next;
    Value key;
    Value value;
} Node;

typedef struct {
    int capacity;
    int bucket_count;
    Node** entries;
    // int collisions_counter; // for debugging
    bool is_memory_infrastructure;
} Table;

struct ObjectString;

Table table_new_empty(void);

void table_init(Table* table);
void table_init_memory_infrastructure(Table* table);

//void table_set(Table* table, struct ObjectString* key, Value value);
void table_set(Table* table, struct Value key, Value value); // TODO: report success or failure
//bool table_get(Table* table, struct ObjectString* key, Value* out);
bool table_get(Table* table, struct Value key, Value* out);

void table_set_value_directly(Table* table, struct Value key, Value value);
bool table_get_value_directly(Table* table, Value key, Value* out);

void table_set_cstring_key(Table* table, const char* key, Value value); // TODO: report success or failure
bool table_get_cstring_key(Table* table, const char* key, Value* out);

bool table_delete(Table* table, Value key);

void table_free(Table* table);

PointerArray table_iterate(Table* table, const char* alloc_string);

void table_print(Table* table); /* For user display */
void table_print_debug(Table* table); /* for trace execution and debugging */
void table_print_debug_as_buckets(Table* table, bool show_empty_buckets);

#endif
