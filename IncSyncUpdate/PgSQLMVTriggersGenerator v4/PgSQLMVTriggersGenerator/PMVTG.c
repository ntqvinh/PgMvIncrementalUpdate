#include "PMVTG.h"
#include "PMVTG_Config.h"
#include "PMVTG_CString.h"
#include "PMVTG_Query.h"

struct s_SelectingQuery;
struct s_Table;
struct s_Column;

extern struct s_SelectingQuery		*PREPARED_STATEMENT_selectingQuery;
extern struct s_Table				*PREPARED_STATEMENT_table[MAX_NUMBER_OF_TABLES];
extern struct s_Column				*PREPARED_STATEMENT_selectedColumn[MAX_NUMBER_OF_COLS];
extern int							PREPARED_STATEMENT_nSelectedCol;
extern struct s_Column				*PREPARED_STATEMENT_primaryColumn[MAX_NUMBER_OF_COLS];
extern int							PREPARED_STATEMENT_nPkCol;
extern char							*PREPARED_STATEMENT_outConditions[MAX_NUMBER_OF_TABLES][MAX_NUMBER_OF_ELEMENTS];
extern int							PREPARED_STATEMENT_outConditionsElementNum[MAX_NUMBER_OF_TABLES];
extern char							*PREPARED_STATEMENT_inConditions[MAX_NUMBER_OF_TABLES][MAX_NUMBER_OF_ELEMENTS];
extern char							*PREPARED_STATEMENT_inConditionsOld[MAX_NUMBER_OF_TABLES][MAX_NUMBER_OF_ELEMENTS];
extern char							*PREPARED_STATEMENT_inConditionsNew[MAX_NUMBER_OF_TABLES][MAX_NUMBER_OF_ELEMENTS];
extern int							PREPARED_STATEMENT_inConditionsElementNum[MAX_NUMBER_OF_TABLES];
extern char							*PREPARED_STATEMENT_binConditions[MAX_NUMBER_OF_TABLES][MAX_NUMBER_OF_ELEMENTS];
extern int							PREPARED_STATEMENT_binConditionsElementNum[MAX_NUMBER_OF_TABLES];
extern char							*PREPARED_STATEMENT_firstCheckingCondition[MAX_NUMBER_OF_TABLES];
extern char							*PREPARED_STATEMENT_firstCheckingConditionOld[MAX_NUMBER_OF_TABLES];
extern char							*PREPARED_STATEMENT_firstCheckingConditionNew[MAX_NUMBER_OF_TABLES];


// TRUE EXIT
void stdExit(PGconn *PREPARED_STATEMENT_connection) {
    DISCONNECT;
	_getch();
    exit(1);
}

char * getColumnName(char *colPos) {
	char *kq;
	char *nextSpace = findSubString(colPos, " ");
	if (nextSpace) {
		kq = subString(colPos, colPos, nextSpace);
	}
	else {
		kq = colPos;
	}
	return kq;
}

// GET PRECEDED TABLE NAME
//Based on column position, get table name preceded
//Ex: 'khoa.ma_khoa', colPos = '.ma_khoa' -> output: 'khoa'
/*
	function getPrecededTableName nhận tham số đầu vào là chuỗi wholeExpression, dựa vào vị trí của dấu "." trong chuỗi để xác định tên bảng liền trước nó\
	'khoa.ma_khoa', colPos = '.ma_khoa' -> output: 'khoa'
	Sử dụng để tách tên bảng, tên cột
*/
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

			// if at start pos is a normal letter | nếu vị trí bắt đầu là ký tự thường
			if (i == operatorsNum)

				// check if start is at the beginning pos or not, if yes, get the substring | kiểm tra có bắt đầu lặp ở ký tự đầu tiên hay không
				//Nếu có nghĩa là từ ký tự đầu tiên tới colPos - 1 là tên bảng, lấy substring và return, ngược lại tiếp tục duyệt tìm ngược ra trước (start--)
				if (start == wholeExpression)
					return subString(wholeExpression, start, colPos-1);
				else start--;
			
			// if start pos is one of the delimiters, break the loop | gặp delimiters thì dừng
			else flag = FALSE;
		} while (flag);

		// get string at exclusive start pos
		return subString(wholeExpression, start+1, colPos-1);
	} else return NULL;

}

