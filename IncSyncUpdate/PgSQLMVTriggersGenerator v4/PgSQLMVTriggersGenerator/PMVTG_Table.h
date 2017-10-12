#pragma once
#include "PMVTG_Boolean.h"

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
	Boolean isAPartOfOuterJoin;
	Boolean isMainTableOfOuterJoin;
	Boolean isComplementTableOfOuterJoin;
	Boolean hasColumnInWhereClause;
	int colsInSelectNum;
};

// Initialize a table type pointer and allocate memory
void initTable(Table *table);

// Delete a table
void delTable(Table *table);

//clone table
Table copyTable(Table table);
