#include "mviau.h"
#include "MVIAUTG_CString.h"

struct s_SelectingQuery;
struct s_Table;
struct s_Column;

extern struct s_SelectingQuery		*_MVIAU_SelectingQuery;
extern struct s_Table				*_MVIAU_Tables							[MAX_N_TABLES];
extern struct s_Column				*_MVIAU_SelectedColumn					[MAX_N_COLS];
extern int							_MVIAU_NSelectedCol;
extern char							*_MVIAU_OutConditions						[MAX_N_TABLES][MAX_N_ELEMENTS];	
extern int							_MVIAU_OutConditionsElementNum			[MAX_N_TABLES];		
extern char							*_MVIAU_InConditions						[MAX_N_TABLES][MAX_N_ELEMENTS];	
extern int							_MVIAU_InConditionsElementNum				[MAX_N_TABLES];		
extern char							*_MVIAU_BinConditions						[MAX_N_TABLES][MAX_N_ELEMENTS];	
extern int							_MVIAU_BinConditionsElementNum			[MAX_N_TABLES];		

// CREATE VAR NAME, dynamically allocated string
char *createVarName(const Column column) {
	SET(rs, "");
	SET(prefix, createTypePrefix(column->type));
	if (column->context == COLUMN_CONTEXT_TABLE_WIRED || column->context == COLUMN_CONTEXT_STAND_ALONE) {
		rs = copy(prefix);
		STR_APPEND(rs, "_");
		STR_APPEND(rs, column->name);
		STR_APPEND(rs, "_");
		STR_APPEND(rs, column->table->name);

		if (column->context == COLUMN_CONTEXT_TABLE_WIRED) 
			STR_APPEND(rs, "_table");
	} else {
		SET(colName, randomString(-1));

		rs = copy(prefix);
		STR_APPEND(rs, "_");
		STR_APPEND(rs, colName);

		FREE(colName);
	}
	return rs;
}

// CREATE TYPE PREFIX, const string
char *createTypePrefix(const char *SQLTypeName) {
	char *ret;
	if (STR_EQUAL(SQLTypeName, "character varying") 
		|| STR_EQUAL(SQLTypeName, "character")
		|| strstr(SQLTypeName, "time")
		|| strstr(SQLTypeName, "date")) {

		ret = "str";

	} else if (STR_EQUAL(SQLTypeName, "integer")) {
		ret = "int";

	} else if (STR_EQUAL(SQLTypeName, "bigint")) {
		ret = "bigint";

	} else {
		ret = "num";
	}

	return ret;
}

// CREATE C TYPE, const string
char *createCType(const char *SQLTypeName) {
	char *ret;
	if (STR_EQUAL(SQLTypeName, "character varying") 
		|| STR_EQUAL(SQLTypeName, "character")
		|| strstr(SQLTypeName, "time")
		|| strstr(SQLTypeName, "date")) {

		ret = "char *";
	} else if (STR_EQUAL(SQLTypeName, "integer")) {
		ret = "int";

	} else if (STR_EQUAL(SQLTypeName, "bigint")) {
		ret = "int64";

	} else {
		ret = "Numeric";
	}

	return string(ret);
}

// CREATE SQL TYPE
char *createSQLType(const struct s_Column* column) {
	SET(sqlType, copy(column->type));
	SET(ctype, createCType(column->type));

	if (STR_EQUAL(ctype, "char *") && STR_INEQUAL(column->characterLength, "")) {
		STR_APPEND(sqlType, "(");
		STR_APPEND(sqlType, column->characterLength);
		STR_APPEND(sqlType, ")");
	}

	FREE(ctype);

	return sqlType;
}

// GET PRECEDED TABLE NAME
char* getPrecededTableName(char *wholeExpression, char *colPos) {
	int i;
	char *start;
	
	char operators[] = {'(', ')', ' ', '=', '>', '<', '+', '-', '*', '/'};
	int operatorsNum = 10;

	Boolean flag = TRUE;

	if (colPos != NULL && colPos != wholeExpression && *(colPos-1) == '.') {
		start = colPos-2;
		do {
			i = 0;
			while (i < operatorsNum && (*start != operators[i])) i++;

			// if at start pos is a normal letter
			if (i == operatorsNum)

				// check if start is at the beginning pos or not, if yes, get the substring
				if (start == wholeExpression)
					return subString(wholeExpression, start, colPos-1);
				else start--;
			
			// if start pos is one of the delimiters, break the loop
			else flag = FALSE;
		} while (flag);

		// get string at exclusive start pos
		return subString(wholeExpression, start+1, colPos-1);
	} else return NULL;

}

