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
Definitions and notation | Khái niệm chung
	Original table. Some docs call it "base table". | Bảng gốc - các bảng tham gia vào mệnh đề FROM trong truy vấn gốc
	Original selecting query - the query used to create MV. | Truy vấn gốc - truy vấn tạo KNT
A2:		Algorithm version 2 - optimized. | Thuật toán tối ưu (thuật toán CNGT cải tiến)
OBJ_A2_CHECK: Boolean variable that checks if the condition for implementing A2 algorithm is met (or not)
Biến boolean kiểm tra TVG đủ điều kiện triển khai thuật toán A2 hay không

---------------------------------------------------------------------------------------------------------*/

MAIN{

	// Loop variables
	/*
	DMT: 
	Description: j is used for table looping through table list in FROM clause
	Biến j thường dùng để lặp qua các bảng trong mệnh đề FROM
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
DMT: Load query's tables and columns in FROM clause to OBJ_TABLE | load các đối tượng table, column trong FROM vào OBJ_TABLE
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
Thuật toán phân tích các cột được chọn trong mệnh đề SELECT
Nếu cột được chọn là cột của 1 bảng, gọi hàm addRealColumn
Nếu không phải cột của 1 bảng (flag = false) thì kiểm tra có phải là hàm hay không
Nếu là hàm SUM, MIN, MAX,... thì AddFunction
Nếu ko phải làm tức là biểu thức gộp, addRealColumn

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

Phần này giải thích chi tiết ở tệp Note.docx
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

	/*	CREATE TRIGGERS' FIRST CHECKING CONDITION - FCC*/
	/*	FCC là biến Boolean ghi nhận thao tác dữ liệu trên BG hiện tại có ảnh hưởng đến KNT hay ko
		Sử dụng để xác định có thực hiện CNGT hay thoát khỏi hàm trigger bằng lệnh if*/
	// 1. Embeding variables
	
	// For each IN condition of table j
	for (k = 0; k < OBJ_IN_NCONDITIONS(j); k++) {

		char *conditionCode, *tmp;
		conditionCode = copy(OBJ_IN_CONDITIONS(j, k));
		// For each table 
		/*
		DMT: These lines of code convert the condition from SQL type to C-code type base on varName
		Những dòng sau convert biểu thức điều kiện từ SQL type sang C-code dựa vào var_name
		ex: STR_EQUAL_CI(sinh_vien.que_quan ,  "Da Nang") -> STR_EQUAL_CI(str_que_quan_sinh_vien_table ,  "Da Nang")
		Find column position, scan backward to find table name by getPrecededTableName function
		Tìm vị trí cột, scan từ sau ra trước để tìm table name
		find conditionCode like tableName.columnName
		replace conditionCode by varName
		//tìm chuỗi điều kiện dạng tableName.columnName và thay bằng varname của column đó
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
// Xây dựng lại mệnh đề SELECT dạng đầy đủ <tên bảng>.<tên cột>, đưa vào OBJ_SELECT_CLAUSE
// Ví dụ người dùng nhập SELECT ma_khoa, ten_khoa FROM khoa
// -> output: select khoa.ma_khoa, khoa.ten_khoa
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

/* DMT: OBJ_A2_CHECK only TRUE if all selected elements of the orinial query contains only SUM or COUNT or both.
If MIN or MAX detected -> OBJ_A2_CHECK = FALSE because we can't implement A2 for ORGSQ that have MIN/MAX in SELECT.
//OBJ_A2_CHECK chỉ trả về TRUE khi tất cả các cột được select trong TVG chỉ chứa SUM hoặc COUNT hoặc cả 2
Nếu Min, Max xuất hiện -> false
*/
if (OBJ_STATIC_BOOL_AGG) {
	if (OBJ_A2_CHECK && OBJ_STATIC_BOOL_AGG_MINMAX) {
		OBJ_A2_CHECK = FALSE;
	}
}

/*	DMT: The block of code below works for detection of A2 condition - all columns of Ti that joined in key of Ti
(primary key or unique key): Ci1, Ci2, ... Cij, ... Cik	*/
/*
	Những dòng sau phân tích để phát hiện điều kiện triển khai thuật toán A2 mục tất cả cột của bảng Ti tham gia vào khóa chính
*/
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
					OBJ_A2_TG_TABLE(j, OBJ_A2_TG_NTABLES(j)++) = copy(tableName);
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
// OBJ_AGG_INVOLVED_CHECK (j) = TRUE nghĩa là table j có cột tham gia vào hàm thống kê
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
DMT: OBJ_KEY_IN_G_CHECK(table) = TRUE means the table has a key in Group By Clause
	OBJ_KEY_IN_G_CHECK(table) = TRUE : table có khóa chính thay gia vào Group by
*/

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
					Note: Break here is very valuable, because in Database structure design, primary key columns
					usually appear first. In case the table has many columns, break can save memory resources.
					-----
					Break ngay khi tìm thấy 1 cột khóa chính không xuất hiện trong mệnh đề Group By
					vì điều kiện cho A2 là tất cả các cột kết hợp trong khoá chính phải xuất hiện trong GROUP BY
					Vì vậy, nếu 1 cột chính không xuất hiện trong GROUP BY, vòng lặp không còn cần thiết nữa.
					Lưu ý: Break ở đây là rất có giá trị, bởi vì trong thiết kế cấu trúc cơ sở dữ liệu, cột khóa chính
					thường xuất hiện đầu tiên. Trong trường hợp bảng có nhiều cột, break có thể tiết kiệm tài nguyên bộ nhớ.
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
		FPRINTF(SQL, "create table %s (\n", mvName);

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
				FPRINTF(SQL, "	%s %s%s", OBJ_SELECTED_COL(i)->name, OBJ_SELECTED_COL(i)->type, characterLength);
			else
				FPRINTF(SQL, "	%s %s%s", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->type, characterLength);

			if (i != OBJ_NSELECTED - 1)
				FPRINTF(SQL, ",\n");
			FREE(characterLength);
			FREE(ctype);
		}
		FPRINTF(SQL, "\n);");

		for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
			// insert
			if (OBJ_AGG_INVOLVED_CHECK(j)) {
				FPRINTF(SQL, "\n\n/* insert on %s*/", OBJ_TABLE(j)->name);
				FPRINTF(SQL, "\ncreate function pmvtg_%s_insert_on_%s() returns trigger as '%s', 'pmvtg_%s_insert_on_%s' language c strict;",
					mvName, OBJ_TABLE(j)->name, dllFile, mvName, OBJ_TABLE(j)->name);

				FPRINTF(SQL, "\ncreate trigger pgtg_%s_insert_on_%s after insert on %s for each row execute procedure pmvtg_%s_insert_on_%s();",
					mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);
			}

			// delete
			FPRINTF(SQL, "\n\n/* delete on %s*/", OBJ_TABLE(j)->name);
			FPRINTF(SQL, "\ncreate function pmvtg_%s_delete_on_%s() returns trigger as '%s', 'pmvtg_%s_delete_on_%s' language c strict;",
				mvName, OBJ_TABLE(j)->name, dllFile, mvName, OBJ_TABLE(j)->name);

			FPRINTF(SQL, "\ncreate trigger pgtg_%s_before_delete_on_%s before delete on %s for each row execute procedure pmvtg_%s_delete_on_%s();",
				mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);

			if (!OBJ_KEY_IN_G_CHECK(j) && OBJ_STATIC_BOOL_AGG_MINMAX)
				FPRINTF(SQL, "\ncreate trigger pgtg_%s_after_delete_on_%s after delete on %s for each row execute procedure pmvtg_%s_delete_on_%s();",
					mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);

			// update
			FPRINTF(SQL, "\n\n/* update on %s*/", OBJ_TABLE(j)->name);
			FPRINTF(SQL, "\ncreate function pmvtg_%s_update_on_%s() returns trigger as '%s', 'pmvtg_%s_update_on_%s' language c strict;",
				mvName, OBJ_TABLE(j)->name, dllFile, mvName, OBJ_TABLE(j)->name);

			if ((!OBJ_A2_CHECK || !OBJ_AGG_INVOLVED_CHECK(j)) && (!OBJ_KEY_IN_G_CHECK(j) || OBJ_AGG_INVOLVED_CHECK(j))) {
				FPRINTF(SQL, "\ncreate trigger pgtg_%s_before_update_on_%s before update on %s for each row execute procedure pmvtg_%s_update_on_%s();",
					mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);
			}

			FPRINTF(SQL, "\ncreate trigger pgtg_%s_after_update_on_%s after update on %s for each row execute procedure pmvtg_%s_update_on_%s();",
				mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);
		}

		fclose(sqlWriter);
	} // --- END OF GENERATING MV'S SQL CODE

	{ // GENERATE C TRIGGER CODE
	  // Macro 'F' is an alias of cWriter variable
		FILE *cWriter = fopen(triggerFile, "w");
		FPRINTF(F, "#include \"ctrigger.h\"\n\n");

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
				FPRINTF(F, "TRIGGER(%s) {\n", triggerName);
				FREE(triggerName);

				// Variable declaration
				FPRINTF(F, "	// MV's data\n");
				for (i = 0; i < OBJ_NSELECTED; i++) {
					char *varname = OBJ_SELECTED_COL(i)->varName;
					char *tableName = "<expression>";
					if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
					FPRINTF(F, "	char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
				}

				// Table's vars
				FPRINTF(F, "	// Table's data\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *varname = OBJ_TABLE(j)->cols[i]->varName;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					FPRINTF(F, "	%s %s;\n", ctype, varname);
					if (STR_INEQUAL(ctype, "char *")) {
						if (STR_EQUAL(ctype, "Numeric"))
							FPRINTF(F, "	char * str_%s;\n", varname);
						else
							FPRINTF(F, "	char str_%s[20];\n", varname);
					}
					FREE(ctype);
				}

				// Standard trigger preparing procedures
				FPRINTF(F, "\n	BEGIN;\n\n");

				// Get values of table's fields
				FPRINTF(F, "	// Get table's data\n");
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
					FPRINTF(F, "	%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						FPRINTF(F, "	%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
					FREE(ctype);
				}

				// FCC
				FPRINTF(F, "\n	// FCC\n");
				FPRINTF(F, "	if (%s) {\n", OBJ_FCC(j));

				// Re-query
				FPRINTF(F, "\n		// Re-query\n");
				FPRINTF(F, "		DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
				FPRINTF(F, "		ADDQ(\" from \");\n");
				FPRINTF(F, "		ADDQ(\"%s\");\n", OBJ_SQ->from);
				FPRINTF(F, "		ADDQ(\" where true \");\n");
				if (OBJ_SQ->hasWhere) FPRINTF(F, "		ADDQ(\" and %s\");\n", OBJ_SQ->where);
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++)
					if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
						else { quote = ""; strprefix = "str_"; }
						FPRINTF(F, "		ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
						FPRINTF(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
						FPRINTF(F, "		ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}
				if (OBJ_SQ->hasGroupby) FPRINTF(F, "		ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
				if (OBJ_SQ->hasHaving) FPRINTF(F, "		ADDQ(\" having %s\");\n", OBJ_SQ->having);

				FPRINTF(F, "		EXEC;\n");

				// Allocate cache
				FPRINTF(F, "\n		// Allocate cache\n		DEFR(%d);\n", OBJ_NSELECTED);

				// Save dQ result
				FPRINTF(F, "\n		// Save dQ result\n		FOR_EACH_RESULT_ROW(i) {\n");

				for (i = 0; i < OBJ_NSELECTED; i++) {
					FPRINTF(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
					FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
				}

				FPRINTF(F, "		}\n");

				// For each dQi:
				FPRINTF(F, "\n		// For each dQi:\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

				k = 0;
				for (i = 0; i < OBJ_NSELECTED; i++)
					FPRINTF(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

				// Check if there is a similar row in MV
				FPRINTF(F, "\n			// Check if there is a similar row in MV\n");
				FPRINTF(F, "			DEFQ(\"select mvcount from %s where true \");\n", mvName);
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype;
						ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						FPRINTF(F, "			ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						FPRINTF(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						FPRINTF(F, "			ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}

				FPRINTF(F, "			EXEC;\n");

				FPRINTF(F, "\n			if (NO_ROW) {\n");
				FPRINTF(F, "				// insert new row to mv\n");
				FPRINTF(F, "				DEFQ(\"insert into %s values (\");\n", mvName);

				for (i = 0; i < OBJ_NSELECTED; i++) {
					char *quote;
					char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
					if (STR_EQUAL(ctype, "char *"))
						quote = "'";
					else quote = "";
					FPRINTF(F, "				ADDQ(\"%s\");\n", quote);
					FPRINTF(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
					FPRINTF(F, "				ADDQ(\"%s\");\n", quote);
					if (i != OBJ_NSELECTED - 1) FPRINTF(F, "				ADDQ(\", \");\n");
					FREE(ctype);
				}

				FPRINTF(F, "				ADDQ(\")\");\n");
				FPRINTF(F, "				EXEC;\n");

				FPRINTF(F, "			} else {\n");

				FPRINTF(F, "				// update old row\n");
				FPRINTF(F, "				DEFQ(\"update %s set \");\n", mvName);
				check = FALSE;
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {

						if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_SUM)) {
							if (check) FPRINTF(F, "				ADDQ(\", \");\n");
							check = TRUE;

							FPRINTF(F, "				ADDQ(\" %s = %s + \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
						else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_COUNT)) {
							if (check) FPRINTF(F, "				ADDQ(\", \");\n");
							check = TRUE;

							FPRINTF(F, "				ADDQ(\" %s = %s + \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
						else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_MIN)) {
							if (check) FPRINTF(F, "				ADDQ(\", \");\n");
							check = TRUE;

							FPRINTF(F, "				ADDQ(\" %s = (case when %s > \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "				ADDQ(\" then \");\n");
							FPRINTF(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "				ADDQ(\" else %s end) \");\n", OBJ_SELECTED_COL(i)->varName);
						}
						else {
							// AF_MAX_
							if (check) FPRINTF(F, "				ADDQ(\", \");\n");
							check = TRUE;

							FPRINTF(F, "				ADDQ(\" %s = (case when %s < \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "				ADDQ(\" then \");\n");
							FPRINTF(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "				ADDQ(\" else %s end) \");\n", OBJ_SELECTED_COL(i)->varName);
						}
					}

				FPRINTF(F, "				ADDQ(\" where true \");\n");
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						FPRINTF(F, "				ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						FPRINTF(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						FPRINTF(F, "				ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}
				FPRINTF(F, "				EXEC;\n");

				FPRINTF(F, "			}\n");
				FPRINTF(F, "		}\n");

				// End of trigger
				FPRINTF(F, "\n	}\n\n	/*\n		Finish\n	*/\n	END;\n}\n\n");

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
				FPRINTF(F, "TRIGGER(%s) {\n", triggerName);
				FREE(triggerName);

				/*
				DMT: According to the A2
				1. if Primary key of table j is in group by clause, it means we can determine what tuples we should
				delete from mv using this key, so that we dont need to determine using its column

				2. if Primary key of table j is NOT IN group by clause, we must determine what tuples should be deleted from MV
				using the table j column
				---
				1. Nếu khóa chính của bảng j nằm trong Group by, có nghĩa là có thể xác định tuples nào chúng ta nên
				Xóa từ mv sử dụng khóa này, do đó không cần phải xác định bằng cách sử dụng cột của nó

				2. Nếu khoá chính của bảng j không nằm trong group by, ta phải xác định tuples nào cần xóa khỏi MV
				Sử dụng cột bảng j
				*/

				/*DMT: second case: Primary key of table j is NOT IN group by clause*/
				if (!OBJ_KEY_IN_G_CHECK(j)) {
					// MV's vars
					FPRINTF(F, "	// MV's data\n");
					for (i = 0; i < OBJ_NSELECTED; i++) {
						char *varname = OBJ_SELECTED_COL(i)->varName;
						char *tableName = "<expression>";
						if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
						FPRINTF(F, "	char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
					}
				}

				// Table's vars
				FPRINTF(F, "	// Table's data\n");
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
						FPRINTF(F, "	%s %s;\n", ctype, varname);
						if (STR_INEQUAL(ctype, "char *")) {
							if (STR_EQUAL(ctype, "Numeric"))
								FPRINTF(F, "	char * str_%s;\n", varname);
							else
								FPRINTF(F, "	char str_%s[20];\n", varname);
						}
						FREE(ctype);
					}
				}

				/*
				DMT: if table j's primary key is in group by clause, it means we can remove the row in MV imediately.
				if table j's primary key is not in group by clause, we need to count how many tuples are being deleting.
				If count == mvcount (count(*) in mv) so just remove the record. otherwise, decrease the count column
				--
				Nếu khóa chính của table j trong G, có nghĩa là có thể loại bỏ các bản ghi trong MV ngay lập tức.
				Nếu khoá chính của bảng j không nằm trong G, cần phải đếm bao nhiêu tuple đang được xoá bỏ.
				Nếu đếm == mvcount (count(*) trong mv) chỉ cần loại bỏ bản ghi. Nếu không, giảm giá trị cột COUNT(*).
				*/
				if (!OBJ_KEY_IN_G_CHECK(j)) {
					// Count
					FPRINTF(F, "	char * count;\n");
				}

				// Standard trigger preparing procedures
				FPRINTF(F, "\n	BEGIN;\n\n");

				// Get values of table's fields
				FPRINTF(F, "	// Get table's data\n");
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
						FPRINTF(F, "	%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
						if (STR_INEQUAL(ctype, "char *"))
							FPRINTF(F, "	%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
						FREE(ctype);
					}
				}

				// FCC
				FPRINTF(F, "\n	// FCC\n");
				//DMT: not neccessary, just comment it
				//FPRINTF(F, "	if (%s) {\n", OBJ_FCC(j));
				FPRINTF(F, "	if (1) {\n");

				if (OBJ_KEY_IN_G_CHECK(j)) {
					FPRINTF(F, "		DEFQ(\"delete from %s where true\");\n", mvName);
					for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
							char *quote;
							char *strprefix;
							char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
							if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
							else { quote = ""; strprefix = "str_"; }
							FPRINTF(F, "		ADDQ(\" and %s = %s\");\n", OBJ_TABLE(j)->cols[i]->name, quote);
							FPRINTF(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
							FPRINTF(F, "		ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}
					}
					FPRINTF(F, "		EXEC;\n");
				}
				else {
					if (OBJ_STATIC_BOOL_AGG_MINMAX)
						FPRINTF(F, "\n	if (UTRIGGER_FIRED_BEFORE) {\n");
					/*
					TRIGGER BODY START HERE
					*/
					// Re-query
					FPRINTF(F, "\n		// Re-query\n");
					FPRINTF(F, "		DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
					FPRINTF(F, "		ADDQ(\" from \");\n");
					FPRINTF(F, "		ADDQ(\"%s\");\n", OBJ_SQ->from);
					FPRINTF(F, "		ADDQ(\" where true \");\n");
					if (OBJ_SQ->hasWhere) FPRINTF(F, "		ADDQ(\" and %s\");\n", OBJ_SQ->where);
					for (i = 0; i < OBJ_TABLE(j)->nCols; i++)
						if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
							char *quote;
							char *strprefix;
							char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
							if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
							else { quote = ""; strprefix = "str_"; }
							FPRINTF(F, "		ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
							FPRINTF(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
							FPRINTF(F, "		ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}
					if (OBJ_SQ->hasGroupby) FPRINTF(F, "		ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
					if (OBJ_SQ->hasHaving) FPRINTF(F, "		ADDQ(\" having %s\");\n", OBJ_SQ->having);

					FPRINTF(F, "		EXEC;\n");

					// Allocate cache
					FPRINTF(F, "\n		// Allocate cache\n		DEFR(%d);\n", OBJ_NSELECTED);

					// Save dQ result
					FPRINTF(F, "\n		// Save dQ result\n		FOR_EACH_RESULT_ROW(i) {\n");

					for (i = 0; i < OBJ_NSELECTED; i++) {
						FPRINTF(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
						FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
					}

					FPRINTF(F, "		}\n");

					// For each dQi:
					FPRINTF(F, "\n		// For each dQi:\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

					k = 0;
					for (i = 0; i < OBJ_NSELECTED; i++)
						FPRINTF(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

					// Check if there is a similar row in MV
					FPRINTF(F, "\n			// Check if there is a similar row in MV\n");
					FPRINTF(F, "			DEFQ(\"select mvcount from %s where true \");\n", mvName);
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							FPRINTF(F, "			ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							FPRINTF(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "			ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}

					FPRINTF(F, "			EXEC;\n");

					FPRINTF(F, "\n			GET_STR_ON_RESULT(count, 0, 1);\n");

					FPRINTF(F, "\n			if (STR_EQUAL(count, mvcount)) {\n");
					FPRINTF(F, "				// Delete the row\n");
					FPRINTF(F, "				DEFQ(\"delete from %s where true\");\n", mvName);

					for (i = 0; i < OBJ_NSELECTED; i++)
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							FPRINTF(F, "				ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							FPRINTF(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "				ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}

					FPRINTF(F, "				EXEC;\n");

					FPRINTF(F, "			} else {\n");

					FPRINTF(F, "				// ow, decrease the count column \n");
					FPRINTF(F, "				DEFQ(\"update %s set \");\n", mvName);

					//Neu la lan dau tien thi khong in dau ',' tu lan 2 tro di co in dau ',' 
					check = FALSE;
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_SUM)) {
								if (check) FPRINTF(F, "				ADDQ(\", \");\n");
								check = TRUE;

								FPRINTF(F, "				ADDQ(\" %s = %s - \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
								FPRINTF(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							}
							else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_COUNT)) {
								if (check) FPRINTF(F, "				ADDQ(\", \");\n");
								check = TRUE;

								FPRINTF(F, "				ADDQ(\" %s = %s - \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
								FPRINTF(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							}
						}
					FPRINTF(F, "				ADDQ(\" where true \");\n");
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							FPRINTF(F, "				ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							FPRINTF(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "				ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}
					FPRINTF(F, "				EXEC;\n");

					FPRINTF(F, "			}\n");
					FPRINTF(F, "		}\n");

					// --------------------------------------------------
					if (OBJ_STATIC_BOOL_AGG_MINMAX) {
						FPRINTF(F, "\n	} else {\n");
						FPRINTF(F, "		//Trigger fired after\n");
						FPRINTF(F, "		DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
						FPRINTF(F, "		ADDQ(\" from \");\n");
						FPRINTF(F, "		ADDQ(\"%s\");\n", OBJ_SQ->from);
						FPRINTF(F, "		ADDQ(\" where true \");\n");
						if (OBJ_SQ->hasWhere) FPRINTF(F, "		ADDQ(\" and %s\");\n", OBJ_SQ->where);
						for (i = 0; i < OBJ_TG_NCOLS(j); i++) {
							char *quote;
							char *strprefix;
							char *ctype = createCType(OBJ_TG_COL(j, i)->type);
							if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
							else { quote = ""; strprefix = "str_"; }
							FPRINTF(F, "		ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TG_COL(j, i)->name, quote);
							FPRINTF(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TG_COL(j, i)->varName);
							FPRINTF(F, "		ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}
						if (OBJ_SQ->hasGroupby) FPRINTF(F, "		ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
						if (OBJ_SQ->hasHaving) FPRINTF(F, "			ADDQ(\" having %s\");\n", OBJ_SQ->having);

						FPRINTF(F, "		EXEC;\n");

						FPRINTF(F, "\n		DEFR(%d);\n", OBJ_NSELECTED);

						FPRINTF(F, "\n		FOR_EACH_RESULT_ROW(i) {\n");

						for (i = 0; i < OBJ_NSELECTED; i++) {
							FPRINTF(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
							FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}

						FPRINTF(F, "		}\n");

						// For each dQi:
						FPRINTF(F, "\n		// For each dQi:\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

						k = 0;
						for (i = 0; i < OBJ_NSELECTED; i++)
							FPRINTF(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

						FPRINTF(F, "			DEFQ(\"update %s set mvcount = mvcount \");\n", mvName);

						for (i = 0; i < OBJ_NSELECTED; i++) {
							if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_MIN) || STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_MAX_)) {
								FPRINTF(F, "			ADDQ(\", %s = \");\n", OBJ_SELECTED_COL(i)->varName);
								FPRINTF(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							}
						}

						FPRINTF(F, "			ADDQ(\" where true \");\n");
						for (i = 0; i < OBJ_NSELECTED; i++)
							if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
								char *quote;
								char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
								if (STR_EQUAL(ctype, "char *")) quote = "\'";
								else quote = "";
								FPRINTF(F, "			ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
								FPRINTF(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
								FPRINTF(F, "			ADDQ(\"%s \");\n", quote);
								FREE(ctype);
							}
						FPRINTF(F, "			EXEC;\n");

						FPRINTF(F, "		}\n");

						FPRINTF(F, "\n	}\n");
					}
				}

				// End of trigger
				FPRINTF(F, "\n	}\n\n	END;\n}\n\n");

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
				FPRINTF(F, "TRIGGER(%s) {\n", triggerName);
				FREE(triggerName);
				FPRINTF(F, "//OBJ_A2_CHECK && OBJ_AGG_INVOLVED_CHECK(j)\n");
				// MV's data (old and new rows)
				FPRINTF(F, "	// MV's data (old and new rows)\n", triggerName);
				for (i = 0; i < OBJ_NSELECTED; i++) {
					char *varname = OBJ_SELECTED_COL(i)->varName;
					char *tableName = "<expression>";
					if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
					if (i < OBJ_NSELECTED - 1) {
						FPRINTF(F, "	char *old_%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
						FPRINTF(F, "	char *new_%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
					}
					else {
						FPRINTF(F, "	char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
					}
				}

				// Old and new rows' data
				FPRINTF(F, "	// Old and new rows' data\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *varname = OBJ_TABLE(j)->cols[i]->varName;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					FPRINTF(F, "	%s old_%s;\n", ctype, varname);
					if (STR_INEQUAL(ctype, "char *")) {
						if (STR_EQUAL(ctype, "Numeric"))
							FPRINTF(F, "	char * old_str_%s;\n", varname);
						else
							FPRINTF(F, "	char old_str_%s[20];\n", varname);
					}
					FPRINTF(F, "	%s new_%s;\n", ctype, varname);
					if (STR_INEQUAL(ctype, "char *")) {
						if (STR_EQUAL(ctype, "Numeric"))
							FPRINTF(F, "	char * new_str_%s;\n", varname);
						else
							FPRINTF(F, "	char new_str_%s[20];\n", varname);
					}

					FREE(ctype);
				}
				FPRINTF(F, "	char * old_count;");
				FPRINTF(F, "\n	char * new_count;\n");

				// Begin
				FPRINTF(F, "\n	BEGIN;\n\n");

				// Get data of old rows
				FPRINTF(F, "	// Get data of old rows\n");
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
					FPRINTF(F, "	%s(old_%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						FPRINTF(F, "	%s(old_%s, old_str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);

					FREE(ctype);
				}

				// Get data of new rows
				FPRINTF(F, "	// Get data of new rows\n");
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
					FPRINTF(F, "	%s(new_%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						FPRINTF(F, "	%s(new_%s, new_str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
					FREE(ctype);
				}

				// FCC
				FPRINTF(F, "\n	// FCC - A2_CHECK = TRUE & Table involved AGG \n");
				FPRINTF(F, "	if ((%s) || (%s)) {\n", OBJ_FCC_OLD(j), OBJ_FCC_NEW(j));
				// FPRINTF(F, "		INIT_UPDATE_A2_FLAG;\n");

				// Query
				FPRINTF(F, "		QUERY(\"select \");\n");
				for (i = 0; i < OBJ_NSELECTED; i++) {
					if (OBJ_SELECTED_COL(i)->table) {
						FPRINTF(F, "		_QUERY(\"%s.%s\");\n", OBJ_SELECTED_COL(i)->table->name, OBJ_SELECTED_COL(i)->name);
					}
					else {
						int nParts, partsType[MAX_NUMBER_OF_ELEMENTS];
						char *parts[MAX_NUMBER_OF_ELEMENTS];
						A2Split(j, OBJ_SELECTED_COL(i)->funcArg, &nParts, partsType, parts);
						FPRINTF(F, "		_QUERY(\"%s\");\n", OBJ_SELECTED_COL(i)->func);
						for (k = 0; k < nParts; k++) {
							if (partsType[k] != j) {
								FPRINTF(F, "//--- part k\n");
								FPRINTF(F, "	 	_QUERY(\"%s\");\n", parts[k]);
							}
							else {
								FPRINTF(F, "		_QUERY(old_%s);\n", parts[k]);
							}
						}


						FPRINTF(F, "		_QUERY(\")\");\n");

						for (k = 0; k < nParts; k++) {
							FREE(parts[k]);
						}
					}
					if (i < OBJ_NSELECTED - 1) {
						FPRINTF(F, "		_QUERY(\",\");\n");
					}
				}
				FPRINTF(F, "		_QUERY(\" from \");\n");
				check = FALSE;
				for (i = 0; i < OBJ_SQ->fromElementsNum; i++) {
					if (i != j) {
						if (check)
							FPRINTF(F, "		_QUERY(\", %s\");\n", OBJ_TABLE(i)->name);
						else
							FPRINTF(F, "		_QUERY(\"%s\");\n", OBJ_TABLE(i)->name);
						check = TRUE;
					}
				}
				FPRINTF(F, "		_QUERY(\" where %s \");\n", OBJ_A2_WHERE_CLAUSE(j));
				
				for (i = 0; i < OBJ_TG_NCOLS(j); i++) {
					char *quote;
					char *strprefix;
					char *ctype = createCType(OBJ_TG_COL(j, i)->type);
					if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = "old_"; }
					else { quote = ""; strprefix = "old_str_"; }
					FPRINTF(F, "		_QUERY(\" and %s.%s = %s\");\n", OBJ_A2_TG_TABLE(j, i), OBJ_TG_COL(j, i)->name, quote);
					FPRINTF(F, "		_QUERY(%s%s);\n", strprefix, OBJ_TG_COL(j, i)->varName);
					FPRINTF(F, "		_QUERY(\"%s \");\n", quote);
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
					FPRINTF(F, "		_QUERY(\" and %s %s %s\");\n", leftCondition, operator, quote);
					FPRINTF(F, "		_QUERY(%s%s);\n", strprefix, rightVarName);
					FPRINTF(F, "		_QUERY(\"%s \");\n", quote);
				}



				FPRINTF(F, "		_QUERY(\" group by %s \");\n", OBJ_SQ->groupby);
				FPRINTF(F, "		_QUERY(\" union all \");\n");
				//FPRINTF(F, "		_QUERY(\" %s \");\n", OBJ_SELECT_CLAUSE);
				FPRINTF(F, "		_QUERY(\"select \");\n");
				for (i = 0; i < OBJ_NSELECTED; i++) {
					if (OBJ_SELECTED_COL(i)->table) {
						FPRINTF(F, "		_QUERY(\"%s.%s\");\n", OBJ_SELECTED_COL(i)->table->name, OBJ_SELECTED_COL(i)->name);
					}
					else {
						int nParts, partsType[MAX_NUMBER_OF_ELEMENTS];
						char *parts[MAX_NUMBER_OF_ELEMENTS];
						A2Split(j, OBJ_SELECTED_COL(i)->funcArg, &nParts, partsType, parts);

						FPRINTF(F, "		_QUERY(\"%s\");\n", OBJ_SELECTED_COL(i)->func);
						for (k = 0; k < nParts; k++) {
							if (partsType[k] != j) {
								FPRINTF(F, "		_QUERY(\"%s\");\n", parts[k]);
							}
							else {
								FPRINTF(F, "		_QUERY(new_%s);\n", parts[k]);
							}
						}

						FPRINTF(F, "		_QUERY(\")\");\n");

						for (k = 0; k < nParts; k++) {
							FREE(parts[k]);
						}
					}
					if (i < OBJ_NSELECTED - 1) {
						FPRINTF(F, "		_QUERY(\",\");\n");
					}
				}
				FPRINTF(F, "		_QUERY(\" from \");\n");
				check = FALSE;
				for (i = 0; i < OBJ_SQ->fromElementsNum; i++) {
					if (i != j) {
						if (check)
							FPRINTF(F, "		_QUERY(\", %s\");\n", OBJ_TABLE(i)->name);
						else
							FPRINTF(F, "		_QUERY(\"%s\");\n", OBJ_TABLE(i)->name);
						check = TRUE;
					}
				}
				FPRINTF(F, "		_QUERY(\" where %s \");\n", OBJ_A2_WHERE_CLAUSE(j));
				
				for (i = 0; i < OBJ_TG_NCOLS(j); i++) {
					char *quote;
					char *strprefix;
					char *ctype = createCType(OBJ_TG_COL(j, i)->type);
					if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = "new_"; }
					else { quote = ""; strprefix = "new_str_"; }
					FPRINTF(F, "		_QUERY(\" and %s.%s = %s\");\n", OBJ_A2_TG_TABLE(j, i), OBJ_TG_COL(j, i)->name, quote);
					FPRINTF(F, "		_QUERY(%s%s);\n", strprefix, OBJ_TG_COL(j, i)->varName);
					FPRINTF(F, "		_QUERY(\"%s \");\n", quote);
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
					FPRINTF(F, "		_QUERY(\" and %s %s %s\");\n", leftCondition, operator, quote);
					FPRINTF(F, "		_QUERY(%s%s);\n", strprefix, rightVarName);
					FPRINTF(F, "		_QUERY(\"%s \");\n", quote);
				}

				FPRINTF(F, "		_QUERY(\" group by %s \");\n", OBJ_SQ->groupby);
				FPRINTF(F, "		INF(query);\n\n");
				FPRINTF(F, "		EXECUTE_QUERY;\n\n");

				FPRINTF(F, "		ALLOCATE_CACHE;\n\n");

				FPRINTF(F, "		A2_INIT_CYCLE;\n\n");

				FPRINTF(F, "		FOR_EACH_RESULT_ROW(i) {\n");
				FPRINTF(F, "			if (A2_VALIDATE_CYCLE) {\n");
				//FPRINTF(F, "			if (%s) {\n", OBJ_FCC_OLD(j));
				for (i = 0; i < OBJ_NSELECTED - 1; i++) {
					FPRINTF(F, "				GET_STR_ON_RESULT(old_%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
					FPRINTF(F, "				SAVE_TO_CACHE(old_%s);\n", OBJ_SELECTED_COL(i)->varName);
				}
				FPRINTF(F, "				GET_STR_ON_RESULT(old_count, i, %d);\n", OBJ_NSELECTED);
				FPRINTF(F, "				SAVE_TO_CACHE(old_count);\n");
				//FPRINTF(F, "			}\n");
				FPRINTF(F, "			} else {\n");
				//FPRINTF(F, "			if (%s) {\n", OBJ_FCC_NEW(j));
				for (i = 0; i < OBJ_NSELECTED - 1; i++) {
					FPRINTF(F, "				GET_STR_ON_RESULT(new_%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
					FPRINTF(F, "				SAVE_TO_CACHE(new_%s);\n", OBJ_SELECTED_COL(i)->varName);
				}
				FPRINTF(F, "				GET_STR_ON_RESULT(new_count, i, %d);\n", OBJ_NSELECTED);
				FPRINTF(F, "				SAVE_TO_CACHE(new_count);\n");

				//FPRINTF(F, "			}\n");
				FPRINTF(F, "			}\n");
				FPRINTF(F, "		}\n\n");

				FPRINTF(F, "		A2_INIT_CYCLE;\n\n");

				FPRINTF(F, "		FOR_EACH_CACHE_ROW(i) {\n");
				FPRINTF(F, "			if (A2_VALIDATE_CYCLE) {\n");
				FPRINTF(F, "			if (%s) {\n", OBJ_FCC_OLD(j));
				for (i = 0; i < OBJ_NSELECTED - 1; i++)
					FPRINTF(F, "				old_%s = GET_FROM_CACHE(i, %d);\n", OBJ_SELECTED_COL(i)->varName, i);
				FPRINTF(F, "				old_count = GET_FROM_CACHE(i, %d);\n\n", OBJ_NSELECTED - 1);

				FPRINTF(F, "				QUERY(\"select mvcount from %s where true \");\n", mvName);
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						FPRINTF(F, "				_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						FPRINTF(F, "				_QUERY(old_%s);\n", OBJ_SELECTED_COL(i)->varName);
						FPRINTF(F, "				_QUERY(\"%s \");\n", quote);
						FREE(ctype);
					}

				FPRINTF(F, "				EXECUTE_QUERY;\n\n");
				FPRINTF(F, "				GET_STR_ON_RESULT(mvcount, 0, 1);\n\n");

				FPRINTF(F, "				if (STR_EQUAL(old_count, mvcount)) {\n");
				FPRINTF(F, "					QUERY(\"delete from %s where true\");\n", mvName);

				for (i = 0; i < OBJ_NSELECTED; i++) {
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						FPRINTF(F, "					_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						FPRINTF(F, "					_QUERY(old_%s);\n", OBJ_SELECTED_COL(i)->varName);
						FPRINTF(F, "					_QUERY(\"%s \");\n", quote);
						FREE(ctype);
					}
				}

				FPRINTF(F, "					EXECUTE_QUERY;\n\n");

				FPRINTF(F, "				} else {\n");

				FPRINTF(F, "					QUERY(\"update %s set \");\n", mvName);

				check = FALSE;
				for (i = 0; i < OBJ_NSELECTED - 1; i++) {
					if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {

						if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_SUM)) {
							if (check) FPRINTF(F, "					_QUERY(\", \");\n");
							check = TRUE;

							FPRINTF(F, "					_QUERY(\" %s = %s - \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "					_QUERY(old_%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
						else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_COUNT)) {
							if (check) FPRINTF(F, "					_QUERY(\", \");\n");
							check = TRUE;

							FPRINTF(F, "					_QUERY(\" %s = %s - \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "					_QUERY(old_%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
					}
				}
				if (check) FPRINTF(F, "					_QUERY(\", \");\n");
				FPRINTF(F, "					_QUERY(\" mvcount = mvcount - \");\n");
				FPRINTF(F, "					_QUERY(old_count);\n");

				FPRINTF(F, "					_QUERY(\" where true \");\n");
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						FPRINTF(F, "					_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						FPRINTF(F, "					_QUERY(old_%s);\n", OBJ_SELECTED_COL(i)->varName);
						FPRINTF(F, "					_QUERY(\"%s \");\n", quote);
						FREE(ctype);
					}
				FPRINTF(F, "					EXECUTE_QUERY;\n");
				FPRINTF(F, "				}\n");
				FPRINTF(F, "			}\n");
				FPRINTF(F, "			} else {\n");
				FPRINTF(F, "			if (%s) {\n", OBJ_FCC_NEW(j));

				for (i = 0; i < OBJ_NSELECTED - 1; i++)
					FPRINTF(F, "				new_%s = GET_FROM_CACHE(i, %d);\n", OBJ_SELECTED_COL(i)->varName, i);
				FPRINTF(F, "				new_count = GET_FROM_CACHE(i, %d);\n\n", OBJ_NSELECTED - 1);

				FPRINTF(F, "				QUERY(\"select mvcount from %s where true \");\n", mvName);
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						FPRINTF(F, "				_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						FPRINTF(F, "				_QUERY(new_%s);\n", OBJ_SELECTED_COL(i)->varName);
						FPRINTF(F, "				_QUERY(\"%s \");\n", quote);
						FREE(ctype);
					}

				FPRINTF(F, "				EXECUTE_QUERY;\n\n");

				FPRINTF(F, "				if (NO_ROW) {\n");
				FPRINTF(F, "					QUERY(\"insert into %s values (\");\n", mvName);

				for (i = 0; i < OBJ_NSELECTED - 1; i++) {
					char *quote;
					char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
					if (STR_EQUAL(ctype, "char *"))
						quote = "'";
					else quote = "";
					FPRINTF(F, "					_QUERY(\"%s\");\n", quote);
					FPRINTF(F, "					_QUERY(new_%s);\n", OBJ_SELECTED_COL(i)->varName);
					FPRINTF(F, "					_QUERY(\"%s\");\n", quote);
					FPRINTF(F, "					_QUERY(\", \");\n");
					FREE(ctype);
				}
				FPRINTF(F, "					_QUERY(new_count);\n");

				FPRINTF(F, "					_QUERY(\")\");\n");
				FPRINTF(F, "					EXECUTE_QUERY;\n");

				FPRINTF(F, "				} else {\n");
				FPRINTF(F, "					QUERY(\"update %s set \");\n", mvName);

				check = FALSE;
				for (i = 0; i < OBJ_NSELECTED - 1; i++) {
					if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_SUM)) {
							if (check) FPRINTF(F, "					ADDQ(\", \");\n");
							check = TRUE;

							FPRINTF(F, "					ADDQ(\" %s = %s + \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "					ADDQ(new_%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
						else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_COUNT)) {
							if (check) FPRINTF(F, "					ADDQ(\", \");\n");
							check = TRUE;

							FPRINTF(F, "					ADDQ(\" %s = %s + \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "					ADDQ(new_%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
					}
				}
				if (check) FPRINTF(F, "					_QUERY(\", \");\n");
				FPRINTF(F, "					_QUERY(\" mvcount = mvcount + \");\n");
				FPRINTF(F, "					_QUERY(new_count);\n");

				FPRINTF(F, "					_QUERY(\" where true \");\n");
				for (i = 0; i < OBJ_NSELECTED; i++) {
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						FPRINTF(F, "					_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						FPRINTF(F, "					_QUERY(new_%s);\n", OBJ_SELECTED_COL(i)->varName);
						FPRINTF(F, "					_QUERY(\"%s \");\n", quote);
						FREE(ctype);
					}
				}
				FPRINTF(F, "					EXECUTE_QUERY;\n");
				FPRINTF(F, "				}\n");
				FPRINTF(F, "			}\n");
				FPRINTF(F, "			}\n");
				FPRINTF(F, "		}\n");
				FPRINTF(F, "	}\n");
				FPRINTF(F, "	END;\n");
				FPRINTF(F, "}\n\n");

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
				FPRINTF(F, "TRIGGER(%s) {\n", triggerName);
				FREE(triggerName);
				FPRINTF(F, "//OBJ_KEY_IN_G_CHECK(j) && !OBJ_AGG_INVOLVED_CHECK(j)\n");

				// Old and new rows' data
				FPRINTF(F, "	// Old and new rows' data\n", triggerName);
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *varname = OBJ_TABLE(j)->cols[i]->varName;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					FPRINTF(F, "	%s old_%s;\n", ctype, varname);
					if (STR_INEQUAL(ctype, "char *")) {
						if (STR_EQUAL(ctype, "Numeric"))
							FPRINTF(F, "	char * old_str_%s;\n", varname);
						else
							FPRINTF(F, "	char old_str_%s[20];\n", varname);
					}

					FPRINTF(F, "	%s new_%s;\n", ctype, varname);
					if (STR_INEQUAL(ctype, "char *")) {
						if (STR_EQUAL(ctype, "Numeric"))
							FPRINTF(F, "	char * new_str_%s;\n", varname);
						else
							FPRINTF(F, "	char new_str_%s[20];\n", varname);
					}

					FREE(ctype);


				}

				// Begin
				FPRINTF(F, "\n	BEGIN;\n\n");

				// Get data of old rows
				FPRINTF(F, "	// Get data of old rows\n");
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
					FPRINTF(F, "	%s(old_%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						FPRINTF(F, "	%s(old_%s, old_str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);

					FREE(ctype);
				}

				// Get data of new rows
				FPRINTF(F, "	// Get data of new rows\n");
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
					FPRINTF(F, "	%s(new_%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						FPRINTF(F, "	%s(new_%s, new_str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
					FREE(ctype);
				}

				// FCC
				FPRINTF(F, "\n	// FCC\n");
				//DMT: Tran Trong Nhan's old code
				//FPRINTF(F, "	if (%s) {\n", OBJ_FCC(j));
				//DMT: Do Minh Tuan's new code
				/*
				Because according to the A2: KEY_IN_G(j) && !AGG(j), for update:
				B1: Filter from dTi_old records that dont meet FCC
				dTi_old, -> OBJ_FCC_OLD is the solution
				*/
				/* DELETE FROM MV IF FCC_OLD IS TRUE AND FCC_NEW IS FALSE */
				FPRINTF(F, "	if ((%s) && !(%s)) {\n", OBJ_FCC_OLD(j), OBJ_FCC_NEW(j));
				FPRINTF(F, "		DEFQ(\"delete from %s where true \");\n", mvName);
				for (i = 0; i < OBJ_NSELECTED; i++) {
					if (OBJ_SELECTED_COL(i)->context == COLUMN_CONTEXT_FREE
						&& OBJ_SELECTED_COL(i)->isPrimaryColumn
						&& OBJ_SELECTED_COL(i)->table == OBJ_TABLE(j)) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
						else { quote = ""; strprefix = "str_"; }

						FPRINTF(F, "		_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						FPRINTF(F, "		_QUERY(old_%s%s_table);\n", strprefix, OBJ_SELECTED_COL(i)->varName);
						FPRINTF(F, "		_QUERY(\"%s \");\n", quote);
						FREE(ctype);
					}
				}
				FPRINTF(F, "		EXECUTE_QUERY;\n");
				FPRINTF(F, "	}\n");
				
				/* INSERT INTO MV IF FCC_OLD IS FALSE AND FCC_NEW IS TRUE */
				FPRINTF(F, "	if (!(%s) && (%s)) {\n", OBJ_FCC_OLD(j), OBJ_FCC_NEW(j));

				// Variable declaration
				FPRINTF(F, "		// MV's data\n");
				for (i = 0; i < OBJ_NSELECTED; i++) {
					char *varname = OBJ_SELECTED_COL(i)->varName;
					char *tableName = "<expression>";
					if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
					FPRINTF(F, "		char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
				}

				// Table's vars
				FPRINTF(F, "	// Table's data\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *varname = OBJ_TABLE(j)->cols[i]->varName;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					FPRINTF(F, "		%s %s;\n", ctype, varname);
					if (STR_INEQUAL(ctype, "char *")) {
						if (STR_EQUAL(ctype, "Numeric"))
							FPRINTF(F, "		char * str_%s;\n", varname);
						else
							FPRINTF(F, "		char str_%s[20];\n", varname);
					}
					FREE(ctype);
				}
				// Re-query
				FPRINTF(F, "\n		// Re-query\n");
				FPRINTF(F, "		DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
				FPRINTF(F, "		ADDQ(\" from \");\n");
				FPRINTF(F, "		ADDQ(\"%s\");\n", OBJ_SQ->from);
				FPRINTF(F, "		ADDQ(\" where true \");\n");
				if (OBJ_SQ->hasWhere) FPRINTF(F, "		ADDQ(\" and %s\");\n", OBJ_SQ->where);
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++)
					if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
						else { quote = ""; strprefix = "str_"; }
						FPRINTF(F, "		ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
						FPRINTF(F, "		ADDQ(new_%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
						FPRINTF(F, "		ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}
				if (OBJ_SQ->hasGroupby) FPRINTF(F, "		ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
				if (OBJ_SQ->hasHaving) FPRINTF(F, "		ADDQ(\" having %s\");\n", OBJ_SQ->having);

				FPRINTF(F, "		EXEC;\n");

				// Allocate cache
				FPRINTF(F, "\n		// Allocate cache\n		DEFR(%d);\n", OBJ_NSELECTED);

				// Save dQ result
				FPRINTF(F, "\n		// Save dQ result\n		FOR_EACH_RESULT_ROW(i) {\n");

				for (i = 0; i < OBJ_NSELECTED; i++) {
					FPRINTF(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
					FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
				}

				FPRINTF(F, "		}\n");

				// For each dQi:
				FPRINTF(F, "\n	// For each dQi:\n	FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

				k = 0;
				for (i = 0; i < OBJ_NSELECTED; i++)
					FPRINTF(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

				FPRINTF(F, "			// insert new row to mv\n");
				FPRINTF(F, "			DEFQ(\"insert into %s values (\");\n", mvName);

				for (i = 0; i < OBJ_NSELECTED; i++) {
					char *quote;
					char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
					if (STR_EQUAL(ctype, "char *"))
						quote = "'";
					else quote = "";
					FPRINTF(F, "			ADDQ(\"%s\");\n", quote);
					FPRINTF(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
					FPRINTF(F, "			ADDQ(\"%s\");\n", quote);
					if (i != OBJ_NSELECTED - 1) FPRINTF(F, "			ADDQ(\", \");\n");
					FREE(ctype);
				}

				FPRINTF(F, "			ADDQ(\")\");\n");
				FPRINTF(F, "			EXEC;\n");
				FPRINTF(F, "		}\n");
				FPRINTF(F, "	}\n");
				/* UPDATE THE MV IF FCC_OLD IS TRUE AND FCC NEW IS TRUE */
				FPRINTF(F, "	if ((%s) && (%s)) {\n", OBJ_FCC_OLD(j), OBJ_FCC_NEW(j));

				FPRINTF(F, "		QUERY(\"update %s set \");\n", mvName);
				check = FALSE;
				for (i = 0; i < OBJ_NSELECTED; i++) {
					if (OBJ_SELECTED_COL(i)->context == COLUMN_CONTEXT_FREE
						&& OBJ_SELECTED_COL(i)->table == OBJ_TABLE(j)) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
						else { quote = ""; strprefix = "str_"; }

						if (check) FPRINTF(F, "		_QUERY(\", \");\n");
						check = TRUE;

						FPRINTF(F, "		_QUERY(\" %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						FPRINTF(F, "		_QUERY(new_%s%s_table);\n", strprefix, OBJ_SELECTED_COL(i)->varName);
						FPRINTF(F, "		_QUERY(\"%s \");\n", quote);
						FREE(ctype);
					}
				}
				FPRINTF(F, "		_QUERY(\" where true \");\n");
				for (i = 0; i < OBJ_NSELECTED; i++) {
					if (OBJ_SELECTED_COL(i)->context == COLUMN_CONTEXT_FREE
						&& OBJ_SELECTED_COL(i)->isPrimaryColumn
						&& OBJ_SELECTED_COL(i)->table == OBJ_TABLE(j)) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
						else { quote = ""; strprefix = "str_"; }

						FPRINTF(F, "		_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						FPRINTF(F, "		_QUERY(old_%s%s_table);\n", strprefix, OBJ_SELECTED_COL(i)->varName);
						FPRINTF(F, "		_QUERY(\"%s \");\n", quote);
						FREE(ctype);
					}
				}
				FPRINTF(F, "		EXECUTE_QUERY;\n");
				FPRINTF(F, "	}\n");
				FPRINTF(F, "	END;\n");
				FPRINTF(F, "}\n\n");
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
				FPRINTF(F, "TRIGGER(%s) {\n", triggerName);
				FREE(triggerName);
				FPRINTF(F, "//All the rest\n\n");
				// Variable declaration
				FPRINTF(F, "	/*\n		Variables declaration\n	*/\n");
				// MV's vars
				FPRINTF(F, "	// MV's vars\n");
				for (i = 0; i < OBJ_NSELECTED; i++) {
					char *varname = OBJ_SELECTED_COL(i)->varName;
					char *tableName = "<expression>";
					if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
					FPRINTF(F, "	char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
				}

				// Table's vars
				FPRINTF(F, "	// %s table's vars\n", OBJ_TABLE(j)->name);
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *varname = OBJ_TABLE(j)->cols[i]->varName;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					FPRINTF(F, "	%s %s;\n", ctype, varname);
					if (STR_INEQUAL(ctype, "char *")) {
						if (STR_EQUAL(ctype, "Numeric"))
							FPRINTF(F, "	char * str_%s;\n", varname);
						else
							FPRINTF(F, "	char str_%s[20];\n", varname);
					}
					FREE(ctype);
				}

				FPRINTF(F, "\n	char * count;\n");

				// Standard trigger preparing procedures
				FPRINTF(F, "\n	/*\n		Standard trigger preparing procedures\n	*/\n	REQUIRED_PROCEDURES;\n\n");

				// DELETE OLD
				FPRINTF(F, "	if (UTRIGGER_FIRED_BEFORE) {\n		/* DELETE OLD*/\n");

				// Get values of table's fields
				FPRINTF(F, "		// Get values of table's fields\n");
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
					FPRINTF(F, "		%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						FPRINTF(F, "		%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
					FREE(ctype)
				}

				// FCC
				FPRINTF(F, "\n		// FCC\n");
				FPRINTF(F, "		if (%s) {\n", OBJ_FCC(j));
				//puts(OBJ_TABLE(j)->name);
				//puts(OBJ_FCC(j));
				/*
				TRIGGER BODY START HERE
				*/
				// Re-query
				FPRINTF(F, "\n			// Re-query\n");
				FPRINTF(F, "			DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
				FPRINTF(F, "			ADDQ(\" from \");\n");
				FPRINTF(F, "			ADDQ(\"%s\");\n", OBJ_SQ->from);
				FPRINTF(F, "			ADDQ(\" where true \");\n");
				if (OBJ_SQ->hasWhere) FPRINTF(F, "			ADDQ(\" and %s\");\n", OBJ_SQ->where);
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++)
					if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
						else { quote = ""; strprefix = "str_"; }
						FPRINTF(F, "			ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
						FPRINTF(F, "			ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
						FPRINTF(F, "			ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}
				if (OBJ_SQ->hasGroupby) FPRINTF(F, "			ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
				if (OBJ_SQ->hasHaving) FPRINTF(F, "			ADDQ(\" having %s\");\n", OBJ_SQ->having);

				FPRINTF(F, "			EXEC;\n");

				// Allocate SAVED_RESULT set
				FPRINTF(F, "\n			// Allocate SAVED_RESULT set\n			DEFR(%d);\n", OBJ_NSELECTED);

				// Save dQ result
				FPRINTF(F, "\n			// Save dQ result\n			FOR_EACH_RESULT_ROW(i) {\n");

				for (i = 0; i < OBJ_NSELECTED; i++) {
					FPRINTF(F, "				GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
					FPRINTF(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
				}

				FPRINTF(F, "			}\n");

				// For each dQi:
				FPRINTF(F, "\n			// For each dQi:\n			FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

				k = 0;
				for (i = 0; i < OBJ_NSELECTED; i++)
					FPRINTF(F, "				%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

				// Check if there is a similar row in MV
				FPRINTF(F, "\n				// Check if there is a similar row in MV\n");
				FPRINTF(F, "				DEFQ(\"select mvcount from %s where true \");\n", mvName);
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						FPRINTF(F, "				ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						FPRINTF(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						FPRINTF(F, "				ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}

				FPRINTF(F, "				EXEC;\n");

				FPRINTF(F, "\n				GET_STR_ON_RESULT(count, 0, 1);\n");

				FPRINTF(F, "\n				if (STR_EQUAL(count, mvcount)) {\n");
				FPRINTF(F, "					// Delete the row\n");
				FPRINTF(F, "					DEFQ(\"delete from %s where true\");\n", mvName);

				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						FPRINTF(F, "					ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						FPRINTF(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						FPRINTF(F, "					ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}

				FPRINTF(F, "					EXEC;\n");

				FPRINTF(F, "				} else {\n");

				FPRINTF(F, "					// ow, decrease the count column \n");
				FPRINTF(F, "					DEFQ(\"update %s set \");\n", mvName);

				check = FALSE;
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {

						if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_SUM)) {
							if (check) FPRINTF(F, "					ADDQ(\", \");\n");
							check = TRUE;

							FPRINTF(F, "					ADDQ(\" %s = %s - \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
						else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_COUNT)) {
							if (check) FPRINTF(F, "					ADDQ(\", \");\n");
							check = TRUE;

							FPRINTF(F, "					ADDQ(\" %s = %s - \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
					}
				FPRINTF(F, "					ADDQ(\" where true \");\n");
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						FPRINTF(F, "					ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						FPRINTF(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						FPRINTF(F, "					ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}
				FPRINTF(F, "					EXEC;\n");

				FPRINTF(F, "				}\n");
				FPRINTF(F, "			}\n");

				// End of fcc
				FPRINTF(F, "		}\n");

				// Check time else: after: do insert new row
				FPRINTF(F, "\n	} else {\n");

				if (OBJ_STATIC_BOOL_AGG_MINMAX) {
					// DELETE PART AFTER OF UPDATE
					FPRINTF(F, "\n		/* DELETE */\n		// Get values of table's fields\n");
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
						FPRINTF(F, "		%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
						if (STR_INEQUAL(ctype, "char *"))
							FPRINTF(F, "		%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
						FREE(ctype);
					}

					// FCC
					FPRINTF(F, "\n		// FCC\n");
					FPRINTF(F, "		if (%s) {\n", OBJ_FCC(j));

					// Re-query
					FPRINTF(F, "\n			// Re-query\n");
					FPRINTF(F, "			DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
					FPRINTF(F, "			ADDQ(\" from \");\n");
					FPRINTF(F, "			ADDQ(\"%s\");\n", OBJ_SQ->from);
					FPRINTF(F, "			ADDQ(\" where true \");\n");
					if (OBJ_SQ->hasWhere) FPRINTF(F, "			ADDQ(\" and %s\");\n", OBJ_SQ->where);
					for (i = 0; i < OBJ_TG_NCOLS(j); i++) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_TG_COL(j, i)->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
						else { quote = ""; strprefix = "str_"; }
						FPRINTF(F, "			ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TG_COL(j, i)->name, quote);
						FPRINTF(F, "			ADDQ(%s%s);\n", strprefix, OBJ_TG_COL(j, i)->varName);
						FPRINTF(F, "			ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}
					if (OBJ_SQ->hasGroupby) FPRINTF(F, "			ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
					if (OBJ_SQ->hasHaving) FPRINTF(F, "				ADDQ(\" having %s\");\n", OBJ_SQ->having);

					FPRINTF(F, "			EXEC;\n");

					FPRINTF(F, "\n			DEFR(%d);\n", OBJ_NSELECTED);

					FPRINTF(F, "\n			FOR_EACH_RESULT_ROW(i) {\n");

					for (i = 0; i < OBJ_NSELECTED; i++) {
						FPRINTF(F, "				GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
						FPRINTF(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
					}

					FPRINTF(F, "			}\n");

					// For each dQi:
					FPRINTF(F, "\n			// For each dQi:\n			FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

					k = 0;
					for (i = 0; i < OBJ_NSELECTED; i++)
						FPRINTF(F, "				%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

					FPRINTF(F, "				DEFQ(\"update %s set mvcount = mvcount \");\n", mvName);

					for (i = 0; i < OBJ_NSELECTED; i++) {
						if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_MIN) || STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_MAX_)) {
							FPRINTF(F, "				ADDQ(\", %s = \");\n", OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
					}

					FPRINTF(F, "				ADDQ(\" where true \");\n");
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							FPRINTF(F, "				ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							FPRINTF(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "				ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}

					FPRINTF(F, "				EXEC;\n");
					FPRINTF(F, "			}\n");
					FPRINTF(F, "\n		}\n");
				}

				// INSERT PART AFTER OF UPDATE
				// Get values of table's fields
				FPRINTF(F, "\n		/* INSERT */\n		// Get values of table's fields\n");
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
					FPRINTF(F, "		%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						FPRINTF(F, "		%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
					FREE(ctype);
				}

				// FCC
				FPRINTF(F, "\n		// FCC\n");
				FPRINTF(F, "		if (%s) {\n", OBJ_FCC(j));

				/*
				TRIGGER BODY START HERE
				*/
				// Re-query
				FPRINTF(F, "\n			// Re-query\n");
				FPRINTF(F, "			DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
				FPRINTF(F, "			ADDQ(\" from \");\n");
				FPRINTF(F, "			ADDQ(\"%s\");\n", OBJ_SQ->from);
				FPRINTF(F, "			ADDQ(\" where true \");\n");
				if (OBJ_SQ->hasWhere) FPRINTF(F, "			ADDQ(\" and %s\");\n", OBJ_SQ->where);
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++)
					if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
						else { quote = ""; strprefix = "str_"; }
						FPRINTF(F, "			ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
						FPRINTF(F, "			ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
						FPRINTF(F, "			ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}
				if (OBJ_SQ->hasGroupby) FPRINTF(F, "			ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
				if (OBJ_SQ->hasHaving) FPRINTF(F, "			ADDQ(\" having %s\");\n", OBJ_SQ->having);

				FPRINTF(F, "			EXEC;\n");

				// Allocate SAVED_RESULT set
				FPRINTF(F, "\n			// Allocate SAVED_RESULT set\n			DEFR(%d);\n", OBJ_NSELECTED);

				// Save dQ result
				FPRINTF(F, "\n			// Save dQ result\n			FOR_EACH_RESULT_ROW(i) {\n");

				for (i = 0; i < OBJ_NSELECTED; i++) {
					FPRINTF(F, "				GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
					FPRINTF(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
				}

				FPRINTF(F, "			}\n");

				// For each dQi:
				FPRINTF(F, "\n			// For each dQi:\n			FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

				k = 0;
				for (i = 0; i < OBJ_NSELECTED; i++)
					FPRINTF(F, "				%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

				// Check if there is a similar row in MV
				FPRINTF(F, "\n				// Check if there is a similar row in MV\n");
				FPRINTF(F, "				DEFQ(\"select mvcount from %s where true \");\n", mvName);
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						FPRINTF(F, "				ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						FPRINTF(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						FPRINTF(F, "				ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}

				FPRINTF(F, "				EXEC;\n");

				FPRINTF(F, "\n				if (NO_ROW) {\n");
				FPRINTF(F, "					// insert new row to mv\n");
				FPRINTF(F, "					DEFQ(\"insert into %s values (\");\n", mvName);

				for (i = 0; i < OBJ_NSELECTED; i++) {
					char *quote;
					char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
					if (STR_EQUAL(ctype, "char *"))
						quote = "'";
					else quote = "";
					FPRINTF(F, "					ADDQ(\"%s\");\n", quote);
					FPRINTF(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
					FPRINTF(F, "					ADDQ(\"%s\");\n", quote);
					if (i != OBJ_NSELECTED - 1) FPRINTF(F, "					ADDQ(\", \");\n");
					FREE(ctype);
				}

				FPRINTF(F, "					ADDQ(\")\");\n");
				FPRINTF(F, "					EXEC;\n");

				FPRINTF(F, "				} else {\n");

				// Update agg func fields
				FPRINTF(F, "					// update old row\n");
				FPRINTF(F, "					DEFQ(\"update %s set \");\n", mvName);

				check = FALSE;
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_SUM)) {
							if (check) FPRINTF(F, "					ADDQ(\", \");\n");
							check = TRUE;

							FPRINTF(F, "					ADDQ(\" %s = %s + \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
						else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_COUNT)) {
							if (check) FPRINTF(F, "					ADDQ(\", \");\n");
							check = TRUE;

							FPRINTF(F, "					ADDQ(\" %s = %s + \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
						else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_MIN)) {
							if (check) FPRINTF(F, "					ADDQ(\", \");\n");
							check = TRUE;

							FPRINTF(F, "					ADDQ(\" %s = (case when %s > \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "					ADDQ(\" then \");\n");
							FPRINTF(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "					ADDQ(\" else %s end) \");\n", OBJ_SELECTED_COL(i)->varName);
						}
						else {
							// max
							if (check) FPRINTF(F, "					ADDQ(\", \");\n");
							check = TRUE;

							FPRINTF(F, "					ADDQ(\" %s = (case when %s < \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "					ADDQ(\" then \");\n");
							FPRINTF(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "					ADDQ(\" else %s end) \");\n", OBJ_SELECTED_COL(i)->varName);
						}
					}
				FPRINTF(F, "					ADDQ(\" where true \");\n");
				for (i = 0; i < OBJ_NSELECTED; i++)
					if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						FPRINTF(F, "					ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
						FPRINTF(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						FPRINTF(F, "					ADDQ(\"%s \");\n", quote);
						FREE(ctype);
					}
				FPRINTF(F, "					EXEC;\n");

				FPRINTF(F, "				}\n");
				FPRINTF(F, "			}\n");
				FPRINTF(F, "		}\n");

				// End of check time 
				FPRINTF(F, "\n	}");

				// End of end trigger
				FPRINTF(F, "\n\n	/*\n		Finish\n	*/\n	END;\n}\n\n");
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
	{
		if (OBJ_SQ->hasWhere) {
			//Kiểm tra các bảng trong mệnh đề FROM có cột nào xuất hiện trong mệnh đề WHERE hay không
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
		FPRINTF(SQL, "create table %s (\n", mvName);

		//Loop to OBJ_NSELECTED - 1, because mvcount column is not neccessary in Outer join | Với outer join hiện tại chưa quan tâm đến mvcount (count(*))
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
				FPRINTF(SQL, "	%s %s%s", OBJ_SELECTED_COL(i)->name, OBJ_SELECTED_COL(i)->type, characterLength);
			else
				FPRINTF(SQL, "	%s %s%s", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->type, characterLength);

			if (i != OBJ_NSELECTED - 2)
				FPRINTF(SQL, ",\n");
			FREE(characterLength);
			FREE(ctype);
		}
		FPRINTF(SQL, "\n);");

		for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
			// insert
			FPRINTF(SQL, "\n\n/* insert on %s*/", OBJ_TABLE(j)->name);
			FPRINTF(SQL, "\ncreate function pmvtg_%s_insert_on_%s() returns trigger as '%s', 'pmvtg_%s_insert_on_%s' language c strict;",
					mvName, OBJ_TABLE(j)->name, dllFile, mvName, OBJ_TABLE(j)->name);

			FPRINTF(SQL, "\ncreate trigger pgtg_%s_insert_on_%s after insert on %s for each row execute procedure pmvtg_%s_insert_on_%s();",
					mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);


			// delete
			FPRINTF(SQL, "\n\n/* delete on %s*/", OBJ_TABLE(j)->name);
			FPRINTF(SQL, "\ncreate function pmvtg_%s_delete_on_%s() returns trigger as '%s', 'pmvtg_%s_delete_on_%s' language c strict;",
				mvName, OBJ_TABLE(j)->name, dllFile, mvName, OBJ_TABLE(j)->name);

			FPRINTF(SQL, "\ncreate trigger pgtg_%s_before_delete_on_%s before delete on %s for each row execute procedure pmvtg_%s_delete_on_%s();",
				mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);

			if (!OBJ_KEY_IN_G_CHECK(j) && OBJ_STATIC_BOOL_AGG_MINMAX)
				FPRINTF(SQL, "\ncreate trigger pgtg_%s_after_delete_on_%s after delete on %s for each row execute procedure pmvtg_%s_delete_on_%s();",
					mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);

			// update
			FPRINTF(SQL, "\n\n/* update on %s*/", OBJ_TABLE(j)->name);
			FPRINTF(SQL, "\ncreate function pmvtg_%s_update_on_%s() returns trigger as '%s', 'pmvtg_%s_update_on_%s' language c strict;",
				mvName, OBJ_TABLE(j)->name, dllFile, mvName, OBJ_TABLE(j)->name);

			FPRINTF(SQL, "\ncreate trigger pgtg_%s_before_update_on_%s before update on %s for each row execute procedure pmvtg_%s_update_on_%s();",
					mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);

			FPRINTF(SQL, "\ncreate trigger pgtg_%s_after_update_on_%s after update on %s for each row execute procedure pmvtg_%s_update_on_%s();",
				mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);
		}
		fclose(sqlWriter);
	} // --- END OF GENERATING MV'S SQL CODE

	{ // GENERATE C TRIGGER CODE
	  
		FILE *cWriter = fopen(triggerFile, "w");
		FPRINTF(F, "#include \"ctrigger.h\"\n\n");
		FPRINTF(F, "/*Please use Ctrl (hold) + K + D and release to format the source code if you are using Visual Studio 2010 + */\n\n");
		//DETERMINING TYPE OF TABLE (main or complement), INIT VAR

		for (int j = 0; j < OBJ_SQ->fromElementsNum; j++) {
			for (int k = 0; k < OBJ_NSELECTED - 1; k++) {
				if (STR_EQUAL_CI(OBJ_SELECTED_COL(k)->table->name, OBJ_TABLE(j)->name)) {
					OBJ_TABLE(j)->colsInSelectNum++;
				}
			}
		}
		

		//GEN CODE FOR EACH TABLE 
		for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
			for (int i = 0; i < 2; i++) {
				initGroup(&OBJ_GROUP(i));
			}
			//determine the current table belong to which group | Xác định bảng hiện tại thuộc về nhóm nào
			int groupIn;
			int groupOut;
			Boolean hasGroupOut;
			//Analyzed FROM CLAUSE and save tables To Group | Phân tích mệnh đề FORM và đánh dấu bảng nào thuộc nhóm nào
			{
				hasGroupOut = TRUE;
				//if the last table
				//the last table always belong to the last group (group 1) | bảng cuối cùng luôn thuộc về group 1 bất kể các phép nối trước đó
				if (j == OBJ_SQ->fromElementsNum - 1) {
					groupIn = 1;
					groupOut = 0;
					OBJ_GROUP(groupIn)->tablesNum = 1;
					OBJ_GROUP(groupIn)->tables[0] = OBJ_TABLE(j);
					OBJ_GROUP(groupIn)->joiningTypesNum = 0;
					OBJ_GROUP(groupIn)->joiningConditionsNum = 0;
					OBJ_SQ->groupJoinType = OBJ_SQ->joiningTypesElements[OBJ_SQ->joiningTypesNums - 1];
					OBJ_SQ->groupJoinCondition = OBJ_SQ->joiningConditionsElements[OBJ_SQ->joiningConditionsNums - 1];
					for (int i = 0; i < OBJ_SQ->fromElementsNum - 1; i++) {
						OBJ_GROUP(groupOut)->tables[OBJ_GROUP(groupOut)->tablesNum++] = OBJ_TABLE(i);
						if (i != OBJ_SQ->fromElementsNum - 2) {
							OBJ_GROUP(groupOut)->JoiningTypes[OBJ_GROUP(groupOut)->joiningTypesNum++] = OBJ_SQ->joiningTypesElements[i];
							OBJ_GROUP(groupOut)->JoiningConditions[OBJ_GROUP(groupOut)->joiningConditionsNum++] = OBJ_SQ->joiningConditionsElements[i];
						}
					}
				}
				else {
					//determine the group of table based on the index of last inner join found | xác định nhóm cho table j dựa vào vị trí inner join cuối cùng
					groupIn = 0;
					groupOut = 1;
					int nth_tablesOfGroup = 1;
					if (j <= OBJ_SQ->lastInnerJoinIndex + 1) {
						nth_tablesOfGroup = (OBJ_SQ->lastInnerJoinIndex + 1) + 1;
					}
					else {
						nth_tablesOfGroup = j + 1;
					}
					OBJ_GROUP(groupIn)->tablesNum = nth_tablesOfGroup;
					for (int i = 0; i < nth_tablesOfGroup; i++) {
						OBJ_GROUP(groupIn)->tables[i] = OBJ_TABLE(i);
					}
					OBJ_GROUP(groupIn)->joiningTypesNum = nth_tablesOfGroup - 1;
					OBJ_GROUP(groupIn)->joiningConditionsNum = nth_tablesOfGroup - 1;
					for (int i = 0; i < OBJ_GROUP(groupIn)->joiningTypesNum; i++) {
						OBJ_GROUP(groupIn)->JoiningTypes[i] = OBJ_SQ->joiningTypesElements[i];
					}
					for (int i = 0; i < OBJ_GROUP(groupIn)->joiningConditionsNum; i++) {
						OBJ_GROUP(groupIn)->JoiningConditions[i] = OBJ_SQ->joiningConditionsElements[i];
					}
					if(OBJ_GROUP(groupIn)->tablesNum == OBJ_SQ->fromElementsNum){
						OBJ_SQ->groupJoinType = "null";
						OBJ_SQ->groupJoinCondition = "null";
						//in case the group table num == tables num -> there is no groop out detected | Nếu số lượng bảng của groupIn = số lượng bảng gốc -> không có groupOut
						hasGroupOut = FALSE;
					}
					else {
						OBJ_SQ->groupJoinType = OBJ_SQ->joiningTypesElements[nth_tablesOfGroup - 1];
						OBJ_SQ->groupJoinCondition = OBJ_SQ->joiningConditionsElements[nth_tablesOfGroup - 1];
					}

					//Nếu có group out thì phân tích, lưu dữ liệu cho struct Group Out
					if (hasGroupOut) {
						OBJ_GROUP(groupOut)->tablesNum = OBJ_SQ->fromElementsNum - nth_tablesOfGroup;
						for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
							OBJ_GROUP(groupOut)->tables[i] = OBJ_TABLE(nth_tablesOfGroup + i);
							if (OBJ_SQ->joiningTypesElements[nth_tablesOfGroup + i]) {
								OBJ_GROUP(groupOut)->JoiningTypes[OBJ_GROUP(groupOut)->joiningTypesNum++] = OBJ_SQ->joiningTypesElements[nth_tablesOfGroup + i];
							}
							if (OBJ_SQ->joiningConditionsElements[nth_tablesOfGroup + i]) {
								OBJ_GROUP(groupOut)->JoiningConditions[OBJ_GROUP(groupOut)->joiningConditionsNum++] = OBJ_SQ->joiningConditionsElements[nth_tablesOfGroup + i];
							}
						}
					}
				}					
				//Print some information to see what is happening after analyzing input | in ra màn hình theo dõi thông tin phân tích được
				printf("TABLE: %s\n", OBJ_TABLE(j)->name);
				printf("Group in tables: ");
				for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
					printf("%s , ", OBJ_GROUP(groupIn)->tables[i]->name);
				}
				printf("\n");
				printf("Joining types: ");
				for (int i = 0; i < OBJ_GROUP(groupIn)->joiningTypesNum; i++) {
					printf("%s , ", OBJ_GROUP(groupIn)->JoiningTypes[i]);
				}
				printf("\n");
				printf("Joining conditions: ");
				for (int i = 0; i < OBJ_GROUP(groupIn)->joiningConditionsNum; i++) {
					printf("%s , ", OBJ_GROUP(groupIn)->JoiningConditions[i]);
				}
				printf("\n");
				printf("-----------------------------\n");
				printf("Group joining type: %s\n", OBJ_SQ->groupJoinType);
				printf("Group joining condition: %s\n", OBJ_SQ->groupJoinCondition);
				printf("-----------------------------\n");
			}
			
			//determine index of current table in groupIn
			int indexInGroup;
			for (int k = 0; k < OBJ_GROUP(groupIn)->tablesNum; k++) {
				if (STR_EQUAL_CI(OBJ_GROUP(groupIn)->tables[k]->name, OBJ_TABLE(j)->name)) {
					indexInGroup = k;
				}
			}

			//determine role of table in group | xác định vai trò của bảng trong group
			char *roleInGroup = "main";
			if (indexInGroup == 0 && OBJ_GROUP(groupIn)->tablesNum > 1) {
				joinType = copy(OBJ_GROUP(groupIn)->JoiningTypes[0]);
				if (STR_EQUAL_CI(joinType, "LEFT")) {
					roleInGroup = "main";
				}
				else if (STR_EQUAL_CI(joinType, "RIGHT")) {
					roleInGroup = "comp";
				}
				else {
					roleInGroup = "equal";
				}
			}
			else if(OBJ_GROUP(groupIn)->tablesNum > 1){
				joinType = copy(OBJ_GROUP(groupIn)->JoiningTypes[indexInGroup - 1]);
				if (STR_EQUAL_CI(joinType, "LEFT")) {
					roleInGroup = "comp";
				}
				else if (STR_EQUAL_CI(joinType, "RIGHT")) {
					roleInGroup = "main";
				}
				else {
					roleInGroup = "equal";
				}
			}

			//determine role of group in original query | xác định vao trò của group trong toàn bộ TVG
			char *joinType = "";
			if (groupIn == 0) {
				if (STR_EQUAL_CI(OBJ_SQ->groupJoinType, "LEFT OUTER")) {
					OBJ_GROUP(groupIn)->RoleInOriginalQuery = "main";
				}
				else if (STR_EQUAL_CI(OBJ_SQ->groupJoinType, "RIGHT OUTER")) {
					OBJ_GROUP(groupIn)->RoleInOriginalQuery = "comp";
				}
				else if (STR_EQUAL_CI(OBJ_SQ->groupJoinType, "FULL OUTER")) {
					OBJ_GROUP(groupIn)->RoleInOriginalQuery = "equal";
				}
				//inner join
				else {
					OBJ_GROUP(groupIn)->RoleInOriginalQuery = "inner";
				}
			}
			else {
				if (STR_EQUAL_CI(OBJ_SQ->groupJoinType, "LEFT OUTER")) {
					OBJ_GROUP(groupIn)->RoleInOriginalQuery = "comp";
				}
				else if (STR_EQUAL_CI(OBJ_SQ->groupJoinType, "RIGHT OUTER")) {
					OBJ_GROUP(groupIn)->RoleInOriginalQuery = "main";
				}
				else if (STR_EQUAL_CI(OBJ_SQ->groupJoinType, "FULL OUTER")) {
					OBJ_GROUP(groupIn)->RoleInOriginalQuery = "equal";
				}
				//inner join
				else {
					OBJ_GROUP(groupIn)->RoleInOriginalQuery = "inner";
				}
			}

			printf("Role in original query: %s\n", OBJ_GROUP(groupIn)->RoleInOriginalQuery);
			
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
			FPRINTF(F, "TRIGGER(%s) {\n", triggerName);
			FREE(triggerName);

			// Variable declaration
			FPRINTF(F, "	// MV's data\n");
			for (i = 0; i < OBJ_NSELECTED; i++) {
				char *varname = OBJ_SELECTED_COL(i)->varName;
				char *tableName = "<expression>";
				if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
				FPRINTF(F, "	char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
			}

			// Table's vars
			FPRINTF(F, "	// Table's data\n");
			for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
				char *varname = OBJ_TABLE(j)->cols[i]->varName;
				char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
				FPRINTF(F, "	%s %s;\n", ctype, varname);
				if (STR_INEQUAL(ctype, "char *")) {
					if (STR_EQUAL(ctype, "Numeric"))
						FPRINTF(F, "	char * str_%s;\n", varname);
					else
						FPRINTF(F, "	char str_%s[20];\n", varname);
				}
				FREE(ctype);
			}

			// Standard trigger preparing procedures
			FPRINTF(F, "\n	BEGIN;\n\n");

			// Get values of table's fields
			FPRINTF(F, "	// Get table's data\n");
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
				FPRINTF(F, "	%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
				if (STR_INEQUAL(ctype, "char *"))
					FPRINTF(F, "	%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
				FREE(ctype);
			}

			// FCC
			FPRINTF(F, "\n	// FCC\n");
			
			FPRINTF(F, "	if (%s) {\n", OBJ_FCC(j));
			//define group_in_insert
			FPRINTF(F, "		DEFQ(\"CREATE TEMP TABLE group_in_insert AS( SELECT\");\n");
			FPRINTF(F, "		ADDQ(\"");

			//Determine how many col in group_in_select, use to know when to stop printing "," and in DEFR() later
			//Xác định bao nhiêu cột tham gia vào group_in_insert để xác định số lần sinh ra dấu "," trong mã nguồn C sinh ra
			//và để khai báo bộ nhớ cho chính xác (bằng macro DEFR)
			int colInGroupInInsert = 0;

			/*	groupInSelectColsList is the list of cols that appears in original query
				groupInSelectColsListWithJoiningCols add some cols in group joining condition that might not appears in original query
			*/
			char * groupInSelectColsList = "";
			char * groupInSelectColsListWithJoiningCols = "";

			for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
				for (int k = 0; k < OBJ_GROUP(groupIn)->tablesNum; k++) {
					if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_GROUP(groupIn)->tables[k]->name)) {
						colInGroupInInsert++;
					}
				}
			}

			STR_INIT(groupInSelectColsList, " ");

			int counter = 0;
			//loop through group in
			for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
				for (int k = 0; k < OBJ_GROUP(groupIn)->tablesNum; k++) {
					if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_GROUP(groupIn)->tables[k]->name)) {
						counter++;
						STR_APPEND(groupInSelectColsList, OBJ_SELECTED_COL(i)->table->name);
						STR_APPEND(groupInSelectColsList, ".");
						STR_APPEND(groupInSelectColsList, OBJ_SELECTED_COL(i)->name);
						if (counter < colInGroupInInsert) {
							STR_APPEND(groupInSelectColsList, ", ");
						}
					}
				}				
			}
			
			/*analyze to add the column of joining condition if not appears*/
			//This action is neccessary only if OBJ_SQ->groupJoinCondition != 'null'
			STR_INIT(groupInSelectColsListWithJoiningCols, " ");
			STR_APPEND(groupInSelectColsListWithJoiningCols, groupInSelectColsList);
			char * tmp, isFoundInGroupJoin, isFoundInSelectColsList;
			char *groupJoiningCondition;
			char *colPos = "";

			if (STR_INEQUAL_CI(OBJ_SQ->groupJoinCondition, "null")) {
				//loop through cols in table in group in, search for <table>.<col> in joining condition
				//and space before group join to replace correctly
				STR_INIT(groupJoiningCondition, " ");
				STR_APPEND(groupJoiningCondition, OBJ_SQ->groupJoinCondition);
				STR_INIT(tmp, " ");
				STR_APPEND(tmp, OBJ_SQ->groupJoinCondition);
				while (colPos = findSubString(tmp, ".")) {
					char *tbName = getPrecededTableName(tmp, colPos + 1);
					for (int k = 0; k < OBJ_GROUP(groupIn)->tablesNum; k++) {
						if (STR_EQUAL_CI(OBJ_GROUP(groupIn)->tables[k]->name, trim(tbName))) {
							char *findWhat;
							STR_INIT(findWhat, " ");
							STR_APPEND(findWhat, tbName);
							STR_APPEND(findWhat, getColumnName(colPos));
							isFoundInGroupJoin = findSubString(groupJoiningCondition, findWhat);
							//if found
							if (isFoundInGroupJoin) {
								isFoundInSelectColsList = findSubString(groupInSelectColsList, findWhat);
								if (isFoundInSelectColsList == NULL) {
									STR_APPEND(groupInSelectColsListWithJoiningCols, ", ");
									STR_APPEND(groupInSelectColsListWithJoiningCols, findWhat);
								}
							}
						}
					}
					tmp = colPos + 1;
				}
			}	
			//finish analyzing
			FPRINTF(F, "%s", groupInSelectColsListWithJoiningCols);
			FPRINTF(F, "\");\n");
			FPRINTF(F, "		ADDQ(\"");
			FPRINTF(F, " FROM ");
			for(int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++){
				if (i != 0) {
					FPRINTF(F, " %s JOIN ",OBJ_GROUP(groupIn)->JoiningTypes[i - 1]);
				}
				FPRINTF(F, " %s ", OBJ_GROUP(groupIn)->tables[i]->name);
				if (i != 0) {
					FPRINTF(F, " ON %s ", OBJ_GROUP(groupIn)->JoiningConditions[i - 1]);
				}
			}
			FPRINTF(F, "\");\n");
			FPRINTF(F, "		ADDQ(\"WHERE true \");\n");
			for (int i = 0; i < OBJ_TABLE(j)->nCols; i++) {
				if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
					char *quote;
					char *strprefix;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
					else { quote = ""; strprefix = "str_"; }
					FPRINTF(F, "		ADDQ(\" and %s.%s = %s\"); \n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
					FPRINTF(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
					FPRINTF(F, "		ADDQ(\"%s \"); \n", quote);
					FREE(ctype);
				}
			}
			FPRINTF(F, "		ADDQ(\");\");\n");
			
			//--end of defining group_in_insert

			//define group_out
			int colInGroupOut = 0;
			char * groupOutSelectColsList = "";
			char * groupOutSelectColsListWithJoiningCols = "";
			STR_INIT(groupOutSelectColsList, " ");

			if (hasGroupOut) {
				FPRINTF(F, "		ADDQ(\"CREATE TEMP TABLE group_out AS( SELECT\");\n");
				FPRINTF(F, "		ADDQ(\"");

				//Determine how many col in group_out, use to know when to stop printing "," and in DEFR
				for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
					for (int k = 0; k < OBJ_GROUP(groupOut)->tablesNum; k++) {
						if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_GROUP(groupOut)->tables[k]->name)) {
							colInGroupOut++;
						}
					}
				}

				counter = 0;
				//loop through group out
				for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
					for (int k = 0; k < OBJ_GROUP(groupOut)->tablesNum; k++) {
						if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_GROUP(groupOut)->tables[k]->name)) {
							counter++;
							STR_APPEND(groupOutSelectColsList, OBJ_SELECTED_COL(i)->table->name);
							STR_APPEND(groupOutSelectColsList, ".");
							STR_APPEND(groupOutSelectColsList, OBJ_SELECTED_COL(i)->name);
							if (counter < colInGroupOut) {
								STR_APPEND(groupOutSelectColsList, ", ");
							}
							/*FPRINTF(F," %s.%s%s", OBJ_SELECTED_COL(i)->table->name, OBJ_SELECTED_COL(i)->name,
							(counter < colInGroupInInsert) ? ", ":" ");*/
						}
					}
				}

				/*analyze to add the column of joining condition if not appears*/
				STR_INIT(groupOutSelectColsListWithJoiningCols, " ");
				STR_APPEND(groupOutSelectColsListWithJoiningCols, groupOutSelectColsList);
				//loop through cols in table in group in, search for <table>.<col> in joining condition
				//and space before group join to replace correctly
				groupJoiningCondition = "";
				STR_INIT(groupJoiningCondition, " ");
				STR_APPEND(groupJoiningCondition, OBJ_SQ->groupJoinCondition);
				if (STR_INEQUAL_CI(OBJ_SQ->groupJoinCondition, "null")) {
					tmp = "";
					STR_INIT(tmp, " ");
					STR_APPEND(tmp, OBJ_SQ->groupJoinCondition);
					colPos = "";
					while (colPos = findSubString(tmp, ".")) {
						char *tbName = getPrecededTableName(tmp, colPos + 1);
						for (int k = 0; k < OBJ_GROUP(groupOut)->tablesNum; k++) {
							if (STR_EQUAL_CI(OBJ_GROUP(groupOut)->tables[k]->name, trim(tbName))) {
								char *findWhat;
								STR_INIT(findWhat, " ");
								STR_APPEND(findWhat, tbName);
								STR_APPEND(findWhat, getColumnName(colPos));
								isFoundInGroupJoin = findSubString(groupJoiningCondition, findWhat);
								//if found
								if (isFoundInGroupJoin) {
									isFoundInSelectColsList = findSubString(groupOutSelectColsList, findWhat);
									if (isFoundInSelectColsList == NULL) {
										STR_APPEND(groupOutSelectColsListWithJoiningCols, ", ");
										STR_APPEND(groupOutSelectColsListWithJoiningCols, findWhat);
									}
								}
							}
						}
						tmp = colPos + 1;
					}
				}
				//finish analyzing
				FPRINTF(F, "%s", groupOutSelectColsListWithJoiningCols);
				FPRINTF(F, "\");\n");
				FPRINTF(F, "		ADDQ(\"");
				FPRINTF(F, " FROM ");
				for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
					if (i != 0) {
						FPRINTF(F, " %s JOIN ", OBJ_GROUP(groupOut)->JoiningTypes[i - 1]);
					}
					FPRINTF(F, " %s ", OBJ_GROUP(groupOut)->tables[i]->name);
					if (i != 0) {
						FPRINTF(F, " ON %s ", OBJ_GROUP(groupOut)->JoiningConditions[i - 1]);
					}
				}
				FPRINTF(F, "\");\n");
				FPRINTF(F, "		ADDQ(\");\");\n");

				//end of defining group out
			}

			//define mv_insert
			char *analyzedGroupJoinCondition;
			//and space before group join to replace correctly
			STR_INIT(analyzedGroupJoinCondition, " ");
			STR_APPEND(analyzedGroupJoinCondition, OBJ_SQ->groupJoinCondition);
			/*Group joining condition is in the format: <table of group 1>.col_x = <table of group 2>.col_y
				But we cant know which table belongs to which group
				-> need to analyzed to know which table is in which group and replace the table name by group name
				ex: group joining condition = 't2.c4 = t3.c5' , t2 in group 0 (currently is group in), t3 in group 1 (currently is group out)
				-> after analyzed: analyzedGroupJoinCondition = group_in_insert.c4 = group_out.c5
			*/
			tmp = " ";
			//and space before group join to use the getPrecededTableName
			STR_INIT(tmp, " ");
			STR_APPEND(tmp, OBJ_SQ->groupJoinCondition);

			colPos = "";
			while (colPos = findSubString(tmp, ".")) {
				char *tbName = getPrecededTableName(tmp, colPos + 1);
				//determine group of tbName
				for (int k = 0; k < OBJ_GROUP(groupIn)->tablesNum; k++) {
					if (STR_EQUAL_CI(OBJ_GROUP(groupIn)->tables[k]->name, trim(tbName))) {
						char *findWhat;
						STR_INIT(findWhat, " ");
						STR_APPEND(findWhat, tbName);
						STR_APPEND(findWhat, ".");
						char *replaceWith = " group_in_insert.";
						analyzedGroupJoinCondition = dammf_replaceAll(analyzedGroupJoinCondition, findWhat, replaceWith);
						//printf("analyzedGroupJoinCondition = %s\n", analyzedGroupJoinCondition);
					}
				}
				for (int k = 0; k < OBJ_GROUP(groupOut)->tablesNum; k++) {
					if (STR_EQUAL_CI(OBJ_GROUP(groupOut)->tables[k]->name, trim(tbName))) {
						char *findWhat;
						STR_INIT(findWhat, " ");
						STR_APPEND(findWhat, tbName);
						STR_APPEND(findWhat, ".");
						char *replaceWith = " group_out.";
						analyzedGroupJoinCondition = dammf_replaceAll(analyzedGroupJoinCondition, findWhat, replaceWith);
						//printf("analyzedGroupJoinCondition = %s\n", analyzedGroupJoinCondition);
					}
				}
				tmp = colPos + 1;
			}

			
			int colInMvInsert = OBJ_NSELECTED - 1; // dont count mvcount
			/*we need to replace <table>.<col> in groupOutSelectColsList -> group_in_insert.col*/
			tmp = " ";
			//and space before group join to use the getPrecededTableName
			STR_INIT(tmp, " ");
			STR_APPEND(tmp, groupInSelectColsList);

			colPos = "";
			while (colPos = findSubString(tmp, ".")) {
				char *tbName = getPrecededTableName(tmp, colPos + 1);
				char *findWhat;
				STR_INIT(findWhat, " ");
				STR_APPEND(findWhat, tbName);
				STR_APPEND(findWhat, ".");
				groupInSelectColsList = replace_str(groupInSelectColsList, findWhat, " group_in_insert.");
				tmp = colPos + 1;
			}
			
			tmp = " ";
			STR_INIT(tmp, " ");
			STR_APPEND(tmp, groupOutSelectColsList);

			colPos = "";
			while (colPos = findSubString(tmp, ".")) {
				char *tbName = getPrecededTableName(tmp, colPos + 1);
				char *findWhat;
				STR_INIT(findWhat, " ");
				STR_APPEND(findWhat, tbName);
				STR_APPEND(findWhat, ".");
				groupOutSelectColsList = replace_str(groupOutSelectColsList, findWhat, " group_out.");
				tmp = colPos + 1;
			}
			FPRINTF(F, "		ADDQ(\"CREATE TEMP TABLE mv_insert AS( SELECT\");\n");
			if (hasGroupOut) {
				//because insert required the correct position of column 
				if (groupIn == 0) {
					FPRINTF(F, "		ADDQ(\" %s, %s \");\n", groupInSelectColsList, groupOutSelectColsList);
				}
				else {
					FPRINTF(F, "		ADDQ(\" %s, %s \");\n", groupOutSelectColsList, groupInSelectColsList);
				}
				FPRINTF(F, "		ADDQ(\" \");\n");
				//the order of 2 group in joining condition is also important

				if (groupIn == 0) {
					FPRINTF(F, "		ADDQ(\" FROM group_in_insert %s JOIN group_out ON %s \");\n", OBJ_SQ->groupJoinType, analyzedGroupJoinCondition);
				}
				else {
					FPRINTF(F, "		ADDQ(\" FROM group_out %s JOIN group_in_insert ON %s \");\n", OBJ_SQ->groupJoinType, analyzedGroupJoinCondition);
				}

				FPRINTF(F, "		ADDQ(\"WHERE true \");\n");
				for (int i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
						else { quote = ""; strprefix = "str_"; }
						FPRINTF(F, "		ADDQ(\" and group_in_insert.%s = %s\"); \n", OBJ_TABLE(j)->cols[i]->name, quote);
						FPRINTF(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
						FPRINTF(F, "		ADDQ(\"%s \"); \n", quote);
						FREE(ctype);
					}
				}
			}
			else {
				FPRINTF(F, "		ADDQ(\" %s FROM group_in_insert \");\n", groupInSelectColsList);
			}
			FPRINTF(F, "		ADDQ(\");\");\n");
			//end of defining mv_insert
			//define group_in_delete
			/*	Group in delete: only need when current table is comp or equal in group	*/
			//save the number of column in Group in delete in colInGroupInDelete var to use for DEFR() later
			int	colInGroupInDelete = 0;

			if ((STR_EQUAL_CI(roleInGroup, "comp") || STR_EQUAL_CI(roleInGroup, "equal"))&&!OBJ_SQ->hasJoin) {
				counter = 0;
				//loop through selected col and just care about column of tables in group in, table <> current table
				
				for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
					if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
						//just take primary key 
						for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
							if (OBJ_GROUP(groupIn)->tables[i]->cols[k]->isPrimaryColumn) {
								colInGroupInDelete++;
							}
						}								
					}
				}

				FPRINTF(F, "		ADDQ(\"CREATE TEMP TABLE group_in_delete AS( SELECT DISTINCT \");\n");
				FPRINTF(F, "		ADDQ(\"");

				for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
					if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
						//just take primary key 
						for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
							if (OBJ_GROUP(groupIn)->tables[i]->cols[k]->isPrimaryColumn) {
								counter++;
								FPRINTF(F, " %s%s ",
									OBJ_GROUP(groupIn)->tables[i]->cols[k]->name, (counter < colInGroupInDelete ? "," : " "));
							}
						}
					}
				}
								
				FPRINTF(F, "\");\n");
				FPRINTF(F, "		ADDQ(\" FROM mv_insert \");\n");
				FPRINTF(F, "		ADDQ(\");\");\n");
			}			

			//end of defining group_in_delete

			//define mv_delete
			/* mv_delete is only declared when insert to comp or equal group */
			//CONDITION TO HAVE mv_delete
			int colInMvDelete = 0;
			if (STR_EQUAL_CI(OBJ_GROUP(groupIn)->RoleInOriginalQuery, "comp") 
				|| STR_EQUAL_CI(OBJ_GROUP(groupIn)->RoleInOriginalQuery, "equal")) {
				//determine col in mv delete, use for DEFR() later
				for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
					for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
						if (OBJ_GROUP(groupOut)->tables[i]->cols[k]->isPrimaryColumn) {
							colInMvDelete++;
						}
					}
				}
				counter = 0;
				FPRINTF(F, "		ADDQ(\"CREATE TEMP TABLE mv_delete AS( SELECT DISTINCT \");\n");
				FPRINTF(F, "		ADDQ(\"");
				
				for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
					for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
						if (OBJ_GROUP(groupOut)->tables[i]->cols[k]->isPrimaryColumn) {
							counter++;
							FPRINTF(F, " %s%s ", OBJ_GROUP(groupOut)->tables[i]->cols[k]->name, (counter < colInMvDelete ? "," : " "));
						}
					}
				}

				FPRINTF(F, "\");\n");
				counter = 0;
				FPRINTF(F, "		ADDQ(\" FROM mv_insert WHERE \");\n");
				for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
					for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
						if (OBJ_GROUP(groupOut)->tables[i]->cols[k]->isPrimaryColumn) {
							FPRINTF(F, "		ADDQ(\" %s %s is not null \");\n", (counter > 0 ? "or" : " "), OBJ_GROUP(groupOut)->tables[i]->cols[k]->name);
							counter++;
						}
					}
				}


				FPRINTF(F, "		ADDQ(\");\");\n");
			}

			//end of defining mv_delete
			
			//STEP 1: SELECT FROM MV_INSERT, INSERT INTO MV
			FPRINTF(F, "		ADDQ(\"SELECT * FROM mv_insert; \");\n");
			FPRINTF(F, "		EXEC_;\n");
			FPRINTF(F, "		if(NO_ROW) {\n");
			FPRINTF(F, "			goto end_;\n");
			FPRINTF(F, "		}\n");
			FPRINTF(F, "		DEFR(%d);\n", colInMvInsert);
			//save mv insert to saved result set
			
			FPRINTF(F, "		FOR_EACH_RESULT_ROW(i){\n");
			for (i = 0; i < OBJ_NSELECTED - 1; i++) {
				if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
					FPRINTF(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
					FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
				}
				else {
					FPRINTF(F, "			GET_NULLABLE_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
					FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
				}
			}
			FPRINTF(F, "		}\n");
			
			FPRINTF(F, "\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED - 1);

			k = 0;
			for (i = 0; i < OBJ_NSELECTED - 1; i++)
				FPRINTF(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

			FPRINTF(F, "\n			DEFQ(\"INSERT INTO %s VALUES(\");\n", mvName);
			for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
				char *quote;
				char *ctype;
				ctype = createCType(OBJ_SELECTED_COL(i)->type);
				if (STR_EQUAL(ctype, "char *")) quote = "\'";
				else quote = "";
				if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
					FPRINTF(F, "			ADDQ(\"%s\");\n", quote);
					FPRINTF(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
					FPRINTF(F, "			ADDQ(\"%s\");\n", quote);
				}
				else {
					FPRINTF(F, "			ADDQ_NULL_OR_VALUE(%s, \"%s\");\n", OBJ_SELECTED_COL(i)->varName, quote);
				}
				FREE(ctype);
				if (i < OBJ_NSELECTED - 2) {
					FPRINTF(F, "			ADDQ(\",\");\n");
				}
			}
			FPRINTF(F, "			ADDQ(\");\");\n");
			FPRINTF(F, "\n			EXEC_; \n", OBJ_NSELECTED - 1);
			// end of FOR_EACH_SAVED_RESULT_ROW
			FPRINTF(F, "\n		} // end of FOR_EACH_SAVED_RESULT_ROW \n", OBJ_NSELECTED - 1);
			//END OF STEP 1

			//STEP 2: delete from mv1: if in group, role of current table is comp or equal, delete all records of type(main, main, NULL, NULL)
			//with main is from group_in_delete
			if ((STR_EQUAL_CI(roleInGroup, "comp") || STR_EQUAL_CI(roleInGroup, "equal")) && !OBJ_SQ->hasJoin) {
				FPRINTF(F, "\n		DEFQ(\"SELECT * FROM group_in_delete; \");\n");
				FPRINTF(F, "		EXEC_;\n");

				FPRINTF(F, "		DEFR(%d);\n", colInGroupInDelete);
				
				FPRINTF(F, "		FOR_EACH_RESULT_ROW(i){\n");
				counter = 0;
				for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
					if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
						//just take primary key 
						for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
							if (OBJ_GROUP(groupIn)->tables[i]->cols[k]->isPrimaryColumn) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)) {
										FPRINTF(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(l)->varName, counter + 1);
										FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(l)->varName);
										counter++;
									}
								}								
							}
						}
					}
				}

				FPRINTF(F, "		}\n");

				FPRINTF(F, "\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", colInGroupInDelete);

				counter = 0;
				for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
					if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
						//just take primary key 
						for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
							if (OBJ_GROUP(groupIn)->tables[i]->cols[k]->isPrimaryColumn) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)) {
										FPRINTF(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(l)->varName, counter++);
									}
								}
							}
						}
					}
				}

				FPRINTF(F, "\n			DEFQ(\"DELETE FROM %s WHERE true \");\n", mvName);
				
				for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
					if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
						//just take primary key 
						for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
							if (OBJ_GROUP(groupIn)->tables[i]->cols[k]->isPrimaryColumn) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)) {
										char *quote;
										char *ctype;
										ctype = createCType(OBJ_SELECTED_COL(l)->type);
										if (STR_EQUAL(ctype, "char *")) quote = "\'";
										else quote = "";										
										FPRINTF(F, "			ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(l)->name, quote);
										FPRINTF(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(l)->varName);
										FPRINTF(F, "			ADDQ(\"%s\");\n", quote);										
										FREE(ctype);
									}
								}
							}
						}
					}
				}

				//because primary key of table j is always join in select so dont need to loop through obj_select_col 
				for (int i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
						FPRINTF(F, "			ADDQ(\" and %s is null\");\n", OBJ_TABLE(j)->cols[i]->name);
					}
				}
				FPRINTF(F, "			EXEC_;\n");
				FPRINTF(F, "		}\n");
				
			}
			//END OF STEP 2
			//STEP 3: Delete from mv1 if current group role in query is comp or equal (from mv_delete)
			if (STR_EQUAL_CI(OBJ_GROUP(groupIn)->RoleInOriginalQuery, "comp")
				|| STR_EQUAL_CI(OBJ_GROUP(groupIn)->RoleInOriginalQuery, "equal")) {
				FPRINTF(F, "\n		DEFQ(\"SELECT * FROM mv_delete; \");\n");
				FPRINTF(F, "		EXEC_;\n");
				FPRINTF(F, "		DEFR(%d);\n", colInMvDelete);
				FPRINTF(F, "		FOR_EACH_RESULT_ROW(i){\n");
				counter = 0;
				for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
					//just take primary key 
					for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
						if (OBJ_GROUP(groupOut)->tables[i]->cols[k]->isPrimaryColumn) {
							for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
								if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)) {
									FPRINTF(F, "			GET_NULLABLE_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(l)->varName, counter + 1);
									FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(l)->varName);
									counter++;
								}
							}
						}
					}
				}
				FPRINTF(F, "		}\n");


				FPRINTF(F, "\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", colInMvDelete);

				counter = 0;
				for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
					//just take primary key 
					for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
						if (OBJ_GROUP(groupOut)->tables[i]->cols[k]->isPrimaryColumn) {
							for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
								if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)) {
									FPRINTF(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(l)->varName, counter++);
								}
							}
						}
					}
				}

				FPRINTF(F, "\n			DEFQ(\"DELETE FROM %s WHERE true \");\n", mvName);

				for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
					//just take primary key 
					for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
						if (OBJ_GROUP(groupOut)->tables[i]->cols[k]->isPrimaryColumn) {
							for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
								if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)) {
									char *quote;
									char *ctype;
									ctype = createCType(OBJ_SELECTED_COL(l)->type);
									if (STR_EQUAL(ctype, "char *")) quote = "\'";
									else quote = "";
									FPRINTF(F, "			ADDQ(\" and %s \");\n", OBJ_SELECTED_COL(l)->name);
									FPRINTF(F, "			ADDQ_ISNULL_OR_VALUE(%s, \"%s\", \"=\");\n", OBJ_SELECTED_COL(l)->varName, quote);
									FREE(ctype);
								}
							}
						}
					}					
				}

				//because primary key of table j is always join in select so dont need to loop through obj_select_col 
				for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
					for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
						if (OBJ_GROUP(groupIn)->tables[i]->cols[k]->isPrimaryColumn) {
							FPRINTF(F, "			ADDQ(\" and %s is null\");\n", OBJ_GROUP(groupIn)->tables[i]->cols[k]->name);
						}
					}
				}
				
				FPRINTF(F, "			EXEC_;\n");
				FPRINTF(F, "		}\n");

			}
			//END OF STEP 3

			// End of trigger
			FPRINTF(F, "\n	}\n\n	/*\n		Finish\n	*/\n");
			FPRINTF(F, "	end_: DEFQ(\"DROP TABLE IF EXISTS mv_insert, mv_delete, group_in_insert, group_in_delete, group_out;\"); EXEC_; \n	END; \n}\n\n");

			//END OF GENERATE CODE FOR INSERT EVENT

			 /*******************************************************************************************************
															DELETE EVENT
			 ********************************************************************************************************/
			if (TRUE) {
				
				//re-analyze joining condition
				analyzedGroupJoinCondition = dammf_replaceAll(analyzedGroupJoinCondition, "group_in_insert.", "group_in_delete.");
				
				Boolean check;
				// Define trigger function
				char *triggerName;
				STR_INIT(triggerName, "pmvtg_");
				STR_APPEND(triggerName, mvName);
				STR_APPEND(triggerName, "_delete_on_");
				STR_APPEND(triggerName, OBJ_TABLE(j)->name);
				FPRINTF(F, "TRIGGER(%s) {\n", triggerName);
				FREE(triggerName);

				// Variable declaration
				FPRINTF(F, "	// MV's data\n");
				for (i = 0; i < OBJ_NSELECTED; i++) {
					char *varname = OBJ_SELECTED_COL(i)->varName;
					char *tableName = "<expression>";
					if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
					FPRINTF(F, "	char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
				}

				// Table's vars
				FPRINTF(F, "	// Table's data\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *varname = OBJ_TABLE(j)->cols[i]->varName;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					FPRINTF(F, "	%s %s;\n", ctype, varname);
					if (STR_INEQUAL(ctype, "char *")) {
						if (STR_EQUAL(ctype, "Numeric"))
							FPRINTF(F, "	char * str_%s;\n", varname);
						else
							FPRINTF(F, "	char str_%s[20];\n", varname);
					}
					FREE(ctype);
				}

				// Standard trigger preparing procedures
				FPRINTF(F, "\n	BEGIN;\n\n");

				// Get values of table's fields
				FPRINTF(F, "	// Get table's data\n");
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
					FPRINTF(F, "	%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						FPRINTF(F, "	%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
					FREE(ctype);
				}

				// FCC
				FPRINTF(F, "\n	// FCC\n");
				FPRINTF(F, "	if(%s){\n",  OBJ_FCC(j));
				
				//define group_in_delete
				FPRINTF(F, "		DEFQ(\"CREATE TEMP TABLE group_in_delete AS( SELECT\");\n");
				FPRINTF(F, "		ADDQ(\"");

				//Determine how many col in group_in_delete, use to know when to stop printing "," and in DEFR() later
				colInGroupInDelete = 0;

				/*	groupInSelectColsList is the list of cols that appears in original query
				groupInSelectColsListWithJoiningCols add some cols in group joining condition that might not appears in original query
				*/
				groupInSelectColsList = "";
				groupInSelectColsListWithJoiningCols = "";

				for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
					for (int k = 0; k < OBJ_GROUP(groupIn)->tablesNum; k++) {
						if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_GROUP(groupIn)->tables[k]->name)) {
							colInGroupInDelete++;
						}
					}
				}

				STR_INIT(groupInSelectColsList, " ");

				int counter = 0;
				//loop through group in
				for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
					for (int k = 0; k < OBJ_GROUP(groupIn)->tablesNum; k++) {
						if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_GROUP(groupIn)->tables[k]->name)) {
							counter++;
							STR_APPEND(groupInSelectColsList, OBJ_SELECTED_COL(i)->table->name);
							STR_APPEND(groupInSelectColsList, ".");
							STR_APPEND(groupInSelectColsList, OBJ_SELECTED_COL(i)->name);
							if (counter < colInGroupInInsert) {
								STR_APPEND(groupInSelectColsList, ", ");
							}
							/*FPRINTF(F," %s.%s%s", OBJ_SELECTED_COL(i)->table->name, OBJ_SELECTED_COL(i)->name,
							(counter < colInGroupInInsert) ? ", ":" ");*/
						}
					}
				}

				/*analyze to add the column of joining condition if not appears*/
				STR_INIT(groupInSelectColsListWithJoiningCols, " ");
				STR_APPEND(groupInSelectColsListWithJoiningCols, groupInSelectColsList);
				//loop through cols in table in group in, search for <table>.<col> in joining condition
				char * tmp, isFoundInGroupJoin, isFoundInSelectColsList;
				char *groupJoiningCondition;
				//and space before group join to replace correctly
				STR_INIT(groupJoiningCondition, " ");
				STR_APPEND(groupJoiningCondition, OBJ_SQ->groupJoinCondition);
				STR_INIT(tmp, " ");
				STR_APPEND(tmp, OBJ_SQ->groupJoinCondition);
				char *colPos = "";
				while (colPos = findSubString(tmp, ".")) {
					char *tbName = getPrecededTableName(tmp, colPos + 1);
					for (int k = 0; k < OBJ_GROUP(groupIn)->tablesNum; k++) {
						if (STR_EQUAL_CI(OBJ_GROUP(groupIn)->tables[k]->name, trim(tbName))) {
							char *findWhat;
							STR_INIT(findWhat, " ");
							STR_APPEND(findWhat, tbName);
							STR_APPEND(findWhat, getColumnName(colPos));
							isFoundInGroupJoin = findSubString(groupJoiningCondition, findWhat);
							//if found
							if (isFoundInGroupJoin) {
								isFoundInSelectColsList = findSubString(groupInSelectColsList, findWhat);
								if (isFoundInSelectColsList == NULL) {
									STR_APPEND(groupInSelectColsListWithJoiningCols, ", ");
									STR_APPEND(groupInSelectColsListWithJoiningCols, findWhat);
								}
							}
						}
					}
					tmp = colPos + 1;
				}

				//finish analyzing
				FPRINTF(F, "%s", groupInSelectColsListWithJoiningCols);
				FPRINTF(F, "\");\n");
				FPRINTF(F, "		ADDQ(\"");
				FPRINTF(F, " FROM ");
				for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
					if (i != 0) {
						FPRINTF(F, " %s JOIN ", OBJ_GROUP(groupIn)->JoiningTypes[i - 1]);
					}
					FPRINTF(F, " %s ", OBJ_GROUP(groupIn)->tables[i]->name);
					if (i != 0) {
						FPRINTF(F, " ON %s ", OBJ_GROUP(groupIn)->JoiningConditions[i - 1]);
					}
				}
				FPRINTF(F, "\");\n");
				FPRINTF(F, "		ADDQ(\"WHERE true \");\n");
				for (int i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
						else { quote = ""; strprefix = "str_"; }
						FPRINTF(F, "		ADDQ(\" and %s.%s = %s\"); \n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
						FPRINTF(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
						FPRINTF(F, "		ADDQ(\"%s \"); \n", quote);
						FREE(ctype);
					}
				}
				FPRINTF(F, "		ADDQ(\");\");\n");
				//--end of defining group_in_delete
				//define group_out

				char * groupOutSelectColsList = "";
				char * groupOutSelectColsListWithJoiningCols = "";
				STR_INIT(groupOutSelectColsList, " ");

				if (hasGroupOut) {
					FPRINTF(F, "		ADDQ(\"CREATE TEMP TABLE group_out AS( SELECT\");\n");
					FPRINTF(F, "		ADDQ(\"");

					//Determine how many col in group_out, use to know when to stop printing "," and in DEFR
					int colInGroupOut = 0;
					for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
						for (int k = 0; k < OBJ_GROUP(groupOut)->tablesNum; k++) {
							if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_GROUP(groupOut)->tables[k]->name)) {
								colInGroupOut++;
							}
						}
					}
					counter = 0;
					//loop through group out
					for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
						for (int k = 0; k < OBJ_GROUP(groupOut)->tablesNum; k++) {
							if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_GROUP(groupOut)->tables[k]->name)) {
								counter++;
								STR_APPEND(groupOutSelectColsList, OBJ_SELECTED_COL(i)->table->name);
								STR_APPEND(groupOutSelectColsList, ".");
								STR_APPEND(groupOutSelectColsList, OBJ_SELECTED_COL(i)->name);
								if (counter < colInGroupOut) {
									STR_APPEND(groupOutSelectColsList, ", ");
								}								
							}
						}
					}

					/*analyze to add the column of joining condition if not appears*/
					STR_INIT(groupOutSelectColsListWithJoiningCols, " ");
					STR_APPEND(groupOutSelectColsListWithJoiningCols, groupOutSelectColsList);
					//loop through cols in table in group in, search for <table>.<col> in joining condition
					//and space before group join to replace correctly
					groupJoiningCondition = "";
					STR_INIT(groupJoiningCondition, " ");
					STR_APPEND(groupJoiningCondition, OBJ_SQ->groupJoinCondition);
					tmp = "";
					STR_INIT(tmp, " ");
					STR_APPEND(tmp, OBJ_SQ->groupJoinCondition);
					colPos = "";
					while (colPos = findSubString(tmp, ".")) {
						char *tbName = getPrecededTableName(tmp, colPos + 1);
						for (int k = 0; k < OBJ_GROUP(groupOut)->tablesNum; k++) {
							if (STR_EQUAL_CI(OBJ_GROUP(groupOut)->tables[k]->name, trim(tbName))) {
								char *findWhat;
								STR_INIT(findWhat, " ");
								STR_APPEND(findWhat, tbName);
								STR_APPEND(findWhat, getColumnName(colPos));
								isFoundInGroupJoin = findSubString(groupJoiningCondition, findWhat);
								//if found
								if (isFoundInGroupJoin) {
									isFoundInSelectColsList = findSubString(groupOutSelectColsList, findWhat);
									if (isFoundInSelectColsList == NULL) {
										STR_APPEND(groupOutSelectColsListWithJoiningCols, ", ");
										STR_APPEND(groupOutSelectColsListWithJoiningCols, findWhat);
									}
								}
							}
						}
						tmp = colPos + 1;
					}

					//finish analyzing

					FPRINTF(F, "%s", groupOutSelectColsListWithJoiningCols);
					FPRINTF(F, "\");\n");
					FPRINTF(F, "		ADDQ(\"");
					FPRINTF(F, " FROM ");
					for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
						if (i != 0) {
							FPRINTF(F, " %s JOIN ", OBJ_GROUP(groupOut)->JoiningTypes[i - 1]);
						}
						FPRINTF(F, " %s ", OBJ_GROUP(groupOut)->tables[i]->name);
						if (i != 0) {
							FPRINTF(F, " ON %s ", OBJ_GROUP(groupOut)->JoiningConditions[i - 1]);
						}
					}
					FPRINTF(F, "\");\n");
					FPRINTF(F, "		ADDQ(\");\");\n");
					//end of defining group out
				}				
				//define mv_delete
				analyzedGroupJoinCondition;
				//and space before group join to replace correctly
				STR_INIT(analyzedGroupJoinCondition, " ");
				STR_APPEND(analyzedGroupJoinCondition, OBJ_SQ->groupJoinCondition);
				/*Group joining condition is in the format: <table of group 1>.col_x = <table of group 2>.col_y
				But we cant know which table belongs to which group
				-> need to analyzed to know which table is in which group and replace the table name by group name
				ex: group joining condition = 't2.c4 = t3.c5' , t2 in group 0 (currently is group in), t3 in group 1 (currently is group out)
				-> after analyzed: analyzedGroupJoinCondition = group_in_insert.c4 = group_out.c5
				*/
				tmp = " ";
				//and space before group join to use the getPrecededTableName
				STR_INIT(tmp, " ");
				STR_APPEND(tmp, OBJ_SQ->groupJoinCondition);

				colPos = "";
				while (colPos = findSubString(tmp, ".")) {
					char *tbName = getPrecededTableName(tmp, colPos + 1);
					//determine group of tbName
					for (int k = 0; k < OBJ_GROUP(groupIn)->tablesNum; k++) {
						if (STR_EQUAL_CI(OBJ_GROUP(groupIn)->tables[k]->name, trim(tbName))) {
							char *findWhat;
							STR_INIT(findWhat, " ");
							STR_APPEND(findWhat, tbName);
							STR_APPEND(findWhat, ".");
							char *replaceWith = " group_in_delete.";
							analyzedGroupJoinCondition = dammf_replaceAll(analyzedGroupJoinCondition, findWhat, replaceWith);
							//printf("analyzedGroupJoinCondition = %s\n", analyzedGroupJoinCondition);
						}
					}
					for (int k = 0; k < OBJ_GROUP(groupOut)->tablesNum; k++) {
						if (STR_EQUAL_CI(OBJ_GROUP(groupOut)->tables[k]->name, trim(tbName))) {
							char *findWhat;
							STR_INIT(findWhat, " ");
							STR_APPEND(findWhat, tbName);
							STR_APPEND(findWhat, ".");
							char *replaceWith = " group_out.";
							analyzedGroupJoinCondition = dammf_replaceAll(analyzedGroupJoinCondition, findWhat, replaceWith);
							//printf("analyzedGroupJoinCondition = %s\n", analyzedGroupJoinCondition);
						}
					}
					tmp = colPos + 1;
				}


				int colInMvInsert = OBJ_NSELECTED - 1; // dont count mvcount
													   /*we need to replace <table>.<col> in groupOutSelectColsList -> group_in_insert.col*/
				tmp = " ";
				//and space before group join to use the getPrecededTableName
				STR_INIT(tmp, " ");
				STR_APPEND(tmp, groupInSelectColsList);

				colPos = "";
				while (colPos = findSubString(tmp, ".")) {
					char *tbName = getPrecededTableName(tmp, colPos + 1);
					char *findWhat;
					STR_INIT(findWhat, " ");
					STR_APPEND(findWhat, tbName);
					STR_APPEND(findWhat, ".");
					groupInSelectColsList = replace_str(groupInSelectColsList, findWhat, " group_in_delete.");
					tmp = colPos + 1;
				}

				tmp = " ";
				STR_INIT(tmp, " ");
				STR_APPEND(tmp, groupOutSelectColsList);

				colPos = "";
				while (colPos = findSubString(tmp, ".")) {
					char *tbName = getPrecededTableName(tmp, colPos + 1);
					char *findWhat;
					STR_INIT(findWhat, " ");
					STR_APPEND(findWhat, tbName);
					STR_APPEND(findWhat, ".");
					groupOutSelectColsList = replace_str(groupOutSelectColsList, findWhat, " group_out.");
					tmp = colPos + 1;
				}

				FPRINTF(F, "		ADDQ(\"CREATE TEMP TABLE mv_delete AS( SELECT\");\n");
				if (hasGroupOut) {
					//because insert required the correct position of column 
					if (groupIn == 0) {
						FPRINTF(F, "		ADDQ(\" %s, %s \");\n", groupInSelectColsList, groupOutSelectColsList);
					}
					else {
						FPRINTF(F, "		ADDQ(\" %s, %s \");\n", groupOutSelectColsList, groupInSelectColsList);
					}
					FPRINTF(F, "		ADDQ(\" \");\n");
					//the order of 2 group in joining condition is also important

					if (groupIn == 0) {
						FPRINTF(F, "		ADDQ(\" FROM group_in_delete %s JOIN group_out ON %s \");\n", OBJ_SQ->groupJoinType, analyzedGroupJoinCondition);
					}
					else {
						FPRINTF(F, "		ADDQ(\" FROM group_out %s JOIN group_in_delete ON %s \");\n", OBJ_SQ->groupJoinType, analyzedGroupJoinCondition);
					}

					FPRINTF(F, "		ADDQ(\"WHERE true \");\n");
					for (int i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
							char *quote;
							char *strprefix;
							char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
							if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
							else { quote = ""; strprefix = "str_"; }
							FPRINTF(F, "		ADDQ(\" and group_in_delete.%s = %s\"); \n", OBJ_TABLE(j)->cols[i]->name, quote);
							FPRINTF(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
							FPRINTF(F, "		ADDQ(\"%s \"); \n", quote);
							FREE(ctype);
						}
					}

				}
				else {
					FPRINTF(F, "		ADDQ(\"%s FROM group_in_delete \");\n", groupInSelectColsList);
				}

				FPRINTF(F, "		ADDQ(\");\");\n");
				//------------------------------------------------
				//end of defining mv_delete
				//define group_in_insert
				/* Condition to have group in insert: the deleting table is comp or equal in the current group
					then check if there is any records of type: (main, main, current table col, current table col , ...) in mv
					if not, insert main, main, null, null, ...
				*/
				int	colInGroupInInsert = 0;

				//CONDITION TO HAVE group_in_insert
				//roleInGroup = "disabled";
				if ((STR_EQUAL_CI(roleInGroup, "comp") || STR_EQUAL_CI(roleInGroup, "equal"))&&!OBJ_SQ->hasJoin) {
					//loop through selected col and just care about column of tables in group in, table <> current table

					for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
						if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
							for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)) {
										colInGroupInInsert++;
									}
								}
							}
						}
					}
					
					FPRINTF(F, "		ADDQ(\"CREATE TEMP TABLE group_in_insert AS( SELECT DISTINCT \");\n");
					FPRINTF(F, "		ADDQ(\"");
					counter = 0;
					for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
						if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
							for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)) {
										counter++;
										FPRINTF(F, " %s%s ",
											OBJ_GROUP(groupIn)->tables[i]->cols[k]->name, (counter < colInGroupInInsert ? "," : " "));
									}
								}
							}
						}
					}
					FPRINTF(F, "\");\n");
					FPRINTF(F, "		ADDQ(\" FROM mv_delete \");\n");
					FPRINTF(F, "		ADDQ(\");\");\n");
				}
					
				//end of defining group_in_insert
				//define mv_insert
				/*condition to have mv_insert is the group of current table is comp or equal in original query*/
				colInMvInsert = 0;
				if (STR_EQUAL_CI(OBJ_GROUP(groupIn)->RoleInOriginalQuery, "comp")
					|| STR_EQUAL_CI(OBJ_GROUP(groupIn)->RoleInOriginalQuery, "equal")) {
					//determine col in mv delete, use for DEFR() later
					for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
						for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
							for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
								if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)
								&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupOut)->tables[i]->name)) {
									colInMvInsert++;
								}
							}
						}
					}
					counter = 0;
					FPRINTF(F, "		ADDQ(\"CREATE TEMP TABLE mv_insert AS( SELECT DISTINCT \");\n");
					FPRINTF(F, "		ADDQ(\"");

					for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
						for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
							for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
								if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)
									&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupOut)->tables[i]->name)) {
									counter++;
									FPRINTF(F, " %s%s ", OBJ_GROUP(groupOut)->tables[i]->cols[k]->name, (counter < colInMvInsert ? "," : " "));
								}
							}
						}
					}
					
					FPRINTF(F, "\");\n");
					FPRINTF(F, "		ADDQ(\" FROM mv_delete WHERE \");\n");
					counter = 0;
					for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
						for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
							//just need to check the primary key 
							if (OBJ_GROUP(groupOut)->tables[i]->cols[k]->isPrimaryColumn) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)) {
										FPRINTF(F, "		ADDQ(\" %s %s is not null \");\n", counter > 0 ? "or" : " ", OBJ_GROUP(groupOut)->tables[i]->cols[k]->name);
										counter++;

									}
								}
							}							
						}
					}

					FPRINTF(F, "		ADDQ(\");\");\n");
				}

				//end of defining mv_insert

				//STEP 1: SELECT FROM MV_DELETE, DELETE FROM MV
				FPRINTF(F, "		ADDQ(\"SELECT * FROM mv_delete; \");\n");
				FPRINTF(F, "		EXEC_;\n");
				FPRINTF(F, "		if(NO_ROW) {\n");
				FPRINTF(F, "			goto end_;\n");
				FPRINTF(F, "		}\n");
				//FPRINTF(F, "		DEFR(%d);\n", colInMvDelete);
				FPRINTF(F, "		DEFR(%d);\n", OBJ_NSELECTED - 1);

				FPRINTF(F, "		FOR_EACH_RESULT_ROW(i){\n");
				for (i = 0; i < OBJ_NSELECTED - 1; i++) {
					if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
						FPRINTF(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
						FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
					}
					else {
						FPRINTF(F, "			GET_NULLABLE_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
						FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
					}
				}
				FPRINTF(F, "		}\n");

				FPRINTF(F, "\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED - 1);

				k = 0;
				for (i = 0; i < OBJ_NSELECTED - 1; i++)
					FPRINTF(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

				FPRINTF(F, "\n			DEFQ(\"DELETE FROM %s WHERE TRUE \");\n", mvName);
				for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
					if (STR_INEQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
						continue;
					}

					for (int k = 0; k < OBJ_TABLE(j)->nCols; k++) {
						if (STR_EQUAL(OBJ_SELECTED_COL(i)->name, OBJ_TABLE(j)->cols[k]->name) && OBJ_TABLE(j)->cols[k]->isPrimaryColumn) {

							char *quote;
							char *ctype;
							ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							FPRINTF(F, "			ADDQ(\" AND %s \");\n", OBJ_SELECTED_COL(i)->name);
							FPRINTF(F, "			ADDQ(\"=\");\n", quote);
							FPRINTF(F, "			ADDQ(\"%s\");\n", quote);
							FPRINTF(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "			ADDQ(\"%s\");\n", quote);

							FREE(ctype);
						}
					}
				}
				/*for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
					char *quote;
					char *ctype;
					ctype = createCType(OBJ_SELECTED_COL(i)->type);
					if (STR_EQUAL(ctype, "char *")) quote = "\'";
					else quote = "";
					FPRINTF(F, "			ADDQ(\" AND %s \");\n", OBJ_SELECTED_COL(i)->name);
					if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
						FPRINTF(F, "			ADDQ(\"=\");\n", quote);
						FPRINTF(F, "			ADDQ(\"%s\");\n", quote);
						FPRINTF(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						FPRINTF(F, "			ADDQ(\"%s\");\n", quote);
					}
					else {
						FPRINTF(F, "			ADDQ_ISNULL_OR_VALUE(%s, \"%s\", \"=\");\n", OBJ_SELECTED_COL(i)->varName, quote);
					}
					FREE(ctype);					
				}*/
				FPRINTF(F, "\n			EXEC_; \n");
				// end of FOR_EACH_SAVED_RESULT_ROW
				FPRINTF(F, "\n		} // end of FOR_EACH_SAVED_RESULT_ROW \n");

				//STEP 2: Insert into mv1: if in group, role of current table is comp or equal
				// check if there is no more now of current table match the main table. if so, insert(main, main, NULL, NULL,...)
				//with main is from group_in_insert
				if ((STR_EQUAL_CI(roleInGroup, "comp") || STR_EQUAL_CI(roleInGroup, "equal")) && !OBJ_SQ->hasJoin) {
					FPRINTF(F, "\n		DEFQ(\"SELECT * FROM group_in_insert; \");\n");
					FPRINTF(F, "		EXEC_;\n");

					FPRINTF(F, "		DEFR(%d);\n", colInGroupInInsert);

					FPRINTF(F, "		FOR_EACH_RESULT_ROW(i){\n");
					counter = 0;
					for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
						if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
							for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)) {
										FPRINTF(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(l)->varName, counter + 1);
										FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(l)->varName);
										counter++;
									}
								}
							}
						}
					}

					FPRINTF(F, "		}\n");

					FPRINTF(F, "\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", colInGroupInInsert);

					counter = 0;
					for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
						if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {							
							for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)) {
										FPRINTF(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(l)->varName, counter++);
									}
								}
							}
						}
					}

					FPRINTF(F, "\n			DEFQ(\"SELECT count(*) FROM %s WHERE true \");\n", mvName);

					for(int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++){
						if (STR_INEQUAL(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
							for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
								if (OBJ_GROUP(groupIn)->tables[i]->cols[k]->isPrimaryColumn) {
									for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
										if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)) {
											char *quote;
											char *ctype;
											ctype = createCType(OBJ_SELECTED_COL(l)->type);
											if (STR_EQUAL(ctype, "char *")) quote = "\'";
											else quote = "";
											FPRINTF(F, "			ADDQ(\" AND %s \");\n", OBJ_SELECTED_COL(l)->name);
											FPRINTF(F, "			ADDQ_ISNULL_OR_VALUE(%s,\"%s\", \"=\");\n", OBJ_SELECTED_COL(l)->varName, quote);
											FREE(ctype);
										}
									}									
								}
							}
						}
					}

					//because primary key of table j is always join in select so dont need to loop through obj_select_col 
					for (int i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
							FPRINTF(F, "			ADDQ(\" and %s is NOT null\");\n", OBJ_TABLE(j)->cols[i]->name);
						}
					}
					FPRINTF(F, "			EXEC_;\n");
					FPRINTF(F, "			GET_STR_ON_RESULT(mvcount, 0, 1);\n\n");
					//if there is no row, insert
					FPRINTF(F, "			if (STR_EQUAL(mvcount,\"0\")) {\n");
					FPRINTF(F, "				DEFQ(\"INSERT INTO %s VALUES(\");\n", mvName);
					//if current group is the first, loop through table in group in
					//else loop through table in group out first

					counter = 0;
					if (groupIn == 0) {
						for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
							for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)
										&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupIn)->tables[i]->name)) {
										//if is main table
										if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
											char *quote;
											char *ctype;
											ctype = createCType(OBJ_SELECTED_COL(l)->type);
											if (STR_EQUAL(ctype, "char *")) quote = "\'";
											else quote = "";
											FPRINTF(F, "				ADDQ_NULL_OR_VALUE(%s,\"%s\");\n", OBJ_SELECTED_COL(l)->varName, quote);
											FREE(ctype);
										}
										else {
											FPRINTF(F, "				ADDQ(\" null \");\n");
										}
										counter++;
										if (counter < OBJ_NSELECTED - 1) {
											FPRINTF(F, "				ADDQ(\",\");\n");
										}
									}
								}
							}
						}
						for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
							for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)
										&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupOut)->tables[i]->name)) {
										FPRINTF(F, "				ADDQ(\" null \");\n");
										counter++;
										if (counter < OBJ_NSELECTED - 1) {
											FPRINTF(F, "				ADDQ(\",\");\n");
										}
									}
								}
							}
						}
					}
					else {
						for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
							for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)
										&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupOut)->tables[i]->name)) {
										FPRINTF(F, "				ADDQ(\" null \");\n");
										counter++;
										if (counter < OBJ_NSELECTED - 1) {
											FPRINTF(F, "				ADDQ(\",\");\n");
										}
									}
								}
							}
						}

						for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
							for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)
										&&STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupIn)->tables[i]->name)) {
										//if is main table
										if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
											char *quote;
											char *ctype;
											ctype = createCType(OBJ_SELECTED_COL(l)->type);
											if (STR_EQUAL(ctype, "char *")) quote = "\'";
											else quote = "";
											FPRINTF(F, "				ADDQ_NULL_OR_VALUE(%s,\"%s\");\n", OBJ_SELECTED_COL(l)->varName, quote);
											FREE(ctype);
										}
										else {
											FPRINTF(F, "				ADDQ(\" null \");\n");
										}
										counter++;
										if (counter < OBJ_NSELECTED - 1) {
											FPRINTF(F, "				ADDQ(\",\");\n");
										}
									}
								}
							}
						}
					}
					FPRINTF(F, "				ADDQ(\");\");\n");
					FPRINTF(F, "				EXEC_;\n");
					FPRINTF(F, "			}\n");

					FPRINTF(F, "		}\n");

				} 
				//END OF STEP 2
				//STEP 3: from mv_insert, insert into mv if after delete, there is no row in group in match with group out
				if (STR_EQUAL_CI(OBJ_GROUP(groupIn)->RoleInOriginalQuery, "comp")
					|| STR_EQUAL_CI(OBJ_GROUP(groupIn)->RoleInOriginalQuery, "equal")) {
					FPRINTF(F, "\n		DEFQ(\"SELECT * FROM mv_insert; \");\n");
					FPRINTF(F, "		EXEC_;\n");

					FPRINTF(F, "		DEFR(%d);\n", colInMvInsert);

					FPRINTF(F, "		FOR_EACH_RESULT_ROW(i){\n");
					counter = 0;
					for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
						for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
							for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
								if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)
									&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupOut)->tables[i]->name)) {
									FPRINTF(F, "			GET_NULLABLE_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(l)->varName, counter + 1);
									FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(l)->varName);
									counter++;
								}
							}
						}
					}

					FPRINTF(F, "		}\n");

					FPRINTF(F, "\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", colInMvInsert);

					counter = 0;
					for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
						for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
							for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
								if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)
									&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupOut)->tables[i]->name)) {
									FPRINTF(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(l)->varName, counter++);
								}
							}
						}
					}

					FPRINTF(F, "\n			DEFQ(\"SELECT count(*) FROM %s WHERE true \");\n", mvName);

					for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
						for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
							if (OBJ_GROUP(groupOut)->tables[i]->cols[k]->isPrimaryColumn) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)) {
										char *quote;
										char *ctype;
										ctype = createCType(OBJ_SELECTED_COL(l)->type);
										if (STR_EQUAL(ctype, "char *")) quote = "\'";
										else quote = "";
										FPRINTF(F, "			ADDQ(\" AND %s \");\n", OBJ_SELECTED_COL(l)->name);
										FPRINTF(F, "			ADDQ_ISNULL_OR_VALUE(%s,\"%s\", \"=\");\n", OBJ_SELECTED_COL(l)->varName, quote);
										FREE(ctype);
									}
								}
							}
						}
					}

					for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
						for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
							if (OBJ_GROUP(groupIn)->tables[i]->cols[k]->isPrimaryColumn) {
								FPRINTF(F, "			ADDQ(\" and %s is NOT null\");\n", OBJ_GROUP(groupIn)->tables[i]->cols[k]->name);
							}
						}
					}

					FPRINTF(F, "			EXEC_;\n");
					FPRINTF(F, "			GET_STR_ON_RESULT(mvcount, 0, 1);\n\n");
					//if there is no row, insert
					FPRINTF(F, "			if (STR_EQUAL(mvcount,\"0\")) {\n");
					FPRINTF(F, "				DEFQ(\"INSERT INTO %s VALUES(\");\n", mvName);
					//if current group is the first, loop through table in group in
					//else loop through table in group out first

					counter = 0;
					if (groupIn == 0) {
						for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
							for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)
										//to avoid: foreign key appear in current table should not be count
										&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupIn)->tables[i]->name)) {
										FPRINTF(F, "				ADDQ(\" null \");\n");
										counter++;
										if (counter < OBJ_NSELECTED - 1) {
											FPRINTF(F, "				ADDQ(\",\");\n");
										}
									}
								}
							}
						}

						for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
							for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)
										//to avoid: foreign key appear in current table should not be count
										&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupOut)->tables[i]->name)) {
										char *quote;
										char *ctype;
										ctype = createCType(OBJ_SELECTED_COL(l)->type);
										if (STR_EQUAL(ctype, "char *")) quote = "\'";
										else quote = "";
										FPRINTF(F, "				ADDQ_NULL_OR_VALUE(%s,\"%s\");\n", OBJ_SELECTED_COL(l)->varName, quote);
										FREE(ctype);
										counter++;
										if (counter < OBJ_NSELECTED - 1) {
											FPRINTF(F, "				ADDQ(\",\");\n");
										}
									}
								}
							}
						}
					}
					else {

						for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
							for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)
										//to avoid: foreign key appear in current table should not be count
										&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupOut)->tables[i]->name)) {
										char *quote;
										char *ctype;
										ctype = createCType(OBJ_SELECTED_COL(l)->type);
										if (STR_EQUAL(ctype, "char *")) quote = "\'";
										else quote = "";
										FPRINTF(F, "				ADDQ_NULL_OR_VALUE(%s,\"%s\");\n", OBJ_SELECTED_COL(l)->varName, quote);
										FREE(ctype);
										counter++;
										if (counter < OBJ_NSELECTED - 1) {
											FPRINTF(F, "				ADDQ(\",\");\n");
										}
									}
								}
							}
						}

						for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
							for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)
										//to avoid: foreign key appear in current table should not be count
										&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupIn)->tables[i]->name)) {
										FPRINTF(F, "				ADDQ(\" null \");\n");
										counter++;
										if (counter < OBJ_NSELECTED - 1) {
											FPRINTF(F, "				ADDQ(\",\");\n");
										}										
									}
								}
							}
						}

					}
					FPRINTF(F, "				ADDQ(\");\");\n");
					FPRINTF(F, "				EXEC_;\n");
					FPRINTF(F, "			}\n");

					FPRINTF(F, "		}\n");
				}
				//END OF STEP 3

				FPRINTF(F, "	}\n");
				FPRINTF(F, "	end_: DEFQ(\"DROP TABLE IF EXISTS mv_insert, mv_delete, group_in_delete, group_in_insert, group_out;\"); EXEC_; \n	END; \n}\n\n");
				
				
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
				FPRINTF(F, "TRIGGER(%s) {\n", triggerName);
				FREE(triggerName);

				// Variable declaration
				FPRINTF(F, "	// MV's data\n");
				for (i = 0; i < OBJ_NSELECTED; i++) {
					char *varname = OBJ_SELECTED_COL(i)->varName;
					char *tableName = "<expression>";
					if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
					FPRINTF(F, "	char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
				}

				// Table's vars
				FPRINTF(F, "	// Table's data\n");
				for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
					char *varname = OBJ_TABLE(j)->cols[i]->varName;
					char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
					FPRINTF(F, "	%s %s;\n", ctype, varname);
					if (STR_INEQUAL(ctype, "char *")) {
						if (STR_EQUAL(ctype, "Numeric"))
							FPRINTF(F, "	char * str_%s;\n", varname);
						else
							FPRINTF(F, "	char str_%s[20];\n", varname);
					}
					FREE(ctype);
				}

				// Standard trigger preparing procedures
				FPRINTF(F, "\n	BEGIN;\n\n");

				FPRINTF(F, "	if(UTRIGGER_FIRED_BEFORE){\n");
				// Get values of table's fields
				FPRINTF(F, "	// Get table's data\n");
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
					FPRINTF(F, "	%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						FPRINTF(F, "	%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
					FREE(ctype);
				}
				//DELETE
				if(TRUE){
					// FCC
					FPRINTF(F, "\n	// FCC\n");
					FPRINTF(F, "	if(%s){\n", OBJ_FCC(j));

					//define group_in_delete
					FPRINTF(F, "		DEFQ(\"CREATE TEMP TABLE group_in_delete AS( SELECT\");\n");
					FPRINTF(F, "		ADDQ(\"");

					//Determine how many col in group_in_delete, use to know when to stop printing "," and in DEFR() later
					colInGroupInDelete = 0;

					/*	groupInSelectColsList is the list of cols that appears in original query
					groupInSelectColsListWithJoiningCols add some cols in group joining condition that might not appears in original query
					*/
					groupInSelectColsList = "";
					groupInSelectColsListWithJoiningCols = "";

					for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
						for (int k = 0; k < OBJ_GROUP(groupIn)->tablesNum; k++) {
							if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_GROUP(groupIn)->tables[k]->name)) {
								colInGroupInDelete++;
							}
						}
					}

					STR_INIT(groupInSelectColsList, " ");

					int counter = 0;
					//loop through group in
					for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
						for (int k = 0; k < OBJ_GROUP(groupIn)->tablesNum; k++) {
							if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_GROUP(groupIn)->tables[k]->name)) {
								counter++;
								STR_APPEND(groupInSelectColsList, OBJ_SELECTED_COL(i)->table->name);
								STR_APPEND(groupInSelectColsList, ".");
								STR_APPEND(groupInSelectColsList, OBJ_SELECTED_COL(i)->name);
								if (counter < colInGroupInInsert) {
									STR_APPEND(groupInSelectColsList, ", ");
								}
								/*FPRINTF(F," %s.%s%s", OBJ_SELECTED_COL(i)->table->name, OBJ_SELECTED_COL(i)->name,
								(counter < colInGroupInInsert) ? ", ":" ");*/
							}
						}
					}

					/*analyze to add the column of joining condition if not appears*/
					STR_INIT(groupInSelectColsListWithJoiningCols, " ");
					STR_APPEND(groupInSelectColsListWithJoiningCols, groupInSelectColsList);
					//loop through cols in table in group in, search for <table>.<col> in joining condition
					char * tmp, isFoundInGroupJoin, isFoundInSelectColsList;
					char *groupJoiningCondition;
					//and space before group join to replace correctly
					STR_INIT(groupJoiningCondition, " ");
					STR_APPEND(groupJoiningCondition, OBJ_SQ->groupJoinCondition);
					STR_INIT(tmp, " ");
					STR_APPEND(tmp, OBJ_SQ->groupJoinCondition);
					char *colPos = "";
					while (colPos = findSubString(tmp, ".")) {
						char *tbName = getPrecededTableName(tmp, colPos + 1);
						for (int k = 0; k < OBJ_GROUP(groupIn)->tablesNum; k++) {
							if (STR_EQUAL_CI(OBJ_GROUP(groupIn)->tables[k]->name, trim(tbName))) {
								char *findWhat;
								STR_INIT(findWhat, " ");
								STR_APPEND(findWhat, tbName);
								STR_APPEND(findWhat, getColumnName(colPos));
								isFoundInGroupJoin = findSubString(groupJoiningCondition, findWhat);
								//if found
								if (isFoundInGroupJoin) {
									isFoundInSelectColsList = findSubString(groupInSelectColsList, findWhat);
									if (isFoundInSelectColsList == NULL) {
										STR_APPEND(groupInSelectColsListWithJoiningCols, ", ");
										STR_APPEND(groupInSelectColsListWithJoiningCols, findWhat);
									}
								}
							}
						}
						tmp = colPos + 1;
					}

					//finish analyzing
					FPRINTF(F, "%s", groupInSelectColsListWithJoiningCols);
					FPRINTF(F, "\");\n");
					FPRINTF(F, "		ADDQ(\"");
					FPRINTF(F, " FROM ");
					for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
						if (i != 0) {
							FPRINTF(F, " %s JOIN ", OBJ_GROUP(groupIn)->JoiningTypes[i - 1]);
						}
						FPRINTF(F, " %s ", OBJ_GROUP(groupIn)->tables[i]->name);
						if (i != 0) {
							FPRINTF(F, " ON %s ", OBJ_GROUP(groupIn)->JoiningConditions[i - 1]);
						}
					}
					FPRINTF(F, "\");\n");
					FPRINTF(F, "		ADDQ(\"WHERE true \");\n");
					for (int i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
							char *quote;
							char *strprefix;
							char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
							if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
							else { quote = ""; strprefix = "str_"; }
							FPRINTF(F, "		ADDQ(\" and %s.%s = %s\"); \n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
							FPRINTF(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
							FPRINTF(F, "		ADDQ(\"%s \"); \n", quote);
							FREE(ctype);
						}
					}
					FPRINTF(F, "		ADDQ(\");\");\n");
					//--end of defining group_in_delete
					//define group_out

					char * groupOutSelectColsList = "";
					char * groupOutSelectColsListWithJoiningCols = "";
					STR_INIT(groupOutSelectColsList, " ");

					if (hasGroupOut) {
						FPRINTF(F, "		ADDQ(\"CREATE TEMP TABLE group_out AS( SELECT\");\n");
						FPRINTF(F, "		ADDQ(\"");

						//Determine how many col in group_out, use to know when to stop printing "," and in DEFR
						int colInGroupOut = 0;
						for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
							for (int k = 0; k < OBJ_GROUP(groupOut)->tablesNum; k++) {
								if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_GROUP(groupOut)->tables[k]->name)) {
									colInGroupOut++;
								}
							}
						}
						counter = 0;
						//loop through group out
						for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
							for (int k = 0; k < OBJ_GROUP(groupOut)->tablesNum; k++) {
								if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_GROUP(groupOut)->tables[k]->name)) {
									counter++;
									STR_APPEND(groupOutSelectColsList, OBJ_SELECTED_COL(i)->table->name);
									STR_APPEND(groupOutSelectColsList, ".");
									STR_APPEND(groupOutSelectColsList, OBJ_SELECTED_COL(i)->name);
									if (counter < colInGroupOut) {
										STR_APPEND(groupOutSelectColsList, ", ");
									}
									/*FPRINTF(F," %s.%s%s", OBJ_SELECTED_COL(i)->table->name, OBJ_SELECTED_COL(i)->name,
									(counter < colInGroupInInsert) ? ", ":" ");*/
								}
							}
						}

						/*analyze to add the column of joining condition if not appears*/
						STR_INIT(groupOutSelectColsListWithJoiningCols, " ");
						STR_APPEND(groupOutSelectColsListWithJoiningCols, groupOutSelectColsList);
						//loop through cols in table in group in, search for <table>.<col> in joining condition
						//and space before group join to replace correctly
						groupJoiningCondition = "";
						STR_INIT(groupJoiningCondition, " ");
						STR_APPEND(groupJoiningCondition, OBJ_SQ->groupJoinCondition);
						tmp = "";
						STR_INIT(tmp, " ");
						STR_APPEND(tmp, OBJ_SQ->groupJoinCondition);
						colPos = "";
						while (colPos = findSubString(tmp, ".")) {
							char *tbName = getPrecededTableName(tmp, colPos + 1);
							for (int k = 0; k < OBJ_GROUP(groupOut)->tablesNum; k++) {
								if (STR_EQUAL_CI(OBJ_GROUP(groupOut)->tables[k]->name, trim(tbName))) {
									char *findWhat;
									STR_INIT(findWhat, " ");
									STR_APPEND(findWhat, tbName);
									STR_APPEND(findWhat, getColumnName(colPos));
									isFoundInGroupJoin = findSubString(groupJoiningCondition, findWhat);
									//if found
									if (isFoundInGroupJoin) {
										isFoundInSelectColsList = findSubString(groupOutSelectColsList, findWhat);
										if (isFoundInSelectColsList == NULL) {
											STR_APPEND(groupOutSelectColsListWithJoiningCols, ", ");
											STR_APPEND(groupOutSelectColsListWithJoiningCols, findWhat);
										}
									}
								}
							}
							tmp = colPos + 1;
						}

						//finish analyzing

						FPRINTF(F, "%s", groupOutSelectColsListWithJoiningCols);
						FPRINTF(F, "\");\n");
						FPRINTF(F, "		ADDQ(\"");
						FPRINTF(F, " FROM ");
						for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
							if (i != 0) {
								FPRINTF(F, " %s JOIN ", OBJ_GROUP(groupOut)->JoiningTypes[i - 1]);
							}
							FPRINTF(F, " %s ", OBJ_GROUP(groupOut)->tables[i]->name);
							if (i != 0) {
								FPRINTF(F, " ON %s ", OBJ_GROUP(groupOut)->JoiningConditions[i - 1]);
							}
						}
						FPRINTF(F, "\");\n");
						FPRINTF(F, "		ADDQ(\");\");\n");
						//end of defining group out
					}
					//define mv_delete
					analyzedGroupJoinCondition;
					//and space before group join to replace correctly
					STR_INIT(analyzedGroupJoinCondition, " ");
					STR_APPEND(analyzedGroupJoinCondition, OBJ_SQ->groupJoinCondition);
					/*Group joining condition is in the format: <table of group 1>.col_x = <table of group 2>.col_y
					But we cant know which table belongs to which group
					-> need to analyzed to know which table is in which group and replace the table name by group name
					ex: group joining condition = 't2.c4 = t3.c5' , t2 in group 0 (currently is group in), t3 in group 1 (currently is group out)
					-> after analyzed: analyzedGroupJoinCondition = group_in_insert.c4 = group_out.c5
					*/
					tmp = " ";
					//and space before group join to use the getPrecededTableName
					STR_INIT(tmp, " ");
					STR_APPEND(tmp, OBJ_SQ->groupJoinCondition);

					colPos = "";
					while (colPos = findSubString(tmp, ".")) {
						char *tbName = getPrecededTableName(tmp, colPos + 1);
						//determine group of tbName
						for (int k = 0; k < OBJ_GROUP(groupIn)->tablesNum; k++) {
							if (STR_EQUAL_CI(OBJ_GROUP(groupIn)->tables[k]->name, trim(tbName))) {
								char *findWhat;
								STR_INIT(findWhat, " ");
								STR_APPEND(findWhat, tbName);
								STR_APPEND(findWhat, ".");
								char *replaceWith = " group_in_delete.";
								analyzedGroupJoinCondition = dammf_replaceAll(analyzedGroupJoinCondition, findWhat, replaceWith);
								//printf("analyzedGroupJoinCondition = %s\n", analyzedGroupJoinCondition);
							}
						}
						for (int k = 0; k < OBJ_GROUP(groupOut)->tablesNum; k++) {
							if (STR_EQUAL_CI(OBJ_GROUP(groupOut)->tables[k]->name, trim(tbName))) {
								char *findWhat;
								STR_INIT(findWhat, " ");
								STR_APPEND(findWhat, tbName);
								STR_APPEND(findWhat, ".");
								char *replaceWith = " group_out.";
								analyzedGroupJoinCondition = dammf_replaceAll(analyzedGroupJoinCondition, findWhat, replaceWith);
								//printf("analyzedGroupJoinCondition = %s\n", analyzedGroupJoinCondition);
							}
						}
						tmp = colPos + 1;
					}


					int colInMvInsert = OBJ_NSELECTED - 1; // dont count mvcount
														   /*we need to replace <table>.<col> in groupOutSelectColsList -> group_in_insert.col*/
					tmp = " ";
					//and space before group join to use the getPrecededTableName
					STR_INIT(tmp, " ");
					STR_APPEND(tmp, groupInSelectColsList);

					colPos = "";
					while (colPos = findSubString(tmp, ".")) {
						char *tbName = getPrecededTableName(tmp, colPos + 1);
						char *findWhat;
						STR_INIT(findWhat, " ");
						STR_APPEND(findWhat, tbName);
						STR_APPEND(findWhat, ".");
						groupInSelectColsList = replace_str(groupInSelectColsList, findWhat, " group_in_delete.");
						tmp = colPos + 1;
					}

					tmp = " ";
					STR_INIT(tmp, " ");
					STR_APPEND(tmp, groupOutSelectColsList);

					colPos = "";
					while (colPos = findSubString(tmp, ".")) {
						char *tbName = getPrecededTableName(tmp, colPos + 1);
						char *findWhat;
						STR_INIT(findWhat, " ");
						STR_APPEND(findWhat, tbName);
						STR_APPEND(findWhat, ".");
						groupOutSelectColsList = replace_str(groupOutSelectColsList, findWhat, " group_out.");
						tmp = colPos + 1;
					}

					FPRINTF(F, "		ADDQ(\"CREATE TEMP TABLE mv_delete AS( SELECT\");\n");
					if (hasGroupOut) {
						//because insert required the correct position of column 
						if (groupIn == 0) {
							FPRINTF(F, "		ADDQ(\" %s, %s \");\n", groupInSelectColsList, groupOutSelectColsList);
						}
						else {
							FPRINTF(F, "		ADDQ(\" %s, %s \");\n", groupOutSelectColsList, groupInSelectColsList);
						}
						FPRINTF(F, "		ADDQ(\" \");\n");
						//the order of 2 group in joining condition is also important

						if (groupIn == 0) {
							FPRINTF(F, "		ADDQ(\" FROM group_in_delete %s JOIN group_out ON %s \");\n", OBJ_SQ->groupJoinType, analyzedGroupJoinCondition);
						}
						else {
							FPRINTF(F, "		ADDQ(\" FROM group_out %s JOIN group_in_delete ON %s \");\n", OBJ_SQ->groupJoinType, analyzedGroupJoinCondition);
						}

						FPRINTF(F, "		ADDQ(\"WHERE true \");\n");
						for (int i = 0; i < OBJ_TABLE(j)->nCols; i++) {
							if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
								char *quote;
								char *strprefix;
								char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
								if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
								else { quote = ""; strprefix = "str_"; }
								FPRINTF(F, "		ADDQ(\" and group_in_delete.%s = %s\"); \n", OBJ_TABLE(j)->cols[i]->name, quote);
								FPRINTF(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
								FPRINTF(F, "		ADDQ(\"%s \"); \n", quote);
								FREE(ctype);
							}
						}
					}
					else {
						FPRINTF(F, "		ADDQ(\" %s FROM group_in_delete \");\n", groupInSelectColsList);
					}
					FPRINTF(F, "		ADDQ(\");\");\n");
					//------------------------------------------------
					//end of defining mv_delete
					//define group_in_insert
					/* Condition to have group in insert: the deleting table is comp or equal in the current group
					then check if there is any records of type: (main, main, current table col, current table col , ...) in mv
					if not, insert main, main, null, null, ...
					*/
					int	colInGroupInInsert = 0;

					//CONDITION TO HAVE group_in_insert
					//roleInGroup = "disabled";
					if ((STR_EQUAL_CI(roleInGroup, "comp") || STR_EQUAL_CI(roleInGroup, "equal"))&&!OBJ_SQ->hasJoin) {
						//loop through selected col and just care about column of tables in group in, table <> current table

						for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
							if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
								for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
									for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
										if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)) {
											colInGroupInInsert++;
										}
									}
								}
							}
						}

						FPRINTF(F, "		ADDQ(\"CREATE TEMP TABLE group_in_insert AS( SELECT DISTINCT \");\n");
						FPRINTF(F, "		ADDQ(\"");
						counter = 0;
						for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
							if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
								for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
									for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
										if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)) {
											counter++;
											FPRINTF(F, " %s%s ",
												OBJ_GROUP(groupIn)->tables[i]->cols[k]->name, (counter < colInGroupInInsert ? "," : " "));
										}
									}
								}
							}
						}
						FPRINTF(F, "\");\n");
						FPRINTF(F, "		ADDQ(\" FROM mv_delete \");\n");
						FPRINTF(F, "		ADDQ(\");\");\n");
					}

					//end of defining group_in_insert
					//define mv_insert
					/*condition to have mv_insert is the group of current table is comp or equal in original query*/
					colInMvInsert = 0;
					if (STR_EQUAL_CI(OBJ_GROUP(groupIn)->RoleInOriginalQuery, "comp")
						|| STR_EQUAL_CI(OBJ_GROUP(groupIn)->RoleInOriginalQuery, "equal")) {
						//determine col in mv delete, use for DEFR() later
						for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
							for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)
										&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupOut)->tables[i]->name)) {
										colInMvInsert++;
									}
								}
							}
						}
						counter = 0;
						FPRINTF(F, "		ADDQ(\"CREATE TEMP TABLE mv_insert AS( SELECT DISTINCT \");\n");
						FPRINTF(F, "		ADDQ(\"");

						for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
							for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)
										&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupOut)->tables[i]->name)) {
										counter++;
										FPRINTF(F, " %s%s ", OBJ_GROUP(groupOut)->tables[i]->cols[k]->name, (counter < colInMvInsert ? "," : " "));
									}
								}
							}
						}

						FPRINTF(F, "\");\n");
						FPRINTF(F, "		ADDQ(\" FROM mv_delete WHERE \");\n");
						counter = 0;
						for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
							for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
								//just need to check the primary key 
								if (OBJ_GROUP(groupOut)->tables[i]->cols[k]->isPrimaryColumn) {
									for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
										if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)) {
											FPRINTF(F, "		ADDQ(\" %s %s is not null \");\n", counter > 0 ? "or" : " ", OBJ_GROUP(groupOut)->tables[i]->cols[k]->name);
											counter++;

										}
									}
								}
							}
						}

						FPRINTF(F, "		ADDQ(\");\");\n");
					}

					//end of defining mv_insert

					//STEP 1: SELECT FROM MV_DELETE, DELETE FROM MV
					FPRINTF(F, "		ADDQ(\"SELECT * FROM mv_delete; \");\n");
					FPRINTF(F, "		EXEC_;\n");
					FPRINTF(F, "		if(NO_ROW) {\n");
					FPRINTF(F, "			goto end_delete;\n");
					FPRINTF(F, "		}\n");
					//FPRINTF(F, "		DEFR(%d);\n", colInMvDelete);
					FPRINTF(F, "		DEFR(%d);\n", OBJ_NSELECTED - 1);

					FPRINTF(F, "		FOR_EACH_RESULT_ROW(i){\n");
					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
							FPRINTF(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
							FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
						else {
							FPRINTF(F, "			GET_NULLABLE_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
							FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
					}
					FPRINTF(F, "		}\n");

					FPRINTF(F, "\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED - 1);

					k = 0;
					for (i = 0; i < OBJ_NSELECTED - 1; i++)
						FPRINTF(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

					FPRINTF(F, "\n			DEFQ(\"DELETE FROM %s WHERE TRUE \");\n", mvName);
					for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
						if (STR_INEQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
							continue;
						}

						for (int k = 0; k < OBJ_TABLE(j)->nCols; k++) {
							if (STR_EQUAL(OBJ_SELECTED_COL(i)->name, OBJ_TABLE(j)->cols[k]->name) && OBJ_TABLE(j)->cols[k]->isPrimaryColumn) {

								char *quote;
								char *ctype;
								ctype = createCType(OBJ_SELECTED_COL(i)->type);
								if (STR_EQUAL(ctype, "char *")) quote = "\'";
								else quote = "";
								FPRINTF(F, "			ADDQ(\" AND %s \");\n", OBJ_SELECTED_COL(i)->name);
								FPRINTF(F, "			ADDQ(\"=\");\n", quote);
								FPRINTF(F, "			ADDQ(\"%s\");\n", quote);
								FPRINTF(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
								FPRINTF(F, "			ADDQ(\"%s\");\n", quote);

								FREE(ctype);
							}
						}
					}
					/*for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
					char *quote;
					char *ctype;
					ctype = createCType(OBJ_SELECTED_COL(i)->type);
					if (STR_EQUAL(ctype, "char *")) quote = "\'";
					else quote = "";
					FPRINTF(F, "			ADDQ(\" AND %s \");\n", OBJ_SELECTED_COL(i)->name);
					if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
					FPRINTF(F, "			ADDQ(\"=\");\n", quote);
					FPRINTF(F, "			ADDQ(\"%s\");\n", quote);
					FPRINTF(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
					FPRINTF(F, "			ADDQ(\"%s\");\n", quote);
					}
					else {
					FPRINTF(F, "			ADDQ_ISNULL_OR_VALUE(%s, \"%s\", \"=\");\n", OBJ_SELECTED_COL(i)->varName, quote);
					}
					FREE(ctype);
					}*/
					FPRINTF(F, "\n			EXEC_; \n");
					// end of FOR_EACH_SAVED_RESULT_ROW
					FPRINTF(F, "\n		} // end of FOR_EACH_SAVED_RESULT_ROW \n");

					//STEP 2: Insert into mv1: if in group, role of current table is comp or equal
					// check if there is no more now of current table match the main table. if so, insert(main, main, NULL, NULL,...)
					//with main is from group_in_insert
					if ((STR_EQUAL_CI(roleInGroup, "comp") || STR_EQUAL_CI(roleInGroup, "equal")) && !OBJ_SQ->hasJoin) {
						FPRINTF(F, "\n		DEFQ(\"SELECT * FROM group_in_insert; \");\n");
						FPRINTF(F, "		EXEC_;\n");

						FPRINTF(F, "		DEFR(%d);\n", colInGroupInInsert);

						FPRINTF(F, "		FOR_EACH_RESULT_ROW(i){\n");
						counter = 0;
						for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
							if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
								for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
									for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
										if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)) {
											FPRINTF(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(l)->varName, counter + 1);
											FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(l)->varName);
											counter++;
										}
									}
								}
							}
						}

						FPRINTF(F, "		}\n");

						FPRINTF(F, "\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", colInGroupInInsert);

						counter = 0;
						for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
							if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
								for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
									for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
										if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)) {
											FPRINTF(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(l)->varName, counter++);
										}
									}
								}
							}
						}

						FPRINTF(F, "\n			DEFQ(\"SELECT count(*) FROM %s WHERE true \");\n", mvName);

						for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
							if (STR_INEQUAL(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
								for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
									if (OBJ_GROUP(groupIn)->tables[i]->cols[k]->isPrimaryColumn) {
										for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
											if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)) {
												char *quote;
												char *ctype;
												ctype = createCType(OBJ_SELECTED_COL(l)->type);
												if (STR_EQUAL(ctype, "char *")) quote = "\'";
												else quote = "";
												FPRINTF(F, "			ADDQ(\" AND %s \");\n", OBJ_SELECTED_COL(l)->name);
												FPRINTF(F, "			ADDQ_ISNULL_OR_VALUE(%s,\"%s\", \"=\");\n", OBJ_SELECTED_COL(l)->varName, quote);
												FREE(ctype);
											}
										}
									}
								}
							}
						}

						//because primary key of table j is always join in select so dont need to loop through obj_select_col 
						for (int i = 0; i < OBJ_TABLE(j)->nCols; i++) {
							if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
								FPRINTF(F, "			ADDQ(\" and %s is NOT null\");\n", OBJ_TABLE(j)->cols[i]->name);
							}
						}
						FPRINTF(F, "			EXEC_;\n");
						FPRINTF(F, "			GET_STR_ON_RESULT(mvcount, 0, 1);\n\n");
						//if there is no row, insert
						FPRINTF(F, "			if (STR_EQUAL(mvcount,\"0\")) {\n");
						FPRINTF(F, "				DEFQ(\"INSERT INTO %s VALUES(\");\n", mvName);
						//if current group is the first, loop through table in group in
						//else loop through table in group out first

						counter = 0;
						if (groupIn == 0) {
							for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
								for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
									for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
										if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)
											&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupIn)->tables[i]->name)) {
											//if is main table
											if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
												char *quote;
												char *ctype;
												ctype = createCType(OBJ_SELECTED_COL(l)->type);
												if (STR_EQUAL(ctype, "char *")) quote = "\'";
												else quote = "";
												FPRINTF(F, "				ADDQ_NULL_OR_VALUE(%s,\"%s\");\n", OBJ_SELECTED_COL(l)->varName, quote);
												FREE(ctype);
											}
											else {
												FPRINTF(F, "				ADDQ(\" null \");\n");
											}
											counter++;
											if (counter < OBJ_NSELECTED - 1) {
												FPRINTF(F, "				ADDQ(\",\");\n");
											}
										}
									}
								}
							}
							for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
								for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
									for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
										if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)
											&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupOut)->tables[i]->name)) {
											FPRINTF(F, "				ADDQ(\" null \");\n");
											counter++;
											if (counter < OBJ_NSELECTED - 1) {
												FPRINTF(F, "				ADDQ(\",\");\n");
											}
										}
									}
								}
							}
						}
						else {
							for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
								for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
									for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
										if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)
											&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupOut)->tables[i]->name)) {
											FPRINTF(F, "				ADDQ(\" null \");\n");
											counter++;
											if (counter < OBJ_NSELECTED - 1) {
												FPRINTF(F, "				ADDQ(\",\");\n");
											}
										}
									}
								}
							}

							for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
								for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
									for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
										if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)
											&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupIn)->tables[i]->name)) {
											//if is main table
											if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
												char *quote;
												char *ctype;
												ctype = createCType(OBJ_SELECTED_COL(l)->type);
												if (STR_EQUAL(ctype, "char *")) quote = "\'";
												else quote = "";
												FPRINTF(F, "				ADDQ_NULL_OR_VALUE(%s,\"%s\");\n", OBJ_SELECTED_COL(l)->varName, quote);
												FREE(ctype);
											}
											else {
												FPRINTF(F, "				ADDQ(\" null \");\n");
											}
											counter++;
											if (counter < OBJ_NSELECTED - 1) {
												FPRINTF(F, "				ADDQ(\",\");\n");
											}
										}
									}
								}
							}
						}
						FPRINTF(F, "				ADDQ(\");\");\n");
						FPRINTF(F, "				EXEC_;\n");
						FPRINTF(F, "			}\n");

						FPRINTF(F, "		}\n");

					}
					//END OF STEP 2
					//STEP 3: from mv_insert, insert into mv if after delete, there is no row in group in match with group out
					if (STR_EQUAL_CI(OBJ_GROUP(groupIn)->RoleInOriginalQuery, "comp")
						|| STR_EQUAL_CI(OBJ_GROUP(groupIn)->RoleInOriginalQuery, "equal")) {
						FPRINTF(F, "\n		DEFQ(\"SELECT * FROM mv_insert; \");\n");
						FPRINTF(F, "		EXEC_;\n");

						FPRINTF(F, "		DEFR(%d);\n", colInMvInsert);

						FPRINTF(F, "		FOR_EACH_RESULT_ROW(i){\n");
						counter = 0;
						for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
							for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)
										&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupOut)->tables[i]->name)) {
										FPRINTF(F, "			GET_NULLABLE_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(l)->varName, counter + 1);
										FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(l)->varName);
										counter++;
									}
								}
							}
						}

						FPRINTF(F, "		}\n");

						FPRINTF(F, "\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", colInMvInsert);

						counter = 0;
						for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
							for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
								for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
									if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)
										&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupOut)->tables[i]->name)) {
										FPRINTF(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(l)->varName, counter++);
									}
								}
							}
						}

						FPRINTF(F, "\n			DEFQ(\"SELECT count(*) FROM %s WHERE true \");\n", mvName);

						for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
							for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
								if (OBJ_GROUP(groupOut)->tables[i]->cols[k]->isPrimaryColumn) {
									for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
										if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)) {
											char *quote;
											char *ctype;
											ctype = createCType(OBJ_SELECTED_COL(l)->type);
											if (STR_EQUAL(ctype, "char *")) quote = "\'";
											else quote = "";
											FPRINTF(F, "			ADDQ(\" AND %s \");\n", OBJ_SELECTED_COL(l)->name);
											FPRINTF(F, "			ADDQ_ISNULL_OR_VALUE(%s,\"%s\", \"=\");\n", OBJ_SELECTED_COL(l)->varName, quote);
											FREE(ctype);
										}
									}
								}
							}
						}

						for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
							for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
								if (OBJ_GROUP(groupIn)->tables[i]->cols[k]->isPrimaryColumn) {
									FPRINTF(F, "			ADDQ(\" and %s is NOT null\");\n", OBJ_GROUP(groupIn)->tables[i]->cols[k]->name);
								}
							}
						}

						FPRINTF(F, "			EXEC_;\n");
						FPRINTF(F, "			GET_STR_ON_RESULT(mvcount, 0, 1);\n\n");
						//if there is no row, insert
						FPRINTF(F, "			if (STR_EQUAL(mvcount,\"0\")) {\n");
						FPRINTF(F, "				DEFQ(\"INSERT INTO %s VALUES(\");\n", mvName);
						//if current group is the first, loop through table in group in
						//else loop through table in group out first

						counter = 0;
						if (groupIn == 0) {
							for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
								for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
									for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
										if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)
											//to avoid: foreign key appear in current table should not be count
											&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupIn)->tables[i]->name)) {
											FPRINTF(F, "				ADDQ(\" null \");\n");
											counter++;
											if (counter < OBJ_NSELECTED - 1) {
												FPRINTF(F, "				ADDQ(\",\");\n");
											}
										}
									}
								}
							}

							for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
								for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
									for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
										if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)
											//to avoid: foreign key appear in current table should not be count
											&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupOut)->tables[i]->name)) {
											char *quote;
											char *ctype;
											ctype = createCType(OBJ_SELECTED_COL(l)->type);
											if (STR_EQUAL(ctype, "char *")) quote = "\'";
											else quote = "";
											FPRINTF(F, "				ADDQ_NULL_OR_VALUE(%s,\"%s\");\n", OBJ_SELECTED_COL(l)->varName, quote);
											FREE(ctype);
											counter++;
											if (counter < OBJ_NSELECTED - 1) {
												FPRINTF(F, "				ADDQ(\",\");\n");
											}
										}
									}
								}
							}
						}
						else {

							for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
								for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
									for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
										if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)
											//to avoid: foreign key appear in current table should not be count
											&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupOut)->tables[i]->name)) {
											char *quote;
											char *ctype;
											ctype = createCType(OBJ_SELECTED_COL(l)->type);
											if (STR_EQUAL(ctype, "char *")) quote = "\'";
											else quote = "";
											FPRINTF(F, "				ADDQ_NULL_OR_VALUE(%s,\"%s\");\n", OBJ_SELECTED_COL(l)->varName, quote);
											FREE(ctype);
											counter++;
											if (counter < OBJ_NSELECTED - 1) {
												FPRINTF(F, "				ADDQ(\",\");\n");
											}
										}
									}
								}
							}

							for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
								for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
									for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
										if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)
											//to avoid: foreign key appear in current table should not be count
											&& STR_EQUAL_CI(OBJ_SELECTED_COL(l)->table->name, OBJ_GROUP(groupIn)->tables[i]->name)) {
											FPRINTF(F, "				ADDQ(\" null \");\n");
											counter++;
											if (counter < OBJ_NSELECTED - 1) {
												FPRINTF(F, "				ADDQ(\",\");\n");
											}
										}
									}
								}
							}

						}
						FPRINTF(F, "				ADDQ(\");\");\n");
						FPRINTF(F, "				EXEC_;\n");
						FPRINTF(F, "			}\n");

						FPRINTF(F, "		}\n");
					}
					//END OF STEP 3

					FPRINTF(F, "	}\n");
					FPRINTF(F, "	end_delete: DEFQ(\"DROP TABLE IF EXISTS mv_insert, mv_delete, group_in_delete, group_in_insert, group_out;\"); EXEC_; \n	END; \n\n");

				}
				
				FPRINTF(F, "	 }\n"); //end of if UTRIGGER FIRED BEFORE
				FPRINTF(F, "	else if(!UTRIGGER_FIRED_BEFORE){\n");
				// Get values of table's fields
				FPRINTF(F, "	// Get table's data\n");
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
					FPRINTF(F, "	%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
					if (STR_INEQUAL(ctype, "char *"))
						FPRINTF(F, "	%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
					FREE(ctype);
				}

				// FCC
				FPRINTF(F, "\n	// FCC\n");
				if (TRUE) {
					// FCC
					FPRINTF(F, "\n	// FCC\n");

					FPRINTF(F, "	if (%s) {\n", OBJ_FCC(j));
					//define group_in_insert
					FPRINTF(F, "		DEFQ(\"CREATE TEMP TABLE group_in_insert AS( SELECT\");\n");
					FPRINTF(F, "		ADDQ(\"");

					//Determine how many col in group_in_select, use to know when to stop printing "," and in DEFR() later
					int colInGroupInInsert = 0;

					/*	groupInSelectColsList is the list of cols that appears in original query
					groupInSelectColsListWithJoiningCols add some cols in group joining condition that might not appears in original query
					*/
					char * groupInSelectColsList = "";
					char * groupInSelectColsListWithJoiningCols = "";

					for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
						for (int k = 0; k < OBJ_GROUP(groupIn)->tablesNum; k++) {
							if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_GROUP(groupIn)->tables[k]->name)) {
								colInGroupInInsert++;
							}
						}
					}

					STR_INIT(groupInSelectColsList, " ");

					int counter = 0;
					//loop through group in
					for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
						for (int k = 0; k < OBJ_GROUP(groupIn)->tablesNum; k++) {
							if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_GROUP(groupIn)->tables[k]->name)) {
								counter++;
								STR_APPEND(groupInSelectColsList, OBJ_SELECTED_COL(i)->table->name);
								STR_APPEND(groupInSelectColsList, ".");
								STR_APPEND(groupInSelectColsList, OBJ_SELECTED_COL(i)->name);
								if (counter < colInGroupInInsert) {
									STR_APPEND(groupInSelectColsList, ", ");
								}
								/*FPRINTF(F," %s.%s%s", OBJ_SELECTED_COL(i)->table->name, OBJ_SELECTED_COL(i)->name,
								(counter < colInGroupInInsert) ? ", ":" ");*/
							}
						}
					}

					/*analyze to add the column of joining condition if not appears*/
					//This action is neccessary only if OBJ_SQ->groupJoinCondition != 'null'
					STR_INIT(groupInSelectColsListWithJoiningCols, " ");
					STR_APPEND(groupInSelectColsListWithJoiningCols, groupInSelectColsList);
					char * tmp, isFoundInGroupJoin, isFoundInSelectColsList;
					char *groupJoiningCondition;
					char *colPos = "";

					if (STR_INEQUAL_CI(OBJ_SQ->groupJoinCondition, "null")) {
						//loop through cols in table in group in, search for <table>.<col> in joining condition
						//and space before group join to replace correctly
						STR_INIT(groupJoiningCondition, " ");
						STR_APPEND(groupJoiningCondition, OBJ_SQ->groupJoinCondition);
						STR_INIT(tmp, " ");
						STR_APPEND(tmp, OBJ_SQ->groupJoinCondition);
						while (colPos = findSubString(tmp, ".")) {
							char *tbName = getPrecededTableName(tmp, colPos + 1);
							for (int k = 0; k < OBJ_GROUP(groupIn)->tablesNum; k++) {
								if (STR_EQUAL_CI(OBJ_GROUP(groupIn)->tables[k]->name, trim(tbName))) {
									char *findWhat;
									STR_INIT(findWhat, " ");
									STR_APPEND(findWhat, tbName);
									STR_APPEND(findWhat, getColumnName(colPos));
									isFoundInGroupJoin = findSubString(groupJoiningCondition, findWhat);
									//if found
									if (isFoundInGroupJoin) {
										isFoundInSelectColsList = findSubString(groupInSelectColsList, findWhat);
										if (isFoundInSelectColsList == NULL) {
											STR_APPEND(groupInSelectColsListWithJoiningCols, ", ");
											STR_APPEND(groupInSelectColsListWithJoiningCols, findWhat);
										}
									}
								}
							}
							tmp = colPos + 1;
						}
					}
					//finish analyzing
					FPRINTF(F, "%s", groupInSelectColsListWithJoiningCols);
					FPRINTF(F, "\");\n");
					FPRINTF(F, "		ADDQ(\"");
					FPRINTF(F, " FROM ");
					for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
						if (i != 0) {
							FPRINTF(F, " %s JOIN ", OBJ_GROUP(groupIn)->JoiningTypes[i - 1]);
						}
						FPRINTF(F, " %s ", OBJ_GROUP(groupIn)->tables[i]->name);
						if (i != 0) {
							FPRINTF(F, " ON %s ", OBJ_GROUP(groupIn)->JoiningConditions[i - 1]);
						}
					}
					FPRINTF(F, "\");\n");
					FPRINTF(F, "		ADDQ(\"WHERE true \");\n");
					for (int i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
							char *quote;
							char *strprefix;
							char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
							if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
							else { quote = ""; strprefix = "str_"; }
							FPRINTF(F, "		ADDQ(\" and %s.%s = %s\"); \n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
							FPRINTF(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
							FPRINTF(F, "		ADDQ(\"%s \"); \n", quote);
							FREE(ctype);
						}
					}
					FPRINTF(F, "		ADDQ(\");\");\n");

					//--end of defining group_in_insert

					//define group_out
					int colInGroupOut = 0;
					char * groupOutSelectColsList = "";
					char * groupOutSelectColsListWithJoiningCols = "";
					STR_INIT(groupOutSelectColsList, " ");

					if (hasGroupOut) {
						FPRINTF(F, "		ADDQ(\"CREATE TEMP TABLE group_out AS( SELECT\");\n");
						FPRINTF(F, "		ADDQ(\"");

						//Determine how many col in group_out, use to know when to stop printing "," and in DEFR
						for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
							for (int k = 0; k < OBJ_GROUP(groupOut)->tablesNum; k++) {
								if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_GROUP(groupOut)->tables[k]->name)) {
									colInGroupOut++;
								}
							}
						}

						counter = 0;
						//loop through group out
						for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
							for (int k = 0; k < OBJ_GROUP(groupOut)->tablesNum; k++) {
								if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_GROUP(groupOut)->tables[k]->name)) {
									counter++;
									STR_APPEND(groupOutSelectColsList, OBJ_SELECTED_COL(i)->table->name);
									STR_APPEND(groupOutSelectColsList, ".");
									STR_APPEND(groupOutSelectColsList, OBJ_SELECTED_COL(i)->name);
									if (counter < colInGroupOut) {
										STR_APPEND(groupOutSelectColsList, ", ");
									}
									/*FPRINTF(F," %s.%s%s", OBJ_SELECTED_COL(i)->table->name, OBJ_SELECTED_COL(i)->name,
									(counter < colInGroupInInsert) ? ", ":" ");*/
								}
							}
						}

						/*analyze to add the column of joining condition if not appears*/
						STR_INIT(groupOutSelectColsListWithJoiningCols, " ");
						STR_APPEND(groupOutSelectColsListWithJoiningCols, groupOutSelectColsList);
						//loop through cols in table in group in, search for <table>.<col> in joining condition
						//and space before group join to replace correctly
						groupJoiningCondition = "";
						STR_INIT(groupJoiningCondition, " ");
						STR_APPEND(groupJoiningCondition, OBJ_SQ->groupJoinCondition);
						if (STR_INEQUAL_CI(OBJ_SQ->groupJoinCondition, "null")) {
							tmp = "";
							STR_INIT(tmp, " ");
							STR_APPEND(tmp, OBJ_SQ->groupJoinCondition);
							colPos = "";
							while (colPos = findSubString(tmp, ".")) {
								char *tbName = getPrecededTableName(tmp, colPos + 1);
								for (int k = 0; k < OBJ_GROUP(groupOut)->tablesNum; k++) {
									if (STR_EQUAL_CI(OBJ_GROUP(groupOut)->tables[k]->name, trim(tbName))) {
										char *findWhat;
										STR_INIT(findWhat, " ");
										STR_APPEND(findWhat, tbName);
										STR_APPEND(findWhat, getColumnName(colPos));
										isFoundInGroupJoin = findSubString(groupJoiningCondition, findWhat);
										//if found
										if (isFoundInGroupJoin) {
											isFoundInSelectColsList = findSubString(groupOutSelectColsList, findWhat);
											if (isFoundInSelectColsList == NULL) {
												STR_APPEND(groupOutSelectColsListWithJoiningCols, ", ");
												STR_APPEND(groupOutSelectColsListWithJoiningCols, findWhat);
											}
										}
									}
								}
								tmp = colPos + 1;
							}
						}
						//finish analyzing
						FPRINTF(F, "%s", groupOutSelectColsListWithJoiningCols);
						FPRINTF(F, "\");\n");
						FPRINTF(F, "		ADDQ(\"");
						FPRINTF(F, " FROM ");
						for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
							if (i != 0) {
								FPRINTF(F, " %s JOIN ", OBJ_GROUP(groupOut)->JoiningTypes[i - 1]);
							}
							FPRINTF(F, " %s ", OBJ_GROUP(groupOut)->tables[i]->name);
							if (i != 0) {
								FPRINTF(F, " ON %s ", OBJ_GROUP(groupOut)->JoiningConditions[i - 1]);
							}
						}
						FPRINTF(F, "\");\n");
						FPRINTF(F, "		ADDQ(\");\");\n");

						//end of defining group out
					}

					//define mv_insert
					char *analyzedGroupJoinCondition;
					//and space before group join to replace correctly
					STR_INIT(analyzedGroupJoinCondition, " ");
					STR_APPEND(analyzedGroupJoinCondition, OBJ_SQ->groupJoinCondition);
					/*Group joining condition is in the format: <table of group 1>.col_x = <table of group 2>.col_y
					But we cant know which table belongs to which group
					-> need to analyzed to know which table is in which group and replace the table name by group name
					ex: group joining condition = 't2.c4 = t3.c5' , t2 in group 0 (currently is group in), t3 in group 1 (currently is group out)
					-> after analyzed: analyzedGroupJoinCondition = group_in_insert.c4 = group_out.c5
					*/
					tmp = " ";
					//and space before group join to use the getPrecededTableName
					STR_INIT(tmp, " ");
					STR_APPEND(tmp, OBJ_SQ->groupJoinCondition);

					colPos = "";
					while (colPos = findSubString(tmp, ".")) {
						char *tbName = getPrecededTableName(tmp, colPos + 1);
						//determine group of tbName
						for (int k = 0; k < OBJ_GROUP(groupIn)->tablesNum; k++) {
							if (STR_EQUAL_CI(OBJ_GROUP(groupIn)->tables[k]->name, trim(tbName))) {
								char *findWhat;
								STR_INIT(findWhat, " ");
								STR_APPEND(findWhat, tbName);
								STR_APPEND(findWhat, ".");
								char *replaceWith = " group_in_insert.";
								analyzedGroupJoinCondition = dammf_replaceAll(analyzedGroupJoinCondition, findWhat, replaceWith);
								//printf("analyzedGroupJoinCondition = %s\n", analyzedGroupJoinCondition);
							}
						}
						for (int k = 0; k < OBJ_GROUP(groupOut)->tablesNum; k++) {
							if (STR_EQUAL_CI(OBJ_GROUP(groupOut)->tables[k]->name, trim(tbName))) {
								char *findWhat;
								STR_INIT(findWhat, " ");
								STR_APPEND(findWhat, tbName);
								STR_APPEND(findWhat, ".");
								char *replaceWith = " group_out.";
								analyzedGroupJoinCondition = dammf_replaceAll(analyzedGroupJoinCondition, findWhat, replaceWith);
								//printf("analyzedGroupJoinCondition = %s\n", analyzedGroupJoinCondition);
							}
						}
						tmp = colPos + 1;
					}


					int colInMvInsert = OBJ_NSELECTED - 1; // dont count mvcount
														   /*we need to replace <table>.<col> in groupOutSelectColsList -> group_in_insert.col*/
					tmp = " ";
					//and space before group join to use the getPrecededTableName
					STR_INIT(tmp, " ");
					STR_APPEND(tmp, groupInSelectColsList);

					colPos = "";
					while (colPos = findSubString(tmp, ".")) {
						char *tbName = getPrecededTableName(tmp, colPos + 1);
						char *findWhat;
						STR_INIT(findWhat, " ");
						STR_APPEND(findWhat, tbName);
						STR_APPEND(findWhat, ".");
						groupInSelectColsList = replace_str(groupInSelectColsList, findWhat, " group_in_insert.");
						tmp = colPos + 1;
					}

					tmp = " ";
					STR_INIT(tmp, " ");
					STR_APPEND(tmp, groupOutSelectColsList);

					colPos = "";
					while (colPos = findSubString(tmp, ".")) {
						char *tbName = getPrecededTableName(tmp, colPos + 1);
						char *findWhat;
						STR_INIT(findWhat, " ");
						STR_APPEND(findWhat, tbName);
						STR_APPEND(findWhat, ".");
						groupOutSelectColsList = replace_str(groupOutSelectColsList, findWhat, " group_out.");
						tmp = colPos + 1;
					}
					FPRINTF(F, "		ADDQ(\"CREATE TEMP TABLE mv_insert AS( SELECT\");\n");
					if (hasGroupOut) {
						//because insert required the correct position of column 
						if (groupIn == 0) {
							FPRINTF(F, "		ADDQ(\" %s, %s \");\n", groupInSelectColsList, groupOutSelectColsList);
						}
						else {
							FPRINTF(F, "		ADDQ(\" %s, %s \");\n", groupOutSelectColsList, groupInSelectColsList);
						}
						FPRINTF(F, "		ADDQ(\" \");\n");
						//the order of 2 group in joining condition is also important

						if (groupIn == 0) {
							FPRINTF(F, "		ADDQ(\" FROM group_in_insert %s JOIN group_out ON %s \");\n", OBJ_SQ->groupJoinType, analyzedGroupJoinCondition);
						}
						else {
							FPRINTF(F, "		ADDQ(\" FROM group_out %s JOIN group_in_insert ON %s \");\n", OBJ_SQ->groupJoinType, analyzedGroupJoinCondition);
						}

						FPRINTF(F, "		ADDQ(\"WHERE true \");\n");
						for (int i = 0; i < OBJ_TABLE(j)->nCols; i++) {
							if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
								char *quote;
								char *strprefix;
								char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
								if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
								else { quote = ""; strprefix = "str_"; }
								FPRINTF(F, "		ADDQ(\" and group_in_insert.%s = %s\"); \n", OBJ_TABLE(j)->cols[i]->name, quote);
								FPRINTF(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
								FPRINTF(F, "		ADDQ(\"%s \"); \n", quote);
								FREE(ctype);
							}
						}
					}
					else {
						FPRINTF(F, "		ADDQ(\" %s FROM group_in_insert \");\n", groupInSelectColsList);
					}
					
					FPRINTF(F, "		ADDQ(\");\");\n");
					//BREAK;
					//end of defining mv_insert
					//define group_in_delete
					/*	Group in delete: only need when current table is comp or equal in group	*/
					//save the number of column in Group in delete in colInGroupInDelete var to use for DEFR() later
					int	colInGroupInDelete = 0;

					//CONDITION TO HAVE group_in_delete
					/*NEW VERSION OF ALGORITHM: group_in_delete is no longer need
					The following line of code is to disable it
					*/
					//roleInGroup = "disabled";
					if ((STR_EQUAL_CI(roleInGroup, "comp") || STR_EQUAL_CI(roleInGroup, "equal"))&&!OBJ_SQ->hasJoin) {
						counter = 0;
						//loop through selected col and just care about column of tables in group in, table <> current table

						for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
							if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
								//just take primary key 
								for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
									if (OBJ_GROUP(groupIn)->tables[i]->cols[k]->isPrimaryColumn) {
										colInGroupInDelete++;
									}
								}
							}
						}

						FPRINTF(F, "		ADDQ(\"CREATE TEMP TABLE group_in_delete AS( SELECT DISTINCT \");\n");
						FPRINTF(F, "		ADDQ(\"");

						for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
							if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
								//just take primary key 
								for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
									if (OBJ_GROUP(groupIn)->tables[i]->cols[k]->isPrimaryColumn) {
										counter++;
										FPRINTF(F, " %s%s ",
											OBJ_GROUP(groupIn)->tables[i]->cols[k]->name, (counter < colInGroupInDelete ? "," : " "));
									}
								}
							}
						}

						FPRINTF(F, "\");\n");
						FPRINTF(F, "		ADDQ(\" FROM mv_insert \");\n");
						FPRINTF(F, "		ADDQ(\");\");\n");
					}

					//end of defining group_in_delete

					//define mv_delete
					/* mv_delete is only declared when insert to comp or equal group */
					//CONDITION TO HAVE mv_delete
					int colInMvDelete = 0;
					if (STR_EQUAL_CI(OBJ_GROUP(groupIn)->RoleInOriginalQuery, "comp")
						|| STR_EQUAL_CI(OBJ_GROUP(groupIn)->RoleInOriginalQuery, "equal")) {
						//determine col in mv delete, use for DEFR() later
						for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
							for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
								if (OBJ_GROUP(groupOut)->tables[i]->cols[k]->isPrimaryColumn) {
									colInMvDelete++;
								}
							}
						}
						counter = 0;
						FPRINTF(F, "		ADDQ(\"CREATE TEMP TABLE mv_delete AS( SELECT DISTINCT \");\n");
						FPRINTF(F, "		ADDQ(\"");

						for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
							for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
								if (OBJ_GROUP(groupOut)->tables[i]->cols[k]->isPrimaryColumn) {
									counter++;
									FPRINTF(F, " %s%s ", OBJ_GROUP(groupOut)->tables[i]->cols[k]->name, (counter < colInMvDelete ? "," : " "));
								}
							}
						}

						FPRINTF(F, "\");\n");
						counter = 0;
						FPRINTF(F, "		ADDQ(\" FROM mv_insert WHERE \");\n");
						for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
							for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
								if (OBJ_GROUP(groupOut)->tables[i]->cols[k]->isPrimaryColumn) {
									FPRINTF(F, "		ADDQ(\" %s %s is not null \");\n", (counter > 0 ? "or" : " "), OBJ_GROUP(groupOut)->tables[i]->cols[k]->name);
									counter++;
								}
							}
						}


						FPRINTF(F, "		ADDQ(\");\");\n");
					}

					//end of defining mv_delete

					//STEP 1: SELECT FROM MV_INSERT, INSERT INTO MV
					FPRINTF(F, "		ADDQ(\"SELECT * FROM mv_insert; \");\n");
					FPRINTF(F, "		EXEC_;\n");
					FPRINTF(F, "		if(NO_ROW) {\n");
					FPRINTF(F, "			goto end_insert;\n");
					FPRINTF(F, "		}\n");
					FPRINTF(F, "		DEFR(%d);\n", colInMvInsert);
					//save mv insert to saved result set

					FPRINTF(F, "		FOR_EACH_RESULT_ROW(i){\n");
					for (i = 0; i < OBJ_NSELECTED - 1; i++) {
						if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
							FPRINTF(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
							FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
						else {
							FPRINTF(F, "			GET_NULLABLE_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i + 1);
							FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}
					}
					FPRINTF(F, "		}\n");

					FPRINTF(F, "\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED - 1);

					k = 0;
					for (i = 0; i < OBJ_NSELECTED - 1; i++)
						FPRINTF(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

					FPRINTF(F, "\n			DEFQ(\"INSERT INTO %s VALUES(\");\n", mvName);
					for (int i = 0; i < OBJ_NSELECTED - 1; i++) {
						char *quote;
						char *ctype;
						ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *")) quote = "\'";
						else quote = "";
						if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->table->name, OBJ_TABLE(j)->name)) {
							FPRINTF(F, "			ADDQ(\"%s\");\n", quote);
							FPRINTF(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							FPRINTF(F, "			ADDQ(\"%s\");\n", quote);
						}
						else {
							FPRINTF(F, "			ADDQ_NULL_OR_VALUE(%s, \"%s\");\n", OBJ_SELECTED_COL(i)->varName, quote);
						}
						FREE(ctype);
						if (i < OBJ_NSELECTED - 2) {
							FPRINTF(F, "			ADDQ(\",\");\n");
						}
					}
					FPRINTF(F, "			ADDQ(\");\");\n");
					FPRINTF(F, "\n			EXEC_; \n", OBJ_NSELECTED - 1);
					// end of FOR_EACH_SAVED_RESULT_ROW
					FPRINTF(F, "\n		} // end of FOR_EACH_SAVED_RESULT_ROW \n", OBJ_NSELECTED - 1);
					//END OF STEP 1

					//STEP 2: delete from mv1: if in group, role of current table is comp or equal, delete all records of type(main, main, NULL, NULL)
					//with main is from group_in_delete
					if ((STR_EQUAL_CI(roleInGroup, "comp") || STR_EQUAL_CI(roleInGroup, "equal"))&&!OBJ_SQ->hasJoin) {
						FPRINTF(F, "\n		DEFQ(\"SELECT * FROM group_in_delete; \");\n");
						FPRINTF(F, "		EXEC_;\n");

						FPRINTF(F, "		DEFR(%d);\n", colInGroupInDelete);

						FPRINTF(F, "		FOR_EACH_RESULT_ROW(i){\n");
						counter = 0;
						for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
							if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
								//just take primary key 
								for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
									if (OBJ_GROUP(groupIn)->tables[i]->cols[k]->isPrimaryColumn) {
										for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
											if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)) {
												FPRINTF(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(l)->varName, counter + 1);
												FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(l)->varName);
												counter++;
											}
										}
									}
								}
							}
						}

						FPRINTF(F, "		}\n");

						FPRINTF(F, "\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", colInGroupInDelete);

						counter = 0;
						for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
							if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
								//just take primary key 
								for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
									if (OBJ_GROUP(groupIn)->tables[i]->cols[k]->isPrimaryColumn) {
										for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
											if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)) {
												FPRINTF(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(l)->varName, counter++);
											}
										}
									}
								}
							}
						}

						FPRINTF(F, "\n			DEFQ(\"DELETE FROM %s WHERE true \");\n", mvName);

						for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
							if (STR_INEQUAL_CI(OBJ_GROUP(groupIn)->tables[i]->name, OBJ_TABLE(j)->name)) {
								//just take primary key 
								for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
									if (OBJ_GROUP(groupIn)->tables[i]->cols[k]->isPrimaryColumn) {
										for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
											if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupIn)->tables[i]->cols[k]->name)) {
												char *quote;
												char *ctype;
												ctype = createCType(OBJ_SELECTED_COL(l)->type);
												if (STR_EQUAL(ctype, "char *")) quote = "\'";
												else quote = "";
												FPRINTF(F, "			ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(l)->name, quote);
												FPRINTF(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(l)->varName);
												FPRINTF(F, "			ADDQ(\"%s\");\n", quote);
												FREE(ctype);
											}
										}
									}
								}
							}
						}

						//because primary key of table j is always join in select so dont need to loop through obj_select_col 
						for (int i = 0; i < OBJ_TABLE(j)->nCols; i++) {
							if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
								FPRINTF(F, "			ADDQ(\" and %s is null\");\n", OBJ_TABLE(j)->cols[i]->name);
							}
						}
						FPRINTF(F, "			EXEC_;\n");
						FPRINTF(F, "		}\n");

					}
					//END OF STEP 2
					//STEP 3: Delete from mv1 if current group role in query is comp or equal (from mv_delete)
					if (STR_EQUAL_CI(OBJ_GROUP(groupIn)->RoleInOriginalQuery, "comp")
						|| STR_EQUAL_CI(OBJ_GROUP(groupIn)->RoleInOriginalQuery, "equal")) {
						FPRINTF(F, "\n		DEFQ(\"SELECT * FROM mv_delete; \");\n");
						FPRINTF(F, "		EXEC_;\n");
						FPRINTF(F, "		DEFR(%d);\n", colInMvDelete);
						FPRINTF(F, "		FOR_EACH_RESULT_ROW(i){\n");
						counter = 0;
						for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
							//just take primary key 
							for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
								if (OBJ_GROUP(groupOut)->tables[i]->cols[k]->isPrimaryColumn) {
									for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
										if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)) {
											FPRINTF(F, "			GET_NULLABLE_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(l)->varName, counter + 1);
											FPRINTF(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(l)->varName);
											counter++;
										}
									}
								}
							}
						}
						FPRINTF(F, "		}\n");


						FPRINTF(F, "\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", colInMvDelete);

						counter = 0;
						for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
							//just take primary key 
							for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
								if (OBJ_GROUP(groupOut)->tables[i]->cols[k]->isPrimaryColumn) {
									for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
										if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)) {
											FPRINTF(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(l)->varName, counter++);
										}
									}
								}
							}
						}

						FPRINTF(F, "\n			DEFQ(\"DELETE FROM %s WHERE true \");\n", mvName);

						for (int i = 0; i < OBJ_GROUP(groupOut)->tablesNum; i++) {
							//just take primary key 
							for (int k = 0; k < OBJ_GROUP(groupOut)->tables[i]->nCols; k++) {
								if (OBJ_GROUP(groupOut)->tables[i]->cols[k]->isPrimaryColumn) {
									for (int l = 0; l < OBJ_NSELECTED - 1; l++) {
										if (STR_EQUAL_CI(OBJ_SELECTED_COL(l)->name, OBJ_GROUP(groupOut)->tables[i]->cols[k]->name)) {
											char *quote;
											char *ctype;
											ctype = createCType(OBJ_SELECTED_COL(l)->type);
											if (STR_EQUAL(ctype, "char *")) quote = "\'";
											else quote = "";
											FPRINTF(F, "			ADDQ(\" and %s \");\n", OBJ_SELECTED_COL(l)->name);
											FPRINTF(F, "			ADDQ_ISNULL_OR_VALUE(%s, \"%s\", \"=\");\n", OBJ_SELECTED_COL(l)->varName, quote);
											FREE(ctype);
										}
									}
								}
							}
						}

						//because primary key of table j is always join in select so dont need to loop through obj_select_col 
						for (int i = 0; i < OBJ_GROUP(groupIn)->tablesNum; i++) {
							for (int k = 0; k < OBJ_GROUP(groupIn)->tables[i]->nCols; k++) {
								if (OBJ_GROUP(groupIn)->tables[i]->cols[k]->isPrimaryColumn) {
									FPRINTF(F, "			ADDQ(\" and %s is null\");\n", OBJ_GROUP(groupIn)->tables[i]->cols[k]->name);
								}
							}
						}

						FPRINTF(F, "			EXEC_;\n");
						FPRINTF(F, "		}\n");

					}
					//END OF STEP 3

					// End of trigger
					FPRINTF(F, "\n	}\n\n	/*\n		Finish\n	*/\n");
					FPRINTF(F, "	end_insert: DEFQ(\"DROP TABLE IF EXISTS mv_insert, mv_delete, group_in_insert, group_in_delete, group_out;\"); EXEC_; \n	END;\n\n");

				}

				FPRINTF(F, "	}\n");

				FPRINTF(F, "}\n");
			
			} //END OF UPDATE EVENT
			for (i = 0; i < 2; i++)
				FREE_GROUP(OBJ_GROUP(i));
		} //END OF FOR EACH TABLE IN FROM ELEMENTS NUM

	}//END OF GENERATE C TRIGGER CODE


	/*--------------------------------------------- END OF REGION: OUTER JOIN -------------------------------------
														
	---------------------------------------------------------------------------------------------------------------
	*/
}

end_:
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


// Clear: OBJ_GROUP
//for (i = 0; i < 2; i++)
//	FREE_GROUP(OBJ_GROUP(i));

// Clear: OBJ_SQ
FREE_SELECT_QUERY;
printf("\n------------------- FINISH GENERATING CODE -----------------------\n");
PMVTG_END;
}
