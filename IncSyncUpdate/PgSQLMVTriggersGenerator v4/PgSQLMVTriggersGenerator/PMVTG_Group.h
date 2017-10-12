#pragma once

#include "PMVTG_Config.h"
#include "PMVTG_Table.h"

/*struct s_Group is used to store group's data for outer-join MV */

typedef struct s_Group *Group;

struct s_Group {
	//number of tables joined in a group 
	int tablesNum;
	//Dynamic array of tables in group
	struct s_Table **tables;
	//role of the group in the original query, can be "main", "comp" or "equal" (full outer join)
	char* RoleInOriginalQuery;
	
	int joiningTypesNum;
	int joiningConditionsNum;
	//joining types of tables in group, can be "LEFT OUTER", "RIGHT OUTER", "FUL OUTER", "INNER"
	char** JoiningTypes;
	//joining conditions of tables in group, in format <table_name>col = <table_name>.col"
	char** JoiningConditions;
};

void initGroup(Group *group);

void delGroup(Group *group);