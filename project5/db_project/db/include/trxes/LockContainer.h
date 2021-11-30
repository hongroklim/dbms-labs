#ifndef __LOCKCONTAINER_H__
#define __LOCKCONTAINER_H__

#include "trxes/LockEntry.h"
#include <map>

class LockContainer {
private:
    std::map<std::size_t, LockEntry*>* keyMap{};
    pthread_mutex_t mutex{};

    std::size_t getHashKey(int table_id, uint64_t pagenum);

public:
    LockContainer();
    ~LockContainer();

    LockEntry* getOrInsert(int table_id, uint64_t pagenum);
    int countEntries();
};

#endif /* __LOCKCONTAINER_H__ */
