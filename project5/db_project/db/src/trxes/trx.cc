#include "trxes/trx.h"

#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>
#include "trxes/LockContainer.h"
#include "trxes/TrxContainer.h"

#include "indexes/bpt.h"

struct lock_t {
    int64_t key{};
    int lockMode;

    lock_t* prev{};
    lock_t* next{};

    LockEntry* sentinel{};

    bool isAcquired{};

    lock_t* trxNext{};
    int trxId = 0;

    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    int waitCnt = 0;

    // TODO extract into logs
    bool isDirty = false;
    char* org_value{};
    int org_val_size = 0;

    ~lock_t(){
        if(isDirty && org_val_size > 0)
            delete[] org_value;
    }
};

// Attributes
LockContainer* lc{};
TrxContainer* tc{};

// Methods' Headers
std::vector<lock_t*> lock_find_prev_locks(LockEntry* entry, lock_t* newLock);
lock_t* trx_find_acquired_lock(int trxId, int64_t table_id,
            pagenum_t pagenum, int64_t key, int lockMode);
int trx_append_lock(int trxId, lock_t* newLock);
bool trx_dead_lock(int trxId, std::vector<lock_t*> prevLocks);

// Methods
int init_lock_table() {
    lc = new LockContainer();
    tc = new TrxContainer();
    return 0;
}

lock_t* lock_acquire(int64_t table_id, pagenum_t pagenum, int64_t key, int trx_id, int lock_mode){
    if(lock_mode != LOCK_TYPE_SHARED && lock_mode != LOCK_TYPE_EXCLUSIVE){
        std::cout << "Lock Mode must be either 0(Shared) or 1(Exclusive)" << std::endl;
        return nullptr;
    }

    // Get the corresponding entry
    LockEntry* entry = lc->getOrInsert(table_id, pagenum);

    // Lock the entry
    pthread_mutex_lock(entry->getMutex());

    // Whether it has been already acquired in the same Transaction
    lock_t* lock = trx_find_acquired_lock(trx_id, table_id, pagenum, key, lock_mode);
    if(lock != nullptr){
        pthread_mutex_unlock(entry->getMutex());
        return lock;
    }
        
    // Create a new lock
    lock = new lock_t{key, lock_mode, entry->getTail(), nullptr, entry, false};
    trx_append_lock(trx_id, lock);

    // whether pass or wait?
    std::vector<lock_t*> prevLocks = lock_find_prev_locks(entry, lock);
    
    // Detect Dead lock
    if(trx_dead_lock(trx_id, prevLocks)){
        std::cout << "Deadlock detected" << std::endl;
        pthread_mutex_unlock(entry->getMutex());

        delete lock;
        return nullptr;
    }

    // Check the ancestor
    if(entry->getHead() == nullptr){
        // Set head and tail
        entry->setHead(lock);

    }else{
        // Append behind the tail
        entry->getTail()->next = lock;
    }

    // Append the last
    entry->setTail(lock);

    // Unlock the entry
    pthread_mutex_unlock(entry->getMutex());

    // Early return when no waiting
    if(prevLocks.size() == 0){
        lock->isAcquired = true;
        return lock;
    }

    // Otherwise, pick the tail of locks
    lock_t* prevLock = prevLocks.front();

    // Suspend the previous lock
    pthread_mutex_lock(&prevLock->mutex);

    // Prevent a lost wake-up problem
    bool isWaited = false;
    if(prevLock->isAcquired){
        prevLock->waitCnt++;
        isWaited = true;
        pthread_cond_wait(&prevLock->cond, &prevLock->mutex);
    }

    if(!isWaited){
        pthread_mutex_unlock(&prevLock->mutex);
    }else{
        prevLock->waitCnt--;
        pthread_mutex_unlock(&prevLock->mutex);

        if(prevLock->waitCnt == 0){
            delete prevLock;
        }
    }

    return lock;
}

std::vector<lock_t*> lock_find_prev_locks(LockEntry* entry, lock_t* newLock){
    std::vector<lock_t*> locks;

    lock_t* lock = entry->getTail();
    while(lock != nullptr){
        // Skip conditions
        if(lock->key != newLock->key){
            lock = lock->prev;
            continue;

        }else if(lock->key == newLock->key
                && lock->trxId == newLock->trxId){
            lock = lock->prev;
            continue;
        }
        
        // Break conditions
        if(lock->lockMode == LOCK_TYPE_EXCLUSIVE){
            if(locks.size() > 0 && locks.back()->lockMode == LOCK_TYPE_SHARED){
                // Skip the situation when X ... S ... (X)
                break;
            }
            
            // If encountered lock is X (whatever acquired or not)
            locks.push_back(lock);
            break;

        }else if(newLock->lockMode == LOCK_TYPE_EXCLUSIVE
                && lock->lockMode == LOCK_TYPE_SHARED && lock->isAcquired){
            // If new lock is X and encounters acquired S
            locks.push_back(lock);
        }

        // Continue
        lock = lock->prev;
    }

    // the backward chain is the first
    return locks;
}

struct lock_test_info_t {
    int64_t table_id = 999;
    pagenum_t pagenum = 999;
    int64_t key = 999;
    std::vector<int> trx;
};

lock_test_info_t lockTest;

