#include "PMVTG_Boolean.h"
#include <string.h>

#ifndef PMVTG_CSTRING
#define PMVTG_CSTRING 1

/*
	NOTE:
	constant string = iteral string
	variable string = char array = [static array | dynamic array]
	dynamic allocated string = dynamic char array
*/


/*
	STRING INIT SHORTHAND
*/
#define STR_INIT(source, initStr) source = string(initStr)

/*
	STRING APPEND SHORTHAND
*/
#define STR_APPEND(sourceStr, appendStr) sourceStr = dammf_append(sourceStr, appendStr)

/*
	STRING LENGTH
*/

#define RANDOM_STRING_LENGTH 10
#define LEN(a) (strlen(a))

/*
	COMPARING 2 STRINGS
	Input: strings - [variable | constant], case [sensitive | insensitive]
	Return: true/false
*/

#define STR_EQUAL(a, b) (a && b && strcmp(a, b) == 0)
#define STR_INEQUAL(a, b) (a && b && strcmp(a, b) != 0)
#define STR_EQUAL_CI(a, b) (a && b && _strcmpi(a, b) == 0)
#define STR_INEQUAL_CI(a, b) (a && b && _strcmpi(a, b) != 0)
#define STR_EMPTY(a) (STR_EQUAL(a, ""))

/*
	CASE MANIPULATING
*/

#define IS_UPPER(a) ((a) >= 65 && (a) <= 90)
#define IS_LOWER(a) ((a) >= 97 && (a) <= 122)
#define TO_UPPER(a) ((a) = (int) (a) - 32)
#define TO_LOWER(a) ((a) = (int) (a) + 32)
#define IS_LETTER(a) ((a) >= 65 && (a) <= 90 || (a) >= 97 && (a) <= 122)

/*
	FREE ALLOCATED MEMORY
*/
#define FREE(p) if (p) {free(p); p = NULL;}

/*
	TAKE A SUB STRING FROM A STRING
	Input: string - [variable | constant], 
		   start - [char pointer, inclusive], end - [char pointer, exclusive]
	Return: new string allocated by 'malloc'
*/
char *subString(char *str, char *start, char *end);

/*
	TRIM LEADING AND TRAILING WHITE SPACES
	Input: string - [variable | constant]
	Return: new string allocated by 'malloc'
*/
char *trim(char *str);

/*
	REMOVE ALL SPACES IN STRING
	Input: string - [variable | constant]
	Return: new string allocated by 'malloc'
*/
char *removeSpaces(char *str);

/*
	TAKE A SUB STRING FROM A POSITION TO THE REST
	Input: string - [variable | constant], start - [char pointer, inclusive]
	Return: new string allocated by 'malloc'
*/
char *takeStrFrom(char *str, char *start);

/*
	TAKE A SUB STRING FROM START TO A POSITION
	Input: string - [variable | constant], end - [char pointer, exclusive] !?
	Return: new string allocated by 'malloc'
*/
char *takeStrTo(char *str, char *end);

/*
	COPY A STRING
	Input: string - [variable | constant]
	Return: new string allocated by 'malloc'.
*/
char *copy(char *str);
// An alias of copy string, 'malloc' allocated
char *string(char *str);

/*
	TO LOWERCASE
	Input: string - [variable | constant]
	Return: new string allocated by 'malloc'.
*/
char *toLower(char *str);

/*
	MAKE LOWERCASE
	Input: string - [variable]
	Return: self
*/
void makeLower(char *str);

/*
	TO UPPERCASE
	Input: string - [variable | constant]
	Return: new string allocated by 'malloc'
*/
char *toUpper(char *str);

/*
	DELETE SUB STRING
	Input: string - [variable | constant], start - [char pointer, inclusive], length
	Return: new string allocated by 'malloc'
*/
char *deleteSubString(char *str, char *start, unsigned int length);

/*
	INSERT SUB STRING
	Input: string - [variable | constant], insert position - [char pointer, inclusive], sub string - [variable | constant].
	Return: new string allocated by 'malloc'
*/
char *insertSubString(char *str, char *insPos, char *subStr);

