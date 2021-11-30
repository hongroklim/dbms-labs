#include "trxes/TrxContainer.h"

TrxContainer::TrxContainer(){
    keyMap = new std::map<int, lock_t*>();
    mutex = PTHREAD_MUTEX_INITIALIZER;
}

TrxContainer::~TrxContainer(){
    delete keyMap;
}

int TrxContainer::put(){
    pthread_mutex_lock(&mutex);

    // increase the current Transaction ID
    keyMap->insert({++trxIdSeq, nullptr});

    pthread_mutex_unlock(&mutex);
    return trxIdSeq;
}

int TrxContainer::setHead(int trxId, lock_t* lock){
    pthread_mutex_lock(&mutex);

    int result = 0;
    auto entry = keyMap->find(trxId);

    if(entry != keyMap->end()){
        keyMap->at(trxId) = lock;
    }else{
        result = -1;
    }

    pthread_mutex_unlock(&mutex);
    return result;
}

lock_t* TrxContainer::getHead(int trxId){
    pthread_mutex_lock(&mutex);

    lock_t* result{};
    auto entry = keyMap->find(trxId);

    if(entry != keyMap->end()){
        result = entry->second;
    }else{
        throw std::runtime_error(trxId+" is not found in the transactions");
    }

    pthread_mutex_unlock(&mutex);
    return result;
}

void TrxContainer::remove(int trxId){
    pthread_mutex_lock(&mutex);

    keyMap->erase(trxId);

    pthread_mutex_unlock(&mutex);
}

pthread_mutex_t* TrxContainer::getMutex(){
    return &mutex;
}