void lock_test_append(int trx_id, int lock_mode, bool isAcquired){
    if(std::find(lockTest.trx.begin(), lockTest.trx.end(), trx_id) == lockTest.trx.end()){
        lockTest.trx.push_back(trx_id);
    }

    LockEntry* entry = lc->getOrInsert(lockTest.table_id, lockTest.pagenum);
    lock_t* lock = new lock_t{lockTest.key, lock_mode, entry->getTail(), nullptr, entry, isAcquired};
    trx_append_lock(trx_id, lock);

    // Check the ancestor
    if(entry->getHead() == nullptr){
        // Set head and tail
        entry->setHead(lock);

    }else{
        // Append behind the tail
        entry->getTail()->next = lock;
    }

    // Append the last
    entry->setTail(lock);
}

void lock_test_clear(){
    for(auto trxId : lockTest.trx){
        trx_commit(trxId);
    }
}

int lock_record(lock_t* lock_obj, char* org_value, uint16_t org_val_size){
    lock_obj->org_value = new char[org_val_size];
    memcpy(lock_obj->org_value, org_value, org_val_size);

    lock_obj->org_val_size = org_val_size;
    lock_obj->isDirty = true;

    return 0;
}

int lock_release(lock_t* lock_obj) {
    LockEntry* entry = lock_obj->sentinel;
    
    // Lock the entry
    pthread_mutex_lock(entry->getMutex());

    // Signal the descendants
    pthread_mutex_lock(&lock_obj->mutex);

    bool isWaits = lock_obj->waitCnt > 0;
    lock_obj->isAcquired = false;
    pthread_cond_broadcast(&lock_obj->cond);

    pthread_mutex_unlock(&lock_obj->mutex);

    // Modify the head and tail in the entry
    if(entry->getHead() == lock_obj)
        entry->setHead(lock_obj->next);
    if(entry->getTail() == lock_obj)
        entry->setTail(lock_obj->prev);

    // Modify neighbor's reference
    if(lock_obj->prev != nullptr)
        lock_obj->prev->next = lock_obj->next;
    if(lock_obj->next != nullptr)
        lock_obj->next->prev = lock_obj->prev;

    // Delete itself
    if(!isWaits)
        delete lock_obj;

    // Unlock the entry
    pthread_mutex_unlock(entry->getMutex());

    return 0;
}

int trx_begin(void){
    int trxId = tc->put();
    return trxId;
}

lock_t* trx_find_acquired_lock(int trxId, int64_t table_id,
            pagenum_t pagenum, int64_t key, int lockMode){
    lock_t* lock = tc->getHead(trxId);

    while(lock != nullptr){
        if(lock->key == key
                && lock->sentinel->equals(table_id, pagenum)
                && lock->isAcquired
                && (lock->lockMode == lockMode
                    || lock->lockMode == LOCK_TYPE_EXCLUSIVE)){
            break;
        }else{
            lock = lock->trxNext;
        }
    }

    return lock;
}

int trx_append_lock(int trxId, lock_t* newLock){
    lock_t* lock = tc->getHead(trxId);

    // Set Transaction Id
    newLock->trxId = trxId;

    if(lock == nullptr){
        tc->setHead(trxId, newLock);
    }else{
        // find the last lock among the same transactions
        lock_t* tmpLock{};
        while(lock != nullptr){
            tmpLock = lock;
            lock = lock->trxNext;
        }

        // Append behind the last
        tmpLock->trxNext = newLock;
    }

    return 0;
}

bool trx_dead_lock(int trxId, std::vector<lock_t*> prevLocks){
    if(prevLocks.size() > 0){
        // Check previous locks' transactions
        lock_t* prevTrxLock;
        std::vector<lock_t*> prevPrevLocks;

        // Check Horizontally
        for(auto prevLock : prevLocks){
            // Why the prevLock's transaction isn't terminated?
            if(prevLock->trxId == trxId)
                return true;

            // Check Vertically
            prevTrxLock = tc->getHead(prevLock->trxId);
            while(prevTrxLock != nullptr){
                if(!prevTrxLock->isAcquired){
                    // Check the waiting locks
                    prevPrevLocks = lock_find_prev_locks(
                        prevTrxLock->sentinel, prevTrxLock);
                    
                    if(trx_dead_lock(trxId, prevPrevLocks)){
                        return true;
                    }
                }
                prevTrxLock = prevTrxLock->trxNext;
            }
        }
    }
    return false;
}

int trx_rollback(int trx_id){
    lock_t *lock = tc->getHead(trx_id);

    // Release all locks
    lock_t* tmpLock{};
    while(lock != nullptr){
        tmpLock = lock->trxNext;
        
        // If the lock is X and dirty
        if(lock->lockMode == LOCK_TYPE_EXCLUSIVE && lock->isDirty){
            uint16_t temp_val_size = 0;
            db_undo(lock->sentinel->getTableId(), lock->sentinel->getPagenum(),
            lock->key, lock->org_value, lock->org_val_size);
        }

        lock_release(lock);
        lock = tmpLock;
    }
    
    // Remove the transaction
    tc->remove(trx_id);

    return trx_id;
}

int trx_commit(int trx_id){
    lock_t *lock = tc->getHead(trx_id);

    // Release all locks
    lock_t* tmpLock{};
    while(lock != nullptr){
        tmpLock = lock->trxNext;
        lock_release(lock);
        lock = tmpLock;
    }
    
    // Remove the transaction
    tc->remove(trx_id);

    return trx_id;
}