#ifndef DB_BPT_H
#define DB_BPT_H

#include <stdint.h>

#define MAX_TABLE_COUNT 20

/*
 * Open existing data file using ‘pathname’ or create one if not existed
 * If success, return the unique table id, which represents the own table in this database.
 * Otherwise, return negative value.
 */
int64_t open_table(char *pathname);

/*
 * Insert input ‘key/value’ (record) with its size to data file at the right place.
 * If success, return 0. Otherwise, return non-zero value.
 */
int db_insert(int64_t table_id, int64_t key, char * value, uint16_t val_size);

/*
 * Find the record containing input ‘key’.
 * If found matching ‘key’, store matched ‘value’ string in ret_valand matched ‘size’ in val_size.
 * If success, return 0. Otherwise, return non-zero value.
 * // TODO trx_id in db_find
 */
int db_find(int64_t table_id, int64_t key, char * ret_val, uint16_t * val_size, int trx_id);

/*
 * Find the matching record and delete it if found.
 * If success, return 0. Otherwise, return non-zero value.
 */
int db_delete(int64_t table_id, int64_t key);

/*
 * Find the matching key and modify the values.
 * If found matching‘key’, update the value of the record to ‘values’
 * string with its ‘new_val_size’ and store its size in ‘old_val_size’.
 * If success, return 0. Otherwise, return non-zero value.
 */
int db_update(int64_t table_id, int64_t key, char* values,
        uint16_t new_val_size, uint16_t* old_val_size, int trx_id);

int db_undo(int64_t table_id, uint64_t pagenum,
        int64_t key, char* org_value, uint16_t org_val_size);

/*
 * Initialize your database management system.
 * Initialize and allocate anything you need.
 * The total number of tables is less than 20.
 * If success, return 0. Otherwise, return non-zero value.
 */
int init_db(int num_buf);

/*
 * Shutdown your database management system.
 * Clean up everything.
 * If success, return 0. Otherwise, return non-zero value.
 */
int shutdown_db();

#endif //DB_BPT_H
