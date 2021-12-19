#include "logs/LogRecord.h"

#include <cstring>

LogRecord::LogRecord(int pTrxId, short pType) {
    type = pType;
    setLogSize();

    trxId = pTrxId;
}

LogRecord::LogRecord(LogRecord *logRecord) {
    if(logRecord->type != LOG_TYPE_UPDATE)
        return;

    logSize = logRecord->logSize;
    trxId = logRecord->trxId;
    type = LOG_TYPE_COMPENSATE;

    tableId = logRecord->tableId;
    pagenum = logRecord->pagenum;
    offset = logRecord->offset;
    dataLength = logRecord->dataLength;

    // Inverse at compensation
    oldImage = logRecord->newImage;
    newImage = logRecord->oldImage;

    nextUndoLsn = logRecord->nextUndoLsn;
}

LogRecord::~LogRecord() {
    delete[] oldImage;
    delete[] newImage;
    delete[] bytes;
}

void LogRecord::setLogSize() {
    if(type == LOG_TYPE_BEGIN || type == LOG_TYPE_COMMIT || type == LOG_TYPE_ROLLBACK)
        logSize = 28;
    else if(type == LOG_TYPE_UPDATE)
        logSize = 48 + (2 * dataLength);
    else if(type == LOG_TYPE_COMPENSATE)
        logSize = 48 + (2 * dataLength) + 8;
}

char* LogRecord::getBytes() {
    delete[] bytes;

    if(logSize <= 0)
        return nullptr;

    bytes = new char[logSize];

    memcpy(&bytes[0], &logSize, sizeof(uint32_t));
    memcpy(&bytes[4], &lsn, sizeof(uint64_t));
    memcpy(&bytes[12], &prevLsn, sizeof(uint64_t));
    memcpy(&bytes[20], &trxId, sizeof(int));
    memcpy(&bytes[24], &type, sizeof(short));

    if(type == LOG_TYPE_UPDATE || type == LOG_TYPE_COMPENSATE){
        memcpy(&bytes[28], &tableId, sizeof(int64_t));
        memcpy(&bytes[36], &pagenum, sizeof(uint64_t));
        memcpy(&bytes[44], &offset, sizeof(uint16_t));
        memcpy(&bytes[46], &dataLength, sizeof(uint16_t));
        memcpy(&bytes[48], &oldImage, dataLength);
        memcpy(&bytes[48+dataLength], &newImage, dataLength);
    }

    if(type == LOG_TYPE_COMPENSATE)
        memcpy(&bytes[48+(2*dataLength)], &nextUndoLsn, sizeof(uint64_t));

    return bytes;
}

void LogRecord::setBytes(char* data){
    memcpy(&logSize, &data[0], sizeof(uint32_t));
    memcpy(&lsn, &data[4], sizeof(uint64_t));
    memcpy(&prevLsn, &data[12], sizeof(uint64_t));
    memcpy(&trxId, &data[20], sizeof(int));
    memcpy(&type, &data[24], sizeof(short));

    if(type == LOG_TYPE_UPDATE || type == LOG_TYPE_COMPENSATE){
        memcpy(&tableId, &data[28], sizeof(int64_t));
        memcpy(&pagenum, &data[36], sizeof(uint64_t));
        memcpy(&offset, &data[44], sizeof(uint16_t));
        memcpy(&dataLength, &data[46], sizeof(uint16_t));
        memcpy(&oldImage, &data[48], dataLength);
        memcpy(&newImage, &data[48+dataLength], dataLength);
    }

    if(type == LOG_TYPE_COMPENSATE)
        memcpy(&nextUndoLsn, &data[48+(2*dataLength)], sizeof(uint64_t));
}
