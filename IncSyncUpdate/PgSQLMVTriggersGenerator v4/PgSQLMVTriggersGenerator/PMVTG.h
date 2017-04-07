#pragma once

#include <conio.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include "PMVTG_Boolean.h"
#include "PMVTG_Config.h"

struct s_SelectingQuery;
struct s_Table;
struct s_Column;

#define PREPARE_STATEMENT const char					*PREPARED_STATEMENT_connectionInfo;									\
						  PGconn						*PREPARED_STATEMENT_connection;										\
						  PGresult						*PREPARED_STATEMENT_queryResult;									\
						  char							PREPARED_STATEMENT_query[MAX_LENGTH_OF_QUERY];								\
						  struct s_SelectingQuery		*PREPARED_STATEMENT_selectingQuery;									\
						  Table							PREPARED_STATEMENT_table[MAX_NUMBER_OF_TABLES];								\
						  Column						PREPARED_STATEMENT_selectedColumn[MAX_NUMBER_OF_SELECT_ELEMENT];			\
						  int							PREPARED_STATEMENT_nSelectedCol = 0;								\
						  char							*PREPARED_STATEMENT_outConditions[MAX_NUMBER_OF_TABLES][MAX_NUMBER_OF_ELEMENTS];	\
						  int							PREPARED_STATEMENT_outConditionsElementNum[MAX_NUMBER_OF_TABLES];			\
						  char							*PREPARED_STATEMENT_inConditions[MAX_NUMBER_OF_TABLES][MAX_NUMBER_OF_ELEMENTS];		\
						  char							*PREPARED_STATEMENT_inConditionsOld[MAX_NUMBER_OF_TABLES][MAX_NUMBER_OF_ELEMENTS];	\
						  char							*PREPARED_STATEMENT_inConditionsNew[MAX_NUMBER_OF_TABLES][MAX_NUMBER_OF_ELEMENTS];	\
						  int							PREPARED_STATEMENT_inConditionsElementNum[MAX_NUMBER_OF_TABLES];			\
						  char							*PREPARED_STATEMENT_binConditions[MAX_NUMBER_OF_TABLES][MAX_NUMBER_OF_ELEMENTS];	\
						  int							PREPARED_STATEMENT_binConditionsElementNum[MAX_NUMBER_OF_TABLES];			\
						  char							*PREPARED_STATEMENT_firstCheckingCondition[MAX_NUMBER_OF_TABLES];			\
						  char							*PREPARED_STATEMENT_firstCheckingConditionOld[MAX_NUMBER_OF_TABLES];		\
						  char							*PREPARED_STATEMENT_firstCheckingConditionNew[MAX_NUMBER_OF_TABLES];		\
						  char							*PREPARED_STATEMENT_selectClause = NULL;							\
						  Boolean						PREPARED_STATEMENT_hasAggFunc = FALSE;								\
						  Boolean						PREPARED_STATEMENT_hasAggMinMax = FALSE;							\
						  Column						PREPARED_STATEMENT_TGColumns[MAX_NUMBER_OF_TABLES][MAX_NUMBER_OF_ELEMENTS];			\
						  int							PREPARED_STATEMENT_nTGColumns[MAX_NUMBER_OF_TABLES];						\
						  Boolean						PREPARED_STATEMENT_hasOnlyAggSumCount = FALSE;						\
						  char							*PREPARED_STATEMENT_A2WhereClause[MAX_NUMBER_OF_TABLES];					\
						  Boolean						PREPARED_STATEMENT_A2AggInvolvedCheck[MAX_NUMBER_OF_TABLES];				\
						  Boolean						PREPARED_STATEMENT_keyInGCheck[MAX_NUMBER_OF_TABLES];						\
						  char							*PREPARED_STATEMENT_A2TGTables[MAX_NUMBER_OF_TABLES][MAX_NUMBER_OF_ELEMENTS];		\
						  int							PREPARED_STATEMENT_A2TGNTables[MAX_NUMBER_OF_TABLES]

#define OBJ_SQ									PREPARED_STATEMENT_selectingQuery
#define OBJ_FCC(table)							PREPARED_STATEMENT_firstCheckingCondition[table]

