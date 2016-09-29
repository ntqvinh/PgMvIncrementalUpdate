#include <stdlib.h>
#include "MVIAUTG_Column.h"
#include "MVIAUTG_CString.h"

// INIT COLUMN
void initColumn(Column *column) {
	*column = (Column) malloc (sizeof(struct s_Column));
	(*column)->context = COLUMN_CONTEXT_STAND_ALONE;
	(*column)->table = NULL;
	(*column)->name = NULL;
	(*column)->type = NULL;
	(*column)->characterLength = NULL;
	(*column)->ordinalPosition = 0;
	(*column)->isNullable = FALSE;
	(*column)->isPrimaryColumn = FALSE;
	(*column)->originalName = NULL;
	(*column)->hasNormalFunction = FALSE;
	(*column)->hasAggregateFunction = FALSE;
	(*column)->func = NULL;
	(*column)->funcArg = NULL;
	(*column)->varName = NULL;
}

// CLONE COLUMN
Column copyColumn(Column column){
	Column tmp = (Column) malloc (sizeof(struct s_Column));
	tmp->context = column->context;
	tmp->table = column->table;
	tmp->name = copy(column->name);
	tmp->type = copy(column->type);
	tmp->characterLength = copy(column->characterLength);
	tmp->ordinalPosition = column->ordinalPosition;
	tmp->isNullable = column->isNullable;
	tmp->isPrimaryColumn = column->isPrimaryColumn;
	tmp->originalName = copy(column->originalName);
	tmp->hasNormalFunction = column->hasNormalFunction;
	tmp->hasAggregateFunction = column->hasAggregateFunction;
	tmp->func = copy(column->func);
	tmp->funcArg = copy(column->funcArg);
	tmp->varName = copy(column->varName);

	return tmp;
}

// DELETE COLUMN
void delColumn(Column *column){
	if (*column) {
		FREE((*column)->name);
		FREE((*column)->type);
		FREE((*column)->characterLength);
		FREE((*column)->originalName);
		FREE((*column)->func);
		FREE((*column)->funcArg);
		FREE((*column)->varName);
		FREE(*column);
	}
}