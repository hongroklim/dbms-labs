#include "logs/LogFile.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <algorithm>

LogFile::LogFile(char *log_path) {
    initFd(log_path);
    fileOffset = getFileOffset();
}

void LogFile::initFd(const char* log_path) {
    // open file in R/W, SYNC and create if no exist
    fd = open(log_path, O_RDWR | O_CREAT);
    if(fd == -1){
        std::cout << "fail to open log file" << std::endl;
        throw std::runtime_error("fail to open log file");
    }

    // set privileges of file
    if(fchmod(fd, S_IRUSR | S_IWUSR) == -1 || fsync(fd) == -1){
        throw std::runtime_error("fail to change mode of log file");
    }
}

int64_t LogFile::getFileOffset() {
    uint32_t logSize = 0;
    int64_t offset = 0;

    while(true){
        if(lseek(fd, offset, SEEK_SET) != offset
           || read(fd, &logSize, sizeof(logSize)) != sizeof(logSize)){
            return offset;
        }

        offset += logSize;
    }
}

uint64_t LogFile::writeLog(std::vector<LogRecord *> logs) {
    uint64_t flushedLsn = 0;

    for(auto logRecord : logs){
        if(lseek(fd, fileOffset, SEEK_SET) != fileOffset
           || write(fd, logRecord->getBytes(), logRecord->logSize)
              != logRecord->logSize){
            std::cout << "fail to flush log record " << logRecord->lsn << std::endl;
            return 0;
        }

        fileOffset += logRecord->logSize;
        flushedLsn = logRecord->lsn;

        fdatasync(fd);
        delete logRecord;
    }

    return flushedLsn;
}

void LogFile::getCompensateLogs(std::vector<LogRecord *> *cmpLogs, int trxId) {
    uint insertIdx = cmpLogs->size();
    uint64_t bufferLsn = insertIdx > 0 ? cmpLogs->back()->lsn : 0;

    int64_t offset = 0;
    LogRecord* logRecord{};

    while(offset <= fileOffset){
        logRecord = getLogRecord(offset);
        if(logRecord == nullptr || (insertIdx > 0 && logRecord->lsn >= bufferLsn))
            break;

        if(logRecord->trxId == trxId && logRecord->type == LOG_TYPE_UPDATE)
            cmpLogs->insert(cmpLogs->begin() + insertIdx, new LogRecord(logRecord));

        offset += logRecord->logSize;
    }
}

LogRecord* LogFile::getLogRecord(int64_t offset) {
    // Get log size
    uint32_t logSize = 0;
    if(lseek(fd, offset, SEEK_SET) != offset
       || read(fd, &logSize, sizeof(uint32_t)) != sizeof(uint32_t)){
        return nullptr;
    }

    // Allocate block
    char* bytes = new char[logSize];
    if(lseek(fd, offset, SEEK_SET) != offset
            || read(fd, bytes, logSize) != logSize){
        return nullptr;
    }

    LogRecord* logRecord = new LogRecord(0, 0);
    logRecord->setBytes(bytes);

    return logRecord;
}

void LogFile::analysis(std::vector<int> &winner, std::vector<int> &loser) {
    int64_t offset = 0;
    LogRecord* logRecord{};

    while(offset <= fileOffset){
        logRecord = getLogRecord(offset);
        if(logRecord == nullptr)
            break;

        if(logRecord->type == LOG_TYPE_BEGIN){
            loser.push_back(logRecord->trxId);
        }else if(logRecord->type == LOG_TYPE_COMMIT || logRecord->type == LOG_TYPE_ROLLBACK){
            loser.erase(std::remove(loser.begin(), loser.end(), logRecord->trxId), loser.end());
            winner.push_back(logRecord->trxId);
        }

        offset += logRecord->logSize;
    }
}

void LogFile::redoPhase(FILE* fp, int log_num) {
    int64_t offset = 0;
    LogRecord* logRecord{};

    while(offset <= fileOffset){
        logRecord = getLogRecord(offset);
        if(logRecord == nullptr)
            break;

        if(logRecord->type == LOG_TYPE_BEGIN){
            fprintf(fp, "LSN %lu [BEGIN] Transaction id %d\n", logRecord->lsn, logRecord->trxId);
        }else if(logRecord->type == LOG_TYPE_UPDATE){
            fprintf(fp, "LSN %lu [UPDATE] Transaction id %d redo apply\n", logRecord->lsn, logRecord->trxId);
        }else if(logRecord->type == LOG_TYPE_COMMIT){
            fprintf(fp, "LSN %lu [COMMIT] Transaction id %d\n", logRecord->lsn, logRecord->trxId);
        }else if(logRecord->type == LOG_TYPE_ROLLBACK){
            fprintf(fp, "LSN %lu [ROLLBACK] Transaction id %d\n", logRecord->lsn, logRecord->trxId);
        }

        offset += logRecord->logSize;
    }

}

void LogFile::undoPhase(FILE* fp, int log_num, const std::vector<int> &loser) {

}
