#pragma once

#include "MVIAUTG_Table.h"
#include "MVIAUTG_Column.h"
#include "MVIAUTG_Config.h"

/*
	PMVTG Limitation Convention:
	1. Words, keywords are separated by at least 1 space character
	2. No alias
	3. Column name does not need to precede with table name 
	4. Not supported yet:
		- extract... from
		- select *
		- condition with parentheses
	5. Join + where condition: primary 'and' operator
*/

/*
	SUPPORTED SELECTING QUERY KEYWORDS - KW PREFIX
*/
#define KW_SELECT										"select "
#define KW_FROM											" from "
#define KW_JOIN											" join "
#define KW_INNERJOIN									" inner join "
#define KW_ON											" on "
#define KW_WHERE										" where "
#define KW_GROUPBY										" group by "
#define KW_HAVING										" having "

/*
	LOGIC KEYWORDS - KW PREFIX
*/
#define KW_AND											" and "
#define KW_OR											" or "
#define KW_NOT											"not "

/*
	AGGREGATE FUNCTIONS - AF PREFIX
*/
#define AF_SUM											"sum("
#define AF_COUNT										"count("
#define AF_AVG											"avg("
#define AF_MIN											"min("
#define _AF_MAX											"max("

/*
	TYPE					PMVTG_SELECTING_QUERY
	Keyword					SelectingQuery
	Value					'struct s_SelectingQuery' type pointer
	Change note (v4)		'Having' clause keeps be analyzed but not use anymore
*/
typedef struct s_SelectingQuery *SelectingQuery;
struct s_SelectingQuery {
	// Pointers for the positions of clauses' keyword
	char *selectPos, *fromPos, *wherePos, *groupbyPos, *havingPos;

	// Clauses' content (as 'malloc' allocated strings)
	char *select, *from, *where, *groupby, *having;

	// Columns list
	char *selectElements[MAX_N_ELEMENTS];
	int selectElementsNum;

	// Tables list
	char *fromElements[MAX_N_ELEMENTS];
	int fromElementsNum;

	// On conditions
	char *onConditions[MAX_N_TABLES][MAX_N_ELEMENTS];
	int onConditionsNum[MAX_N_TABLES];
	int onNum;
	
	// Where conditions
	char *whereConditions[MAX_N_ELEMENTS];
	int whereConditionsNum;

	// Having condition
	char *havingConditions[MAX_N_ELEMENTS];
	int havingConditionsNum;

	// Group by elements
	char *groupbyElements[MAX_N_ELEMENTS];
	int groupbyElementsNum;

	// Determine if each clause appeared or not
	Boolean hasJoin, hasWhere, hasGroupby, hasHaving;
};

/* CONSTRUCTOR
	Input: string (variable string)
	Return: struct s_SelectingQuery type pointer
*/
SelectingQuery analyzeSelectingQuery(char *selectingQuery);

/* 
	Standardize query before analyzing
	- Trim the leading spaces
	- Check if the 'select' keyword is at the beginning of selecting query, if not, exit
	- Multiple spaces between 2 words are now merged
	! Internal using only (used by 'analyzeSelectingQuery').
*/
Boolean standardizeSelectingQuery(char *selectingQuery);

/* 
	Get the next keyword by the current keyword in the given selecting query
	! Internal using only (used by 'analyzeSelectingQuery').
*/
char *nextElement(SelectingQuery selectingQuery, char *currentElement);

/* 
	Transform sql logic expression to C style expression
	- Primary 'and'
	- C style operator (number comparing + string comparing)
	! Internal using only (used by 'analyzeSelectingQuery').
*/
void analyzeLogicExpression(char *logicExpression, char **resultSet, int *resultLen);

/*
	Tree memory allocated for SelectingQuery object
	Use in pair with 'analyzeSelectingQuery'
*/
void freeSelectingQuery(SelectingQuery *sq);