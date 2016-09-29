#pragma once
#include "MVIAUTG_Config.h"

typedef struct s_Log *Log;
#define LOG_MANAGEMENT_N_INFO 4
struct s_Log{
	char* logId;
	char* logAction;
	char* logTime;
	char* tableName;
	int objTableId;

	int nPk;
	char* newPrimaryKeys[MAX_N_ELEMENTS];
	char* oldPrimaryKeys[MAX_N_ELEMENTS];
	char* oldNormalCols[MAX_N_ELEMENTS];
};

// Initialize a log type pointer and allocate memory
Log initLog(char* tableName, char* logId);

// Delete a log
void delLog(Log *log);