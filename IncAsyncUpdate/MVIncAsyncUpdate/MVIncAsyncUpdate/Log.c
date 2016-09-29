#include "Log.h"
#include <stdlib.h>
#include "MVIAUTG_CString.h"

// Initialize a log type pointer and allocate memory
Log initLog(char* tableName, char* logId) {
	int i;
	Log log = (Log) malloc (sizeof(struct s_Log));
	log->logId = copy(logId);
	log->tableName = copy(tableName);
	log->logAction = NULL;
	log->logTime = NULL;
	log->objTableId = -1;

	log->nPk = 0;
	for (i = 0; i < MAX_N_ELEMENTS; i++) {
		log->newPrimaryKeys[i] = NULL;
		log->oldPrimaryKeys[i] = NULL;
		log->oldNormalCols[i] = NULL;
	}
	return log;
}

// Delete a log
void delLog(Log *log) {
	int i;
	FREE((*log)->tableName);
	FREE((*log)->logId);
	FREE((*log)->logAction);
	FREE((*log)->logTime);
	for (i = 0; i < MAX_N_ELEMENTS; i++) {
		FREE((*log)->newPrimaryKeys[i]);
		FREE((*log)->oldPrimaryKeys[i]);
		FREE((*log)->oldNormalCols[i]);
	}
	FREE(*log);
}