// ADD REAL COLUMN
Boolean addRealColumn(char *selectedName, char *originalColName) {
	int j, k;
	// Find dot position
	char *dotPos = strstr(selectedName, ".");
	char *columnName, *tableName;
	Boolean flag = FALSE;

	if (dotPos) {
		columnName = takeStrFrom(selectedName, dotPos+1);
		tableName = getPrecededTableName(selectedName, dotPos+1);
	} else { 
		columnName = copy(selectedName);
		tableName = copy("*");
	}

	// For each table
	for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {

		// For each column
		for (k = 0; k < OBJ_TABLE(j)->nCols; k++) {
			
			// Check for the selected name, determining the next step is necessary or not
			if (STR_EQUAL(columnName, OBJ_TABLE(j)->cols[k]->name)
				&& (STR_EQUAL(tableName, "*") || STR_EQUAL(tableName, OBJ_TABLE(j)->name))) {

					// standalone col
					if (originalColName == NULL) {
						
						OBJ_SELECTED_COL(OBJ_NSELECTED) = copyColumn(OBJ_TABLE(j)->cols[k]);

						OBJ_SELECTED_COL(OBJ_NSELECTED)->context = COLUMN_CONTEXT_STAND_ALONE;
						FREE(OBJ_SELECTED_COL(OBJ_NSELECTED)->originalName);
						FREE(OBJ_SELECTED_COL(OBJ_NSELECTED)->varName);
						OBJ_SELECTED_COL(OBJ_NSELECTED)->varName = createVarName(OBJ_SELECTED_COL(OBJ_NSELECTED));
					} else {
					// combined col
						OBJ_SELECTED_COL(OBJ_NSELECTED) = copyColumn(OBJ_TABLE(j)->cols[k]);
						FREE(OBJ_SELECTED_COL(OBJ_NSELECTED)->originalName);
						FREE(OBJ_SELECTED_COL(OBJ_NSELECTED)->varName);
						OBJ_SELECTED_COL(OBJ_NSELECTED)->originalName = removeSpaces(originalColName);
						OBJ_SELECTED_COL(OBJ_NSELECTED)->varName = createVarName(OBJ_SELECTED_COL(OBJ_NSELECTED));
						OBJ_SELECTED_COL(OBJ_NSELECTED)->context = COLUMN_CONTEXT_TABLE_UNWIRED;
					}
					OBJ_NSELECTED++;
					flag = TRUE;
					break;
			}
		}
		if (flag) break;
	}

	free(columnName);
	free(tableName);

	return flag;
}

