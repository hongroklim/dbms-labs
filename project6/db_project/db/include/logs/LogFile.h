#ifndef DB_LOGFILE_H
#define DB_LOGFILE_H

#include "logs/LogRecord.h"

#include <cstring>
#include <vector>
#include <cstdint>
#include <iostream>

class LogFile {
private:
    int fd{};
    int64_t fileOffset{};

public:
    explicit LogFile(char* log_path);

    void initFd(const char* log_path);
    int64_t getFileOffset();

    uint64_t writeLog(std::vector<LogRecord*> logs);
    void getCompensateLogs(std::vector<LogRecord *>* cmpLogs, int trxId);

    LogRecord* getLogRecord(int64_t offset);

    void analysis(std::vector<int>& winner, std::vector<int>& loser);

    void redoPhase(FILE* fp, int log_num);
    void undoPhase(FILE* fp, int log_num, const std::vector<int>& loser);
};

#endif //DB_LOGFILE_H
