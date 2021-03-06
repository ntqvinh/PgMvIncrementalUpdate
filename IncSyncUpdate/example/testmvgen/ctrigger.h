﻿/****************************************************************************************

	This file is about creating a new programming interface for user's trigger function,
	which uses macro definitions to create shortcuts for long complex C syntax code lines,
	for the purpose of coding speed and simple semantic understanding.

	Creators: 
		Algorithm, advisor: Nguyen Tran Quoc Vinh (ntquocvinh@{ued.vn, gmail.com})
		Programming: Tran Trong Nhan (trongnhan.tran93@gmail.com)
					 Do Minh Tuan (it.dominhtuan@gmail.com)

*****************************************************************************************/
#ifndef CTRIGGER
#define CTRIGGER 1

#include "postgres.h"
#include "executor/spi.h"
#include "commands/trigger.h"
#include "utils/rel.h"
#include "utils/date.h"
#include "utils/numeric.h"
#include "datatype/timestamp.h"
#include <string.h>

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

//------------------------------------------------------------------------------------------------------------------------------------------

/*
	MAXIMUM LENGTH OF A QUERY
	This length is considered to be modified.
*/
#define MAX_QUERY_LENGTH 2000

/*
	MACRO FOR CALLING THE 'SAVED RESULT SET'
	The SRS is used for temporary storing a result set (with a different format).
							USUAL RESULT SET					|					SAVED RESULT SET
	1.	Row index:			0									|					0
	2.	Col index:			1									|					0
	3.	Col index meaning:	The index of column in db table		|					The order of adding
*/
#define SAVED_RESULT savedResult
#define SAVED_RESULT_NESTED savedResultNested

/*
	MACRO FOR CALLING THE SIZE VARIABLE OF 'SAVED RESULT SET'
	This macro is intended for internal purpose.
*/
#define RESULT_COUNT resultCount
#define RESULT_COUNT_NESTED resultCountNested

/*
	THE NUMBER OF SAVED RESULT SET'S ROW
	This macro is intended for internal purpose.
*/
#define ROW_NUM numberOfRows
#define ROW_NUM_NESTED numberOfRowsInTheNestedLoop
#define COL_NUM_NESTED numberOfColsInTheNestedLoop
#define COL_NUM numberOfCols

/*
	CORE VARIABLES
	User's trigger function variables must not be named the same as these.
	This macro is intended for internal purpose.
*/
#define PREPARE_STATEMENTS TriggerData *trigdata = (TriggerData *) fcinfo->context;\
						   TupleDesc   tupdesc;                                    \
						   HeapTuple   rettuple;								   \
						   bool        isnull, checkNull = false;				   \
						   char		   query[MAX_QUERY_LENGTH];					   \
						   char**	   SAVED_RESULT = NULL;						   \
						   char**	   SAVED_RESULT_NESTED = NULL;				   \
						   int32	   RESULT_COUNT = 0, ret, i, j;				   \
						   int32	   RESULT_COUNT_NESTED = 0;					   \
						   int32	   ROW_NUM;									   \
						   int32	   ROW_NUM_NESTED;							   \
						   int32	   COL_NUM_NESTED;							   \
						   int32	   COL_NUM;									   \
						   int32	   a2Flag = 0;								   \
						   int32	   a2FlagCounter = 0;						   \
						   int32       a2Limiter;								   \
						   int32	   _PMVTGA2MechanismCounter;				   \
						   int32	   _PMVTGA2MechanismLimiter

/*
	TRIGGERED ROW'S DESCRIPTION
	For internal purpose only.
*/
#define GET_TRIGGERED_ROW_DESC tupdesc = trigdata->tg_relation->rd_att

/*
	PREPARE FOR TRIGGER FUNCTION RETURNING ROW
	This row is returned in UPDATE queries.
	For internal purpose only.
*/
#define PREPARE_RETURN_NEW_ROW rettuple = trigdata->tg_newtuple

