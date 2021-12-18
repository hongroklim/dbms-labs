#include "trxes/TrxContainer.h"

#include <iostream>

TrxContainer::TrxContainer(){
    keyMap = new std::map<int, lock_t*>();
    mutex = PTHREAD_MUTEX_INITIALIZER;
}

TrxContainer::~TrxContainer(){
    delete keyMap;
}

int TrxContainer::put(int trxId){
    pthread_mutex_lock(&mutex);
    int newTrxId = trxId > 0 ? trxId : ++this->trxIdSeq;
    keyMap->insert({newTrxId, nullptr});
    
    pthread_mutex_unlock(&mutex);
    return newTrxId;
}

int TrxContainer::setHead(int trxId, lock_t* lock){
    int result = 0;
    auto entry = keyMap->find(trxId);

    if(entry != keyMap->end()){
        keyMap->at(trxId) = lock;
    }else{
        result = -1;
    }

    return result;
}

lock_t* TrxContainer::getHead(int trxId){
    lock_t* result{};
    auto entry = keyMap->find(trxId);

    if(entry != keyMap->end()){
        result = entry->second;
    }else{
        for(const auto& e : *keyMap){
            std::cout << e.first << ":" << e.second << " ";
        }
        std::cout << std::endl;
        std::cout << trxId << " is not found in the transactions" << std::endl;
        return nullptr;
    }
    return result;
}

bool TrxContainer::isExist(int trxId){
    auto entry = keyMap->find(trxId);
    return entry != keyMap->end();
}

void TrxContainer::remove(int trxId){
    keyMap->erase(trxId);
}

pthread_mutex_t* TrxContainer::getMutex(){
    return &mutex;
}