// ADD FUNCTION (NORMAL & AGGREGATE FUNCTIONS)
char* addFunction(char *selectedName, char *remainder, int funcNum, char **funcList, Boolean isAggFunc) {
	int i;
	Boolean avgAppear = FALSE;
	char *funcPos, 
		 *start, *end, 
		 *tmp, *tmp_pre, *tmp_post, 
		 *remain;

	// This is the draft string for getting the aggregate functions, returned at the end.
	remain = copy(remainder);
	do {
		start = remain;
		i = 0;
		while ((funcPos = findSubString(start, funcList[i])) == NULL && i < funcNum-1) i++;

		if (funcPos) {

			/*
				Save the column marked with aggregate function 'TRUE' to the 'selected list'
			*/

			// Check parentheses number
			int check = 0;

			// Starting position of agg func's arg, the opening parenthese + 1
			char *p = funcPos + strlen(funcList[i]);

			// Allocate new column
			initColumn(&OBJ_SELECTED_COL(OBJ_NSELECTED));

			// Find the corresponding closing parenthese
			for (; *p; p++)
				if (*p == '(') check++;
				else if (*p == ')')
					if (check == 0) {
						OBJ_SELECTED_COL(OBJ_NSELECTED)->funcArg = subString(remain, funcPos + strlen(funcList[i]), p);
						break;
					} else check--;

			OBJ_SELECTED_COL(OBJ_NSELECTED)->context = COLUMN_CONTEXT_TABLE_UNWIRED;

			// Original name = selected name (virtualColumn)
			OBJ_SELECTED_COL(OBJ_NSELECTED)->originalName = removeSpaces(selectedName);

			// Function name
			if (STR_INEQUAL(funcList[i], AF_AVG))
				OBJ_SELECTED_COL(OBJ_NSELECTED)->func = copy(funcList[i]);
			else {
				avgAppear = TRUE;
				OBJ_SELECTED_COL(OBJ_NSELECTED)->func = string(AF_SUM);

				initColumn(&OBJ_SELECTED_COL(OBJ_NSELECTED+1));
				OBJ_SELECTED_COL(OBJ_NSELECTED)->context = COLUMN_CONTEXT_EXTRACTED;
				OBJ_SELECTED_COL(OBJ_NSELECTED+1)->hasAggregateFunction = TRUE;
				OBJ_SELECTED_COL(OBJ_NSELECTED+1)->func = string(AF_COUNT);
				OBJ_SELECTED_COL(OBJ_NSELECTED+1)->funcArg = copy(OBJ_SELECTED_COL(OBJ_NSELECTED)->funcArg);
				OBJ_SELECTED_COL(OBJ_NSELECTED+1)->originalName = removeSpaces(selectedName);
				OBJ_SELECTED_COL(OBJ_NSELECTED+1)->type = string("bigint");
				tmp = append(OBJ_SELECTED_COL(OBJ_NSELECTED+1)->func, OBJ_SELECTED_COL(OBJ_NSELECTED+1)->funcArg);
				OBJ_SELECTED_COL(OBJ_NSELECTED+1)->name = append(tmp, ")");
				free(tmp);
				OBJ_SELECTED_COL(OBJ_NSELECTED+1)->varName = createVarName(OBJ_SELECTED_COL(OBJ_NSELECTED+1));
			}
			// Name = "func(funcArg)"
			tmp = append(OBJ_SELECTED_COL(OBJ_NSELECTED)->func, OBJ_SELECTED_COL(OBJ_NSELECTED)->funcArg);
			OBJ_SELECTED_COL(OBJ_NSELECTED)->name = append(tmp, ")");
			free(tmp);

			if (isAggFunc) {
				OBJ_SELECTED_COL(OBJ_NSELECTED)->hasAggregateFunction = TRUE;

				if (STR_EQUAL(funcList[i], AF_SUM) 
					|| STR_EQUAL(funcList[i], _AF_MAX)
					|| STR_EQUAL(funcList[i], AF_MIN)
					|| STR_EQUAL(funcList[i], AF_AVG))

					OBJ_SELECTED_COL(OBJ_NSELECTED)->type = string("numeric");
				// COUNT
				else 
					OBJ_SELECTED_COL(OBJ_NSELECTED)->type = string("bigint");
			} else {
				OBJ_SELECTED_COL(OBJ_NSELECTED)->hasNormalFunction = TRUE;
				// type of normal functions

				if (STR_EQUAL(funcList[i], "abs("))
					OBJ_SELECTED_COL(OBJ_NSELECTED)->type = string("numeric");

			}

			OBJ_SELECTED_COL(OBJ_NSELECTED)->varName = createVarName(OBJ_SELECTED_COL(OBJ_NSELECTED));

			if (avgAppear)
				OBJ_NSELECTED += 2;
			else OBJ_NSELECTED++;

			// The string before aggFuncPos is saved
			tmp_pre = subString(remain, start, funcPos);

			// The position after the closing parenthese of aggregate function
			if (*(p+1) != '\0')
				// Not the end of string
				end = p + 1;
			else {
				// If it is the end of string, *end = ")"
				end = p;
				// break;
			}

			tmp_post = takeStrFrom(remain, end);

			tmp = remain;
			remain = append(tmp_pre, tmp_post);
			FREE(tmp);

			FREE(tmp_pre);
			FREE(tmp_post);
		}

	} while (funcPos);

	return remain;
}

// FILTER TRIGGER CONDITION
void filterTriggerCondition(char* _condition, int table) {
	int i, j;

	Boolean hasAgg = FALSE, 
		    inCol = FALSE, 
			outCol = FALSE;

	char *AF[] = {AF_SUM, AF_COUNT, AF_AVG, AF_MIN, _AF_MAX};
	int afNum = 5;

	char *expr;
	char *tmp1, *tmp2;
	char *condition = copy(_condition);
	expr = copy(condition);

	// 1. Search for any aggregate function
	for (i = 0; i < afNum; i++)
		if (findSubString(expr, AF[i])) { 
			  hasAgg = TRUE; 
			  break;
		}

	// If any aggregate function found, => OUT
	if (hasAgg) {
		FREE(expr);
		ADD_OBJ_OUT_CONDITION(table, condition);
		return;
	}

	// 2. Search for in-columns
	inCol = hasInCol(expr, table);

	// If no in-col found, => OUT
	if (!inCol) {
		FREE(expr);
		ADD_OBJ_OUT_CONDITION(table, condition);
		return;
	}

	// 3. Search for out-columns
	outCol = hasOutCol(expr, table);

	// If no out-col found, => IN
	if (!outCol) {
		FREE(expr);
		ADD_OBJ_IN_CONDITION(table, condition);
		return;
	}
	
	// 4. All the rest => OUT
	ADD_OBJ_BIN_CONDITION(table, condition);

	FREE(expr);
}

