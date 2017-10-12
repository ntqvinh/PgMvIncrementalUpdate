#pragma once

#include "PMVTG_Config.h"
#include "PMVTG_Table.h"
#include "PMVTG_Column.h"

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

/*version 5 - Support outer join clause*/
#define KW_OUTER_JOIN									" outer join "
#define KW_LEFT_OUTER_JOIN								" left outer join "
#define KW_RIGHT_OUTER_JOIN								" right outer join "
#define KW_FULL_OUTER_JOIN								" full outer join "
#define MAIN_TABLE										"MAIN"
#define COMPLEMENT_TABLE								"COMP"
#define POSITION_LEFT									"POSITION_LEFT"
#define POSITION_RIGHT									"POSITION_RIGHT"


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
#define AF_MAX_											"max("

/*
	TYPE					PMVTG_SELECTING_QUERY
	Keyword					SelectingQuery
	Value					'struct s_SelectingQuery' type pointer
	Change note (v4)		'Having' clause keeps be analyzed but not use anymore
*/
typedef struct s_SelectingQuery *SelectingQuery;
struct s_SelectingQuery {
	// Pointers for the positions of clauses' keyword
	char *selectPos, *fromPos, *wherePos, *groupbyPos, *havingPos, *outerJoinPos, *innerJoinPos;

	// Clauses' content (as 'malloc' allocated strings)
	char *select, *from, *where, *groupby, *having;

	// Columns list
	char *selectElements[MAX_NUMBER_OF_ELEMENTS];
	int selectElementsNum;

	// Tables list
	char *fromElements[MAX_NUMBER_OF_ELEMENTS];
	int fromElementsNum;

	// On conditions
	char *onConditions[MAX_NUMBER_OF_TABLES][MAX_NUMBER_OF_ELEMENTS];
	int onConditionsNum[MAX_NUMBER_OF_TABLES];
	int onNum;

	//On Outer join conditions
	char *onOuterJoinConditions[MAX_NUMBER_OF_TABLES][MAX_NUMBER_OF_ELEMENTS];
	int onOuterJoinConditionsNum[MAX_NUMBER_OF_TABLES];
	int onOuterJoinNum;

	char *outerJoinMainTables[MAX_NUMBER_OF_TABLES];
	int outerJoinMainTablesNum;
	char *outerJoinComplementTables[MAX_NUMBER_OF_TABLES];
	int outerJoinComplementTablesNum;

	// Where conditions
	char *whereConditions[MAX_NUMBER_OF_ELEMENTS];
	int whereConditionsNum;

	// Having condition
	char *havingConditions[MAX_NUMBER_OF_ELEMENTS];
	int havingConditionsNum;

	// Group by elements
	char *groupbyElements[MAX_NUMBER_OF_ELEMENTS];
	int groupbyElementsNum;

	// Determine if each clause appeared or not
	Boolean hasJoin, hasWhere, hasGroupby, hasHaving, hasOuterJoin, hasInnerJoin;
	
	//from the last version, not sure that currently needs
	char *outerJoinType;
	char *groupOuterJoinType;
	char *groupOuterJoinCondition;
	// :))
	//these 2 are for new inner-outer join combined
	char *groupJoinType;
	char *groupJoinCondition;
	
	char *joiningTypesElements[MAX_NUMBER_OF_ELEMENTS];
	int joiningTypesNums;
	char *joiningConditionsElements[MAX_NUMBER_OF_ELEMENTS];
	int joiningConditionsNums;
	int lastInnerJoinIndex; 
	
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