/*
	REPLACE FIRST SUB STRING / FIND AND REPLACE (FIRST OCCURRED)
	Input: string, string to find, string to replace - [variable | constant]
	Return: new string allocated by 'malloc'
*/
char *replaceFirst(char *str, char *oldString, char *newString);

/*
	APPEND A STRING TO A STRING
	Input: 2 strings - [variable | constant]
	Return: new string allocated by 'malloc' = source + str
*/
char *append(char *source, char *str);

/*
	SPLIT THE STRING SEPARATED BY THE GIVEN DELIMITERS
	Input: spliting string, delimiters - [variable | constant], 
		   resultSet - char pointer array [static | dynamic], 
		   resultLen - int [ref], 
		   isCharDelim - [TRUE: each char in delimiter string is a delimiter, case-sensitive 
						| FALSE: whole string is delimiter, case-insensitive]
	Return: ref param > resultSet, resultLen
	! The spliting string 'str' is kept original
	! Elements of 'resultSet' are 'malloc' allocated
	! Empty elements are removed
*/
void split(char *str, char *delim, char **resultSet, int *resultLen, Boolean isCharDelim);

/*
	CREATE A RANDOM STRING WITH SPECIFIC LENGTH
	Input: Length of the random string.
	Return: new string allocated by 'malloc'.
	! To use this function, the container function must include the <time.h> library and call 'srand(time(NULL))' first.
	! If length = -1, the length will be set default by the RANDOM_STRING_LENGTH macro.
*/
char* randomString(size_t length);

/*
	FIND SUB STRING
	Input: 2 strings [variable | constant]
	Return: a char type pointer to the 'subStr''s starting position in 'str'
	! Case insensitive 
	! Similar function: strstr (in string.h) is case sensitive
*/
char *findSubString(char *str, char *subStr);

/*
	COMPARE 2 CHARACTER, CASE INSENSITIVE
*/
Boolean charEqualCI(char a, char b);

/* -----------------------------------------------------------------------
	DAMMFs - dynamic allocated memory managed functions
	Functions that support built-in memory managed while processing.
*/ //---------------------------------------------------------------------

/*
	TRIM (DAMMF)
	Input: string - [dynamic allocated]
	Return: new string allocated by 'malloc'
	! Input parameter 'str' is freed after function execution
*/
char *dammf_trim(char *str);

/*
	SPLIT (DAMMF)
	Input: spliting string - [dynamic allocated], delimiters - [variable | constant], 
		   resultSet - char pointer array [static | dynamic], 
		   resultLen - int [ref], 
		   isCharDelim - [TRUE: each char in delimiter string is a delimiter, case-sensitive 
						| FALSE: whole string is delimiter, case-insensitive]
	Return: ref param > resultSet, resultLen
	! Input parameter 'str' is freed after function execution
	! Elements of 'resultSet' are 'malloc' allocated
	! Empty elements are removed
*/
void *dammf_split(char *str, char *delim, char **resultSet, int *resultLen, Boolean isCharDelim);

/*
	REPLACE FIRST(DAMMF)
	Input: string - [dynamic allocated], string to find, string to replace - [variable | constant]
	Return: new string allocated by 'malloc'
	! Input parameter 'str' is freed after function execution
*/
char *dammf_replaceFirst(char *str, char *oldString, char *newString);

/*
	INSERT SUB STRING (DAMMF)
	Input: string - [dynamic allocated], insert position - [char pointer, inclusive], sub string - [variable | constant].
	Return: new string allocated by 'malloc'
	! Input parameter 'str' is freed after function execution
*/
char *dammf_insertSubString(char *str, char *insPos, char *subStr);

/*
	APPEND (DAMMF)
	Input: source - [dynamic allocated], str - [variable | constant]
	! Input parameter 'source' is freed after function execution
*/
char *dammf_append(char *source, char *str);

/*
	REPLACE ALL (DAMMF)
	Input: string - [dynamic allocated], string to find, string to replace - [variable | constant]
	Return: string allocated by 'malloc', all 'oldString' occurrences are replaced by 'newString'
	! Input parameter 'str' is freed after function execution
*/
char *dammf_replaceAll(char *str, char *oldString, char* newString);
char *replace_str(char *str, char *orig, char *rep);
#endif