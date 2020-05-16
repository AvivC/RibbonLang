#ifndef ribbon_table_h
#define ribbon_table_h

#include "common.h"
#include "value.h"
#include "pointerarray.h"

typedef struct {
    Value key;
    Value value;
    char tombstone;
} Entry;

typedef struct {
    size_t capacity; /* Capacity of current underlying entries array */
    size_t count; /* Number of entries + tombstones, for determining when to grow */
    size_t num_entries; /* Number of logical entries */
    Entry* entries;
    bool is_memory_infrastructure;
    bool is_growing; /* For debugging */
    size_t collision_count; /* For debugging */
} Table;

struct ObjectString;

Table table_new_empty(void);

void table_init(Table* table);
void table_init_memory_infrastructure(Table* table);

void table_set(Table* table, struct Value key, Value value);
bool table_get(Table* table, Value key, Value* out);

void table_set_cstring_key(Table* table, const char* key, Value value);
bool table_get_cstring_key(Table* table, const char* key, Value* out);

void table_set_value_in_cell(Table* table, Value key, Value value);

bool table_delete(Table* table, Value key);

void table_free(Table* table);

PointerArray table_iterate(Table* table, const char* alloc_string);

void table_print(Table* table); /* For user display */
void table_print_debug(Table* table); /* for trace execution and debugging */

#if DEBUG_TABLE_STATS
void table_debug_print_general_stats(void);
#endif

#endif
