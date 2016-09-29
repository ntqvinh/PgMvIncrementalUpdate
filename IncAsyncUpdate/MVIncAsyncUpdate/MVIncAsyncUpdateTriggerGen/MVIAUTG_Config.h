#define ESTIMATING_VALUE									(100)

/*
	MAXIMUM CHARACTER LENGTHS
	! + 1 for string terminator
*/

#define MAX_L_QUERY											2000
#define MAX_L_TRIGGER_NAME									1000
#define MAX_L_VAR											20
#define MAX_L_DBNAME										ESTIMATING_VALUE
#define MAX_L_USERNAME										ESTIMATING_VALUE
#define MAX_L_PASSWORD										ESTIMATING_VALUE
#define MAX_L_OUTPUT_PATH									ESTIMATING_VALUE
#define MAX_L_MV_NAME										ESTIMATING_VALUE
#define MAX_L_FILE_NAME										ESTIMATING_VALUE
#define MAX_L_PORT											5

/*
	MAXIMUM NUMBER OF ELEMENTS
*/

#define MAX_N_ELEMENTS										50
#define MAX_N_TABLES										10
#define MAX_N_COLS											ESTIMATING_VALUE
#define MAX_N_EXPRESSIONS									(MAX_N_TABLES * 10)
#define MAX_N_SELECT_ELEMENT								ESTIMATING_VALUE
