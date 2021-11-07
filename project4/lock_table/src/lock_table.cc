#include "lock_table.h"

#include <pthread.h>
#include "LockContainer.h"

struct lock_t {
    lock_t* prev;
    lock_t* next;
    
    LockEntry* sentinel;

    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
};

typedef struct lock_t lock_t;

LockContainer* lc{};

int init_lock_table() {
    lc = new LockContainer();
    return 0;
}

lock_t* lock_acquire(int table_id, int64_t key) {
    // Get corresponding entry
    LockEntry* entry = lc->getOrInsert(table_id, key);

    // Lock the entry
    pthread_mutex_lock(entry->getMutex());

    // Create a new lock
    lock_t* lock = new lock_t{entry->getTail(), nullptr, entry};

    // Check the ancestor
    if(entry->getHead() == nullptr){
        // Set head and tail
        entry->setHead(lock);
        entry->setTail(lock);
        
        pthread_mutex_unlock(entry->getMutex());
        return lock;
    }

    // Append behind the tail
    entry->getTail()->next = lock;
    entry->setTail(lock);
    
    // Unlock the entry
    pthread_mutex_unlock(entry->getMutex());
    
    // Wait for the previous lock
    lock_t* prevLock = lock->prev;
    pthread_mutex_lock(&prevLock->mutex);
    
    // Prevent a lost wake-up problem
    if(entry->getHead() != lock){
        pthread_cond_wait(&prevLock->cond, &prevLock->mutex);
    }

    pthread_mutex_unlock(&prevLock->mutex);
    delete prevLock;

    return lock;
}

int lock_release(lock_t* lock_obj) {
    LockEntry* entry = lock_obj->sentinel;
    
    // Lock the entry
    pthread_mutex_lock(entry->getMutex());
    
    // Check the descendant
    if(lock_obj->next != nullptr){
        pthread_mutex_lock(&lock_obj->mutex);

        // Signal (with changing the head)
        pthread_cond_signal(&lock_obj->cond);
        entry->setHead(lock_obj->next);

        pthread_mutex_unlock(&lock_obj->mutex);

    }else{
        // Delete the sole lock
        delete lock_obj;

        // Modify the head and tail in the entry
        entry->setHead(nullptr);
        entry->setTail(nullptr);
    }

    // Unlock the entry
    pthread_mutex_unlock(entry->getMutex());

    return 0;
}