// HAS IN COL
Boolean hasInCol(char* condition, int table) {
	int i;
	char *tmp = copy(condition);
	char *start = tmp;
	char *colPos, *tableName;

	// for each cols
	for (i = 0; i < OBJ_TABLE(table)->nCols; i++) {

		// Find inner col
		do {
			colPos = findSubString(start, OBJ_TABLE(table)->cols[i]->name);
			
			// if found
			if (colPos) {

				// get table name
				tableName = getPrecededTableName(tmp, colPos);

				// No table name -> cols of inner table -> inner col found
				// else if there's a table name, and it equal the inner table name -> inner col found
				if (tableName == NULL || STR_EQUAL(tableName, OBJ_TABLE(table)->name)) {
					FREE(tmp);
					FREE(tableName);
					return TRUE;
				}
				

				// If there's a table name, but it does not equal to the inner table name 
				// -> continue searching the col in expression
				free(tableName);
				start = colPos + strlen(OBJ_TABLE(table)->cols[i]->name) - 1;
			}
		} while (colPos);
	}

	FREE(tmp);
	return FALSE;
}

// HAS OUT COL
Boolean hasOutCol(char *condition, int table) {
	int i, j;
	char *tmp = copy(condition);
	char *start = tmp;
	char *colPos, *tableName;

	// For each outter table
	for (j = 0; j < OBJ_SQ->fromElementsNum; j++)
		if (j != table)

			// For each col
			for (i = 0; i < OBJ_TABLE(j)->nCols; i++) {

				// Search for outter col
				do {
					colPos = findSubString(start, OBJ_TABLE(j)->cols[i]->name);

					// Found
					if (colPos) {

						// Get its table name
						tableName = getPrecededTableName(tmp, colPos);

						// If there's no table name -> column is belong to table by default -> outter col
						// If there's a table name & the name is inequal to the inner table name -> outter col
						if (tableName == NULL || STR_INEQUAL(tableName, OBJ_TABLE(table)->name)) {
							FREE(tmp);
							FREE(tableName);
							return TRUE;
						}

						// If there's a table name & the name is equal to the inner table name -> continue searching
						// that col name in the expression
						FREE(tableName);
						start = colPos + strlen(OBJ_TABLE(j)->cols[i]->name) - 1;
					}
				} while (colPos);
			}
	FREE(tmp);
	return FALSE;
}

void A2Split(int table, char* expression, int *nParts, int *partsType, char **parts) {
	int i, j;
	if (STR_EQUAL(expression, "*")) {
		(*nParts) = 1;
		partsType[0] = -1;
		parts[0] = string("*");
		return;
	}
	split(expression, "+-*/", parts, nParts, TRUE);
	for (i = 0; i < (*nParts); i++) {
		char *dotPos = strstr(parts[i], ".");
		if (dotPos) {
			char *tableName = getPrecededTableName(parts[i], dotPos+1);
			partsType[i] = -1;
			for (j = 0; j < OBJ_SQ->fromElementsNum; j++) {
				if (STR_EQUAL_CI(tableName, OBJ_TABLE(j)->name)) {
					partsType[i] = j;
					break;
				}
			}
			if (i > 0) {
				char *start = findSubString(expression, parts[i]) - 1;
				char *end = start + 1;
				char *opr = subString(expression, start, end);
				(*nParts)++;
				for (j = i; j < (*nParts) - 1; j++) {
					parts[j+1] = parts[j];
					partsType[j+1] = partsType[j];
				}
				parts[i] = opr;
				partsType[i] = -1;
				i++;
			}
			FREE(tableName);
		}
	}

	for (i = 0; i < (*nParts); i++) {
		if (partsType[i] == table) {
			char *dotPos = strstr(parts[i], ".");
			if (dotPos) {
				char *colName = takeStrFrom(parts[i], dotPos + 1);

				for (j = 0; j < OBJ_TABLE(partsType[i])->nCols; j++) {
					if (STR_EQUAL_CI(colName, OBJ_TABLE(partsType[i])->cols[j]->name)) {
						char tmp[ESTIMATED_LENGTH];
						INT32_TO_STR(OBJ_TABLE(partsType[i])->cols[j]->ordinalPosition, tmp);
						//STR_APPEND(tmp, OBJ_TABLE(partsType[i])->cols[j]->varName);
						FREE(parts[i]);
						parts[i] = copy(tmp);
					}
				}

				FREE(colName);
			}
		}
	}
}