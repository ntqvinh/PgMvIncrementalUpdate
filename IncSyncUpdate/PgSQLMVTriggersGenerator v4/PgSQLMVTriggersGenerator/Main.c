// For standard input, output, memory allocating
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>

// For random function
#include <time.h>

// For file processing
#include <windows.h>

// For PMVTG
#include "PMVTG_Boolean.h"
#include "PMVTG_CString.h"
#include "PMVTG_Query.h"
#include "PMVTG.h"

// For memory leak detector
//#include <vld.h>

/*DMT: ---------------------------------------------------------------------------------------------------
Definitions and notation
ORGTB:	Original table. Some docs call it "base table".
ORGSQ:	Original selecting query - the query used to create MV.
A2:		Algorithm version 2 - optimized.
OBJ_A2_CHECK: Boolean variable that checks if the condition for implementing A2 algorithm is met (or not)
PRINT_TO_FILE: macro stands for fprintf. Used to distinguish with printf, which frequently used for logging and debugging

---------------------------------------------------------------------------------------------------------*/

MAIN{

	// Loop variables
	/*
	Description: j is used for table looping through table list in FROM clause
	*/
	int i, j, k, m;

// Connection info
char dbName[MAX_LENGTH_OF_DBNAME], username[MAX_LENGTH_OF_USERNAME], password[MAX_LENGTH_OF_PASSWORD],
port[MAX_LENGTH_OF_PORT], host[MAX_LENGTH_OF_HOST];
char *connectionString;

// PMVTG info
char outputPath[MAX_LENGTH_OF_OUTPUT_PATH], mvName[MAX_LENGTH_OF_MV_NAME], outputFilesName[MAX_LENGTH_OF_FILE_NAME];
char *triggerFile, *sqlFile, *dllFile;
char *copyCmd;

// Create random seed
srand(time(NULL));

// Input & analyze query, create: OBJ_SQ contains query analyzed information
INPUT_SELECT_QUERY;
ANALYZE_SELECT_QUERY;

// ! Exit check point
if (OBJ_SQ == NULL)
return puts("PMVTG: Error while analyzing query."), 0;
else
puts("PMVTG: Query is analyzed successfully.");

// Input outputing information
scanf("%s%s%s", outputPath, mvName, outputFilesName);

// triggerFileName = full file path of trigger file
triggerFile = copy(outputPath);
STR_APPEND(triggerFile, "\\");
STR_APPEND(triggerFile, outputFilesName);
STR_APPEND(triggerFile, "_triggersrc.c");

// sqlFileName = full file path of SQL file
sqlFile = copy(outputPath);
STR_APPEND(sqlFile, "\\");
STR_APPEND(sqlFile, outputFilesName);
STR_APPEND(sqlFile, "_mvsrc.sql");

// dllFile = full file path of DLL file (implicit .dll)
dllFile = copy(outputFilesName);
STR_APPEND(dllFile, "_triggersrc");
// Command that copies 'ctrigger.h' to output folder
// 'ctrigger.h' is a necessary header for C-trigger included within generated C file
copyCmd = copy("copy ");
STR_APPEND(copyCmd, "ctrigger.h ");
STR_APPEND(copyCmd, outputPath);
STR_APPEND(copyCmd, "\\ctrigger.h");
SYSTEM_EXEC(copyCmd);
FREE(copyCmd);

// Connect to database server
scanf("%s%s%s%s%s", dbName, username, password, host, port);
connectionString = copy("host='");
STR_APPEND(connectionString, host);
STR_APPEND(connectionString, "'dbname = '");
STR_APPEND(connectionString, dbName);
STR_APPEND(connectionString, "' user = '");
STR_APPEND(connectionString, username);
STR_APPEND(connectionString, "' password = '");
STR_APPEND(connectionString, password);
STR_APPEND(connectionString, "' port = ");
STR_APPEND(connectionString, port);
DEFINE_CONNECTION(connectionString);
CONNECT;
FREE(connectionString);

// ! Exit check point
if (!IS_CONNECTED)
{
	ERR("Connection to database failed");
}
else
puts("Connect to database successfully!");
puts("----------------------------------------------");

/*--------------------------------------------------------------------------------------------------------
DMT: Load query's tables and columns in FROM clause to OBJ_TABLE
--------------------------------------------------------------------------------------------------------*/
for (i = 0; i < OBJ_SQ->fromElementsNum; i++) {
	initTable(&OBJ_TABLE(i));
	OBJ_TABLE(i)->name = copy(OBJ_SQ->fromElements[i]);

	// Create columns
	DEFQ("SELECT column_name, data_type, ordinal_position, character_maximum_length, is_nullable ");
	ADDQ("FROM information_schema.columns WHERE table_name = '");
	ADDQ(OBJ_TABLE(i)->name);
	ADDQ("';");
	EXEC;
	for (j = 0; j < TUPLES_NUM; j++) {
		initColumn(&OBJ_TABLE(i)->cols[OBJ_TABLE(i)->nCols]);

		OBJ_TABLE(i)->cols[OBJ_TABLE(i)->nCols]->context = COLUMN_CONTEXT_TABLE;
		OBJ_TABLE(i)->cols[OBJ_TABLE(i)->nCols]->table = OBJ_TABLE(i);
		OBJ_TABLE(i)->cols[OBJ_TABLE(i)->nCols]->name = copy(VALUE_AT(j, 0));
		OBJ_TABLE(i)->cols[OBJ_TABLE(i)->nCols]->type = copy(VALUE_AT(j, 1));
		OBJ_TABLE(i)->cols[OBJ_TABLE(i)->nCols]->ordinalPosition = atoi(VALUE_AT(j, 2));
		OBJ_TABLE(i)->cols[OBJ_TABLE(i)->nCols]->characterLength = copy(VALUE_AT(j, 3));
		OBJ_TABLE(i)->cols[OBJ_TABLE(i)->nCols]->isNullable = STR_EQUAL_CI(VALUE_AT(j, 4), "true");
		//DMT: important step to create varName from column name
		OBJ_TABLE(i)->cols[OBJ_TABLE(i)->nCols]->varName = createVarName(OBJ_TABLE(i)->cols[OBJ_TABLE(i)->nCols]);

		OBJ_TABLE(i)->nCols++;
	}
	CLEAR;
}

// Set tables' primary key
for (i = 0; i < OBJ_SQ->fromElementsNum; i++) {
	DEFQ("SELECT pg_attribute.attname FROM pg_index, pg_class, pg_attribute WHERE pg_class.oid = '\"");
	ADDQ(OBJ_TABLE(i)->name);
	ADDQ("\"'::regclass AND indrelid = pg_class.oid AND pg_attribute.attrelid = pg_class.oid AND pg_attribute.attnum = any(pg_index.indkey) AND indisprimary;");
	EXEC;
	for (j = 0; j < TUPLES_NUM; j++)
		for (k = 0; k < OBJ_TABLE(i)->nCols; k++)
			if (STR_EQUAL(OBJ_TABLE(i)->cols[k]->name, VALUE_AT(j, 0))) {
				OBJ_TABLE(i)->cols[k]->isPrimaryColumn = TRUE;
				break;
			}
	CLEAR;
}

/*--------------------------------------------------------------------------------------------------------
Analyze selected columns list, expressions are separated to columns, create: OBJ_SELECTED_COL, OBJ_NSELECTED
--------------------------------------------------------------------------------------------------------*/
/*DMT:
From MV's query, for each column i in SELECT clause
If i is a column of a table, then call addRealColumn
If i isn't a column of a table (flag = false) then check if i is a function
If i is a function (SUM, MIN, MAX, ...) then addFunction
Else check if i is combined expressions then addRealColumn
*/
for (i = 0; i < OBJ_SQ->selectElementsNum; i++) {
	Boolean flag = addRealColumn(OBJ_SQ->selectElements[i], NULL);

	if (!flag) {
		char *remain, *tmp;
		char *AF[] = { AF_SUM, AF_COUNT, AF_AVG, AF_MIN, AF_MAX_ };
		char *NF[] = { "abs(" };

		char *delim = "()+*-/";
		char *filter[MAX_NUMBER_OF_ELEMENTS];
		int filterLen;

		// otherwise, first, check for the aggregate functions
		tmp = removeSpaces(OBJ_SQ->selectElements[i]);
		remain = addFunction(OBJ_SQ->selectElements[i], tmp, 5, AF, TRUE);
		free(tmp);
		// then, check for the normal functions
		tmp = remain;
		remain = addFunction(OBJ_SQ->selectElements[i], remain, 1, NF, FALSE);
		free(tmp);

		// finally, check for the remaining columns' name
		split(remain, delim, filter, &filterLen, TRUE);
		for (j = 0; j < filterLen; j++) {
			addRealColumn(filter[j], OBJ_SQ->selectElements[i]);
			free(filter[j]);
		}

		free(remain);
	}
}

// Additional count column for algorithm purpose
initColumn(&OBJ_SELECTED_COL(OBJ_NSELECTED));
OBJ_SELECTED_COL(OBJ_NSELECTED)->context = COLUMN_CONTEXT_NOT_TABLE;
OBJ_SELECTED_COL(OBJ_NSELECTED)->hasAggregateFunction = TRUE;
OBJ_SELECTED_COL(OBJ_NSELECTED)->name = string("count(*)");
OBJ_SELECTED_COL(OBJ_NSELECTED)->func = string(AF_COUNT);
OBJ_SELECTED_COL(OBJ_NSELECTED)->funcArg = string("*");
OBJ_SELECTED_COL(OBJ_NSELECTED)->type = string("bigint");
OBJ_SELECTED_COL(OBJ_NSELECTED)->varName = string("mvcount");
OBJ_NSELECTED++;

/*--------------------------------------------------------------------------------------------------------
Analyze joining conditions

IN: container includes conditions involved by inner column, used for trigger's FCC
OUT: container includes conditions involved by outer column
BIN: container includes conditions involved by both inner and outer column (usually join conditions)
(BIN is sub set of OUT, separately saved for future dev)
Create: OBJ_IN_CONDITIONS, OBJ_OUT_CONDITIONS, OBJ_BIN_CONDITIONS, OBJ_FCC
------------------------------------------------------------------------------------------------------*/
//-----------------START------------------------
for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {

	// Check on WHERE
	if (OBJ_SQ->hasWhere)
		for (i = 0; i < OBJ_SQ->whereConditionsNum; i++) {
			filterTriggerCondition(OBJ_SQ->whereConditions[i], j, FALSE);
		}

	// Check on JOIN
	if (OBJ_SQ->hasJoin)
		for (i = 0; i < OBJ_SQ->onNum; i++)
			for (k = 0; k < OBJ_SQ->onConditionsNum[i]; k++)
				filterTriggerCondition(OBJ_SQ->onConditions[i][k], j, FALSE);

	//Check on OUTER JOIN
	if (OBJ_SQ->hasOuterJoin) {
		for (i = 0; i < OBJ_SQ->onOuterJoinNum; i++)
			for (k = 0; k < OBJ_SQ->onOuterJoinConditionsNum[i]; k++)
				filterTriggerCondition(OBJ_SQ->onOuterJoinConditions[i][k], j, TRUE);
	}

	// Check on HAVING
	if (OBJ_SQ->hasHaving)
		for (i = 0; i < OBJ_SQ->havingConditionsNum; i++)
			filterTriggerCondition(OBJ_SQ->havingConditions[i], j, FALSE);

	/* CREATE TRIGGERS' FIRST CHECKING CONDITION - FCC*/
	// 1. Embeding variables
	printf("---\n");
	for (i = 0; i < OBJ_SQ->fromElementsNum; i++) {
		for (int x = 0; x < OBJ_OUT_NCONDITIONS(i); x++)
			printf("OBJ_OUT_CONDITIONS(%s, %d) = %s \n", OBJ_TABLE(i)->name, x, OBJ_OUT_CONDITIONS(i, x));
	}
	for (i = 0; i < OBJ_SQ->fromElementsNum; i++) {
		for (int x = 0; x < OBJ_IN_NCONDITIONS(i); x++)
			printf("OBJ_IN_CONDITIONS(%s, %d) = %s \n", OBJ_TABLE(i)->name, x, OBJ_IN_CONDITIONS(i, x));
	}
	for (i = 0; i < OBJ_SQ->fromElementsNum; i++) {
		for (int x = 0; x < OBJ_BIN_NCONDITIONS(i); x++)
			printf("OBJ_BIN_CONDITIONS(%s, %d) = %s \n", OBJ_TABLE(i)->name, x, OBJ_BIN_CONDITIONS(i, x));
	}printf("---\n");
	// For each IN condition of table j
	for (k = 0; k < OBJ_IN_NCONDITIONS(j); k++) {

		char *conditionCode, *tmp;
		conditionCode = copy(OBJ_IN_CONDITIONS(j, k));
		// For each table 
		/*
		DMT: These lines of code convert the condition from SQL type to C-code type base on varName
		ex: STR_EQUAL_CI(sinh_vien.que_quan ,  "Da Nang") -> STR_EQUAL_CI(str_que_quan_sinh_vien_table ,  "Da Nang")
		Find column position, scan backward to find table name by getPrecededTableName function
		find conditionCode like tableName.colmnName
		replace conditionCode by varName
		ex: colPos = que_quan -> scan backward -> table = sinh_vien -> conditionCode = sinh_vien.que_quan -> replace
		*/
		for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
			char *colPos, *tableName;
			char *start, *end, *oldString, *draft, *checkVar;
			draft = conditionCode;

			// Find inner columns' position - T: tìm vị trí của cột
			do {
				colPos = findSubString(draft, OBJ_TABLE(j)->cols[i]->name);
				if (colPos) {
					tableName = getPrecededTableName(conditionCode, colPos);
					if (tableName) start = colPos - strlen(tableName) - 1;
					else start = colPos;
					end = colPos + strlen(OBJ_TABLE(j)->cols[i]->name);
					oldString = subString(conditionCode, start, end);
					tmp = conditionCode;
					conditionCode = replaceFirst(conditionCode, oldString, OBJ_TABLE(j)->cols[i]->varName);

					// Prevent the infinite loop in some cases
					draft = conditionCode;
					do {
						checkVar = strstr(draft, OBJ_TABLE(j)->cols[i]->varName);
						if (checkVar)
							draft = checkVar + (strlen(OBJ_TABLE(j)->cols[i]->varName) - 1);
					} while (checkVar);

					FREE(tmp);
					FREE(oldString);
					FREE(tableName);
				}
			} while (colPos);
		}

		tmp = OBJ_IN_CONDITIONS(j, k);
		OBJ_IN_CONDITIONS(j, k) = copy(conditionCode);
		OBJ_IN_CONDITIONS_OLD(j, k) = a2FCCRefactor(conditionCode, "old_");
		OBJ_IN_CONDITIONS_NEW(j, k) = a2FCCRefactor(conditionCode, "new_");
		FREE(tmp);
		FREE(conditionCode);
	}// end of For each IN condition

	 // 2. Combine conditions with 'and' (&&) opr
	 /*
	 ! Normal function will be built in pmvtg_ctrigger's lib
	 */
	OBJ_FCC(j) = copy("1 ");
	OBJ_FCC_OLD(j) = copy("1 ");
	OBJ_FCC_NEW(j) = copy("1 ");

	for (i = 0; i < OBJ_IN_NCONDITIONS(j); i++) {
		OBJ_FCC(j) = dammf_append(OBJ_FCC(j), " && ");
		OBJ_FCC(j) = dammf_append(OBJ_FCC(j), OBJ_IN_CONDITIONS(j, i));

		OBJ_FCC_OLD(j) = dammf_append(OBJ_FCC_OLD(j), " && ");
		OBJ_FCC_OLD(j) = dammf_append(OBJ_FCC_OLD(j), OBJ_IN_CONDITIONS_OLD(j, i));

		OBJ_FCC_NEW(j) = dammf_append(OBJ_FCC_NEW(j), " && ");
		OBJ_FCC_NEW(j) = dammf_append(OBJ_FCC_NEW(j), OBJ_IN_CONDITIONS_NEW(j, i));
	}

	//OBJ_FCC_OLD(j) = a2FCCRefactor(OBJ_FCC(j), "old");
	//OBJ_FCC_NEW(j) = a2FCCRefactor(OBJ_FCC(j), "new");
} //-----------------END------------------------

DISCONNECT;

// Rebuild new select clause based on the original for algorithm purpose, create: OBJ_SELECT_CLAUSE
OBJ_SELECT_CLAUSE = copy("select ");
for (i = 0; i < OBJ_NSELECTED; i++) {
	char *name = copy(OBJ_SELECTED_COL(i)->name);
	if (OBJ_SELECTED_COL(i)->context == COLUMN_CONTEXT_FREE) {
		//DMT: These 2 line insert backward. ex: name = "tuoi"
		name = dammf_insertSubString(name, name, ".");
		//DMT: name = ".tuoi"
		name = dammf_insertSubString(name, name, OBJ_SELECTED_COL(i)->table->name);
		//DMT: name = "sinh_vien.tuoi"
	}
	STR_APPEND(OBJ_SELECT_CLAUSE, name);
	if (i != OBJ_NSELECTED - 1)
		STR_APPEND(OBJ_SELECT_CLAUSE, ", ");
	free(name);
}
// Check for aggregate functions, especially MIN and/or MAX, define: OBJ_STATIC_BOOL_AGG, OBJ_STATIC_BOOL_AGG_MINMAX
// OBJ_STATIC_BOOL_AGG is alias of OBJ_A1_CHECK
// Check in selected list if there are only sum & count aggregate functions involved in query
// Define OBJ_A2_CHECK (alias of OBJ_STATIC_BOOL_ONLY_AGG_SUMCOUNT)
// Note: A2 is a calling mask of updated algorithm for query contains only sum & count aggregate functions
// Note: Avg is turned into sum & count due to algorithm
for (i = 0; i < OBJ_NSELECTED - 1; i++) {
	if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {
		OBJ_STATIC_BOOL_AGG = TRUE;
		if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->func, AF_MIN) || STR_EQUAL_CI(OBJ_SELECTED_COL(i)->func, AF_MAX_)) {
			OBJ_STATIC_BOOL_AGG_MINMAX = TRUE;
		}
		if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->func, AF_SUM) || STR_EQUAL_CI(OBJ_SELECTED_COL(i)->func, AF_COUNT)) {
			OBJ_A2_CHECK = TRUE;
		}
	}
}

/* DMT: OBJ_A2_CHECK only TRUE if all selected elements of ORGSQ contains only SUM or COUNT or both.
If MIN or MAX detected -> OBJ_A2_CHECK = FALSE because we can't implement A2 for ORGSQ that have MIN/MAX in SELECT.
*/
if (OBJ_STATIC_BOOL_AGG) {
	if (OBJ_A2_CHECK && OBJ_STATIC_BOOL_AGG_MINMAX) {
		OBJ_A2_CHECK = FALSE;
	}
}

/*	DMT: The block of code below works for detection of A2 condition - all columns of Ti that joined in key of Ti
(primary key or unique key): Ci1, Ci2, ... Cij, ... Cik	*/
// Check for columns that are both in T & G clause, define: OBJ_TG_COL, OBJ_TG_NCOLS
// Note: defined objects do not need to free after use (unlike created objects)
// TG: columns appeared in current table & MV, + primary key
// Go on pair with OBJ_A2_TG_TABLE, which determines the tables (of column in sql) that use that value from the current table
// customers.cust_id = <sales.cust_id> (TG)
// costs.promo_id = <sales.promo_id> (pk) and ...
// Also define: OBJ_A2_TG_NTABLES, OBJ_A2_TG_TABLE
// OBJ_A2_TG_NTABLES & OBJ_TG_NCOLS are duplicated each other

/*
DMT: For each table j in FROM
For each conlumn i in table j
If found i in GROUP BY[k], and tableName of GROUP BY[k] != j
{	OBJ_TG_COL(j) = i;
OBJ_A2_TG_TABLE(j) = tableName;
}
*/
for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
	OBJ_A2_TG_NTABLES(j) = 0;
	OBJ_TG_NCOLS(j) = 0;
	for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
		char *colname = copy(OBJ_TABLE(j)->cols[i]->name);
		for (k = 0; k < OBJ_SQ->groupbyElementsNum; k++) {

			char *colPos = findSubString(OBJ_SQ->groupbyElements[k], colname);
			if (colPos) {
				char *tableName = getPrecededTableName(OBJ_SQ->groupbyElements[k], colPos);
				if (STR_INEQUAL_CI(tableName, OBJ_TABLE(j)->name)) {
					OBJ_TG_COL(j, OBJ_TG_NCOLS(j)++) = OBJ_TABLE(j)->cols[i];
					//printf("Cot x cua j ma co mat trong group by duoi dang k.x:\n OBJ_TG_COL(%s, %d) = %s; Table k =%s \n", OBJ_TABLE(j)->name, OBJ_TG_NCOLS(j) - 1, OBJ_TABLE(j)->cols[i]->name, tableName);
					OBJ_A2_TG_TABLE(j, OBJ_A2_TG_NTABLES(j)++) = copy(tableName);
					//printf("->OBJ_A2_TG_TABLE(%s, %d) = %s;\n", OBJ_TABLE(j)->name, OBJ_A2_TG_NTABLES(j) - 1, OBJ_A2_TG_TABLE(j, OBJ_A2_TG_NTABLES(j) - 1));
				}
				FREE(tableName);
			}
		}
		FREE(colname);
	}
}

/*
DMT: For each primary key i of table j of FROM
For each column m of table k of FROM, k != j
If i_name = m_name
{
OBJ_TG_COL(j) = i;
OBJ_A2_TG_TABLE(j) = k;
}
*/
for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
	for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
		if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
			for (k = 0; k < OBJ_SQ->fromElementsNum; k++) {
				if (k != j) {
					for (m = 0; m < OBJ_TABLE(k)->nCols; m++) {
						if (STR_EQUAL_CI(OBJ_TABLE(j)->cols[i]->name, OBJ_TABLE(k)->cols[m]->name)) {
							OBJ_TG_COL(j, OBJ_TG_NCOLS(j)++) = OBJ_TABLE(j)->cols[i];
							OBJ_A2_TG_TABLE(j, OBJ_A2_TG_NTABLES(j)++) = copy(OBJ_TABLE(k)->name);
							//printf("Khoa chinh x cua j ma trung ten voi mot cot cua k: \n OBJ_TG_COL(%s, %d) = %s; Table j = %s, Table k = %s \n", OBJ_TABLE(j)->name, OBJ_TG_NCOLS(j) - 1, OBJ_TABLE(j)->cols[i]->name, OBJ_TABLE(j)->name, OBJ_TABLE(k)->name);
							/*printf("--> OBJ_A2_TG_TABLE(%s, %d) = %s, Table j = %s, Table k = %s \n",
								OBJ_TABLE(j)->name, OBJ_A2_TG_NTABLES(j) - 1, OBJ_A2_TG_TABLE(j, OBJ_A2_TG_NTABLES(j) - 1),
								OBJ_TABLE(j)->name, OBJ_TABLE(k)->name);*/
						}
					}
				}
			}
		}
	}
}
/*  DMT: Khi chạy đến đây thì OBJ_TG_COL(j, index) đã chứa:
1. Các cột của bảng j mà các cột này được bảng k khác tham chiếu và có mặt trong group by dưới dạng k.i
2. Cột khóa chính của bảng j mà trùng tên với một cột khác trong bảng k (nhiều khả năng là khóa ngoại của k
vì khi thiết kế CSDL ít gặp trường hợp cột này trùng tên với khóa chính của bảng khác mà giữa 2 bảng ko có liên hệ gì)
*/
//#region Check if the condition for the A2 algorithm is met. Define: ABJ_A2_CHECK

