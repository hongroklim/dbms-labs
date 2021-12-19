#ifndef DB_LOGRECORD_H
#define DB_LOGRECORD_H

#include <cstdint>

#define LOG_TYPE_BEGIN 0
#define LOG_TYPE_UPDATE 1
#define LOG_TYPE_COMMIT 2
#define LOG_TYPE_ROLLBACK 3
#define LOG_TYPE_COMPENSATE 4

class LogRecord {
private:
    char* bytes{};
    void setLogSize();

public:
    uint32_t logSize{};
    uint64_t lsn{};
    uint64_t prevLsn{};
    int trxId{};
    short type{};

    int64_t tableId{};
    uint64_t pagenum{};
    uint16_t offset{};
    uint16_t dataLength{};
    char* oldImage{};
    char* newImage{};

    uint64_t nextUndoLsn{};

    LogRecord(int pTrxId, short pType);
    explicit LogRecord(LogRecord *logRecord);
    ~LogRecord();

    void setBytes(char* data);
    char* getBytes();
};

#endif //DB_LOGRECORD_H