// For a2 algorithm
#define OBJ_FCC_OLD(table)						PREPARED_STATEMENT_firstCheckingConditionOld[table]
#define OBJ_FCC_NEW(table)						PREPARED_STATEMENT_firstCheckingConditionNew[table]
#define OBJ_IN_CONDITIONS_OLD(table, x)			PREPARED_STATEMENT_inConditionsOld[table][x]
#define OBJ_IN_CONDITIONS_NEW(table, x)			PREPARED_STATEMENT_inConditionsNew[table][x]

#define OBJ_TABLE(i)							PREPARED_STATEMENT_table[i]
#define OBJ_SELECTED_COL(i)						PREPARED_STATEMENT_selectedColumn[i]
#define OBJ_OUT_CONDITIONS(table, x)			PREPARED_STATEMENT_outConditions[table][x]
#define OBJ_IN_CONDITIONS(table, x)				PREPARED_STATEMENT_inConditions[table][x]
#define OBJ_BIN_CONDITIONS(table, x)			PREPARED_STATEMENT_binConditions[table][x]
#define OBJ_SELECT_CLAUSE						PREPARED_STATEMENT_selectClause
#define OBJ_STATIC_BOOL_AGG						PREPARED_STATEMENT_hasAggFunc
#define OBJ_STATIC_BOOL_AGG_MINMAX				PREPARED_STATEMENT_hasAggMinMax
// 'delMinMaxWhere' of old version, table column appear in group by, need fix, not correct
// a2tgtable(lop) = khoa
#define OBJ_A2_TG_TABLE(table, x)				PREPARED_STATEMENT_A2TGTables[table][x]
#define OBJ_A2_TG_NTABLES(table)				PREPARED_STATEMENT_A2TGNTables[table]
#define OBJ_TG_COL(table, x)					PREPARED_STATEMENT_TGColumns[table][x]
#define OBJ_TG_NCOLS(table)						PREPARED_STATEMENT_nTGColumns[table]
#define OBJ_A2_CHECK							PREPARED_STATEMENT_hasOnlyAggSumCount
#define OBJ_A2_WHERE_CLAUSE(table)				PREPARED_STATEMENT_A2WhereClause[table]
#define OBJ_AGG_INVOLVED_CHECK(table)			PREPARED_STATEMENT_A2AggInvolvedCheck[table]
#define OBJ_KEY_IN_G_CHECK(table)				PREPARED_STATEMENT_keyInGCheck[table]

#define OBJ_BIN_NCONDITIONS(table)				(PREPARED_STATEMENT_binConditionsElementNum[table])
#define OBJ_IN_NCONDITIONS(table)				(PREPARED_STATEMENT_inConditionsElementNum[table])
#define OBJ_OUT_NCONDITIONS(table)				(PREPARED_STATEMENT_outConditionsElementNum[table])
#define OBJ_NSELECTED							PREPARED_STATEMENT_nSelectedCol

#define ANALYZE_SELECT_QUERY					OBJ_SQ = analyzeSelectingQuery(PREPARED_STATEMENT_query)
#define FREE_SELECT_QUERY						freeSelectingQuery(&OBJ_SQ)
#define FREE_TABLE(table)						delTable(&table);
#define FREE_COL(col)							delColumn(&col);

#define ADD_OBJ_OUT_CONDITION(table, X)			OBJ_OUT_CONDITIONS(table, OBJ_OUT_NCONDITIONS(table)++) = X

#define ADD_OBJ_IN_CONDITION_OLD(table, X)		OBJ_IN_CONDITIONS_OLD(table, OBJ_IN_NCONDITIONS(table)) = X
#define ADD_OBJ_IN_CONDITION_NEW(table, X)		OBJ_IN_CONDITIONS_NEW(table, OBJ_IN_NCONDITIONS(table)) = X
#define ADD_OBJ_IN_CONDITION(table, X)			OBJ_IN_CONDITIONS(table, OBJ_IN_NCONDITIONS(table)++) = X

#define ADD_OBJ_BIN_CONDITION(table, X)			OBJ_BIN_CONDITIONS(table, OBJ_BIN_NCONDITIONS(table)++) = X