// Create: OBJ_A2_WHERE_CLAUSE
//T: OBJ_A2_CHECK = true means there is only sum and count appear in select
if (OBJ_A2_CHECK) {
	for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
		STR_INIT(OBJ_A2_WHERE_CLAUSE(j), " true ");

		// Preprocess out conditions & recombine
		for (i = 0; i < OBJ_OUT_NCONDITIONS(j); i++) {
			OBJ_OUT_CONDITIONS(j, i) = ConditionCToSQL(OBJ_OUT_CONDITIONS(j, i));
			STR_APPEND(OBJ_A2_WHERE_CLAUSE(j), " and ");
			STR_APPEND(OBJ_A2_WHERE_CLAUSE(j), OBJ_OUT_CONDITIONS(j, i));
		}

		//printf("OBJ_A2_WHERE_CLAUSE(%s) = %s\n", OBJ_TABLE(j)->name, OBJ_A2_WHERE_CLAUSE(j));
	}
}

// Define: OBJ_AGG_INVOLVED_CHECK
// Important requirement: full column name as format: <table-name>.<column-name>
//T: OBJ_AGG_INVOLVED_CHECK (j) = TRUE means the table j has a column that appears in agg function ()
for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
	OBJ_AGG_INVOLVED_CHECK(j) = FALSE;
}

for (i = 0; i < OBJ_NSELECTED - 1; i++) {
	if (OBJ_SELECTED_COL(i)->hasAggregateFunction && STR_INEQUAL(OBJ_SELECTED_COL(i)->funcArg, "*")) {
		char *columnList[MAX_NUMBER_OF_ELEMENTS];
		int columnListLen = 0;
		split(OBJ_SELECTED_COL(i)->funcArg, "+-*/", columnList, &columnListLen, TRUE);
		for (k = 0; k < columnListLen; k++) {
			char *dotPos = strstr(columnList[k], ".");
			if (dotPos) {
				char *tableName = getPrecededTableName(columnList[k], dotPos + 1);
				for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
					if (STR_EQUAL_CI(tableName, OBJ_TABLE(j)->name)) {
						OBJ_AGG_INVOLVED_CHECK(j) = TRUE;
						break;
					}
				}
				FREE(tableName);
			}
		}

		for (k = 0; k < columnListLen; k++) {
			FREE(columnList[k]);
		}
	}
}

/*
Define: OBJ_KEY_IN_G_CHECK
DMT: OBJ_KEY_IN_G_CHECK(table) = TRUE means the table has a key in Group By Clause */

for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
	OBJ_KEY_IN_G_CHECK(j) = TRUE;
}

