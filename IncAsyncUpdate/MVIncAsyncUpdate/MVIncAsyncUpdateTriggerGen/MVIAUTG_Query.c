#include <stdio.h>
#include <stdlib.h>
#include "MVIAUTG_CString.h"
#include "MVIAUTG_Query.h"

// ANALYZE SELECTING QUERY
SelectingQuery analyzeSelectingQuery(char *selectingQuery) {
	int i;
	char *rightLim, *tmp;
	SelectingQuery sq;

	if (!standardizeSelectingQuery(selectingQuery)) return NULL;

	sq = (SelectingQuery) malloc (sizeof(struct s_SelectingQuery));

	// Get clauses' position
	sq->selectPos = findSubString(selectingQuery, KW_SELECT);
	sq->fromPos = findSubString(selectingQuery, KW_FROM);
	sq->wherePos = findSubString(selectingQuery, KW_WHERE);
	sq->groupbyPos = findSubString(selectingQuery, KW_GROUPBY);
	sq->havingPos = findSubString(selectingQuery, KW_HAVING);

	// Check if some clauses appear in the query or not
	if (sq->wherePos)   sq->hasWhere = TRUE;   else sq->hasWhere = FALSE;
	if (sq->groupbyPos) sq->hasGroupby = TRUE; else sq->hasGroupby = FALSE;
	if (sq->havingPos)  sq->hasHaving = TRUE;  else sq->hasHaving = FALSE;
	
	// Init length variables
	sq->selectElementsNum = 0;
	sq->fromElementsNum = 0;
	sq->onNum = 0;
	for (i = 0; i < MAX_N_TABLES; i++)
		sq->onConditionsNum[i] = 0;
	sq->whereConditionsNum = 0;
	sq->groupbyElementsNum = 0;
	sq->havingConditionsNum = 0;

	/*
		PROCESS 'SELECT' CLAUSE
		Spit with commas as separator
	*/
	// Get select clause as string
	sq->select = subString(selectingQuery, sq->selectPos + LEN(KW_SELECT), sq->fromPos);

	// Copy q->select to process
	tmp = copy(sq->select);
		
	// Split elements
	dammf_split(tmp, ",", sq->selectElements, &(sq->selectElementsNum), TRUE);

	// Trim leading & trailing spaces of each element
	for (i = 0; i < sq->selectElementsNum; i++)
		sq->selectElements[i] = dammf_trim(sq->selectElements[i]);

	/*
		PROCESS 'FROM' CLAUSE
		- Split with commas as separator (not using 'join' keyword)
		- Split with 'join... on' keywords
	*/
	rightLim = nextElement(sq, KW_FROM);
	if (rightLim != NULL)
		sq->from = subString(selectingQuery, sq->fromPos + LEN(KW_FROM), rightLim);
	else
		sq->from = takeStrFrom(selectingQuery, sq->fromPos + LEN(KW_FROM));

	// Copy q->from to process
	tmp = copy(sq->from);

	// Check if 'from' clause contain 'join' keyword or not
	// If not, separate using comma
	if (findSubString(tmp, KW_JOIN) == NULL){
		// Save tables' name to obj->fromElements[]
		dammf_split(tmp, ",", sq->fromElements, &(sq->fromElementsNum), TRUE);

		// Trim leading & trailing white spaces in tables' name
		for (i = 0; i < sq->fromElementsNum; i++)
			sq->fromElements[i] = dammf_trim(sq->fromElements[i]);
	} else {
		// 'From' clause contain ' join ' or ' inner join '
		int i, j;
		char *joinKeywords[] = {KW_INNERJOIN, KW_JOIN};
		int joinKeywordsLen[] = {LEN(KW_INNERJOIN), LEN(KW_JOIN)};
		int joinKeywordsNum = 2;

		// Clauses contain table with 'on' conditions
		char *jclause[MAX_N_ELEMENTS];
		int jclauseLen = 0;

		char *start, *end;

		char *joinPosSearch, *innerJoinPosSearch;
		char *joinPosArr[MAX_N_ELEMENTS];
		char *innerJoinPosArr[MAX_N_ELEMENTS];
		int joinPosArrLen = 0, innerJoinPosArrLen = 0;

		start = tmp;

		// Split 'join' / 'inner join', using temporary array
		// temporary array contains ' on ' keyword
		/*while (TRUE) {
			i = 0;
			while ((end = findSubString(start, joinKeywords[i])) == NULL && i < joinKeywordsNum-1) i++;
			if (end){
				jclause[jclauseLen++] = subString(tmp, start, end);
				start = end + joinKeywordsLen[i];
			} else break;
		}
		jclause[jclauseLen++] = takeStrFrom(tmp, start);
		*/

		joinPosSearch = tmp;
		innerJoinPosSearch = tmp;

		// find join
		while (TRUE) {
			end = findSubString(joinPosSearch, KW_JOIN);
			if (end) {
				joinPosArr[joinPosArrLen++] = end;
				joinPosSearch = end + LEN(KW_JOIN);
			} else {
				break;
			}
		}

		// find inner join
		while (TRUE) {
			end = findSubString(innerJoinPosSearch, KW_INNERJOIN);
			if (end) {
				innerJoinPosArr[innerJoinPosArrLen++] = end;
				innerJoinPosSearch = end + LEN(KW_INNERJOIN);
			} else {
				break;
			}
		}

		// merge + split
		for (i = 0; i < joinPosArrLen; i++) {
			Boolean isInnerJoin = FALSE;
			for (j = 0; j < innerJoinPosArrLen; j++) {
				if ((innerJoinPosArr[j] + 6) == joinPosArr[i]) {
					isInnerJoin = TRUE;
				}
			}

			if (isInnerJoin) {
				if (end = findSubString(start, KW_INNERJOIN)) {
					jclause[jclauseLen++] = subString(tmp, start, end);
					start = end + LEN(KW_INNERJOIN);
				}
			} else {
				if (end = findSubString(start, KW_JOIN)) {
					jclause[jclauseLen++] = subString(tmp, start, end);
					start = end + LEN(KW_JOIN);
				}
			}

		}
		jclause[jclauseLen++] = takeStrFrom(tmp, start);

		free(tmp);

		// Save tables' name to obj->fromArgs[] after trim leading & trailing white spaces in tables' name
		for (i = 0; i < jclauseLen; i++) {
			end = findSubString(jclause[i], KW_ON);
			if (end) {
				tmp = takeStrTo(jclause[i], end);
				sq->fromElements[sq->fromElementsNum++] = dammf_trim(tmp);

				// Analyze logic expression
				tmp = takeStrFrom(jclause[i], end + LEN(KW_ON));
				analyzeLogicExpression(tmp, sq->onConditions[sq->onNum], &(sq->onConditionsNum[sq->onNum]));
				(sq->onNum)++;
				free(tmp);

				free(jclause[i]);
			} else {
				// join with no 'on'
				// tmp = jclause = table name
				sq->fromElements[sq->fromElementsNum++] = dammf_trim(jclause[i]);
			}
		}
	}
	
	/*
		PROCESS 'WHERE' CLAUSE
	*/
	// Get 'where' clause
	if (sq->hasWhere){
		rightLim = nextElement(sq, KW_WHERE);
		if (rightLim)
			sq->where = subString(selectingQuery, sq->wherePos + LEN(KW_WHERE), rightLim);
		else
			sq->where = takeStrFrom(selectingQuery, sq->wherePos + LEN(KW_WHERE));
		analyzeLogicExpression(sq->where, sq->whereConditions, &(sq->whereConditionsNum));
	} else {
		sq->where = NULL;
	}

	/*
		PROCESS GROUP BY CLAUSE
	*/
	// Get 'group by' clause as string
	if (sq->hasGroupby){
		rightLim = nextElement(sq, KW_GROUPBY);
		if (rightLim)
			sq->groupby = subString(selectingQuery, sq->groupbyPos + LEN(KW_GROUPBY), rightLim);
		else
			sq->groupby = takeStrFrom(selectingQuery, sq->groupbyPos + LEN(KW_GROUPBY));
		tmp = copy(sq->groupby);
		dammf_split(tmp, ",", sq->groupbyElements, &(sq->groupbyElementsNum), TRUE);

		// Trim leading & trailing spaces of each element
		for (i = 0; i < sq->groupbyElementsNum; i++)
			sq->groupbyElements[i] = dammf_trim(sq->groupbyElements[i]);
	} else {
		sq->groupby = NULL;
	}

	/*
		PROCESS HAVING CLAUSE
	*/
	if (sq->hasHaving){
		rightLim = nextElement(sq, KW_HAVING);
		if (rightLim)
			sq->having = subString(selectingQuery, sq->havingPos + LEN(KW_HAVING), rightLim);
		else
			sq->having = takeStrFrom(selectingQuery, sq->havingPos + LEN(KW_HAVING));
		analyzeLogicExpression(sq->having, sq->havingConditions, &(sq->havingConditionsNum));
	} else {
		sq->having = NULL;
	}

	return sq;
}

