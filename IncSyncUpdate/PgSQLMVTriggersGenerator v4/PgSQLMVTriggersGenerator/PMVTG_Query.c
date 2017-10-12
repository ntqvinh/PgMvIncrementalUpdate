#include <stdio.h>
#include <stdlib.h>
#include "PMVTG_CString.h"
#include "PMVTG_Query.h"
#include "PMVTG_Group.h"

// ANALYZE SELECTING QUERY
/*
	Hàm Phân tích truy vấn gốc
	Các công việc thực hiện:
	Chuẩn hóa TVG (xóa khoảng trắng không cần thiết, kiểm tra tính hợp lệ của từ khóa SELECT...)
	Tạo cấu trúc struct của SelectingQuery lưu trữ các thông tin TVG
	Xác định vị trí các mệnh đề trong TVG (FROM, WHERE, JOIN,...)
	Phân tích từng mệnh đề một để bóc tách thông tin, lưu trữ vào cấu trúc struct
*/
SelectingQuery analyzeSelectingQuery(char *selectingQuery) {
	int i;
	char *rightLim, *tmp;
	SelectingQuery sq;

	if (!standardizeSelectingQuery(selectingQuery)) return NULL;

	sq = (SelectingQuery)malloc(sizeof(struct s_SelectingQuery));

	// Get clauses' position | Xác định vị trí các mệnh đề
	sq->selectPos = findSubString(selectingQuery, KW_SELECT);
	sq->fromPos = findSubString(selectingQuery, KW_FROM);
	sq->wherePos = findSubString(selectingQuery, KW_WHERE);
	sq->groupbyPos = findSubString(selectingQuery, KW_GROUPBY);
	sq->havingPos = findSubString(selectingQuery, KW_HAVING);
	sq->outerJoinPos = findSubString(selectingQuery, KW_OUTER_JOIN);
	sq->innerJoinPos = findSubString(selectingQuery, KW_INNERJOIN);

	// Check if some clauses appear in the query or not | Kiểm tra các mệnh đề này có xuất hiện hay không
	//Với Select và FROM là 2 mệnh đề bắt buộc phải có nên không cần kiểm tra
	if (sq->wherePos)   sq->hasWhere = TRUE;   else sq->hasWhere = FALSE;
	if (sq->groupbyPos) sq->hasGroupby = TRUE; else sq->hasGroupby = FALSE;
	if (sq->havingPos)  sq->hasHaving = TRUE;  else sq->hasHaving = FALSE;
	if (sq->outerJoinPos)  sq->hasOuterJoin = TRUE;  else sq->hasOuterJoin = FALSE;
	if (sq->innerJoinPos)  sq->hasInnerJoin = TRUE;  else sq->hasInnerJoin = FALSE;

	// Init length variables
	sq->selectElementsNum = 0;
	sq->fromElementsNum = 0;
	sq->onNum = 0;
	sq->onOuterJoinNum = 0;
	sq->outerJoinComplementTablesNum = 0;
	sq->outerJoinMainTablesNum = 0;
	sq->lastInnerJoinIndex = -1;
	sq->joiningTypesNums = 0;
	sq->joiningConditionsNums = 0;
	for (i = 0; i < MAX_NUMBER_OF_TABLES; i++) {
		sq->onConditionsNum[i] = 0;
		sq->onOuterJoinConditionsNum[i] = 0;
	}		
	sq->whereConditionsNum = 0;
	sq->groupbyElementsNum = 0;
	sq->havingConditionsNum = 0;

	/*
		PROCESS 'SELECT' CLAUSE
		Spit with commas as separator
	*/
	// Get select clause as string | Lấy mệnh đề select
	//T: ex: select a, b, c from d ==> sq->select = "a, b, c "
	sq->select = subString(selectingQuery, sq->selectPos + LEN(KW_SELECT), sq->fromPos);

	// Copy sq->select to process | copy ra riêng để xử lý, không làm ảnh hưởng đến mệnh đề cũ
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
	//Kiểm tra nếu FROM không chứa từ khóa JOIN tức là mệnh đề có dạng: 'FROM a,b,c,d' -> các phần tử của mệnh đề FROM phân tách bằng dấu ,
	if (findSubString(tmp, KW_JOIN) == NULL) {
		// Save tables' name to obj->fromElements[]
		dammf_split(tmp, ",", sq->fromElements, &(sq->fromElementsNum), TRUE);

		// Trim leading & trailing white spaces in tables' name
		for (i = 0; i < sq->fromElementsNum; i++)
			sq->fromElements[i] = dammf_trim(sq->fromElements[i]);
	}
	else {
	//Có JOIN trong FROM
		if (sq->hasOuterJoin) {
		/*
			Nếu xuất hiện outer join thì phân tích mệnh đề outer join, lưu trữ các thông tin như loại join (LEFT, RIGHT, FULL)
			điều kiện join giữa các bảng
		*/
			int i, j;
			char *start, *end;
			char *fromClause = copy(sq->from);
			//load first table
			char *tmp = findSubString(fromClause, " ");
			if (tmp) {
				sq->fromElements[sq->fromElementsNum++] = trim(subString(fromClause, fromClause, tmp));
			}
			while (tmp = findSubString(tmp, KW_ON)) {
				char *tableName = "";
				end = tmp;
				//avoid the first space from left
				tmp = tmp - 1;
				while (*(tmp) != ' ') {
					tmp = tmp - 1;
				}
				tableName = trim(subString(tmp, tmp, end));
				sq->fromElements[sq->fromElementsNum++] = tableName;
				tmp = tmp + LEN(tableName) + LEN(KW_ON);
			}
			//analyzed joining type
			free(tmp);
			tmp = fromClause;
			while (tmp = findSubString(tmp, KW_JOIN)) {
				char *joiningType = "";
				end = tmp;
				tmp = tmp - 6;
				joiningType = subString(tmp, tmp, end);
				//if outer, determine type of outer
				if (STR_EQUAL_CI(trim(joiningType), "OUTER")) {
					tmp = tmp - 5;
					joiningType = subString(tmp, tmp, end);
				}else{
					sq->lastInnerJoinIndex = sq->joiningTypesNums;
					sq->hasJoin = TRUE;
				}
				sq->joiningTypesElements[sq->joiningTypesNums++] = trim(joiningType);
				tmp = tmp + LEN(joiningType) + LEN(KW_JOIN);
			}
			//analyzed joining condition
			free(tmp);
			tmp = fromClause;
			while (tmp = findSubString(tmp, KW_ON)) {
				char *joiningCondition = "";
				end = findSubString(tmp, KW_INNERJOIN) || findSubString(tmp, KW_LEFT_OUTER_JOIN) 
					|| findSubString(tmp, KW_RIGHT_OUTER_JOIN) || findSubString(tmp, KW_FULL_OUTER_JOIN);
				//the last
				if (!end) {
					//tmp + 4 = tmp + ' ON ' 
					joiningCondition = takeStrFrom(tmp, tmp + 4);
				}
				else {
					end = findSubString(tmp, KW_INNERJOIN);
					if (!end) {
						end = findSubString(tmp, KW_LEFT_OUTER_JOIN);
						if (!end) {
							end = findSubString(tmp, KW_RIGHT_OUTER_JOIN);
							if (!end) {
								end = findSubString(tmp, KW_FULL_OUTER_JOIN);
							}
						}
					}
					joiningCondition = takeStrTo(tmp + 4, end);
				}
				sq->joiningConditionsElements[sq->joiningConditionsNums++] = joiningCondition;
				tmp = tmp + LEN(KW_ON) + LEN(joiningCondition);
			}
			free(fromClause);
		}
		//INNER JOIN ONLY
		else if (!sq->hasOuterJoin) {
			sq->hasJoin = TRUE;
			// 'From' clause contain ' join ' or ' inner join '
			int i, j;
			char *joinKeywords[] = { KW_INNERJOIN, KW_JOIN };
			int joinKeywordsLen[] = { LEN(KW_INNERJOIN), LEN(KW_JOIN) };
			int joinKeywordsNum = 2;

			// Clauses contain table with 'on' conditions
			char *jclause[MAX_NUMBER_OF_ELEMENTS];
			int jclauseLen = 0;

			char *start, *end;
			//T: tìm vị trí JOIN
			char *joinPosSearch, *innerJoinPosSearch;
			char *joinPosArr[MAX_NUMBER_OF_ELEMENTS];
			char *innerJoinPosArr[MAX_NUMBER_OF_ELEMENTS];
			int joinPosArrLen = 0, innerJoinPosArrLen = 0;

			start = tmp;
			
			joinPosSearch = tmp;
			innerJoinPosSearch = tmp;
			//T: Tức là kiếm join, kiếm inner join, sau đó merge. Nếu kiếm đc inner join thì đương nhiên cũng có join trong đó
			/*T: vd: a inner join b
					-> joinPos = join b
					->innerJoinPos = inner join b
					-> nhưng cả 2 giống nhau -> cần merge
			*/
			// find join
			while (TRUE) {
				end = findSubString(joinPosSearch, KW_JOIN);
				if (end) {
					joinPosArr[joinPosArrLen++] = end;
					joinPosSearch = end + LEN(KW_JOIN);
				}
				else {
					break;
				}
			}

			// find inner join
			while (TRUE) {
				end = findSubString(innerJoinPosSearch, KW_INNERJOIN);
				if (end) {
					innerJoinPosArr[innerJoinPosArrLen++] = end;
					innerJoinPosSearch = end + LEN(KW_INNERJOIN);
				}
				else {
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
				}
				else {
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
				printf("end  = %s\n", end);
				if (end) {

					tmp = takeStrTo(jclause[i], end); //T: tmp lúc này là tên bảng ngay trước on
					printf("tmp  = %s\n", tmp);
					sq->fromElements[sq->fromElementsNum++] = dammf_trim(tmp);

					// Analyze logic expression
					tmp = takeStrFrom(jclause[i], end + LEN(KW_ON));
					printf("tmp  = %s\n", tmp);
					//T: tmp lúc này là biểu thức logic 
					analyzeLogicExpression(tmp, sq->onConditions[sq->onNum], &(sq->onConditionsNum[sq->onNum]));
					(sq->onNum)++;
					free(tmp);

					free(jclause[i]);
				}
				else {
					// join with no 'on'
					// tmp = jclause = table name
					sq->fromElements[sq->fromElementsNum++] = dammf_trim(jclause[i]);
				}
			}
		}
	}


	/*
		PROCESS 'WHERE' CLAUSE
	*/
	// Get 'where' clause
	if (sq->hasWhere) {
		rightLim = nextElement(sq, KW_WHERE);
		if (rightLim)
			sq->where = subString(selectingQuery, sq->wherePos + LEN(KW_WHERE), rightLim);
		else
			sq->where = takeStrFrom(selectingQuery, sq->wherePos + LEN(KW_WHERE));
		analyzeLogicExpression(sq->where, sq->whereConditions, &(sq->whereConditionsNum));
	}
	else {
		sq->where = NULL;
	}

	/*
		PROCESS GROUP BY CLAUSE
	*/
	// Get 'group by' clause as string
	if (sq->hasGroupby) {
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
	}
	else {
		sq->groupby = NULL;
	}

	/*
		PROCESS HAVING CLAUSE
	*/
	if (sq->hasHaving) {
		rightLim = nextElement(sq, KW_HAVING);
		if (rightLim)
			sq->having = subString(selectingQuery, sq->havingPos + LEN(KW_HAVING), rightLim);
		else
			sq->having = takeStrFrom(selectingQuery, sq->havingPos + LEN(KW_HAVING));
		analyzeLogicExpression(sq->having, sq->havingConditions, &(sq->havingConditionsNum));
	}
	else {
		sq->having = NULL;
	}

	return sq;
}

//STANDARDIZE SELECTING QUERY
/*
	T: function standardizeSelectingQuery.
	1. Xóa dấu khoảng trắng ở đầu
	2. kiểm tra vị trí của select có hợp lệ ko
	3. Xóa khoảng trắng dư thừa
*/
Boolean standardizeSelectingQuery(char *selectingQuery) {
	// 0. Trim the leading spaces, like LTRIM in SQL
	{
		int len = LEN(selectingQuery);
		int start = 0;
		int i;

		//trimUnicodeChar(selectingQuery);

		while (selectingQuery[start] && selectingQuery[start] == ' ') start++;
		for (i = start; i < len; i++)
			selectingQuery[i - start] = selectingQuery[i];
		selectingQuery[i - start] = 0;
	}
	// 1. Check if the 'select' keyword is at the beginning of selecting query, if not, exit
	{
		char *selectPos = findSubString(selectingQuery, KW_SELECT);
		if (!selectPos || !(selectPos == selectingQuery))
			return puts("> standardizeSelectingQuery error: 'select' keyword is not in the right position."), FALSE;
	}

	// 2. Multiple spaces between 2 words are now merged
	{
		int i, j;
		int len = LEN(selectingQuery);
		for (i = 0; i < len - 1; i++)
			if (selectingQuery[i] == ' ' && selectingQuery[i + 1] == ' ') {
				for (j = i; j < len - 1; j++)
					selectingQuery[j] = selectingQuery[j + 1];
				selectingQuery[j] = 0;
				i--;
			}
	}
	//printf("result: %s\n", selectingQuery);
	return TRUE;
}

// NEXT ELEMENT
/*
	T:
	Công dụng: trả về chuỗi con, bắt đầu bằng phần tử tiếp theo của currentElement, trả về NULL nếu ko còn phần tử nào
	Thứ tự: SELECT, FROM, WHERE, GROUP BY, HAVING
	current = SELECT -> trả về đoạn từ FROM trở đi, ...
	vd: TVG:
	select khoa.ma_khoa, khoa.ten_khoa, lop_sh.ten_lop, count(sinh_vien.ma_sv)
	from khoa,lop_sh,sinh_vien
	where khoa.ma_khoa = lop_sh.ma_khoa and sinh_vien.ma_lop = lop_sh.ma_lop and sinh_vien.que_quan = 'Da Nang'
	group by khoa.ten_khoa, lop_sh.ten_lop, khoa.ma_khoa
	---------------
	curent element:  from
	-> return:  where khoa.ma_khoa = lop_sh.ma_khoa and sinh_vien.ma_lop = lop_sh.ma_lop and sinh_vien.que_quan = 'Da Nang' group by khoa.ten_khoa, lop_sh.ten_lop, khoa.ma_khoa
	curent element:  where
	-> return:  group by khoa.ten_khoa, lop_sh.ten_lop, khoa.ma_khoa
	curent element:  group by
	-> return: (null)
	---------------
	Tóm lại: X = {SELECT, FROM, WHERE, GROUP BY, HAVING}
	-> nếu currentElement là x[i] thì tức là muốn lấy đoạn bắt đầu từ x[i+1] trở đi
*/
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
	//printf("curent element: %s - return: %s\n", currentElement, rs);
	return rs;
}