/*
	PREPARE FOR TRIGGER FUNCTION RETURNING ROW
	For internal purpose only.
*/
#define PREPARE_RETURN_TRIGGERED_ROW rettuple = trigdata->tg_trigtuple

/*
	CONNECT TO SPI
	Check for connect error by using macro 'RET_ERR' define below.
	For internal purpose only.
*/
#define CONNECT ret = SPI_connect()

// Print/show an error message, then stop the process
#define ERR(msg) elog(ERROR, msg)

// Print a message in console log
#define INF(msg) elog(INFO, msg)

/*
	EXEC A QUERY
	This macro is used for EXEC a query defined by 'DEFQ' and 'ADDQ'.
	! Only one query's result set is saved at a time, 
	using the SRS if you want to store the result set and EXEC another query statement.
*/
#define EXEC ret = SPI_exec(query, 0)
#define EXEC_ INF(query); EXEC
#define EXECUTE_QUERY EXEC
/*
	DISCONNECT FROM SPI
	For internal purpose only.
*/
#define DISCONNECT SPI_finish()

// Print/show an error message, then stop the process if the previous GET (field) command return NULL value
#define NULL_ERR(msg) if (isnull) elog(ERROR, msg)

// Print a message in console log if the previous GET (field) command return NULL value
#define NULL_INF(msg) if (isnull) elog(INFO, msg)

// Print/show an error message, then stop the process if the previous SPI action return a minus value
#define RET_ERR(msg) if (ret < 0) elog(ERROR, msg)

// Check if user's trigger function fire before or not a event
#define UTRIGGER_FIRED_BEFORE (TRIGGER_FIRED_BEFORE(trigdata->tg_event))

// Declare a query command with an init string value, only one DEFQ macro statement is used in constructing a query.
#define DEFQ(c) strcpy(query, c)

// Append a string value to current query, many ADDQ macro statement can be used after a DEFQ in constructing a query.
#define ADDQ(c) strcat(query, c)


// Check if is there any row in the result set
#define NO_ROW (SPI_tuptable->alloced == SPI_tuptable->free)

/*
	DEFINING SAVED RESULT SET (TEMPORARILY)
	After EXEC a query command, you can use DEFR macro to allocate a space for temporary storing the result set.
	! Only 1 SRS can be used at a time, or you have to defining a set yourself without using the provided macros.
*/

#define DEFR(colNum) ROW_NUM = SPI_processed;												\
					 if (SAVED_RESULT != NULL) SPI_pfree(SAVED_RESULT);						\
					 SAVED_RESULT = (char**) SPI_palloc (sizeof(char*) * (ROW_NUM*colNum)); \
					 RESULT_COUNT = 0

#define DEFR_NESTED(colNum)	ROW_NUM_NESTED = SPI_processed;															\
								if (SAVED_RESULT_NESTED != NULL) SPI_pfree(SAVED_RESULT_NESTED);						\
								SAVED_RESULT_NESTED = (char**) SPI_palloc (sizeof(char*) * (ROW_NUM_NESTED*colNum));	\
								RESULT_COUNT_NESTED = 0

/*
	ADDING FIELDS VALUE TO THE SAVED RESULT SET
	! Remember the adding order for each value, it's the column index if you get the value out
	(using GET_SAVED_RESULT macro).
*/
#define ADDR(element) SAVED_RESULT[RESULT_COUNT++] = element
#define ADDR_NESTED(element) SAVED_RESULT_NESTED[RESULT_COUNT_NESTED++] = element

/*
	GET THE VALUE IN THE SRS
	String constant are returned.
	! Row and column are both 0-indexed.
	! This macro should be only used within the FOR_EACH_SAVED_RESULT_ROW looping macro.
*/
#define GET_SAVED_RESULT(row, col) SAVED_RESULT[row + col]
#define GET_SAVED_RESULT_NESTED(row, col) SAVED_RESULT_NESTED[row + col]

