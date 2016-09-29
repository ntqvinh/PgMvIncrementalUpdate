#pragma once

/*
	Usage demo:
	---
	Table table;
	initTable(&table);
	delTable(&table);
*/

struct s_Column;
typedef struct s_Table *Table;

struct s_Table{
	char *name;
	unsigned int nCols;
	// Dynamic array of struct Column pointer
	struct s_Column **cols;
};

// Initialize a table type pointer and allocate memory
void initTable(Table *table);

// Delete a table
void delTable(Table *table);
