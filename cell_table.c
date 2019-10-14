#include "cell_table.h"
#include "table.h"
#include "object.h"

void cell_table_init(CellTable* table) {
	table_init(&table->table);
}

void cell_table_set_value(CellTable* table, ObjectString* key, Value value) {
	cell_table_set_value_cstring_key(table, key->chars, value);
}

void cell_table_set_value_cstring_key(CellTable* table, const char* key, Value value) {
	Table* inner_table = &table->table;
	Value current;
	if (table_get_cstring_key(inner_table, key, &current)) {
		ObjectCell* cell = NULL;
		if ((cell = VALUE_AS_OBJECT(current, OBJECT_CELL, ObjectCell)) != NULL) {
			cell->value = value;
		} else {
			FAIL("Found non ObjectCell* as a value in CellTable when setting value. Actual value type: %d.", value.type);
			return;
		}
	} else {
		Value cell_value = MAKE_VALUE_OBJECT(object_cell_new(value));
		table_set_cstring_key(inner_table, key, cell_value);
	}
}

void cell_table_set_cell(CellTable* table, ObjectString* key, struct ObjectCell* cell) {
	cell_table_set_cell_cstring_key(table, key->chars, cell);
}

void cell_table_set_cell_cstring_key(CellTable* table, const char* key, struct ObjectCell* cell) {
	Table* inner_table = &table->table;
	table_set_cstring_key(inner_table, key, MAKE_VALUE_OBJECT(cell));
}

bool cell_table_get_value_cstring_key(CellTable* table, const char* key, Value* out) {
	Table* inner_table = &table->table;
	Value current;
	if (table_get_cstring_key(inner_table, key, &current)) {
		ObjectCell* cell = NULL;
		if ((cell = VALUE_AS_OBJECT(current, OBJECT_CELL, ObjectCell)) == NULL) {
			FAIL("Found non ObjectCell* as a value in CellTable when getting value.");
		}
		*out = cell->value;
		return true;
	} else {
		return false;
	}
}

bool cell_table_get_cell_cstring_key(CellTable* table, const char* key, struct ObjectCell** out) {
	Table* inner_table = &table->table;
	Value current;
	if (table_get_cstring_key(inner_table, key, &current)) {
		struct ObjectCell* cell = NULL;
		if ((cell = VALUE_AS_OBJECT(current, OBJECT_CELL, ObjectCell)) == NULL) {
			FAIL("Found non ObjectCell* as a value in CellTable when getting value.");
		}
		*out = cell;
		return true;
	} else {
		return false;
	}
}

void cell_table_free(CellTable* table) {
	table_free(&table->table);
}

CellTable cell_table_new_empty(void) {
	CellTable table;
	cell_table_init(&table);
	return table;
}
