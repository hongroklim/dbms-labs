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

    if(trxId <= 0){
        // increase the current Transaction ID
        keyMap->insert({++trxIdSeq, nullptr});
    }else{
        // set the designated id
        keyMap->insert({trxId, nullptr});
    }
    
    pthread_mutex_unlock(&mutex);
    return trxIdSeq;
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
        std::cout << trxId << " is not found in the transactions" << std::endl;
        throw std::runtime_error("Failed to find transaction id");
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