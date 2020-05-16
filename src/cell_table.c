#include "cell_table.h"
#include "table.h"
#include "plane_object.h"

void cell_table_init(CellTable* table) {
	table_init(&table->table);
}

void cell_table_set_value(CellTable* table, ObjectString* key, Value value) {
	/* Using special case route in table.c for optimization */
	table_set_value_in_cell(&table->table, MAKE_VALUE_OBJECT(key), value);
}

bool cell_table_get_value(CellTable* table, struct ObjectString* key, Value* out) {
	ObjectCell* cell;
	if (cell_table_get_cell(table, key, &cell) && cell->is_filled) {
		*out = cell->value;
		return true;
	}

	return false;
}

void cell_table_set_value_cstring_key(CellTable* table, const char* key, Value value) {
	cell_table_set_value(table, object_string_copy_from_null_terminated(key), value);
}

bool cell_table_get_value_cstring_key(CellTable* table, const char* key, Value* out) {
	return cell_table_get_value(table, object_string_copy_from_null_terminated(key), out);
}

bool cell_table_get_cell(CellTable* table, struct ObjectString* key, struct ObjectCell** out) {
	Table* inner_table = &table->table;
	Value current;
	if (table_get(inner_table, MAKE_VALUE_OBJECT(key), &current)) {
		assert(object_value_is(current, OBJECT_CELL));
		*out = (ObjectCell*) current.as.object;;
		return true;
	}

	return false;
}

void cell_table_set_cell(CellTable* table, struct ObjectString* key, struct ObjectCell* cell) {
	table_set(&table->table, MAKE_VALUE_OBJECT(key), MAKE_VALUE_OBJECT(cell));
}

bool cell_table_get_cell_cstring_key(CellTable* table, const char* key, struct ObjectCell** out) {
	return cell_table_get_cell(table, object_string_copy_from_null_terminated(key), out);
}

void cell_table_set_cell_cstring_key(CellTable* table, const char* key, struct ObjectCell* cell) {
	cell_table_set_cell(table, object_string_copy_from_null_terminated(key), cell);
}

void cell_table_free(CellTable* table) {
	table_free(&table->table);
	cell_table_init(table);
}

CellTable cell_table_new_empty(void) {
	CellTable table;
	cell_table_init(&table);
	return table;
}
