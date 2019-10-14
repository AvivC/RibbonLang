#ifndef plane_cell_table_h
#define plane_cell_table_h

#include "common.h"
#include "object.h"

typedef struct {
	Table table;
} CellTable;

void cell_table_init(CellTable* table);
void cell_table_set_value_cstring_key(CellTable* table, const char* key, Value value);
void cell_table_set_cell_cstring_key(CellTable* table, const char* key, ObjectCell* cell);
bool cell_table_get_value(CellTable* table, ObjectString* key, Value* out);
bool cell_table_get_cell(CellTable* table, ObjectString* key, ObjectCell** out);
void cell_table_free(CellTable* table);

#endif
