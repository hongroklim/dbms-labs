#ifndef DB_LOG_H
#define DB_LOG_H

#include "indexes/LeafPage.h"

int log_init(int flag, int log_num, char* log_path, char* logmsg_path);

int log_shutdown();

int log_recovery(int flag, int log_num, char* logmsg_path);

void log_begin(int trxId);

uint64_t log_update(LeafPage* leafPage, int64_t key, int trxId,
                    char* newValue, uint16_t newValSize, uint16_t* oldValSize);

void log_flush();

void log_commit(int trxId);

void log_abort(int trxId);

#endif //DB_LOG_H