// ANALYZE LOGIC EXPRESSION
/*
	T: function analyze logic expression (phân tích biểu thức logic)
	Hành động thực hiện:
	đầu tiên là split theo AND, trong mỗi AND split theo OR.
	Điều này là do logic expression được quy về dạng chuẩn tắc hội: ...AND...AND (...OR...OR...) AND...
	Cho mỗi điều kiện trong or, kiểm tra biểu thức so sánh là so sánh chuỗi hay so sánh số rồi chuyển từ dạng
	cú pháp PL/PgSQL về dạng cú pháp C:
	vd: quequan = 'Da Nang' -> STR_EQUAL_LI(quequan, "DaNang")
		tuoi <> 3			-> tuoi != 3
		tuoi = 10			-> tuoi == 10
*/
void analyzeLogicExpression(char *logicExpression, char **resultSet, int *resultLen) {
	int i, j, k;
	//printf("logic expression = %s\n", logicExpression);
	char *tmp = trim(logicExpression);
	Boolean numCompare = TRUE;

	dammf_split(tmp, KW_AND, resultSet, resultLen, FALSE);
	// Process each condition
	for (i = 0; i < (*resultLen); i++) {
		char * or [MAX_NUMBER_OF_EXPRESSIONS];
		int orLen = 0;

		resultSet[i] = dammf_replaceAll(resultSet[i], "<>", "!=");
		resultSet[i] = dammf_trim(resultSet[i]);

		// Get or array
		dammf_split(resultSet[i], KW_OR, or , &orLen, FALSE);

		resultSet[i] = copy("");

		for (j = 0; j < orLen; j++) {

			char *not;
			or [j] = dammf_trim(or [j]);
			not = findSubString(or [j], KW_NOT);
			numCompare = TRUE;

			// Check if 'not ' is a keyword
			/*T: chỗ này hơi phức tạp
				'not ' là keyword nếu tìm thấy 'not' trong or[j] và:
				1. not == or[j].
					ví dụ: OR NOT ma_sv = NULL
						-> or[j] = NOT ma_sv = NULL
						-> not = NOT ma_sv = NULL
						=> not == or[j]
				2. not != or[j] && trước not không phải là ký tự.
					ví dụ: OR A.Column NOT TRUE
						-> or[j] = A.Column NOT TRUE
						-> not = NOT TRUE
						=> not != or[j]
				not-1 = ' ' ko phải ký tự -> not là keyword
			*/
			/*T: Nếu NOT là keyword, thì chuyển về dạng !()
				ví dụ:
			*/
			if (not && (not == or [j] || (not != or [j] && !IS_LETTER(*(not- 1))))) {
				char *k;

				// n -> !
				*not = '!';
				// o -> (
				*(not+ 1) = '(';

				for (k = not+ 2; *k; k++)
					if (*(k + 1)) *k = *(k + 1);

				*(k - 1) = ')';

			}

			/*T: numCompare = true nếu or[j] đang so sánh số, = false nếu so sánh chuỗi. nếu tìm thấy dấu ' trong or[j] tức là so sánh chuỗi*/
			for (k = 0; k < LEN(or [j]); k++)
				if (or [j][k] == '\'') {
					numCompare = FALSE;
					or [j][k] = '\"';
				}

			//T: nếu đang so sánh chuỗi
			if (!numCompare) {
				char *compare[MAX_NUMBER_OF_ELEMENTS];
				int len;
				//printf("or[%d] = %s\n", j, or [j]);
				split(or [j], "!=", compare, &len, FALSE);
				//printf("len = %d\n", len);
				/*
					T: split theo "!=", nếu len = 2 tức là điều kiện so sánh chuỗi là != thiệt
					Nếu split theo "!=" mà len = 1 tức là ko có != ở trỏng, tức là điều kiện là "="

				*/
				if (len == 2) {
					//T: trường hợp !=
					FREE(or [j]);
					or [j] = copy("");
					if (compare[0][0] != '!') or [j] = dammf_append(or [j], "STR_INEQUAL_CI(");
					else {
						or [j] = dammf_append(or [j], "!STR_INEQUAL_CI(");
						compare[0][0] = ' ';
					}
					or [j] = dammf_append(or [j], compare[0]);
					or [j] = dammf_append(or [j], ", ");
					or [j] = dammf_append(or [j], compare[1]);
					or [j] = dammf_append(or [j], ")");
				}
				else if (len == 1) {
					// =
					FREE(compare[0]);
					dammf_split(or [j], "=", compare, &len, FALSE);
					or [j] = copy("");
					if (compare[0][0] != '!')
						or [j] = dammf_append(or [j], "STR_EQUAL_CI(");
					else {
						or [j] = dammf_append(or [j], "!STR_EQUAL_CI(");
						compare[0][0] = ' ';
					}
					or [j] = dammf_append(or [j], compare[0]);
					or [j] = dammf_append(or [j], ", ");
					or [j] = dammf_append(or [j], compare[1]);
					or [j] = dammf_append(or [j], ")");
				}
				//T: xóa bộ nhớ 
				for (k = 0; k < len; k++)
					FREE(compare[k]);
			}
			else {
				//T: else tức là đang so sánh số (compareNum = TRUE)
				/*char *compare[2];
				int len;
				split(or[j], "!=", compare, &len, FALSE);
				if (len == 1)
					or[j] = dammf_replaceAll(or[j], "=", "==");
				free(compare[0]);
				if (len == 2) free(compare[1]);*/

				char *pos = strstr(or [j], "=");
				//T: tự nhiên dòng này khai báo xong ko dùng
		//		char *a = "!", *b = ">", *c = "<";
				if (pos && *(pos - 1) != '!' && *(pos - 1) != '>' && *(pos - 1) != '<') {
					or [j] = dammf_replaceAll(or [j], "=", "==");
				}
			}

			resultSet[i] = dammf_append(resultSet[i], or [j]);

			if (j < orLen - 1)
				resultSet[i] = dammf_append(resultSet[i], " || ");
		}

		for (k = 0; k < orLen; k++) {
			FREE(or [k]);
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

		for (i = 0; i < (*sq)->onOuterJoinNum; i++)
			for (j = 0; j < (*sq)->onOuterJoinConditionsNum[i]; j++)
				FREE((*sq)->onOuterJoinConditions[i][j]);

		for (i = 0; i < (*sq)->whereConditionsNum; i++)
			FREE((*sq)->whereConditions[i]);

		for (i = 0; i < (*sq)->groupbyElementsNum; i++)
			FREE((*sq)->groupbyElements[i]);

		for (i = 0; i < (*sq)->havingConditionsNum; i++)
			FREE((*sq)->havingConditions[i]);

		FREE((*sq));
	}
}