/* STANDARDIZE SELECTING QUERY
*/
Boolean standardizeSelectingQuery(char *selectingQuery) {

	// 0. Trim the leading spaces
	{
		int len = LEN(selectingQuery);
		int start = 0;
		int i;

		//trimUnicodeChar(selectingQuery);

		while (selectingQuery[start] && selectingQuery[start] == ' ') start++;
		for (i = start; i < len; i++)
			selectingQuery[i-start] = selectingQuery[i];
		selectingQuery[i-start] = 0;
	}

	// 1. Check if the 'select' keyword is at the beginning of selecting query, if not, exit
	{
		char *selectPos = findSubString(selectingQuery, KW_SELECT);		
		if (!selectPos) {
			return puts("! Standardization Error: 'Select' keyword not found"), FALSE;
		}
	}

	// 2. Multiple spaces between 2 words are now merged
	{
		int i, j;
		int len = LEN(selectingQuery);
		for (i = 0; i < len-1; i++)
			if (selectingQuery[i] == ' ' && selectingQuery[i+1] == ' ') {
				for (j = i; j < len-1; j++)
					selectingQuery[j] = selectingQuery[j+1];
				selectingQuery[j] = 0;
				i--;
			}
	}

	return TRUE;
}

// NEXT ELEMENT
char *nextElement(SelectingQuery selectingQuery, char *currentElement) {
	char *rs;

	if (selectingQuery->hasWhere && STR_EQUAL(currentElement, KW_FROM)) 
		rs = selectingQuery->wherePos;

	else if (selectingQuery->hasGroupby && 
		    (STR_EQUAL(currentElement, KW_FROM) || 
			 STR_EQUAL(currentElement, KW_WHERE)))
		rs = selectingQuery->groupbyPos;

	else if (selectingQuery->hasHaving && 
		    (STR_EQUAL(currentElement, KW_FROM) || 
			 STR_EQUAL(currentElement, KW_WHERE) || 
			 STR_EQUAL(currentElement, KW_GROUPBY)))
		rs = selectingQuery->havingPos;

	else rs = NULL;

	return rs;
}

