#include "trxes/trx.h"

#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>
#include "buffers/buffer.h"
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
    bool isReleased{};

    lock_t* trxNext{};
    int trxId = 0;

    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    int waitCnt = 0;

    // Lock Compression
    bool* bitmap{};

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
LockContainer* lockContainer{};
TrxContainer* trxContainer{};
TrxContainer* implicitContainer{};

// Methods' Headers
std::vector<lock_t*> lock_find_prev_locks(LockEntry* entry, lock_t* lock);
lock_t* trx_find_acquired_lock(TrxContainer* container, int trxId,
                int64_t table_id, pagenum_t pagenum, int64_t key, int lockMode);
bool trx_dead_lock(int trxId, std::vector<lock_t*> prevLocks);
int trx_append_lock(TrxContainer* container, int trxId, lock_t* newLock);

// Methods
int init_lock_table() {
    lockContainer = new LockContainer();
    trxContainer = new TrxContainer();
    implicitContainer = new TrxContainer();

    return 0;
}

lock_t* lock_acquire(int64_t table_id, pagenum_t pagenum, int64_t key, int trx_id, int lock_mode){
    if(lock_mode != LOCK_TYPE_SHARED && lock_mode != LOCK_TYPE_EXCLUSIVE){
        std::cout << "Lock Mode must be either 0(Shared) or 1(Exclusive)" << std::endl;
        return nullptr;
    }

    // Unpin first
    buffer_unpin(table_id, pagenum);

    // Get the corresponding entry then lock
    LockEntry* entry = lockContainer->getOrInsert(table_id, pagenum);
    pthread_mutex_lock(entry->getMutex());

    // Whether it has been already acquired in the same Transaction
    lock_t* lock = trx_find_acquired_lock(trxContainer, trx_id, table_id, pagenum, key, lock_mode);

    // Search for implicit locks
    if(lock == nullptr)
        lock = trx_find_acquired_lock(implicitContainer, trx_id, table_id, pagenum, key, lock_mode);

    if(lock != nullptr){
        pthread_mutex_unlock(entry->getMutex());
        buffer_pin(table_id, pagenum);
        return lock;

    }
        
    // Create a new lock
    lock = new lock_t{key, lock_mode, entry->getTail(), nullptr, entry, false, false};
    lock->trxId = trx_id;

    // whether pass or wait?
    std::vector<lock_t*> prevLocks = lock_find_prev_locks(entry, lock);
    
    // When there is no explicit lock
    if(prevLocks.empty()){
        // Pin the page first
        pthread_mutex_unlock(entry->getMutex());
        buffer_pin(table_id, pagenum);

        // Lookup and Trial of implicit lock
        lock_t* prevLock = lock_implicit(lock);

        if(prevLock != nullptr){
            // Unpin then keep locking
            buffer_unpin(table_id, pagenum);
            pthread_mutex_lock(entry->getMutex());

            if(entry->getHead() == nullptr)
                entry->setHead(prevLock);

            if(entry->getTail() == nullptr){
                prevLock->prev = nullptr;
                entry->setTail(prevLock);
            }else{
                prevLock->prev = entry->getTail();
                prevLock->prev->next = prevLock;
            }

            // Append the last
            entry->setTail(prevLock);
            prevLocks.push_back(prevLock);

        }else if(lock->lockMode == LOCK_TYPE_EXCLUSIVE){
            // Success to become an implicit one
            lock->isAcquired = true;
            return lock;

        }else if(lock->lockMode == LOCK_TYPE_SHARED){
            // Confirm that there is no precedent
            buffer_unpin(table_id, pagenum);
            pthread_mutex_lock(entry->getMutex());
        }
    }

    if(trx_dead_lock(trx_id, prevLocks)){
        std::cout << "Deadlock detected for " << trx_id << "," << key << std::endl;

        pthread_mutex_unlock(entry->getMutex());
        buffer_pin(table_id, pagenum);

        delete lock;
        return nullptr;
    }

    // Append into the transaction
    pthread_mutex_lock(trxContainer->getMutex());
    trx_append_lock(trxContainer, trx_id, lock);
    pthread_mutex_unlock(trxContainer->getMutex());

    // Check the ancestor
    if(entry->getHead() == nullptr){
        // Set head and tail
        entry->setHead(lock);
    }else{
        // Append behind the tail
        lock->prev = entry->getTail();
        lock->prev->next = lock;
    }

    // Append the last
    entry->setTail(lock);

    // Unlock the entry
    pthread_mutex_unlock(entry->getMutex());

    // Early return when no waiting
    if(prevLocks.empty()){
        lock->isAcquired = true;
        buffer_pin(table_id, pagenum);

        return lock;
    }

    // Otherwise, pick the tail of locks
    lock_t* prevLock = prevLocks.front();

    // Suspend the previous lock
    pthread_mutex_lock(&prevLock->mutex);

    // Prevent a lost wake-up problem
    bool isWaited = false;
    if(!prevLock->isReleased){
        prevLock->waitCnt++;
        isWaited = true;
        pthread_cond_wait(&prevLock->cond, &prevLock->mutex);
    }

    pthread_mutex_unlock(&prevLock->mutex);

    // Delete the previous lock if necessary
    if(isWaited){
        prevLock->waitCnt--;
        pthread_mutex_unlock(&prevLock->mutex);

        if(prevLock->waitCnt == 0){
            delete prevLock;
        }
    }

    lock->isAcquired = true;
    buffer_pin(table_id, pagenum);
    return lock;
}

