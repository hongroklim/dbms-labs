#ifndef DB_TRX_H
#define DB_TRX_H

#include <stdint.h>
#include <pthread.h>
#include "pages/page.h"

#define LOCK_TYPE_SHARED 0
#define LOCK_TYPE_EXCLUSIVE 1

typedef struct lock_t lock_t;

/* APIs for lock table */
int lock_init();

int lock_shutdown();

lock_t* lock_acquire(int64_t table_id, pagenum_t pagenum, int64_t key, int trx_id, int lock_mode);

/* 
 * Return the previous implicit lock
 * (which is linked into explicit locks in this fuction)
 * Otherwise, return nullptr. That is, there is no precedent
 */
lock_t* lock_implicit(lock_t* lock_obj);


/**
 * @brief Find the valid implicit lock and return it
 * 
 * it will pop from the implicit list then be put
 * in the series of explicit lock.
 * 
 * @param trx_id 
 * @param lock_obj 
 * @return lock_t* 
 */
lock_t* lock_to_explicit(int trx_id, lock_t* lock_obj);

int lock_release(lock_t* lock_obj, bool isCommit);

/*
 * Allocate a transaction structure and initialize it.
 * Return a unique transaction id (>= 1) if success, otherwise return 0.
 * Note that the transaction id should be unique for each transaction;
 * that is, you need to allocate a transaction id holding a mutex.
 */
int trx_begin(void);

int trx_abort(int trx_id);

/*
 * Clean up the transaction with the given trx_id(transaction id) and its related information
 * that has been used in your lock manager. (Shrinking phase of strict 2PL)
 * Return the completed transaction id if success, otherwise return 0.
 */
int trx_commit(int trx_id);

#endif //DB_TRX_H