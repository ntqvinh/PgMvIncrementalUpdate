#include <libpq-fe.h>
#include <stdlib.h>
#include "MVIAUTG_Query.h"

#define ESTIMATED_LENGTH 100
#define PORT_LENGTH 10
#define MASTER_QUERY_LENGTH 1000

#define I _MVIAUTG_QueryResultBigIntIterator
#define J _MVIAUTG_TableIntIterator

#define K _MVIAUTG_ColumnIntIterator
#define M _MVIAUTG_1stMultiPurposeIterator
#define N _MVIAUTG_2ndMultiPurposeIterator

#define C _MVIAUTG_CWriter
#define SQL _MVIAUTG_SQLWriter
#define INF _MVIAUTG_INFWriter

#define MAIN BEGIN;																																\
			 int main(int argc, char **argv)

#define BEGIN																																	\
																char		_MVIAUTG_Database			[ESTIMATED_LENGTH];						\
																char		_MVIAUTG_Username			[ESTIMATED_LENGTH];						\
																char		_MVIAUTG_Password			[ESTIMATED_LENGTH];						\
																char		_MVIAUTG_Port				[PORT_LENGTH];							\
																char		_MVIAUTG_MasterQuery		[MASTER_QUERY_LENGTH];					\
																char		_MVIAUTG_MVName				[ESTIMATED_LENGTH];						\
																char		_MVIAUTG_OutputPath			[ESTIMATED_LENGTH];						\
																char		_MVIAUTG_PostgreSQLPath		[ESTIMATED_LENGTH];						\
													  SelectingQuery		_MVIAUTG_SelectingQuery		= NULL;									\
															    Table		_MVIAUTG_Tables				[MAX_N_TABLES];							\
															    Column		_MVIAUTG_SelectedColumn		[MAX_N_SELECT_ELEMENT];					\
																char		*_MVIAUTG_FirstCheckingCondition[MAX_N_TABLES];						\
																int			_MVIAUTG_NSelectedCol		= 0;									\
																char		*_MVIAUTG_OutConditions		[MAX_N_TABLES][MAX_N_ELEMENTS];			\
																int			_MVIAUTG_OutConditionsElementNum[MAX_N_TABLES];						\
																char		*_MVIAUTG_InConditions		[MAX_N_TABLES][MAX_N_ELEMENTS];			\
																int			_MVIAUTG_InConditionsElementNum[MAX_N_TABLES];						\
																char		*_MVIAUTG_BinConditions		[MAX_N_TABLES][MAX_N_ELEMENTS];			\
																int			_MVIAUTG_BinConditionsElementNum[MAX_N_TABLES];						\
																char		*_MVIAUTG_ConnectionString  = NULL;									\
																PGconn		*_MVIAUTG_Connection		= NULL;									\
																char        *_MVIAUTG_Query				= NULL;									\
																PGresult	*_MVIAUTG_QueryResult		= NULL;									\
																FILE		*C = NULL, *SQL = NULL, *INF = NULL;								\
																char		*_MVIAUTG_CFilePath;												\
																char		*_MVIAUTG_SQLFilePath;												\
																long long   I;																	\
																int			J, K, M, N																

#define INPUT																																	\
gets(_MVIAUTG_MasterQuery);																														\
scanf("%s%s%s%s%s\n", _MVIAUTG_Database, _MVIAUTG_Username, _MVIAUTG_Password, _MVIAUTG_Port, _MVIAUTG_MVName);									\
gets(_MVIAUTG_OutputPath);																														\
gets(_MVIAUTG_PostgreSQLPath);																													\
STR_INIT(_MVIAUTG_CFilePath, _MVIAUTG_OutputPath); STR_APPEND(_MVIAUTG_CFilePath, "\\");														\
STR_APPEND(_MVIAUTG_CFilePath, _MVIAUTG_MVName); STR_APPEND(_MVIAUTG_CFilePath, "_triggersrc.c");												\
C = fopen(_MVIAUTG_CFilePath, "w");	FREE(_MVIAUTG_CFilePath);																					\
STR_INIT(_MVIAUTG_SQLFilePath, _MVIAUTG_OutputPath); STR_APPEND(_MVIAUTG_SQLFilePath, "\\");													\
STR_APPEND(_MVIAUTG_SQLFilePath, _MVIAUTG_MVName); STR_APPEND(_MVIAUTG_SQLFilePath, "_mvsrc.sql");												\
SQL = fopen(_MVIAUTG_SQLFilePath, "w"); FREE(_MVIAUTG_SQLFilePath)

#define CONNECT																																	\
																FREE(_MVIAUTG_ConnectionString);												\
																STR_INIT(_MVIAUTG_ConnectionString, "dbname = '");								\
																STR_APPEND(_MVIAUTG_ConnectionString, _MVIAUTG_Database);						\
																STR_APPEND(_MVIAUTG_ConnectionString, "' user = '");							\
																STR_APPEND(_MVIAUTG_ConnectionString, _MVIAUTG_Username);						\
																STR_APPEND(_MVIAUTG_ConnectionString, "' password = '");						\
																STR_APPEND(_MVIAUTG_ConnectionString, _MVIAUTG_Password);						\
																STR_APPEND(_MVIAUTG_ConnectionString, "' port = ");								\
																STR_APPEND(_MVIAUTG_ConnectionString, _MVIAUTG_Port);							\
																_MVIAUTG_Connection = PQconnectdb(_MVIAUTG_ConnectionString)