if (OBJ_SQ->hasGroupby) {
	// T: loop through each table in FROM clause
	for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
		//T: loop through each column in TABLE(j) 
		for (k = 0; k < OBJ_TABLE(j)->nCols; k++) {
			//T: if column k is primary column, check = false
			if (OBJ_TABLE(j)->cols[k]->isPrimaryColumn) {
				Boolean check = FALSE;
				char *fullColName;
				STR_INIT(fullColName, OBJ_TABLE(j)->name);
				STR_APPEND(fullColName, ".");
				STR_APPEND(fullColName, OBJ_TABLE(j)->cols[k]->name);

				for (i = 0; i < OBJ_SQ->groupbyElementsNum; i++) {
					if (STR_EQUAL(fullColName, OBJ_SQ->groupbyElements[i])) {
						check = TRUE;
						break;
					}
				}

				FREE(fullColName);
				if (!check) {
					OBJ_KEY_IN_G_CHECK(j) = FALSE;
					/*DMT: break immediately when found 1 primary column doesn't appear in GROUP BY CLAUSE
					It's because the condition for A2 is all columns that joined in primary key must appear in GROUP BY
					So if 1 primary column doesn't appear in GROUP BY, the loop is no longer neccessary.
					Note: Break here is very valuable, because in Database structure design, primary key's columns
					usually appear first. In case the table has many columns, break can save memory resources.
					*/
					break;
				}
			}
		}
	}
}
//#endregion
/*
GENERATE CODE
*/
if (!OBJ_SQ->hasOuterJoin) {

	{ // GENERATE MV'S SQL CODE
	  // 'SQL' macro is an alias of 'sqlWriter' variable
		FILE *sqlWriter = fopen(sqlFile, "w");
		PRINT_TO_FILE(SQL, "create table %s (\n", mvName);

		for (i = 0; i < OBJ_NSELECTED; i++) {
			char *characterLength;
			char *ctype;
			STR_INIT(characterLength, "");
			ctype = createCType(OBJ_SELECTED_COL(i)->type);
			if (STR_EQUAL(ctype, "char *") && STR_INEQUAL(OBJ_SELECTED_COL(i)->characterLength, "")) {
				FREE(characterLength);
				STR_INIT(characterLength, "(");
				STR_APPEND(characterLength, OBJ_SELECTED_COL(i)->characterLength);
				STR_APPEND(characterLength, ")");
			}

			if (OBJ_SELECTED_COL(i)->context == COLUMN_CONTEXT_FREE)
				PRINT_TO_FILE(SQL, "	%s %s%s", OBJ_SELECTED_COL(i)->name, OBJ_SELECTED_COL(i)->type, characterLength);
			else
				PRINT_TO_FILE(SQL, "	%s %s%s", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->type, characterLength);

			if (i != OBJ_NSELECTED - 1)
				PRINT_TO_FILE(SQL, ",\n");
			FREE(characterLength);
			FREE(ctype);
		}
		PRINT_TO_FILE(SQL, "\n);");

		for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
			// insert
			if (OBJ_AGG_INVOLVED_CHECK(j)) {
				PRINT_TO_FILE(SQL, "\n\n/* insert on %s*/", OBJ_TABLE(j)->name);
				PRINT_TO_FILE(SQL, "\ncreate function pmvtg_%s_insert_on_%s() returns trigger as '%s', 'pmvtg_%s_insert_on_%s' language c strict;",
					mvName, OBJ_TABLE(j)->name, dllFile, mvName, OBJ_TABLE(j)->name);

				PRINT_TO_FILE(SQL, "\ncreate trigger pgtg_%s_insert_on_%s after insert on %s for each row execute procedure pmvtg_%s_insert_on_%s();",
					mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);
			}

			// delete
			PRINT_TO_FILE(SQL, "\n\n/* delete on %s*/", OBJ_TABLE(j)->name);
			PRINT_TO_FILE(SQL, "\ncreate function pmvtg_%s_delete_on_%s() returns trigger as '%s', 'pmvtg_%s_delete_on_%s' language c strict;",
				mvName, OBJ_TABLE(j)->name, dllFile, mvName, OBJ_TABLE(j)->name);

			PRINT_TO_FILE(SQL, "\ncreate trigger pgtg_%s_before_delete_on_%s before delete on %s for each row execute procedure pmvtg_%s_delete_on_%s();",
				mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);

			if (!OBJ_KEY_IN_G_CHECK(j) && OBJ_STATIC_BOOL_AGG_MINMAX)
				PRINT_TO_FILE(SQL, "\ncreate trigger pgtg_%s_after_delete_on_%s after delete on %s for each row execute procedure pmvtg_%s_delete_on_%s();",
					mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);

			// update
			PRINT_TO_FILE(SQL, "\n\n/* update on %s*/", OBJ_TABLE(j)->name);
			PRINT_TO_FILE(SQL, "\ncreate function pmvtg_%s_update_on_%s() returns trigger as '%s', 'pmvtg_%s_update_on_%s' language c strict;",
				mvName, OBJ_TABLE(j)->name, dllFile, mvName, OBJ_TABLE(j)->name);

			if ((!OBJ_A2_CHECK || !OBJ_AGG_INVOLVED_CHECK(j)) && (!OBJ_KEY_IN_G_CHECK(j) || OBJ_AGG_INVOLVED_CHECK(j))) {
				PRINT_TO_FILE(SQL, "\ncreate trigger pgtg_%s_before_update_on_%s before update on %s for each row execute procedure pmvtg_%s_update_on_%s();",
					mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);
			}

			PRINT_TO_FILE(SQL, "\ncreate trigger pgtg_%s_after_update_on_%s after update on %s for each row execute procedure pmvtg_%s_update_on_%s();",
				mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);
		}

		fclose(sqlWriter);
	} // --- END OF GENERATING MV'S SQL CODE

	{ // GENERATE C TRIGGER CODE
	  // Macro 'F' is an alias of cWriter variable
		FILE *cWriter = fopen(triggerFile, "w");
		PRINT_TO_FILE(F, "#include \"ctrigger.h\"\n\n");

		// FOREACH TABLE
		for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {

			/*******************************************************************************************************
															INSERT EVENT
			********************************************************************************************************/
			// With insert event, tables which not involved in aggregate calculation do not needed to trigger data
			if (OBJ_AGG_INVOLVED_CHECK(j)) {
				Boolean check;
				// Define trigger function
				char *triggerName;
				STR_INIT(triggerName, "pmvtg_");
				STR_APPEND(triggerName, mvName);
				STR_APPEND(triggerName, "_insert_on_");
				STR_APPEND(triggerName, OBJ_TABLE(j)->name);
				PRINT_TO_FILE(F, "TRIGGER(%s) {\n", triggerName);
				FREE(triggerName);

				// Variable declaration
				PRINT_TO_FILE(F, "	// MV's data\n");
				for (i = 0; i < OBJ_NSELECTED; i++) {
					char *varname = OBJ_SELECTED_COL(i)->varName;
					char *tableName = "<expression>";
					if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
					PRINT_TO_FILE(F, "	char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
				}

				// Table's vars
				PRINT_TO_FILE(F, "	// Table's data\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *varname = OBJ_TABLE(j)->cols[i]->varName;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					PRINT_TO_FILE(F, "	%s %s;\n", ctype, varname);
					if (STR_INEQUAL(ctype, "char *")) {
						if (STR_EQUAL(ctype, "Numeric"))
							PRINT_TO_FILE(F, "	char * str_%s;\n", varname);
						else
							PRINT_TO_FILE(F, "	char str_%s[20];\n", varname);
					}
					FREE(ctype);
				}

				// Standard trigger preparing procedures
				PRINT_TO_FILE(F, "\n	BEGIN;\n\n");

				// Get values of table's fields
				PRINT_TO_FILE(F, "	// Get table's data\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *func;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					char *convert = "";
					if (STR_EQUAL(ctype, "char *"))
						func = "GET_STR_ON_TRIGGERED_ROW";
					else if (STR_EQUAL(ctype, "int")) {
						func = "GET_INT32_ON_TRIGGERED_ROW";
						convert = "INT32_TO_STR";
					}
					else if (STR_EQUAL(ctype, "int64")) {
						func = "GET_INT64_ON_TRIGGERED_ROW";
						convert = "INT64_TO_STR";
					}
					else {
						func = "GET_NUMERIC_ON_TRIGGERED_ROW";
						convert = "NUMERIC_TO_STR";
					}
					PRINT_TO_FILE(F, "	%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						PRINT_TO_FILE(F, "	%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
					FREE(ctype);
				}

				// FCC
				PRINT_TO_FILE(F, "\n	// FCC\n");
				PRINT_TO_FILE(F, "	if (%s) {\n", OBJ_FCC(j));

				// Re-query
				PRINT_TO_FILE(F, "\n		// Re-query\n");
				PRINT_TO_FILE(F, "		DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
				PRINT_TO_FILE(F, "		ADDQ(\" from \");\n");
				PRINT_TO_FILE(F, "		ADDQ(\"%s\");\n", OBJ_SQ->from);
				PRINT_TO_FILE(F, "		ADDQ(\" where true \");\n");
				if (OBJ_SQ->hasWhere) PRINT_TO_FILE(F, "		ADDQ(\" and %s\");\n", OBJ_SQ->where);
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++)
					if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
						else { quote = ""; strprefix = "str_"; }
						PRINT_TO_FILE(F, "		ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
						PRINT_TO_FILE(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
						PRINT_TO_FILE(F, "		ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}
				if (OBJ_SQ->hasGroupby) PRINT_TO_FILE(F, "		ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
				if (OBJ_SQ->hasHaving) PRINT_TO_FILE(F, "		ADDQ(\" having %s\");\n", OBJ_SQ->having);

				PRINT_TO_FILE(F, "		EXEC;\n");

				// Allocate cache
				PRINT_TO_FILE(F, "\n		// Allocate cache\n		DEFR(%d);\n", OBJ_NSELECTED);

				// Save dQ result
				PRINT_TO_FILE(F, "\n		// Save dQ result\n		FOR_EACH_RESULT_ROW(i) {\n");

				for (i = 0; i < OBJ_NSELECTED; i++) {
					PRINT_TO_FILE(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
					PRINT_TO_FILE(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
				}

				PRINT_TO_FILE(F, "		}\n");

				// For each dQi:
				PRINT_TO_FILE(F, "\n		// For each dQi:\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

				k = 0;
				for (i = 0; i < OBJ_NSELECTED; i++)
					PRINT_TO_FILE(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

				// Check if there is a similar row in MV
				PRINT_TO_FILE(F, "\n			// Check if there is a similar row in MV\n");
				PRINT_TO_FILE(F, "			DEFQ(\"select mvcount from %s where true \");\n", mvName);
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype;
						ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						PRINT_TO_FILE(F, "			ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						PRINT_TO_FILE(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "			ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}

				PRINT_TO_FILE(F, "			EXEC;\n");

				PRINT_TO_FILE(F, "\n			if (NO_ROW) {\n");
				PRINT_TO_FILE(F, "				// insert new row to mv\n");
				PRINT_TO_FILE(F, "				DEFQ(\"insert into %s values (\");\n", mvName);

				for (i = 0; i < OBJ_NSELECTED; i++) {
					char *quote;
					char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
					if (STR_EQUAL(ctype, "char *"))
						quote = "'";
					else quote = "";
					PRINT_TO_FILE(F, "				ADDQ(\"%s\");\n", quote);
					PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
					PRINT_TO_FILE(F, "				ADDQ(\"%s\");\n", quote);
					if (i != OBJ_NSELECTED - 1) PRINT_TO_FILE(F, "				ADDQ(\", \");\n");
					FREE(ctype);
				}

				PRINT_TO_FILE(F, "				ADDQ(\")\");\n");
				PRINT_TO_FILE(F, "				EXEC;\n");

				PRINT_TO_FILE(F, "			} else {\n");

				PRINT_TO_FILE(F, "				// update old row\n");
				PRINT_TO_FILE(F, "				DEFQ(\"update %s set \");\n", mvName);
				check = FALSE;
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {

						if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_SUM)) {
							if (check) PRINT_TO_FILE(F, "				ADDQ(\", \");\n");
							check = TRUE;

							PRINT_TO_FILE(F, "				ADDQ(\" %s = %s + \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
						else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_COUNT)) {
							if (check) PRINT_TO_FILE(F, "				ADDQ(\", \");\n");
							check = TRUE;

							PRINT_TO_FILE(F, "				ADDQ(\" %s = %s + \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
						else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_MIN)) {
							if (check) PRINT_TO_FILE(F, "				ADDQ(\", \");\n");
							check = TRUE;

							PRINT_TO_FILE(F, "				ADDQ(\" %s = (case when %s > \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "				ADDQ(\" then \");\n");
							PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "				ADDQ(\" else %s end) \");\n", OBJ_SELECTED_COL(i)->varName);
						}
						else {
							// AF_MAX_
							if (check) PRINT_TO_FILE(F, "				ADDQ(\", \");\n");
							check = TRUE;

							PRINT_TO_FILE(F, "				ADDQ(\" %s = (case when %s < \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "				ADDQ(\" then \");\n");
							PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "				ADDQ(\" else %s end) \");\n", OBJ_SELECTED_COL(i)->varName);
						}
					}

				PRINT_TO_FILE(F, "				ADDQ(\" where true \");\n");
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						PRINT_TO_FILE(F, "				ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "				ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}
				PRINT_TO_FILE(F, "				EXEC;\n");

				PRINT_TO_FILE(F, "			}\n");
				PRINT_TO_FILE(F, "		}\n");

				// End of trigger
				PRINT_TO_FILE(F, "\n	}\n\n	/*\n		Finish\n	*/\n	END;\n}\n\n");

			}// --- END OF CHECK POINT FOR INSERT EVENT (OBJ_AGG_INVOLVED_CHECK)
			 // --- END OF INSERT EVENT

			 /*******************************************************************************************************
															DELETE EVENT
			 ********************************************************************************************************/
			if (TRUE) {
				Boolean check;
				// Define trigger function
				char *triggerName;
				STR_INIT(triggerName, "pmvtg_");
				STR_APPEND(triggerName, mvName);
				STR_APPEND(triggerName, "_delete_on_");
				STR_APPEND(triggerName, OBJ_TABLE(j)->name);
				PRINT_TO_FILE(F, "TRIGGER(%s) {\n", triggerName);
				FREE(triggerName);

				/*
				DMT: According to the A2
				1. if Primary key of table j is in group by clause, it means we can determine what tuples we should
				delete from mv using this key, so that we dont need to determine using its column

				2. if Primary key of table j is NOT IN group by clause, we must determine what tuples should be deleted from MV
				using the table j column
				*/

				/*DMT: second case: Primary key of table j is NOT IN group by clause*/
				if (!OBJ_KEY_IN_G_CHECK(j)) {
					// MV's vars
					PRINT_TO_FILE(F, "	// MV's data\n");
					for (i = 0; i < OBJ_NSELECTED; i++) {
						char *varname = OBJ_SELECTED_COL(i)->varName;
						char *tableName = "<expression>";
						if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
						PRINT_TO_FILE(F, "	char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
					}
				}

				// Table's vars
				PRINT_TO_FILE(F, "	// Table's data\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn || (!OBJ_TABLE(j)->cols[i]->isPrimaryColumn && !OBJ_KEY_IN_G_CHECK(j))) {
						/*puts(OBJ_TABLE(j)->cols[i]->name);
						if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
						puts("primary key");
						}
						else {
						puts("(!OBJ_TABLE(j)->cols[i]->isPrimaryColumn && !OBJ_KEY_IN_G_CHECK(j))");
						}*/
						char *varname = OBJ_TABLE(j)->cols[i]->varName;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						PRINT_TO_FILE(F, "	%s %s;\n", ctype, varname);
						if (STR_INEQUAL(ctype, "char *")) {
							if (STR_EQUAL(ctype, "Numeric"))
								PRINT_TO_FILE(F, "	char * str_%s;\n", varname);
							else
								PRINT_TO_FILE(F, "	char str_%s[20];\n", varname);
						}
						FREE(ctype);
					}
				}

				/*
				DMT: if table j's primary key is in group by clause, it means we can remove the row in MV imediately.
				if table j's primary key is not in group by clause, we need to count how many tuples are being deleting.
				If count == mvcount (count(*) in mv) so just remove the record. otherwise, decrease the count column
				*/
				if (!OBJ_KEY_IN_G_CHECK(j)) {
					// Count
					PRINT_TO_FILE(F, "	char * count;\n");
				}

				// Standard trigger preparing procedures
				PRINT_TO_FILE(F, "\n	BEGIN;\n\n");

				// Get values of table's fields
				PRINT_TO_FILE(F, "	// Get table's data\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn || (!OBJ_TABLE(j)->cols[i]->isPrimaryColumn && !OBJ_KEY_IN_G_CHECK(j))) {
						char *func;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						char *convert = "";
						if (STR_EQUAL(ctype, "char *"))
							func = "GET_STR_ON_TRIGGERED_ROW";
						else if (STR_EQUAL(ctype, "int")) {
							func = "GET_INT32_ON_TRIGGERED_ROW";
							convert = "INT32_TO_STR";
						}
						else if (STR_EQUAL(ctype, "int64")) {
							func = "GET_INT64_ON_TRIGGERED_ROW";
							convert = "INT64_TO_STR";
						}
						else {
							func = "GET_NUMERIC_ON_TRIGGERED_ROW";
							convert = "NUMERIC_TO_STR";
						}
						PRINT_TO_FILE(F, "	%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
						if (STR_INEQUAL(ctype, "char *"))
							PRINT_TO_FILE(F, "	%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
						FREE(ctype);
					}
				}

				// FCC
				PRINT_TO_FILE(F, "\n	// FCC\n");
				//DMT: not neccessary, just comment it
				//PRINT_TO_FILE(F, "	if (%s) {\n", OBJ_FCC(j));
				PRINT_TO_FILE(F, "	if (1) {\n");

				if (OBJ_KEY_IN_G_CHECK(j)) {
					PRINT_TO_FILE(F, "		DEFQ(\"delete from %s where true\");\n", mvName);
					for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
							char *quote;
							char *strprefix;
							char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
							if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
							else { quote = ""; strprefix = "str_"; }
							PRINT_TO_FILE(F, "		ADDQ(\" and %s = %s\");\n", OBJ_TABLE(j)->cols[i]->name, quote);
							PRINT_TO_FILE(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
							PRINT_TO_FILE(F, "		ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}
					}
					PRINT_TO_FILE(F, "		EXEC;\n");
				}
				else {
					if (OBJ_STATIC_BOOL_AGG_MINMAX)
						PRINT_TO_FILE(F, "\n	if (UTRIGGER_FIRED_BEFORE) {\n");
					/*
					TRIGGER BODY START HERE
					*/
					// Re-query
					PRINT_TO_FILE(F, "\n		// Re-query\n");
					PRINT_TO_FILE(F, "		DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
					PRINT_TO_FILE(F, "		ADDQ(\" from \");\n");
					PRINT_TO_FILE(F, "		ADDQ(\"%s\");\n", OBJ_SQ->from);
					PRINT_TO_FILE(F, "		ADDQ(\" where true \");\n");
					if (OBJ_SQ->hasWhere) PRINT_TO_FILE(F, "		ADDQ(\" and %s\");\n", OBJ_SQ->where);
					for (i = 0; i < OBJ_TABLE(j)->nCols; i++)
						if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
							char *quote;
							char *strprefix;
							char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
							if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
							else { quote = ""; strprefix = "str_"; }
							PRINT_TO_FILE(F, "		ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
							PRINT_TO_FILE(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
							PRINT_TO_FILE(F, "		ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}
					if (OBJ_SQ->hasGroupby) PRINT_TO_FILE(F, "		ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
					if (OBJ_SQ->hasHaving) PRINT_TO_FILE(F, "		ADDQ(\" having %s\");\n", OBJ_SQ->having);

					PRINT_TO_FILE(F, "		EXEC;\n");

					// Allocate cache
					PRINT_TO_FILE(F, "\n		// Allocate cache\n		DEFR(%d);\n", OBJ_NSELECTED);

					// Save dQ result
					PRINT_TO_FILE(F, "\n		// Save dQ result\n		FOR_EACH_RESULT_ROW(i) {\n");

					for (i = 0; i < OBJ_NSELECTED; i++) {
						PRINT_TO_FILE(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
						PRINT_TO_FILE(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
					}

					PRINT_TO_FILE(F, "		}\n");

					// For each dQi:
					PRINT_TO_FILE(F, "\n		// For each dQi:\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

					k = 0;
					for (i = 0; i < OBJ_NSELECTED; i++)
						PRINT_TO_FILE(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

					// Check if there is a similar row in MV
					PRINT_TO_FILE(F, "\n			// Check if there is a similar row in MV\n");
					PRINT_TO_FILE(F, "			DEFQ(\"select mvcount from %s where true \");\n", mvName);
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							PRINT_TO_FILE(F, "			ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							PRINT_TO_FILE(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "			ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}

					PRINT_TO_FILE(F, "			EXEC;\n");

					PRINT_TO_FILE(F, "\n			GET_STR_ON_RESULT(count, 0, 1);\n");

					PRINT_TO_FILE(F, "\n			if (STR_EQUAL(count, mvcount)) {\n");
					PRINT_TO_FILE(F, "				// Delete the row\n");
					PRINT_TO_FILE(F, "				DEFQ(\"delete from %s where true\");\n", mvName);

					for (i = 0; i < OBJ_NSELECTED; i++)
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							PRINT_TO_FILE(F, "				ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "				ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}

					PRINT_TO_FILE(F, "				EXEC;\n");

					PRINT_TO_FILE(F, "			} else {\n");

					PRINT_TO_FILE(F, "				// ow, decrease the count column \n");
					PRINT_TO_FILE(F, "				DEFQ(\"update %s set \");\n", mvName);

					//Neu la lan dau tien thi khong in dau ',' tu lan 2 tro di co in dau ',' 
					check = FALSE;
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_SUM)) {
								if (check) PRINT_TO_FILE(F, "				ADDQ(\", \");\n");
								check = TRUE;

								PRINT_TO_FILE(F, "				ADDQ(\" %s = %s - \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
								PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							}
							else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_COUNT)) {
								if (check) PRINT_TO_FILE(F, "				ADDQ(\", \");\n");
								check = TRUE;

								PRINT_TO_FILE(F, "				ADDQ(\" %s = %s - \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
								PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							}
						}
					PRINT_TO_FILE(F, "				ADDQ(\" where true \");\n");
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							PRINT_TO_FILE(F, "				ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "				ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}
					PRINT_TO_FILE(F, "				EXEC;\n");

					PRINT_TO_FILE(F, "			}\n");
					PRINT_TO_FILE(F, "		}\n");

					// --------------------------------------------------
					if (OBJ_STATIC_BOOL_AGG_MINMAX) {
						PRINT_TO_FILE(F, "\n	} else {\n");
						PRINT_TO_FILE(F, "		//Trigger fired after\n");
						PRINT_TO_FILE(F, "		DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
						PRINT_TO_FILE(F, "		ADDQ(\" from \");\n");
						PRINT_TO_FILE(F, "		ADDQ(\"%s\");\n", OBJ_SQ->from);
						PRINT_TO_FILE(F, "		ADDQ(\" where true \");\n");
						if (OBJ_SQ->hasWhere) PRINT_TO_FILE(F, "		ADDQ(\" and %s\");\n", OBJ_SQ->where);
						for (i = 0; i < OBJ_TG_NCOLS(j); i++) {
							char *quote;
							char *strprefix;
							char *ctype = createCType(OBJ_TG_COL(j, i)->type);
							if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
							else { quote = ""; strprefix = "str_"; }
							PRINT_TO_FILE(F, "		ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TG_COL(j, i)->name, quote);
							PRINT_TO_FILE(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TG_COL(j, i)->varName);
							PRINT_TO_FILE(F, "		ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}
						if (OBJ_SQ->hasGroupby) PRINT_TO_FILE(F, "		ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
						if (OBJ_SQ->hasHaving) PRINT_TO_FILE(F, "			ADDQ(\" having %s\");\n", OBJ_SQ->having);

						PRINT_TO_FILE(F, "		EXEC;\n");

						PRINT_TO_FILE(F, "\n		DEFR(%d);\n", OBJ_NSELECTED);

						PRINT_TO_FILE(F, "\n		FOR_EACH_RESULT_ROW(i) {\n");

						for (i = 0; i < OBJ_NSELECTED; i++) {
							PRINT_TO_FILE(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
							PRINT_TO_FILE(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}

						PRINT_TO_FILE(F, "		}\n");

						// For each dQi:
						PRINT_TO_FILE(F, "\n		// For each dQi:\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

						k = 0;
						for (i = 0; i < OBJ_NSELECTED; i++)
							PRINT_TO_FILE(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

						PRINT_TO_FILE(F, "			DEFQ(\"update %s set mvcount = mvcount \");\n", mvName);

						for (i = 0; i < OBJ_NSELECTED; i++) {
							if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_MIN) || STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_MAX_)) {
								PRINT_TO_FILE(F, "			ADDQ(\", %s = \");\n", OBJ_SELECTED_COL(i)->varName);
								PRINT_TO_FILE(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							}
						}

						PRINT_TO_FILE(F, "			ADDQ(\" where true \");\n");
						for (i = 0; i < OBJ_NSELECTED; i++)
							if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
								char *quote;
								char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
								if (STR_EQUAL(ctype, "char *")) quote = "\'";
								else quote = "";
								PRINT_TO_FILE(F, "			ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
								PRINT_TO_FILE(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
								PRINT_TO_FILE(F, "			ADDQ(\"%s \");\n", quote);
								FREE(ctype);
							}
						PRINT_TO_FILE(F, "			EXEC;\n");

						PRINT_TO_FILE(F, "		}\n");

						PRINT_TO_FILE(F, "\n	}\n");
					}
				}

				// End of trigger
				PRINT_TO_FILE(F, "\n	}\n\n	END;\n}\n\n");

			} // --- END OF CHECK POINT FOR DELETE EVENT
			  // --- END OF DELETE EVENT

			  /*******************************************************************************************************
															UPDATE EVENT
			  ********************************************************************************************************/
			  //T: Nếu TVG chỉ có SUM và/hoặc COUNT, và bảng j tham gia vào hàm AF
			if (OBJ_A2_CHECK && OBJ_AGG_INVOLVED_CHECK(j)) {

				Boolean check;
				// Define trigger function
				char *triggerName;
				STR_INIT(triggerName, "pmvtg_");
				STR_APPEND(triggerName, mvName);
				STR_APPEND(triggerName, "_update_on_");
				STR_APPEND(triggerName, OBJ_TABLE(j)->name);
				PRINT_TO_FILE(F, "TRIGGER(%s) {\n", triggerName);
				FREE(triggerName);
				PRINT_TO_FILE(F, "//OBJ_A2_CHECK && OBJ_AGG_INVOLVED_CHECK(j)\n");
				// MV's data (old and new rows)
				PRINT_TO_FILE(F, "	// MV's data (old and new rows)\n", triggerName);
				for (i = 0; i < OBJ_NSELECTED; i++) {
					char *varname = OBJ_SELECTED_COL(i)->varName;
					char *tableName = "<expression>";
					if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
					if (i < OBJ_NSELECTED - 1) {
						PRINT_TO_FILE(F, "	char *old_%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
						PRINT_TO_FILE(F, "	char *new_%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
					}
					else {
						PRINT_TO_FILE(F, "	char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
					}
				}

				// Old and new rows' data
				PRINT_TO_FILE(F, "	// Old and new rows' data\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *varname = OBJ_TABLE(j)->cols[i]->varName;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					PRINT_TO_FILE(F, "	%s old_%s;\n", ctype, varname);
					if (STR_INEQUAL(ctype, "char *")) {
						if (STR_EQUAL(ctype, "Numeric"))
							PRINT_TO_FILE(F, "	char * old_str_%s;\n", varname);
						else
							PRINT_TO_FILE(F, "	char old_str_%s[20];\n", varname);
					}
					PRINT_TO_FILE(F, "	%s new_%s;\n", ctype, varname);
					if (STR_INEQUAL(ctype, "char *")) {
						if (STR_EQUAL(ctype, "Numeric"))
							PRINT_TO_FILE(F, "	char * new_str_%s;\n", varname);
						else
							PRINT_TO_FILE(F, "	char new_str_%s[20];\n", varname);
					}

					FREE(ctype);
				}
				PRINT_TO_FILE(F, "	char * old_count;");
				PRINT_TO_FILE(F, "\n	char * new_count;\n");

				// Begin
				PRINT_TO_FILE(F, "\n	BEGIN;\n\n");

				// Get data of old rows
				PRINT_TO_FILE(F, "	// Get data of old rows\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *func;
					char *convert = "";
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					if (STR_EQUAL(ctype, "char *"))
						func = "GET_STR_ON_TRIGGERED_ROW";
					else if (STR_EQUAL(ctype, "int")) {
						func = "GET_INT32_ON_TRIGGERED_ROW";
						convert = "INT32_TO_STR";
					}
					else if (STR_EQUAL(ctype, "int64")) {
						func = "GET_INT64_ON_TRIGGERED_ROW";
						convert = "INT64_TO_STR";
					}
					else {
						func = "GET_NUMERIC_ON_TRIGGERED_ROW";
						convert = "NUMERIC_TO_STR";
					}
					PRINT_TO_FILE(F, "	%s(old_%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						PRINT_TO_FILE(F, "	%s(old_%s, old_str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);

					FREE(ctype);
				}

				// Get data of new rows
				PRINT_TO_FILE(F, "	// Get data of new rows\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *func;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					char *convert = "";
					if (STR_EQUAL(ctype, "char *"))
						func = "GET_STR_ON_NEW_ROW";
					else if (STR_EQUAL(ctype, "int")) {
						func = "GET_INT32_ON_NEW_ROW";
						convert = "INT32_TO_STR";
					}
					else if (STR_EQUAL(ctype, "int64")) {
						func = "GET_INT64_ON_NEW_ROW";
						convert = "INT64_TO_STR";
					}
					else {
						func = "GET_NUMERIC_ON_NEW_ROW";
						convert = "NUMERIC_TO_STR";
					}
					PRINT_TO_FILE(F, "	%s(new_%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						PRINT_TO_FILE(F, "	%s(new_%s, new_str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
					FREE(ctype);
				}

				// FCC
				PRINT_TO_FILE(F, "\n	// FCC - A2_CHECK = TRUE & Table involved AGG \n");
				PRINT_TO_FILE(F, "	if ((%s) || (%s)) {\n", OBJ_FCC_OLD(j), OBJ_FCC_NEW(j));
				// PRINT_TO_FILE(F, "		INIT_UPDATE_A2_FLAG;\n");

				// Query
				PRINT_TO_FILE(F, "		QUERY(\"select \");\n");
				for (i = 0; i < OBJ_NSELECTED; i++) {
					if (OBJ_SELECTED_COL(i)->table) {
						PRINT_TO_FILE(F, "		_QUERY(\"%s.%s\");\n", OBJ_SELECTED_COL(i)->table->name, OBJ_SELECTED_COL(i)->name);
					}
					else {
						int nParts, partsType[MAX_NUMBER_OF_ELEMENTS];
						char *parts[MAX_NUMBER_OF_ELEMENTS];
						A2Split(j, OBJ_SELECTED_COL(i)->funcArg, &nParts, partsType, parts);
						PRINT_TO_FILE(F, "		_QUERY(\"%s\");\n", OBJ_SELECTED_COL(i)->func);
						for (k = 0; k < nParts; k++) {
							if (partsType[k] != j) {
								PRINT_TO_FILE(F, "//--- part k\n");
								PRINT_TO_FILE(F, "	 	_QUERY(\"%s\");\n", parts[k]);
							}
							else {
								PRINT_TO_FILE(F, "		_QUERY(old_%s);\n", parts[k]);
							}
						}


						PRINT_TO_FILE(F, "		_QUERY(\")\");\n");

						for (k = 0; k < nParts; k++) {
							FREE(parts[k]);
						}
					}
					if (i < OBJ_NSELECTED - 1) {
						PRINT_TO_FILE(F, "		_QUERY(\",\");\n");
					}
				}
				PRINT_TO_FILE(F, "		_QUERY(\" from \");\n");
				check = FALSE;
				for (i = 0; i < OBJ_SQ->fromElementsNum; i++) {
					if (i != j) {
						if (check)
							PRINT_TO_FILE(F, "		_QUERY(\", %s\");\n", OBJ_TABLE(i)->name);
						else
							PRINT_TO_FILE(F, "		_QUERY(\"%s\");\n", OBJ_TABLE(i)->name);
						check = TRUE;
					}
				}
				PRINT_TO_FILE(F, "		_QUERY(\" where %s \");\n", OBJ_A2_WHERE_CLAUSE(j));
				/*for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
				if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
				char *quote;
				char *strprefix;
				char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
				if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = "old_"; }
				else { quote = ""; strprefix = "old_str_"; }
				PRINT_TO_FILE(F, "		_QUERY(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
				PRINT_TO_FILE(F, "		_QUERY(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
				PRINT_TO_FILE(F, "		_QUERY(\"%s \");\n", quote);
				FREE(ctype);
				}
				}*/

				for (i = 0; i < OBJ_TG_NCOLS(j); i++) {
					char *quote;
					char *strprefix;
					char *ctype = createCType(OBJ_TG_COL(j, i)->type);
					if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = "old_"; }
					else { quote = ""; strprefix = "old_str_"; }
					PRINT_TO_FILE(F, "		_QUERY(\" and %s.%s = %s\");\n", OBJ_A2_TG_TABLE(j, i), OBJ_TG_COL(j, i)->name, quote);
					PRINT_TO_FILE(F, "		_QUERY(%s%s);\n", strprefix, OBJ_TG_COL(j, i)->varName);
					PRINT_TO_FILE(F, "		_QUERY(\"%s \");\n", quote);
					FREE(ctype);
				}

				//DMT: NEW CODE ADDED 
				for (i = 0; i < OBJ_BIN_NCONDITIONS(j); i++) {
					char *quote;
					char *strprefix;
					char *condition[MAX_NUMBER_OF_ELEMENTS];
					int len = 0;
					char *bin_condition = copy(OBJ_BIN_CONDITIONS(j, i));
					char *pos = strstr(bin_condition, "=");
					char *operator = NULL;
					switch (*(pos - 1)) {
						case '>':
							operator = ">=";
							break;
						case '<':
							operator = "<=";
							break;
						case '!':
							operator = "!=";
							break;
						default:
							operator = "==";
							break;
					}

					char *ctype = "";
					char *leftCondition = "";
					char *rightVarName = "";
					split(bin_condition, operator, condition, &len, FALSE);
					for (int k = 0; k < len; k++) {
						char *dotPos = findSubString(condition[k], ".");
						char *currentTable = getPrecededTableName(condition[k], dotPos + 1);
						if (STR_INEQUAL_CI(currentTable, OBJ_TABLE(j)->name)) {
							leftCondition = condition[k];
						}
						else {
							for (int l = 0; l < OBJ_TABLE(j)->nCols; l++) {
								if (STR_EQUAL_CI(OBJ_TABLE(j)->cols[l]->name, trim((dotPos + 1)))) {
									ctype = createCType(OBJ_TABLE(j)->cols[l]->type);
									rightVarName = OBJ_TABLE(j)->cols[l]->varName;
								}
							}
						}
					}

					if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = "old_"; }
					else { quote = ""; strprefix = "old_str_"; }
					if (STR_EQUAL_CI(operator, "==")) {
						operator = "=";
					}
					PRINT_TO_FILE(F, "		_QUERY(\" and %s %s %s\");\n", leftCondition, operator, quote);
					PRINT_TO_FILE(F, "		_QUERY(%s%s);\n", strprefix, rightVarName);
					PRINT_TO_FILE(F, "		_QUERY(\"%s \");\n", quote);
				}



				PRINT_TO_FILE(F, "		_QUERY(\" group by %s \");\n", OBJ_SQ->groupby);
				PRINT_TO_FILE(F, "		_QUERY(\" union all \");\n");
				//PRINT_TO_FILE(F, "		_QUERY(\" %s \");\n", OBJ_SELECT_CLAUSE);
				PRINT_TO_FILE(F, "		_QUERY(\"select \");\n");
				for (i = 0; i < OBJ_NSELECTED; i++) {
					if (OBJ_SELECTED_COL(i)->table) {
						PRINT_TO_FILE(F, "		_QUERY(\"%s.%s\");\n", OBJ_SELECTED_COL(i)->table->name, OBJ_SELECTED_COL(i)->name);
					}
					else {
						int nParts, partsType[MAX_NUMBER_OF_ELEMENTS];
						char *parts[MAX_NUMBER_OF_ELEMENTS];
						A2Split(j, OBJ_SELECTED_COL(i)->funcArg, &nParts, partsType, parts);

						PRINT_TO_FILE(F, "		_QUERY(\"%s\");\n", OBJ_SELECTED_COL(i)->func);
						for (k = 0; k < nParts; k++) {
							if (partsType[k] != j) {
								PRINT_TO_FILE(F, "		_QUERY(\"%s\");\n", parts[k]);
							}
							else {
								PRINT_TO_FILE(F, "		_QUERY(new_%s);\n", parts[k]);
							}
						}

						PRINT_TO_FILE(F, "		_QUERY(\")\");\n");

						for (k = 0; k < nParts; k++) {
							FREE(parts[k]);
						}
					}
					if (i < OBJ_NSELECTED - 1) {
						PRINT_TO_FILE(F, "		_QUERY(\",\");\n");
					}
				}
				PRINT_TO_FILE(F, "		_QUERY(\" from \");\n");
				check = FALSE;
				for (i = 0; i < OBJ_SQ->fromElementsNum; i++) {
					if (i != j) {
						if (check)
							PRINT_TO_FILE(F, "		_QUERY(\", %s\");\n", OBJ_TABLE(i)->name);
						else
							PRINT_TO_FILE(F, "		_QUERY(\"%s\");\n", OBJ_TABLE(i)->name);
						check = TRUE;
					}
				}
				PRINT_TO_FILE(F, "		_QUERY(\" where %s \");\n", OBJ_A2_WHERE_CLAUSE(j));
				/*for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
				if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
				char *quote;
				char *strprefix;
				char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
				if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = "new_"; }
				else { quote = ""; strprefix = "new_str_"; }
				PRINT_TO_FILE(F, "		_QUERY(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
				PRINT_TO_FILE(F, "		_QUERY(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
				PRINT_TO_FILE(F, "		_QUERY(\"%s \");\n", quote);
				FREE(ctype);
				}
				}*/
				for (i = 0; i < OBJ_TG_NCOLS(j); i++) {
					char *quote;
					char *strprefix;
					char *ctype = createCType(OBJ_TG_COL(j, i)->type);
					if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = "new_"; }
					else { quote = ""; strprefix = "new_str_"; }
					PRINT_TO_FILE(F, "		_QUERY(\" and %s.%s = %s\");\n", OBJ_A2_TG_TABLE(j, i), OBJ_TG_COL(j, i)->name, quote);
					PRINT_TO_FILE(F, "		_QUERY(%s%s);\n", strprefix, OBJ_TG_COL(j, i)->varName);
					PRINT_TO_FILE(F, "		_QUERY(\"%s \");\n", quote);
					FREE(ctype);
				}

				//DMT: NEW CODE ADDED 
				for (i = 0; i < OBJ_BIN_NCONDITIONS(j); i++) {
					char *quote;
					char *strprefix;
					char *condition[MAX_NUMBER_OF_ELEMENTS];
					int len = 0;
					char *bin_condition = copy(OBJ_BIN_CONDITIONS(j, i));
					char *pos = strstr(bin_condition, "=");
					char *operator = NULL;
					switch (*(pos - 1)) {
					case '>':
						operator = ">=";
						break;
					case '<':
						operator = "<=";
						break;
					case '!':
						operator = "!=";
						break;
					default:
						operator = "==";
						break;
					}

					//T: neu operator ko hop le
					if (operator == NULL) {
						break;
					}
					char *ctype = "";
					char *leftCondition = "";
					char *rightVarName = "";
					split(bin_condition, operator, condition, &len, FALSE);
					for (int k = 0; k < len; k++) {
						char *dotPos = findSubString(condition[k], ".");
						char *currentTable = getPrecededTableName(condition[k], dotPos + 1);
						if (STR_INEQUAL_CI(currentTable, OBJ_TABLE(j)->name)) {
							leftCondition = condition[k];
						}
						else {
							for (int l = 0; l < OBJ_TABLE(j)->nCols; l++) {
								if (STR_EQUAL_CI(OBJ_TABLE(j)->cols[l]->name, trim((dotPos + 1)))) {
									ctype = createCType(OBJ_TABLE(j)->cols[l]->type);
									rightVarName = OBJ_TABLE(j)->cols[l]->varName;
								}
							}
						}
					}

					if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = "old_"; }
					else { quote = ""; strprefix = "new_str_"; }
					if (STR_EQUAL_CI(operator, "==")) {
						operator = "=";
					}
					PRINT_TO_FILE(F, "		_QUERY(\" and %s %s %s\");\n", leftCondition, operator, quote);
					PRINT_TO_FILE(F, "		_QUERY(%s%s);\n", strprefix, rightVarName);
					PRINT_TO_FILE(F, "		_QUERY(\"%s \");\n", quote);
				}

				PRINT_TO_FILE(F, "		_QUERY(\" group by %s \");\n", OBJ_SQ->groupby);
				PRINT_TO_FILE(F, "		INF(query);\n\n");
				PRINT_TO_FILE(F, "		EXECUTE_QUERY;\n\n");

				PRINT_TO_FILE(F, "		ALLOCATE_CACHE;\n\n");

				PRINT_TO_FILE(F, "		A2_INIT_CYCLE;\n\n");

				PRINT_TO_FILE(F, "		FOR_EACH_RESULT_ROW(i) {\n");
				PRINT_TO_FILE(F, "			if (A2_VALIDATE_CYCLE) {\n");
				//PRINT_TO_FILE(F, "			if (%s) {\n", OBJ_FCC_OLD(j));
				for (i = 0; i < OBJ_NSELECTED - 1; i++) {
					PRINT_TO_FILE(F, "				GET_STR_ON_RESULT(old_%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
					PRINT_TO_FILE(F, "				SAVE_TO_CACHE(old_%s);\n", OBJ_SELECTED_COL(i)->varName);
				}
				PRINT_TO_FILE(F, "				GET_STR_ON_RESULT(old_count, i, %d);\n", OBJ_NSELECTED);
				PRINT_TO_FILE(F, "				SAVE_TO_CACHE(old_count);\n");
				//PRINT_TO_FILE(F, "			}\n");
				PRINT_TO_FILE(F, "			} else {\n");
				//PRINT_TO_FILE(F, "			if (%s) {\n", OBJ_FCC_NEW(j));
				for (i = 0; i < OBJ_NSELECTED - 1; i++) {
					PRINT_TO_FILE(F, "				GET_STR_ON_RESULT(new_%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
					PRINT_TO_FILE(F, "				SAVE_TO_CACHE(new_%s);\n", OBJ_SELECTED_COL(i)->varName);
				}
				PRINT_TO_FILE(F, "				GET_STR_ON_RESULT(new_count, i, %d);\n", OBJ_NSELECTED);
				PRINT_TO_FILE(F, "				SAVE_TO_CACHE(new_count);\n");

				//PRINT_TO_FILE(F, "			}\n");
				PRINT_TO_FILE(F, "			}\n");
				PRINT_TO_FILE(F, "		}\n\n");

				PRINT_TO_FILE(F, "		A2_INIT_CYCLE;\n\n");

				PRINT_TO_FILE(F, "		FOR_EACH_CACHE_ROW(i) {\n");
				PRINT_TO_FILE(F, "			if (A2_VALIDATE_CYCLE) {\n");
				PRINT_TO_FILE(F, "			if (%s) {\n", OBJ_FCC_OLD(j));
				for (i = 0; i < OBJ_NSELECTED - 1; i++)
					PRINT_TO_FILE(F, "				old_%s = GET_FROM_CACHE(i, %d);\n", OBJ_SELECTED_COL(i)->varName, i);
				PRINT_TO_FILE(F, "				old_count = GET_FROM_CACHE(i, %d);\n\n", OBJ_NSELECTED - 1);

				PRINT_TO_FILE(F, "				QUERY(\"select mvcount from %s where true \");\n", mvName);
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						PRINT_TO_FILE(F, "				_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						PRINT_TO_FILE(F, "				_QUERY(old_%s);\n", OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "				_QUERY(\"%s \");\n", quote);
						FREE(ctype);
					}

				PRINT_TO_FILE(F, "				EXECUTE_QUERY;\n\n");
				PRINT_TO_FILE(F, "				GET_STR_ON_RESULT(mvcount, 0, 1);\n\n");

				PRINT_TO_FILE(F, "				if (STR_EQUAL(old_count, mvcount)) {\n");
				PRINT_TO_FILE(F, "					QUERY(\"delete from %s where true\");\n", mvName);

				for (i = 0; i < OBJ_NSELECTED; i++) {
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						PRINT_TO_FILE(F, "					_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						PRINT_TO_FILE(F, "					_QUERY(old_%s);\n", OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "					_QUERY(\"%s \");\n", quote);
						FREE(ctype);
					}
				}

				PRINT_TO_FILE(F, "					EXECUTE_QUERY;\n\n");

				PRINT_TO_FILE(F, "				} else {\n");

				PRINT_TO_FILE(F, "					QUERY(\"update %s set \");\n", mvName);

				check = FALSE;
				for (i = 0; i < OBJ_NSELECTED - 1; i++) {
					if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {

						if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_SUM)) {
							if (check) PRINT_TO_FILE(F, "					_QUERY(\", \");\n");
							check = TRUE;

							PRINT_TO_FILE(F, "					_QUERY(\" %s = %s - \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					_QUERY(old_%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
						else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_COUNT)) {
							if (check) PRINT_TO_FILE(F, "					_QUERY(\", \");\n");
							check = TRUE;

							PRINT_TO_FILE(F, "					_QUERY(\" %s = %s - \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					_QUERY(old_%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
					}
				}
				if (check) PRINT_TO_FILE(F, "					_QUERY(\", \");\n");
				PRINT_TO_FILE(F, "					_QUERY(\" mvcount = mvcount - \");\n");
				PRINT_TO_FILE(F, "					_QUERY(old_count);\n");

				PRINT_TO_FILE(F, "					_QUERY(\" where true \");\n");
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						PRINT_TO_FILE(F, "					_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						PRINT_TO_FILE(F, "					_QUERY(old_%s);\n", OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "					_QUERY(\"%s \");\n", quote);
						FREE(ctype);
					}
				PRINT_TO_FILE(F, "					EXECUTE_QUERY;\n");
				PRINT_TO_FILE(F, "				}\n");
				PRINT_TO_FILE(F, "			}\n");
				PRINT_TO_FILE(F, "			} else {\n");
				PRINT_TO_FILE(F, "			if (%s) {\n", OBJ_FCC_NEW(j));

				for (i = 0; i < OBJ_NSELECTED - 1; i++)
					PRINT_TO_FILE(F, "				new_%s = GET_FROM_CACHE(i, %d);\n", OBJ_SELECTED_COL(i)->varName, i);
				PRINT_TO_FILE(F, "				new_count = GET_FROM_CACHE(i, %d);\n\n", OBJ_NSELECTED - 1);

				PRINT_TO_FILE(F, "				QUERY(\"select mvcount from %s where true \");\n", mvName);
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						PRINT_TO_FILE(F, "				_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						PRINT_TO_FILE(F, "				_QUERY(new_%s);\n", OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "				_QUERY(\"%s \");\n", quote);
						FREE(ctype);
					}

				PRINT_TO_FILE(F, "				EXECUTE_QUERY;\n\n");

				PRINT_TO_FILE(F, "				if (NO_ROW) {\n");
				PRINT_TO_FILE(F, "					QUERY(\"insert into %s values (\");\n", mvName);

				for (i = 0; i < OBJ_NSELECTED - 1; i++) {
					char *quote;
					char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
					if (STR_EQUAL(ctype, "char *"))
						quote = "'";
					else quote = "";
					PRINT_TO_FILE(F, "					_QUERY(\"%s\");\n", quote);
					PRINT_TO_FILE(F, "					_QUERY(new_%s);\n", OBJ_SELECTED_COL(i)->varName);
					PRINT_TO_FILE(F, "					_QUERY(\"%s\");\n", quote);
					PRINT_TO_FILE(F, "					_QUERY(\", \");\n");
					FREE(ctype);
				}
				PRINT_TO_FILE(F, "					_QUERY(new_count);\n");

				PRINT_TO_FILE(F, "					_QUERY(\")\");\n");
				PRINT_TO_FILE(F, "					EXECUTE_QUERY;\n");

				PRINT_TO_FILE(F, "				} else {\n");
				PRINT_TO_FILE(F, "					QUERY(\"update %s set \");\n", mvName);

				check = FALSE;
				for (i = 0; i < OBJ_NSELECTED - 1; i++) {
					if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_SUM)) {
							if (check) PRINT_TO_FILE(F, "					ADDQ(\", \");\n");
							check = TRUE;

							PRINT_TO_FILE(F, "					ADDQ(\" %s = %s + \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					ADDQ(new_%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
						else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_COUNT)) {
							if (check) PRINT_TO_FILE(F, "					ADDQ(\", \");\n");
							check = TRUE;

							PRINT_TO_FILE(F, "					ADDQ(\" %s = %s + \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					ADDQ(new_%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
					}
				}
				if (check) PRINT_TO_FILE(F, "					_QUERY(\", \");\n");
				PRINT_TO_FILE(F, "					_QUERY(\" mvcount = mvcount + \");\n");
				PRINT_TO_FILE(F, "					_QUERY(new_count);\n");

				PRINT_TO_FILE(F, "					_QUERY(\" where true \");\n");
				for (i = 0; i < OBJ_NSELECTED; i++) {
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						PRINT_TO_FILE(F, "					_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						PRINT_TO_FILE(F, "					_QUERY(new_%s);\n", OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "					_QUERY(\"%s \");\n", quote);
						FREE(ctype);
					}
				}
				PRINT_TO_FILE(F, "					EXECUTE_QUERY;\n");
				PRINT_TO_FILE(F, "				}\n");
				PRINT_TO_FILE(F, "			}\n");
				PRINT_TO_FILE(F, "			}\n");
				PRINT_TO_FILE(F, "		}\n");
				PRINT_TO_FILE(F, "	}\n");
				PRINT_TO_FILE(F, "	END;\n");
				PRINT_TO_FILE(F, "}\n\n");

			}
			//T: Nếu tất cả các cột tạo nên khóa tham gia vào group by và ko có cột nào trong số đó tham gia vào AF
			else if (OBJ_KEY_IN_G_CHECK(j) && !OBJ_AGG_INVOLVED_CHECK(j)) {
				//puts("OBJ_KEY_IN_G_CHECK(j) && !OBJ_AGG_INVOLVED_CHECK(j)");
				Boolean check;
				// Define trigger function
				char *triggerName;
				STR_INIT(triggerName, "pmvtg_");
				STR_APPEND(triggerName, mvName);
				STR_APPEND(triggerName, "_update_on_");
				STR_APPEND(triggerName, OBJ_TABLE(j)->name);
				PRINT_TO_FILE(F, "TRIGGER(%s) {\n", triggerName);
				FREE(triggerName);
				PRINT_TO_FILE(F, "//OBJ_KEY_IN_G_CHECK(j) && !OBJ_AGG_INVOLVED_CHECK(j)\n");

				// Old and new rows' data
				PRINT_TO_FILE(F, "	// Old and new rows' data\n", triggerName);
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *varname = OBJ_TABLE(j)->cols[i]->varName;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					PRINT_TO_FILE(F, "	%s old_%s;\n", ctype, varname);
					if (STR_INEQUAL(ctype, "char *")) {
						if (STR_EQUAL(ctype, "Numeric"))
							PRINT_TO_FILE(F, "	char * old_str_%s;\n", varname);
						else
							PRINT_TO_FILE(F, "	char old_str_%s[20];\n", varname);
					}

					PRINT_TO_FILE(F, "	%s new_%s;\n", ctype, varname);
					if (STR_INEQUAL(ctype, "char *")) {
						if (STR_EQUAL(ctype, "Numeric"))
							PRINT_TO_FILE(F, "	char * new_str_%s;\n", varname);
						else
							PRINT_TO_FILE(F, "	char new_str_%s[20];\n", varname);
					}

					FREE(ctype);


				}

				// Begin
				PRINT_TO_FILE(F, "\n	BEGIN;\n\n");

				// Get data of old rows
				PRINT_TO_FILE(F, "	// Get data of old rows\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *func;
					char *convert = "";
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					if (STR_EQUAL(ctype, "char *"))
						func = "GET_STR_ON_TRIGGERED_ROW";
					else if (STR_EQUAL(ctype, "int")) {
						func = "GET_INT32_ON_TRIGGERED_ROW";
						convert = "INT32_TO_STR";
					}
					else if (STR_EQUAL(ctype, "int64")) {
						func = "GET_INT64_ON_TRIGGERED_ROW";
						convert = "INT64_TO_STR";
					}
					else {
						func = "GET_NUMERIC_ON_TRIGGERED_ROW";
						convert = "NUMERIC_TO_STR";
					}
					PRINT_TO_FILE(F, "	%s(old_%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						PRINT_TO_FILE(F, "	%s(old_%s, old_str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);

					FREE(ctype);
				}

				// Get data of new rows
				PRINT_TO_FILE(F, "	// Get data of new rows\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *func;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					char *convert = "";
					if (STR_EQUAL(ctype, "char *"))
						func = "GET_STR_ON_NEW_ROW";
					else if (STR_EQUAL(ctype, "int")) {
						func = "GET_INT32_ON_NEW_ROW";
						convert = "INT32_TO_STR";
					}
					else if (STR_EQUAL(ctype, "int64")) {
						func = "GET_INT64_ON_NEW_ROW";
						convert = "INT64_TO_STR";
					}
					else {
						func = "GET_NUMERIC_ON_NEW_ROW";
						convert = "NUMERIC_TO_STR";
					}
					PRINT_TO_FILE(F, "	%s(new_%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						PRINT_TO_FILE(F, "	%s(new_%s, new_str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
					FREE(ctype);
				}

				// FCC
				PRINT_TO_FILE(F, "\n	// FCC\n");
				//DMT: Tran Trong Nhan's old code
				//PRINT_TO_FILE(F, "	if (%s) {\n", OBJ_FCC(j));
				//DMT: Do Minh Tuan's new code
				/*
				Because according to the A2: KEY_IN_G(j) && !AGG(j), for update:
				B1: Filter from dTi_old records that dont meet FCC
				dTi_old, -> OBJ_FCC_OLD is the solution
				*/
				/* DELETE FROM MV IF FCC_OLD IS TRUE AND FCC_NEW IS FALSE */
				PRINT_TO_FILE(F, "	if ((%s) && !(%s)) {\n", OBJ_FCC_OLD(j), OBJ_FCC_NEW(j));
				PRINT_TO_FILE(F, "		DEFQ(\"delete from %s where true \");\n", mvName);
				for (i = 0; i < OBJ_NSELECTED; i++) {
					if (OBJ_SELECTED_COL(i)->context == COLUMN_CONTEXT_FREE
						&& OBJ_SELECTED_COL(i)->isPrimaryColumn
						&& OBJ_SELECTED_COL(i)->table == OBJ_TABLE(j)) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
						else { quote = ""; strprefix = "str_"; }

						PRINT_TO_FILE(F, "		_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						PRINT_TO_FILE(F, "		_QUERY(old_%s%s_table);\n", strprefix, OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "		_QUERY(\"%s \");\n", quote);
						FREE(ctype);
					}
				}
				PRINT_TO_FILE(F, "		EXECUTE_QUERY;\n");
				PRINT_TO_FILE(F, "	}\n");
				
				/* INSERT INTO MV IF FCC_OLD IS FALSE AND FCC_NEW IS TRUE */
				PRINT_TO_FILE(F, "	if (!(%s) && (%s)) {\n", OBJ_FCC_OLD(j), OBJ_FCC_NEW(j));

				// Variable declaration
				PRINT_TO_FILE(F, "		// MV's data\n");
				for (i = 0; i < OBJ_NSELECTED; i++) {
					char *varname = OBJ_SELECTED_COL(i)->varName;
					char *tableName = "<expression>";
					if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
					PRINT_TO_FILE(F, "		char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
				}

				// Table's vars
				PRINT_TO_FILE(F, "	// Table's data\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *varname = OBJ_TABLE(j)->cols[i]->varName;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					PRINT_TO_FILE(F, "		%s %s;\n", ctype, varname);
					if (STR_INEQUAL(ctype, "char *")) {
						if (STR_EQUAL(ctype, "Numeric"))
							PRINT_TO_FILE(F, "		char * str_%s;\n", varname);
						else
							PRINT_TO_FILE(F, "		char str_%s[20];\n", varname);
					}
					FREE(ctype);
				}
				// Re-query
				PRINT_TO_FILE(F, "\n		// Re-query\n");
				PRINT_TO_FILE(F, "		DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
				PRINT_TO_FILE(F, "		ADDQ(\" from \");\n");
				PRINT_TO_FILE(F, "		ADDQ(\"%s\");\n", OBJ_SQ->from);
				PRINT_TO_FILE(F, "		ADDQ(\" where true \");\n");
				if (OBJ_SQ->hasWhere) PRINT_TO_FILE(F, "		ADDQ(\" and %s\");\n", OBJ_SQ->where);
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++)
					if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
						else { quote = ""; strprefix = "str_"; }
						PRINT_TO_FILE(F, "		ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
						PRINT_TO_FILE(F, "		ADDQ(new_%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
						PRINT_TO_FILE(F, "		ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}
				if (OBJ_SQ->hasGroupby) PRINT_TO_FILE(F, "		ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
				if (OBJ_SQ->hasHaving) PRINT_TO_FILE(F, "		ADDQ(\" having %s\");\n", OBJ_SQ->having);

				PRINT_TO_FILE(F, "		EXEC;\n");

				// Allocate cache
				PRINT_TO_FILE(F, "\n		// Allocate cache\n		DEFR(%d);\n", OBJ_NSELECTED);

				// Save dQ result
				PRINT_TO_FILE(F, "\n		// Save dQ result\n		FOR_EACH_RESULT_ROW(i) {\n");

				for (i = 0; i < OBJ_NSELECTED; i++) {
					PRINT_TO_FILE(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
					PRINT_TO_FILE(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
				}

				PRINT_TO_FILE(F, "		}\n");

				// For each dQi:
				PRINT_TO_FILE(F, "\n	// For each dQi:\n	FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

				k = 0;
				for (i = 0; i < OBJ_NSELECTED; i++)
					PRINT_TO_FILE(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

				PRINT_TO_FILE(F, "			// insert new row to mv\n");
				PRINT_TO_FILE(F, "			DEFQ(\"insert into %s values (\");\n", mvName);

				for (i = 0; i < OBJ_NSELECTED; i++) {
					char *quote;
					char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
					if (STR_EQUAL(ctype, "char *"))
						quote = "'";
					else quote = "";
					PRINT_TO_FILE(F, "			ADDQ(\"%s\");\n", quote);
					PRINT_TO_FILE(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
					PRINT_TO_FILE(F, "			ADDQ(\"%s\");\n", quote);
					if (i != OBJ_NSELECTED - 1) PRINT_TO_FILE(F, "			ADDQ(\", \");\n");
					FREE(ctype);
				}

				PRINT_TO_FILE(F, "			ADDQ(\")\");\n");
				PRINT_TO_FILE(F, "			EXEC;\n");
				PRINT_TO_FILE(F, "		}\n");
				PRINT_TO_FILE(F, "	}\n");
				/* UPDATE THE MV IF FCC_OLD IS TRUE AND FCC NEW IS TRUE */
				PRINT_TO_FILE(F, "	if ((%s) && (%s)) {\n", OBJ_FCC_OLD(j), OBJ_FCC_NEW(j));

				PRINT_TO_FILE(F, "		QUERY(\"update %s set \");\n", mvName);
				check = FALSE;
				for (i = 0; i < OBJ_NSELECTED; i++) {
					if (OBJ_SELECTED_COL(i)->context == COLUMN_CONTEXT_FREE
						&& OBJ_SELECTED_COL(i)->table == OBJ_TABLE(j)) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
						else { quote = ""; strprefix = "str_"; }

						if (check) PRINT_TO_FILE(F, "		_QUERY(\", \");\n");
						check = TRUE;

						PRINT_TO_FILE(F, "		_QUERY(\" %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						PRINT_TO_FILE(F, "		_QUERY(new_%s%s_table);\n", strprefix, OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "		_QUERY(\"%s \");\n", quote);
						FREE(ctype);
					}
				}
				PRINT_TO_FILE(F, "		_QUERY(\" where true \");\n");
				for (i = 0; i < OBJ_NSELECTED; i++) {
					if (OBJ_SELECTED_COL(i)->context == COLUMN_CONTEXT_FREE
						&& OBJ_SELECTED_COL(i)->isPrimaryColumn
						&& OBJ_SELECTED_COL(i)->table == OBJ_TABLE(j)) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
						else { quote = ""; strprefix = "str_"; }

						PRINT_TO_FILE(F, "		_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						PRINT_TO_FILE(F, "		_QUERY(old_%s%s_table);\n", strprefix, OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "		_QUERY(\"%s \");\n", quote);
						FREE(ctype);
					}
				}
				PRINT_TO_FILE(F, "		EXECUTE_QUERY;\n");
				PRINT_TO_FILE(F, "	}\n");
				PRINT_TO_FILE(F, "	END;\n");
				PRINT_TO_FILE(F, "}\n\n");
			}
			//T: Cập nhật như trường hợp chung
			else {
				//puts("The last case");
				Boolean check;
				// Define trigger function
				char *triggerName;
				STR_INIT(triggerName, "pmvtg_");
				STR_APPEND(triggerName, mvName);
				STR_APPEND(triggerName, "_update_on_");
				STR_APPEND(triggerName, OBJ_TABLE(j)->name);
				PRINT_TO_FILE(F, "TRIGGER(%s) {\n", triggerName);
				FREE(triggerName);
				PRINT_TO_FILE(F, "//All the rest\n\n");
				// Variable declaration
				PRINT_TO_FILE(F, "	/*\n		Variables declaration\n	*/\n");
				// MV's vars
				PRINT_TO_FILE(F, "	// MV's vars\n");
				for (i = 0; i < OBJ_NSELECTED; i++) {
					char *varname = OBJ_SELECTED_COL(i)->varName;
					char *tableName = "<expression>";
					if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
					PRINT_TO_FILE(F, "	char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
				}

				// Table's vars
				PRINT_TO_FILE(F, "	// %s table's vars\n", OBJ_TABLE(j)->name);
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *varname = OBJ_TABLE(j)->cols[i]->varName;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					PRINT_TO_FILE(F, "	%s %s;\n", ctype, varname);
					if (STR_INEQUAL(ctype, "char *")) {
						if (STR_EQUAL(ctype, "Numeric"))
							PRINT_TO_FILE(F, "	char * str_%s;\n", varname);
						else
							PRINT_TO_FILE(F, "	char str_%s[20];\n", varname);
					}
					FREE(ctype);
				}

				PRINT_TO_FILE(F, "\n	char * count;\n");

				// Standard trigger preparing procedures
				PRINT_TO_FILE(F, "\n	/*\n		Standard trigger preparing procedures\n	*/\n	REQUIRED_PROCEDURES;\n\n");

				// DELETE OLD
				PRINT_TO_FILE(F, "	if (UTRIGGER_FIRED_BEFORE) {\n		/* DELETE OLD*/\n");

				// Get values of table's fields
				PRINT_TO_FILE(F, "		// Get values of table's fields\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *func;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					char *convert = "";
					if (STR_EQUAL(ctype, "char *"))
						func = "GET_STR_ON_TRIGGERED_ROW";
					else if (STR_EQUAL(ctype, "int")) {
						func = "GET_INT32_ON_TRIGGERED_ROW";
						convert = "INT32_TO_STR";
					}
					else if (STR_EQUAL(ctype, "int64")) {
						func = "GET_INT64_ON_TRIGGERED_ROW";
						convert = "INT64_TO_STR";
					}
					else {
						func = "GET_NUMERIC_ON_TRIGGERED_ROW";
						convert = "NUMERIC_TO_STR";
					}
					PRINT_TO_FILE(F, "		%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						PRINT_TO_FILE(F, "		%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
					FREE(ctype)
				}

				// FCC
				PRINT_TO_FILE(F, "\n		// FCC\n");
				PRINT_TO_FILE(F, "		if (%s) {\n", OBJ_FCC(j));
				//puts(OBJ_TABLE(j)->name);
				//puts(OBJ_FCC(j));
				/*
				TRIGGER BODY START HERE
				*/
				// Re-query
				PRINT_TO_FILE(F, "\n			// Re-query\n");
				PRINT_TO_FILE(F, "			DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
				PRINT_TO_FILE(F, "			ADDQ(\" from \");\n");
				PRINT_TO_FILE(F, "			ADDQ(\"%s\");\n", OBJ_SQ->from);
				PRINT_TO_FILE(F, "			ADDQ(\" where true \");\n");
				if (OBJ_SQ->hasWhere) PRINT_TO_FILE(F, "			ADDQ(\" and %s\");\n", OBJ_SQ->where);
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++)
					if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
						else { quote = ""; strprefix = "str_"; }
						PRINT_TO_FILE(F, "			ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
						PRINT_TO_FILE(F, "			ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
						PRINT_TO_FILE(F, "			ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}
				if (OBJ_SQ->hasGroupby) PRINT_TO_FILE(F, "			ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
				if (OBJ_SQ->hasHaving) PRINT_TO_FILE(F, "			ADDQ(\" having %s\");\n", OBJ_SQ->having);

				PRINT_TO_FILE(F, "			EXEC;\n");

				// Allocate SAVED_RESULT set
				PRINT_TO_FILE(F, "\n			// Allocate SAVED_RESULT set\n			DEFR(%d);\n", OBJ_NSELECTED);

				// Save dQ result
				PRINT_TO_FILE(F, "\n			// Save dQ result\n			FOR_EACH_RESULT_ROW(i) {\n");

				for (i = 0; i < OBJ_NSELECTED; i++) {
					PRINT_TO_FILE(F, "				GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
					PRINT_TO_FILE(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
				}

				PRINT_TO_FILE(F, "			}\n");

				// For each dQi:
				PRINT_TO_FILE(F, "\n			// For each dQi:\n			FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

				k = 0;
				for (i = 0; i < OBJ_NSELECTED; i++)
					PRINT_TO_FILE(F, "				%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

				// Check if there is a similar row in MV
				PRINT_TO_FILE(F, "\n				// Check if there is a similar row in MV\n");
				PRINT_TO_FILE(F, "				DEFQ(\"select mvcount from %s where true \");\n", mvName);
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						PRINT_TO_FILE(F, "				ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "				ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}

				PRINT_TO_FILE(F, "				EXEC;\n");

				PRINT_TO_FILE(F, "\n				GET_STR_ON_RESULT(count, 0, 1);\n");

				PRINT_TO_FILE(F, "\n				if (STR_EQUAL(count, mvcount)) {\n");
				PRINT_TO_FILE(F, "					// Delete the row\n");
				PRINT_TO_FILE(F, "					DEFQ(\"delete from %s where true\");\n", mvName);

				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						PRINT_TO_FILE(F, "					ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "					ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}

				PRINT_TO_FILE(F, "					EXEC;\n");

				PRINT_TO_FILE(F, "				} else {\n");

				PRINT_TO_FILE(F, "					// ow, decrease the count column \n");
				PRINT_TO_FILE(F, "					DEFQ(\"update %s set \");\n", mvName);

				check = FALSE;
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {

						if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_SUM)) {
							if (check) PRINT_TO_FILE(F, "					ADDQ(\", \");\n");
							check = TRUE;

							PRINT_TO_FILE(F, "					ADDQ(\" %s = %s - \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
						else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_COUNT)) {
							if (check) PRINT_TO_FILE(F, "					ADDQ(\", \");\n");
							check = TRUE;

							PRINT_TO_FILE(F, "					ADDQ(\" %s = %s - \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
					}
				PRINT_TO_FILE(F, "					ADDQ(\" where true \");\n");
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						PRINT_TO_FILE(F, "					ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "					ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}
				PRINT_TO_FILE(F, "					EXEC;\n");

				PRINT_TO_FILE(F, "				}\n");
				PRINT_TO_FILE(F, "			}\n");

				// End of fcc
				PRINT_TO_FILE(F, "		}\n");

				// Check time else: after: do insert new row
				PRINT_TO_FILE(F, "\n	} else {\n");

				if (OBJ_STATIC_BOOL_AGG_MINMAX) {
					// DELETE PART AFTER OF UPDATE
					PRINT_TO_FILE(F, "\n		/* DELETE */\n		// Get values of table's fields\n");
					for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						char *func;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						char *convert = "";
						if (STR_EQUAL(ctype, "char *"))
							func = "GET_STR_ON_TRIGGERED_ROW";
						else if (STR_EQUAL(ctype, "int")) {
							func = "GET_INT32_ON_TRIGGERED_ROW";
							convert = "INT32_TO_STR";
						}
						else if (STR_EQUAL(ctype, "int64")) {
							func = "GET_INT64_ON_TRIGGERED_ROW";
							convert = "INT64_TO_STR";
						}
						else {
							func = "GET_NUMERIC_ON_TRIGGERED_ROW";
							convert = "NUMERIC_TO_STR";
						}
						PRINT_TO_FILE(F, "		%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
						if (STR_INEQUAL(ctype, "char *"))
							PRINT_TO_FILE(F, "		%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
						FREE(ctype);
					}

					// FCC
					PRINT_TO_FILE(F, "\n		// FCC\n");
					PRINT_TO_FILE(F, "		if (%s) {\n", OBJ_FCC(j));

					// Re-query
					PRINT_TO_FILE(F, "\n			// Re-query\n");
					PRINT_TO_FILE(F, "			DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
					PRINT_TO_FILE(F, "			ADDQ(\" from \");\n");
					PRINT_TO_FILE(F, "			ADDQ(\"%s\");\n", OBJ_SQ->from);
					PRINT_TO_FILE(F, "			ADDQ(\" where true \");\n");
					if (OBJ_SQ->hasWhere) PRINT_TO_FILE(F, "			ADDQ(\" and %s\");\n", OBJ_SQ->where);
					for (i = 0; i < OBJ_TG_NCOLS(j); i++) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_TG_COL(j, i)->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
						else { quote = ""; strprefix = "str_"; }
						PRINT_TO_FILE(F, "			ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TG_COL(j, i)->name, quote);
						PRINT_TO_FILE(F, "			ADDQ(%s%s);\n", strprefix, OBJ_TG_COL(j, i)->varName);
						PRINT_TO_FILE(F, "			ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}
					if (OBJ_SQ->hasGroupby) PRINT_TO_FILE(F, "			ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
					if (OBJ_SQ->hasHaving) PRINT_TO_FILE(F, "				ADDQ(\" having %s\");\n", OBJ_SQ->having);

					PRINT_TO_FILE(F, "			EXEC;\n");

					PRINT_TO_FILE(F, "\n			DEFR(%d);\n", OBJ_NSELECTED);

					PRINT_TO_FILE(F, "\n			FOR_EACH_RESULT_ROW(i) {\n");

					for (i = 0; i < OBJ_NSELECTED; i++) {
						PRINT_TO_FILE(F, "				GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
						PRINT_TO_FILE(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
					}

					PRINT_TO_FILE(F, "			}\n");

					// For each dQi:
					PRINT_TO_FILE(F, "\n			// For each dQi:\n			FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

					k = 0;
					for (i = 0; i < OBJ_NSELECTED; i++)
						PRINT_TO_FILE(F, "				%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

					PRINT_TO_FILE(F, "				DEFQ(\"update %s set mvcount = mvcount \");\n", mvName);

					for (i = 0; i < OBJ_NSELECTED; i++) {
						if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_MIN) || STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_MAX_)) {
							PRINT_TO_FILE(F, "				ADDQ(\", %s = \");\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
					}

					PRINT_TO_FILE(F, "				ADDQ(\" where true \");\n");
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							PRINT_TO_FILE(F, "				ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "				ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}

					PRINT_TO_FILE(F, "				EXEC;\n");
					PRINT_TO_FILE(F, "			}\n");
					PRINT_TO_FILE(F, "\n		}\n");
				}

				// INSERT PART AFTER OF UPDATE
				// Get values of table's fields
				PRINT_TO_FILE(F, "\n		/* INSERT */\n		// Get values of table's fields\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *func;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					char *convert = "";
					if (STR_EQUAL(ctype, "char *"))
						func = "GET_STR_ON_NEW_ROW";
					else if (STR_EQUAL(ctype, "int")) {
						func = "GET_INT32_ON_NEW_ROW";
						convert = "INT32_TO_STR";
					}
					else if (STR_EQUAL(ctype, "int64")) {
						func = "GET_INT64_ON_NEW_ROW";
						convert = "INT64_TO_STR";
					}
					else {
						func = "GET_NUMERIC_ON_NEW_ROW";
						convert = "NUMERIC_TO_STR";
					}
					PRINT_TO_FILE(F, "		%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						PRINT_TO_FILE(F, "		%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
					FREE(ctype);
				}

				// FCC
				PRINT_TO_FILE(F, "\n		// FCC\n");
				PRINT_TO_FILE(F, "		if (%s) {\n", OBJ_FCC(j));

				/*
				TRIGGER BODY START HERE
				*/
				// Re-query
				PRINT_TO_FILE(F, "\n			// Re-query\n");
				PRINT_TO_FILE(F, "			DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
				PRINT_TO_FILE(F, "			ADDQ(\" from \");\n");
				PRINT_TO_FILE(F, "			ADDQ(\"%s\");\n", OBJ_SQ->from);
				PRINT_TO_FILE(F, "			ADDQ(\" where true \");\n");
				if (OBJ_SQ->hasWhere) PRINT_TO_FILE(F, "			ADDQ(\" and %s\");\n", OBJ_SQ->where);
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++)
					if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
						else { quote = ""; strprefix = "str_"; }
						PRINT_TO_FILE(F, "			ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
						PRINT_TO_FILE(F, "			ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
						PRINT_TO_FILE(F, "			ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}
				if (OBJ_SQ->hasGroupby) PRINT_TO_FILE(F, "			ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
				if (OBJ_SQ->hasHaving) PRINT_TO_FILE(F, "			ADDQ(\" having %s\");\n", OBJ_SQ->having);

				PRINT_TO_FILE(F, "			EXEC;\n");

				// Allocate SAVED_RESULT set
				PRINT_TO_FILE(F, "\n			// Allocate SAVED_RESULT set\n			DEFR(%d);\n", OBJ_NSELECTED);

				// Save dQ result
				PRINT_TO_FILE(F, "\n			// Save dQ result\n			FOR_EACH_RESULT_ROW(i) {\n");

				for (i = 0; i < OBJ_NSELECTED; i++) {
					PRINT_TO_FILE(F, "				GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
					PRINT_TO_FILE(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
				}

				PRINT_TO_FILE(F, "			}\n");

				// For each dQi:
				PRINT_TO_FILE(F, "\n			// For each dQi:\n			FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

				k = 0;
				for (i = 0; i < OBJ_NSELECTED; i++)
					PRINT_TO_FILE(F, "				%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

				// Check if there is a similar row in MV
				PRINT_TO_FILE(F, "\n				// Check if there is a similar row in MV\n");
				PRINT_TO_FILE(F, "				DEFQ(\"select mvcount from %s where true \");\n", mvName);
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						PRINT_TO_FILE(F, "				ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "				ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}

				PRINT_TO_FILE(F, "				EXEC;\n");

				PRINT_TO_FILE(F, "\n				if (NO_ROW) {\n");
				PRINT_TO_FILE(F, "					// insert new row to mv\n");
				PRINT_TO_FILE(F, "					DEFQ(\"insert into %s values (\");\n", mvName);

				for (i = 0; i < OBJ_NSELECTED; i++) {
					char *quote;
					char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
					if (STR_EQUAL(ctype, "char *"))
						quote = "'";
					else quote = "";
					PRINT_TO_FILE(F, "					ADDQ(\"%s\");\n", quote);
					PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
					PRINT_TO_FILE(F, "					ADDQ(\"%s\");\n", quote);
					if (i != OBJ_NSELECTED - 1) PRINT_TO_FILE(F, "					ADDQ(\", \");\n");
					FREE(ctype);
				}

				PRINT_TO_FILE(F, "					ADDQ(\")\");\n");
				PRINT_TO_FILE(F, "					EXEC;\n");

				PRINT_TO_FILE(F, "				} else {\n");

				// Update agg func fields
				PRINT_TO_FILE(F, "					// update old row\n");
				PRINT_TO_FILE(F, "					DEFQ(\"update %s set \");\n", mvName);

				check = FALSE;
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_SUM)) {
							if (check) PRINT_TO_FILE(F, "					ADDQ(\", \");\n");
							check = TRUE;

							PRINT_TO_FILE(F, "					ADDQ(\" %s = %s + \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
						else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_COUNT)) {
							if (check) PRINT_TO_FILE(F, "					ADDQ(\", \");\n");
							check = TRUE;

							PRINT_TO_FILE(F, "					ADDQ(\" %s = %s + \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
						else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_MIN)) {
							if (check) PRINT_TO_FILE(F, "					ADDQ(\", \");\n");
							check = TRUE;

							PRINT_TO_FILE(F, "					ADDQ(\" %s = (case when %s > \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					ADDQ(\" then \");\n");
							PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					ADDQ(\" else %s end) \");\n", OBJ_SELECTED_COL(i)->varName);
						}
						else {
							// max
							if (check) PRINT_TO_FILE(F, "					ADDQ(\", \");\n");
							check = TRUE;

							PRINT_TO_FILE(F, "					ADDQ(\" %s = (case when %s < \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					ADDQ(\" then \");\n");
							PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					ADDQ(\" else %s end) \");\n", OBJ_SELECTED_COL(i)->varName);
						}
					}
				PRINT_TO_FILE(F, "					ADDQ(\" where true \");\n");
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						PRINT_TO_FILE(F, "					ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "					ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}
				PRINT_TO_FILE(F, "					EXEC;\n");

				PRINT_TO_FILE(F, "				}\n");
				PRINT_TO_FILE(F, "			}\n");
				PRINT_TO_FILE(F, "		}\n");

				// End of check time 
				PRINT_TO_FILE(F, "\n	}");

				// End of end trigger
				PRINT_TO_FILE(F, "\n\n	/*\n		Finish\n	*/\n	END;\n}\n\n");
			}// --- END OF CHECK POINT FOR UPDATE EVENT (A2 mechanism Vs. Normal mechanism)
			 // --- END OF UPDATE EVENT

		} // --- END OF 'FOREACH TABLE'

		  //fclose(cWriter);
	} // --- END OF GENERATING C TRIGGER CODE
}
else
if (OBJ_SQ->hasOuterJoin) {
	 
	/*--------------------------------------------- REGION: OUTER JOIN --------------------------------------------
													Author: Do Minh Tuan
	---------------------------------------------------------------------------------------------------------------
	*/

	/*	Check for column joined in WHERE clause or not
		Uses to determine whether the 
	*/
	{
		if (OBJ_SQ->hasWhere) {
			//loop through each table in FROM
			char *whereClause = copy(OBJ_SQ->where);
			
			for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
				OBJ_TABLE(j)->hasColumnInWhereClause = FALSE;
				char *dotPos = "";
				char *tmp = whereClause;
				do {
					dotPos = findSubString(tmp, ".");
					if (dotPos) {
						char *tableName = getPrecededTableName(whereClause, dotPos + 1);
						if (STR_EQUAL_CI(tableName, OBJ_TABLE(j)->name)) {
							OBJ_TABLE(j)->hasColumnInWhereClause = TRUE;
							printf("'%s' = true \n", OBJ_TABLE(j)->name);
						}
						tmp = dotPos + 1;
					}
				} while (dotPos);				
			}
		}
	}


	{ // GENERATE MV'S SQL CODE
	  // 'SQL' macro is an alias of 'sqlWriter' variable
		FILE *sqlWriter = fopen(sqlFile, "w");
		PRINT_TO_FILE(SQL, "create table %s (\n", mvName);

		//Loop to OBJ_NSELECTED - 1, because mvcount column is not neccessary 
		for (i = 0; i < OBJ_NSELECTED - 1; i++) {
			char *characterLength;
			char *ctype;
			STR_INIT(characterLength, "");
			ctype = createCType(OBJ_SELECTED_COL(i)->type);
			if (STR_EQUAL(ctype, "char *") && STR_INEQUAL(OBJ_SELECTED_COL(i)->characterLength, "")) {
				FREE(characterLength);
				STR_INIT(characterLength, "(");
				STR_APPEND(characterLength, OBJ_SELECTED_COL(i)->characterLength);
				STR_APPEND(characterLength, ")");
			}

			if (OBJ_SELECTED_COL(i)->context == COLUMN_CONTEXT_FREE)
				PRINT_TO_FILE(SQL, "	%s %s%s", OBJ_SELECTED_COL(i)->name, OBJ_SELECTED_COL(i)->type, characterLength);
			else
				PRINT_TO_FILE(SQL, "	%s %s%s", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->type, characterLength);

			if (i != OBJ_NSELECTED - 2)
				PRINT_TO_FILE(SQL, ",\n");
			FREE(characterLength);
			FREE(ctype);
		}
		PRINT_TO_FILE(SQL, "\n);");

		for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
			// insert
			PRINT_TO_FILE(SQL, "\n\n/* insert on %s*/", OBJ_TABLE(j)->name);
			PRINT_TO_FILE(SQL, "\ncreate function pmvtg_%s_insert_on_%s() returns trigger as '%s', 'pmvtg_%s_insert_on_%s' language c strict;",
					mvName, OBJ_TABLE(j)->name, dllFile, mvName, OBJ_TABLE(j)->name);

			PRINT_TO_FILE(SQL, "\ncreate trigger pgtg_%s_insert_on_%s after insert on %s for each row execute procedure pmvtg_%s_insert_on_%s();",
					mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);


			// delete
			PRINT_TO_FILE(SQL, "\n\n/* delete on %s*/", OBJ_TABLE(j)->name);
			PRINT_TO_FILE(SQL, "\ncreate function pmvtg_%s_delete_on_%s() returns trigger as '%s', 'pmvtg_%s_delete_on_%s' language c strict;",
				mvName, OBJ_TABLE(j)->name, dllFile, mvName, OBJ_TABLE(j)->name);

			PRINT_TO_FILE(SQL, "\ncreate trigger pgtg_%s_before_delete_on_%s before delete on %s for each row execute procedure pmvtg_%s_delete_on_%s();",
				mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);

			if (!OBJ_KEY_IN_G_CHECK(j) && OBJ_STATIC_BOOL_AGG_MINMAX)
				PRINT_TO_FILE(SQL, "\ncreate trigger pgtg_%s_after_delete_on_%s after delete on %s for each row execute procedure pmvtg_%s_delete_on_%s();",
					mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);

			// update
			PRINT_TO_FILE(SQL, "\n\n/* update on %s*/", OBJ_TABLE(j)->name);
			PRINT_TO_FILE(SQL, "\ncreate function pmvtg_%s_update_on_%s() returns trigger as '%s', 'pmvtg_%s_update_on_%s' language c strict;",
				mvName, OBJ_TABLE(j)->name, dllFile, mvName, OBJ_TABLE(j)->name);

			PRINT_TO_FILE(SQL, "\ncreate trigger pgtg_%s_before_update_on_%s before update on %s for each row execute procedure pmvtg_%s_update_on_%s();",
					mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);

			PRINT_TO_FILE(SQL, "\ncreate trigger pgtg_%s_after_update_on_%s after update on %s for each row execute procedure pmvtg_%s_update_on_%s();",
				mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);
		}
		fclose(sqlWriter);
	} // --- END OF GENERATING MV'S SQL CODE

	{ // GENERATE C TRIGGER CODE
	  // Macro 'F' is an alias of cWriter variable
		FILE *cWriter = fopen(triggerFile, "w");
		PRINT_TO_FILE(F, "#include \"ctrigger.h\"\n\n");
		PRINT_TO_FILE(F, "/*Please use Ctrl (hold) + K + D and release to format the source code if you are using Visual Studio 2010 and later */\n\n");
		//DETERMINE TYPE OF TABLE (main or complement), INIT VAR
		//Note: Can't do this inside the loop for generate code, must determine first

		printf("%s OUTER JOIN detected! \n", OBJ_SQ->outerJoinType);

		for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
			OBJ_TABLE(j)->isAPartOfOuterJoin = FALSE;
			OBJ_TABLE(j)->isMainTableOfOuterJoin = FALSE;
			OBJ_TABLE(j)->isComplementTableOfOuterJoin = FALSE;
			OBJ_TABLE(j)->colsInSelectNum = 0;
			for (int k = 0; k < OBJ_SQ->outerJoinMainTablesNum; k++) {
				if (STR_EQUAL(OBJ_SQ->outerJoinMainTables[k], OBJ_TABLE(j)->name)) {
					OBJ_TABLE(j)->isMainTableOfOuterJoin = TRUE;
					OBJ_TABLE(j)->isAPartOfOuterJoin = TRUE;
					printf("Main table: %s\n", OBJ_TABLE(j)->name);
				}
			}

			for (int k = 0; k < OBJ_SQ->outerJoinComplementTablesNum; k++) {
				if (STR_EQUAL(OBJ_SQ->outerJoinComplementTables[k], OBJ_TABLE(j)->name)) {
					OBJ_TABLE(j)->isAPartOfOuterJoin = TRUE;
					OBJ_TABLE(j)->isComplementTableOfOuterJoin = TRUE;
					printf("Complement table: %s\n", OBJ_TABLE(j)->name);
				}
			}

			//Continue if not a part of outer join
			if (!OBJ_TABLE(j)->isAPartOfOuterJoin) {
				printf("Not join in outer join: %s \n", OBJ_TABLE(j)->name);
				continue;
			}

			//Count number of column of table j join in Select clause
			for (int k = 0; k < OBJ_NSELECTED - 1; k++) {
				if (STR_EQUAL_CI(OBJ_SELECTED_COL(k)->table->name, OBJ_TABLE(j)->name)) {
					OBJ_TABLE(j)->colsInSelectNum++;
				}
			}
		}

		//GEN CODE FOR EACH TABLE
		for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
			
			/*******************************************************************************************************
															INSERT EVENT
			********************************************************************************************************/
			Boolean check;
			// Define trigger function
			char *triggerName;
			STR_INIT(triggerName, "pmvtg_");
			STR_APPEND(triggerName, mvName);
			STR_APPEND(triggerName, "_insert_on_");
			STR_APPEND(triggerName, OBJ_TABLE(j)->name);
			PRINT_TO_FILE(F, "TRIGGER(%s) {\n", triggerName);
			FREE(triggerName);

			// Variable declaration
			PRINT_TO_FILE(F, "	// MV's data\n");
			for (i = 0; i < OBJ_NSELECTED; i++) {
				char *varname = OBJ_SELECTED_COL(i)->varName;
				char *tableName = "<expression>";
				if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
				PRINT_TO_FILE(F, "	char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
			}

			// Table's vars
			PRINT_TO_FILE(F, "	// Table's data\n");
			for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
				char *varname = OBJ_TABLE(j)->cols[i]->varName;
				char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
				PRINT_TO_FILE(F, "	%s %s;\n", ctype, varname);
				if (STR_INEQUAL(ctype, "char *")) {
					if (STR_EQUAL(ctype, "Numeric"))
						PRINT_TO_FILE(F, "	char * str_%s;\n", varname);
					else
						PRINT_TO_FILE(F, "	char str_%s[20];\n", varname);
				}
				FREE(ctype);
			}

			// Standard trigger preparing procedures
			PRINT_TO_FILE(F, "\n	BEGIN;\n\n");

			// Get values of table's fields
			PRINT_TO_FILE(F, "	// Get table's data\n");
			for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
				char *func;
				char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
				char *convert = "";
				if (STR_EQUAL(ctype, "char *"))
					func = "GET_STR_ON_TRIGGERED_ROW";
				else if (STR_EQUAL(ctype, "int")) {
					func = "GET_INT32_ON_TRIGGERED_ROW";
					convert = "INT32_TO_STR";
				}
				else if (STR_EQUAL(ctype, "int64")) {
					func = "GET_INT64_ON_TRIGGERED_ROW";
					convert = "INT64_TO_STR";
				}
				else {
					func = "GET_NUMERIC_ON_TRIGGERED_ROW";
					convert = "NUMERIC_TO_STR";
				}
				PRINT_TO_FILE(F, "	%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
				if (STR_INEQUAL(ctype, "char *"))
					PRINT_TO_FILE(F, "	%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
				FREE(ctype);
			}

			// FCC
			PRINT_TO_FILE(F, "\n	// FCC\n");

			PRINT_TO_FILE(F, " if (%s) {\n", OBJ_FCC(j));
						
			/* 1- GENERATE CODE FOR MAIN TABLE*/
			if (OBJ_TABLE(j)->isMainTableOfOuterJoin) {
				GenReQueryOuterJoinWithPrimaryKey(j, F, OBJ_SELECT_CLAUSE);

				if (OBJ_SQ->hasGroupby) PRINT_TO_FILE(F, "		ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
				if (OBJ_SQ->hasHaving) PRINT_TO_FILE(F, "		ADDQ(\" having %s\");\n", OBJ_SQ->having);

				PRINT_TO_FILE(F, "		EXEC;\n");
				// Allocate cache
				PRINT_TO_FILE(F, "\n		// Allocate cache\n		DEFR(%d);\n", OBJ_NSELECTED - 1); //-1 because mvcount is not neccessary
				// Save dQ result
				PRINT_TO_FILE(F, "\n		// Save dQ result\n		FOR_EACH_RESULT_ROW(i) {\n");

				for (i = 0; i < OBJ_NSELECTED - 1; i++) {
					//if the column belongs to the main table then get its value
					if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
						PRINT_TO_FILE(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
						PRINT_TO_FILE(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
					}
					//if the column belongs to the complement table then check for null
					else {
						PRINT_TO_FILE(F, "			if(NULL_CELL(i, %d)){\n", i + 1);
						PRINT_TO_FILE(F, "				%s = \"null\";\n", OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "			}\n");
						PRINT_TO_FILE(F, "			else {\n");
						PRINT_TO_FILE(F, "				GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
						PRINT_TO_FILE(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "			}\n");
					}
				}

				PRINT_TO_FILE(F, "		}\n");
				// For each dQi:
				PRINT_TO_FILE(F, "\n		// For each dQi:\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED - 1);

				k = 0;
				for (i = 0; i < OBJ_NSELECTED - 1; i++)
					PRINT_TO_FILE(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);
				/* 1.1 - IF FULL OUTER JOIN DETECTED */
				if (STR_EQUAL_CI(OBJ_SQ->outerJoinType, "FULL")) {
					//Check if there is another row joined with other table
					PRINT_TO_FILE(F, "			//If not joined yet \n");
					PRINT_TO_FILE(F, "			if(1 ");
					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						if (STR_INEQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
							PRINT_TO_FILE(F, "&& STR_EQUAL_CI(%s, \"null\") ", OBJ_SELECTED_COL(i)->varName);
						}						
					}
					PRINT_TO_FILE(F, "){\n");
					PRINT_TO_FILE(F, "				DEFQ(\"insert into %s values (\");\n", mvName);

					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *"))
							quote = "'";
						else quote = "";
						PRINT_TO_FILE(F, "				if(STR_EQUAL_CI(%s, \"null\")){\n", OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "					ADDQ(\"null\"); \n");
						PRINT_TO_FILE(F, "				} else {\n");
						PRINT_TO_FILE(F, "					ADDQ(\"%s\");\n", quote);
						PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "					ADDQ(\"%s\"); \n", quote);
						PRINT_TO_FILE(F, "				} \n");
						if (i != OBJ_NSELECTED - 2) PRINT_TO_FILE(F, "				ADDQ(\", \");\n");
						FREE(ctype);
					}
					PRINT_TO_FILE(F, "				ADDQ(\")\");\n");
					PRINT_TO_FILE(F, "				EXEC;\n");
					PRINT_TO_FILE(F, "			}//end of check if not joined yet \n");
					PRINT_TO_FILE(F, "			else {//if joined already \n");
					/*DANGEROUS REGION: JUST CORRECT WITH 2 TABLES ONLY*/
					Boolean isCurrentTableMain = OBJ_TABLE(j)->isMainTableOfOuterJoin;
					Boolean isCurrentTableComplement = OBJ_TABLE(j)->isComplementTableOfOuterJoin;

					for (int i = 0; i < OBJ_SQ->fromElementsNum; i++) {
						if (i != j) {
							OBJ_TABLE(j)->isMainTableOfOuterJoin = TRUE;
							OBJ_TABLE(j)->isComplementTableOfOuterJoin = FALSE;
						}
					}
					OBJ_TABLE(j)->isMainTableOfOuterJoin = FALSE;
					OBJ_TABLE(j)->isComplementTableOfOuterJoin = TRUE;
					GenCodeInsertForComplementTable(j, F, mvName, '		');
					
					OBJ_TABLE(j)->isMainTableOfOuterJoin = isCurrentTableMain;
					OBJ_TABLE(j)->isComplementTableOfOuterJoin = isCurrentTableComplement;
					/*END OF DANGEROUS REGION :v */
					PRINT_TO_FILE(F, "			}//end of check if joined already \n");
				} //end of if FULL outer join
				/* 1.2 - IF LEFT - RIGHT OUTER JOIN*/
				else {
					// Check if there is a similar row in MV
					PRINT_TO_FILE(F, "\n			// Check if there is a similar row in MV\n");
					PRINT_TO_FILE(F, "			DEFQ(\"select * from %s where true \");\n", mvName);
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype;
							ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
								PRINT_TO_FILE(F, "			ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
								PRINT_TO_FILE(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
								PRINT_TO_FILE(F, "			ADDQ(\"%s\"); \n", quote);
							}
							else {
								PRINT_TO_FILE(F, "			ADDQ(\" and %s \");\n", OBJ_SELECTED_COL(i)->name);
								PRINT_TO_FILE(F, "			if(STR_EQUAL_CI(%s, \"null\")){\n", OBJ_SELECTED_COL(i)->varName);
								PRINT_TO_FILE(F, "				ADDQ(\"isnull\"); \n");
								PRINT_TO_FILE(F, "			} else {\n");
								PRINT_TO_FILE(F, "				ADDQ(\"=\"); \n");
								PRINT_TO_FILE(F, "				ADDQ(\"%s\"); \n", quote);
								PRINT_TO_FILE(F, "				ADDQ(%s); \n", OBJ_SELECTED_COL(i)->varName);
								PRINT_TO_FILE(F, "				ADDQ(\"%s\"); \n", quote);
								PRINT_TO_FILE(F, "			}\n");
							}

							FREE(ctype);
						}

					PRINT_TO_FILE(F, "			EXEC;\n");

					PRINT_TO_FILE(F, "\n			if (NO_ROW) { \n");
					PRINT_TO_FILE(F, "				// insert new row to mv \n");
					PRINT_TO_FILE(F, "				DEFQ(\"insert into %s values (\");\n", mvName);

					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *"))
							quote = "'";
						else quote = "";
						PRINT_TO_FILE(F, "				if(STR_EQUAL_CI(%s, \"null\")){\n", OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "					ADDQ(\"null\"); \n");
						PRINT_TO_FILE(F, "				} else {\n");
						PRINT_TO_FILE(F, "					ADDQ(\"%s\");\n", quote);
						PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						PRINT_TO_FILE(F, "					ADDQ(\"%s\"); \n", quote);
						PRINT_TO_FILE(F, "				} \n");
						if (i != OBJ_NSELECTED - 2) PRINT_TO_FILE(F, "				ADDQ(\", \");\n");
						FREE(ctype);
					}

					PRINT_TO_FILE(F, "				ADDQ(\")\");\n");
					PRINT_TO_FILE(F, "				EXEC;\n");
					PRINT_TO_FILE(F, "			}\n");
				} //end of if LEFT - RIGHT outer join
				PRINT_TO_FILE(F, "		}\n");
				// End of trigger
				PRINT_TO_FILE(F, "	}\n\n	/*\n		Finish\n	*/\n	END;\n}\n\n");
			}

			/* GENERATE CODE FOR COMPLEMENT TABLE */
			else if (OBJ_TABLE(j)->isComplementTableOfOuterJoin) {
				GenReQueryOuterJoinWithPrimaryKey(j, F, OBJ_SELECT_CLAUSE);
				for (int i = 0; i < OBJ_BIN_NCONDITIONS(j); i++) {
					char *condition = copy(OBJ_BIN_CONDITIONS(j, i));
					char* resultSet[MAX_NUMBER_OF_ELEMENTS];
					int *resultLen = 0;
					char *pos = strstr(condition, "=");
					char *operator = NULL;
					switch (*(pos - 1)) {
					case '>':
						operator = ">=";
						break;
					case '<':
						operator = "<=";
						break;
					case '!':
						operator = "!=";
						break;
					default:
						operator = "==";
						break;
					}
					dammf_split(condition, operator, resultSet, &resultLen, FALSE);
					Boolean findOuterTableName = FALSE;
					Boolean findOuterTableColumn = FALSE;
					for (int k = 0; k < resultLen; k++) {
						char *tmp = resultSet[k];
						char *dotPos = findSubString(tmp, ".");
						char *tbName = getPrecededTableName(tmp, dotPos + 1);
						if (STR_INEQUAL_CI(OBJ_TABLE(j)->name, tbName)) {				
							findOuterTableName = TRUE;
						}
					}
					if (findOuterTableName) {
						for (int k = 0; k < resultLen; k++) {
							char *tmp = resultSet[k];
							char *dotPos = findSubString(tmp, ".");
							char *tbName = getPrecededTableName(tmp, dotPos + 1);
							if (STR_EQUAL_CI(OBJ_TABLE(j)->name, tbName)) {
								char *colName = dotPos + 1;
								for (int l = 0; l < OBJ_TABLE(j)->nCols; l++) {
									if (STR_EQUAL_CI(OBJ_TABLE(j)->cols[l]->name, colName)) {
										findOuterTableColumn = TRUE;										
									}
								}
							}
						}
					}

					if (findOuterTableName && findOuterTableColumn) {
						for (int k = 0; k < resultLen; k++) {
							char *tmp = resultSet[k];
							char *dotPos = findSubString(tmp, ".");
							char *tbName = getPrecededTableName(tmp, dotPos + 1);
							if (STR_INEQUAL_CI(OBJ_TABLE(j)->name, tbName)) {
								PRINT_TO_FILE(F, "		ADDQ(\" AND %s = \");\n", tmp);
								findOuterTableName = TRUE;
							}
						}
						for (int k = 0; k < resultLen; k++) {
							char *tmp = resultSet[k];
							char *dotPos = findSubString(tmp, ".");
							char *tbName = getPrecededTableName(tmp, dotPos + 1);
							if (STR_EQUAL_CI(OBJ_TABLE(j)->name, tbName)) {
								char *colName = dotPos + 1;
								for (int l = 0; l < OBJ_TABLE(j)->nCols; l++) {
									if (STR_EQUAL_CI(OBJ_TABLE(j)->cols[l]->name, colName)) {
										findOuterTableColumn = TRUE;
										char *quote;
										char *strprefix;
										char *ctype = createCType(OBJ_TABLE(j)->cols[l]->type);
										if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
										else { quote = ""; strprefix = "str_"; }
										PRINT_TO_FILE(F, "		ADDQ(\"%s\"); \n", quote);
										PRINT_TO_FILE(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[l]->varName);
										PRINT_TO_FILE(F, "		ADDQ(\"%s\"); \n", quote);
										FREE(ctype);
									}
								}
							}
						}
					}
				}


				if (OBJ_SQ->hasGroupby) PRINT_TO_FILE(F, "		ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
				if (OBJ_SQ->hasHaving) PRINT_TO_FILE(F, "		ADDQ(\" having %s\");\n", OBJ_SQ->having);
				
				PRINT_TO_FILE(F, "		EXEC;\n");

				// Allocate cache
				PRINT_TO_FILE(F, "\n		// Allocate cache\n		DEFR(%d);\n", OBJ_NSELECTED - 1);

				// Save dQ result
				PRINT_TO_FILE(F, "\n		// Save dQ result\n		FOR_EACH_RESULT_ROW(i) {\n");

				for (i = 0; i < OBJ_NSELECTED - 1; i++) {
					PRINT_TO_FILE(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
					PRINT_TO_FILE(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
				}

				PRINT_TO_FILE(F, "		}\n");
				// For each dQi:
				PRINT_TO_FILE(F, "\n		// For each dQi:\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED - 1);
				k = 0;
				for (i = 0; i < OBJ_NSELECTED - 1; i++)
					PRINT_TO_FILE(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);
				GenCodeInsertForComplementTable(j, F, mvName, ' ');
				PRINT_TO_FILE(F, "\n		}\n");
				// End of trigger
				PRINT_TO_FILE(F, "\n	}\n\n	/*\n		Finish\n	*/\n	END;\n}\n\n");

			}//END OF GENERATE CODE FOR COMPLEMENT TABLE

			 /*******************************************************************************************************
															DELETE EVENT
			 ********************************************************************************************************/
			if (TRUE) {
				Boolean check;
				// Define trigger function
				char *triggerName;
				STR_INIT(triggerName, "pmvtg_");
				STR_APPEND(triggerName, mvName);
				STR_APPEND(triggerName, "_delete_on_");
				STR_APPEND(triggerName, OBJ_TABLE(j)->name);
				PRINT_TO_FILE(F, "TRIGGER(%s) {\n", triggerName);
				FREE(triggerName);

				// Variable declaration
				PRINT_TO_FILE(F, "	// MV's data\n");
				for (i = 0; i < OBJ_NSELECTED; i++) {
					char *varname = OBJ_SELECTED_COL(i)->varName;
					char *tableName = "<expression>";
					if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
					PRINT_TO_FILE(F, "	char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
				}

				// Table's vars
				PRINT_TO_FILE(F, "	// Table's data\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *varname = OBJ_TABLE(j)->cols[i]->varName;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					PRINT_TO_FILE(F, "	%s %s;\n", ctype, varname);
					if (STR_INEQUAL(ctype, "char *")) {
						if (STR_EQUAL(ctype, "Numeric"))
							PRINT_TO_FILE(F, "	char * str_%s;\n", varname);
						else
							PRINT_TO_FILE(F, "	char str_%s[20];\n", varname);
					}
					FREE(ctype);
				}

				// Standard trigger preparing procedures
				PRINT_TO_FILE(F, "\n	BEGIN;\n\n");

				// Get values of table's fields
				PRINT_TO_FILE(F, "	// Get table's data\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *func;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					char *convert = "";
					if (STR_EQUAL(ctype, "char *"))
						func = "GET_STR_ON_TRIGGERED_ROW";
					else if (STR_EQUAL(ctype, "int")) {
						func = "GET_INT32_ON_TRIGGERED_ROW";
						convert = "INT32_TO_STR";
					}
					else if (STR_EQUAL(ctype, "int64")) {
						func = "GET_INT64_ON_TRIGGERED_ROW";
						convert = "INT64_TO_STR";
					}
					else {
						func = "GET_NUMERIC_ON_TRIGGERED_ROW";
						convert = "NUMERIC_TO_STR";
					}
					PRINT_TO_FILE(F, "	%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						PRINT_TO_FILE(F, "	%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
					FREE(ctype);
				}

				// FCC
				PRINT_TO_FILE(F, "\n	// FCC\n");
				/*GENERATE CODE FOR MAIN TABLE*/
				if (OBJ_TABLE(j)->isMainTableOfOuterJoin) {
					PRINT_TO_FILE(F, "	if(%s){\n",  OBJ_FCC(j));
					if (STR_EQUAL_CI(OBJ_SQ->outerJoinType, "FULL")) {

						GenReQueryOuterJoinWithPrimaryKey(j, F, OBJ_SELECT_CLAUSE);
						
						if (OBJ_SQ->hasGroupby) PRINT_TO_FILE(F, "		ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
						if (OBJ_SQ->hasHaving) PRINT_TO_FILE(F, "		ADDQ(\" having %s\");\n", OBJ_SQ->having);

						PRINT_TO_FILE(F, "		EXEC;\n");
						// Allocate cache
						PRINT_TO_FILE(F, "\n		// Allocate cache\n		DEFR(%d);\n", OBJ_NSELECTED - 1); //-1 because mvcount is not neccessary
						 // Save dQ result
						PRINT_TO_FILE(F, "\n		// Save dQ result\n		FOR_EACH_RESULT_ROW(i) {\n");

						for (i = 0; i < OBJ_NSELECTED - 1; i++) {
							//if the column belongs to the main table, get the value
							if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
								PRINT_TO_FILE(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
								PRINT_TO_FILE(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
							}
							//if the column belongs to the complement table, check NULL value 
							else {
								PRINT_TO_FILE(F, "			if(NULL_CELL(i, %d)){\n", i + 1);
								PRINT_TO_FILE(F, "				%s = \"null\";\n", OBJ_SELECTED_COL(i)->varName);
								PRINT_TO_FILE(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
								PRINT_TO_FILE(F, "			}\n");
								PRINT_TO_FILE(F, "			else {\n");
								PRINT_TO_FILE(F, "				GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
								PRINT_TO_FILE(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
								PRINT_TO_FILE(F, "			}\n");
							}
						}

						PRINT_TO_FILE(F, "		}\n");
						// For each dQi:
						PRINT_TO_FILE(F, "\n		// For each dQi:\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED - 1);

						k = 0;
						for (i = 0; i < OBJ_NSELECTED - 1; i++)
							PRINT_TO_FILE(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);
						/* 1.1 - NEU LA FULL OUTER JOIN*/						
							//kiem tra join
							PRINT_TO_FILE(F, "			//Neu chua join \n");
							PRINT_TO_FILE(F, "			if(1 ");
							for (i = 0; i < OBJ_NSELECTED - 1; i++) {
								if (STR_INEQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
									PRINT_TO_FILE(F, "&& STR_EQUAL_CI(%s, \"null\") ", OBJ_SELECTED_COL(i)->varName);
								}
							}
							PRINT_TO_FILE(F, "){\n");
							
							PRINT_TO_FILE(F, "				DEFQ(\"delete from %s where true \");\n", mvName);
							for (i = 0; i < OBJ_NSELECTED - 1; i++) {
								if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
									for (int k = 0; k < OBJ_TABLE(j)->nCols; k++) {
										if (STR_EQUAL_CI(OBJ_TABLE(j)->cols[k]->name, OBJ_SELECTED_COL(i)->name)) {
											char *quote;
											char *ctype = createCType(OBJ_TABLE(j)->cols[k]->type);
											if (STR_EQUAL(ctype, "char *")) { quote = "\'";  }
											else { quote = ""; }
											PRINT_TO_FILE(F, "				ADDQ(\" and %s = %s\"); \n", OBJ_SELECTED_COL(i)->name, quote);
											PRINT_TO_FILE(F, "				ADDQ(%s);\n",  OBJ_SELECTED_COL(i)->varName);
											PRINT_TO_FILE(F, "				ADDQ(\"%s \"); \n", quote);

											FREE(ctype);
										}
									}
								}
							}
							PRINT_TO_FILE(F, "				EXEC;\n");
							PRINT_TO_FILE(F, "			}//end of kiem tra chua join\n");
							PRINT_TO_FILE(F, "			else {//neu da join\n");
							
							/* REWRITE THE SELECT QUERY TO MV */
							PRINT_TO_FILE(F, "				DEFQ(\"select * from %s where true \");\n", mvName);
							for (i = 0; i < OBJ_NSELECTED - 1; i++) {
								if (STR_INEQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
									char *quote;
									char *ctype;
									ctype = createCType(OBJ_SELECTED_COL(i)->type);
									if (STR_EQUAL(ctype, "char *")) quote = "\'";
									else quote = "";
									PRINT_TO_FILE(F, "				ADDQ(\" and %s = %s\"); \n", OBJ_SELECTED_COL(i)->name, quote);
									PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
									PRINT_TO_FILE(F, "				ADDQ(\"%s \"); \n", quote);
									FREE(ctype);
								}
							}
							for (i = 0; i < OBJ_NSELECTED - 1; i++) {
								if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
									
									PRINT_TO_FILE(F, "				ADDQ(\" and %s is not null \"); \n", OBJ_SELECTED_COL(i)->name);
								}
							}
							PRINT_TO_FILE(F, "				EXEC;\n");
							PRINT_TO_FILE(F, "				if(ROW_NUM_IN_RESULT_SET == 1 && !%d){\n", OBJ_TABLE(j)->hasColumnInWhereClause);
							int colInSelect = 0;
							for (i = 0; i < OBJ_NSELECTED - 1; i++) {
								if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
									colInSelect++;
								}
							}

							PRINT_TO_FILE(F, "					DEFQ(\"update %s set \");\n", mvName);
							for (i = 0; i < OBJ_NSELECTED - 1; i++) {
								if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
									PRINT_TO_FILE(F, "					ADDQ(\"%s = null\");\n", OBJ_SELECTED_COL(i)->name);
									colInSelect--;
									if (colInSelect > 0) {
										PRINT_TO_FILE(F, "					ADDQ(\",\");\n");
									}
								}
							}
							PRINT_TO_FILE(F, "					ADDQ(\" where true \");\n");

							for (i = 0; i < OBJ_NSELECTED - 1; i++) {
								char *quote;
								char *ctype;
								ctype = createCType(OBJ_SELECTED_COL(i)->type);
								if (STR_EQUAL(ctype, "char *")) quote = "\'";
								else quote = "";
								PRINT_TO_FILE(F, "					ADDQ(\" and %s = %s\"); \n", OBJ_SELECTED_COL(i)->name, quote);
								PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
								PRINT_TO_FILE(F, "					ADDQ(\"%s \"); \n", quote);
								FREE(ctype);
							}
							PRINT_TO_FILE(F, "					EXEC;\n");
							PRINT_TO_FILE(F, "				} else {\n");
							PRINT_TO_FILE(F, "					DEFQ(\"delete from %s \");\n", mvName);
							PRINT_TO_FILE(F, "					ADDQ(\" where true \");\n");

							for (i = 0; i < OBJ_NSELECTED - 1; i++) {
								if (OBJ_SELECTED_COL(i)->table->isMainTableOfOuterJoin) {
									char *quote;
									char *ctype;
									ctype = createCType(OBJ_SELECTED_COL(i)->type);
									if (STR_EQUAL(ctype, "char *")) quote = "\'";
									else quote = "";
									PRINT_TO_FILE(F, "					ADDQ(\" and %s = %s\"); \n", OBJ_SELECTED_COL(i)->name, quote);
									PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
									PRINT_TO_FILE(F, "					ADDQ(\"%s \"); \n", quote);

									FREE(ctype);
								}
							}
							PRINT_TO_FILE(F, "					ADDQ(\" AND true \");\n");

							for (i = 0; i < OBJ_NSELECTED - 1; i++) {
								if (OBJ_SELECTED_COL(i)->table->isComplementTableOfOuterJoin) {
									for (int k = 0; k < OBJ_TABLE(j)->nCols; k++) {
										if (STR_EQUAL_CI(OBJ_TABLE(j)->cols[k]->name, OBJ_SELECTED_COL(i)->name)) {
											char *quote;
											char *strprefix;
											char *ctype = createCType(OBJ_TABLE(j)->cols[k]->type);
											if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
											else { quote = ""; strprefix = "str_"; }
											PRINT_TO_FILE(F, "					ADDQ(\" and %s = %s\"); \n", OBJ_TABLE(j)->cols[k]->name, quote);
											PRINT_TO_FILE(F, "					ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[k]->varName);
											PRINT_TO_FILE(F, "					ADDQ(\"%s \"); \n", quote);
											FREE(ctype);
										}
									}

								}
							}
							PRINT_TO_FILE(F, "					EXEC;\n");
							PRINT_TO_FILE(F, "				}\n"); 
							PRINT_TO_FILE(F, "			}\n");
							PRINT_TO_FILE(F, "		}//end of kiem tra da join\n");
					}
					//LEFT - RIGHT OUTER JOIN
					else {
						PRINT_TO_FILE(F, "		DEFQ(\"delete from %s where true \");\n", mvName);
						for (i = 0; i < OBJ_NSELECTED - 1; i++) {
							if (OBJ_SELECTED_COL(i)->table->isMainTableOfOuterJoin) {
								for (int k = 0; k < OBJ_TABLE(j)->nCols; k++) {
									if (STR_EQUAL_CI(OBJ_TABLE(j)->cols[k]->name, OBJ_SELECTED_COL(i)->name)) {
										char *quote;
										char *strprefix;
										char *ctype = createCType(OBJ_TABLE(j)->cols[k]->type);
										if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
										else { quote = ""; strprefix = "str_"; }
										PRINT_TO_FILE(F, "		ADDQ(\" and %s = %s\"); \n", OBJ_TABLE(j)->cols[k]->name, quote);
										PRINT_TO_FILE(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[k]->varName);
										PRINT_TO_FILE(F, "		ADDQ(\"%s \"); \n", quote);

										FREE(ctype);
									}
								}
							}
						}
						PRINT_TO_FILE(F, "		EXEC;\n");
					}

					PRINT_TO_FILE(F, "	}\n");
					PRINT_TO_FILE(F, "	END; \n");
					PRINT_TO_FILE(F, "}\n");
				}
				/*GENERATE CODE FOR COMPLEMENT TABLE*/
				else if (OBJ_TABLE(j)->isComplementTableOfOuterJoin) {
					PRINT_TO_FILE(F, "	if(%s){\n", OBJ_FCC(j));
					PRINT_TO_FILE(F, "	//Re-query \n");
					char *outerJoinSelect = copy(OBJ_SELECT_CLAUSE);
					outerJoinSelect = replaceFirst(outerJoinSelect, ", count(*)", "");
					PRINT_TO_FILE(F, "		DEFQ(\"%s \");\n", outerJoinSelect);
					PRINT_TO_FILE(F, "		ADDQ(\" from \");\n");
					PRINT_TO_FILE(F, "		ADDQ(\"%s\");\n", OBJ_SQ->from);
					PRINT_TO_FILE(F, "		ADDQ(\" where true \");\n");
					if (OBJ_SQ->hasWhere) PRINT_TO_FILE(F, "		ADDQ(\" and (%s)\");\n", OBJ_SQ->where);
					for (int i = 0; i < OBJ_BIN_NCONDITIONS(j); i++) {
						
						char *condition = copy(OBJ_BIN_CONDITIONS(j, i));
						char* resultSet[MAX_NUMBER_OF_ELEMENTS];
						int *resultLen = 0;
						char *pos = strstr(condition, "=");
						char *operator = NULL;
						switch (*(pos - 1)) {
						case '>':
							operator = ">=";
							break;
						case '<':
							operator = "<=";
							break;
						case '!':
							operator = "!=";
							break;
						default:
							operator = "==";
							break;
						}
						dammf_split(condition, operator, resultSet, &resultLen, FALSE);
						Boolean findOuterTableName = FALSE;
						Boolean findOuterTableColumn = FALSE;
						char *outerTableName = "";
						char *outerTableColumn = "";
						for (int k = 0; k < resultLen; k++) {
							char *tmp = resultSet[k];
							char *dotPos = findSubString(tmp, ".");
							outerTableName = getPrecededTableName(tmp, dotPos + 1);
							if (STR_INEQUAL_CI(OBJ_TABLE(j)->name, outerTableName)) {
								findOuterTableName = TRUE;
							}
						}
						if (findOuterTableName) {
							for (int k = 0; k < resultLen; k++) {
								char *tmp = resultSet[k];
								char *dotPos = findSubString(tmp, ".");
								char *tbName_tmp = getPrecededTableName(tmp, dotPos + 1);
								if (tbName_tmp, outerTableName) {
									char *colName = dotPos + 1;
									colName = trim(colName);
									for (int l = 0; l < OBJ_TABLE(j)->nCols; l++) {
										if (STR_EQUAL_CI(OBJ_TABLE(j)->cols[l]->name, colName)) {
											outerTableColumn = copy(colName);
											findOuterTableColumn = TRUE;
										}
									}
								}
							}
						}

						if (findOuterTableName && findOuterTableColumn) {
							for (int k = 0; k < resultLen; k++) {
								char *tmp = resultSet[k];
								char *dotPos = findSubString(tmp, ".");
								char *tbName = getPrecededTableName(tmp, dotPos + 1);
								if (STR_INEQUAL_CI(OBJ_TABLE(j)->name, tbName)) {
									PRINT_TO_FILE(F, "		ADDQ(\" AND %s = \");\n", tmp);
									//findOuterTableName = TRUE;
								}
							}
							for (int k = 0; k < resultLen; k++) {
								char *tmp = resultSet[k];
								char *dotPos = findSubString(tmp, ".");
								char *tbName_tmp = getPrecededTableName(tmp, dotPos + 1);
								if (STR_EQUAL_CI(OBJ_TABLE(j)->name, tbName_tmp)) {
									char *colName = dotPos + 1;
									colName = trim(colName);
									for (int l = 0; l < OBJ_TABLE(j)->nCols; l++) {		
										if (STR_EQUAL_CI(OBJ_TABLE(j)->cols[l]->name, colName)) {
											//findOuterTableColumn = TRUE;
											char *quote;
											char *strprefix;
											char *ctype = createCType(OBJ_TABLE(j)->cols[l]->type);
											if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
											else { quote = ""; strprefix = "str_"; }
											PRINT_TO_FILE(F, "		ADDQ(\"%s\"); \n", quote);
											PRINT_TO_FILE(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[l]->varName);
											PRINT_TO_FILE(F, "		ADDQ(\"%s\"); \n", quote);
											FREE(ctype);
										}
									}
								}
							}
						}
					}
					if (OBJ_SQ->hasGroupby) PRINT_TO_FILE(F, "		ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
					if (OBJ_SQ->hasHaving) PRINT_TO_FILE(F, "		ADDQ(\" having %s\");\n", OBJ_SQ->having);
					PRINT_TO_FILE(F, "		EXEC;\n\n");
					PRINT_TO_FILE(F, "		DEFR(%d);\n\n", OBJ_NSELECTED - 1);
					PRINT_TO_FILE(F, "		if(NO_ROW) goto end_; \n");
					PRINT_TO_FILE(F, "\n		// Save dQ result\n		FOR_EACH_RESULT_ROW(i) {\n");
					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						//if the column belongs to the main table, get the value
						if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
							PRINT_TO_FILE(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
							PRINT_TO_FILE(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
						//if the column belongs to the complement table, check NULL value 
						else {
							PRINT_TO_FILE(F, "			if(NULL_CELL(i, %d)){\n", i + 1);
							PRINT_TO_FILE(F, "				%s = \"null\";\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "			}\n");
							PRINT_TO_FILE(F, "			else {\n");
							PRINT_TO_FILE(F, "				GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
							PRINT_TO_FILE(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "			}\n");
						}
					}

					PRINT_TO_FILE(F, "		}\n");
					// For each dQi:
					PRINT_TO_FILE(F, "\n		// For each dQi:\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) { \n", OBJ_NSELECTED - 1);

					k = 0;
					for (i = 0; i < OBJ_NSELECTED - 1; i++)
						PRINT_TO_FILE(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

					PRINT_TO_FILE(F, "				DEFQ(\"select * from %s where true \"); \n", mvName);
					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						if (OBJ_SELECTED_COL(i)->table->isMainTableOfOuterJoin) {
							char *quote;
							char *ctype;
							ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							PRINT_TO_FILE(F, "			ADDQ(\" and %s = %s\"); \n", OBJ_SELECTED_COL(i)->name, quote);
							PRINT_TO_FILE(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "			ADDQ(\"%s \"); \n", quote);

							FREE(ctype);
						}
					}
					PRINT_TO_FILE(F, "			EXEC; \n");
					PRINT_TO_FILE(F, "			if(ROW_NUM_IN_RESULT_SET == 1 && !%d){\n", OBJ_TABLE(j)->hasColumnInWhereClause);
					int colInSelect = 0;
					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						if (OBJ_SELECTED_COL(i)->table->isComplementTableOfOuterJoin) {
							colInSelect++;
						}
					}
					
					PRINT_TO_FILE(F, "				DEFQ(\"update %s set \");\n", mvName);
					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						if (OBJ_SELECTED_COL(i)->table->isComplementTableOfOuterJoin) {
							PRINT_TO_FILE(F, "				ADDQ(\"%s = null\");\n", OBJ_SELECTED_COL(i)->name);
							colInSelect--;
							if (colInSelect > 0) {
								PRINT_TO_FILE(F, "				ADDQ(\",\");\n");
							}
						}
					}
					PRINT_TO_FILE(F, "				ADDQ(\" where true \");\n");

					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						if (OBJ_SELECTED_COL(i)->table->isMainTableOfOuterJoin) {
							char *quote;
							char *ctype;
							ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							PRINT_TO_FILE(F, "				ADDQ(\" and %s = %s\"); \n", OBJ_SELECTED_COL(i)->name, quote);
							PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "				ADDQ(\"%s \"); \n", quote);							
							FREE(ctype);
						}
					}
					PRINT_TO_FILE(F, "				EXEC;\n");
					PRINT_TO_FILE(F, "			} else {\n");
					PRINT_TO_FILE(F, "				DEFQ(\"delete from %s \");\n", mvName);
					PRINT_TO_FILE(F, "				ADDQ(\" where true \");\n");
					
					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						if (OBJ_SELECTED_COL(i)->table->isMainTableOfOuterJoin) {
							char *quote;
							char *ctype;
							ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							PRINT_TO_FILE(F, "				ADDQ(\" and %s = %s\"); \n", OBJ_SELECTED_COL(i)->name, quote);
							PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "				ADDQ(\"%s \"); \n", quote);
							
							FREE(ctype);
						}
					}
					PRINT_TO_FILE(F, "				ADDQ(\" AND true \");\n");
					
					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						if (OBJ_SELECTED_COL(i)->table->isComplementTableOfOuterJoin) {
							for (int k = 0; k < OBJ_TABLE(j)->nCols; k++) {
								if (STR_EQUAL_CI(OBJ_TABLE(j)->cols[k]->name, OBJ_SELECTED_COL(i)->name)) {
									char *quote;
									char *strprefix;
									char *ctype = createCType(OBJ_TABLE(j)->cols[k]->type);
									if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
									else { quote = ""; strprefix = "str_"; }
									PRINT_TO_FILE(F, "				ADDQ(\" and %s = %s\"); \n", OBJ_TABLE(j)->cols[k]->name, quote);
									PRINT_TO_FILE(F, "				ADDQ(%s%s);\n", strprefix,OBJ_TABLE(j)->cols[k]->varName);
									PRINT_TO_FILE(F, "				ADDQ(\"%s \"); \n", quote);									
									FREE(ctype);
								}
							}
							
						}
					}
					PRINT_TO_FILE(F, "				EXEC;\n");
					PRINT_TO_FILE(F, "			}\n");
					PRINT_TO_FILE(F, "		}\n");
					PRINT_TO_FILE(F, "	}\n");
					PRINT_TO_FILE(F, "	end_: END; \n");
					PRINT_TO_FILE(F, "}\n");
				}
			}


			/*******************************************************************************************************
														UPDATE EVENT
			********************************************************************************************************/
			if (TRUE)
			{
				Boolean check;
				// Define trigger function
				char *triggerName;
				STR_INIT(triggerName, "pmvtg_");
				STR_APPEND(triggerName, mvName);
				STR_APPEND(triggerName, "_update_on_");
				STR_APPEND(triggerName, OBJ_TABLE(j)->name);
				PRINT_TO_FILE(F, "TRIGGER(%s) {\n", triggerName);
				FREE(triggerName);

				// Variable declaration
				PRINT_TO_FILE(F, "	// MV's data\n");
				for (i = 0; i < OBJ_NSELECTED; i++) {
					char *varname = OBJ_SELECTED_COL(i)->varName;
					char *tableName = "<expression>";
					if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
					PRINT_TO_FILE(F, "	char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
				}

				// Table's vars
				PRINT_TO_FILE(F, "	// Table's data\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *varname = OBJ_TABLE(j)->cols[i]->varName;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					PRINT_TO_FILE(F, "	%s %s;\n", ctype, varname);
					if (STR_INEQUAL(ctype, "char *")) {
						if (STR_EQUAL(ctype, "Numeric"))
							PRINT_TO_FILE(F, "	char * str_%s;\n", varname);
						else
							PRINT_TO_FILE(F, "	char str_%s[20];\n", varname);
					}
					FREE(ctype);
				}

				// Standard trigger preparing procedures
				PRINT_TO_FILE(F, "\n	BEGIN;\n\n");

				PRINT_TO_FILE(F, "	if(UTRIGGER_FIRED_BEFORE){\n");
				// Get values of table's fields
				PRINT_TO_FILE(F, "	// Get table's data\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *func;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					char *convert = "";
					if (STR_EQUAL(ctype, "char *"))
						func = "GET_STR_ON_TRIGGERED_ROW";
					else if (STR_EQUAL(ctype, "int")) {
						func = "GET_INT32_ON_TRIGGERED_ROW";
						convert = "INT32_TO_STR";
					}
					else if (STR_EQUAL(ctype, "int64")) {
						func = "GET_INT64_ON_TRIGGERED_ROW";
						convert = "INT64_TO_STR";
					}
					else {
						func = "GET_NUMERIC_ON_TRIGGERED_ROW";
						convert = "NUMERIC_TO_STR";
					}
					PRINT_TO_FILE(F, "	%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						PRINT_TO_FILE(F, "	%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
					FREE(ctype);
				}

				// FCC
				PRINT_TO_FILE(F, "\n	// FCC\n");
				/*GENERATE CODE FOR MAIN TABLE*/
				if (OBJ_TABLE(j)->isMainTableOfOuterJoin) {
					PRINT_TO_FILE(F, "	if(%s){\n",  OBJ_FCC(j));
					if (STR_EQUAL_CI(OBJ_SQ->outerJoinType, "FULL")) {

						GenReQueryOuterJoinWithPrimaryKey(j, F, OBJ_SELECT_CLAUSE);

						if (OBJ_SQ->hasGroupby) PRINT_TO_FILE(F, "		ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
						if (OBJ_SQ->hasHaving) PRINT_TO_FILE(F, "		ADDQ(\" having %s\");\n", OBJ_SQ->having);

						PRINT_TO_FILE(F, "		EXEC;\n");
						// Allocate cache
						PRINT_TO_FILE(F, "\n		// Allocate cache\n		DEFR(%d);\n", OBJ_NSELECTED - 1); //-1 because mvcount is not neccessary
																											  // Save dQ result
						PRINT_TO_FILE(F, "\n		// Save dQ result\n		FOR_EACH_RESULT_ROW(i) {\n");

						for (i = 0; i < OBJ_NSELECTED - 1; i++) {
							//if the column belongs to the main table, get the value
							if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
								PRINT_TO_FILE(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
								PRINT_TO_FILE(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
							}
							//if the column belongs to the complement table, check NULL value 
							else {
								PRINT_TO_FILE(F, "			if(NULL_CELL(i, %d)){\n", i + 1);
								PRINT_TO_FILE(F, "				%s = \"null\";\n", OBJ_SELECTED_COL(i)->varName);
								PRINT_TO_FILE(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
								PRINT_TO_FILE(F, "			}\n");
								PRINT_TO_FILE(F, "			else {\n");
								PRINT_TO_FILE(F, "				GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
								PRINT_TO_FILE(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
								PRINT_TO_FILE(F, "			}\n");
							}
						}

						PRINT_TO_FILE(F, "		}\n");
						// For each dQi:
						PRINT_TO_FILE(F, "\n		// For each dQi:\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED - 1);

						k = 0;
						for (i = 0; i < OBJ_NSELECTED - 1; i++)
							PRINT_TO_FILE(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);
						/* 1.1 - NEU LA FULL OUTER JOIN*/
						//kiem tra join
						PRINT_TO_FILE(F, "			//Neu chua join \n");
						PRINT_TO_FILE(F, "			if(1 ");
						for (i = 0; i < OBJ_NSELECTED - 1; i++) {
							if (STR_INEQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
								PRINT_TO_FILE(F, "&& STR_EQUAL_CI(%s, \"null\") ", OBJ_SELECTED_COL(i)->varName);
							}
						}
						PRINT_TO_FILE(F, "){\n");

						PRINT_TO_FILE(F, "				DEFQ(\"delete from %s where true \");\n", mvName);
						for (i = 0; i < OBJ_NSELECTED - 1; i++) {
							if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
								for (int k = 0; k < OBJ_TABLE(j)->nCols; k++) {
									if (STR_EQUAL_CI(OBJ_TABLE(j)->cols[k]->name, OBJ_SELECTED_COL(i)->name)) {
										char *quote;
										char *ctype = createCType(OBJ_TABLE(j)->cols[k]->type);
										if (STR_EQUAL(ctype, "char *")) { quote = "\'"; }
										else { quote = ""; }
										PRINT_TO_FILE(F, "				ADDQ(\" and %s = %s\"); \n", OBJ_SELECTED_COL(i)->name, quote);
										PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
										PRINT_TO_FILE(F, "				ADDQ(\"%s \"); \n", quote);

										FREE(ctype);
									}
								}
							}
						}
						PRINT_TO_FILE(F, "				EXEC;\n");
						PRINT_TO_FILE(F, "			}//end of kiem tra chua join\n");
						PRINT_TO_FILE(F, "			else {//neu da join\n");

						/* REWRITE THE SELECT QUERY TO MV */
						PRINT_TO_FILE(F, "				DEFQ(\"select * from %s where true \");\n", mvName);
						for (i = 0; i < OBJ_NSELECTED - 1; i++) {
							if (STR_INEQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
								char *quote;
								char *ctype;
								ctype = createCType(OBJ_SELECTED_COL(i)->type);
								if (STR_EQUAL(ctype, "char *")) quote = "\'";
								else quote = "";
								PRINT_TO_FILE(F, "				ADDQ(\" and %s = %s\"); \n", OBJ_SELECTED_COL(i)->name, quote);
								PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
								PRINT_TO_FILE(F, "				ADDQ(\"%s \"); \n", quote);
								FREE(ctype);
							}
						}
						for (i = 0; i < OBJ_NSELECTED - 1; i++) {
							if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {

								PRINT_TO_FILE(F, "				ADDQ(\" and %s is not null \"); \n", OBJ_SELECTED_COL(i)->name);
							}
						}
						PRINT_TO_FILE(F, "				EXEC;\n");
						PRINT_TO_FILE(F, "				if(ROW_NUM_IN_RESULT_SET == 1 && !%d){\n", OBJ_TABLE(j)->hasColumnInWhereClause);
						int colInSelect = 0;
						for (i = 0; i < OBJ_NSELECTED - 1; i++) {
							if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
								colInSelect++;
							}
						}

						PRINT_TO_FILE(F, "					DEFQ(\"update %s set \");\n", mvName);
						for (i = 0; i < OBJ_NSELECTED - 1; i++) {
							if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
								PRINT_TO_FILE(F, "					ADDQ(\"%s = null\");\n", OBJ_SELECTED_COL(i)->name);
								colInSelect--;
								if (colInSelect > 0) {
									PRINT_TO_FILE(F, "					ADDQ(\",\");\n");
								}
							}
						}
						PRINT_TO_FILE(F, "					ADDQ(\" where true \");\n");

						for (i = 0; i < OBJ_NSELECTED - 1; i++) {
							char *quote;
							char *ctype;
							ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							PRINT_TO_FILE(F, "					ADDQ(\" and %s = %s\"); \n", OBJ_SELECTED_COL(i)->name, quote);
							PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					ADDQ(\"%s \"); \n", quote);
							FREE(ctype);
						}
						PRINT_TO_FILE(F, "					EXEC;\n");
						PRINT_TO_FILE(F, "				} else {\n");
						PRINT_TO_FILE(F, "					DEFQ(\"delete from %s \");\n", mvName);
						PRINT_TO_FILE(F, "					ADDQ(\" where true \");\n");

						for (i = 0; i < OBJ_NSELECTED - 1; i++) {
							if (OBJ_SELECTED_COL(i)->table->isMainTableOfOuterJoin) {
								char *quote;
								char *ctype;
								ctype = createCType(OBJ_SELECTED_COL(i)->type);
								if (STR_EQUAL(ctype, "char *")) quote = "\'";
								else quote = "";
								PRINT_TO_FILE(F, "					ADDQ(\" and %s = %s\"); \n", OBJ_SELECTED_COL(i)->name, quote);
								PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
								PRINT_TO_FILE(F, "					ADDQ(\"%s \"); \n", quote);

								FREE(ctype);
							}
						}
						PRINT_TO_FILE(F, "					ADDQ(\" AND true \");\n");
							
						for (i = 0; i < OBJ_NSELECTED - 1; i++) {
							if (OBJ_SELECTED_COL(i)->table->isComplementTableOfOuterJoin) {
								for (int k = 0; k < OBJ_TABLE(j)->nCols; k++) {
									if (STR_EQUAL_CI(OBJ_TABLE(j)->cols[k]->name, OBJ_SELECTED_COL(i)->name)) {
										char *quote;
										char *strprefix;
										char *ctype = createCType(OBJ_TABLE(j)->cols[k]->type);
										if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
										else { quote = ""; strprefix = "str_"; }
										PRINT_TO_FILE(F, "					ADDQ(\" and %s = %s\"); \n", OBJ_TABLE(j)->cols[k]->name, quote);
										PRINT_TO_FILE(F, "					ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[k]->varName);
										PRINT_TO_FILE(F, "					ADDQ(\"%s \"); \n", quote);
										FREE(ctype);
									}
								}

							}
						}
						PRINT_TO_FILE(F, "					EXEC;\n");
						PRINT_TO_FILE(F, "				}\n");
						PRINT_TO_FILE(F, "			}\n");
						PRINT_TO_FILE(F, "		}//end of kiem tra da join\n");
					}
					//LEFT - RIGHT OUTER JOIN
					else {
						PRINT_TO_FILE(F, "		DEFQ(\"delete from %s where true \");\n", mvName);
						for (i = 0; i < OBJ_NSELECTED - 1; i++) {
							if (OBJ_SELECTED_COL(i)->table->isMainTableOfOuterJoin) {
								for (int k = 0; k < OBJ_TABLE(j)->nCols; k++) {
									if (STR_EQUAL_CI(OBJ_TABLE(j)->cols[k]->name, OBJ_SELECTED_COL(i)->name)) {
										char *quote;
										char *strprefix;
										char *ctype = createCType(OBJ_TABLE(j)->cols[k]->type);
										if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
										else { quote = ""; strprefix = "str_"; }
										PRINT_TO_FILE(F, "		ADDQ(\" and %s = %s\"); \n", OBJ_TABLE(j)->cols[k]->name, quote);
										PRINT_TO_FILE(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[k]->varName);
										PRINT_TO_FILE(F, "		ADDQ(\"%s \"); \n", quote);

										FREE(ctype);
									}
								}
							}
						}
						PRINT_TO_FILE(F, "		EXEC;\n");
					}

					PRINT_TO_FILE(F, "	}\n");
					PRINT_TO_FILE(F, "	END; \n");
				}
				/*GENERATE CODE FOR COMPLEMENT TABLE*/
				else if (OBJ_TABLE(j)->isComplementTableOfOuterJoin) {
					PRINT_TO_FILE(F, "		if(%s){\n",  OBJ_FCC(j));
					PRINT_TO_FILE(F, "		//Re-query \n");
					char *outerJoinSelect = copy(OBJ_SELECT_CLAUSE);
					outerJoinSelect = replaceFirst(outerJoinSelect, ", count(*)", "");
					PRINT_TO_FILE(F, "			DEFQ(\"%s \");\n", outerJoinSelect);
					PRINT_TO_FILE(F, "			ADDQ(\" from \");\n");
					PRINT_TO_FILE(F, "			ADDQ(\"%s\");\n", OBJ_SQ->from);
					PRINT_TO_FILE(F, "			ADDQ(\" where true \");\n");
					if (OBJ_SQ->hasWhere) PRINT_TO_FILE(F, "			ADDQ(\" and (%s)\");\n", OBJ_SQ->where);
					for (int i = 0; i < OBJ_BIN_NCONDITIONS(j); i++) {

						char *condition = copy(OBJ_BIN_CONDITIONS(j, i));
						char* resultSet[MAX_NUMBER_OF_ELEMENTS];
						int *resultLen = 0;
						char *pos = strstr(condition, "=");
						char *operator = NULL;
						switch (*(pos - 1)) {
						case '>':
							operator = ">=";
							break;
						case '<':
							operator = "<=";
							break;
						case '!':
							operator = "!=";
							break;
						default:
							operator = "==";
							break;
						}
						dammf_split(condition, operator, resultSet, &resultLen, FALSE);
						Boolean findOuterTableName = FALSE;
						Boolean findOuterTableColumn = FALSE;
						char *outerTableName = "";
						char *outerTableColumn = "";
						for (int k = 0; k < resultLen; k++) {
							char *tmp = resultSet[k];
							char *dotPos = findSubString(tmp, ".");
							outerTableName = getPrecededTableName(tmp, dotPos + 1);
							if (STR_INEQUAL_CI(OBJ_TABLE(j)->name, outerTableName)) {
								findOuterTableName = TRUE;
							}
						}
						if (findOuterTableName) {
							for (int k = 0; k < resultLen; k++) {
								char *tmp = resultSet[k];
								char *dotPos = findSubString(tmp, ".");
								char *tbName_tmp = getPrecededTableName(tmp, dotPos + 1);
								if (tbName_tmp, outerTableName) {
									char *colName = dotPos + 1;
									colName = trim(colName);
									for (int l = 0; l < OBJ_TABLE(j)->nCols; l++) {
										if (STR_EQUAL_CI(OBJ_TABLE(j)->cols[l]->name, colName)) {
											outerTableColumn = copy(colName);
											findOuterTableColumn = TRUE;
										}
									}
								}
							}
						}

						if (findOuterTableName && findOuterTableColumn) {
							for (int k = 0; k < resultLen; k++) {
								char *tmp = resultSet[k];
								char *dotPos = findSubString(tmp, ".");
								char *tbName = getPrecededTableName(tmp, dotPos + 1);
								if (STR_INEQUAL_CI(OBJ_TABLE(j)->name, tbName)) {
									PRINT_TO_FILE(F, "			ADDQ(\" AND %s = \");\n", tmp);
									//findOuterTableName = TRUE;
								}
							}
							for (int k = 0; k < resultLen; k++) {
								char *tmp = resultSet[k];
								char *dotPos = findSubString(tmp, ".");
								char *tbName_tmp = getPrecededTableName(tmp, dotPos + 1);
								if (STR_EQUAL_CI(OBJ_TABLE(j)->name, tbName_tmp)) {
									char *colName = dotPos + 1;
									colName = trim(colName);
									for (int l = 0; l < OBJ_TABLE(j)->nCols; l++) {
										if (STR_EQUAL_CI(OBJ_TABLE(j)->cols[l]->name, colName)) {
											//findOuterTableColumn = TRUE;
											char *quote;
											char *strprefix;
											char *ctype = createCType(OBJ_TABLE(j)->cols[l]->type);
											if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
											else { quote = ""; strprefix = "str_"; }
											PRINT_TO_FILE(F, "			ADDQ(\"%s\"); \n", quote);
											PRINT_TO_FILE(F, "			ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[l]->varName);
											PRINT_TO_FILE(F, "			ADDQ(\"%s\"); \n", quote);
											FREE(ctype);
										}
									}
								}
							}
						}
					}
					if (OBJ_SQ->hasGroupby) PRINT_TO_FILE(F, "			ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
					if (OBJ_SQ->hasHaving) PRINT_TO_FILE(F, "			ADDQ(\" having %s\");\n", OBJ_SQ->having);
					PRINT_TO_FILE(F, "			EXEC;\n\n");
					PRINT_TO_FILE(F, "			DEFR(%d);\n\n", OBJ_NSELECTED - 1);
					PRINT_TO_FILE(F, "			if(NO_ROW) goto end_; \n");

					PRINT_TO_FILE(F, "\n			// Save dQ result\n			FOR_EACH_RESULT_ROW(i) {\n");

					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						//if the column belongs to the main table, get the value
						if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
							PRINT_TO_FILE(F, "				GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
							PRINT_TO_FILE(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
						//if the column belongs to the complement table, check NULL value 
						else {
							PRINT_TO_FILE(F, "				if(NULL_CELL(i, %d)){\n", i + 1);
							PRINT_TO_FILE(F, "					%s = \"null\";\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "				}\n");
							PRINT_TO_FILE(F, "				else {\n");
							PRINT_TO_FILE(F, "					GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
							PRINT_TO_FILE(F, "					ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "				}\n");
						}
					}

					PRINT_TO_FILE(F, "			}\n");
					// For each dQi:
					PRINT_TO_FILE(F, "\n			// For each dQi:\n			FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED - 1);

					k = 0;
					for (i = 0; i < OBJ_NSELECTED - 1; i++)
						PRINT_TO_FILE(F, "				%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

					PRINT_TO_FILE(F, "				DEFQ(\"select * from %s where true \"); \n", mvName);
					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						if (OBJ_SELECTED_COL(i)->table->isMainTableOfOuterJoin) {
							char *quote;
							char *ctype;
							ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							PRINT_TO_FILE(F, "				ADDQ(\" and %s = %s\"); \n", OBJ_SELECTED_COL(i)->name, quote);
							PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "				ADDQ(\"%s \"); \n", quote);

							FREE(ctype);
						}
					}
					PRINT_TO_FILE(F, "				EXEC; \n");
					PRINT_TO_FILE(F, "				if(ROW_NUM_IN_RESULT_SET == 1 && !%d){\n", OBJ_TABLE(j)->hasColumnInWhereClause);
					int colInSelect = 0;
					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						if (OBJ_SELECTED_COL(i)->table->isComplementTableOfOuterJoin) {
							colInSelect++;
						}
					}

					PRINT_TO_FILE(F, "					DEFQ(\"update %s set \");\n", mvName);
					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						if (OBJ_SELECTED_COL(i)->table->isComplementTableOfOuterJoin) {
							PRINT_TO_FILE(F, "					ADDQ(\"%s = null\");\n", OBJ_SELECTED_COL(i)->name);
							colInSelect--;
							if (colInSelect > 0) {
								PRINT_TO_FILE(F, "					ADDQ(\",\");\n");
							}
						}
					}

					PRINT_TO_FILE(F, "					ADDQ(\" where true \");\n");

					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						if (OBJ_SELECTED_COL(i)->table->isMainTableOfOuterJoin) {
							char *quote;
							char *ctype;
							ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							PRINT_TO_FILE(F, "					ADDQ(\" and %s = %s\"); \n", OBJ_SELECTED_COL(i)->name, quote);
							PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					ADDQ(\"%s \"); \n", quote);
							FREE(ctype);
						}
					}
					PRINT_TO_FILE(F, "					EXEC;\n");
					PRINT_TO_FILE(F, "				} else {\n");
					PRINT_TO_FILE(F, "					DEFQ(\"delete from %s \");\n", mvName);
					PRINT_TO_FILE(F, "					ADDQ(\" where true \");\n");

					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						if (OBJ_SELECTED_COL(i)->table->isMainTableOfOuterJoin) {
							char *quote;
							char *ctype;
							ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							PRINT_TO_FILE(F, "					ADDQ(\" and %s = %s\"); \n", OBJ_SELECTED_COL(i)->name, quote);
							PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					ADDQ(\"%s \"); \n", quote);

							FREE(ctype);
						}
					}
					PRINT_TO_FILE(F, "					ADDQ(\" AND true \");\n");

					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						if (OBJ_SELECTED_COL(i)->table->isComplementTableOfOuterJoin) {
							for (int k = 0; k < OBJ_TABLE(j)->nCols; k++) {
								if (STR_EQUAL_CI(OBJ_TABLE(j)->cols[k]->name, OBJ_SELECTED_COL(i)->name)) {
									char *quote;
									char *strprefix;
									char *ctype = createCType(OBJ_TABLE(j)->cols[k]->type);
									if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
									else { quote = ""; strprefix = "str_"; }
									PRINT_TO_FILE(F, "					ADDQ(\" and %s = %s\"); \n", OBJ_TABLE(j)->cols[k]->name, quote);
									PRINT_TO_FILE(F, "					ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[k]->varName);
									PRINT_TO_FILE(F, "					ADDQ(\"%s \"); \n", quote);
									FREE(ctype);
								}
							}

						}
					}
					PRINT_TO_FILE(F, "					EXEC;\n");
					PRINT_TO_FILE(F, "				}\n");

					PRINT_TO_FILE(F, "			}\n");
					PRINT_TO_FILE(F, "		}\n");
				}
				PRINT_TO_FILE(F, "	 } else if(!UTRIGGER_FIRED_BEFORE){\n");
				// Get values of table's fields
				PRINT_TO_FILE(F, "	// Get table's data\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *func;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					char *convert = "";
					if (STR_EQUAL(ctype, "char *"))
						func = "GET_STR_ON_NEW_ROW";
					else if (STR_EQUAL(ctype, "int")) {
						func = "GET_INT32_ON_NEW_ROW";
						convert = "INT32_TO_STR";
					}
					else if (STR_EQUAL(ctype, "int64")) {
						func = "GET_INT64_ON_NEW_ROW";
						convert = "INT64_TO_STR";
					}
					else {
						func = "GET_NUMERIC_ON_NEW_ROW";
						convert = "NUMERIC_TO_STR";
					}
					PRINT_TO_FILE(F, "	%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						PRINT_TO_FILE(F, "	%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
					FREE(ctype);
				}

				// FCC
				PRINT_TO_FILE(F, "\n	// FCC\n");
				PRINT_TO_FILE(F, "		if (%s) {\n", OBJ_FCC(j));

				/* 1- GENERATE CODE FOR MAIN TABLE*/
				if (OBJ_TABLE(j)->isMainTableOfOuterJoin) {
					GenReQueryOuterJoinWithPrimaryKey(j, F, OBJ_SELECT_CLAUSE);

					if (OBJ_SQ->hasGroupby) PRINT_TO_FILE(F, "		ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
					if (OBJ_SQ->hasHaving) PRINT_TO_FILE(F, "		ADDQ(\" having %s\");\n", OBJ_SQ->having);

					PRINT_TO_FILE(F, "		EXEC;\n");
					// Allocate cache
					PRINT_TO_FILE(F, "\n		// Allocate cache\n		DEFR(%d);\n", OBJ_NSELECTED - 1); //-1 because mvcount is not neccessary
																										  // Save dQ result
					PRINT_TO_FILE(F, "\n		// Save dQ result\n		FOR_EACH_RESULT_ROW(i) {\n");

					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						//Nếu cột này thuộc bảng chính thì lấy luôn
						if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
							PRINT_TO_FILE(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
							PRINT_TO_FILE(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
						//Nếu cột thuộc bảng bù phải kiểm tra null
						else {
							PRINT_TO_FILE(F, "			if(NULL_CELL(i, %d)){\n", i + 1);
							PRINT_TO_FILE(F, "				%s = \"null\";\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "			}\n");
							PRINT_TO_FILE(F, "			else {\n");
							PRINT_TO_FILE(F, "				GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
							PRINT_TO_FILE(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "			}\n");
						}
					}

					PRINT_TO_FILE(F, "		}\n");
					// For each dQi:
					PRINT_TO_FILE(F, "\n		// For each dQi:\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED - 1);

					k = 0;
					for (i = 0; i < OBJ_NSELECTED - 1; i++)
						PRINT_TO_FILE(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);
					/* 1.1 - NEU LA FULL OUTER JOIN*/
					if (STR_EQUAL_CI(OBJ_SQ->outerJoinType, "FULL")) {
						//kiem tra join
						PRINT_TO_FILE(F, "			//Neu chua join \n");
						PRINT_TO_FILE(F, "			if(1 ");
						for (i = 0; i < OBJ_NSELECTED - 1; i++) {
							if (STR_INEQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
								PRINT_TO_FILE(F, "&& STR_EQUAL_CI(%s, \"null\") ", OBJ_SELECTED_COL(i)->varName);
							}
						}
						PRINT_TO_FILE(F, "){\n");
						PRINT_TO_FILE(F, "					DEFQ(\"insert into %s values (\");\n", mvName);

						for (i = 0; i < OBJ_NSELECTED - 1; i++) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *"))
								quote = "'";
							else quote = "";
							PRINT_TO_FILE(F, "					if(STR_EQUAL_CI(%s, \"null\")){\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "						ADDQ(\"null\"); \n");
							PRINT_TO_FILE(F, "					} else {\n");
							PRINT_TO_FILE(F, "						ADDQ(\"%s\");\n", quote);
							PRINT_TO_FILE(F, "						ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "						ADDQ(\"%s\"); \n", quote);
							PRINT_TO_FILE(F, "					} \n");
							if (i != OBJ_NSELECTED - 2) PRINT_TO_FILE(F, "					ADDQ(\", \");\n");
							FREE(ctype);
						}
						PRINT_TO_FILE(F, "					ADDQ(\")\");\n");
						PRINT_TO_FILE(F, "					EXEC;\n");
						PRINT_TO_FILE(F, "			}//end of kiem tra chua join\n");
						PRINT_TO_FILE(F, "			else {//neu da join\n");
						/*DANGEROUS CHANGES: JUST CORRECT WITH 2 TABLES ONLY*/
						Boolean isCurrentTableMain = OBJ_TABLE(j)->isMainTableOfOuterJoin;
						Boolean isCurrentTableComplement = OBJ_TABLE(j)->isComplementTableOfOuterJoin;

						for (int i = 0; i < OBJ_SQ->fromElementsNum; i++) {
							if (i != j) {
								OBJ_TABLE(j)->isMainTableOfOuterJoin = TRUE;
								OBJ_TABLE(j)->isComplementTableOfOuterJoin = FALSE;
							}
						}
						OBJ_TABLE(j)->isMainTableOfOuterJoin = FALSE;
						OBJ_TABLE(j)->isComplementTableOfOuterJoin = TRUE;
						GenCodeInsertForComplementTable(j, F, mvName, '		');

						OBJ_TABLE(j)->isMainTableOfOuterJoin = isCurrentTableMain;
						OBJ_TABLE(j)->isComplementTableOfOuterJoin = isCurrentTableComplement;
						/*END OF DANGEROUS REGION :v */
						PRINT_TO_FILE(F, "			}//end of kiem tra da join\n");
					}
					/* 1.2 - NEU LA LEFT - RIGHT OUTER JOIN*/
					else {
						// Check if there is a similar row in MV
						PRINT_TO_FILE(F, "\n			// Check if there is a similar row in MV\n");
						PRINT_TO_FILE(F, "			DEFQ(\"select * from %s where true \");\n", mvName);
						for (i = 0; i < OBJ_NSELECTED; i++)
							if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
								char *quote;
								char *ctype;
								ctype = createCType(OBJ_SELECTED_COL(i)->type);
								if (STR_EQUAL(ctype, "char *")) quote = "\'";
								else quote = "";
								if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
									PRINT_TO_FILE(F, "			ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
									PRINT_TO_FILE(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
									PRINT_TO_FILE(F, "			ADDQ(\"%s\"); \n", quote);
								}
								else {
									PRINT_TO_FILE(F, "			ADDQ(\" and %s \");\n", OBJ_SELECTED_COL(i)->name);
									PRINT_TO_FILE(F, "			if(STR_EQUAL_CI(%s, \"null\")){\n", OBJ_SELECTED_COL(i)->varName);
									PRINT_TO_FILE(F, "				ADDQ(\"isnull\"); \n");
									PRINT_TO_FILE(F, "			} else {\n");
									PRINT_TO_FILE(F, "				ADDQ(\"=\"); \n");
									PRINT_TO_FILE(F, "				ADDQ(\"%s\"); \n", quote);
									PRINT_TO_FILE(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
									PRINT_TO_FILE(F, "				ADDQ(\"%s\"); \n", quote);
									PRINT_TO_FILE(F, "			}\n");
								}

								FREE(ctype);
							}

						PRINT_TO_FILE(F, "			EXEC;\n");

						PRINT_TO_FILE(F, "\n			if (NO_ROW) { \n");
						PRINT_TO_FILE(F, "				// insert new row to mv \n");
						PRINT_TO_FILE(F, "				DEFQ(\"insert into %s values (\");\n", mvName);

						for (i = 0; i < OBJ_NSELECTED - 1; i++) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *"))
								quote = "'";
							else quote = "";
							PRINT_TO_FILE(F, "				if(STR_EQUAL_CI(%s, \"null\")){\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					ADDQ(\"null\"); \n");
							PRINT_TO_FILE(F, "				} else {\n");
							PRINT_TO_FILE(F, "					ADDQ(\"%s\");\n", quote);
							PRINT_TO_FILE(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							PRINT_TO_FILE(F, "					ADDQ(\"%s\"); \n", quote);
							PRINT_TO_FILE(F, "				} \n");
							if (i != OBJ_NSELECTED - 2) PRINT_TO_FILE(F, "				ADDQ(\", \");\n");
							FREE(ctype);
						}

						PRINT_TO_FILE(F, "				ADDQ(\")\");\n");
						PRINT_TO_FILE(F, "				EXEC;\n");
						PRINT_TO_FILE(F, "			}\n");
					}
					PRINT_TO_FILE(F, "		}\n");
					// End of trigger
					PRINT_TO_FILE(F, "\n	}\n\n	/*\n		Finish\n	*/\n");
				}

				/* GENERATE CODE FOR COMPLEMENT TABLE */
				else if (OBJ_TABLE(j)->isComplementTableOfOuterJoin) {
					GenReQueryOuterJoinWithPrimaryKey(j, F, OBJ_SELECT_CLAUSE);
					for (int i = 0; i < OBJ_BIN_NCONDITIONS(j); i++) {

						char *condition = copy(OBJ_BIN_CONDITIONS(j, i));
						char* resultSet[MAX_NUMBER_OF_ELEMENTS];
						int *resultLen = 0;
						char *pos = strstr(condition, "=");
						char *operator = NULL;
						switch (*(pos - 1)) {
						case '>':
							operator = ">=";
							break;
						case '<':
							operator = "<=";
							break;
						case '!':
							operator = "!=";
							break;
						default:
							operator = "==";
							break;
						}
						dammf_split(condition, operator, resultSet, &resultLen, FALSE);
						Boolean findOuterTableName = FALSE;
						Boolean findOuterTableColumn = FALSE;
						char *outerTableName = "";
						char *outerTableColumn = "";
						for (int k = 0; k < resultLen; k++) {
							char *tmp = resultSet[k];
							char *dotPos = findSubString(tmp, ".");
							outerTableName = getPrecededTableName(tmp, dotPos + 1);
							if (STR_INEQUAL_CI(OBJ_TABLE(j)->name, outerTableName)) {
								findOuterTableName = TRUE;
							}
						}
						if (findOuterTableName) {
							for (int k = 0; k < resultLen; k++) {
								char *tmp = resultSet[k];
								char *dotPos = findSubString(tmp, ".");
								char *tbName_tmp = getPrecededTableName(tmp, dotPos + 1);
								if (tbName_tmp, outerTableName) {
									char *colName = dotPos + 1;
									colName = trim(colName);
									for (int l = 0; l < OBJ_TABLE(j)->nCols; l++) {
										if (STR_EQUAL_CI(OBJ_TABLE(j)->cols[l]->name, colName)) {
											outerTableColumn = copy(colName);
											findOuterTableColumn = TRUE;
										}
									}
								}
							}
						}

						if (findOuterTableName && findOuterTableColumn) {
							for (int k = 0; k < resultLen; k++) {
								char *tmp = resultSet[k];
								char *dotPos = findSubString(tmp, ".");
								char *tbName = getPrecededTableName(tmp, dotPos + 1);
								if (STR_INEQUAL_CI(OBJ_TABLE(j)->name, tbName)) {
									PRINT_TO_FILE(F, "		ADDQ(\" AND %s = \");\n", tmp);
									//findOuterTableName = TRUE;
								}
							}
							for (int k = 0; k < resultLen; k++) {
								char *tmp = resultSet[k];
								char *dotPos = findSubString(tmp, ".");
								char *tbName_tmp = getPrecededTableName(tmp, dotPos + 1);
								if (STR_EQUAL_CI(OBJ_TABLE(j)->name, tbName_tmp)) {
									char *colName = dotPos + 1;
									colName = trim(colName);
									for (int l = 0; l < OBJ_TABLE(j)->nCols; l++) {
										if (STR_EQUAL_CI(OBJ_TABLE(j)->cols[l]->name, colName)) {
											//findOuterTableColumn = TRUE;
											char *quote;
											char *strprefix;
											char *ctype = createCType(OBJ_TABLE(j)->cols[l]->type);
											if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
											else { quote = ""; strprefix = "str_"; }
											PRINT_TO_FILE(F, "		ADDQ(\"%s\"); \n", quote);
											PRINT_TO_FILE(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[l]->varName);
											PRINT_TO_FILE(F, "		ADDQ(\"%s\"); \n", quote);
											FREE(ctype);
										}
									}
								}
							}
						}
					}

					if (OBJ_SQ->hasGroupby) PRINT_TO_FILE(F, "		ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
					if (OBJ_SQ->hasHaving) PRINT_TO_FILE(F, "		ADDQ(\" having %s\");\n", OBJ_SQ->having);

					PRINT_TO_FILE(F, "		EXEC;\n");

					// Allocate cache
					PRINT_TO_FILE(F, "\n		// Allocate cache\n		DEFR(%d);\n", OBJ_NSELECTED - 1);

					// Save dQ result
					PRINT_TO_FILE(F, "\n		// Save dQ result\n		FOR_EACH_RESULT_ROW(i) {\n");

					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						PRINT_TO_FILE(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
						PRINT_TO_FILE(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
					}

					PRINT_TO_FILE(F, "		}\n");
					// For each dQi:
					PRINT_TO_FILE(F, "\n		// For each dQi:\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED - 1);
					k = 0;
					for (i = 0; i < OBJ_NSELECTED - 1; i++)
						PRINT_TO_FILE(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);
					GenCodeInsertForComplementTable(j, F, mvName, ' ');
					PRINT_TO_FILE(F, "\n		}\n");
					// End of trigger
					PRINT_TO_FILE(F, "\n	}\n\n	/*\n		Finish\n	*/\n");
				}
				PRINT_TO_FILE(F, "	 }\n");
				PRINT_TO_FILE(F, "	end_: END;\n");
				PRINT_TO_FILE(F, "}\n");
			}

		} //END OF FOR EACH TABLE IN FROM ELEMENTS NUM

	}//END OF GENERATE C TRIGGER CODE


	/*--------------------------------------------- END OF REGION: OUTER JOIN -------------------------------------
														
	---------------------------------------------------------------------------------------------------------------
	*/
}
// Clear: file paths
FREE(sqlFile);
FREE(triggerFile);
FREE(dllFile);

// Clear: OBJ_A2_TG_TABLE
for (j = 0; j < OBJ_SQ->fromElementsNum; j++)
	for (i = 0; i < OBJ_A2_TG_NTABLES(j); i++)
		FREE(OBJ_A2_TG_TABLE(j, i));

// Clear: OBJ_A2_WHERE_CLAUSE
// Because pointers are not initialized as null, a check is needed before freeing

if (OBJ_A2_CHECK) {
	for (j = 0; j < OBJ_SQ->fromElementsNum; j++)
		FREE(OBJ_A2_WHERE_CLAUSE(j));
}

// Clear: OBJ_SELECT_CLAUSE
FREE(OBJ_SELECT_CLAUSE);

// Clear: OBJ_IN_CONDITIONS
for (i = 0; i < OBJ_SQ->fromElementsNum; i++)
	for (j = 0; j < OBJ_IN_NCONDITIONS(i); j++) {
		FREE(OBJ_IN_CONDITIONS(i, j));
		FREE(OBJ_IN_CONDITIONS_OLD(i, j));
		FREE(OBJ_IN_CONDITIONS_NEW(i, j));
	}

// Clear: OBJ_OUT_CONDITIONS
for (i = 0; i < OBJ_SQ->fromElementsNum; i++)
	for (j = 0; j < OBJ_OUT_NCONDITIONS(i); j++)
		FREE(OBJ_OUT_CONDITIONS(i, j));

// Clear: OBJ_BIN_CONDITIONS
for (i = 0; i < OBJ_SQ->fromElementsNum; i++)
	for (j = 0; j < OBJ_BIN_NCONDITIONS(i); j++)
		FREE(OBJ_BIN_CONDITIONS(i, j));

// Clear: OBJ_FCC
for (i = 0; i < OBJ_SQ->fromElementsNum; i++) {
	FREE(OBJ_FCC(i));
	FREE(OBJ_FCC_OLD(i));
	FREE(OBJ_FCC_NEW(i));
}

// Clear: OBJ_SELECTED_COL
for (i = 0; i < OBJ_NSELECTED; i++)
	FREE_COL(OBJ_SELECTED_COL(i));

// Clear: OBJ_TABLE
for (i = 0; i < OBJ_SQ->fromElementsNum; i++)
	FREE_TABLE(OBJ_TABLE(i));

// Clear: OBJ_SQ
FREE_SELECT_QUERY;
printf("\n------------------- FINISH GENERATING CODE -----------------------\n");
PMVTG_END;
}