// ANALYZE LOGIC EXPRESSION
void analyzeLogicExpression(char *logicExpression, char **resultSet, int *resultLen) {
	int i, j, k;
	char *tmp = trim(logicExpression);
	Boolean numCompare = TRUE;
	dammf_split(tmp, KW_AND, resultSet, resultLen, FALSE);

	// Process each condition
	for (i = 0; i < (*resultLen); i++) {
		char *or[MAX_N_EXPRESSIONS];
		int orLen = 0;

		resultSet[i] = dammf_replaceAll(resultSet[i], "<>", "!=");
		resultSet[i] = dammf_trim(resultSet[i]);
		
		// Get or array
		dammf_split(resultSet[i], KW_OR, or, &orLen, FALSE);
		
		resultSet[i] = copy("");

		for (j = 0; j < orLen; j++) {
			
			char *not;
			or[j] = dammf_trim(or[j]);
			not = findSubString(or[j], KW_NOT);
			numCompare = TRUE;

			// Check if 'not ' is a keyword
			if (not && (not == or[j] || (not != or[j] && !IS_LETTER(*(not-1))))) {
				char *k;

				// n -> !
				*not = '!';

				// o -> (
				*(not+1) = '(';

				for (k = not+2; *k; k++)
					if (*(k+1)) *k = *(k+1);

				*(k-1) = ')';
			}

			for (k = 0; k < LEN(or[j]); k++)
				if (or[j][k] == '\'') {
					numCompare = FALSE;
					or[j][k] = '\"';
				}

			if (!numCompare) {
				char *compare[MAX_N_ELEMENTS];
				int len;
				split(or[j], "!=", compare, &len, FALSE);

				if (len == 2) {	
					FREE(or[j]);
					or[j] = copy("");
					if (compare[0][0] != '!') or[j] = dammf_append(or[j], "STR_INEQUAL_CI(");
					else {
						or[j] = dammf_append(or[j], "!STR_INEQUAL_CI(");
						compare[0][0] = ' ';
					}
					or[j] = dammf_append(or[j], compare[0]);
					or[j] = dammf_append(or[j], ", ");
					or[j] = dammf_append(or[j], compare[1]);
					or[j] = dammf_append(or[j], ")");
				} else if (len == 1) {
					FREE(compare[0]);
					dammf_split(or[j], "=", compare, &len, FALSE);	
					or[j] = copy("");
					if (compare[0][0] != '!')
						or[j] = dammf_append(or[j], "STR_EQUAL_CI(");
					else {
						or[j] = dammf_append(or[j], "!STR_EQUAL_CI(");
						compare[0][0] = ' ';
					}
					or[j] = dammf_append(or[j], compare[0]);
					or[j] = dammf_append(or[j], ", ");
					or[j] = dammf_append(or[j], compare[1]);
					or[j] = dammf_append(or[j], ")");
				}

				for (k = 0; k < len; k++)
					FREE(compare[k]);
			} else {
				/*char *compare[2];
				int len;

				split(or[j], "!=", compare, &len, FALSE);
				if (len == 1)
					or[j] = dammf_replaceAll(or[j], "=", "==");
				free(compare[0]);
				if (len == 2) free(compare[1]);*/

				char *pos = strstr(or[j], "=");
				char *a = "!", *b = ">", *c = "<";
				if (pos && *(pos-1) != '!' && *(pos-1) != '>' && *(pos-1) != '<') {
					or[j] = dammf_replaceAll(or[j], "=", "==");
				}
			}

			resultSet[i] = dammf_append(resultSet[i], or[j]);

			if (j < orLen-1)
				resultSet[i] = dammf_append(resultSet[i], " || ");
		}

		for (k = 0; k < orLen; k++) {
			FREE(or[k]);
		}
	}
}

// FREE MEMORY
void freeSelectingQuery(SelectingQuery *sq) {
	if (sq) {
		int i, j;

		FREE((*sq)->select);
		FREE((*sq)->from);
		FREE((*sq)->where);
		FREE((*sq)->groupby);
		FREE((*sq)->having);

		for (i = 0; i < (*sq)->selectElementsNum; i++)
			FREE((*sq)->selectElements[i]);

		for (i = 0; i < (*sq)->fromElementsNum; i++)
			FREE((*sq)->fromElements[i]);

		for (i = 0; i < (*sq)->onNum; i++)
			for (j = 0; j < (*sq)->onConditionsNum[i]; j++)
				FREE((*sq)->onConditions[i][j]);

		for (i = 0; i < (*sq)->whereConditionsNum; i++)
			FREE((*sq)->whereConditions[i]);

		for (i = 0; i < (*sq)->groupbyElementsNum; i++)
			FREE((*sq)->groupbyElements[i]);

		for (i = 0; i < (*sq)->havingConditionsNum; i++)
			FREE((*sq)->havingConditions[i]);

		FREE((*sq));
	}
}