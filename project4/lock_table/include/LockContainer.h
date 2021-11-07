#ifndef __LOCKCONTAINER_H__
#define __LOCKCONTAINER_H__

#include "LockEntry.h"
#include <map>

class LockContainer {
private:
    std::map<std::size_t, LockEntry*>* keyMap{};
    pthread_mutex_t mutex{};

    std::size_t getHashKey(int table_id, int64_t key);

public:
    LockContainer();
    ~LockContainer();

    LockEntry* getOrInsert(int table_id, int64_t key);
    int countEntries();
};

#endif /* __LOCKCONTAINER_H__ */