#define INPUT_SELECT_QUERY						gets(PREPARED_STATEMENT_query)
#define F										cWriter
#define SQL										sqlWriter
//T: The purpose of the definition of PRINT_TO_FILE is to distinguish "fprintf" with "printf" which frequently used for debug
#define PRINT_TO_FILE							fprintf

#define MAIN PREPARE_STATEMENT; \
			 int main(int argc, char **argv)

#define DEFINE_CONNECTION(connectionString) PREPARED_STATEMENT_connectionInfo = connectionString

#define CONNECT PREPARED_STATEMENT_connection = PQconnectdb(PREPARED_STATEMENT_connectionInfo)
#define DISCONNECT PQfinish(PREPARED_STATEMENT_connection)

#define CONNECTION_STATUS PQstatus(PREPARED_STATEMENT_connection)
#define IS_CONNECTED PQstatus(PREPARED_STATEMENT_connection) == 0 // 'ConnStatusType::CONNECTION_OK' ~ 'CONNECTION_OK' MACRO

// Should be put in a block
#define ERR(msg) fprintf(stderr, "%s: ", msg);				  \
				 fprintf(stderr, "%s\n", PQerrorMessage(PREPARED_STATEMENT_connection)); \
				 stdExit(PREPARED_STATEMENT_connection);

#define EXEC PREPARED_STATEMENT_queryResult = PQexec(PREPARED_STATEMENT_connection, PREPARED_STATEMENT_query)
#define STATUS PQresultStatus(PREPARED_STATEMENT_queryResult)
#define CLEAR PQclear(PREPARED_STATEMENT_queryResult)

#define GET_FIELDS_NUM(n) n = PQnfields(PREPARED_STATEMENT_queryResult)
#define FIELDS_NUM PQnfields(PREPARED_STATEMENT_queryResult)

#define GET_FIELD_NAME(str, i) str = PQfname(PREPARED_STATEMENT_queryResult, i)
#define FIELD_NAME(i) PQfname(PREPARED_STATEMENT_queryResult, i)

#define GET_TUPLES_NUM(n) n = PQntuples(PREPARED_STATEMENT_queryResult)
#define TUPLES_NUM PQntuples(PREPARED_STATEMENT_queryResult)

#define GET_VALUE_AT(str, row, col) str = PQgetvalue(PREPARED_STATEMENT_queryResult, row, col)
#define VALUE_AT(row, col) PQgetvalue(PREPARED_STATEMENT_queryResult, row, col)

#define PMVTG_END _getch(); \
				  return 0

#define SYSTEM_EXEC(cmd) system(cmd)

#define INT32_TO_STR(i32, s) sprintf(s, "%d", i32)
#define DEFQ(c) strcpy(PREPARED_STATEMENT_query, c)
#define ADDQ(c) strcat(PREPARED_STATEMENT_query, c)

int strLenTarsPuts;
#define TARS_PUTS(str) strLenTarsPuts = strlen(str); for(i = 0; i <strLenTarsPuts; i++) {printf("%c", str[i]); Sleep(10);} printf("\n")

char *getPrecededTableName(char* wholeExpression, char* colPos);
void stdExit(PGconn*);
char *createVarName(const struct s_Column* column);
char *createTypePrefix(const char *SQLTypeName);
char *createCType(const char *SQLTypeName);

Boolean addRealColumn(char *selectedName, char *originalColName);
char* addFunction(char *selectedName, char *remainder, int funcNum, char **funcList, Boolean isAggFunc);
void filterTriggerCondition(char* condition, int table);
Boolean hasInCol(char* condition, int table);
Boolean hasOutCol(char *condition, int table);
void A2Split(int table, char* expression, int *nParts, int *partsType, char **parts);
char *a2FCCRefactor(char* originalFCC, char *prefix);

//----
char* ConditionCToSQL(char* conditionC);

char *ReplaceCharacter(const char *s, char h, const char *repl);
//--outer join
void GenReQueryOuterJoinWithPrimaryKey(int table, FILE *f, char *selectClause);
void GenCodeInsertForComplementTable(int table, FILE *f, char *mvName, char tab);