std::vector<lock_t*> lock_find_prev_locks(LockEntry* entry, lock_t* lock){
    std::vector<lock_t*> locks;

    lock_t* prevLock = lock->prev != nullptr ? lock : entry->getTail();

    while(prevLock != nullptr){
        // Skip conditions
        if(prevLock->key != lock->key){
            prevLock = prevLock->prev;
            continue;

        }else if(prevLock->key == lock->key
                 && prevLock->trxId == lock->trxId){
            prevLock = prevLock->prev;
            continue;
        }
        
        // Break conditions
        if(prevLock->lockMode == LOCK_TYPE_EXCLUSIVE){
            if(!locks.empty() && locks.back()->lockMode == LOCK_TYPE_SHARED){
                // Skip the situation when X ... S ... (X)
                break;
            }
            
            // If encountered lock is X (whatever acquired or not)
            locks.push_back(prevLock);
            break;

        }else if(lock->lockMode == LOCK_TYPE_EXCLUSIVE
                 && prevLock->lockMode == LOCK_TYPE_SHARED && prevLock->isAcquired){
            // If new lock is X and encounters acquired S
            locks.push_back(prevLock);
        }

        // Continue
        prevLock = prevLock->prev;
    }

    // the backward chain is the first
    return locks;
}

lock_t* lock_implicit(lock_t* lock_obj){
    // Assume that there is no explicit precedent
    int prevTrxId = db_read_trx(lock_obj->sentinel->getTableId(),
                    lock_obj->sentinel->getPagenum(), lock_obj->key);
    
    pthread_mutex_lock(trxContainer->getMutex());
    
    // Whether the previous lock is alive
    bool isValidPrev = prevTrxId > 0 && trxContainer->isExist(prevTrxId);

    if(isValidPrev){
        // Otherwise, extract into explicit lock
        lock_t* prevLock = lock_to_explicit(prevTrxId, lock_obj);

        // Then link to the transaction
        trx_append_lock(trxContainer, prevTrxId, prevLock);
        pthread_mutex_unlock(trxContainer->getMutex());
        return prevLock;
        
    }else if(lock_obj->lockMode == LOCK_TYPE_SHARED){
        // there is no implicit lock
        pthread_mutex_unlock(trxContainer->getMutex());

    }else if(lock_obj->lockMode == LOCK_TYPE_EXCLUSIVE){
        // the new lock will be the implicit one
        db_write_trx(lock_obj->sentinel->getTableId(),
            lock_obj->sentinel->getPagenum(), lock_obj->key, lock_obj->trxId);
        
        lock_obj->isAcquired = true;
        pthread_mutex_unlock(trxContainer->getMutex());

        // Append into implicit container
        pthread_mutex_lock(implicitContainer->getMutex());
        trx_append_lock(implicitContainer, lock_obj->trxId, lock_obj);
        pthread_mutex_unlock(implicitContainer->getMutex());
    }

    // It means that there is no precedent implicit lock
    return nullptr;
}

lock_t* lock_to_explicit(int trx_id, lock_t* new_lock){
    pthread_mutex_lock(implicitContainer->getMutex());

    lock_t* tmpLock = nullptr;
    lock_t* prevLock = implicitContainer->getHead(trx_id);

    // Find the corresponding implicit one
    while(prevLock != nullptr){
        if(prevLock->key == new_lock->key)
            break;
        tmpLock = prevLock;
        prevLock = prevLock->trxNext;
    }

    if(prevLock == nullptr){
        std::cout << "Failed to find a implicit lock " << new_lock->key << std::endl;
        pthread_mutex_unlock(implicitContainer->getMutex());
        return nullptr;
    }

    // Unchain the previous references
    if(implicitContainer->getHead(trx_id) == prevLock){
        implicitContainer->setHead(trx_id, prevLock->trxNext);
    }else if(tmpLock != nullptr){
        tmpLock->trxNext = prevLock->trxNext;
    }

    pthread_mutex_unlock(implicitContainer->getMutex());

    prevLock->trxNext = nullptr;
    return prevLock;
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
    //buffer_unpin(entry->getTableId(), entry->getPagenum());
    pthread_mutex_lock(entry->getMutex());

    // Signal the descendants
    pthread_mutex_lock(&lock_obj->mutex);

    bool isWaits = lock_obj->waitCnt > 0;
    lock_obj->isReleased = true;
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
    //buffer_pin(entry->getTableId(), entry->getPagenum());

    return 0;
}

