#include "trxes/trx.h"

#include <iostream>
#include <vector>
#include <algorithm>
#include "trxes/LockContainer.h"
#include "trxes/TrxContainer.h"

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
};

// Attributes
LockContainer* lc{};
TrxContainer* tc{};

// Methods' Headers
std::vector<lock_t*> lock_find_prev_locks(LockEntry* entry, lock_t* newLock);
lock_t* trx_find_acquired_lock(int trxId, int64_t table_id,
            pagenum_t pagenum, int64_t key, int lockMode);
int trx_append_lock(int trxId, lock_t* newLock);
bool trx_dead_lock(int newTrxId, int tgtTrxId);

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

    // Whether it has been already acquired in the same Transaction
    lock_t* lock = trx_find_acquired_lock(trx_id, table_id, pagenum, key, lock_mode);
    if(lock != nullptr)
        return lock;

    // Get the corresponding entry
    LockEntry* entry = lc->getOrInsert(table_id, pagenum);

    // Lock the entry
    pthread_mutex_lock(entry->getMutex());

    // Create a new lock
    lock = new lock_t{key, lock_mode, entry->getTail(), nullptr, entry, false};
    trx_append_lock(trx_id, lock);

    // whether pass or wait?
    std::vector<lock_t*> prevLocks = lock_find_prev_locks(entry, lock);
    lock_t* prevLock = prevLocks.size() != 0 ? prevLocks.back() : nullptr;

    // Detect Dead lock
    if(prevLock != nullptr && trx_dead_lock(trx_id, prevLock->trxId)){
        std::cout << "Deadlock detected" << std::endl;

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
    if(prevLock == nullptr){
        lock->isAcquired = true;
        return lock;
    }

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
            locks.push_back(lock);
            break;

        }else if(newLock->lockMode == LOCK_TYPE_EXCLUSIVE
                && lock->lockMode == LOCK_TYPE_SHARED
                && lock->isAcquired){
            locks.push_back(lock);
            break;
        }

        // Continue
        lock = lock->prev;
    }

    // tail to the last
    reverse(locks.begin(), locks.end());
    return locks;
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
        tmpLock->trxNext = lock;
    }

    return 0;
}

bool trx_dead_lock(int newTrxId, int tgtTrxId){
    // Traverse all locks in the transaction
    lock_t* lock = tc->getHead(tgtTrxId);
    std::vector<lock_t*> prevLocks;
    
    while(lock != nullptr){
        // Skip the acquiring locks
        if(lock->isAcquired)
            continue;
        
        // Find the waiting locks
        prevLocks = lock_find_prev_locks(lock->sentinel, lock);
        for(auto prevLock : prevLocks){
            if(prevLock->trxId == newTrxId
                    || trx_dead_lock(newTrxId, prevLock->trxId)){
                // Waiting recursively, return true
                // Otherwise, keep traverse
                return true;
            }
        }

        // Move the next
        lock = lock->trxNext;
    }

    return false;
}

int trx_rollback(int trx_id){
    // TODO utilize undo log
    return 0;
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