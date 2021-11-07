#include "LockEntry.h"

LockEntry::LockEntry(int table_id, int64_t key){
    this->table_id = table_id;
    this->key = key;

    this->tail = nullptr;
    this->head = nullptr;

    this->next = nullptr;
    this->mutex = PTHREAD_MUTEX_INITIALIZER;
}

LockEntry::~LockEntry(){
    // TODO destructor of LockEntry
}

bool LockEntry::equals(int table_id, int64_t key){
    return this->table_id == table_id && this->key == key;
}

void LockEntry::setNext(LockEntry* next){
    this->next = next;
}

LockEntry* LockEntry::getNext(){
    return next;
}

void LockEntry::setTail(lock_t* tail){
    this->tail = tail;
}

lock_t* LockEntry::getTail(){
    return this->tail;
}

void LockEntry::setHead(lock_t* head){
    this->head = head;
}

lock_t* LockEntry::getHead(){
    return this->head;
}

pthread_mutex_t* LockEntry::getMutex(){
    return &mutex;
}