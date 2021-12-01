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
LockContainer* lockContainer{};
TrxContainer* trxContainer{};
TrxContainer* implicitContainer{};

// Methods' Headers
std::vector<lock_t*> lock_find_prev_locks(LockEntry* entry, lock_t* newLock);
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

    // Get the corresponding entry
    LockEntry* entry = lockContainer->getOrInsert(table_id, pagenum);

    // Lock the entry
    pthread_mutex_lock(entry->getMutex());

    // Whether it has been already acquired in the same Transaction
    lock_t* lock = trx_find_acquired_lock(trxContainer, trx_id, table_id,
                                            pagenum, key, lock_mode);
    if(lock != nullptr){
        pthread_mutex_unlock(entry->getMutex());
        buffer_pin(table_id, pagenum);

        return lock;
    }
        
    // Create a new lock
    lock = new lock_t{key, lock_mode, entry->getTail(), nullptr, entry, false};
    lock->trxId = trx_id;

    // whether pass or wait?
    std::vector<lock_t*> prevLocks = lock_find_prev_locks(entry, lock);
    
    // When there is no explicit lock
    if(prevLocks.size() == 0){
        // Pin the page first
        buffer_pin(table_id, pagenum);

        // Lookup and Trial of implicit lock
        lock_t* prevLock = lock_implicit(lock);

        if(prevLock != nullptr){
            // Unpin then keep locking
            buffer_unpin(table_id, pagenum);
            prevLocks.push_back(prevLock);

        }else if(lock->lockMode == LOCK_TYPE_EXCLUSIVE){
            // Success to become an implicit one
            pthread_mutex_unlock(entry->getMutex());
            return lock;

        }else if(lock->lockMode == LOCK_TYPE_SHARED){
            // Confirm that there is no predecent
            buffer_unpin(table_id, pagenum);
        }
    }

    // Detect Dead lock
    //pthread_mutex_lock(trxContainer->getMutex());
    if(trx_dead_lock(trx_id, prevLocks)){
        std::cout << "Deadlock detected" << std::endl;

        pthread_mutex_unlock(trxContainer->getMutex());
        pthread_mutex_unlock(entry->getMutex());
        buffer_pin(table_id, pagenum);

        delete lock;
        return nullptr;
    }

    // Append into the transaction
    trx_append_lock(trxContainer, trx_id, lock);
    //pthread_mutex_unlock(trxContainer->getMutex());

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
        buffer_pin(table_id, pagenum);

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

    buffer_pin(table_id, pagenum);
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

lock_t* lock_implicit(lock_t* lock_obj){
    // Assume that there is no explicit precedent
    int prevTrxId = db_read_trx(lock_obj->sentinel->getTableId(),
                    lock_obj->sentinel->getPagenum(), lock_obj->key);
    
    pthread_mutex_lock(trxContainer->getMutex());
    
    // Whether the previous lock is alive
    bool isValidPrev = prevTrxId > 0 && trxContainer->isExist(prevTrxId);

    if(isValidPrev){
        // Otherwise extract into explicit lock
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

    // It means that there is no implicit lock
    return nullptr;
}

lock_t* lock_to_explicit(int trx_id, lock_t* new_lock){
    pthread_mutex_lock(implicitContainer->getMutex());

    lock_t* tmpLock = nullptr;
    lock_t* prevLock = implicitContainer->getHead(trx_id);

    // Find the corresponding implicit one
    while(prevLock != nullptr && prevLock->key != new_lock->key){
        tmpLock = prevLock;
        prevLock = prevLock->trxNext;
    }

    if(prevLock == nullptr){
        std::cout << "Failed to find the implicit lock" << std::endl;
        pthread_mutex_unlock(implicitContainer->getMutex());
        return nullptr;
    }

    // Unchain the previous references
    if(tmpLock != nullptr)
        tmpLock->trxNext = prevLock->trxNext;
    else
        implicitContainer->setHead(trx_id, nullptr);

    // Append the last to the entry
    LockEntry* entry = lockContainer->getOrInsert(
        new_lock->sentinel->getTableId(), new_lock->sentinel->getPagenum());
    
    if(entry->getHead() == nullptr)
        entry->setHead(prevLock);
    else
        entry->getTail()->next = prevLock;
    
    // Then append the last one

    pthread_mutex_unlock(implicitContainer->getMutex());

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
    int trxId = trxContainer->put();
    implicitContainer->put(trxId);
    return trxId;
}

lock_t* trx_find_acquired_lock(TrxContainer* container, int trxId, int64_t table_id,
            pagenum_t pagenum, int64_t key, int lockMode){
    pthread_mutex_lock(container->getMutex());
    lock_t* lock = container->getHead(trxId);

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

    pthread_mutex_unlock(container->getMutex());
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
            prevTrxLock = trxContainer->getHead(prevLock->trxId);
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

int trx_rollback(TrxContainer* container, int trx_id, bool canRelease){
    pthread_mutex_lock(container->getMutex());
    lock_t* lock = container->getHead(trx_id);

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
    container->remove(trx_id);
    pthread_mutex_unlock(container->getMutex());

    return 0;
}

int trx_rollback(int trx_id){
    
    trx_rollback(trxContainer, trx_id, true);
    trx_rollback(implicitContainer, trx_id, false);
    
    return trx_id;
}

int trx_commit(TrxContainer* container, int trx_id){
    pthread_mutex_lock(container->getMutex());
    lock_t *lock = container->getHead(trx_id);

    // Release all locks
    lock_t* tmpLock{};
    while(lock != nullptr){
        tmpLock = lock->trxNext;
        lock_release(lock);
        lock = tmpLock;
    }
    
    // Remove the transaction
    container->remove(trx_id);
    pthread_mutex_unlock(container->getMutex());

    return trx_id;
}

int trx_commit(int trx_id){
    
    trx_commit(trxContainer, trx_id);
    trx_commit(implicitContainer, trx_id);

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