//Base on joining condition position, get the table name preceded
//ex: wholeExpression = t2 on t1.col = t2.col
// startDelim = on => output: t2
/*
	function getPrecededTableNameInJoiningCondition trả về tên bảng ngay trước mệnh đề ON
	wholeExpression = t2 on t1.col = t2.col
	startDelim = on => output: t2
*/
char *getPrecededTableNameInJoiningCondition(char* wholeExpression, char* startDelim) {
	char* kq = "";
	if (STR_EQUAL_CI(trim(startDelim), trim(KW_ON))) {
		kq = wholeExpression;
		while (TRUE) {
			kq = kq - 1;
			if (*(kq) == ' ') {
				kq = kq + 1;
				break;
			}				
		}
		kq = subString(kq, kq, wholeExpression);	
	}
	//starts with Outer join
	else {
		kq = wholeExpression;
		//Take keyword: LEFT, RIGHT or FULL. Default - 5
		kq = kq - 5;
		Boolean isRightOuterJoin = FALSE;

		if (STR_EQUAL_CI(findSubString(kq, "RIGHT OUTER JOIN"), kq)) {
			isRightOuterJoin = TRUE;
		}

		while (TRUE) {
			kq = kq - 1;
			if (*(kq) == ' ' && !isRightOuterJoin) {
				kq = kq + 1;
				break;
			}
			else if (*(kq) == ' ' && isRightOuterJoin) {
				isRightOuterJoin = FALSE;
			}
		}
		kq = subString(kq, kq, wholeExpression - 5);
	}

	return dammf_trim(kq);
}

//char *findJoiningCondition(char *expression) {
//	char *kq = "";
//	char *temp;
//	kq = findSubString(expression, KW_OUTER_JOIN);
//	if (kq) {
//		kq = kq - 5;
//		kq = subString(expression, expression, kq);
//	}
//	//if not find any OUTER JOIN keyword else, it mean expression contains the last joining condition
//	//This version doesn't care about maybe another condition with AND, OR, ...
//	else {
//		//check for where
//		temp = findSubString(expression, KW_WHERE);
//		if (temp) {
//			kq = subString(expression, expression, temp);
//		}
//		else {
//			temp = findSubString(expression, KW_GROUPBY);
//			if (temp) {
//				kq = subString(expression, expression, temp);
//			}
//			else {
//				kq = expression;
//			}
//			
//		}		
//	}
//	return kq;
//}
//
//char *findCurrentJoinType(char *currentTableName, char *expression) {
//	char *kq = "";
//	kq = expression - LEN(currentTableName) - LEN(KW_OUTER_JOIN);
//	kq = kq - 5;
//	return trim(subString(kq, kq, kq + 5));
//}

// CREATE VAR NAME *
/* function createVarName(const struct Column)
	generate C var name for generating code using it's column name and context
	Tạo ra tên biến C dùng trong việc sinh mã sử dụng tên cột và context, kiểu dữ liệu trong cột để tạo
*/
char *createVarName(const Column column) {
	char *rs = "";
	char *prefix = createTypePrefix(column->type);
	if (column->context != COLUMN_CONTEXT_EXPRESSION && column->context != COLUMN_CONTEXT_NOT_TABLE) {
		char *colName = column->name;
		char *tableName = column->table->name;
		char *randStr = randomString(-1);

		rs = copy(prefix);
		STR_APPEND(rs, "_");
		STR_APPEND(rs, colName);
		STR_APPEND(rs, "_");
		STR_APPEND(rs, tableName);

		if (column->context == COLUMN_CONTEXT_TABLE) 
			STR_APPEND(rs, "_table");
		
		FREE(randStr);

	} else {
		
		char *colName = randomString(-1);
		
		rs = copy(prefix);
		STR_APPEND(rs, "_");
		STR_APPEND(rs, colName);

		FREE(colName);
	}
	return rs;
}

// CREATE TYPE PREFIX *
/*
	T: input: SQLType, output: prefix  type
	Quy tắc: character varying, character, time, date -> str
	integer -> int
	bigint -> bigint
	others: num (decimal, bit, ...)

*/
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

// CREATE C TYPE *
/*
	Dựa vào SQLTypeName để xác định kiểu dữ liệu tương ứng trong C
*/
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

