#ifndef plane_table_h
#define plane_table_h

#include "common.h"
#include "value.h"
// #include "object.h"

// typedef ObjectString_t ObjectString;

typedef struct {
    struct ObjectString* key;
    Value value;
} Entry;

typedef struct {
    int capacity;
    int count;
    Entry* entries;
    int collisionsCounter; // for debugging
} Table;

void initTable(Table* table);
void setTable(Table* table, struct ObjectString* key, Value value); // TODO: report success or failure
bool getTable(Table* table, struct ObjectString* key, Value* out);

void printTable(Table* table); // temp - for debugging

#endif
