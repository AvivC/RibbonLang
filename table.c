#include "table.h"
#include "object.h"
#include "memory.h"
#include "common.h"
#include "value.h"

#define MAX_LOAD_FACTOR 0.75

bool cstringsEqual(const char* a, const char* b);
bool stringsEqual(ObjectString* a, ObjectString* b);

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
    while (entry->key != NULL && !cstringsEqual(entry->key, key)) {
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
    table->entries = allocate(sizeof(Entry) * table->capacity, "Hash table array");
    
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
    deallocate(oldEntries, sizeof(Entry) * oldCapacity, "Hash table array");
}

void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->collisionsCounter = 0;
    table->entries = NULL;
}

void setTableCStringKey(Table* table, const char* key, Value value) {
    if (table->count + 1 > table->capacity * MAX_LOAD_FACTOR) {
        growTable(table);
    }
    
    DEBUG_PRINT("Finding entry '%s' in hash table.", key);
    Entry* entry = findEntry(table, key, true);
    if (entry->key == NULL) {
        table->count++;
    }
    entry->key = key; // If it's not NULL, we're needlessly overriding the same key
    entry->value = value;
}

void setTable(Table* table, struct ObjectString* key, Value value) {
    setTableCStringKey(table, key->chars, value);
}

bool getTableCStringKey(Table* table, const char* key, Value* out) {
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

bool getTable(Table* table, struct ObjectString* key, Value* out) {
    return getTableCStringKey(table, key->chars, out);
}

void printTable(Table* table) {
    printf("Capacity: %d \nCount: %d \nCollisions: %d \nData: \n\n", table->capacity, table->count, table->collisionsCounter);
    
    for (int i = 0; i < table->capacity; i ++) {
        Entry* entry = &table->entries[i];
        const char* key = entry->key == NULL ? "null" : entry->key;
        printf("%d = [Key: %s, Value: ", i, key);
        printValue(entry->value);
        printf("]\n");
    }
}

void freeTable(Table* table) {
    deallocate(table->entries, sizeof(Entry) * table->capacity, "Hash table array");
    initTable(table);
}
