#include "table.h"
#include "object.h"
#include "memory.h"
#include "common.h"
#include "value.h"

#define MAX_LOAD_FACTOR 0.75

bool stringsEqual(ObjectString* a, ObjectString* b);

static unsigned long hashString(const char* chars) {  // maybe should be unsigned char*?
    unsigned long hash = 5381;
    int c;

    while ((c = *chars++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

static Entry* findEntry(Table* table, ObjectString* key, bool settingValue) {
    if (table->capacity == 0) {
        DEBUG_PRINT("Illegal state: table capacity is 0.");
    }
    
    unsigned long hash = hashString(key->chars);
    // if table->capacity == 0 we have Undefined Behavior. Outer functions should guard against that
    int slot = hash % table->capacity;
    
    int nulls = 0;
    for (int i = 0; i < table->capacity; i++) {
        if (table->entries[i].key == NULL) nulls++;
    }
    
    Entry* entry = &table->entries[slot];
    while (entry->key != NULL && !stringsEqual(entry->key, key)) {
        if (settingValue) {
            table->collisionsCounter++;
        }
        
        entry = &table->entries[++slot % table->capacity];
    }
    
    return entry;
}

static void growTable(Table* table) {
    DEBUG_PRINT("Growing table.");
    
    int oldCapacity = table->capacity;
    Entry* oldEntries = table->entries;
    
    table->capacity = GROW_CAPACITY(table->capacity);
    table->entries = allocate(sizeof(Entry) * table->capacity, "Hash table");
    
    DEBUG_PRINT("Old capacity: %d. New capacity: %d", oldCapacity, table->capacity);
    
    for (int i = 0; i < table->capacity; i++) {
        table->entries[i] = (Entry) {.key = NULL, .value = MAKE_VALUE_NUMBER(0)}; // TODO: add proper Nil values
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
    
    DEBUG_PRINT("Deallocating old table array.");
    deallocate(oldEntries, sizeof(Entry) * oldCapacity, "hash table array");
}

void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->collisionsCounter = 0;
    table->entries = NULL;
}

void setTable(Table* table, ObjectString* key, Value value) {
    if (table->count + 1 > table->capacity * MAX_LOAD_FACTOR) {
        growTable(table);
    }
    
    DEBUG_PRINT("Finding entry '%s' in hash table.", key->chars);
    Entry* entry = findEntry(table, key, true);
    if (entry->key == NULL) {
        table->count++;
    }
    entry->key = key; // If it's not NULL, we're needlessly overriding the same key
    entry->value = value;
}

bool getTable(Table* table, ObjectString* key, Value* out) {
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

void printTable(Table* table) {
    printf("Printing table\n");
    printf("Capacity: %d \nCount: %d \nCollisions: %d \nData: \n\n", table->capacity, table->count, table->collisionsCounter);
    
    for (int i = 0; i < table->capacity; i ++) {
        Entry* entry = &table->entries[i];
        char* key = entry->key == NULL ? "null" : entry->key->chars;
        double value = entry->value.as.number;
        printf("%d = [Key: %s, Value: %f]\n", i, key, value);
    }
}
