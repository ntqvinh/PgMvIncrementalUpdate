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

	(*table)->cols = (struct s_Column**) malloc (sizeof(struct s_Column*) * MAX_N_COLS);
	for (i = 0; i < MAX_N_COLS; i++)
		(*table)->cols[i] = NULL;
}

// DELETE TABLE
void delTable(Table *table) {
	int i;

	FREE((*table)->name);

	for (i = 0; i < MAX_N_COLS; i++)
		delColumn(&((*table)->cols[i]));

	FREE((*table)->cols);

	FREE(*table);
}