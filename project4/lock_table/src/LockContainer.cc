#include "LockContainer.h"

#include <string>

LockContainer::LockContainer(){
    keyMap = new std::map<std::size_t, LockEntry*>();
    mutex = PTHREAD_MUTEX_INITIALIZER;
}

LockContainer::~LockContainer(){
    // Remove all entries
    for(auto entry : *keyMap){
        delete entry.second;
    }

    delete keyMap;
}

std::size_t LockContainer::getHashKey(int table_id, int64_t key){
    uint64_t merged = table_id + key;
    std::size_t hashKey = std::hash<uint64_t>{}(merged);
    return hashKey;
}

LockEntry* LockContainer::getOrInsert(int table_id, int64_t key){
    // Hashing given parameters
    std::size_t hashKey = getHashKey(table_id, key);
    
    // Lock the container
    pthread_mutex_lock(&mutex);

    // Find key among exists
    auto item = keyMap->find(hashKey);
    LockEntry* entry{};
    if (item != keyMap->end()) {
        // Success to find
        entry = item->second;

        // Traverse the linked list
        LockEntry* prevEntry{};
        while(entry != nullptr && !entry->equals(table_id, key)){
            prevEntry = entry;
            entry = entry->getNext();
        }

        // Not exist in the list
        if(entry == nullptr){
            entry = new LockEntry(table_id, key);
            prevEntry->setNext(entry);
        }

    } else {
        // Fail to find a matched key
        entry = new LockEntry(table_id, key);
        keyMap->insert({hashKey, entry});
    }

    // Unlock the container
    pthread_mutex_unlock(&mutex);

    return entry;
}

int LockContainer::countEntries(){
    std::map<std::size_t, LockEntry*>::iterator it;

    LockEntry* entry{};
    int counts = 0;
    for(auto e : *keyMap){
        entry = e.second;
        while(entry != nullptr){
            entry = entry->getNext();
            counts++;
        }
    }

    return counts;
}