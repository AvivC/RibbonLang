#include <string.h>
#include "table.h"
#include "object.h"
#include "memory.h"
#include "common.h"
#include "value.h"

#define MAX_LOAD_FACTOR 0.75

bool object_cstrings_equal(const char* a, const char* b);
bool object_strings_equal(ObjectString* a, ObjectString* b);

static unsigned long hashString(const char* chars) {  // maybe should be unsigned char*?
    unsigned long hash = 5381;
    int c;

    while ((c = *chars++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

static Entry* findEntry(Table* table, const char* key, bool settingValue) {
    if (table->capacity == 0) {
        FAIL("Illegal state: table capacity is 0.");
    }
    
    unsigned long hash = hashString(key);
    // if table->capacity == 0 we have Undefined Behavior. Outer functions should guard against that
    int slot = hash % table->capacity;
    
    int nulls = 0;
    for (int i = 0; i < table->capacity; i++) {
        if (table->entries[i].key == NULL) nulls++;
    }
    
    Entry* entry = &table->entries[slot];
    while (entry->key != NULL && !object_cstrings_equal(entry->key, key)) {
        if (settingValue) {
            table->collisionsCounter++;
        }
        
        entry = &table->entries[++slot % table->capacity];
    }
    
    return entry;
}

static void growTable(Table* table) {
    DEBUG_MEMORY("Growing table.");
    
    int oldCapacity = table->capacity;
    Entry* oldEntries = table->entries;
    
    table->capacity = GROW_CAPACITY(table->capacity);
    table->entries = allocate(sizeof(Entry) * table->capacity, "Hash table array");
    
    DEBUG_MEMORY("Old capacity: %d. New capacity: %d", oldCapacity, table->capacity);
    
    for (int i = 0; i < table->capacity; i++) {
        table->entries[i] = (Entry) {.key = NULL, .value = MAKE_VALUE_NIL()};
    }
    
    for (int i = 0; i < oldCapacity; i++) {
        Entry* oldEntry = &oldEntries[i];
        
        if (oldEntry->key == NULL) {
            continue;
        }
        
        Entry* newEntry = findEntry(table, oldEntry->key, false);
        newEntry->key = oldEntry->key;
        newEntry->value = oldEntry->value;
    }
    
    DEBUG_MEMORY("Deallocating old table array.");
    deallocate(oldEntries, sizeof(Entry) * oldCapacity, "Hash table array");
}

void table_init(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->collisionsCounter = 0;
    table->entries = NULL;
}

void table_set_cstring_key(Table* table, const char* key, Value value) {
	/*
	 * TODO: Currently we're doing defensive copying of the incoming key,
	 * to prevent it from being collected along with an external ObjectString later on.
	 * The correct thing to do is to work with ObjectString here, or just generally Values. That's a pretty big refactor, to be done later.
	*/

	char* copied_key = allocate(strlen(key) + 1, "Table cstring key");
	strcpy(copied_key, key);

    if (table->count + 1 > table->capacity * MAX_LOAD_FACTOR) {
        growTable(table);
    }
    
    DEBUG_MEMORY("Finding entry '%s' in hash table.", key);
    Entry* entry = findEntry(table, copied_key, true);

    if (entry->key == NULL) {
        table->count++;
    } else {
		deallocate(entry->key, strlen(entry->key) + 1, "Table cstring key");
    }

    entry->key = copied_key; // If it's not NULL, we're needlessly overriding the same key
    entry->value = value;
}

void table_set(Table* table, struct ObjectString* key, Value value) {
    table_set_cstring_key(table, key->chars, value);
}

bool table_get_cstring_key(Table* table, const char* key, Value* out) {
    if (table->entries == NULL) {
        return false;
    }
    
    Entry* entry = findEntry(table, key, false);
    if (entry->key == NULL) {
        return false;
    }
    *out = entry->value;
    return true;
}

bool table_get(Table* table, struct ObjectString* key, Value* out) {
    return table_get_cstring_key(table, key->chars, out);
}

/* Get a PointerArray of Value* of all set entries in the table. */
PointerArray table_iterate(Table* table) {
	PointerArray array;
	pointer_array_init(&array, "table_iterate pointer array buffer");

	/* TODO: Pretty naive and inefficient - we scan the whole table in memory even though
	 * many entries are likely to be empty */
	for (int i = 0; i < table->capacity; i++) {
		Entry* entry = &table->entries[i];

		if (entry->key != NULL) {
			pointer_array_write(&array, entry);
		} /*else {
			if (entry->value.type != VALUE_NIL)
		}*/
	}

	return array;
}

void table_print(Table* table) {
	PointerArray entries = table_iterate(table);

	printf("[");
	for (int i = 0; i < entries.count; i++) {
		Entry* entry = entries.values[i];
		const char* key = entry->key; // TODO: Not only strings as keys
		Value value = entry->value;
		printf("\"%s\": ", key);
		value_print(value);

		if (i < entries.count - 1) {
			printf(", ");
		}
	}
	printf("]");

	pointer_array_free(&entries);
}

void table_print_debug(Table* table) {
    printf("Capacity: %d \nCount: %d \nCollisions: %d \n", table->capacity, table->count, table->collisionsCounter);
    
    if (table->capacity > 0) {
    	printf("Data: \n");
		for (int i = 0; i < table->capacity; i ++) {
			Entry* entry = &table->entries[i];
			const char* key = entry->key == NULL ? "null" : entry->key;
			printf("%d = [Key: %s, Value: ", i, key);
			value_print(entry->value);
			printf("]\n");
		}
    } else {
    	printf("Table is empty.\n");
    }
}

void table_free(Table* table) {
	PointerArray entries = table_iterate(table);
	for (int i = 0; i < entries.count; i++) {
		Entry* entry = entries.values[i];
		deallocate(entry->key, strlen(entry->key) + 1, "Table cstring key");
	}
	pointer_array_free(&entries);

    deallocate(table->entries, sizeof(Entry) * table->capacity, "Hash table array");
    table_init(table);
}

Table table_new_empty(void) {
	Table table;
	table_init(&table);
	return table;
}
