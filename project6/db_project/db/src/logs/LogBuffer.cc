#include <iostream>
#include <utility>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "logs/LogBuffer.h"

LogBuffer::LogBuffer(char* log_path) {
    logRecords = std::vector<LogRecord*>();
    logRecords.reserve(MAX_RECORD_COUNT);

    flushedLsn = 0;
    mutex = PTHREAD_MUTEX_INITIALIZER;

    logFile = new LogFile(log_path);

    lsnSeq = 1;
    trxLsn = new std::map<int, uint64_t>();
}

LogBuffer::~LogBuffer() {
    for(auto logRecord : logRecords){
        delete logRecord;
    }

    delete logFile;
    delete trxLsn;
}

uint64_t LogBuffer::append(LogRecord* logRecord) {
    pthread_mutex_lock(&mutex);
    if(logRecords.size() == MAX_RECORD_COUNT)
        flush();

    logRecord->lsn = lsnSeq;
    lsnSeq++;
    logRecord->prevLsn = getTrxLsn(logRecord->trxId);

    logRecords.push_back(logRecord);
    updateTrxLsn(logRecord->trxId, logRecord->lsn);

    pthread_mutex_unlock(&mutex);
    return logRecord->lsn;
}

void LogBuffer::flush() {
    pthread_mutex_lock(&mutex);
    uint64_t lsn = logFile->writeLog(logRecords);
    if(lsn != 0 && lsn > flushedLsn)
        flushedLsn = lsn;
    logRecords.clear();
    pthread_mutex_unlock(&mutex);
}

void LogBuffer::updateTrxLsn(int trxId, uint64_t lsn) {
    trxLsn->insert_or_assign(trxId, lsn);
}

uint64_t LogBuffer::getTrxLsn(int trxId) {
    auto kv = trxLsn->find(trxId);
    return (kv != trxLsn->end()) ? kv->second : 0;
}

std::vector<LogRecord *> LogBuffer::getCompensateLogs(int trxId) {
    std::vector<LogRecord *> cmpLogs;
    bool scannedAll = false;

    for(auto it=logRecords.rbegin(); it != logRecords.rend(); it++){
        if((*it)->trxId == trxId && (*it)->type == LOG_TYPE_UPDATE){
            cmpLogs.push_back(new LogRecord((*it)));
        }else if((*it)->trxId == trxId && (*it)->type == LOG_TYPE_BEGIN){
            scannedAll = true;
            break;
        }
    }

    // Early return if all logs are in buffer
    if(scannedAll)
        return cmpLogs;

    // Access log file
    logFile->getCompensateLogs(&cmpLogs, trxId);

    return cmpLogs;
}

void LogBuffer::analysis(std::vector<int> &winner, std::vector<int> &loser) {
    logFile->analysis(winner, loser);
}

void LogBuffer::redoPhase(FILE* fp, int log_num){
    logFile->redoPhase(fp, log_num);
}

void LogBuffer::undoPhase(FILE* fp, int log_num, const std::vector<int>& loser){
    logFile->undoPhase(fp, log_num, loser);
}
