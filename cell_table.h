#ifndef plane_cell_table_h
#define plane_cell_table_h

#include "table.h"
#include "common.h"

typedef struct {
	Table table;
} CellTable;

struct ObjectCell;

void cell_table_init(CellTable* table);
void cell_table_set_value_cstring_key(CellTable* table, const char* key, Value value);
void cell_table_set_cell_cstring_key(CellTable* table, const char* key, struct ObjectCell* cell);
bool cell_table_get_value_cstring_key(CellTable* table, const char* key, Value* out);
bool cell_table_get_cell_cstring_key(CellTable* table, const char* key, struct ObjectCell** out);
void cell_table_free(CellTable* table);

#endif
