#ifndef DB_LOGBUFFER_H
#define DB_LOGBUFFER_H

#include "logs/LogRecord.h"
#include "logs/LogFile.h"

#include <vector>
#include <string>
#include <map>

class LogBuffer {
private:
    std::vector<LogRecord*> logRecords{};
    static const int MAX_RECORD_COUNT = 20;

    uint64_t flushedLsn{};
    LogFile* logFile{};

    uint64_t lsnSeq{};
    std::map<int, uint64_t>* trxLsn{};

    pthread_mutex_t mutex{};

public:
    explicit LogBuffer(char* log_path);
    ~LogBuffer();

    uint64_t append(LogRecord* logRecord);
    void flush();

    void updateTrxLsn(int trxId, uint64_t lsn);
    uint64_t getTrxLsn(int trxId);

    std::vector<LogRecord*> getCompensateLogs(int trxId);

    void analysis(std::vector<int>& winner, std::vector<int>& loser);
    void redoPhase(FILE* fp, int log_num);
    void undoPhase(FILE* fp, int log_num, const std::vector<int>& loser);
};

#endif //DB_LOGBUFFER_H
