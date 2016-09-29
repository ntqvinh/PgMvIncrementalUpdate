#include <stdlib.h>
#include "MVIAUTG_CString.h"
#include "MVIAUTG_Boolean.h"

// SUB STRING
char *subString(char *str, char *start, char *end) {
	int i;
	char *rs;

	if (str == NULL) return NULL;

	i = 0;
	rs = (char*) malloc (sizeof(char) * (end-start+1));

	// Move pointer to start pos
	for (; str < start; str++);

	// From start pos to end pos, copy to rs
	while (str < end){
		*(rs+i++) = *str++;
	}

	// Write string terminator
	*(rs+i) = '\0';

	return rs;
}

// TRIM
char *trim(char *str) {
	char *start = str, *end;

	// Trim leading spaces
	while(*start == ' ') start++;

	// ~ *start == '\0', end of the string and all spaces leading => return '\0'
	if(*start == 0)
		return start;

	// Trim trailing spaces
	end = start + LEN(start) - 1;
	while(end > start && *end == ' ') end--;

	return subString(str, start, end+1);
}

// REMOVE ALL SPACES
char *removeSpaces(char *str) {
	
	char *p;
	int spacesCount = 0;
	int rsLen = 0;
	char *rs = NULL;

	// First, remove leading & trailing spaces
	char *tmp = trim(str);

	// Get the number of remaining spaces
	for (p = tmp; *p; p++)
		if (*p == ' ') spacesCount++;

	// Allocate result string, len = trimed string len - number of remaining spaces
	rs = (char*) malloc (sizeof(char) * (LEN(tmp)-spacesCount) + 1);

	// Copy the non-space characters to result string
	for (p = tmp;  *p; p++)
		if (*p != ' ')
			rs[rsLen++] = *p;

	// Write string terminator
	rs[rsLen] = '\0';

	// Free the trimed string because it's allocated by 'malloc' in 'trim' function
	free(tmp);

	return rs;
}

// TAKE THE REST
char *takeStrFrom(char *str, char *start) {
	if (str) return subString(str, start, str+LEN(str));
	return NULL;
}

// TAKE STRING TO
char *takeStrTo(char *str, char *end) {
	if (str) return subString(str, str, end);
	return NULL;
}

// COPY
char *copy(char *str) {
	if (str) return takeStrFrom(str, str);
	return NULL;
}

// STRING
char *string(char *str) {
	return copy(str);
}

// TO LOWERCASE
char *toLower(char *str) {
	int i;

	// Copy str to rs
	char *rs = copy(str);
	int len = LEN(str);

	// Process rs
	for (i = 0; i < len; i++)
		if (IS_UPPER(rs[i])) TO_LOWER(rs[i]);

	return rs;
}

// MAKE LOWERCASE
void makeLower(char *str) {
	int i;
	for (i = 0; i < LEN(str); i++)
		if (IS_UPPER(str[i])) TO_LOWER(str[i]);
}


// TO UPPERCASE
char *toUpper(char *str) {
	int i;

	// Copy str to rs
	char *rs = copy(str);
	int len = LEN(str);

	// Process rs
	for (i = 0; i < len; i++)
		if (IS_LOWER(rs[i])) TO_UPPER(rs[i]);

	return rs;
}

// DELETE SUB STRING
char *deleteSubString(char *str, char *start, unsigned int length) {
	char *rs = (char*) malloc (sizeof(char) * (LEN(str) - length + 1));

	char *tmp1 = takeStrTo(str, start);
	char *tmp2 = takeStrFrom(str, start + length);

	strcpy(rs, tmp1);
	strcat(rs, tmp2);

	// tmp1 & tmp2 are allocated by 'malloc' so it's necessary to free them.
	free(tmp1);
	free(tmp2);

	return rs;
}

// INSERT SUB STRING
char *insertSubString(char *str, char *insPos, char *subStr) {
	char *rs = (char*) malloc (sizeof (char) * (LEN(str) + LEN(subStr) + 1));
	
	char *tmp1 = subString(str, str, insPos);
	char *tmp2 = takeStrFrom(str, insPos);

	strcpy(rs, tmp1);
	strcat(rs, subStr);
	strcat(rs, tmp2);

	// tmp1 & tmp2 are allocated by 'malloc' so it's necessary to free them.
	free(tmp1);
	free(tmp2);

	return rs;
}

// REPLACE FIRST SUB STRING
char *replaceFirst(char *str, char *oldString, char *newString) {
	char *rs = (char*) malloc (sizeof(char) * (LEN(str) - LEN(oldString) + LEN(newString) + 1) );

	// Find the start index of 'oldString'
	char *first = findSubString(str, oldString);

	char *tmp1 = takeStrTo(str, first);
	char *tmp2 = takeStrFrom(str, first + LEN(oldString));

	strcpy(rs, tmp1);
	strcat(rs, newString);
	strcat(rs, tmp2);

	// tmp1 & tmp2 are allocated by 'malloc' so it's necessary to free them.
	// 'first' is a char pointer (local variable), not a string.
	free(tmp1);
	free(tmp2);

	return rs;
}

