#include <stdio.h>
#include <stdlib.h>
#include "PMVTG_Config.h"
#include "PMVTG_CString.h"
#include "PMVTG_Table.h"
#include "PMVTG_Group.h"

// INIT Group
void initGroup(Group *group) {
	int i;
	*group = (Group)malloc(sizeof(struct s_Group));
	(*group)->tablesNum = 0;
	(*group)->joiningTypesNum = 0;
	(*group)->joiningConditionsNum = 0;

	(*group)->tables = (struct s_Table**) malloc(sizeof(struct s_Table*) * MAX_NUMBER_OF_TABLES);
	for (i = 0; i < MAX_NUMBER_OF_TABLES; i++) {
		(*group)->tables[i] = NULL;
	}

	(*group)->RoleInOriginalQuery = NULL;
	(*group)->JoiningConditions = (char *)malloc(sizeof(char *) * MAX_NUMBER_OF_TABLES);
	(*group)->JoiningTypes = (char *)malloc(sizeof(char *) * MAX_NUMBER_OF_TABLES);
}

// DELETE TABLE
void delGroup(Group *group) {
	int i;
	//Dont need to free each table, because all tables are free before
	
	FREE((*group)->tables);
	FREE(*group);
}