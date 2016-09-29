// For standard input, output & memory allocating
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>

// For random function
#include <time.h>

// For file processing
#include <windows.h>

// For memory leak detector
//#include <vld.h>

//
#include "MVIAUTG_Boolean.h"
#include "MVIAUTG_CString.h"
#include "mviau.h"
#include "Log.h"

MAIN {
	//char *masterFileName;
	//char *masterFilePath;
	char *startingPoint;
	//FILE *masterFile;

	INPUT;

	/*STR_INIT(masterFileName, _MVIAU_MVName);
	STR_APPEND(masterFileName, ".");
	STR_APPEND(masterFileName, _MVIAU_Database);
	STR_APPEND(masterFileName, ".mviautg");

	STR_INIT(masterFilePath, _MVIAU_PostgreSQLPath);
	STR_APPEND(masterFilePath, "\\mviautg\\");
	STR_APPEND(masterFilePath, masterFileName);

	printf("%s\n", masterFilePath);
	masterFile = fopen(masterFilePath, "r");

	// GET INFO FROM FILE
	{
	}*/

	OBJ_SQ = analyzeSelectingQuery(_MVIAU_MasterQuery);

	if (OBJ_SQ) {
		printf(" > Analyze query successfully!\n");
	} else {
		printf(" > Failed to analyze master query!\n");
		END;
	}

	CONNECT;
	if (IS_CONNECTED) {
		printf("Connected to database!\n");
	} else {
		printf("Connected to database failed!\n");
	}

	// 2. Advanced analyzation (get primary key, condition, ...)
	FOREACH_TABLE {
		initTable(&OBJ_TABLE(J));
		OBJ_TABLE(J)->name = copy(OBJ_SQ->fromElements[J]);

		// Create columns
		QUERY("SELECT column_name, data_type, ordinal_position, character_maximum_length, is_nullable FROM information_schema.columns WHERE table_name = '");
		_QUERY(OBJ_TABLE(J)->name);
		_QUERY("';");
		EXECUTE;
		if (!QUERY_EXECUTED) {
			printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
		}
		FOREACH_ROW {
			initColumn(&OBJ_TABLE(J)->cols[OBJ_TABLE(J)->nCols]);

			OBJ_TABLE(J)->cols[OBJ_TABLE(J)->nCols]->context = COLUMN_CONTEXT_TABLE_WIRED;
			OBJ_TABLE(J)->cols[OBJ_TABLE(J)->nCols]->table = OBJ_TABLE(J);
			OBJ_TABLE(J)->cols[OBJ_TABLE(J)->nCols]->name = copy(DATA(0));
			OBJ_TABLE(J)->cols[OBJ_TABLE(J)->nCols]->type = copy(DATA(1));
			OBJ_TABLE(J)->cols[OBJ_TABLE(J)->nCols]->ordinalPosition = atoi(DATA(2));
			OBJ_TABLE(J)->cols[OBJ_TABLE(J)->nCols]->characterLength = copy(DATA(3));
			OBJ_TABLE(J)->cols[OBJ_TABLE(J)->nCols]->isNullable = STR_EQUAL_CI(DATA(4), "true");
			OBJ_TABLE(J)->cols[OBJ_TABLE(J)->nCols]->varName = createVarName(OBJ_TABLE(J)->cols[OBJ_TABLE(J)->nCols]);

			OBJ_TABLE(J)->nCols++;			
		}
	}

	// Set tables' primary key
	FOREACH_TABLE {
		QUERY("SELECT pg_attribute.attname FROM pg_index, pg_class, pg_attribute WHERE pg_class.oid = '\"");
		_QUERY(OBJ_TABLE(J)->name);
		_QUERY("\"'::regclass AND indrelid = pg_class.oid AND pg_attribute.attrelid = pg_class.oid AND pg_attribute.attnum = any(pg_index.indkey) AND indisprimary;");
		EXECUTE;
		if (!QUERY_EXECUTED) {
			printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
		}
		FOREACH_ROW {
			for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
				if (STR_EQUAL(OBJ_TABLE(J)->cols[K]->name, DATA(0))) {
					OBJ_TABLE(J)->cols[K]->isPrimaryColumn = TRUE;
					break;
				}
			}
		}
	}

	// Analyze selected columns list, expressions are separated to columns, create: OBJ_SELECTED_COL, OBJ_NSELECTED
	for (K = 0; K < OBJ_SQ->selectElementsNum; K++) {
		Boolean flag = addRealColumn(OBJ_SQ->selectElements[K], NULL);
		if (!flag) {
			char *remain, *tmp;
			char *AF[] = {AF_SUM, AF_COUNT, AF_AVG, AF_MIN, _AF_MAX};
			char *NF[] = {"abs("};

			char *delim = "()+*-/";
			char *filter[MAX_N_ELEMENTS];
			int filterLen;

			// otherwise, first, check for the aggregate functions
			tmp = removeSpaces(OBJ_SQ->selectElements[K]);
			remain = addFunction(OBJ_SQ->selectElements[K], tmp, 5, AF, TRUE);
			FREE(tmp);

			// then, check for the normal functions
			tmp = remain;
			remain = addFunction(OBJ_SQ->selectElements[K], remain, 1, NF, FALSE);
			FREE(tmp);

			// finally, check for the remaining columns' name
			split(remain, delim, filter, &filterLen, TRUE);

			for (M = 0; M < filterLen; M++) {
				addRealColumn(filter[M], OBJ_SQ->selectElements[K]);
				FREE(filter[M]);
			}

			FREE(remain);
		}
	}

	// Additional count column for algorithm purpose
	initColumn(&OBJ_SELECTED_COL(OBJ_NSELECTED));
	OBJ_SELECTED_COL(OBJ_NSELECTED)->context = COLUMN_CONTEXT_TABLE_UNWIRED;
	OBJ_SELECTED_COL(OBJ_NSELECTED)->hasAggregateFunction = TRUE;
	OBJ_SELECTED_COL(OBJ_NSELECTED)->name = string("count(*)");
	OBJ_SELECTED_COL(OBJ_NSELECTED)->func = string(AF_COUNT);
	OBJ_SELECTED_COL(OBJ_NSELECTED)->funcArg = string("*");
	OBJ_SELECTED_COL(OBJ_NSELECTED)->type = string("bigint");
	OBJ_SELECTED_COL(OBJ_NSELECTED)->varName = string("mvcount");
	OBJ_NSELECTED++;

	/*
		Analyze joining conditions

		IN: container includes conditions involved by inner column, used for trigger's FCC
		OUT: container includes conditions involved by outer column
		BIN: container includes conditions involved by both inner and outer column (usually join conditions)
		(BIN is sub set of OUT, separately saved for future dev)

		Create: OBJ_IN_CONDITIONS, OBJ_ OUT_CONDITIONS, OBJ_BIN_CONDITIONS, OBJ_FCC
	*/
	FOREACH_TABLE{

		// Check on WHERE
		if (OBJ_SQ->hasWhere)
		for (M = 0; M < OBJ_SQ->whereConditionsNum; M++)
			filterTriggerCondition(OBJ_SQ->whereConditions[M], J);

		// Check on JOIN
		if (OBJ_SQ->hasJoin)
			for (M = 0; M < OBJ_SQ->onNum; M++)
				for (N = 0; N < OBJ_SQ->onConditionsNum[M]; N++)
					filterTriggerCondition(OBJ_SQ->onConditions[M][N], J);

		// Check on HAVING
		if (OBJ_SQ->hasHaving)
			for (M = 0; M < OBJ_SQ->havingConditionsNum; M++)
				filterTriggerCondition(OBJ_SQ->havingConditions[M], J);

		/* CREATE TRIGGERS' FIRST CHECKING CONDITION */
		// 1. Embeding variables

		// For each IN condition
		for (M = 0; M < OBJ_IN_NCONDITIONS(J); M++) {
			char *conditionCode, *tmp;
			conditionCode = copy(OBJ_IN_CONDITIONS(J, M));

			// For each table
			for (N = 0; N < OBJ_TABLE(J)->nCols; N++) {
				char *colPos, *tableName;
				char *start, *end, *oldString, *draft, *checkVar;
				draft = conditionCode;

				// Find inner columns' position
				do {
					colPos = findSubString(draft, OBJ_TABLE(J)->cols[N]->name);
					if (colPos) {
						tableName = getPrecededTableName(conditionCode, colPos);
						if (tableName) start = colPos - strlen(tableName) - 1;
						else start = colPos;
						end = colPos + strlen(OBJ_TABLE(J)->cols[N]->name);
						oldString = subString(conditionCode, start, end);
						tmp = conditionCode;
						conditionCode = replaceFirst(conditionCode, oldString, OBJ_TABLE(J)->cols[N]->varName);

						// Prevent the infinite loop in some cases
						draft = conditionCode;
						do {
							checkVar = strstr(draft, OBJ_TABLE(J)->cols[N]->varName);
							if (checkVar)
								draft = checkVar + (strlen(OBJ_TABLE(J)->cols[N]->varName) - 1);
						} while (checkVar);

						FREE(tmp);
						FREE(oldString);
						FREE(tableName);
					}
				} while (colPos);

				tmp = OBJ_IN_CONDITIONS(J, M);
				OBJ_IN_CONDITIONS(J, M) = copy(conditionCode);
				FREE(tmp);
			}
			FREE(conditionCode);
		}

		// 2. Combine conditions with 'and' (&&) opr
		/*
			! Normal function will be built in pmvtg_ctrigger's lib
		*/
		STR_INIT(OBJ_FCC(J), "1 ");
		for (M = 0; M < OBJ_IN_NCONDITIONS(J); M++) {
			STR_APPEND(OBJ_FCC(J), " && ");
			STR_APPEND(OBJ_FCC(J), OBJ_IN_CONDITIONS(J, M));
		}
	}

	// Rebuild new select clause based on the original for algorithm purpose, create: OBJ_SELECT_CLAUSE
	OBJ_SELECT_CLAUSE = copy("select ");
	for (K = 0; K < OBJ_NSELECTED; K++) {
		char *name = copy(OBJ_SELECTED_COL(K)->name);
		if (OBJ_SELECTED_COL(K)->context == COLUMN_CONTEXT_STAND_ALONE) {
			name = dammf_insertSubString(name, name, ".");
			name = dammf_insertSubString(name, name, OBJ_SELECTED_COL(K)->table->name);
		}
		STR_APPEND(OBJ_SELECT_CLAUSE, name);
		if (K != OBJ_NSELECTED-1)
			STR_APPEND(OBJ_SELECT_CLAUSE, ", ");
		free(name);
	}

	// Check for aggregate functions, especially MIN and/or MAX, define: OBJ_STATIC_BOOL_AGG, OBJ_STATIC_BOOL_AGG_MINMAX
	// OBJ_STATIC_BOOL_AGG is alias of OBJ_A1_CHECK
	// Check in selected list if there are only sum & count aggregate functions involved in query
	// Define OBJ_A2_CHECK (alias of OBJ_STATIC_BOOL_ONLY_AGG_SUMCOUNT)
	// Note: A2 is a calling mask of updated algorithm for query contains only sum & count aggregate functions
	// Note: Avg is turned into sum & count due to algorithm
	for (K = 0; K < OBJ_NSELECTED-1; K++) {
		if (OBJ_SELECTED_COL(K)->hasAggregateFunction) {
			OBJ_STATIC_BOOL_AGG = TRUE;
			if (STR_EQUAL_CI(OBJ_SELECTED_COL(K)->func, AF_MIN) || STR_EQUAL_CI(OBJ_SELECTED_COL(K)->func, _AF_MAX)) {
				OBJ_STATIC_BOOL_AGG_MINMAX = TRUE;
			}
			if (STR_EQUAL_CI(OBJ_SELECTED_COL(K)->func, AF_SUM) || STR_EQUAL_CI(OBJ_SELECTED_COL(K)->func, AF_COUNT)) {
				OBJ_A2_CHECK = TRUE;
			}
		}
	}

	if (OBJ_STATIC_BOOL_AGG) {
		if (OBJ_A2_CHECK && OBJ_STATIC_BOOL_AGG_MINMAX) {
			OBJ_A2_CHECK = FALSE;
		}
	}

	// Check for columns that are both in T & G clause, define: OBJ_TG_COL, OBJ_TG_NCOLS
	// Note: defined objects do not need to free after use (unlike created objects)
	// TG: columns appeared in current table & MV, + primary key
	// Go on pair with OBJ_A2_TG_TABLE, which determines the tables (of column in sql) that use that value from the current table
	// customers.cust_id = <sales.cust_id> (TG)
	// costs.promo_id = <sales.promo_id> (pk) and ...
	// Also define: OBJ_A2_TG_NTABLES, OBJ_A2_TG_TABLE
	// OBJ_A2_TG_NTABLES & OBJ_TG_NCOLS are duplicated each other
	FOREACH_TABLE {
		OBJ_A2_TG_NTABLES(J) = 0;
		OBJ_TG_NCOLS(J) = 0;

		for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
			char *colname = copy(OBJ_TABLE(J)->cols[K]->name);

			for (M = 0; M < OBJ_SQ->groupbyElementsNum; M++) {
				char *colPos = findSubString(OBJ_SQ->groupbyElements[M], colname);
				
				if (colPos) {
					char *tableName = getPrecededTableName(OBJ_SQ->groupbyElements[M], colPos);
					if (STR_INEQUAL_CI(tableName, OBJ_TABLE(J)->name)) {
						OBJ_TG_COL(J, OBJ_TG_NCOLS(J)++) = OBJ_TABLE(J)->cols[K];
						OBJ_A2_TG_TABLE(J, OBJ_A2_TG_NTABLES(J)++) = copy(tableName);
					}
					FREE(tableName);
				}
			}
			FREE(colname);
		}
	}

	for (J = 0; J < OBJ_SQ->fromElementsNum; J++) {
		for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
			if (OBJ_TABLE(J)->cols[K]->isPrimaryColumn) {
				for (M = 0; M < OBJ_SQ->fromElementsNum; M++) {
					if (M != J) {
						for (N = 0; N < OBJ_TABLE(M)->nCols; N++) {
							if (STR_EQUAL_CI(OBJ_TABLE(J)->cols[K]->name, OBJ_TABLE(M)->cols[N]->name)) {
								OBJ_TG_COL(J, OBJ_TG_NCOLS(J)++) = OBJ_TABLE(J)->cols[K];
								OBJ_A2_TG_TABLE(J, OBJ_A2_TG_NTABLES(J)++) = copy(OBJ_TABLE(M)->name);
							}

						}
					}
				}
			}
		}
	}

	// Create: OBJ_A2_WHERE_CLAUSE
	if (OBJ_OUT_NCONDITIONS(J) > 0) {
		for (J = 0; J < OBJ_SQ->fromElementsNum; J++) {
			STR_INIT(OBJ_A2_WHERE_CLAUSE(J), " true ");

			// Preprocess out conditions & recombine
			for (M = 0; M < OBJ_OUT_NCONDITIONS(J); M++) {
				OBJ_OUT_CONDITIONS(J, M) = dammf_replaceFirst(OBJ_OUT_CONDITIONS(J, M), "==", "=");
				STR_APPEND(OBJ_A2_WHERE_CLAUSE(J), " and ");
				STR_APPEND(OBJ_A2_WHERE_CLAUSE(J), OBJ_OUT_CONDITIONS(J, M));
			}
		}
	}

	// Define: OBJ_AGG_INVOLVED_CHECK
	// Important requirement: full column name as format: <table-name>.<column-name>
	for (J = 0; J < OBJ_SQ->fromElementsNum; J++) {
		OBJ_AGG_INVOLVED_CHECK(J) = FALSE;
	}
	for (K = 0; K < OBJ_NSELECTED-1; K++) {
		if (OBJ_SELECTED_COL(K)->hasAggregateFunction && STR_INEQUAL(OBJ_SELECTED_COL(K)->funcArg, "*")) {
			char *columnList[MAX_N_ELEMENTS];
			int columnListLen = 0;
			split(OBJ_SELECTED_COL(K)->funcArg, "+-*/", columnList, &columnListLen, TRUE);
			for (M = 0; M < columnListLen; M++) {
				char *dotPos = strstr(columnList[M], ".");
				if (dotPos) {
					char *tableName = getPrecededTableName(columnList[M], dotPos+1);
					for (J = 0; J < OBJ_SQ->fromElementsNum; J++) {
						if (STR_EQUAL_CI(tableName, OBJ_TABLE(J)->name)) {
							OBJ_AGG_INVOLVED_CHECK(J) = TRUE;
							break;
						}
					}
					FREE(tableName);
				}
			}

			for (M = 0; M < columnListLen; M++) {
				FREE(columnList[M]);
			}
		}
	}

	// Define: OBJ_KEY_IN_G_CHECK
	for (J = 0; J < OBJ_SQ->fromElementsNum; J++) {
		OBJ_KEY_IN_G_CHECK(J) = TRUE;
	}

	if (OBJ_SQ->hasGroupby) {
		for (J = 0; J < OBJ_SQ->fromElementsNum; J++) {
			for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
				if (OBJ_TABLE(J)->cols[K]->isPrimaryColumn) {
					Boolean check = FALSE;
					char *fullColName;
					STR_INIT(fullColName, OBJ_TABLE(J)->name);
					STR_APPEND(fullColName, ".");
					STR_APPEND(fullColName, OBJ_TABLE(J)->cols[K]->name);

					for (M = 0; M < OBJ_SQ->groupbyElementsNum; M++) {
						if (STR_EQUAL(fullColName, OBJ_SQ->groupbyElements[M])) {
							check = TRUE;
							break;
						}
					}

					FREE(fullColName);
					if (!check) {
						OBJ_KEY_IN_G_CHECK(J) = FALSE;
						break;
					}
				}
			}
		}
	}

	QUERY("select updated_time from mviautg.mv_trace inner join mviautg.mvs on mviautg.mv_trace.mv_id = mviautg.mvs.mv_id where mv_name = '");
	_QUERY(_MVIAU_MVName);
	_QUERY("'");
	EXECUTE;

	startingPoint = copy(DATA(0));
	printf("Starting point : %s\n", startingPoint);

	QUERY("(");
	FOREACH_TABLE {
		_QUERY("select table_name, log_id, log_time from mviautg.");
		_QUERY(OBJ_TABLE(J)->name);
		_QUERY("_log, mviautg.tables where mviautg.tables.table_id = mviautg.");
		_QUERY(OBJ_TABLE(J)->name);
		_QUERY("_log.table_id and log_time >= '");
		_QUERY(startingPoint);
		_QUERY("' ");
		if (J < OBJ_SQ->fromElementsNum - 1) _QUERY("union all ");
	}
	_QUERY(") order by log_time asc");
	EXECUTE;

	FOREACH_ROW {
		
		RESULT(localLog);
		RESULT(updateTime);
		Log log = NULL;
		int i;
		
		log = initLog(DATA(0), DATA(1));

		QUERY("select * from mviautg.");
		_QUERY(log->tableName);
		_QUERY("_log where log_id = ");
		_QUERY(log->logId);
		EXECUTE_TO(localLog);

		log->logAction = copy(DATA_ON(localLog, 0, 1));
		log->logTime = copy(DATA_ON(localLog, 0, 2));

		FOREACH_TABLE {
			if (STR_EQUAL(log->tableName, OBJ_TABLE(J)->name)) {
				log->objTableId = J;
				for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
					if (OBJ_TABLE(J)->cols[K]->isPrimaryColumn) {
						(log->nPk)++;
					}
				}
				break;
			}
		}

		for (M = LOG_MANAGEMENT_N_INFO; M < LOG_MANAGEMENT_N_INFO + log->nPk; M++) {
			log->newPrimaryKeys[M - LOG_MANAGEMENT_N_INFO] = copy(DATA_ON(localLog, 0, M));
		}

		for (M = LOG_MANAGEMENT_N_INFO + log->nPk; M < LOG_MANAGEMENT_N_INFO + (2 * log->nPk); M++) {
			log->oldPrimaryKeys[M - (LOG_MANAGEMENT_N_INFO + log->nPk)] = copy(DATA_ON(localLog, 0, M));
		}

		for (M = LOG_MANAGEMENT_N_INFO + (2 * log->nPk); M < LOG_MANAGEMENT_N_INFO + log->nPk + OBJ_TABLE(log->objTableId)->nCols; M++) {
			log->oldNormalCols[M - (LOG_MANAGEMENT_N_INFO + (2 * log->nPk))] = copy(DATA_ON(localLog, 0, M));
		}

		if (STR_EQUAL(log->logAction, "1")) { // INSERT
			
			if (OBJ_AGG_INVOLVED_CHECK(log->objTableId)) {
				int logColumnCount = 0;
				
				QUERY(OBJ_SELECT_CLAUSE);
				_QUERY(" from ");
				_QUERY(OBJ_SQ->from);
				_QUERY(" where true ");
				if (OBJ_SQ->hasWhere) {_QUERY(" and "); _QUERY(OBJ_SQ->where);}
				for (K = 0; K < OBJ_TABLE(log->objTableId)->nCols; K++) {
					if (OBJ_TABLE(log->objTableId)->cols[K]->isPrimaryColumn) {
						char optionalWhere[ESTIMATED_LENGTH];
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_TABLE(log->objTableId)->cols[K]->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
						else { quote = ""; strprefix = "str_"; }
						sprintf(optionalWhere, " and %s.%s = %s%s%s ", OBJ_TABLE(log->objTableId)->name, OBJ_TABLE(log->objTableId)->cols[K]->name, quote, log->newPrimaryKeys[logColumnCount++], quote);
						_QUERY(optionalWhere);	
						FREE(ctype);
					}
				}
				if (OBJ_SQ->hasGroupby) {_QUERY(" group by "); _QUERY(OBJ_SQ->groupby);}
				if (OBJ_SQ->hasHaving) {_QUERY(" having "); _QUERY(OBJ_SQ->having);}
				
				// 2. Save result
				EXECUTE_TO(requeryResult);

				// 3. For each result:
				FOR(M, requeryResult) {
					
					RESULT(mvcountResult);
					int mvColumnCount = 0;

					QUERY("select mvcount from ");
					_QUERY(_MVIAU_MVName);
					_QUERY(" where true ");
					for (K = 0; K < OBJ_NSELECTED; K++) {
						if (!OBJ_SELECTED_COL(K)->hasAggregateFunction) {
							char selectMVCountWhereClause[ESTIMATED_LENGTH];
							char *quote;
							char *ctype;
							ctype = createCType(OBJ_SELECTED_COL(K)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							sprintf(selectMVCountWhereClause, " and %s = %s%s%s ", OBJ_SELECTED_COL(K)->name, quote, DATA_ON(requeryResult, M, mvColumnCount++), quote);
							_QUERY(selectMVCountWhereClause);	
							FREE(ctype);
						}
					}
					
					EXECUTE_TO(mvcountResult);

					if (NROWS_ON(mvcountResult) == 0) { // insert to mv
						
						RESULT(insertResult);

						mvColumnCount = 0;
						QUERY("insert into ");
						_QUERY(_MVIAU_MVName);
						_QUERY(" values(");

						for (K = 0; K < OBJ_NSELECTED - 1; K++) {
							char selectMVCountWhereClause[ESTIMATED_LENGTH];
							char *quote;
							char *ctype;

							ctype = createCType(OBJ_SELECTED_COL(K)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							sprintf(selectMVCountWhereClause, " %s%s%s, ", quote, DATA_ON(requeryResult, M, mvColumnCount++), quote);
							_QUERY(selectMVCountWhereClause);
							FREE(ctype);
						}
						_QUERY("1)");
						EXECUTE_TO(insertResult);
						if (!QUERY_EXECUTED_ON(insertResult)) {
							printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
						}

						FREE_RESULT(insertResult);
					} else { // update 
						RESULT(updateResult);
						Boolean check = FALSE;
						mvColumnCount = 0;

						mvColumnCount = 0;
						QUERY("update ");
						_QUERY(_MVIAU_MVName);
						_QUERY(" set mvcount = mvcount + 1 ");

						for (K = 0; K < OBJ_NSELECTED-1; K++) {
							if (OBJ_SELECTED_COL(K)->hasAggregateFunction) {
								char updateMVselectClause[ESTIMATED_LENGTH];

								if (STR_EQUAL(OBJ_SELECTED_COL(K)->func, AF_SUM) || STR_EQUAL(OBJ_SELECTED_COL(K)->func, AF_COUNT)) {
									sprintf(updateMVselectClause, ", %s = %s + %s ", OBJ_SELECTED_COL(K)->varName, OBJ_SELECTED_COL(K)->varName, DATA_ON(requeryResult, M, K));
									_QUERY(updateMVselectClause);
								} else if (STR_EQUAL(OBJ_SELECTED_COL(K)->func, AF_MIN)) {
									sprintf(updateMVselectClause, ", %s = (case when %s > %s then %s else %s end) ", OBJ_SELECTED_COL(K)->varName, OBJ_SELECTED_COL(K)->varName, DATA_ON(requeryResult, M, K), DATA_ON(requeryResult, M, K), OBJ_SELECTED_COL(K)->varName);
									_QUERY(updateMVselectClause);
								} else {
									// AF_MAX
									sprintf(updateMVselectClause, ", %s = (case when %s < %s then %s else %s end) ", OBJ_SELECTED_COL(K)->varName, OBJ_SELECTED_COL(K)->varName, DATA_ON(requeryResult, M, K), DATA_ON(requeryResult, M, K), OBJ_SELECTED_COL(K)->varName);
									_QUERY(updateMVselectClause);
								}
							}
						}

						mvColumnCount = 0;
						_QUERY(" where true ");
						for (K = 0; K < OBJ_NSELECTED; K++) {
							if (!OBJ_SELECTED_COL(K)->hasAggregateFunction) {
								char selectMVCountWhereClause[ESTIMATED_LENGTH];
								char *quote;
								char *ctype;
								ctype = createCType(OBJ_SELECTED_COL(K)->type);
								if (STR_EQUAL(ctype, "char *")) quote = "\'";
								else quote = "";
								sprintf(selectMVCountWhereClause, " and %s = %s%s%s ", OBJ_SELECTED_COL(K)->name, quote, DATA_ON(requeryResult, M, mvColumnCount++), quote);
								_QUERY(selectMVCountWhereClause);	
								FREE(ctype);
							}
						}
						EXECUTE_TO(updateResult);
						if (!QUERY_EXECUTED_ON(updateResult)) {
							printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
						}

						FREE_RESULT(updateResult);
					}
				}
			}

		} else if (STR_EQUAL(log->logAction, "2")) { // UPDATE

			/*if (OBJ_A2_CHECK && OBJ_AGG_INVOLVED_CHECK(log->objTableId)) { // A2.1 UPDATE 
	
			} else if (OBJ_KEY_IN_G_CHECK(log->objTableId) && !OBJ_AGG_INVOLVED_CHECK(log->objTableId)) { // A2.2 UPDATE
				Boolean check;
				char sql[ESTIMATED_LENGTH];
				int counter = 0;

				QUERY("select * from ");
				_QUERY(OBJ_TABLE(log->objTableId)->name);
				_QUERY(" where true ");
				for (K = 0; K < OBJ_TABLE(log->objTableId)->nCols; K++) {
					if (OBJ_TABLE(log->objTableId)->cols[K]->isPrimaryColumn) {
						char *quote;
						char *ctype = createCType(OBJ_TABLE(log->objTableId)->cols[K]->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'";}
						else { quote = "";}
						sprintf(sql, " and %s = %s%s%s ", OBJ_TABLE(log->objTableId)->cols[K]->name, quote, log->oldPrimaryKeys[counter++], quote);
						FREE(ctype);
					}
				}
				EXECUTE_TO(requeryResult);

				QUERY("update ");
				_QUERY(_MVIAU_MVName);
				_QUERY(" set ");
				check = FALSE;
				for (K = 0; K < OBJ_NSELECTED; K++) {
					if (OBJ_SELECTED_COL(K)->table == OBJ_TABLE(log->objTableId)) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(K)->type);
						int ordinalPos = OBJ_SELECTED_COL(K)->ordinalPosition;
						if (STR_EQUAL(ctype, "char *")) { quote = "\'";}
						else { quote = "";}

						if (check) _QUERY(", ");
						check = TRUE;

						sprintf(sql, " %s = %s%s%s ", OBJ_SELECTED_COL(K)->name, quote, DATA_ON(requeryResult, 0, ordinalPos), quote);
						_QUERY(sql);
						FREE(ctype);
					}
				}
			} else*/ if (TRUE) { // NORMAL UPDATE merged with A2.1
				Boolean check = FALSE;
				int logColumnCount = 0;
				char sql[ESTIMATED_LENGTH * 5];

				QUERY("select ");
				for (K = 0; K < OBJ_NSELECTED; K++) {
					if (OBJ_SELECTED_COL(K)->table) {
						sprintf(sql, " %s.%s ", OBJ_SELECTED_COL(K)->table->name, OBJ_SELECTED_COL(K)->name);
						_QUERY(sql);
					} else {
						int nParts, partsType[MAX_N_ELEMENTS];
						char *parts[MAX_N_ELEMENTS];
						A2Split(log->objTableId, OBJ_SELECTED_COL(K)->funcArg, &nParts, partsType, parts);

						_QUERY(OBJ_SELECTED_COL(K)->func);
						for (M = 0; M < nParts; M++) {
							if (partsType[M] != log->objTableId) {
								_QUERY(parts[M]);
							} else {
								int ordinalPos = atoi(parts[M]);
								if (ordinalPos <= log->nPk) {
									_QUERY(log->oldPrimaryKeys[ordinalPos - 1]);
								} else {
									_QUERY(log->oldNormalCols[ordinalPos - log->nPk - 1]);
								}
							}
						}

						_QUERY(") ");

						for (M = 0; M < nParts; M++) {
							FREE(parts[M]);
						}
					}
					
					//if (K < OBJ_NSELECTED - 1) {
						_QUERY(", ");
					//}
				}

				_QUERY(" 1 from ");

				FOREACH_TABLE {
					if (J != log->objTableId) {
						if (check) {
							_QUERY(", ");
						}
						_QUERY(OBJ_TABLE(J)->name);
						check = TRUE;
					}
				}

				// out condition is missing some case
				_QUERY(" where ");
				if (OBJ_A2_WHERE_CLAUSE(log->objTableId)) { 
					_QUERY(OBJ_A2_WHERE_CLAUSE(log->objTableId));
					_QUERY(" and ");
				}

				_QUERY(" true ");

				for (K = 0; K < OBJ_TG_NCOLS(log->objTableId); K++) {
					char *quote;
					char *ctype = createCType(OBJ_TG_COL(log->objTableId, K)->type);
					int ordinalPos = OBJ_TG_COL(log->objTableId, K)->ordinalPosition;
					if (STR_EQUAL(ctype, "char *")) { quote = "\'";}
					else { quote = "";}

					sprintf(sql, " and %s.%s = %s", OBJ_A2_TG_TABLE(log->objTableId, K), OBJ_TG_COL(log->objTableId, K)->name, quote);
					_QUERY(sql);

					if (ordinalPos <= log->nPk) {
						_QUERY(log->oldPrimaryKeys[ordinalPos - 1]);
					} else {
						_QUERY(log->oldNormalCols[ordinalPos - log->nPk - 1]);
					}

					_QUERY(quote);
					FREE(ctype);
				}

				if (OBJ_IN_NCONDITIONS(log->objTableId) > 0) {
					for (M = 0; M < OBJ_IN_NCONDITIONS(log->objTableId); M++) {
						for (K = 0; K < OBJ_TABLE(log->objTableId)->nCols; K++) {
							if (findSubString(OBJ_IN_CONDITIONS(log->objTableId, M), OBJ_TABLE(log->objTableId)->cols[K]->name)) {
								char *quote;
								char *ctype = createCType(OBJ_TABLE(log->objTableId)->cols[K]->type);
								int ordinalPos = OBJ_TABLE(log->objTableId)->cols[K]->ordinalPosition;
								if (STR_EQUAL(ctype, "char *")) { quote = "\'";}
								else { quote = "";}

								if (ordinalPos <= log->nPk) {
									sprintf(sql, "%s%s%s", quote, log->oldPrimaryKeys[ordinalPos - 1], quote);
									OBJ_IN_CONDITIONS(log->objTableId, M) = dammf_replaceFirst(OBJ_IN_CONDITIONS(log->objTableId, M), OBJ_TABLE(log->objTableId)->cols[K]->varName, sql);
								} else {
									sprintf(sql, "%s%s%s", quote, log->oldNormalCols[ordinalPos - log->nPk - 1], quote);
									OBJ_IN_CONDITIONS(log->objTableId, M) = dammf_replaceFirst(OBJ_IN_CONDITIONS(log->objTableId, M), OBJ_TABLE(log->objTableId)->cols[K]->varName, sql);
								}

								sprintf(sql, " and %s ", OBJ_IN_CONDITIONS(log->objTableId, M));
								_QUERY(sql);
								FREE(ctype);
							}
						}
					}
				}
				if (OBJ_SQ->hasGroupby) {_QUERY(" group by "); _QUERY(OBJ_SQ->groupby);}
				if (OBJ_SQ->hasHaving) {_QUERY(" having "); _QUERY(OBJ_SQ->having);}

				_QUERY(" union all ");
				
				_QUERY(OBJ_SELECT_CLAUSE);
				_QUERY(", 2 from ");
				_QUERY(OBJ_SQ->from);
				_QUERY(" where true ");
				if (OBJ_SQ->hasWhere) {
					_QUERY(" and ");
					_QUERY(OBJ_SQ->where);
				}
				for (K = 0; K < OBJ_TG_NCOLS(log->objTableId); K++) {
					char *quote;
					char *ctype = createCType(OBJ_TG_COL(log->objTableId, K)->type);
					int ordinalPos = OBJ_TG_COL(log->objTableId, K)->ordinalPosition;
					if (STR_EQUAL(ctype, "char *")) { quote = "\'"; }
					else { quote = "";}
					sprintf(sql, " and %s.%s = %s", OBJ_A2_TG_TABLE(log->objTableId, K), OBJ_TG_COL(log->objTableId, K)->name, quote);
					_QUERY(sql);

					if (ordinalPos <= log->nPk) {
						_QUERY(log->oldPrimaryKeys[ordinalPos - 1]);
					} else {
						_QUERY(log->oldNormalCols[ordinalPos - log->nPk - 1]);
					}

					_QUERY(quote);
					FREE(ctype);
				}
				if (OBJ_SQ->hasGroupby) {_QUERY(" group by "); _QUERY(OBJ_SQ->groupby);}
				if (OBJ_SQ->hasHaving) {_QUERY(" having "); _QUERY(OBJ_SQ->having);}

				EXECUTE_TO(requeryResult);				

				FOR(M, requeryResult) {
					RESULT(mvcountResult);
					char* mvCountOnMv;
					char* mvCountRequery;
					char* a2 = DATA_ON(requeryResult, M, OBJ_NSELECTED);
					mvCountRequery = DATA_ON(requeryResult, M, OBJ_NSELECTED - 1);

					QUERY("select mvcount from ");
					_QUERY(_MVIAU_MVName);
					_QUERY(" where true ");
					for (K = 0; K < OBJ_NSELECTED; K++) {
						if (!OBJ_SELECTED_COL(K)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(K)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							sprintf(sql, " and %s = %s%s%s", OBJ_SELECTED_COL(K)->name, quote, DATA_ON(requeryResult, M, K), quote);
							_QUERY(sql);
							FREE(ctype);
						}
					}
					EXECUTE_TO(mvcountResult);
					mvCountOnMv = DATA_ON(mvcountResult, 0, 0);

					if (STR_EQUAL(a2, "1")) {
						if (STR_EQUAL(mvCountOnMv, mvCountRequery)) {
							RESULT(deleteResult);
							QUERY("delete from ");
							_QUERY(_MVIAU_MVName);
							_QUERY(" where true ");
							for (K = 0; K < OBJ_NSELECTED; K++) {
								if (!OBJ_SELECTED_COL(K)->hasAggregateFunction) {
									char *quote;
									char *ctype = createCType(OBJ_SELECTED_COL(K)->type);
									if (STR_EQUAL(ctype, "char *")) quote = "\'";
									else quote = "";
									sprintf(sql, " and %s = %s%s%s", OBJ_SELECTED_COL(K)->name, quote, DATA_ON(requeryResult, M, K), quote);
									_QUERY(sql);
									FREE(ctype);
								}
							}
							EXECUTE_TO(deleteResult);
							if (!QUERY_EXECUTED_ON(deleteResult)) {
								printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
							}
							FREE_RESULT(deleteResult);
						} else {
							RESULT(updateResult);

							QUERY("update mv1 set mvcount = mvcount - 1 ");
							for (K = 0; K < OBJ_NSELECTED-1; K++) {
								if (OBJ_SELECTED_COL(K)->hasAggregateFunction) {
									char updateMVselectClause[ESTIMATED_LENGTH];
									if (STR_EQUAL(OBJ_SELECTED_COL(K)->func, AF_SUM) || STR_EQUAL(OBJ_SELECTED_COL(K)->func, AF_COUNT)) {
										sprintf(updateMVselectClause, ", %s = %s - %s ", OBJ_SELECTED_COL(K)->varName, OBJ_SELECTED_COL(K)->varName, DATA_ON(requeryResult, M, K));
										_QUERY(updateMVselectClause);
									}
								}
							}
							_QUERY(" where true ");
							for (K = 0; K < OBJ_NSELECTED; K++) {
								if (!OBJ_SELECTED_COL(K)->hasAggregateFunction) {
									char selectMVCountWhereClause[ESTIMATED_LENGTH];
									char *quote;
									char *ctype;
									ctype = createCType(OBJ_SELECTED_COL(K)->type);
									if (STR_EQUAL(ctype, "char *")) quote = "\'";
									else quote = "";
									sprintf(selectMVCountWhereClause, " and %s = %s%s%s ", OBJ_SELECTED_COL(K)->name, quote, DATA_ON(requeryResult, M, K), quote);
									_QUERY(selectMVCountWhereClause);	
									FREE(ctype);
								}
							}
							EXECUTE_TO(updateResult);
							if (!QUERY_EXECUTED_ON(updateResult)) {
								printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
							}

							FREE_RESULT(updateResult);
						}
					} else { // UPDATE: INSERT PHASE
						if (NROWS_ON(mvcountResult) == 0) {
							RESULT(insertResult);

							QUERY("insert into ");
							_QUERY(_MVIAU_MVName);
							_QUERY(" values(");

							for (K = 0; K < OBJ_NSELECTED - 1; K++) {
								char selectMVCountWhereClause[ESTIMATED_LENGTH];
								char *quote;
								char *ctype;

								ctype = createCType(OBJ_SELECTED_COL(K)->type);
								if (STR_EQUAL(ctype, "char *")) quote = "\'";
								else quote = "";
								sprintf(selectMVCountWhereClause, " %s%s%s, ", quote, DATA_ON(requeryResult, M, K), quote);
								_QUERY(selectMVCountWhereClause);
								FREE(ctype);
							}
							_QUERY(DATA_ON(requeryResult, M, OBJ_NSELECTED));
							_QUERY(")");
							EXECUTE_TO(insertResult);
							if (!QUERY_EXECUTED_ON(insertResult)) {
								printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
							}

							FREE_RESULT(insertResult);
						} else {
							RESULT(updateResult);
							Boolean check = FALSE;

							QUERY("update ");
							_QUERY(_MVIAU_MVName);
							_QUERY(" set mvcount =  ");
							_QUERY(DATA_ON(requeryResult, M, OBJ_NSELECTED-1));

							for (K = 0; K < OBJ_NSELECTED-1; K++) {
								if (OBJ_SELECTED_COL(K)->hasAggregateFunction) {
									char updateMVselectClause[ESTIMATED_LENGTH];

									if (STR_EQUAL(OBJ_SELECTED_COL(K)->func, AF_SUM) || STR_EQUAL(OBJ_SELECTED_COL(K)->func, AF_COUNT)) {
										sprintf(updateMVselectClause, ", %s = %s ", OBJ_SELECTED_COL(K)->varName, DATA_ON(requeryResult, M, K));
										_QUERY(updateMVselectClause);
									} else if (STR_EQUAL(OBJ_SELECTED_COL(K)->func, AF_MIN)) {
										sprintf(updateMVselectClause, ", %s = (case when %s > %s then %s else %s end) ", OBJ_SELECTED_COL(K)->varName, OBJ_SELECTED_COL(K)->varName, DATA_ON(requeryResult, M, K), DATA_ON(requeryResult, M, K), OBJ_SELECTED_COL(K)->varName);
										//sprintf(updateMVselectClause, ", %s = %s ", OBJ_SELECTED_COL(K)->varName, DATA_ON(requeryResult, M, K));
										_QUERY(updateMVselectClause);
									} else {
										// AF_MAX
										sprintf(updateMVselectClause, ", %s = (case when %s < %s then %s else %s end) ", OBJ_SELECTED_COL(K)->varName, OBJ_SELECTED_COL(K)->varName, DATA_ON(requeryResult, M, K), DATA_ON(requeryResult, M, K), OBJ_SELECTED_COL(K)->varName);
										//sprintf(updateMVselectClause, ", %s = %s ", OBJ_SELECTED_COL(K)->varName, DATA_ON(requeryResult, M, K));
										_QUERY(updateMVselectClause);
									}
								}
							}

							_QUERY(" where true ");
							for (K = 0; K < OBJ_NSELECTED; K++) {
								if (!OBJ_SELECTED_COL(K)->hasAggregateFunction) {
									char selectMVCountWhereClause[ESTIMATED_LENGTH];
									char *quote;
									char *ctype;
									ctype = createCType(OBJ_SELECTED_COL(K)->type);
									if (STR_EQUAL(ctype, "char *")) quote = "\'";
									else quote = "";
									sprintf(selectMVCountWhereClause, " and %s = %s%s%s ", OBJ_SELECTED_COL(K)->name, quote, DATA_ON(requeryResult, M, K), quote);
									_QUERY(selectMVCountWhereClause);	
									FREE(ctype);
								}
							}

							EXECUTE_TO(updateResult);
							if (!QUERY_EXECUTED_ON(updateResult)) {
								printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
							}

							FREE_RESULT(updateResult);
						}
					}

					FREE_RESULT(mvcountResult);
				}
			}
		} else { // DELETE
			if (TRUE) {
				RESULT(findMaxResult);
				Boolean check = FALSE;
				int logColumnCount = 0;
				char sql[ESTIMATED_LENGTH * 5];

				QUERY("select ");
				for (K = 0; K < OBJ_NSELECTED; K++) {
					if (OBJ_SELECTED_COL(K)->table) {
						sprintf(sql, " %s.%s ", OBJ_SELECTED_COL(K)->table->name, OBJ_SELECTED_COL(K)->name);
						_QUERY(sql);
					} else {
						int nParts, partsType[MAX_N_ELEMENTS];
						char *parts[MAX_N_ELEMENTS];
						A2Split(log->objTableId, OBJ_SELECTED_COL(K)->funcArg, &nParts, partsType, parts);

						_QUERY(OBJ_SELECTED_COL(K)->func);
						for (M = 0; M < nParts; M++) {
							if (partsType[M] != log->objTableId) {
								_QUERY(parts[M]);
							} else {
								int ordinalPos = atoi(parts[M]);
								if (ordinalPos <= log->nPk) {
									_QUERY(log->oldPrimaryKeys[ordinalPos - 1]);
								} else {
									_QUERY(log->oldNormalCols[ordinalPos - log->nPk - 1]);
								}
							}
						}

						_QUERY(") ");

						for (M = 0; M < nParts; M++) {
							FREE(parts[M]);
						}
					}
					
					if (K < OBJ_NSELECTED - 1) {
						_QUERY(", ");
					}
				}

				_QUERY(" from ");

				FOREACH_TABLE {
					if (J != log->objTableId) {
						if (check) {
							_QUERY(", ");
						}
						_QUERY(OBJ_TABLE(J)->name);
						check = TRUE;
					}
				}

				// out condition is missing some case
				_QUERY(" where ");
				if (OBJ_A2_WHERE_CLAUSE(log->objTableId)) { 
					_QUERY(OBJ_A2_WHERE_CLAUSE(log->objTableId));
					_QUERY(" and ");
				}

				_QUERY(" true ");

				for (K = 0; K < OBJ_TG_NCOLS(log->objTableId); K++) {
					char *quote;
					char *ctype = createCType(OBJ_TG_COL(log->objTableId, K)->type);
					int ordinalPos = OBJ_TG_COL(log->objTableId, K)->ordinalPosition;
					if (STR_EQUAL(ctype, "char *")) { quote = "\'";}
					else { quote = "";}

					sprintf(sql, " and %s.%s = %s", OBJ_A2_TG_TABLE(log->objTableId, K), OBJ_TG_COL(log->objTableId, K)->name, quote);
					_QUERY(sql);

					if (ordinalPos <= log->nPk) {
						_QUERY(log->oldPrimaryKeys[ordinalPos - 1]);
					} else {
						_QUERY(log->oldNormalCols[ordinalPos - log->nPk - 1]);
					}

					_QUERY(quote);
					FREE(ctype);
				}

				if (OBJ_IN_NCONDITIONS(log->objTableId) > 0) {
					for (M = 0; M < OBJ_IN_NCONDITIONS(log->objTableId); M++) {
						for (K = 0; K < OBJ_TABLE(log->objTableId)->nCols; K++) {
							if (findSubString(OBJ_IN_CONDITIONS(log->objTableId, M), OBJ_TABLE(log->objTableId)->cols[K]->name)) {
								char *quote;
								char *ctype = createCType(OBJ_TABLE(log->objTableId)->cols[K]->type);
								int ordinalPos = OBJ_TABLE(log->objTableId)->cols[K]->ordinalPosition;
								if (STR_EQUAL(ctype, "char *")) { quote = "\'";}
								else { quote = "";}

								if (ordinalPos <= log->nPk) {
									sprintf(sql, "%s%s%s", quote, log->oldPrimaryKeys[ordinalPos - 1], quote);
									OBJ_IN_CONDITIONS(log->objTableId, M) = dammf_replaceFirst(OBJ_IN_CONDITIONS(log->objTableId, M), OBJ_TABLE(log->objTableId)->cols[K]->varName, sql);
								} else {
									sprintf(sql, "%s%s%s", quote, log->oldNormalCols[ordinalPos - log->nPk - 1], quote);
									OBJ_IN_CONDITIONS(log->objTableId, M) = dammf_replaceFirst(OBJ_IN_CONDITIONS(log->objTableId, M), OBJ_TABLE(log->objTableId)->cols[K]->varName, sql);
								}

								sprintf(sql, " and %s ", OBJ_IN_CONDITIONS(log->objTableId, M));
								_QUERY(sql);
								FREE(ctype);
							}
						}
					}
				}
				if (OBJ_SQ->hasGroupby) {_QUERY(" group by "); _QUERY(OBJ_SQ->groupby);}
				if (OBJ_SQ->hasHaving) {_QUERY(" having "); _QUERY(OBJ_SQ->having);}

				// 2. Save result
				EXECUTE_TO(requeryResult);
				if (!QUERY_EXECUTED_ON(requeryResult)) {
					printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
				}
				// 3. For each result:
				FOR(M, requeryResult) {
					RESULT(mvcountResult);

					char *mvcountRequery = DATA_ON(requeryResult, M, OBJ_NSELECTED-1);
					char *mvcountOnMv;

					int mvColumnCount = 0;

					QUERY("select mvcount from ");
					_QUERY(_MVIAU_MVName);
					_QUERY(" where true ");

					for (K = 0; K < OBJ_NSELECTED; K++) {
						if (!OBJ_SELECTED_COL(K)->hasAggregateFunction) {
							char selectMVCountWhereClause[ESTIMATED_LENGTH];
							char *quote;
							char *ctype;
							ctype = createCType(OBJ_SELECTED_COL(K)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							sprintf(selectMVCountWhereClause, " and %s = %s%s%s ", OBJ_SELECTED_COL(K)->name, quote, DATA_ON(requeryResult, M, mvColumnCount++), quote);
							_QUERY(selectMVCountWhereClause);	
							FREE(ctype);
						}
					}
					
					EXECUTE_TO(mvcountResult);

					mvcountOnMv = DATA_ON(mvcountResult, 0, 0);

					if (STR_EQUAL(mvcountOnMv, mvcountRequery)) { // del from mv
						
						RESULT(deleteResult);

						QUERY("delete from ");
						_QUERY(_MVIAU_MVName);
						_QUERY(" where true ");
						mvColumnCount = 0;
						for (K = 0; K < OBJ_NSELECTED; K++) {
							if (!OBJ_SELECTED_COL(K)->hasAggregateFunction) {
								char selectMVCountWhereClause[ESTIMATED_LENGTH];
								char *quote;
								char *ctype;
								ctype = createCType(OBJ_SELECTED_COL(K)->type);
								if (STR_EQUAL(ctype, "char *")) quote = "\'";
								else quote = "";
								sprintf(selectMVCountWhereClause, " and %s = %s%s%s ", OBJ_SELECTED_COL(K)->name, quote, DATA_ON(requeryResult, M, mvColumnCount++), quote);
								_QUERY(selectMVCountWhereClause);	
								FREE(ctype);
							}
						}

						EXECUTE_TO(deleteResult);
						if (!QUERY_EXECUTED_ON(deleteResult)) {
							printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
						}

						FREE_RESULT(deleteResult);
					} else { // update decrease -- count & sum
						RESULT(updateResult);

						QUERY("update mv1 set mvcount = mvcount - 1 ");
						for (K = 0; K < OBJ_NSELECTED-1; K++) {
							if (OBJ_SELECTED_COL(K)->hasAggregateFunction) {
								char updateMVselectClause[ESTIMATED_LENGTH];
								if (STR_EQUAL(OBJ_SELECTED_COL(K)->func, AF_SUM) || STR_EQUAL(OBJ_SELECTED_COL(K)->func, AF_COUNT)) {
									sprintf(updateMVselectClause, ", %s = %s - %s ", OBJ_SELECTED_COL(K)->varName, OBJ_SELECTED_COL(K)->varName, DATA_ON(requeryResult, M, K));
									_QUERY(updateMVselectClause);
								}
							}
						}
						_QUERY(" where true ");
						mvColumnCount = 0;
						for (K = 0; K < OBJ_NSELECTED; K++) {
							if (!OBJ_SELECTED_COL(K)->hasAggregateFunction) {
								char selectMVCountWhereClause[ESTIMATED_LENGTH];
								char *quote;
								char *ctype;
								ctype = createCType(OBJ_SELECTED_COL(K)->type);
								if (STR_EQUAL(ctype, "char *")) quote = "\'";
								else quote = "";
								sprintf(selectMVCountWhereClause, " and %s = %s%s%s ", OBJ_SELECTED_COL(K)->name, quote, DATA_ON(requeryResult, M, mvColumnCount++), quote);
								_QUERY(selectMVCountWhereClause);	
								FREE(ctype);
							}
						}
						EXECUTE_TO(updateResult);
						if (!QUERY_EXECUTED_ON(updateResult)) {
							printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
						}

						FREE_RESULT(updateResult);
					}
				}

				// Query to find max
				QUERY(OBJ_SELECT_CLAUSE);
				_QUERY(" from ");
				_QUERY(OBJ_SQ->from);
				_QUERY(" where true ");
				if (OBJ_SQ->hasWhere) {_QUERY(" and "); _QUERY(OBJ_SQ->where);}
				for (K = 0; K < OBJ_TG_NCOLS(log->objTableId); K++) {
					char *quote;
					char *ctype = createCType(OBJ_TG_COL(log->objTableId, K)->type);
					int ordinalPos = OBJ_TG_COL(log->objTableId, K)->ordinalPosition;
					if (STR_EQUAL(ctype, "char *")) { quote = "\'";}
					else { quote = "";}

					sprintf(sql, " and %s.%s = %s", OBJ_A2_TG_TABLE(log->objTableId, K), OBJ_TG_COL(log->objTableId, K)->name, quote);
					_QUERY(sql);

					if (ordinalPos <= log->nPk) {
						_QUERY(log->oldPrimaryKeys[ordinalPos - 1]);
					} else {
						_QUERY(log->oldNormalCols[ordinalPos - log->nPk - 1]);
					}

					_QUERY(quote);
					FREE(ctype);
				}
				if (OBJ_SQ->hasGroupby) {_QUERY(" group by "); _QUERY(OBJ_SQ->groupby);}
				if (OBJ_SQ->hasHaving) {_QUERY(" having "); _QUERY(OBJ_SQ->having);}				
				EXECUTE_TO(findMaxResult);

				FOR(M, findMaxResult) {
					RESULT(updateMaxResult);
					char *faculty_id = DATA_ON(findMaxResult, M, 0);
					char *faculty_name = DATA_ON(findMaxResult, M, 1);
					char *class_id = DATA_ON(findMaxResult, M, 2);
					char *class_name = DATA_ON(findMaxResult, M, 3);
					char *count = DATA_ON(findMaxResult, M, 4);
					char *_max= DATA_ON(findMaxResult, M, 5);
					QUERY("update mv1 set num_jskapaeshi = ");
					_QUERY(_max);
					_QUERY(" where true ");
					_QUERY(" and faculty_id = ");
					_QUERY(faculty_id);
					_QUERY(" and faculty_name = '");
					_QUERY(faculty_name);
					_QUERY("' ");
					_QUERY(" and class_id = ");
					_QUERY(class_id);
					_QUERY(" and class_name = '");
					_QUERY(class_name);
					_QUERY("' ");
					EXECUTE_TO(updateMaxResult);
					FREE_RESULT(updateMaxResult);
				}

				FREE_RESULT(findMaxResult);
			}
		}

		QUERY("update mviautg.mv_trace set updated_time = timestamp '");
		_QUERY(log->logTime);
		_QUERY("' + interval '1 milliseconds' where mv_id = (select mv_id from mviautg.mvs where mv_name = '");
		_QUERY(_MVIAU_MVName);
		_QUERY("')");
		//EXECUTE_TO(updateTime);
		if (!QUERY_EXECUTED_ON(updateTime)) {
			//printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
		}

		FREE_RESULT(updateTime);
		FREE_RESULT(localLog);
		delLog(&log);
	}

	/*fclose(masterFile);*/
	FREE(startingPoint);
	//FREE(masterFileName);
	//FREE(masterFilePath);
	END;
}