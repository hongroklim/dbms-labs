#include "logs/log.h"

#include <cstring>
#include "logs/LogBuffer.h"
#include "logs/LogRecord.h"

LogBuffer* logBuffer{};

int log_init(int flag, int log_num, char* log_path, char* logmsg_path){
    logBuffer = new LogBuffer(log_path);
    log_recovery(flag, log_num, logmsg_path);
    return 0;
}

int log_shutdown(){
    logBuffer->flush();
    delete logBuffer;
    return 0;
}

void printAnalysis(FILE* fp, const std::vector<int>& winner, const std::vector<int>& loser){
    std::string winners;
    for(auto e : winner)
        winners += " " + std::to_string(e);

    std::string losers;
    for(auto e : loser)
        losers += " " + std::to_string(e);

    fprintf(fp, "[ANALYSIS] Analysis success. Winner:%s, Loser:%s\n", winners.c_str(), losers.c_str());
}

int log_recovery(int flag,  int log_num, char* logmsg_path){
    FILE* fp = fopen(logmsg_path, "a+");

    fprintf(fp, "[ANALYSIS] Analysis pass start\n");
    std::vector<int> winner, loser;
    logBuffer->analysis(winner, loser);
    printAnalysis(fp, winner, loser);

    fprintf(fp, "[REDO] Redo pass start\n");
    logBuffer->redoPhase(fp, flag == 1 ? log_num : 0);
    fprintf(fp, "[REDO] Redo pass end\n");

    fprintf(fp, "[UNDO] Undo pass start\n");
    logBuffer->undoPhase(fp, flag == 2 ? log_num : 0, std::vector<int>());
    fprintf(fp, "[UNDO] Undo pass end\n");

    fclose (fp);
    return 0;
}

void log_begin(int trxId){
    LogRecord* logRecord = new LogRecord(trxId, LOG_TYPE_BEGIN);
    logBuffer->append(logRecord);
}

uint64_t log_update(LeafPage* leafPage, int64_t key, int trxId,
                    char* newValue, uint16_t newValSize, uint16_t* oldValSize){
    LogRecord* logRecord = new LogRecord(trxId, LOG_TYPE_UPDATE);
    logRecord->tableId = leafPage->getTableId();
    logRecord->pagenum = leafPage->getPagenum();

    logRecord->oldImage = new char[108];
    logRecord->newImage = new char[108];

    // Set old and new image
    memcpy(logRecord->newImage, newValue, newValSize);
    leafPage->readValue(key, logRecord->oldImage, &logRecord->dataLength, &logRecord->offset);
    *oldValSize = logRecord->dataLength;

    return logBuffer->append(logRecord);
}

void log_flush(){
    logBuffer->flush();
}

void log_commit(int trxId){
    LogRecord* logRecord = new LogRecord(trxId, LOG_TYPE_COMMIT);
    logBuffer->append(logRecord);
    logBuffer->flush();
}

void log_abort(int trxId){
    LeafPage* leafPage{};
    uint64_t lsn{};

    std::vector<LogRecord *> comps = logBuffer->getCompensateLogs(trxId);
    for (auto cmpLog: comps) {
        // Append compensate log
        lsn = logBuffer->append(cmpLog);

        // Retrieve leaf and undo
        leafPage = new LeafPage(cmpLog->tableId, cmpLog->pagenum);
        leafPage->updateByOffset(cmpLog->offset, cmpLog->newImage, cmpLog->dataLength, lsn);
        leafPage->save();

        delete cmpLog;
    }

    // Undo changes
    LogRecord* logRecord = new LogRecord(trxId, LOG_TYPE_ROLLBACK);
    logBuffer->append(logRecord);
    logBuffer->flush();
}