// APPEND
char *append(char *source, char *str) {
	char *rs = (char*) malloc (sizeof(char) * (LEN(source) + LEN(str) + 1));
	strcpy(rs, source);
	strcat(rs, str);
	return rs;
}

// SPLIT
void split(char *str, char *delim, char **resultSet, int *resultLen, Boolean isCharDelim) {
	char *tmp = copy(str);

	*resultLen = 0;

	if (isCharDelim) {
		char *nextToken;
		char *rsTmp;
		rsTmp = strtok_s(tmp, delim, &nextToken);
		if (rsTmp)
			resultSet[(*resultLen)++] = copy(rsTmp);

		while (rsTmp) {
			rsTmp = strtok_s(NULL, delim, &nextToken);
			if (rsTmp)
				resultSet[(*resultLen)++] = copy(rsTmp);
		}
	} else {
		char *start = tmp;
		char *delimPos;
		char *emptyCheck;
		do {
			delimPos = findSubString(start, delim);
			if (delimPos) {
				emptyCheck = takeStrTo(start, delimPos);
				if (!STR_EMPTY(emptyCheck)) {
					resultSet[(*resultLen)++] = emptyCheck;
				} else {
					free(emptyCheck);
				}
				start = delimPos + LEN(delim);
			}
		} while (delimPos);

		emptyCheck = takeStrFrom(tmp, start);

		if (!STR_EMPTY(emptyCheck)) {
			resultSet[(*resultLen)++] = emptyCheck;
		} else {
			free(emptyCheck);
		}
	}

	free(tmp);
}

// RANDOM STRING
char* randomString(size_t length) {
    char charset[] = "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char *rs;
	char *dest;

	if (length == -1) length = RANDOM_STRING_LENGTH;

	rs = (char*) malloc (sizeof(char) * length + 1);
	dest = rs;
    while (length-- > 0) {
        int index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *dest = '\0';
	return rs;
}

// FIND SUB STRING
char *findSubString(char *str, char *subStr) {
	int len = LEN(str);
	int sublen = LEN(subStr);
	int i, j;
	char *rs = NULL;

	Boolean **compare = (Boolean**) malloc (sizeof(Boolean*) * len);
	for (i = 0; i < len; i++)
		compare[i] = (Boolean*) malloc (sizeof(Boolean) * sublen);

	for (i = 0; i < len; i++)
		for (j = 0; j < sublen; j++)
			if (charEqualCI(str[i], subStr[j]))
				compare[i][j] = TRUE;
			else compare[i][j] = FALSE;

	for (i = 0; i < len; i++)
		if (compare[i][0]) {
			int _i = i, _j = 0;
			while (_i < len && _j < sublen && compare[_i][_j])  {_i++; _j++;};
			if (_j == sublen) {
				rs = &str[i];
				break;
			}
		}

	for (i = 0; i < len; i++)
		free(compare[i]);
	free(compare);
	return rs;
}

// CHAR EQUAL CI
Boolean charEqualCI(char a, char b) {
	if (IS_LETTER(a) && IS_LETTER(b)) {
		if (a == b)	return TRUE;
		if ((IS_LOWER(a) && IS_LOWER(b)) || (IS_UPPER(a) && IS_UPPER(b)))
			return FALSE;
		if (IS_UPPER(a)) return charEqualCI(TO_LOWER(a), b);
		return charEqualCI(TO_UPPER(a), b);
	} else
		return (a == b);
}

// DAMMF - TRIM
char *dammf_trim(char *str) {
	char *rs = trim(str);
	free(str);
	return rs;
}

// DAMMF - SPLIT
void *dammf_split(char *str, char *delim, char **resultSet, int *resultLen, Boolean isCharDelim) {
	split(str, delim, resultSet, resultLen, isCharDelim);
	free(str);
}

// DAMMF - REPLACE FIRST SUB STRING
char *dammf_replaceFirst(char *str, char *oldString, char *newString) {
	char *rs = replaceFirst(str, oldString, newString);
	free(str);
	return rs;
}

// DAMMF - INSERT SUB STRING
char *dammf_insertSubString(char *str, char *insPos, char *subStr) {
	char * rs = insertSubString(str, insPos, subStr);
	free(str);
	return rs;
}

// DAMMF - APPEND
char *dammf_append(char *source, char *str) {
	char *rs = append(source, str);
	free(source);
	return rs;
}

// DAMMF - REPLACE ALL
char *dammf_replaceAll(char *str, char *oldString, char* newString) {
	char *pos, *newPos;
	char *tmp = copy(str);
	char *t;
	char *start = tmp;
	do {
		pos = findSubString(start, oldString);
		if (pos) {
			t = tmp;
			tmp = replaceFirst(start, oldString, newString);
			free(t);
			start = tmp;
			while (newPos = findSubString(start, newString)) start = newPos + LEN(newString);
		}
	} while (pos);
	free(str);
	return tmp;
}
