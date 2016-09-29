#include <libpq-fe.h>
#include <stdlib.h>
#include "MVIAUTG_Query.h"

#define ESTIMATED_LENGTH 100
#define PORT_LENGTH 10
#define MASTER_QUERY_LENGTH 1000

#define I _MVIAU_QueryResultBigIntIterator
#define J _MVIAU_TableIntIterator

#define K _MVIAU_ColumnIntIterator
#define M _MVIAU_1stMultiPurposeIterator
#define N _MVIAU_2ndMultiPurposeIterator

#define MAIN BEGIN;																																\
			 int main(int argc, char **argv)

#define BEGIN																																	\
																char		_MVIAU_Database				[ESTIMATED_LENGTH];						\
																char		_MVIAU_Username				[ESTIMATED_LENGTH];						\
																char		_MVIAU_Password				[ESTIMATED_LENGTH];						\
																char		_MVIAU_Port					[PORT_LENGTH];							\
																char		_MVIAU_MVName				[ESTIMATED_LENGTH];						\
																char		_MVIAU_PostgreSQLPath		[ESTIMATED_LENGTH];						\
																char		_MVIAU_MasterQuery			[MAX_L_QUERY];							\
													  SelectingQuery		_MVIAU_SelectingQuery		= NULL;									\
															    Table		_MVIAU_Tables				[MAX_N_TABLES];							\
															    Column		_MVIAU_SelectedColumn		[MAX_N_SELECT_ELEMENT];					\
																char		*_MVIAU_FirstCheckingCondition[MAX_N_TABLES];						\
																int			_MVIAU_NSelectedCol			= 0;									\
																char		*_MVIAU_OutConditions		[MAX_N_TABLES][MAX_N_ELEMENTS];			\
																int			_MVIAU_OutConditionsElementNum[MAX_N_TABLES];						\
																char		*_MVIAU_InConditions		[MAX_N_TABLES][MAX_N_ELEMENTS];			\
																int			_MVIAU_InConditionsElementNum[MAX_N_TABLES];						\
																char		*_MVIAU_BinConditions		[MAX_N_TABLES][MAX_N_ELEMENTS];			\
																int			_MVIAU_BinConditionsElementNum[MAX_N_TABLES];						\
																char		*_MVIAU_selectClause		= NULL;									\
																Boolean		_MVIAU_hasAggFunc			= FALSE;								\
																Boolean		_MVIAU_hasAggMinMax			= FALSE;								\
																Column		_MVIAU_TGColumns			[MAX_N_TABLES][MAX_N_ELEMENTS];			\
																int			_MVIAU_nTGColumns			[MAX_N_TABLES];							\
																Boolean		_MVIAU_hasOnlyAggSumCount	= FALSE;								\
																char		*_MVIAU_A2WhereClause		[MAX_N_TABLES];							\
																Boolean		_MVIAU_A2AggInvolvedCheck	[MAX_N_TABLES];							\
																Boolean		_MVIAU_keyInGCheck			[MAX_N_TABLES];							\
																char		*_MVIAU_A2TGTables			[MAX_N_TABLES][MAX_N_ELEMENTS];			\
																int			_MVIAU_A2TGNTables			[MAX_N_TABLES];							\
																char		*_MVIAU_ConnectionString	= NULL;									\
																PGconn		*_MVIAU_Connection			= NULL;									\
																char        *_MVIAU_Query				= NULL;									\
																PGresult	*_MVIAU_QueryResult			= NULL;									\
																PGresult    *requeryResult;														\
																long long   I;																	\
																int			J, K, M, N																

#define INPUT																																	\
scanf("%s%s%s%s%s\n", _MVIAU_Database, _MVIAU_Username, _MVIAU_Password, _MVIAU_Port, _MVIAU_MVName);											\
gets(_MVIAU_PostgreSQLPath);																													\
gets(_MVIAU_MasterQuery)


#define CONNECT																																	\
																FREE(_MVIAU_ConnectionString);													\
																STR_INIT(_MVIAU_ConnectionString, "dbname = '");								\
																STR_APPEND(_MVIAU_ConnectionString, _MVIAU_Database);							\
																STR_APPEND(_MVIAU_ConnectionString, "' user = '");								\
																STR_APPEND(_MVIAU_ConnectionString, _MVIAU_Username);							\
																STR_APPEND(_MVIAU_ConnectionString, "' password = '");							\
																STR_APPEND(_MVIAU_ConnectionString, _MVIAU_Password);							\
																STR_APPEND(_MVIAU_ConnectionString, "' port = ");								\
																STR_APPEND(_MVIAU_ConnectionString, _MVIAU_Port);								\
																_MVIAU_Connection = PQconnectdb(_MVIAU_ConnectionString)

#define IS_CONNECTED											(PQstatus(_MVIAU_Connection) == 0)

#define ERR_MESSAGE												PQerrorMessage(_MVIAU_Connection)

#define DISCONNECT												PQfinish(_MVIAU_Connection)

#define QUERY(query_initialization)								FREE(_MVIAU_Query);																\
																STR_INIT(_MVIAU_Query, query_initialization)

#define _QUERY(query_appendant)									STR_APPEND(_MVIAU_Query, query_appendant)

#define EXECUTE																																	\
																PQclear(_MVIAU_QueryResult);													\
																_MVIAU_QueryResult = PQexec(_MVIAU_Connection, _MVIAU_Query);					\
																I = 0

#define RESULT(resultSet)										PGresult* resultSet = NULL
#define FREE_RESULT(resultSet)									PQclear(resultSet)

#define EXECUTE_TO(resultSet)									resultSet = PQexec(_MVIAU_Connection, _MVIAU_Query)

#define SYSTEM_EXECUTE											system(_MVIAU_Query)

