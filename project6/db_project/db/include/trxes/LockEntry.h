#ifndef __LOCKENTRY_H__
#define __LOCKENTRY_H__

#include "trxes/trx.h"

class LockEntry {
private:
    int64_t table_id;
    uint64_t pagenum;

    lock_t* tail;
    lock_t* head;

    LockEntry* next;

    pthread_mutex_t mutex{};

public:
    LockEntry(int64_t table_id, uint64_t pagenum);
    ~LockEntry();

    int getTableId();
    uint64_t getPagenum();

    bool equals(int64_t table_id, uint64_t pagenum);

    void setNext(LockEntry* next);
    LockEntry* getNext();

    void setTail(lock_t* tail);
    lock_t* getTail();

    void setHead(lock_t* head);
    lock_t* getHead();

    pthread_mutex_t* getMutex();
};

#endif /* __LOCKENTRY_H__ */
