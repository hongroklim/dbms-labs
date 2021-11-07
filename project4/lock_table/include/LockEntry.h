#ifndef __LOCKENTRY_H__
#define __LOCKENTRY_H__

#include "lock_table.h"
#include <pthread.h>

class LockEntry {
private:
    int table_id;
    int64_t key;

    lock_t* tail;
    lock_t* head;

    LockEntry* next;

    pthread_mutex_t mutex{};

public:
    LockEntry(int table_id, int64_t key);
    ~LockEntry();

    bool equals(int table_id, int64_t key);

    void setNext(LockEntry* next);
    LockEntry* getNext();

    void setTail(lock_t* tail);
    lock_t* getTail();

    void setHead(lock_t* head);
    lock_t* getHead();

    pthread_mutex_t* getMutex();
};

#endif /* __LOCKENTRY_H__ */