/*
	LOOP THROUGH EACH ROW IN THE SAVED RESULT SET
	! This looping macro is designed to wrap GET_SAVED_RESULT macro.
*/
#define FOR_EACH_SAVED_RESULT_ROW(intIterator, colNum) for (intIterator = 0; intIterator < ROW_NUM*colNum; intIterator+=colNum)
#define FOR_EACH_SAVED_RESULT_ROW_NESTED(intIterator, colNum) for (intIterator = 0; intIterator < ROW_NUM_NESTED*colNum; intIterator+=colNum)

// Loop through each row in the result set
#define FOR_EACH_RESULT_ROW(intIterator) for (intIterator = 0; intIterator < ROW_NUM; intIterator++)
#define FOR_EACH_RESULT_ROW_NESTED(intIterator) for (intIterator = 0; intIterator < ROW_NUM_NESTED; intIterator++)

//------------------------------------------------------------------------------------------------------------------------------------------

/* 
	DECLARING TRIGGER FUNCTION
	This macro should be put at the very beginning of a trigger function, 
		followed by a pair of curly brackets which contains the function body inside.
*/
#define FUNCTION(funcName) PG_FUNCTION_INFO_V1(funcName);					\
						   PGDLLEXPORT										\
						   Datum											\
						   funcName(PG_FUNCTION_ARGS)

/*
	TRIGGER FUNCTION ENDING
	This macro should be put at the very end of the function body, 
	before the closing curly brackets of the function 
	(goes with the FUNCTION macro).
*/
#define END if (SAVED_RESULT != NULL) SPI_pfree(SAVED_RESULT);				\
			if (SAVED_RESULT_NESTED != NULL) SPI_pfree(SAVED_RESULT_NESTED);\
			SPI_finish();													\
			if (checkNull) {												\
				SPI_getbinval(rettuple, tupdesc, 1, &isnull);				\
				if (isnull)													\
				rettuple = NULL; }											\
				return ((Datum) (rettuple))			

/*#define END SPI_pfree(SAVED_RESULT); \
			SPI_finish();	    \
			if (checkNull) {    \
				SPI_getbinval(rettuple, tupdesc, 1, &isnull); \
				if (isnull)                                   \
				rettuple = NULL; }                            \
				return ((Datum) (rettuple))
*/


/*	
	COMMON REQUIRED PROCEDURES FOR EACH TRIGGER FUNCTION
	- Defining core variables
	- Checking function context
	- Geting triggered row description
	- Preparing returning row
	- Check null
	- Connect to SPI
	This macro should be put after the variables declaration part and before the function's main process.
*/
#define REQUIRED_PROCEDURES PREPARE_STATEMENTS;													\
							if (!CALLED_AS_TRIGGER(fcinfo))										\
							ERR("User's trigger function is not called by trigger manager!");	\
							GET_TRIGGERED_ROW_DESC;												\
							if (TRIGGER_FIRED_BY_UPDATE(trigdata->tg_event))					\
								PREPARE_RETURN_NEW_ROW;											\
							else																\
								PREPARE_RETURN_TRIGGERED_ROW;									\
							if (!TRIGGER_FIRED_BY_DELETE(trigdata->tg_event)					\
										&& TRIGGER_FIRED_BEFORE(trigdata->tg_event))			\
									checkNull = true;											\
							if (CONNECT < 0)													\
								ERR("User's trigger function: SPI_connect returned a minus value")

/*
	GROUP OF MACROS FOR CONVERTING VALUES TO STRING (! VARIABLE STRING)
*/
#define TRIGGER_IS_FIRED_BY_INSERT TRIGGER_FIRED_BY_INSERT(trigdata->tg_event)
#define TRIGGER_IS_FIRED_BY_UPDATE TRIGGER_FIRED_BY_UPDATE(trigdata->tg_event)
#define INT32_TO_STR(i32, s) sprintf(s, "%d", i32)
#define INT64_TO_STR(i64, s) sprintf(s, "%lld", i64)
#define FLOAT32_TO_STR(f32, s) sprintf(s, "%f", f32)

