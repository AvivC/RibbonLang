#ifndef plane_table_h
#define plane_table_h

#include "common.h"
#include "value.h"
#include "pointerarray.h"

typedef struct {
    char* key; // TODO: Not only strings
    Value value;
} Entry;

typedef struct {
    int capacity;
    int count;
    Entry* entries;
    int collisionsCounter; // for debugging
} Table;

struct ObjectString;

void initTable(Table* table);

void setTable(Table* table, struct ObjectString* key, Value value); // TODO: report success or failure
bool getTable(Table* table, struct ObjectString* key, Value* out);

void setTableCStringKey(Table* table, const char* key, Value value); // TODO: report success or failure
bool getTableCStringKey(Table* table, const char* key, Value* out);

void freeTable(Table* table);

PointerArray table_iterate(Table* table);

void table_print(Table* table); /* For user display */
void table_print_debug(Table* table); /* for trace execution and debugging */

#endif
