#pragma once

/*
	Usage demo:
	---
	Column col, _col;
	initColumn(&col);
	_col = copyColumn(col);
	delColumn(&col);
*/

#include "MVIAUTG_Boolean.h"

struct s_Table;
typedef struct s_Column *Column;

// Columns that retrieved from database table information
#define COLUMN_CONTEXT_TABLE_WIRED					0

// Stand-alone columns that identified in the context that does not retrieve from database table information
#define COLUMN_CONTEXT_STAND_ALONE					1

// Stand-alone table columns, value only columns, ... extracted from expression
#define COLUMN_CONTEXT_EXTRACTED					2

// Unspecified contexts except !COLUMN_CONTEXT_TABLE_WIRED
#define COLUMN_CONTEXT_TABLE_UNWIRED				3

struct s_Column {

	/*
		- COLUMN_CONTEXT_TABLE_WIRED: Columns that retrieved from database table information
		- COLUMN_CONTEXT_STAND_ALONE: Stand-alone columns that identified in the context that does not retrieve from database table information
		- COLUMN_CONTEXT_EXTRACTED: Stand-alone table columns, value only columns, ... extracted from expression
		- COLUMN_CONTEXT_TABLE_UNWIRED: Unspecified contexts except !COLUMN_CONTEXT_TABLE_WIRED
	*/
	int context;

	// context = COLUMN_CONTEXT_TABLE_WIRED, instead of 'isTableColumn' (removed)
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

	// context = COLUMN_CONTEXT_EXTRACTED
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
