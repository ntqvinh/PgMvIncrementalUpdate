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
///#include <vld.h>

MAIN {
	
	// Loop variables
	int i, j, k, m;

	// Connection info
	char dbName[MAX_L_DBNAME], username[MAX_L_USERNAME], password[MAX_L_PASSWORD], port[MAX_L_PORT];
	char *connectionString;

	// PMVTG info
	char outputPath[MAX_L_OUTPUT_PATH], mvName[MAX_L_MV_NAME], outputFilesName[MAX_L_FILE_NAME];
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

	// Command that copies 'ctrigger.h' to output folder
	// 'ctrigger.h' is a necessary header for C-trigger included within generated C file
	copyCmd = copy("copy ");
	STR_APPEND(copyCmd, "ctrigger.h ");
	STR_APPEND(copyCmd, outputPath);
	STR_APPEND(copyCmd, "\\ctrigger.h");

	SYSTEM_EXEC(copyCmd);
	FREE(copyCmd);

	// Connect to database server
	scanf("%s%s%s%s", dbName, username, password, port);
	connectionString = copy("dbname = '");
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
		{ERR("Connection to database failed");}
	else 
		puts("Connect to database successfully!");

	// Load query's tables and columns, create: OBJ_TABLE
	for (i = 0; i < OBJ_SQ->fromElementsNum; i++) {
		initTable(&OBJ_TABLE(i));
		OBJ_TABLE(i)->name = copy(OBJ_SQ->fromElements[i]);

		// Create columns
		DEFQ("SELECT column_name, data_type, ordinal_position, character_maximum_length, is_nullable FROM information_schema.columns WHERE table_name = '");
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

	// Analyze selected columns list, expressions are separated to columns, create: OBJ_SELECTED_COL, OBJ_NSELECTED
	for (i = 0; i < OBJ_SQ->selectElementsNum; i++) {
		Boolean flag = addRealColumn(OBJ_SQ->selectElements[i], NULL);
		if (!flag) {
			char *remain, *tmp;
			char *AF[] = {AF_SUM, AF_COUNT, AF_AVG, AF_MIN, _AF_MAX};
			char *NF[] = {"abs("};

			char *delim = "()+*-/";
			char *filter[MAX_N_ELEMENTS];
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

	/*
		Analyze joining conditions

		IN: container includes conditions involved by inner column, used for trigger's FCC
		OUT: container includes conditions involved by outer column
		BIN: container includes conditions involved by both inner and outer column (usually join conditions)
		(BIN is sub set of OUT, separately saved for future dev)

		Create: OBJ_IN_CONDITIONS, OBJ_ OUT_CONDITIONS, OBJ_BIN_CONDITIONS, OBJ_FCC
	*/
	for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {

		// Check on WHERE
		if (OBJ_SQ->hasWhere)
			for (i = 0; i < OBJ_SQ->whereConditionsNum; i++)
				filterTriggerCondition(OBJ_SQ->whereConditions[i], j);

		// Check on JOIN
		if (OBJ_SQ->hasJoin)
			for (i = 0; i < OBJ_SQ->onNum; i++)
				for (k = 0; k < OBJ_SQ->onConditionsNum[i]; k++)
					filterTriggerCondition(OBJ_SQ->onConditions[i][k], j);

		// Check on HAVING
		if (OBJ_SQ->hasHaving)
			for (i = 0; i < OBJ_SQ->havingConditionsNum; i++)
				filterTriggerCondition(OBJ_SQ->havingConditions[i], j);

		/* CREATE TRIGGERS' FIRST CHECKING CONDITION */
		// 1. Embeding variables
		
		// For each IN condition
		for (k = 0; k < OBJ_IN_NCONDITIONS(j); k++) {
			char *conditionCode, *tmp;
			conditionCode = copy(OBJ_IN_CONDITIONS(j, k));

			// For each table
			for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
				char *colPos, *tableName;
				char *start, *end, *oldString, *draft, *checkVar;
				draft = conditionCode;

				// Find inner columns' position
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
		}

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
	}

	DISCONNECT;

	// Rebuild new select clause based on the original for algorithm purpose, create: OBJ_SELECT_CLAUSE
	OBJ_SELECT_CLAUSE = copy("select ");
	for (i = 0; i < OBJ_NSELECTED; i++) {
		char *name = copy(OBJ_SELECTED_COL(i)->name);
		if (OBJ_SELECTED_COL(i)->context == COLUMN_CONTEXT_FREE) {
			name = dammf_insertSubString(name, name, ".");
			name = dammf_insertSubString(name, name, OBJ_SELECTED_COL(i)->table->name);
		}
		STR_APPEND(OBJ_SELECT_CLAUSE, name);
		if (i != OBJ_NSELECTED-1)
			STR_APPEND(OBJ_SELECT_CLAUSE, ", ");
		free(name);
	}

	// Check for aggregate functions, especially MIN and/or MAX, define: OBJ_STATIC_BOOL_AGG, OBJ_STATIC_BOOL_AGG_MINMAX
	// OBJ_STATIC_BOOL_AGG is alias of OBJ_A1_CHECK
	// Check in selected list if there are only sum & count aggregate functions involved in query
	// Define OBJ_A2_CHECK (alias of OBJ_STATIC_BOOL_ONLY_AGG_SUMCOUNT)
	// Note: A2 is a calling mask of updated algorithm for query contains only sum & count aggregate functions
	// Note: Avg is turned into sum & count due to algorithm
	for (i = 0; i < OBJ_NSELECTED-1; i++) {
		if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {
			OBJ_STATIC_BOOL_AGG = TRUE;
			if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->func, AF_MIN) || STR_EQUAL_CI(OBJ_SELECTED_COL(i)->func, _AF_MAX)) {
				OBJ_STATIC_BOOL_AGG_MINMAX = TRUE;
			}
			if (STR_EQUAL_CI(OBJ_SELECTED_COL(i)->func, AF_SUM) || STR_EQUAL_CI(OBJ_SELECTED_COL(i)->func, AF_COUNT)) {
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

	// Create: OBJ_A2_WHERE_CLAUSE
	if (OBJ_A2_CHECK) {
		for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
			STR_INIT(OBJ_A2_WHERE_CLAUSE(j), " true ");

			// Preprocess out conditions & recombine
			for (i = 0; i < OBJ_OUT_NCONDITIONS(j); i++) {
				OBJ_OUT_CONDITIONS(j, i) = dammf_replaceFirst(OBJ_OUT_CONDITIONS(j, i), "==", "=");
				STR_APPEND(OBJ_A2_WHERE_CLAUSE(j), " and ");
				STR_APPEND(OBJ_A2_WHERE_CLAUSE(j), OBJ_OUT_CONDITIONS(j, i));
			}
		}
	}

	// Define: OBJ_AGG_INVOLVED_CHECK
	// Important requirement: full column name as format: <table-name>.<column-name>
	for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
		OBJ_AGG_INVOLVED_CHECK(j) = FALSE;
	}
	for (i = 0; i < OBJ_NSELECTED-1; i++) {
		if (OBJ_SELECTED_COL(i)->hasAggregateFunction && STR_INEQUAL(OBJ_SELECTED_COL(i)->funcArg, "*")) {
			char *columnList[MAX_N_ELEMENTS];
			int columnListLen = 0;
			split(OBJ_SELECTED_COL(i)->funcArg, "+-*/", columnList, &columnListLen, TRUE);
			for (k = 0; k < columnListLen; k++) {
				char *dotPos = strstr(columnList[k], ".");
				if (dotPos) {
					char *tableName = getPrecededTableName(columnList[k], dotPos+1);
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

	// Define: OBJ_KEY_IN_G_CHECK
	for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
		OBJ_KEY_IN_G_CHECK(j) = TRUE;
	}

	if (OBJ_SQ->hasGroupby) {
		for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
			for (k = 0; k < OBJ_TABLE(j)->nCols; k++) {
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
						break;
					}
				}
			}
		}
	}

	/*
		GENERATE CODE
	*/
	{
		{ // GENERATE MV'S SQL CODE
			// 'SQL' macro is an alias of 'sqlWriter' variable
			FILE *sqlWriter = fopen(sqlFile, "w");
			fprintf(SQL, "create table %s (\n", mvName);

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
					fprintf(SQL, "	%s %s%s", OBJ_SELECTED_COL(i)->name, OBJ_SELECTED_COL(i)->type, characterLength);
				else
					fprintf(SQL, "	%s %s%s", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->type, characterLength);

				if (i != OBJ_NSELECTED-1)
					fprintf(SQL, ",\n");
				FREE(characterLength);
				FREE(ctype);
			}
			fprintf(SQL, "\n);");

			for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
				// insert
				if (OBJ_AGG_INVOLVED_CHECK(j)) {
					fprintf(SQL, "\n\n/* insert on %s*/", OBJ_TABLE(j)->name);
					fprintf(SQL, "\ncreate function pmvtg_%s_insert_on_%s() returns trigger as '%s', 'pmvtg_%s_insert_on_%s' language c strict;", 
						mvName, OBJ_TABLE(j)->name, dllFile, mvName, OBJ_TABLE(j)->name);

					fprintf(SQL, "\ncreate trigger pgtg_%s_insert_on_%s after insert on %s for each row execute procedure pmvtg_%s_insert_on_%s();", 
						mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);
				}

				// delete
				fprintf(SQL, "\n\n/* delete on %s*/", OBJ_TABLE(j)->name);
				fprintf(SQL, "\ncreate function pmvtg_%s_delete_on_%s() returns trigger as '%s', 'pmvtg_%s_delete_on_%s' language c strict;", 
					mvName, OBJ_TABLE(j)->name, dllFile, mvName, OBJ_TABLE(j)->name);

				fprintf(SQL, "\ncreate trigger pgtg_%s_before_delete_on_%s before delete on %s for each row execute procedure pmvtg_%s_delete_on_%s();", 
					mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);

				if (!OBJ_KEY_IN_G_CHECK(j) && OBJ_STATIC_BOOL_AGG_MINMAX)
					fprintf(SQL, "\ncreate trigger pgtg_%s_after_delete_on_%s after delete on %s for each row execute procedure pmvtg_%s_delete_on_%s();", 
						mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);

				// update
				fprintf(SQL, "\n\n/* update on %s*/", OBJ_TABLE(j)->name);
				fprintf(SQL, "\ncreate function pmvtg_%s_update_on_%s() returns trigger as '%s', 'pmvtg_%s_update_on_%s' language c strict;", 
					mvName, OBJ_TABLE(j)->name, dllFile, mvName, OBJ_TABLE(j)->name);

				if ((!OBJ_A2_CHECK || !OBJ_AGG_INVOLVED_CHECK(j)) && (!OBJ_KEY_IN_G_CHECK(j) || OBJ_AGG_INVOLVED_CHECK(j))) {
					fprintf(SQL, "\ncreate trigger pgtg_%s_before_update_on_%s before update on %s for each row execute procedure pmvtg_%s_update_on_%s();", 
						mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);
				}

				fprintf(SQL, "\ncreate trigger pgtg_%s_after_update_on_%s after update on %s for each row execute procedure pmvtg_%s_update_on_%s();", 
					mvName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->name, mvName, OBJ_TABLE(j)->name);				
			}

			fclose(sqlWriter);
		} // --- END OF GENERATING MV'S SQL CODE

		{ // GENERATE C TRIGGER CODE
			// Macro 'F' is an alias of cWriter variable
			FILE *cWriter = fopen(triggerFile, "w");
			fprintf(F, "#include \"ctrigger.h\"\n\n");

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
					fprintf(F, "TRIGGER(%s) {\n", triggerName);
					FREE(triggerName);

					// Variable declaration
					fprintf(F, "	// MV's data\n");
					for (i = 0; i < OBJ_NSELECTED; i++) {
						char *varname = OBJ_SELECTED_COL(i)->varName;
						char *tableName =  "<expression>";
						if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
						fprintf(F, "	char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
					}

					// Table's vars
					fprintf(F, "	// Table's data\n");
					for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						char *varname = OBJ_TABLE(j)->cols[i]->varName;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						fprintf(F, "	%s %s;\n", ctype, varname);
						if (STR_INEQUAL(ctype, "char *")) {
							if (STR_EQUAL(ctype, "Numeric"))
								fprintf(F, "	char * str_%s;\n", varname);
							else
								fprintf(F, "	char str_%s[20];\n", varname);
						}
						FREE(ctype);
					}

					// Standard trigger preparing procedures
					fprintf(F, "\n	BEGIN;\n\n");

					// Get values of table's fields
					fprintf(F, "	// Get table's data\n");
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
						fprintf(F, "	%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
						if (STR_INEQUAL(ctype, "char *"))
							fprintf(F, "	%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
						FREE(ctype);
					}

					// FCC
					fprintf(F, "\n	// FCC\n");
					fprintf(F, "	if (%s) {\n", OBJ_FCC(j));

					// Re-query
					fprintf(F, "\n		// Re-query\n");
					fprintf(F, "		DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
					fprintf(F, "		ADDQ(\" from \");\n");
					fprintf(F, "		ADDQ(\"%s\");\n", OBJ_SQ->from);
					fprintf(F, "		ADDQ(\" where true \");\n");
					if (OBJ_SQ->hasWhere) fprintf(F, "		ADDQ(\" and %s\");\n", OBJ_SQ->where);
					for (i = 0; i < OBJ_TABLE(j)->nCols; i++)
						if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
							char *quote;
							char *strprefix;
							char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
							if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
							else { quote = ""; strprefix = "str_"; }
							fprintf(F, "		ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
							fprintf(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
							fprintf(F, "		ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}
					if (OBJ_SQ->hasGroupby) fprintf(F, "		ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
					if (OBJ_SQ->hasHaving) fprintf(F, "		ADDQ(\" having %s\");\n", OBJ_SQ->having);

					fprintf(F, "		EXEC;\n");

					// Allocate cache
					fprintf(F, "\n		// Allocate cache\n		DEFR(%d);\n", OBJ_NSELECTED);

					// Save dQ result
					fprintf(F, "\n		// Save dQ result\n		FOR_EACH_RESULT_ROW(i) {\n");

					for (i = 0; i < OBJ_NSELECTED; i++) {
						fprintf(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i+1);
						fprintf(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
					}

					fprintf(F, "		}\n");

					// For each dQi:
					fprintf(F, "\n		// For each dQi:\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

					k = 0;
					for (i = 0; i < OBJ_NSELECTED; i++)
						fprintf(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

					// Check if there is a similar row in MV
					fprintf(F, "\n			// Check if there is a similar row in MV\n");
					fprintf(F, "			DEFQ(\"select mvcount from %s where true \");\n", mvName);
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype;
							ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							fprintf(F, "			ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							fprintf(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							fprintf(F, "			ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}

					fprintf(F, "			EXEC;\n");

					fprintf(F, "\n			if (NO_ROW) {\n");
					fprintf(F, "				// insert new row to mv\n");
					fprintf(F, "				DEFQ(\"insert into %s values (\");\n", mvName);

					for (i = 0; i < OBJ_NSELECTED; i++) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *"))
							quote = "'";
						else quote = "";
						fprintf(F, "				ADDQ(\"%s\");\n", quote);
						fprintf(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						fprintf(F, "				ADDQ(\"%s\");\n", quote);
						if (i != OBJ_NSELECTED-1) fprintf(F, "				ADDQ(\", \");\n");
						FREE(ctype);
					}

					fprintf(F, "				ADDQ(\")\");\n");
					fprintf(F, "				EXEC;\n");

					fprintf(F, "			} else {\n");

					fprintf(F, "				// update old row\n");
					fprintf(F, "				DEFQ(\"update %s set \");\n", mvName);
					check = FALSE;
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {
					
							if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_SUM)) {
								if (check) fprintf(F, "				ADDQ(\", \");\n");
									check = TRUE;

								fprintf(F, "				ADDQ(\" %s = %s + \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							} else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_COUNT)) {
								if (check) fprintf(F, "				ADDQ(\", \");\n");
									check = TRUE;

								fprintf(F, "				ADDQ(\" %s = %s + \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							} else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_MIN)) {
								if (check) fprintf(F, "				ADDQ(\", \");\n");
									check = TRUE;

								fprintf(F, "				ADDQ(\" %s = (case when %s > \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "				ADDQ(\" then \");\n");
								fprintf(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "				ADDQ(\" else %s end) \");\n", OBJ_SELECTED_COL(i)->varName);
							} else {
								// AF_MAX
								if (check) fprintf(F, "				ADDQ(\", \");\n");
									check = TRUE;

								fprintf(F, "				ADDQ(\" %s = (case when %s < \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "				ADDQ(\" then \");\n");
								fprintf(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "				ADDQ(\" else %s end) \");\n", OBJ_SELECTED_COL(i)->varName);
							}
						}

					fprintf(F, "				ADDQ(\" where true \");\n");
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							fprintf(F, "				ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							fprintf(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							fprintf(F, "				ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}
					fprintf(F, "				EXEC;\n");

					fprintf(F, "			}\n");
					fprintf(F, "		}\n");

					// End of trigger
					fprintf(F, "\n	}\n\n	/*\n		Finish\n	*/\n	END;\n}\n\n");

				} // --- END OF CHECK POINT FOR INSERT EVENT (OBJ_AGG_INVOLVED_CHECK)
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
					fprintf(F, "TRIGGER(%s) {\n", triggerName);
					FREE(triggerName);

					if (!OBJ_KEY_IN_G_CHECK(j)) {
						// MV's vars
						fprintf(F, "	// MV's data\n");
						for (i = 0; i < OBJ_NSELECTED; i++) {
							char *varname = OBJ_SELECTED_COL(i)->varName;
							char *tableName =  "<expression>";
							if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
							fprintf(F, "	char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
						}
					}

					// Table's vars
					fprintf(F, "	// Table's data\n");
					for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn || (!OBJ_TABLE(j)->cols[i]->isPrimaryColumn && !OBJ_KEY_IN_G_CHECK(j))) {
							char *varname = OBJ_TABLE(j)->cols[i]->varName;
							char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
							fprintf(F, "	%s %s;\n", ctype, varname);
							if (STR_INEQUAL(ctype, "char *")) {
								if (STR_EQUAL(ctype, "Numeric"))
									fprintf(F, "	char * str_%s;\n", varname);
								else
									fprintf(F, "	char str_%s[20];\n", varname);
							}
							FREE(ctype);
						}
					}

					if (!OBJ_KEY_IN_G_CHECK(j)) {
						// Count
						fprintf(F, "	char * count;\n");
					}

					// Standard trigger preparing procedures
					fprintf(F, "\n	BEGIN;\n\n");

					// Get values of table's fields
					fprintf(F, "	// Get table's data\n");
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
							} else if (STR_EQUAL(ctype, "int64")) {
								func = "GET_INT64_ON_TRIGGERED_ROW";
								convert = "INT64_TO_STR";
							} else {
								func = "GET_NUMERIC_ON_TRIGGERED_ROW";
								convert = "NUMERIC_TO_STR";
							}
							fprintf(F, "	%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
							if (STR_INEQUAL(ctype, "char *"))
								fprintf(F, "	%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
							FREE(ctype);
						}
					}

					// FCC
					fprintf(F, "\n	// FCC\n");
					fprintf(F, "	if (%s) {\n", OBJ_FCC(j));

					if (OBJ_KEY_IN_G_CHECK(j)) {
						fprintf(F, "		DEFQ(\"delete from %s where true\");\n", mvName);
						for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
							if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
								char *quote;
								char *strprefix;
								char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
								if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
								else { quote = ""; strprefix = "str_"; }
								fprintf(F, "		ADDQ(\" and %s = %s\");\n", OBJ_TABLE(j)->cols[i]->name, quote);
								fprintf(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
								fprintf(F, "		ADDQ(\"%s \");\n", quote);
								FREE(ctype);
							}
						}
						fprintf(F, "		EXEC;\n");
					} else {
						if (OBJ_STATIC_BOOL_AGG_MINMAX)
							fprintf(F, "\n	if (UTRIGGER_FIRED_BEFORE) {\n");
							/*
								TRIGGER BODY START HERE
							*/
							// Re-query
							fprintf(F, "\n		// Re-query\n");
							fprintf(F, "		DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
							fprintf(F, "		ADDQ(\" from \");\n");
							fprintf(F, "		ADDQ(\"%s\");\n", OBJ_SQ->from);
							fprintf(F, "		ADDQ(\" where true \");\n");
							if (OBJ_SQ->hasWhere) fprintf(F, "		ADDQ(\" and %s\");\n", OBJ_SQ->where);
							for (i = 0; i < OBJ_TABLE(j)->nCols; i++)
								if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
									char *quote;
									char *strprefix;
									char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
									if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
									else { quote = ""; strprefix = "str_"; }
									fprintf(F, "		ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
									fprintf(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
									fprintf(F, "		ADDQ(\"%s \");\n", quote);
									FREE(ctype);
								}
							if (OBJ_SQ->hasGroupby) fprintf(F, "		ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
							if (OBJ_SQ->hasHaving) fprintf(F, "		ADDQ(\" having %s\");\n", OBJ_SQ->having);

							fprintf(F, "		EXEC;\n");

							// Allocate cache
							fprintf(F, "\n		// Allocate cache\n		DEFR(%d);\n", OBJ_NSELECTED);

							// Save dQ result
							fprintf(F, "\n		// Save dQ result\n		FOR_EACH_RESULT_ROW(i) {\n");

							for (i = 0; i < OBJ_NSELECTED; i++) {
								fprintf(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i+1);
								fprintf(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
							}

							fprintf(F, "		}\n");

							// For each dQi:
							fprintf(F, "\n		// For each dQi:\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

							k = 0;
							for (i = 0; i < OBJ_NSELECTED; i++)
								fprintf(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

							// Check if there is a similar row in MV
							fprintf(F, "\n			// Check if there is a similar row in MV\n");
							fprintf(F, "			DEFQ(\"select mvcount from %s where true \");\n", mvName);
							for (i = 0; i < OBJ_NSELECTED; i++)
								if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
									char *quote;
									char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
									if (STR_EQUAL(ctype, "char *")) quote = "\'";
									else quote = "";
									fprintf(F, "			ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
									fprintf(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
									fprintf(F, "			ADDQ(\"%s \");\n", quote);
									FREE(ctype);
								}

							fprintf(F, "			EXEC;\n");

							fprintf(F, "\n			GET_STR_ON_RESULT(count, 0, 1);\n");

							fprintf(F, "\n			if (STR_EQUAL(count, mvcount)) {\n");
							fprintf(F, "				// Delete the row\n");
							fprintf(F, "				DEFQ(\"delete from %s where true\");\n", mvName);

							for (i = 0; i < OBJ_NSELECTED; i++)
								if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
									char *quote;
									char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
									if (STR_EQUAL(ctype, "char *")) quote = "\'";
									else quote = "";
									fprintf(F, "				ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
									fprintf(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
									fprintf(F, "				ADDQ(\"%s \");\n", quote);
									FREE(ctype);
								}

							fprintf(F, "				EXEC;\n");

							fprintf(F, "			} else {\n");

							fprintf(F, "				// ow, decrease the count column \n");
							fprintf(F, "				DEFQ(\"update %s set \");\n", mvName);

							check = FALSE;
							for (i = 0; i < OBJ_NSELECTED; i++)
								if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {
									if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_SUM)) {
										if (check) fprintf(F, "				ADDQ(\", \");\n");
										check = TRUE;

										fprintf(F, "				ADDQ(\" %s = %s - \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
										fprintf(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
									} else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_COUNT)) {
										if (check) fprintf(F, "				ADDQ(\", \");\n");
										check = TRUE;

										fprintf(F, "				ADDQ(\" %s = %s - \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
										fprintf(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
									}
								}
							fprintf(F, "				ADDQ(\" where true \");\n");
							for (i = 0; i < OBJ_NSELECTED; i++)
								if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
									char *quote;
									char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
									if (STR_EQUAL(ctype, "char *")) quote = "\'";
									else quote = "";
									fprintf(F, "				ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
									fprintf(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
									fprintf(F, "				ADDQ(\"%s \");\n", quote);
									FREE(ctype);
								}
							fprintf(F, "				EXEC;\n");

							fprintf(F, "			}\n");
							fprintf(F, "		}\n");

							// --------------------------------------------------
							if (OBJ_STATIC_BOOL_AGG_MINMAX) {
								fprintf(F, "\n	} else {\n");

								fprintf(F, "		DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
								fprintf(F, "		ADDQ(\" from \");\n");
								fprintf(F, "		ADDQ(\"%s\");\n", OBJ_SQ->from);
								fprintf(F, "		ADDQ(\" where true \");\n");
								if (OBJ_SQ->hasWhere) fprintf(F, "		ADDQ(\" and %s\");\n", OBJ_SQ->where);
								for (i = 0; i < OBJ_TG_NCOLS(j); i++) {
									char *quote;
									char *strprefix;
									char *ctype = createCType(OBJ_TG_COL(j, i)->type);
									if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
									else { quote = ""; strprefix = "str_"; }
									fprintf(F, "		ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TG_COL(j, i)->name, quote);
									fprintf(F, "		ADDQ(%s%s);\n", strprefix, OBJ_TG_COL(j, i)->varName);
									fprintf(F, "		ADDQ(\"%s \");\n", quote);
									FREE(ctype);
								}
								if (OBJ_SQ->hasGroupby) fprintf(F, "		ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
								if (OBJ_SQ->hasHaving) fprintf(F, "			ADDQ(\" having %s\");\n", OBJ_SQ->having);

								fprintf(F, "		EXEC;\n");

								fprintf(F, "\n		DEFR(%d);\n", OBJ_NSELECTED);

								fprintf(F, "\n		FOR_EACH_RESULT_ROW(i) {\n");

								for (i = 0; i < OBJ_NSELECTED; i++) {
									fprintf(F, "			GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i+1);
									fprintf(F, "			ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
								}

								fprintf(F, "		}\n");

								// For each dQi:
								fprintf(F, "\n		// For each dQi:\n		FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

								k = 0;
								for (i = 0; i < OBJ_NSELECTED; i++)
									fprintf(F, "			%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

								fprintf(F, "			DEFQ(\"update %s set mvcount = mvcount \");\n", mvName);

								for (i = 0; i < OBJ_NSELECTED; i++) {
									if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_MIN) || STR_EQUAL(OBJ_SELECTED_COL(i)->func, _AF_MAX)) {
										fprintf(F, "			ADDQ(\", %s = \");\n", OBJ_SELECTED_COL(i)->varName);
										fprintf(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
									}
								}

								fprintf(F, "			ADDQ(\" where true \");\n");
								for (i = 0; i < OBJ_NSELECTED; i++)
									if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
										char *quote;
										char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
										if (STR_EQUAL(ctype, "char *")) quote = "\'";
										else quote = "";
										fprintf(F, "			ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
										fprintf(F, "			ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
										fprintf(F, "			ADDQ(\"%s \");\n", quote);
										FREE(ctype);
									}
								fprintf(F, "			EXEC;\n");

								fprintf(F, "		}\n");

								fprintf(F, "\n	}\n");
							}
					}

					// End of trigger
					fprintf(F, "\n	}\n\n	END;\n}\n\n");

				} // --- END OF CHECK POINT FOR DELETE EVENT
				// --- END OF DELETE EVENT

				/*******************************************************************************************************
															UPDATE EVENT 
				********************************************************************************************************/
				if (OBJ_A2_CHECK && OBJ_AGG_INVOLVED_CHECK(j)) {
					Boolean check;
					// Define trigger function
					char *triggerName;
					STR_INIT(triggerName, "pmvtg_");
					STR_APPEND(triggerName, mvName);
					STR_APPEND(triggerName, "_update_on_");
					STR_APPEND(triggerName, OBJ_TABLE(j)->name);
					fprintf(F, "TRIGGER(%s) {\n", triggerName);
					FREE(triggerName);

					// MV's data (old and new rows)
					fprintf(F, "	// MV's data (old and new rows)\n", triggerName);
					for (i = 0; i < OBJ_NSELECTED; i++) {
						char *varname = OBJ_SELECTED_COL(i)->varName;
						char *tableName =  "<expression>";
						if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
						if (i < OBJ_NSELECTED - 1) {
							fprintf(F, "	char *old_%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
							fprintf(F, "	char *new_%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
						} else {
							fprintf(F, "	char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
						}
					}

					// Old and new rows' data
					fprintf(F, "	// Old and new rows' data\n");
					for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						char *varname = OBJ_TABLE(j)->cols[i]->varName;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						fprintf(F, "	%s old_%s;\n", ctype, varname);
						if (STR_INEQUAL(ctype, "char *")) {
							if (STR_EQUAL(ctype, "Numeric"))
								fprintf(F, "	char * old_str_%s;\n", varname);
							else
								fprintf(F, "	char old_str_%s[20];\n", varname);
						}
						fprintf(F, "	%s new_%s;\n", ctype, varname);
						if (STR_INEQUAL(ctype, "char *")) {
							if (STR_EQUAL(ctype, "Numeric"))
								fprintf(F, "	char * new_str_%s;\n", varname);
							else
								fprintf(F, "	char new_str_%s[20];\n", varname);
						}

						FREE(ctype);
					}
					fprintf(F, "	char * old_count;");
					fprintf(F, "\n	char * new_count;\n");

					// Begin
					fprintf(F, "\n	BEGIN;\n\n");

					// Get data of old rows
					fprintf(F, "	// Get data of old rows\n");
					for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						char *func;
						char *convert = "";
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						if (STR_EQUAL(ctype, "char *"))
							func = "GET_STR_ON_TRIGGERED_ROW";
						else if (STR_EQUAL(ctype, "int")) {
							func = "GET_INT32_ON_TRIGGERED_ROW";
							convert = "INT32_TO_STR";
						} else if (STR_EQUAL(ctype, "int64")) {
							func = "GET_INT64_ON_TRIGGERED_ROW";
							convert = "INT64_TO_STR";
						} else {
							func = "GET_NUMERIC_ON_TRIGGERED_ROW";
							convert = "NUMERIC_TO_STR";
						}
						fprintf(F, "	%s(old_%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
						if (STR_INEQUAL(ctype, "char *"))
							fprintf(F, "	%s(old_%s, old_str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);

						FREE(ctype);
					}

					// Get data of new rows
					fprintf(F, "	// Get data of new rows\n");
					for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						char *func;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						char *convert = "";
						if (STR_EQUAL(ctype, "char *"))
							func = "GET_STR_ON_NEW_ROW";
						else if (STR_EQUAL(ctype, "int")) {
							func = "GET_INT32_ON_NEW_ROW";
							convert = "INT32_TO_STR";
						} else if (STR_EQUAL(ctype, "int64")) {
							func = "GET_INT64_ON_NEW_ROW";
							convert = "INT64_TO_STR";
						} else {
							func = "GET_NUMERIC_ON_NEW_ROW";
							convert = "NUMERIC_TO_STR";
						}
						fprintf(F, "	%s(new_%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
						if (STR_INEQUAL(ctype, "char *"))
							fprintf(F, "	%s(new_%s, new_str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
						FREE(ctype);
					}

					// FCC
					fprintf(F, "\n	// FCC\n");
					fprintf(F, "	if ((%s) || (%s)) {\n", OBJ_FCC_OLD(j), OBJ_FCC_NEW(j));
					// fprintf(F, "		INIT_UPDATE_A2_FLAG;\n");

					// Query
					fprintf(F, "		QUERY(\"select \");\n");
					for (i = 0; i < OBJ_NSELECTED; i++) {
						if (OBJ_SELECTED_COL(i)->table) {
							fprintf(F, "		_QUERY(\"%s.%s\");\n", OBJ_SELECTED_COL(i)->table->name, OBJ_SELECTED_COL(i)->name);
						} else {
							int nParts, partsType[MAX_N_ELEMENTS];
							char *parts[MAX_N_ELEMENTS];
							A2Split(j, OBJ_SELECTED_COL(i)->funcArg, &nParts, partsType, parts);

							fprintf(F, "		_QUERY(\"%s\");\n", OBJ_SELECTED_COL(i)->func);
							for (k = 0; k < nParts; k++) {
								if (partsType[k] != j) {
									fprintf(F, "		_QUERY(\"%s\");\n", parts[k]);
								} else {
									fprintf(F, "		_QUERY(old_%s);\n", parts[k]);
								}
							}

							fprintf(F, "		_QUERY(\")\");\n");

							for (k = 0; k < nParts; k++) {
								FREE(parts[k]);
							}
						}
						if (i < OBJ_NSELECTED - 1) {
							fprintf(F, "		_QUERY(\",\");\n");
						}
					}
					fprintf(F, "		_QUERY(\" from \");\n");
					check = FALSE;
					for (i = 0; i < OBJ_SQ->fromElementsNum; i++) {
						if (i != j) {
							if (check)
								fprintf(F, "		_QUERY(\", %s\");\n", OBJ_TABLE(i)->name);
							else
								fprintf(F, "		_QUERY(\"%s\");\n", OBJ_TABLE(i)->name);
							check = TRUE;
						}
					}
					fprintf(F, "		_QUERY(\" where %s \");\n", OBJ_A2_WHERE_CLAUSE(j));
					/*for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
							char *quote;
							char *strprefix;
							char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
							if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = "old_"; }
							else { quote = ""; strprefix = "old_str_"; }
							fprintf(F, "		_QUERY(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
							fprintf(F, "		_QUERY(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
							fprintf(F, "		_QUERY(\"%s \");\n", quote);
							FREE(ctype);
						}
					}*/
					for (i = 0; i < OBJ_TG_NCOLS(j); i++) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_TG_COL(j, i)->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = "old_"; }
						else { quote = ""; strprefix = "old_str_"; }
						fprintf(F, "		_QUERY(\" and %s.%s = %s\");\n", OBJ_A2_TG_TABLE(j, i), OBJ_TG_COL(j, i)->name, quote);
						fprintf(F, "		_QUERY(%s%s);\n", strprefix, OBJ_TG_COL(j, i)->varName);
						fprintf(F, "		_QUERY(\"%s \");\n", quote);
						FREE(ctype);
					}
					fprintf(F, "		_QUERY(\" group by %s \");\n", OBJ_SQ->groupby);
					fprintf(F, "		_QUERY(\" union all \");\n");
					//fprintf(F, "		_QUERY(\" %s \");\n", OBJ_SELECT_CLAUSE);
					fprintf(F, "		_QUERY(\"select \");\n");
					for (i = 0; i < OBJ_NSELECTED; i++) {
						if (OBJ_SELECTED_COL(i)->table) {
							fprintf(F, "		_QUERY(\"%s.%s\");\n", OBJ_SELECTED_COL(i)->table->name, OBJ_SELECTED_COL(i)->name);
						} else {
							int nParts, partsType[MAX_N_ELEMENTS];
							char *parts[MAX_N_ELEMENTS];
							A2Split(j, OBJ_SELECTED_COL(i)->funcArg, &nParts, partsType, parts);

							fprintf(F, "		_QUERY(\"%s\");\n", OBJ_SELECTED_COL(i)->func);
							for (k = 0; k < nParts; k++) {
								if (partsType[k] != j) {
									fprintf(F, "		_QUERY(\"%s\");\n", parts[k]);
								} else {
									fprintf(F, "		_QUERY(new_%s);\n", parts[k]);
								}
							}

							fprintf(F, "		_QUERY(\")\");\n");

							for (k = 0; k < nParts; k++) {
								FREE(parts[k]);
							}
						}
						if (i < OBJ_NSELECTED - 1) {
							fprintf(F, "		_QUERY(\",\");\n");
						}
					}
					fprintf(F, "		_QUERY(\" from \");\n");
					check = FALSE;
					for (i = 0; i < OBJ_SQ->fromElementsNum; i++) {
						if (i != j) {
							if (check)
								fprintf(F, "		_QUERY(\", %s\");\n", OBJ_TABLE(i)->name);
							else
								fprintf(F, "		_QUERY(\"%s\");\n", OBJ_TABLE(i)->name);
							check = TRUE;
						}
					}
					fprintf(F, "		_QUERY(\" where %s \");\n", OBJ_A2_WHERE_CLAUSE(j));
					/*for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
							char *quote;
							char *strprefix;
							char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
							if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = "new_"; }
							else { quote = ""; strprefix = "new_str_"; }
							fprintf(F, "		_QUERY(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
							fprintf(F, "		_QUERY(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
							fprintf(F, "		_QUERY(\"%s \");\n", quote);
							FREE(ctype);
						}
					}*/
					for (i = 0; i < OBJ_TG_NCOLS(j); i++) {
						char *quote;
						char *strprefix;
						char *ctype = createCType(OBJ_TG_COL(j, i)->type);
						if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = "new_"; }
						else { quote = ""; strprefix = "new_str_"; }
						fprintf(F, "		_QUERY(\" and %s.%s = %s\");\n", OBJ_A2_TG_TABLE(j, i), OBJ_TG_COL(j, i)->name, quote);
						fprintf(F, "		_QUERY(%s%s);\n", strprefix, OBJ_TG_COL(j, i)->varName);
						fprintf(F, "		_QUERY(\"%s \");\n", quote);
						FREE(ctype);
					}
					fprintf(F, "		_QUERY(\" group by %s \");\n", OBJ_SQ->groupby);
					fprintf(F, "		EXECUTE_QUERY;\n\n");

					fprintf(F, "		ALLOCATE_CACHE;\n\n");

					fprintf(F, "		A2_INIT_CYCLE;\n\n");

					fprintf(F, "		FOR_EACH_RESULT_ROW(i) {\n");
					fprintf(F, "			if (A2_VALIDATE_CYCLE) {\n");
					//fprintf(F, "			if (%s) {\n", OBJ_FCC_OLD(j));
					for (i = 0; i < OBJ_NSELECTED-1; i++) {
						fprintf(F, "				GET_STR_ON_RESULT(old_%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i+1);
						fprintf(F, "				SAVE_TO_CACHE(old_%s);\n", OBJ_SELECTED_COL(i)->varName);
					}
					fprintf(F, "				GET_STR_ON_RESULT(old_count, i, %d);\n", OBJ_NSELECTED);
					fprintf(F, "				SAVE_TO_CACHE(old_count);\n");
					//fprintf(F, "			}\n");
					fprintf(F, "			} else {\n");
					//fprintf(F, "			if (%s) {\n", OBJ_FCC_NEW(j));
					for (i = 0; i < OBJ_NSELECTED-1; i++) {
						fprintf(F, "				GET_STR_ON_RESULT(new_%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i+1);
						fprintf(F, "				SAVE_TO_CACHE(new_%s);\n", OBJ_SELECTED_COL(i)->varName);
					}
					fprintf(F, "				GET_STR_ON_RESULT(new_count, i, %d);\n", OBJ_NSELECTED);
					fprintf(F, "				SAVE_TO_CACHE(new_count);\n");

					//fprintf(F, "			}\n");
					fprintf(F, "			}\n");
					fprintf(F, "		}\n\n");

					fprintf(F, "		A2_INIT_CYCLE;\n\n");

					fprintf(F, "		FOR_EACH_CACHE_ROW(i) {\n");
					fprintf(F, "			if (A2_VALIDATE_CYCLE) {\n");
					fprintf(F, "			if (%s) {\n", OBJ_FCC_OLD(j));
					for (i = 0; i < OBJ_NSELECTED-1; i++)
						fprintf(F, "				old_%s = GET_FROM_CACHE(i, %d);\n", OBJ_SELECTED_COL(i)->varName, i);
					fprintf(F, "				old_count = GET_FROM_CACHE(i, %d);\n\n", OBJ_NSELECTED-1);
					
					fprintf(F, "				QUERY(\"select mvcount from %s where true \");\n", mvName);
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							fprintf(F, "				_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							fprintf(F, "				_QUERY(old_%s);\n", OBJ_SELECTED_COL(i)->varName);
							fprintf(F, "				_QUERY(\"%s \");\n", quote);
							FREE(ctype);
						}

					fprintf(F, "				EXECUTE_QUERY;\n\n");
					fprintf(F, "				GET_STR_ON_RESULT(mvcount, 0, 1);\n\n");

					fprintf(F, "				if (STR_EQUAL(old_count, mvcount)) {\n");
					fprintf(F, "					QUERY(\"delete from %s where true\");\n", mvName);
					
					for (i = 0; i < OBJ_NSELECTED; i++) {
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							fprintf(F, "					_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							fprintf(F, "					_QUERY(old_%s);\n", OBJ_SELECTED_COL(i)->varName);
							fprintf(F, "					_QUERY(\"%s \");\n", quote);
							FREE(ctype);
						}
					}

					fprintf(F, "					EXECUTE_QUERY;\n\n");

					fprintf(F, "				} else {\n");

					fprintf(F, "					QUERY(\"update %s set \");\n", mvName);

					check = FALSE;
					for (i = 0; i < OBJ_NSELECTED-1; i++) {
						if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {
					
							if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_SUM)) {
								if (check) fprintf(F, "					_QUERY(\", \");\n");
								check = TRUE;

								fprintf(F, "					_QUERY(\" %s = %s - \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "					_QUERY(old_%s);\n", OBJ_SELECTED_COL(i)->varName);
							} else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_COUNT)) {
								if (check) fprintf(F, "					_QUERY(\", \");\n");
									check = TRUE;

								fprintf(F, "					_QUERY(\" %s = %s - \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "					_QUERY(old_%s);\n", OBJ_SELECTED_COL(i)->varName);
							}
						}
					}
					if (check) fprintf(F, "					_QUERY(\", \");\n");
					fprintf(F, "					_QUERY(\" mvcount = mvcount - \");\n");
					fprintf(F, "					_QUERY(old_count);\n");

					fprintf(F, "					_QUERY(\" where true \");\n");
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							fprintf(F, "					_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							fprintf(F, "					_QUERY(old_%s);\n", OBJ_SELECTED_COL(i)->varName);
							fprintf(F, "					_QUERY(\"%s \");\n", quote);
							FREE(ctype);
						}
					fprintf(F, "					EXECUTE_QUERY;\n");
					fprintf(F, "				}\n");
					fprintf(F, "			}\n");
					fprintf(F, "			} else {\n");
					fprintf(F, "			if (%s) {\n", OBJ_FCC_NEW(j));

					for (i = 0; i < OBJ_NSELECTED-1; i++)
						fprintf(F, "				new_%s = GET_FROM_CACHE(i, %d);\n", OBJ_SELECTED_COL(i)->varName, i);
					fprintf(F, "				new_count = GET_FROM_CACHE(i, %d);\n\n", OBJ_NSELECTED-1);
					
					fprintf(F, "				QUERY(\"select mvcount from %s where true \");\n", mvName);
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							fprintf(F, "				_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							fprintf(F, "				_QUERY(new_%s);\n", OBJ_SELECTED_COL(i)->varName);
							fprintf(F, "				_QUERY(\"%s \");\n", quote);
							FREE(ctype);
						}

					fprintf(F, "				EXECUTE_QUERY;\n\n");

					fprintf(F, "				if (NO_ROW) {\n");
					fprintf(F, "					QUERY(\"insert into %s values (\");\n", mvName);

					for (i = 0; i < OBJ_NSELECTED-1; i++) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *"))
							quote = "'";
						else quote = "";
						fprintf(F, "					_QUERY(\"%s\");\n", quote);
						fprintf(F, "					_QUERY(new_%s);\n", OBJ_SELECTED_COL(i)->varName);
						fprintf(F, "					_QUERY(\"%s\");\n", quote);
						fprintf(F, "					_QUERY(\", \");\n");
						FREE(ctype);
					}
					fprintf(F, "					_QUERY(new_count);\n");

					fprintf(F, "					_QUERY(\")\");\n");
					fprintf(F, "					EXECUTE_QUERY;\n");

					fprintf(F, "				} else {\n");
					fprintf(F, "					QUERY(\"update %s set \");\n", mvName);

					check = FALSE;
					for (i = 0; i < OBJ_NSELECTED-1; i++) {
						if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_SUM)) {
								if (check) fprintf(F, "					ADDQ(\", \");\n");
									check = TRUE;

								fprintf(F, "					ADDQ(\" %s = %s + \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "					ADDQ(new_%s);\n", OBJ_SELECTED_COL(i)->varName);
							} else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_COUNT)){
								if (check) fprintf(F, "					ADDQ(\", \");\n");
									check = TRUE;

								fprintf(F, "					ADDQ(\" %s = %s + \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "					ADDQ(new_%s);\n", OBJ_SELECTED_COL(i)->varName);
							}
						}
					}
					if (check) fprintf(F, "					_QUERY(\", \");\n");
					fprintf(F, "					_QUERY(\" mvcount = mvcount + \");\n");
					fprintf(F, "					_QUERY(new_count);\n");

					fprintf(F, "					_QUERY(\" where true \");\n");
					for (i = 0; i < OBJ_NSELECTED; i++) {
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							fprintf(F, "					_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							fprintf(F, "					_QUERY(new_%s);\n", OBJ_SELECTED_COL(i)->varName);
							fprintf(F, "					_QUERY(\"%s \");\n", quote);
							FREE(ctype);
						}
					}
					fprintf(F, "					EXECUTE_QUERY;\n");
					fprintf(F, "				}\n");
					fprintf(F, "			}\n");
					fprintf(F, "			}\n");
					fprintf(F, "		}\n");
					fprintf(F, "	}\n");
					fprintf(F, "	END;\n");
					fprintf(F, "}\n\n");

				} else if (OBJ_KEY_IN_G_CHECK(j) && !OBJ_AGG_INVOLVED_CHECK(j)) {
					Boolean check;
					// Define trigger function
					char *triggerName;
					STR_INIT(triggerName, "pmvtg_");
					STR_APPEND(triggerName, mvName);
					STR_APPEND(triggerName, "_update_on_");
					STR_APPEND(triggerName, OBJ_TABLE(j)->name);
					fprintf(F, "TRIGGER(%s) {\n", triggerName);
					FREE(triggerName);

					// Old and new rows' data
					fprintf(F, "	// Old and new rows' data\n", triggerName);
					for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						char *varname = OBJ_TABLE(j)->cols[i]->varName;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						fprintf(F, "	%s old_%s;\n", ctype, varname);
						if (STR_INEQUAL(ctype, "char *")) {
							if (STR_EQUAL(ctype, "Numeric"))
								fprintf(F, "	char * old_str_%s;\n", varname);
							else
								fprintf(F, "	char old_str_%s[20];\n", varname);
						}

						fprintf(F, "	%s new_%s;\n", ctype, varname);
						if (STR_INEQUAL(ctype, "char *")) {
							if (STR_EQUAL(ctype, "Numeric"))
								fprintf(F, "	char * new_str_%s;\n", varname);
							else
								fprintf(F, "	char new_str_%s[20];\n", varname);
						}

						FREE(ctype);


					}

					// Begin
					fprintf(F, "\n	BEGIN;\n\n");

					// Get data of old rows
					fprintf(F, "	// Get data of old rows\n");
					for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						char *func;
						char *convert = "";
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						if (STR_EQUAL(ctype, "char *"))
							func = "GET_STR_ON_TRIGGERED_ROW";
						else if (STR_EQUAL(ctype, "int")) {
							func = "GET_INT32_ON_TRIGGERED_ROW";
							convert = "INT32_TO_STR";
						} else if (STR_EQUAL(ctype, "int64")) {
							func = "GET_INT64_ON_TRIGGERED_ROW";
							convert = "INT64_TO_STR";
						} else {
							func = "GET_NUMERIC_ON_TRIGGERED_ROW";
							convert = "NUMERIC_TO_STR";
						}
						fprintf(F, "	%s(old_%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
						if (STR_INEQUAL(ctype, "char *"))
							fprintf(F, "	%s(old_%s, old_str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);

						FREE(ctype);
					}

					// Get data of new rows
					fprintf(F, "	// Get data of new rows\n");
					for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						char *func;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						char *convert = "";
						if (STR_EQUAL(ctype, "char *"))
							func = "GET_STR_ON_NEW_ROW";
						else if (STR_EQUAL(ctype, "int")) {
							func = "GET_INT32_ON_NEW_ROW";
							convert = "INT32_TO_STR";
						} else if (STR_EQUAL(ctype, "int64")) {
							func = "GET_INT64_ON_NEW_ROW";
							convert = "INT64_TO_STR";
						} else {
							func = "GET_NUMERIC_ON_NEW_ROW";
							convert = "NUMERIC_TO_STR";
						}
						fprintf(F, "	%s(new_%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
						if (STR_INEQUAL(ctype, "char *"))
							fprintf(F, "	%s(new_%s, new_str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
						FREE(ctype);
					}

					// FCC
					fprintf(F, "\n	// FCC\n");
					fprintf(F, "	if (%s) {\n", OBJ_FCC(j));

					fprintf(F, "		QUERY(\"update %s set \");\n", mvName);
					check = FALSE;
					for (i = 0; i < OBJ_NSELECTED; i++) {
						if (OBJ_SELECTED_COL(i)->context == COLUMN_CONTEXT_FREE 
							&& OBJ_SELECTED_COL(i)->table == OBJ_TABLE(j)) {
							char *quote;
							char *strprefix;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
							else { quote = ""; strprefix = "str_"; }

							if (check) fprintf(F, "		_QUERY(\", \");\n");
							check = TRUE;

							fprintf(F, "		_QUERY(\" %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							fprintf(F, "		_QUERY(new_%s%s_table);\n", strprefix, OBJ_SELECTED_COL(i)->varName);
							fprintf(F, "		_QUERY(\"%s \");\n", quote);
							FREE(ctype);
						}
					}
					fprintf(F, "		_QUERY(\" where true \");\n");
					for (i = 0; i < OBJ_NSELECTED; i++) {
						if (OBJ_SELECTED_COL(i)->context == COLUMN_CONTEXT_FREE 
							&& OBJ_SELECTED_COL(i)->isPrimaryColumn
							&& OBJ_SELECTED_COL(i)->table == OBJ_TABLE(j)) {
							char *quote;
							char *strprefix;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
							else { quote = ""; strprefix = "str_"; }

							fprintf(F, "		_QUERY(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							fprintf(F, "		_QUERY(old_%s%s_table);\n", strprefix, OBJ_SELECTED_COL(i)->varName);
							fprintf(F, "		_QUERY(\"%s \");\n", quote);
							FREE(ctype);
						}
					}
					fprintf(F, "		EXECUTE_QUERY;\n");
					fprintf(F, "	}\n");
					fprintf(F, "	END;\n");
					fprintf(F, "}\n\n");
				} else {
					Boolean check;
					// Define trigger function
					char *triggerName;
					STR_INIT(triggerName, "pmvtg_");
					STR_APPEND(triggerName, mvName);
					STR_APPEND(triggerName, "_update_on_");
					STR_APPEND(triggerName, OBJ_TABLE(j)->name);
					fprintf(F, "TRIGGER(%s) {\n", triggerName);
					FREE(triggerName);

					// Variable declaration
					fprintf(F, "	/*\n		Variables declaration\n	*/\n");
					// MV's vars
					fprintf(F, "	// MV's vars\n");
					for (i = 0; i < OBJ_NSELECTED; i++) {
						char *varname = OBJ_SELECTED_COL(i)->varName;
						char *tableName =  "<expression>";
						if (OBJ_SELECTED_COL(i)->table) tableName = OBJ_SELECTED_COL(i)->table->name;
						fprintf(F, "	char *%s; // %s (%s)\n", varname, OBJ_SELECTED_COL(i)->name, tableName);
					}

					// Table's vars
					fprintf(F, "	// %s table's vars\n", OBJ_TABLE(j)->name);
					for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						char *varname = OBJ_TABLE(j)->cols[i]->varName;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						fprintf(F, "	%s %s;\n", ctype, varname);
						if (STR_INEQUAL(ctype, "char *")) {
							if (STR_EQUAL(ctype, "Numeric"))
								fprintf(F, "	char * str_%s;\n", varname);
							else
								fprintf(F, "	char str_%s[20];\n", varname);
						}
						FREE(ctype);
					}

					fprintf(F, "\n	char * count;\n");

					// Standard trigger preparing procedures
					fprintf(F, "\n	/*\n		Standard trigger preparing procedures\n	*/\n	REQUIRED_PROCEDURES;\n\n");

					// DELETE OLD
					fprintf(F, "	if (UTRIGGER_FIRED_BEFORE) {\n		/* DELETE OLD*/\n");

					// Get values of table's fields
					fprintf(F, "		// Get values of table's fields\n");
					for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						char *func;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						char *convert = "";
						if (STR_EQUAL(ctype, "char *"))
							func = "GET_STR_ON_TRIGGERED_ROW";
						else if (STR_EQUAL(ctype, "int")) {
							func = "GET_INT32_ON_TRIGGERED_ROW";
							convert = "INT32_TO_STR";
						} else if (STR_EQUAL(ctype, "int64")) {
							func = "GET_INT64_ON_TRIGGERED_ROW";
							convert = "INT64_TO_STR";
						} else {
							func = "GET_NUMERIC_ON_TRIGGERED_ROW";
							convert = "NUMERIC_TO_STR";
						}
						fprintf(F, "		%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
						if (STR_INEQUAL(ctype, "char *"))
							fprintf(F, "		%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
						FREE(ctype)
					}

					// FCC
					fprintf(F, "\n		// FCC\n");
					fprintf(F, "		if (%s) {\n", OBJ_FCC(j));

					/*
						TRIGGER BODY START HERE
					*/
					// Re-query
					fprintf(F, "\n			// Re-query\n");
					fprintf(F, "			DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
					fprintf(F, "			ADDQ(\" from \");\n");
					fprintf(F, "			ADDQ(\"%s\");\n", OBJ_SQ->from);
					fprintf(F, "			ADDQ(\" where true \");\n");
					if (OBJ_SQ->hasWhere) fprintf(F, "			ADDQ(\" and %s\");\n", OBJ_SQ->where);
					for (i = 0; i < OBJ_TABLE(j)->nCols; i++)
						if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
							char *quote;
							char *strprefix;
							char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
							if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
							else { quote = ""; strprefix = "str_"; }
							fprintf(F, "			ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
							fprintf(F, "			ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
							fprintf(F, "			ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}
					if (OBJ_SQ->hasGroupby) fprintf(F, "			ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
					if (OBJ_SQ->hasHaving) fprintf(F, "			ADDQ(\" having %s\");\n", OBJ_SQ->having);

					fprintf(F, "			EXEC;\n");

					// Allocate SAVED_RESULT set
					fprintf(F, "\n			// Allocate SAVED_RESULT set\n			DEFR(%d);\n", OBJ_NSELECTED);

					// Save dQ result
					fprintf(F, "\n			// Save dQ result\n			FOR_EACH_RESULT_ROW(i) {\n");

					for (i = 0; i < OBJ_NSELECTED; i++) {
						fprintf(F, "				GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i+1);
						fprintf(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
					}

					fprintf(F, "			}\n");

					// For each dQi:
					fprintf(F, "\n			// For each dQi:\n			FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

					k = 0;
					for (i = 0; i < OBJ_NSELECTED; i++)
						fprintf(F, "				%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

					// Check if there is a similar row in MV
					fprintf(F, "\n				// Check if there is a similar row in MV\n");
					fprintf(F, "				DEFQ(\"select mvcount from %s where true \");\n", mvName);
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							fprintf(F, "				ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							fprintf(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							fprintf(F, "				ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}

					fprintf(F, "				EXEC;\n");

					fprintf(F, "\n				GET_STR_ON_RESULT(count, 0, 1);\n");

					fprintf(F, "\n				if (STR_EQUAL(count, mvcount)) {\n");
					fprintf(F, "					// Delete the row\n");
					fprintf(F, "					DEFQ(\"delete from %s where true\");\n", mvName);

					for (i = 0; i < OBJ_NSELECTED; i++) 
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							fprintf(F, "					ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							fprintf(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							fprintf(F, "					ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}

					fprintf(F, "					EXEC;\n");

					fprintf(F, "				} else {\n");

					fprintf(F, "					// ow, decrease the count column \n");
					fprintf(F, "					DEFQ(\"update %s set \");\n", mvName);

					check = FALSE;
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {
					
							if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_SUM)) {
								if (check) fprintf(F, "					ADDQ(\", \");\n");
								check = TRUE;

								fprintf(F, "					ADDQ(\" %s = %s - \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							} else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_COUNT)) {
								if (check) fprintf(F, "					ADDQ(\", \");\n");
									check = TRUE;

								fprintf(F, "					ADDQ(\" %s = %s - \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							}
						}
					fprintf(F, "					ADDQ(\" where true \");\n");
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							fprintf(F, "					ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							fprintf(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							fprintf(F, "					ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}
					fprintf(F, "					EXEC;\n");

					fprintf(F, "				}\n");
					fprintf(F, "			}\n");

					// End of fcc
					fprintf(F, "		}\n");

					// Check time else: after: do insert new row
					fprintf(F, "\n	} else {\n");

					if (OBJ_STATIC_BOOL_AGG_MINMAX) {
						// DELETE PART AFTER OF UPDATE
						fprintf(F, "\n		/* DETELE */\n		// Get values of table's fields\n");
						for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
							char *func;
							char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
							char *convert = "";
							if (STR_EQUAL(ctype, "char *"))
								func = "GET_STR_ON_TRIGGERED_ROW";
							else if (STR_EQUAL(ctype, "int")) {
								func = "GET_INT32_ON_TRIGGERED_ROW";
								convert = "INT32_TO_STR";
							} else if (STR_EQUAL(ctype, "int64")) {
								func = "GET_INT64_ON_TRIGGERED_ROW";
								convert = "INT64_TO_STR";
							} else {
								func = "GET_NUMERIC_ON_TRIGGERED_ROW";
								convert = "NUMERIC_TO_STR";
							}
							fprintf(F, "		%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
							if (STR_INEQUAL(ctype, "char *"))
								fprintf(F, "		%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
							FREE(ctype);
						}

						// FCC
						fprintf(F, "\n		// FCC\n");
						fprintf(F, "		if (%s) {\n", OBJ_FCC(j));

						// Re-query
						fprintf(F, "\n			// Re-query\n");
						fprintf(F, "			DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
						fprintf(F, "			ADDQ(\" from \");\n");
						fprintf(F, "			ADDQ(\"%s\");\n", OBJ_SQ->from);
						fprintf(F, "			ADDQ(\" where true \");\n");
						if (OBJ_SQ->hasWhere) fprintf(F, "			ADDQ(\" and %s\");\n", OBJ_SQ->where);
						for (i = 0; i < OBJ_TG_NCOLS(j); i++) {
								char *quote;
								char *strprefix;
								char *ctype = createCType(OBJ_TG_COL(j, i)->type);
								if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
								else { quote = ""; strprefix = "str_"; }
								fprintf(F, "			ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TG_COL(j, i)->name, quote);
								fprintf(F, "			ADDQ(%s%s);\n", strprefix, OBJ_TG_COL(j, i)->varName);
								fprintf(F, "			ADDQ(\"%s \");\n", quote);
								FREE(ctype);
						}
						if (OBJ_SQ->hasGroupby) fprintf(F, "			ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
						if (OBJ_SQ->hasHaving) fprintf(F, "				ADDQ(\" having %s\");\n", OBJ_SQ->having);

						fprintf(F, "			EXEC;\n");

						fprintf(F, "\n			DEFR(%d);\n", OBJ_NSELECTED);

						fprintf(F, "\n			FOR_EACH_RESULT_ROW(i) {\n");

						for (i = 0; i < OBJ_NSELECTED; i++) {
							fprintf(F, "				GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i+1);
							fprintf(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
						}

						fprintf(F, "			}\n");

						// For each dQi:
						fprintf(F, "\n			// For each dQi:\n			FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

						k = 0;
						for (i = 0; i < OBJ_NSELECTED; i++)
							fprintf(F, "				%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

						fprintf(F, "				DEFQ(\"update %s set mvcount = mvcount \");\n", mvName);

						for (i = 0; i < OBJ_NSELECTED; i++) {
							if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_MIN) || STR_EQUAL(OBJ_SELECTED_COL(i)->func, _AF_MAX)) {
								fprintf(F, "				ADDQ(\", %s = \");\n", OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							}
						}

						fprintf(F, "				ADDQ(\" where true \");\n");
						for (i = 0; i < OBJ_NSELECTED; i++)
							if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
								char *quote;
								char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
								if (STR_EQUAL(ctype, "char *")) quote = "\'";
								else quote = "";
								fprintf(F, "				ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
								fprintf(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "				ADDQ(\"%s \");\n", quote);
								FREE(ctype);
							}

						fprintf(F, "				EXEC;\n");
						fprintf(F, "			}\n");
						fprintf(F, "\n		}\n");
					}

					// INSERT PART AFTER OF UPDATE
					// Get values of table's fields
					fprintf(F, "\n		/* INSERT */\n		// Get values of table's fields\n");
					for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {
						char *func;
						char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
						char *convert = "";
						if (STR_EQUAL(ctype, "char *"))
							func = "GET_STR_ON_NEW_ROW";
						else if (STR_EQUAL(ctype, "int")) {
							func = "GET_INT32_ON_NEW_ROW";
							convert = "INT32_TO_STR";
						} else if (STR_EQUAL(ctype, "int64")) {
							func = "GET_INT64_ON_NEW_ROW";
							convert = "INT64_TO_STR";
						} else {
							func = "GET_NUMERIC_ON_NEW_ROW";
							convert = "NUMERIC_TO_STR";
						}
						fprintf(F, "		%s(%s, %d);\n", func, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->ordinalPosition);
						if (STR_INEQUAL(ctype, "char *"))
							fprintf(F, "		%s(%s, str_%s);\n", convert, OBJ_TABLE(j)->cols[i]->varName, OBJ_TABLE(j)->cols[i]->varName);
						FREE(ctype);
					}

					// FCC
					fprintf(F, "\n		// FCC\n");
					fprintf(F, "		if (%s) {\n", OBJ_FCC(j));

					/*
						TRIGGER BODY START HERE
					*/
					// Re-query
					fprintf(F, "\n			// Re-query\n");
					fprintf(F, "			DEFQ(\"%s\");\n", OBJ_SELECT_CLAUSE);
					fprintf(F, "			ADDQ(\" from \");\n");
					fprintf(F, "			ADDQ(\"%s\");\n", OBJ_SQ->from);
					fprintf(F, "			ADDQ(\" where true \");\n");
					if (OBJ_SQ->hasWhere) fprintf(F, "			ADDQ(\" and %s\");\n", OBJ_SQ->where);
					for (i = 0; i < OBJ_TABLE(j)->nCols; i++)
						if (OBJ_TABLE(j)->cols[i]->isPrimaryColumn) {
							char *quote;
							char *strprefix;
							char *ctype = createCType(OBJ_TABLE(j)->cols[i]->type);
							if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = ""; }
							else { quote = ""; strprefix = "str_"; }
							fprintf(F, "			ADDQ(\" and %s.%s = %s\");\n", OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[i]->name, quote);
							fprintf(F, "			ADDQ(%s%s);\n", strprefix, OBJ_TABLE(j)->cols[i]->varName);
							fprintf(F, "			ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}
					if (OBJ_SQ->hasGroupby) fprintf(F, "			ADDQ(\" group by %s\");\n", OBJ_SQ->groupby);
					if (OBJ_SQ->hasHaving) fprintf(F, "			ADDQ(\" having %s\");\n", OBJ_SQ->having);

					fprintf(F, "			EXEC;\n");

					// Allocate SAVED_RESULT set
					fprintf(F, "\n			// Allocate SAVED_RESULT set\n			DEFR(%d);\n", OBJ_NSELECTED);

					// Save dQ result
					fprintf(F, "\n			// Save dQ result\n			FOR_EACH_RESULT_ROW(i) {\n");

					for (i = 0; i < OBJ_NSELECTED; i++) {
						fprintf(F, "				GET_STR_ON_RESULT(%s, i, %d);\n", OBJ_SELECTED_COL(i)->varName, i+1);
						fprintf(F, "				ADDR(%s);\n", OBJ_SELECTED_COL(i)->varName);
					}

					fprintf(F, "			}\n");

					// For each dQi:
					fprintf(F, "\n			// For each dQi:\n			FOR_EACH_SAVED_RESULT_ROW(i, %d) {\n", OBJ_NSELECTED);

					k = 0;
					for (i = 0; i < OBJ_NSELECTED; i++)
						fprintf(F, "				%s = GET_SAVED_RESULT(i, %d);\n", OBJ_SELECTED_COL(i)->varName, k++);

					// Check if there is a similar row in MV
					fprintf(F, "\n				// Check if there is a similar row in MV\n");
					fprintf(F, "				DEFQ(\"select mvcount from %s where true \");\n", mvName);
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							fprintf(F, "				ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							fprintf(F, "				ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							fprintf(F, "				ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}

					fprintf(F, "				EXEC;\n");

					fprintf(F, "\n				if (NO_ROW) {\n");
					fprintf(F, "					// insert new row to mv\n");
					fprintf(F, "					DEFQ(\"insert into %s values (\");\n", mvName);

					for (i = 0; i < OBJ_NSELECTED; i++) {
						char *quote;
						char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
						if (STR_EQUAL(ctype, "char *"))
							quote = "'";
						else quote = "";
						fprintf(F, "					ADDQ(\"%s\");\n", quote);
						fprintf(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
						fprintf(F, "					ADDQ(\"%s\");\n", quote);
						if (i != OBJ_NSELECTED-1) fprintf(F, "					ADDQ(\", \");\n");
						FREE(ctype);
					}

					fprintf(F, "					ADDQ(\")\");\n");
					fprintf(F, "					EXEC;\n");

					fprintf(F, "				} else {\n");

					// Update agg func fields
					fprintf(F, "					// update old row\n");
					fprintf(F, "					DEFQ(\"update %s set \");\n", mvName);

					check = FALSE;
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_SUM)) {
								if (check) fprintf(F, "					ADDQ(\", \");\n");
									check = TRUE;

								fprintf(F, "					ADDQ(\" %s = %s + \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							} else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_COUNT)){
								if (check) fprintf(F, "					ADDQ(\", \");\n");
									check = TRUE;

								fprintf(F, "					ADDQ(\" %s = %s + \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							} else if (STR_EQUAL(OBJ_SELECTED_COL(i)->func, AF_MIN)) {
								if (check) fprintf(F, "					ADDQ(\", \");\n");
									check = TRUE;

								fprintf(F, "					ADDQ(\" %s = (case when %s > \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "					ADDQ(\" then \");\n");
								fprintf(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "					ADDQ(\" else %s end) \");\n", OBJ_SELECTED_COL(i)->varName);
							} else {
								// max
								if (check) fprintf(F, "					ADDQ(\", \");\n");
									check = TRUE;

								fprintf(F, "					ADDQ(\" %s = (case when %s < \");\n", OBJ_SELECTED_COL(i)->varName, OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "					ADDQ(\" then \");\n");
								fprintf(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
								fprintf(F, "					ADDQ(\" else %s end) \");\n", OBJ_SELECTED_COL(i)->varName);
							}
						}
					fprintf(F, "					ADDQ(\" where true \");\n");
					for (i = 0; i < OBJ_NSELECTED; i++)
						if (!OBJ_SELECTED_COL(i)->hasAggregateFunction) {
							char *quote;
							char *ctype = createCType(OBJ_SELECTED_COL(i)->type);
							if (STR_EQUAL(ctype, "char *")) quote = "\'";
							else quote = "";
							fprintf(F, "					ADDQ(\" and %s = %s\");\n", OBJ_SELECTED_COL(i)->name, quote);
							fprintf(F, "					ADDQ(%s);\n", OBJ_SELECTED_COL(i)->varName);
							fprintf(F, "					ADDQ(\"%s \");\n", quote);
							FREE(ctype);
						}
					fprintf(F, "					EXEC;\n");

					fprintf(F, "				}\n");
					fprintf(F, "			}\n");
					fprintf(F, "		}\n");

					// End of check time 
					fprintf(F, "\n	}");

					// End of end trigger
					fprintf(F, "\n\n	/*\n		Finish\n	*/\n	END;\n}\n\n");
				}// --- END OF CHECK POINT FOR UPDATE EVENT (A2 mechanism Vs. Normal mechanism)
				// --- END OF UPDATE EVENT

			} // --- END OF 'FOREACH TABLE'

			fclose(cWriter);
		} // --- END OF GENERATING C TRIGGER CODE
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

	PMVTG_END;
}