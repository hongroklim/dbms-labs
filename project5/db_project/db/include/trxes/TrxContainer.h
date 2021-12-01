#ifndef DB_TRXCONTAINER_H
#define DB_TRXCONTAINER_H

#include "trxes/trx.h"
#include <map>

class TrxContainer {
private:
    int trxIdSeq = 0;
    std::map<int, lock_t*>* keyMap{};
    pthread_mutex_t mutex{};

public:
    TrxContainer();
    ~TrxContainer();

    int put(int trxId = 0);

    int setHead(int trxId, lock_t* lock);
    lock_t* getHead(int trxId);

    bool isExist(int trxId);
    void remove(int trxId);

    pthread_mutex_t* getMutex();
};

#endif //DB_TRXCONTAINER_H
