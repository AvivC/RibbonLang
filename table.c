#include "table.h"
#include "plane_object.h"

#define TABLE_LOAD_FACTOR 0.75

#if DEBUG_TABLE_STATS

/* For debugging */
static size_t times_called = 0;
static size_t capacity_sum = 0;
static size_t max_capacity = 0;
static size_t bucket_sum = 0;
static size_t avg_bucket_count = 0;
static size_t entries_sum = 0;
static size_t avg_entries_sum = 0;
static double avg_capacity = 0;
static size_t total_collision_count = 0;
static double avg_collision_count = 0;
/* ..... */

void table_debug_print_general_stats(void) {
    printf("Times called: %" PRI_SIZET "\n", times_called);
    printf("Sum capacity: %" PRI_SIZET "\n", capacity_sum);
    printf("Avg capacity: %g\n", avg_capacity);
    printf("Max capacity: %" PRI_SIZET "\n", max_capacity);
    printf("Total collison count: %" PRI_SIZET "\n", total_collision_count);
    printf("Avg collison count: %lf\n", avg_collision_count);
}

#endif

static void* allocate_suitably(Table* table, size_t size, const char* what) {
    if (!table->is_memory_infrastructure) {
        return allocate(size, what);
    }
    return allocate_no_tracking(size);
}

static void deallocate_suitably(Table* table, void* pointer, size_t size, const char* what) {
    if (!table->is_memory_infrastructure) {
        deallocate(pointer, size, what);
        return;
    }
    deallocate_no_tracking(pointer);
}

void table_init(Table* table) {
    table->capacity = 0;
    table->count = 0;
    table->num_entries = 0;
    table->entries = NULL;
    table->is_memory_infrastructure = false;
    table->is_growing = false;
    table->collision_count = 0;
}

void table_init_memory_infrastructure(Table* table) {
    table_init(table);
    table->is_memory_infrastructure = true;
}

Table table_new_empty(void) {
    Table table;
    table_init(&table);
    return table;
}

static bool keys_equal(Value v1, Value v2) {
    int compare_result = -1;
    bool compare_success = value_compare(v1, v2, &compare_result);
    return compare_success && (compare_result == 0);
}

static bool is_empty(Entry* e) {
    return e->key.type == VALUE_NIL;
}

static Entry* find_entry(Table* table, Value key) {
    unsigned long hash;
    if (!value_hash(&key, &hash)) {
        FAIL("Couldn't hash in table::find_entry.");
    }

    // size_t slot = hash % table->capacity;
    size_t slot = hash & (table->capacity - 1);
    Entry* entry = &table->entries[slot];

    Entry* tombstone = NULL;

    bool collision = false;

    while (true) {
        assert(entry->tombstone == 1 || entry->tombstone == 0); /* Remove later */

        if (is_empty(entry)) {
            if (entry->tombstone == 1) {
                if (tombstone == NULL) {
                    tombstone = entry;
                }
            } else {
                entry = tombstone == NULL ? entry : tombstone;
                break;
            }
        } else {
            if (keys_equal(key, entry->key)) {
                break;
            }
        }

        collision = true;
        entry = &table->entries[++slot % table->capacity];
    }

    if (collision) {
        table->collision_count++;
    }

    #if DEBUG_TABLE_STATS
    total_collision_count += table->collision_count;
    times_called++;
    capacity_sum += table->capacity;
    avg_capacity = capacity_sum / times_called;
    if (table->capacity > max_capacity) {
        max_capacity = table->capacity;
    }
    avg_collision_count = total_collision_count / times_called;
    #endif

    return entry;
}

static void grow_table(Table* table) {
    assert(!table->is_growing);

    table->is_growing = true;

    size_t old_capacity = table->capacity;
    Entry* old_entries = table->entries;

    /* TODO: Grow to a prime number here? */
    table->capacity = table->capacity < 8 ? 8 : table->capacity * 2;;
    table->count = 0;
    table->num_entries = 0;
    table->collision_count = 0;
    table->entries = allocate_suitably(table, table->capacity * sizeof(Entry), "Hash table array");

    for (size_t i = 0; i < table->capacity; i++) {
        table->entries[i] = (Entry) {.key = MAKE_VALUE_NIL(), .value = MAKE_VALUE_NIL(), .tombstone = 0};
    }

    for (size_t i = 0; i < old_capacity; i++) {
        Entry* old_entry = &old_entries[i];
        if (!is_empty(old_entry)) {
            table_set(table, old_entry->key, old_entry->value);
        }
    }

    deallocate_suitably(table, old_entries, sizeof(Entry) * old_capacity, "Hash table array");

    table->is_growing = false;
}

bool table_get(Table* table, Value key, Value* out) {
    if (table->capacity == 0) {
        return false;
    }

    Entry* entry = find_entry(table, key);

    assert(is_empty(entry) || keys_equal(entry->key, key)); /* Remove later */

    if (is_empty(entry)) {
        return false;
    }

    *out = entry->value;
    return true;
}

void table_set(Table* table, struct Value key, Value value) {
    if (table->count + 1 >= table->capacity * TABLE_LOAD_FACTOR) {
        grow_table(table);
    }

    Entry* entry = find_entry(table, key);
    assert(is_empty(entry) || keys_equal(entry->key, key)); /* Remove later */

    if (is_empty(entry)) {
        table->count++;
        table->num_entries++;
        entry->key = key;
    }

    entry->value = value;
}

bool table_get_cstring_key(Table* table, const char* key, Value* out) {
    return table_get(table, MAKE_VALUE_OBJECT(object_string_copy_from_null_terminated(key)), out);
}

void table_set_cstring_key(Table* table, const char* key, Value value) {
    table_set(table, MAKE_VALUE_OBJECT(object_string_copy_from_null_terminated(key)), value);
}

bool table_delete(Table* table, Value key) {
    if (table->capacity == 0) {
        return false;
    }

    Entry* entry = find_entry(table, key);
    if (is_empty(entry)) {
        return false;
    }

    entry->key = MAKE_VALUE_NIL();
    entry->value = MAKE_VALUE_NIL();
    entry->tombstone = 1;

    table->num_entries--;

    return true;
}

void table_free(Table* table) {
    deallocate_suitably(table, table->entries, table->capacity * sizeof(Entry), "Hash table array");
    table_init(table);
}

PointerArray table_iterate(Table* table, const char* alloc_string) {
	PointerArray array;
	pointer_array_init(&array, alloc_string);

    for (size_t i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (!is_empty(entry)) {
            pointer_array_write(&array, entry);
        }
    }

	return array;
}

void table_print(Table* table) {
	PointerArray entries = table_iterate(table, "table print table_iterate buffer");

	printf("[");
	for (int i = 0; i < entries.count; i++) {
		Entry* entry = entries.values[i];
		Value value = entry->value;
		value_print(entry->key);
		printf(": ");
		value_print(value);

		if (i < entries.count - 1) {
			printf(", ");
		}
	}
	printf("]");

	pointer_array_free(&entries);
}

void table_print_debug(Table* table) {
    printf("Capacity: %" PRI_SIZET " \nCount: %" PRI_SIZET " \n", table->capacity, table->count);
    
    if (table->capacity > 0) {
        printf("Data: \n");
        for (size_t i = 0; i < table->capacity; i++) {
            Entry* entry = &table->entries[i];
            if (!is_empty(entry)) {
                printf("%" PRI_SIZET " = [Key: ", i);
                value_print(entry->key);
                printf(", Value: ");
                value_print(entry->value);
                printf("]\n");
            }
        }
    } else {
    	printf("Table is empty.\n");
    }
}