#define STATUS																																	\
																PQresultStatus(_MVIAU_QueryResult)

#define STATUS_ON(resultSet)																													\
																PQresultStatus(resultSet)

#define NROWS																																	\
																PQntuples(_MVIAU_QueryResult)

#define NROWS_ON(resultSet)										PQntuples(resultSet)

#define DATA(col)																																\
																PQgetvalue(_MVIAU_QueryResult, I, col)

#define DATA_ON(resultSet, row, col)							PQgetvalue(resultSet, row, col)

#define FOREACH_ROW																																\
																for (I = 0; I < NROWS; I++)

#define FOR(iterator, resultSet)																												\
																for (iterator = 0; iterator < NROWS_ON(resultSet); iterator++)


#define FOREACH_TABLE																															\
																for (J = 0; J < OBJ_SQ->fromElementsNum; J++)


#define QUERY_EXECUTED																															\
																(STATUS == PGRES_COMMAND_OK || STATUS == PGRES_TUPLES_OK)

#define QUERY_EXECUTED_ON(resultSet)																											\
																(STATUS_ON(resultSet) == PGRES_COMMAND_OK || STATUS_ON(resultSet) == PGRES_TUPLES_OK)

#define FREE_MASTER_QUERY																														\
																freeSelectingQuery(&OBJ_SQ)


#define OBJ_SQ									_MVIAU_SelectingQuery
#define OBJ_TABLE(table)						_MVIAU_Tables[table]
#define OBJ_SELECTED_COL(i)						_MVIAU_SelectedColumn[i]
#define OBJ_OUT_CONDITIONS(table, x)			_MVIAU_OutConditions[table][x]
#define OBJ_IN_CONDITIONS(table, x)				_MVIAU_InConditions[table][x]
#define OBJ_BIN_CONDITIONS(table, x)			_MVIAU_BinConditions[table][x]
#define OBJ_FCC(table)							_MVIAU_FirstCheckingCondition[table]

#define OBJ_BIN_NCONDITIONS(table)				_MVIAU_BinConditionsElementNum[table]
#define OBJ_IN_NCONDITIONS(table)				_MVIAU_InConditionsElementNum[table]
#define OBJ_OUT_NCONDITIONS(table)				_MVIAU_OutConditionsElementNum[table]
#define OBJ_NSELECTED							_MVIAU_NSelectedCol

#define ADD_OBJ_OUT_CONDITION(table, X)			OBJ_OUT_CONDITIONS(table, OBJ_OUT_NCONDITIONS(table)++) = X
#define ADD_OBJ_IN_CONDITION(table, X)			OBJ_IN_CONDITIONS(table, OBJ_IN_NCONDITIONS(table)++) = X
#define ADD_OBJ_BIN_CONDITION(table, X)			OBJ_BIN_CONDITIONS(table, OBJ_BIN_NCONDITIONS(table)++) = X

#define OBJ_SELECT_CLAUSE						_MVIAU_selectClause
#define OBJ_STATIC_BOOL_AGG						_MVIAU_hasAggFunc
#define OBJ_STATIC_BOOL_AGG_MINMAX				_MVIAU_hasAggMinMax

// a2tgtable(lop) = khoa, t = table, g = group by?
#define OBJ_A2_TG_TABLE(table, x)				_MVIAU_A2TGTables[table][x]
#define OBJ_A2_TG_NTABLES(table)				_MVIAU_A2TGNTables[table]
#define OBJ_TG_COL(table, x)					_MVIAU_TGColumns[table][x]
#define OBJ_TG_NCOLS(table)						_MVIAU_nTGColumns[table]
#define OBJ_A2_CHECK							_MVIAU_hasOnlyAggSumCount
#define OBJ_A2_WHERE_CLAUSE(table)				_MVIAU_A2WhereClause[table]
#define OBJ_AGG_INVOLVED_CHECK(table)			_MVIAU_A2AggInvolvedCheck[table]
#define OBJ_KEY_IN_G_CHECK(table)				_MVIAU_keyInGCheck[table]

#define ANALYZE_MASTER_QUERY OBJ_SQ = analyzeSelectingQuery(_MVIAU_MasterQuery)

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

#define INT32_TO_STR(i32, s) sprintf(s, "%d", i32)

#define END																																		\
																FREE(_MVIAU_ConnectionString);													\
																FREE(_MVIAU_Query);																\
																PQclear(_MVIAU_QueryResult);													\
																FOREACH_TABLE {																	\
																	for (M = 0; M < OBJ_BIN_NCONDITIONS(J); M++)								\
																		FREE(OBJ_BIN_CONDITIONS(J, M));											\
																	for (M = 0; M < OBJ_OUT_NCONDITIONS(J); M++)								\
																		FREE(OBJ_OUT_CONDITIONS(J, M));											\
																	for (M = 0; M < OBJ_IN_NCONDITIONS(J); M++)									\
																		FREE(OBJ_IN_CONDITIONS(J, M));											\
																	for (M = 0; M < OBJ_A2_TG_NTABLES(J); M++)									\
																		FREE(OBJ_A2_TG_TABLE(J, M));											\
																	if (TRUE) FREE(OBJ_A2_WHERE_CLAUSE(J));								\
																}																				\
																for (K = 0; K < OBJ_NSELECTED; K++) delColumn(&OBJ_SELECTED_COL(K));			\
																FOREACH_TABLE {FREE(OBJ_FCC(J));}												\
																FOREACH_TABLE {delTable(&OBJ_TABLE(J));}										\
																freeSelectingQuery(&OBJ_SQ);													\
																FREE(OBJ_SELECT_CLAUSE);														\
																FREE_RESULT(requeryResult);														\
																DISCONNECT;																		\
																return




		