// ! String constant, numeric scale: 10
#define NUMERIC_TO_STR(numeric, s) s = numeric_out_sci(numeric, 10)

/*
	GROUP OF MACROS FOR GETTING VALUE IN THE TRIGGERED ROW
*/

#define GET_INT32_ON_TRIGGERED_ROW(i32, col) i32 = DatumGetInt32(SPI_getbinval(trigdata->tg_trigtuple, tupdesc, col, &isnull))
#define GET_INT64_ON_TRIGGERED_ROW(i64, col) i64 = DatumGetInt64(SPI_getbinval(trigdata->tg_trigtuple, tupdesc, col, &isnull))
#define GET_FLOAT32_ON_TRIGGERED_ROW(f32, col) f32 = DatumGetFloat4(SPI_getbinval(trigdata->tg_trigtuple, tupdesc, col, &isnull))
#define GET_STR_ON_TRIGGERED_ROW(str, col) str = SPI_getvalue(trigdata->tg_trigtuple, tupdesc, col)
#define GET_NUMERIC_ON_TRIGGERED_ROW(numeric, col) numeric = DatumGetNumeric(SPI_getbinval(trigdata->tg_trigtuple, tupdesc, col, &isnull))

/*
	GROUP OF MACROS FOR GETTING VALUE IN THE NEW UPDATED ROW
*/

#define GET_INT32_ON_NEW_ROW(i32, col) i32 = DatumGetInt32(SPI_getbinval(trigdata->tg_newtuple, tupdesc, col, &isnull))
#define GET_INT64_ON_NEW_ROW(i64, col) i64 = DatumGetInt64(SPI_getbinval(trigdata->tg_newtuple, tupdesc, col, &isnull))
#define GET_STR_ON_NEW_ROW(str, col) str = SPI_getvalue(trigdata->tg_newtuple, tupdesc, col)
#define GET_NUMERIC_ON_NEW_ROW(numeric, col) numeric = DatumGetNumeric(SPI_getbinval(trigdata->tg_newtuple, tupdesc, col, &isnull))

/*
	GROUP OF MACROS FOR GETTING VALUE IN THE RESULT SET
*/


// Check if the value in the specified location in the result set is null or not
#define NULL_CELL(row, col) SPI_getbinval(SPI_tuptable->vals[row], SPI_tuptable->tupdesc, col, &isnull), isnull
// Check if the value in the specified location in the result set is not null
#define NOT_NULL_CELL(row, col) SPI_getbinval(SPI_tuptable->vals[row], SPI_tuptable->tupdesc, col, &isnull), !isnull


#define GET_INT32_ON_RESULT(i32, row, col) i32 = DatumGetInt32(SPI_getbinval(SPI_tuptable->vals[row], SPI_tuptable->tupdesc, col, &isnull))
#define GET_INT64_ON_RESULT(i64, row, col) i64 = DatumGetInt64(SPI_getbinval(SPI_tuptable->vals[row], SPI_tuptable->tupdesc, col, &isnull))
#define GET_FLOAT32_ON_RESULT(f32, row, col) f32 = DatumGetFloat4(SPI_getbinval(SPI_tuptable->vals[row], SPI_tuptable->tupdesc, col, &isnull))
#define GET_FLOAT64_ON_RESULT(f64, row, col) f64 = DatumGetFloat8(SPI_getbinval(SPI_tuptable->vals[row], SPI_tuptable->tupdesc, col, &isnull))
#define GET_NUMERIC_ON_RESULT(numeric, row, col) numeric = DatumGetNumeric(SPI_getbinval(SPI_tuptable->vals[row], SPI_tuptable->tupdesc, col, &isnull))
#define GET_STR_ON_RESULT(str, row, col) str = SPI_getvalue(SPI_tuptable->vals[row], SPI_tuptable->tupdesc, col)
#define GET_NULLABLE_STR_ON_RESULT(str, row, col) if(NULL_CELL(row, col)) str = "null"; \
												 else str = SPI_getvalue(SPI_tuptable->vals[row], SPI_tuptable->tupdesc, col)


