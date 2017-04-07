#include <stdlib.h>
#include "PMVTG_CString.h"
#include "PMVTG_Table.h"
#include "PMVTG_Column.h"
#include "PMVTG_Config.h"

// INIT TABLE
void initTable(Table *table) {
	int i;

	*table = (Table) malloc (sizeof(struct s_Table));

	(*table)->name = NULL;
	(*table)->nCols = 0;

	(*table)->cols = (struct s_Column**) malloc (sizeof(struct s_Column*) * MAX_NUMBER_OF_COLS);
	for (i = 0; i < MAX_NUMBER_OF_COLS; i++)
		(*table)->cols[i] = NULL;
}

// DELETE TABLE
void delTable(Table *table) {
	int i;

	FREE((*table)->name);

	for (i = 0; i < MAX_NUMBER_OF_COLS; i++)
		delColumn(&((*table)->cols[i]));

	FREE((*table)->cols);

	FREE(*table);
}

//CLONE TABLE
Table copyTable(Table table) {
	Table tmp = (Table)malloc(sizeof(struct s_Table));
	tmp->name = copy(table->name);
	tmp->nCols = table->nCols;
	/*for (int i = 0; i < table->nCols; i++) {
		tmp->cols[i] = copyColumn(table->cols[i]);
	}*/
	return tmp;
}