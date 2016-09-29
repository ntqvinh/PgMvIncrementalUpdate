// For standard input, output & memory allocating
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>

// For random function
#include <time.h>

// For file processing
#include <windows.h>

// For memory leak detector
///#include <vld.h>

// 
#include "MVIAUTG_Boolean.h"
#include "MVIAUTG_CString.h"
#include "MVIAUTG.h"

MAIN
{
	
	INPUT;

	CONNECT;
	if (IS_CONNECTED) {
		printf("> Database connected!\n");
	} else {
		printf("> Failed to connect! %s\n", ERR_MESSAGE);
		END;
	}

	// 1. Analyze master query, create: OBJ_SQ
	ANALYZE_MASTER_QUERY;
	if (OBJ_SQ) {
		printf(" > Analyze query successfully!\n");
	} else {
		printf(" > Failed to analyze master query!\n");
		END;
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

	// 3. ONE TIME MVIAUTG MANAGEMENT TABLES INITIALIZED ON DATABASE (MVIAUTG SCHEMA)
	// 3.0. Init schema: `mviautg`
	QUERY("create schema if not exists mviautg");
	EXECUTE;
	if (!QUERY_EXECUTED) {
		printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
	}

	// 3.1 Init table: `mviautg_tables` (table_id, table_name) if not exists
	QUERY("create table if not exists mviautg.tables (table_id bigserial, table_name character varying(100), ");
	_QUERY("constraint mviautg_tables_pk primary key (table_id), ");
	_QUERY("constraint mviautg_tables_table_name_unique unique (table_name));");
	EXECUTE;
	if (!QUERY_EXECUTED) {
		printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
	}

	// 3.2 Init table: `mviautg_mvs` (mv_id, mv_name) if not exists
	QUERY("create table if not exists mviautg.mvs (mv_id bigserial, mv_name character varying(100), ");
	_QUERY("constraint mviautg_mvs_pk primary key (mv_id), ");
	_QUERY("constraint mviautg_mvs_mv_name_unique unique (mv_name));");
	EXECUTE;
	if (!QUERY_EXECUTED) {
		printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
	}

	// 3.3 Init table: `mviautg_mv_trace` (mv_id, updated_time) if not exists
	QUERY("create table if not exists mviautg.mv_trace (mv_id bigserial, updated_time timestamp without time zone DEFAULT clock_timestamp(), ");
	_QUERY("constraint mviautg_mv_trace_pk primary key (mv_id), ");
	_QUERY("constraint mviautg_mv_trace_fk foreign key (mv_id) references mviautg.mvs (mv_id) match simple on update cascade on delete cascade);");
	EXECUTE;
	if (!QUERY_EXECUTED) {
		printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
	}

	/*/ 3.3 Create mviautg_mv_table (mv_id, table_id)
	QUERY("create table if not exists mviautg_mv_table (mv_id bigserial, table_id bigserial, ");
	_QUERY("constraint mviautg_mv_table_pk primary key (mv_id, table_id), ");
	_QUERY("constraint mviautg_mv_table_fk_mv foreign key (mv_id) references mviautg_mvs (mv_id) match simple on update cascade on delete cascade, ");
	_QUERY("constraint mviautg_mv_table_fk_table foreign key (table_id) references mviautg_tables (table_id) match simple on update cascade on delete cascade);");
	EXECUTE;
	if (!QUERY_EXECUTED) {
		printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
	}*/

	/*/ 3.3 Init table: `mviautg_update_trace` (trace_id, table_id, log_id) if not exists
	QUERY("create table if not exists mviautg.update_trace (trace_id bigserial, table_id bigserial, log_id bigserial, ");
	_QUERY("constraint mviautg_update_trace_pk primary key (trace_id), ");
	_QUERY("constraint mviautg_update_trace_fk foreign key (table_id) references mviautg.tables (table_id) match simple on update cascade on delete cascade);");
	EXECUTE;
	if (!QUERY_EXECUTED) {
		printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
	}*/

	// 4. SESSION INITIALIZATION
	// 4.1 Data for MVs table
	QUERY("insert into mviautg.mvs(mv_name) values ('");
	_QUERY(_MVIAUTG_MVName);
	_QUERY("');");
	EXECUTE;
	if (!QUERY_EXECUTED) {
		printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
	}

	// 4.2 Data for MVTrace table
	{
		char *mvId;
		QUERY("select mv_id from mviautg.mvs where mv_name = '");
		_QUERY(_MVIAUTG_MVName);
		_QUERY("'");
		EXECUTE;
		mvId = copy(DATA(0));

		QUERY("(");
		FOREACH_TABLE {
			_QUERY("select table_name, log_id, log_time from mviautg.");
			_QUERY(OBJ_TABLE(J)->name);
			_QUERY("_log, mviautg.tables where mviautg.tables.table_id = mviautg.");
			_QUERY(OBJ_TABLE(J)->name);
			_QUERY("_log.table_id");
			if (J < OBJ_SQ->fromElementsNum - 1) _QUERY(" union all ");
		}
		_QUERY(") order by log_time desc limit 1");
		EXECUTE;

		if (NROWS == 0) {
			QUERY("insert into mviautg.mv_trace(mv_id) select mv_id from mviautg.mvs where mv_name = '");
			_QUERY(_MVIAUTG_MVName);
			_QUERY("';");
			EXECUTE;
			if (!QUERY_EXECUTED) {
				printf("Error [%d] occurs from insert new mv_trace without any log: %s\n", STATUS, ERR_MESSAGE);
			}
		} else {
			char *minTime = copy(DATA(2));
			QUERY("insert into mviautg.mv_trace(mv_id, updated_time) values(");
			_QUERY(mvId);
			_QUERY(", '");
			_QUERY(minTime);
			_QUERY("')");
			EXECUTE;
			if (!QUERY_EXECUTED) {
				printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
			}
			FREE(minTime);
		}

		FREE(mvId);
	}
	

	// 4.3 Data for Tables table
	FOREACH_TABLE {
		QUERY("insert into mviautg.tables(table_name) select '");
		_QUERY(OBJ_TABLE(J)->name);
		_QUERY("' where not exists (select 1 from mviautg.tables where table_name = '");
		_QUERY(OBJ_TABLE(J)->name);
		_QUERY("');");
		EXECUTE;
		if (!QUERY_EXECUTED) {
			printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
		}

		/*/ 4.2.2 MV - Tables
		QUERY("insert into mviautg_mv_table(mv_id, table_id) select mv_id, table_id from mviautg_mvs, mviautg_tables where mv_name = '");
		_QUERY(_MVIAUTG_MVName);
		_QUERY("' and table_name = '");
		_QUERY(OBJ_TABLE(J)->name);
		_QUERY("';");
		EXECUTE;
		if (!QUERY_EXECUTED) {
			printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
		}*/
	}

	// 4.4. Create MV table
	QUERY("create table if not exists ");
	_QUERY(_MVIAUTG_MVName);
	_QUERY("(\n");
	for (K = 0; K < OBJ_NSELECTED; K++) {
		
		char *characterLength;
		char *ctype;
		char colDefine[ESTIMATED_LENGTH];

		STR_INIT(characterLength, "");
		ctype = createCType(OBJ_SELECTED_COL(K)->type);
		if (STR_EQUAL(ctype, "char *") && STR_INEQUAL(OBJ_SELECTED_COL(K)->characterLength, "")) {
			FREE(characterLength);
			STR_INIT(characterLength, "(");
			STR_APPEND(characterLength, OBJ_SELECTED_COL(K)->characterLength);
			STR_APPEND(characterLength, ")");
		}
		
		if (OBJ_SELECTED_COL(K)->context == COLUMN_CONTEXT_STAND_ALONE) {
			sprintf(colDefine, "	%s %s%s", OBJ_SELECTED_COL(K)->name, OBJ_SELECTED_COL(K)->type, characterLength);
		}
		else {
			sprintf(colDefine, "	%s %s%s", OBJ_SELECTED_COL(K)->varName, OBJ_SELECTED_COL(K)->type, characterLength);
		}
		if (K != OBJ_NSELECTED-1)
			strcat(colDefine, ",\n");
		FREE(characterLength);
		FREE(ctype);

		_QUERY(colDefine);
	}
	_QUERY("\n);");
	EXECUTE;
	if (!QUERY_EXECUTED) {
		printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
	}

	// 4.5. Create log tables
	FOREACH_TABLE {
		Boolean check = FALSE;
		SET(logTableName, string("mviautg."));
		SET(constraintName, string("mviautg_"));

		STR_APPEND(logTableName, OBJ_TABLE(J)->name);
		STR_APPEND(logTableName, "_log");

		STR_APPEND(constraintName, OBJ_TABLE(J)->name);
		STR_APPEND(constraintName, "_log");

		QUERY("create table if not exists ");
		_QUERY(logTableName);
		_QUERY(" (log_id bigserial, log_action int, log_time timestamp without time zone DEFAULT clock_timestamp(), table_id bigint");

		for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
			if (OBJ_TABLE(J)->cols[K]->isPrimaryColumn) {
				SET(sqlType, createSQLType(OBJ_TABLE(J)->cols[K]));
				_QUERY(", ");
				_QUERY("new_");
				_QUERY(OBJ_TABLE(J)->cols[K]->name);
				_QUERY(" ");
				_QUERY(sqlType);
				FREE(sqlType);
			}
		}

		for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
			SET(sqlType, createSQLType(OBJ_TABLE(J)->cols[K]));
			_QUERY(", ");
			_QUERY("old_");
			_QUERY(OBJ_TABLE(J)->cols[K]->name);
			_QUERY(" ");
			_QUERY(sqlType);
			FREE(sqlType);
		}

		_QUERY(", constraint ");
		_QUERY(constraintName);
		_QUERY("_pk primary key(log_id));");

		EXECUTE;
		if (!QUERY_EXECUTED) {
			printf("Error [%d] occurs. %s\n", STATUS, ERR_MESSAGE);
		}

		FREE(logTableName);
		FREE(constraintName);
	}

	// 6. ADDITIONAL FILES GENERATION
	// 6.1. <mvname>.<databasename>.mviautg info file
	{
		SET(INFFileName, string(_MVIAUTG_PostgreSQLPath));
		STR_APPEND(INFFileName, "\\mviautg\\");

		QUERY("md \"");
		_QUERY(INFFileName);
		_QUERY("\"");
		SYSTEM_EXECUTE;

		STR_APPEND(INFFileName, _MVIAUTG_MVName);
		STR_APPEND(INFFileName, ".");
		STR_APPEND(INFFileName, _MVIAUTG_Database);
		STR_APPEND(INFFileName, ".mviautg");
		
		if (INF = fopen(INFFileName, "w")) {

			fprintf(INF, "ntables = %d\n", OBJ_SQ->fromElementsNum);
			FOREACH_TABLE {
				fprintf(INF, "%s\n", OBJ_TABLE(J)->name);
			}

			fclose(INF);
		}

		FREE(INFFileName);
	}

	// 6.2. Trigger SQL code
	FOREACH_TABLE {	
		// Define trigger function
		char *triggerName;
		STR_INIT(triggerName, "mviautg_");
		STR_APPEND(triggerName, OBJ_TABLE(J)->name);
		STR_APPEND(triggerName, "_log");

		fprintf(SQL, "\n--Logging trigger for %s table\n", OBJ_TABLE(J)->name);
		fprintf(SQL, "create function %s() returns trigger as '%s' language c strict;\n", triggerName, _MVIAUTG_MVName);
		fprintf(SQL, "create trigger pgtg_%s after insert or update or delete on %s for each row execute procedure %s(); \n", triggerName, OBJ_TABLE(J)->name, triggerName);

		FREE(triggerName);
	}

	// 6.3. Trigger C source code
	fprintf(C, "#include \"pg_ctrigger.h\"\n\n");
	FOREACH_TABLE {
		Boolean check;
		// Define trigger function
		char *triggerName;
		char *logTableName;
		char *tableId;

		QUERY("select table_id from mviautg.tables where table_name = '");
		_QUERY(OBJ_TABLE(J)->name);
		_QUERY("'");
		EXECUTE;

		tableId = DATA(0);

		STR_INIT(triggerName, "mviautg_");
		STR_APPEND(triggerName, OBJ_TABLE(J)->name);
		STR_APPEND(triggerName, "_log");

		STR_INIT(logTableName, "mviautg.");
		STR_APPEND(logTableName, OBJ_TABLE(J)->name);
		STR_APPEND(logTableName, "_log");

		fprintf(C, "TRIGGER(%s) {\n", triggerName);

		// Old and new rows' data
		fprintf(C, "	// Old and new rows' data\n");
		for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
			char *varname = OBJ_TABLE(J)->cols[K]->varName;
			char *ctype = createCType(OBJ_TABLE(J)->cols[K]->type);
			fprintf(C, "	%s old_%s;\n", ctype, varname);
			if (STR_INEQUAL(ctype, "char *")) {
				if (STR_EQUAL(ctype, "Numeric"))
					fprintf(C, "	char * old_str_%s;\n", varname);
				else
					fprintf(C, "	char old_str_%s[20];\n", varname);
			}
			fprintf(C, "	%s new_%s;\n", ctype, varname);
			if (STR_INEQUAL(ctype, "char *")) {
				if (STR_EQUAL(ctype, "Numeric"))
					fprintf(C, "	char * new_str_%s;\n", varname);
				else
					fprintf(C, "	char new_str_%s[20];\n", varname);
			}

			FREE(ctype);
		}

		// Begin
		fprintf(C, "\n	BEGIN;\n");

		fprintf(C, "\n	if (TRIGGER_IS_FIRED_BY_INSERT) {\n\n");

		fprintf(C, "		// Get data of new rows' PK\n");
		for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
			if (OBJ_TABLE(J)->cols[K]->isPrimaryColumn) {
				char *func;
				char *convert = "";
				char *ctype = createCType(OBJ_TABLE(J)->cols[K]->type);
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
				fprintf(C, "		%s(new_%s, %d);\n", func, OBJ_TABLE(J)->cols[K]->varName, OBJ_TABLE(J)->cols[K]->ordinalPosition);
				if (STR_INEQUAL(ctype, "char *"))
					fprintf(C, "		%s(new_%s, new_str_%s);\n", convert, OBJ_TABLE(J)->cols[K]->varName, OBJ_TABLE(J)->cols[K]->varName);

				FREE(ctype);
			}
		}

		fprintf(C, "\n		QUERY(\"insert into \");\n");
		fprintf(C, "		_QUERY(\"%s \");\n", logTableName);
		fprintf(C, "		_QUERY(\"(log_action, table_id\");\n");
		for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
			if (OBJ_TABLE(J)->cols[K]->isPrimaryColumn) {
				fprintf(C, "		_QUERY(\", new_%s\");\n", OBJ_TABLE(J)->cols[K]->name);	
			}
		}
		fprintf(C, "		_QUERY(\") values (1, %s \");\n", tableId);
		for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
			if (OBJ_TABLE(J)->cols[K]->isPrimaryColumn) {
				char *quote;
				char *strprefix;
				char *ctype = createCType(OBJ_TABLE(J)->cols[K]->type);
				if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = "new_"; }
				else { quote = ""; strprefix = "new_str_"; }
				fprintf(C, "		_QUERY(\",%s\");\n", quote);
				fprintf(C, "		_QUERY(%s%s);\n", strprefix, OBJ_TABLE(J)->cols[K]->varName);
				fprintf(C, "		_QUERY(\"%s \");\n", quote);
				FREE(ctype);
			}
		}
		fprintf(C, "		_QUERY(\");\");\n");
		fprintf(C, "		EXECUTE;\n");

		fprintf(C, "\n	} else if (TRIGGER_IS_FIRED_BY_UPDATE) {\n\n");

		// Get data of old rows
		fprintf(C, "		// Get data of old rows\n");
		for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
			char *func;
			char *convert = "";
			char *ctype = createCType(OBJ_TABLE(J)->cols[K]->type);
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
			fprintf(C, "		%s(old_%s, %d);\n", func, OBJ_TABLE(J)->cols[K]->varName, OBJ_TABLE(J)->cols[K]->ordinalPosition);
			if (STR_INEQUAL(ctype, "char *"))
				fprintf(C, "		%s(old_%s, old_str_%s);\n", convert, OBJ_TABLE(J)->cols[K]->varName, OBJ_TABLE(J)->cols[K]->varName);

			FREE(ctype);
		}

		// Get data of new rows
		fprintf(C, "		// Get data of new rows' PK\n");
		for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
			if (OBJ_TABLE(J)->cols[K]->isPrimaryColumn) {
				char *func;
				char *ctype = createCType(OBJ_TABLE(J)->cols[K]->type);
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
				fprintf(C, "		%s(new_%s, %d);\n", func, OBJ_TABLE(J)->cols[K]->varName, OBJ_TABLE(J)->cols[K]->ordinalPosition);
				if (STR_INEQUAL(ctype, "char *"))
					fprintf(C, "		%s(new_%s, new_str_%s);\n", convert, OBJ_TABLE(J)->cols[K]->varName, OBJ_TABLE(J)->cols[K]->varName);
				FREE(ctype);
			}
		}

		fprintf(C, "\n		QUERY(\"insert into \");\n");
		fprintf(C, "		_QUERY(\"%s \");\n", logTableName);
		fprintf(C, "		_QUERY(\"(log_action, table_id\");\n");
		for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
			if (OBJ_TABLE(J)->cols[K]->isPrimaryColumn) {
				fprintf(C, "		_QUERY(\", new_%s\");\n", OBJ_TABLE(J)->cols[K]->name);	
			}
		}
		for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
				fprintf(C, "		_QUERY(\", old_%s\");\n", OBJ_TABLE(J)->cols[K]->name);	
		}
		fprintf(C, "		_QUERY(\") values (2, %s \");\n", tableId);
		for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
			if (OBJ_TABLE(J)->cols[K]->isPrimaryColumn) {
				char *quote;
				char *strprefix;
				char *ctype = createCType(OBJ_TABLE(J)->cols[K]->type);
				if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = "new_"; }
				else { quote = ""; strprefix = "new_str_"; }
				fprintf(C, "		_QUERY(\",%s\");\n", quote);
				fprintf(C, "		_QUERY(%s%s);\n", strprefix, OBJ_TABLE(J)->cols[K]->varName);
				fprintf(C, "		_QUERY(\"%s \");\n", quote);
				FREE(ctype);
			}
		}
		for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
				char *quote;
				char *strprefix;
				char *ctype = createCType(OBJ_TABLE(J)->cols[K]->type);
				if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = "old_"; }
				else { quote = ""; strprefix = "old_str_"; }
				fprintf(C, "		_QUERY(\",%s\");\n", quote);
				fprintf(C, "		_QUERY(%s%s);\n", strprefix, OBJ_TABLE(J)->cols[K]->varName);
				fprintf(C, "		_QUERY(\"%s \");\n", quote);
				FREE(ctype);
		}
		fprintf(C, "		_QUERY(\");\");\n");
		fprintf(C, "		EXECUTE;\n");

		// DELETE EVENT
		fprintf(C, "\n	} else {\n\n");

		fprintf(C, "		// Get data of old rows\n");
		for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
			char *func;
			char *convert = "";
			char *ctype = createCType(OBJ_TABLE(J)->cols[K]->type);
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
			fprintf(C, "		%s(old_%s, %d);\n", func, OBJ_TABLE(J)->cols[K]->varName, OBJ_TABLE(J)->cols[K]->ordinalPosition);
			if (STR_INEQUAL(ctype, "char *"))
				fprintf(C, "		%s(old_%s, old_str_%s);\n", convert, OBJ_TABLE(J)->cols[K]->varName, OBJ_TABLE(J)->cols[K]->varName);

			FREE(ctype);
		}

		fprintf(C, "\n		QUERY(\"insert into \");\n");
		fprintf(C, "		_QUERY(\"%s \");\n", logTableName);
		fprintf(C, "		_QUERY(\"(log_action, table_id\");\n");
		for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
			fprintf(C, "		_QUERY(\", old_%s\");\n", OBJ_TABLE(J)->cols[K]->name);	
		}
		fprintf(C, "		_QUERY(\") values (3, %s \");\n", tableId);
		for (K = 0; K < OBJ_TABLE(J)->nCols; K++) {
			char *quote;
			char *strprefix;
			char *ctype = createCType(OBJ_TABLE(J)->cols[K]->type);
			if (STR_EQUAL(ctype, "char *")) { quote = "\'"; strprefix = "old_"; }
			else { quote = ""; strprefix = "old_str_"; }
			fprintf(C, "		_QUERY(\",%s\");\n", quote);
			fprintf(C, "		_QUERY(%s%s);\n", strprefix, OBJ_TABLE(J)->cols[K]->varName);
			fprintf(C, "		_QUERY(\"%s \");\n", quote);
			FREE(ctype);
		}
		fprintf(C, "		_QUERY(\");\");\n");
		fprintf(C, "		EXECUTE;\n");

		fprintf(C, "\n	}\n");

		// End
		fprintf(C, "\n	END;\n");

		// Eng trigger
		fprintf(C, "}\n\n");

		FREE(triggerName);
		FREE(logTableName);
	}

	END;
	_getch();
}