// Get the row count in the result set
#define ROW_NUM_IN_RESULT_SET SPI_processed

// Defining a string constant
#define STRING(varName) char *varName = NULL

/*/ Short hand for comparing string, equal or not
#define STR_EQUAL(str1, str2) (strcmp(str1, str2) == 0)

// Short hand for comparing string, inequal or not
#define STR_IE(str1, str2) (!STR_EQUAL(str1, str2))*/

#define STR_EQUAL(a, b) (a && b && strcmp(a, b) == 0)
#define STR_INEQUAL(a, b) (a && b && strcmp(a, b) != 0)
#define STR_EQUAL_CI(a, b) (a && b && _strcmpi(a, b) == 0)
#define STR_INEQUAL_CI(a, b) (a && b && _strcmpi(a, b) != 0)


// Like ADDQ but if c is "null", ADDQ("NULL") instead of value
#define ADDQ_NULL_OR_VALUE(c, quote) if (STR_EQUAL_CI(c, "null")) { ADDQ("null"); } else { ADDQ(quote); ADDQ(c); ADDQ(quote); }
#define ADDQ_ISNULL_OR_VALUE(c, quote, _operator) if (STR_EQUAL_CI(c, "null")) { ADDQ("is null"); } \
												   else { ADDQ(_operator); ADDQ(quote); ADDQ(c); ADDQ(quote); }

/*
	CONSTRAINTS
*/

#define INT2STR_CONVERTER_LENGTH 20

/*---
VERSION 2.1: Clearer commands' semantic
---*/
#define TRIGGER(triggerName) PGDLLEXPORT FUNCTION(triggerName)
#define BEGIN REQUIRED_PROCEDURES
#define QUERY(init) DEFQ(init)
#define _QUERY(str) ADDQ(str)
#define _QUERY_NULL_OR_VALUE(c, quote) ADDQ_NULL_OR_VALUE(c, quote) 
#define _QUERY_ISNULL_OR_VALUE(c, quote, _operator) ADDQ_ISNULL_OR_VALUE(c, quote, _operator)
#define EXEC_QUERY EXEC
#define ALLOCATE_CACHE COL_NUM = SPI_tuptable->tupdesc->natts; \
					   DEFR(COL_NUM);						   \
					    a2Limiter = ROW_NUM/2
#define SAVE_TO_CACHE(data) ADDR(data)
#define GET_FROM_CACHE(row, col) GET_SAVED_RESULT(row, col)

/*
	New update mechanism (see Doc), marked as A2
*/
#define INIT_UPDATE_A2_FLAG a2Flag = 0
#define A2_FLAG_SET_OLD if (a2Flag >= a2Limiter) a2Flag = 0
#define A2_FLAG_SET_NEW if (false) a2Flag = a2Limiter
#define A2_FLAG_IS_OLD (a2Flag++ < a2Limiter)

#define FOR_EACH_CACHE_ROW(intIterator) FOR_EACH_SAVED_RESULT_ROW(intIterator, COL_NUM)

/*
	INIT A2 TAG
	(See doc for more information)
	Usually cycle tag is initialized two times, first time is for saving the result to cache
		and then the second time is for getting data from cache and perform updating process
	Macro type: Trigger component | Algorithm tag
*/
#define A2_INIT_CYCLE _PMVTGA2MechanismCounter = 0;				\
					  _PMVTGA2MechanismLimiter = ROW_NUM/2

/*
	VALIDATE A2 TAG
	(See doc for more information)
	Return a boolean 'true' value if the cycle is in the first half (deleting phase), 
		else return a boolean 'false' value if the cycle is in the second half (inserting phase)
	Macro type: Trigger component | Algorithm tag
*/
#define A2_VALIDATE_CYCLE (_PMVTGA2MechanismCounter++ < _PMVTGA2MechanismLimiter)

#endif


