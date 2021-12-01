#ifndef DB_TRX_H
#define DB_TRX_H

#include <stdint.h>
#include <pthread.h>
#include "pages/page.h"

#define LOCK_TYPE_SHARED 0
#define LOCK_TYPE_EXCLUSIVE 1

typedef struct lock_t lock_t;

/* APIs for lock table */
int init_lock_table();

lock_t* lock_acquire(int64_t table_id, pagenum_t pagenum, int64_t key, int trx_id, int lock_mode);

int lock_record(lock_t* lock_obj, char* org_value, uint16_t org_val_size);

int lock_release(lock_t* lock_obj);

/*
 * Allocate a transaction structure and initialize it.
 * Return a unique transaction id (>= 1) if success, otherwise return 0.
 * Note that the transaction id should be unique for each transaction;
 * that is, you need to allocate a transaction id holding a mutex.
 */
int trx_begin(void);

int trx_rollback(int trx_id);

/*
 * Clean up the transaction with the given trx_id(transaction id) and its related information
 * that has been used in your lock manager. (Shrinking phase of strict 2PL)
 * Return the completed transaction id if success, otherwise return 0.
 */
int trx_commit(int trx_id);

#endif //DB_TRX_H