#define IS_CONNECTED																															\
																(PQstatus(_MVIAUTG_Connection) == 0)

#define ERR_MESSAGE																																\
																PQerrorMessage(_MVIAUTG_Connection)

#define DISCONNECT																																\
																PQfinish(_MVIAUTG_Connection)

#define QUERY(query_initialization)																												\
																FREE(_MVIAUTG_Query);															\
																STR_INIT(_MVIAUTG_Query, query_initialization)

#define _QUERY(query_appendant)																													\
																STR_APPEND(_MVIAUTG_Query, query_appendant)

#define EXECUTE																																	\
																PQclear(_MVIAUTG_QueryResult);													\
																_MVIAUTG_QueryResult = PQexec(_MVIAUTG_Connection, _MVIAUTG_Query);				\
																I = 0

#define SYSTEM_EXECUTE											system(_MVIAUTG_Query)

#define STATUS																																	\
																PQresultStatus(_MVIAUTG_QueryResult)

#define NROWS																																	\
																PQntuples(_MVIAUTG_QueryResult)

#define DATA(col)																																\
																PQgetvalue(_MVIAUTG_QueryResult, I, col)

#define FOREACH_ROW																																\
																for (I = 0; I < NROWS; I++)

#define FOREACH_TABLE																															\
																for (J = 0; J < OBJ_SQ->fromElementsNum; J++)


#define QUERY_EXECUTED																															\
																(STATUS == PGRES_COMMAND_OK || STATUS == PGRES_TUPLES_OK)

#define FREE_MASTER_QUERY																														\
																freeSelectingQuery(&OBJ_SQ)


#define OBJ_SQ									_MVIAUTG_SelectingQuery
#define OBJ_TABLE(table)						_MVIAUTG_Tables[table]
#define OBJ_SELECTED_COL(i)						_MVIAUTG_SelectedColumn[i]
#define OBJ_OUT_CONDITIONS(table, x)			_MVIAUTG_OutConditions[table][x]
#define OBJ_IN_CONDITIONS(table, x)				_MVIAUTG_InConditions[table][x]
#define OBJ_BIN_CONDITIONS(table, x)			_MVIAUTG_BinConditions[table][x]
#define OBJ_FCC(table)							_MVIAUTG_FirstCheckingCondition[table]

#define OBJ_BIN_NCONDITIONS(table)				_MVIAUTG_BinConditionsElementNum[table]
#define OBJ_IN_NCONDITIONS(table)				_MVIAUTG_InConditionsElementNum[table]
#define OBJ_OUT_NCONDITIONS(table)				_MVIAUTG_OutConditionsElementNum[table]
#define OBJ_NSELECTED							_MVIAUTG_NSelectedCol

#define ADD_OBJ_OUT_CONDITION(table, X)			OBJ_OUT_CONDITIONS(table, OBJ_OUT_NCONDITIONS(table)++) = X
#define ADD_OBJ_IN_CONDITION(table, X)			OBJ_IN_CONDITIONS(table, OBJ_IN_NCONDITIONS(table)++) = X
#define ADD_OBJ_BIN_CONDITION(table, X)			OBJ_BIN_CONDITIONS(table, OBJ_BIN_NCONDITIONS(table)++) = X

#define ANALYZE_MASTER_QUERY OBJ_SQ = analyzeSelectingQuery(_MVIAUTG_MasterQuery)

char *createVarName(const struct s_Column* column);
char *createTypePrefix(const char *SQLTypeName);
char *createCType(const char *SQLTypeName);
char *createSQLType(const struct s_Column* column);

char *getPrecededTableName(char* wholeExpression, char* colPos);
Boolean addRealColumn(char *selectedName, char *originalColName);
char* addFunction(char *selectedName, char *remainder, int funcNum, char **funcList, Boolean isAggFunc);
void filterTriggerCondition(char* condition, int table);
Boolean hasInCol(char* condition, int table);
Boolean hasOutCol(char *condition, int table);
void A2Split(int table, char* expression, int *nParts, int *partsType, char **parts);

#define END																																		\
																FREE(_MVIAUTG_ConnectionString);												\
																FREE(_MVIAUTG_Query);															\
																PQclear(_MVIAUTG_QueryResult);													\
																FOREACH_TABLE {																	\
																	for (M = 0; M < OBJ_BIN_NCONDITIONS(J); M++)								\
																		FREE(OBJ_BIN_CONDITIONS(J, M));											\
																	for (M = 0; M < OBJ_OUT_NCONDITIONS(J); M++)								\
																		FREE(OBJ_OUT_CONDITIONS(J, M));											\
																	for (M = 0; M < OBJ_IN_NCONDITIONS(J); M++)									\
																		FREE(OBJ_IN_CONDITIONS(J, M));											\
																}																				\
																for (K = 0; K < OBJ_NSELECTED; K++) delColumn(&OBJ_SELECTED_COL(K));			\
																FOREACH_TABLE {FREE(OBJ_FCC(J));}												\
																FOREACH_TABLE {delTable(&OBJ_TABLE(J));}										\
																FREE_MASTER_QUERY;																\
																fclose(C);																		\
																fclose(SQL);																	\
																DISCONNECT;																		\
																return




		