int trx_begin(void){
    int trxId = trxContainer->put();
    implicitContainer->put(trxId);
    return trxId;
}

lock_t* trx_find_acquired_lock(TrxContainer* container, int trxId, int64_t table_id,
            pagenum_t pagenum, int64_t key, int lockMode){
    pthread_mutex_lock(container->getMutex());
    lock_t* lock = container->getHead(trxId);
    pthread_mutex_unlock(container->getMutex());

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

int trx_append_lock(TrxContainer* container, int trxId, lock_t* newLock){
    lock_t* lock = container->getHead(trxId);

    if(lock == nullptr){
        container->setHead(trxId, newLock);
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
    if(!prevLocks.empty()){
        // Check previous locks' transactions
        std::vector<lock_t*> prevTrxLocks;
        lock_t* prevTrxLock;
        std::vector<lock_t*> tmpLocks;

        // Check Horizontally
        for(auto prevLock : prevLocks){
            // Why the prevLock's transaction isn't terminated?
            if(prevLock->trxId == trxId)
                return true;

            // Gather the transaction's waiting locks
            prevTrxLocks.clear();
            prevTrxLock = trxContainer->getHead(prevLock->trxId);
            while(prevTrxLock != nullptr){
                if(!prevTrxLock->isAcquired){
                    tmpLocks = lock_find_prev_locks(prevTrxLock->sentinel, prevTrxLock);
                    if(!tmpLocks.empty()) {
                        prevTrxLocks.insert(prevTrxLocks.end(), tmpLocks.begin(), tmpLocks.end());
                    }
                }
                prevTrxLock = prevTrxLock->trxNext;
            }

            // Recursively check the deadlock
            if(trx_dead_lock(trxId, prevTrxLocks)){
                return true;
            }
        }
    }
    return false;
}

int trx_rollback(TrxContainer* container, int trx_id, bool canRelease){
    pthread_mutex_lock(container->getMutex());
    lock_t* lock = container->getHead(trx_id);
    pthread_mutex_unlock(container->getMutex());

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

        // Release or delete
        if(canRelease)
            lock_release(lock);
        else
            delete lock;

        lock = tmpLock;
    }
    
    // Remove the transaction
    pthread_mutex_lock(container->getMutex());
    container->remove(trx_id);
    pthread_mutex_unlock(container->getMutex());

    return 0;
}

int trx_rollback(int trx_id){
    
    trx_rollback(trxContainer, trx_id, true);
    trx_rollback(implicitContainer, trx_id, false);
    
    return trx_id;
}

int trx_commit(TrxContainer* container, int trx_id, bool canRelease){
    pthread_mutex_lock(container->getMutex());
    lock_t *lock = container->getHead(trx_id);
    pthread_mutex_unlock(container->getMutex());

    // Release all locks
    lock_t* tmpLock{};
    while(lock != nullptr){
        tmpLock = lock->trxNext;

        if(canRelease)
            lock_release(lock);
        else
            delete lock;

        lock = tmpLock;
    }
    
    // Remove the transaction
    pthread_mutex_lock(container->getMutex());
    container->remove(trx_id);
    pthread_mutex_unlock(container->getMutex());

    return trx_id;
}

int trx_commit(int trx_id){
    
    trx_commit(trxContainer, trx_id, true);
    trx_commit(implicitContainer, trx_id, false);

    return trx_id;
}

struct lock_test_info_t {
    int64_t table_id = 1;
    pagenum_t pagenum = 1;
    std::vector<int> trx;
};

lock_test_info_t lockTest;

void lock_test_append(int64_t key, int trx_id, int lock_mode, bool isAcquired){
    if(std::find(lockTest.trx.begin(), lockTest.trx.end(), trx_id) == lockTest.trx.end()){
        lockTest.trx.push_back(trx_id);
    }

    LockEntry* entry = lockContainer->getOrInsert(lockTest.table_id, lockTest.pagenum);
    lock_t* lock = new lock_t{key, lock_mode, entry->getTail(), nullptr, entry, isAcquired};
    trx_append_lock(trxContainer, trx_id, lock);

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