// ADD REAL COLUMN
/*
	T: function addRealColumn(selected name, original col name)
	original column name can be NULL
	method does: adding column's information to OBJ_SELECTED_COL
	return boolean: 
	"Real column": columns which are selected in the original query
	addRealColumn là thao tác tạo ra các đối tượng struct Column cho các cột trong mệnh đề SELECT trong truy vấn tạo KNT
	return: TRUE nếu cột được select hiện tại (selectedName) là cột có trong 1 bảng 
	Về: originalColName
		Nếu column là standalone thì ko có "tên gốc". Nếu column là combined thì cần tên gốc.
		Ví dụ: cho cột selected element [i] là sinh_vien.ma_sv*sinh_vien.tuoi
		-> combined
		-> colName = ma_sv, tableName = sinh_vien, originalColName = sinh_vien.ma_sv*sinh_vien.tuoi
		colName = tuoi, tableName = sinh_vien, originalColName = sinh_vien.ma_sv*sinh_vien.tuoi
*/
Boolean addRealColumn(char *selectedName, char *originalColName) {
	int j, k;
	// Find dot position
	char *dotPos = strstr(selectedName, ".");
	char *columnName, *tableName;
	Boolean flag = FALSE;

	//T: get column name and table name base on the dot position

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
			 //printf("Add real column: column name: %s - table: %s - column: %s \n", columnName, OBJ_TABLE(j)->name, OBJ_TABLE(j)->cols[k]->name);
			// check for the selected name, determining the next step is necessary or not
			if (STR_EQUAL(columnName, OBJ_TABLE(j)->cols[k]->name)
				&& (STR_EQUAL(tableName, "*") || STR_EQUAL(tableName, OBJ_TABLE(j)->name))) {
					// standalone col | cột chỉ đứng riêng lẻ
					if (originalColName == NULL) {
					/
						//vd: sinh_vien.ma_sv
						OBJ_SELECTED_COL(OBJ_NSELECTED) = copyColumn(OBJ_TABLE(j)->cols[k]);
						OBJ_SELECTED_COL(OBJ_NSELECTED)->context = COLUMN_CONTEXT_FREE;
						FREE(OBJ_SELECTED_COL(OBJ_NSELECTED)->originalName);
						FREE(OBJ_SELECTED_COL(OBJ_NSELECTED)->varName);
						OBJ_SELECTED_COL(OBJ_NSELECTED)->varName = createVarName(OBJ_SELECTED_COL(OBJ_NSELECTED));
					} else {
					// combined col | cột được gộp bằng biểu thức
						//vd: sinh_vien.ma_sv*sinh_vien.tuoi
						//printf("combined\n");
						OBJ_SELECTED_COL(OBJ_NSELECTED) = copyColumn(OBJ_TABLE(j)->cols[k]);
						FREE(OBJ_SELECTED_COL(OBJ_NSELECTED)->originalName);
						FREE(OBJ_SELECTED_COL(OBJ_NSELECTED)->varName);
						OBJ_SELECTED_COL(OBJ_NSELECTED)->originalName = removeSpaces(originalColName);
						OBJ_SELECTED_COL(OBJ_NSELECTED)->varName = createVarName(OBJ_SELECTED_COL(OBJ_NSELECTED));
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
/*
Add function là thao tác tạo ra các đối tượng struct Column cho các cột gộp nhóm, các cột là kết quả hàm thống kê trong mệnh đề SELECT trong truy vấn tạo KNT
*/
char* addFunction(char *selectedName, char *remainder, int funcNum, char **funcList, Boolean isAggFunc) {
	/*
		T: input mẫu:
		addFunction(OBJ_SQ->selectElements[i], tmp, 5, AF, TRUE)
		Trong đó:	OBJ_SQ->selectElements[i] = "count(sinh_vien.ma_sv)"
					tmp = "count(sinh_vien.ma_sv)"
					AF = { AF_SUM, AF_COUNT, AF_AVG, AF_MIN, _AF_MAX_ };
	*/
	int i;
	Boolean avgAppear = FALSE;
	char *funcPos, 
		 *start, *end, 
		 *tmp, *tmp_pre, *tmp_post, 
		 *remain;

	// This is the draft string for getting the aggregate functions, returned at the end. | tạo chuỗi nháp để return khi kết thúc hàm
	remain = copy(remainder);
	do {
		start = remain;
		i = 0;
		while ((funcPos = findSubString(start, funcList[i])) == NULL && i < funcNum-1) i++;

		if (funcPos) {

			// Check parentheses number
			int check = 0;

			// Starting position of agg func's arg, the opening parenthese + 1
			char *p = funcPos + strlen(funcList[i]);

			// Allocate new column
			initColumn(&OBJ_SELECTED_COL(OBJ_NSELECTED));

			// Find the corresponding closing parenthese
			for (; *p; p++) {
				if (*p == '(') {
					check++;
				}
				else if (*p == ')') {
					if (check == 0) {
						OBJ_SELECTED_COL(OBJ_NSELECTED)->funcArg = subString(remain, funcPos + strlen(funcList[i]), p);
						break;
					}
					else {
						check--; 
					}
				}
			}

			OBJ_SELECTED_COL(OBJ_NSELECTED)->context = COLUMN_CONTEXT_NOT_TABLE;

			// Original name = selected name (virtualColumn)
			OBJ_SELECTED_COL(OBJ_NSELECTED)->originalName = removeSpaces(selectedName);

			// Function name
			/*T: nếu ko phải là AVG thì tên hàm chính là funcList[i]
				Nếu là AVG thì cột hiện tại dùng để lưu SUM, sau đó tạo tiếp 1 cột khác để lưu COUNT
				Trên nguyên tắc phân tách AVG(E) thành SUM(E) và COUNT(E)
			*/
			if (STR_INEQUAL(funcList[i], AF_AVG))
				OBJ_SELECTED_COL(OBJ_NSELECTED)->func = copy(funcList[i]);
			else {
				avgAppear = TRUE;
				
				OBJ_SELECTED_COL(OBJ_NSELECTED)->func = string(AF_SUM);
				
				initColumn(&OBJ_SELECTED_COL(OBJ_NSELECTED+1));
				OBJ_SELECTED_COL(OBJ_NSELECTED)->context = COLUMN_CONTEXT_EXPRESSION;
				OBJ_SELECTED_COL(OBJ_NSELECTED+1)->hasAggregateFunction = TRUE;
				OBJ_SELECTED_COL(OBJ_NSELECTED+1)->func = string(AF_COUNT);
				OBJ_SELECTED_COL(OBJ_NSELECTED+1)->funcArg = copy(OBJ_SELECTED_COL(OBJ_NSELECTED)->funcArg);
				OBJ_SELECTED_COL(OBJ_NSELECTED+1)->originalName = removeSpaces(selectedName);
				OBJ_SELECTED_COL(OBJ_NSELECTED+1)->type = string("bigint");
				tmp = append(OBJ_SELECTED_COL(OBJ_NSELECTED+1)->func, OBJ_SELECTED_COL(OBJ_NSELECTED+1)->funcArg);
				OBJ_SELECTED_COL(OBJ_NSELECTED + 1)->name = append(tmp, ")");
				free(tmp);
				
				OBJ_SELECTED_COL(OBJ_NSELECTED+1)->context = COLUMN_CONTEXT_EXPRESSION;
				OBJ_SELECTED_COL(OBJ_NSELECTED+1)->varName = createVarName(OBJ_SELECTED_COL(OBJ_NSELECTED+1));
			}
			// Name = "func(funcArg)"
			tmp = append(OBJ_SELECTED_COL(OBJ_NSELECTED)->func, OBJ_SELECTED_COL(OBJ_NSELECTED)->funcArg);
			OBJ_SELECTED_COL(OBJ_NSELECTED)->name = append(tmp, ")");
			
			free(tmp);
			if (isAggFunc) {
				OBJ_SELECTED_COL(OBJ_NSELECTED)->hasAggregateFunction = TRUE;

				if (STR_EQUAL(funcList[i], AF_SUM) 
					|| STR_EQUAL(funcList[i], AF_MAX_)
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
				OBJ_NSELECTED += 2; //T: because we have to store SUM and COUNT instead of AVG | NSELECTED + 2 vì lúc này phải lưu SUM Và COUNT thay vì AVG
									//Đối với các hàm còn lại như MIN, MAX ... thì chỉ + 1
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
			//printf("tmp_pre=%s, tmp_post=%s, remain=%s\n", tmp_pre, tmp_post, remain);
			FREE(tmp_pre);
			FREE(tmp_post);
		}

	} while (funcPos);
	return remain;
}

// FILTER TRIGGER CONDITION
/*T: 
	function: filterTriggerCondition (condition, tableIndex):
	Hàm này giải thích rất dài, tham khảo tệp Note.docx

*/
void filterTriggerCondition(char* _condition, int table, Boolean isOuterJoinConditionFiltering) {

	int i, j;
	Boolean hasAgg = FALSE, 
		    inCol = FALSE, 
			outCol = FALSE;

	char *AF[] = {AF_SUM, AF_COUNT, AF_AVG, AF_MIN, AF_MAX_};
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
	//printf("%s \n", inCol ? "inCol" : "not inCol");
	// If no in-col found, => OUT
	if (!inCol) {
		FREE(expr);
		ADD_OBJ_OUT_CONDITION(table, condition);
		return;
	}

	// 3. Search for out-columns
	outCol = hasOutCol(expr, table);
	//printf("%s \n", outCol ? "outCol" : "not outCol");
	// If no out-col found, => IN
	if (!outCol) {
		FREE(expr);
		//ADD_OBJ_IN_CONDITION_OLD(table, a2FCCRefactor(condition, "_old"));
		//ADD_OBJ_IN_CONDITION_NEW(table, a2FCCRefactor(condition, "_new"));
		if (!isOuterJoinConditionFiltering) {
			ADD_OBJ_IN_CONDITION(table, condition);
		}			
		return;
	}
	
	// 4. All the rest => OUT
	//printf("All the rest \n");
	ADD_OBJ_BIN_CONDITION(table, condition);

	FREE(expr);
}

// HAS IN COL
/*
	T: function HasInCol (*condition, int table)
	return TRUE if obj_table(table) has a col in condition *condition
	ex: sinh_vien.ma_lop == khoa.ma_lop, table = khoa -> return TRUE because there is col 'ma_lop' of 'khoa' that is in col
*/
Boolean hasInCol(char* condition, int table) {
	int i;
	char *tmp = copy(condition);
	char *start = tmp;
	char *colPos, *tableName;
	//printf("In hasInCol: %s - %s \n", condition, OBJ_TABLE(table)->name);
	// for each cols
	for (i = 0; i < OBJ_TABLE(table)->nCols; i++) {

		// Find inner col
		do {
			colPos = findSubString(start, OBJ_TABLE(table)->cols[i]->name);
			
			// if found
			if (colPos) {
				
				// get table name
				tableName = getPrecededTableName(tmp, colPos);
		//		printf("found %s - %s \n", tableName, OBJ_TABLE(table)->name);
				// No table name -> cols of inner table -> inner col found
				// else if there's a table name, and it equal the inner table name -> inner col found
				if (tableName == NULL || STR_EQUAL(tableName, OBJ_TABLE(table)->name)) {
					//printf("found table name = %s, OBJ table name = %s, so return TRUE \n", tableName, OBJ_TABLE(table)->name);
					FREE(tmp);
					FREE(tableName);
					return TRUE;
				}
				
				// If there's a table name, but it does not equal to the inner table name 
				// -> continue searching the col in expression
				free(tableName);
				start = colPos + strlen(OBJ_TABLE(table)->cols[i]->name) - 1;
		//		printf("found table name = %s, OBJ table name = %s, so continue \n ", tableName, OBJ_TABLE(table)->name);
			}
		} while (colPos);
	}

	FREE(tmp);
	//printf(" return FALSE \n ");
	return FALSE;
}

// HAS OUT COL
/*
	T: function hasOutCol(char *condition, int table)
*/
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
							//printf("hasOutCol(%s, %s) = TRUE \n", condition, OBJ_TABLE(table)->name);
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
	//printf("hasOutCol(%s, %s) = FALSE \n", condition, OBJ_TABLE(table)->name);
	return FALSE;
}

void A2Split(int table, char* expression, int *nParts, int *partsType, char **parts) {
	int i, j;
	printf("A2Split for table %s: \n", OBJ_TABLE(table)->name);
	//expression là function argument (OBJ_SELECTED_COL(i)->funcArg)
	if (STR_EQUAL(expression, "*")) { 
		(*nParts) = 1;
		partsType[0] = -1;
		parts[0] = string("*");
		printf("-> returned \n");
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
		printf("-Output: expression = %s\n ", expression);
	}

	for (i = 0; i < (*nParts); i++) {
		if (partsType[i] == table) {
			char *dotPos = strstr(parts[i], ".");
			if (dotPos) {
				char *colName = takeStrFrom(parts[i], dotPos + 1);

				for (j = 0; j < OBJ_TABLE(partsType[i])->nCols; j++) {
					if (STR_EQUAL_CI(colName, OBJ_TABLE(partsType[i])->cols[j]->name)) {
						char *tmp;
						STR_INIT(tmp, "str_");
						STR_APPEND(tmp, OBJ_TABLE(partsType[i])->cols[j]->varName);
						FREE(parts[i]);
						parts[i] = copy(tmp);
						FREE(tmp);
					}
				}

				FREE(colName);
			}
		}
	}

	//printf("A2Split: table = %s, expression = %s, nParts = %d, partsType = %d \n", OBJ_TABLE(table)->name, expression, *nParts, *partsType);

}

char *a2FCCRefactor(char* originalFCC, char *prefix) {
	printf("original FCC = %s\n", originalFCC);
	int startingOffset = 0;
	char *findingPos = NULL;
	char *originalFCCCopied = copy(originalFCC);

	while (findingPos = strstr(originalFCCCopied + startingOffset, "_table")) {

		char *wordEndPoint = findingPos + LEN("_table");
		char *start = wordEndPoint - 1;
		char operators[] = {'(', ')', ' ', '=', '>', '<', '+', '-', '*', '/'};
		int operatorsNum = 10;
		int i = 0;
		char *tmp = takeStrTo(originalFCCCopied, findingPos);
		printf("	tmp = %s\n", tmp);
		Boolean flag = TRUE;
		do {
			i = 0;
			while (i < operatorsNum && (*start != operators[i])) i++;

			// if at start pos is a normal letter
			if (i == operatorsNum) {

				// check if start is at the beginning pos or not, if yes, get the substring
				if (start == originalFCCCopied) {
					char *varName = subString(originalFCCCopied, start, wordEndPoint);
					char *newVarName = string(prefix);

					STR_APPEND(newVarName, varName);
					originalFCCCopied = dammf_replaceFirst(originalFCCCopied, varName, newVarName);
					startingOffset += LEN(prefix);

					FREE(varName);
					FREE(newVarName);
					break;
				}
				else start--;
			} else flag = FALSE;
		} while (flag);

		if (flag == FALSE) {
			char *varName = subString(originalFCCCopied, start + 1, wordEndPoint);
			char *newVarName = string(prefix);
			STR_APPEND(newVarName, varName);
			originalFCCCopied = dammf_replaceFirst(originalFCCCopied, varName, newVarName);
			startingOffset += LEN(prefix);
			FREE(varName);
			FREE(newVarName);
		}
		startingOffset += LEN(tmp) + LEN("_table");
		FREE(tmp);
	}
	printf("returned = %s\n", originalFCCCopied);
	return originalFCCCopied;
}

char *ConditionCToSQL(char *conditionC) {
	char *temp = copy(conditionC);
	char *pos = NULL;
	pos = findSubString(temp, "==");
	if (pos) {
		temp = dammf_replaceFirst(conditionC, "==", "=");
		return temp;
	}	

	pos = findSubString(temp, "!=");
	if (pos) {
		temp = dammf_replaceFirst(conditionC, "!=", "<>");
		return temp;
	}

	pos = findSubString(temp, "STR_EQUAL_CI(");
	if (pos) {
		char *table_column = takeStrFrom(pos, pos + LEN("STR_EQUAL_CI("));
		char *comma = findSubString(table_column, ",");
		table_column = takeStrTo(table_column, comma);
		char *valueToCompare = takeStrFrom(comma, comma + 1);
		char *close = findSubString(valueToCompare, ")");
		valueToCompare = takeStrTo(valueToCompare, close);
		valueToCompare = ReplaceCharacter(valueToCompare, '"', "'");

		temp = copy(table_column);
		temp = dammf_append(temp, " = ");
		temp = dammf_append(temp, valueToCompare);
		
		free(table_column);
		free(valueToCompare);
		
		return temp;
	}

	pos = findSubString(temp, "STR_INEQUAL_CI(");
	if (pos) {

		char *table_column = takeStrFrom(pos, pos + LEN("STR_INEQUAL_CI("));
		char *comma = findSubString(table_column, ",");
		table_column = takeStrTo(table_column, comma);
		char *valueToCompare = takeStrFrom(comma, comma + 1);
		char *close = findSubString(valueToCompare, ")");
		valueToCompare = takeStrTo(valueToCompare, close);
		valueToCompare = ReplaceCharacter(valueToCompare, '"', "'");

		temp = copy(table_column);
		temp = dammf_append(temp, " <> ");
		temp = dammf_append(temp, valueToCompare);

		free(table_column);
		free(valueToCompare);

		return temp;
	}

	return temp;
}

char *ReplaceCharacter(const char *s, char ch, const char *repl) {
	int count = 0;
	const char *t;
	for (t = s; *t; t++)
		count += (*t == ch);

	size_t rlen = strlen(repl);
	char *res = malloc(strlen(s) + (rlen - 1)*count + 1);
	char *ptr = res;
	for (t = s; *t; t++) {
		if (*t == ch) {
			memcpy(ptr, repl, rlen);
			ptr += rlen;
		}
		else {
			*ptr++ = *t;
		}
	}
	*ptr = 0;
	return res;
}
