#include "gtest/gtest.h"

#include "logs/LogBuffer.h"

TEST(InitTest, test){
    LogBuffer* logBuffer = new LogBuffer((char*)"log_file.data");
    delete logBuffer;
}

TEST(InsertTest, test){
    LogBuffer* logBuffer = new LogBuffer((char*)"log_file.data");

    LogRecord* rec01 = new LogRecord(1, LOG_TYPE_BEGIN);
    logBuffer->append(rec01);
    logBuffer->flush();

    delete logBuffer;
}