#pragma once

/*
	Usage demo:
	---
	Column col, _col;
	initColumn(&col);
	_col = copyColumn(col);
	delColumn(&col);
*/

#include "PMVTG_Boolean.h"

struct s_Table;
typedef struct s_Column *Column;

#define COLUMN_CONTEXT_TABLE 0

#define COLUMN_CONTEXT_FREE 1
#define COLUMN_CONTEXT_EXPRESSION 2
#define COLUMN_CONTEXT_NOT_TABLE 3

struct s_Column {

	/*
		Context																					Value of 'context' field
		- Column connected to a specified table													COLUMN_CONTEXT_TABLE
		- Standalone table column, in selected list (SL)										COLUMN_CONTEXT_FREE
		- Standalone table column or expression column, extracted from an expression in SL		COLUMN_CONTEXT_EXPRESSION
		- Not COLUMN_CONTEXT_TABLE (unspecified)												COLUMN_CONTEXT_NOT_TABLE
	*/
	int context;

	// context = COLUMN_CONTEXT_TABLE, instead of 'isTableColumn' (removed)
	struct s_Table *table;
	char *name;
	char *type; // sql type (character, character varying, integer,...)

	/* 
		!= null, only if sql type is 'char' or 'var char'
		'characterLength' is string type for using in generate string code easily
	*/
	char *characterLength; 
	unsigned int ordinalPosition;
	Boolean isNullable;
	Boolean isPrimaryColumn;

	// context = COLUMN_CONTEXT_EXPRESSION
	char *originalName;

	Boolean hasNormalFunction;
	Boolean hasAggregateFunction;

	char *func;
	char *funcArg;

	char *varName;
};

// Allocate memory and initialize a column type pointer
void initColumn(Column *column);

// Clone column
Column copyColumn(Column column);

// Delete a column
void delColumn(Column *column);
