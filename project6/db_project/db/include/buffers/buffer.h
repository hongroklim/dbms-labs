#ifndef DB_BUFFER_H
#define DB_BUFFER_H

#include <stdint.h>
#include <pthread.h>
#include "pages/page.h"

struct block_t {
    int block_id;
    page_t* page{};

    int64_t table_id = 0;
    pagenum_t pagenum = 0;
    bool is_dirty = false;
    pthread_mutex_t mutex{};
    unsigned long lockCnt = 0;

    block_t* prev = nullptr;
    block_t* next = nullptr;

    block_t(int p_block_id){
        block_id = p_block_id;
        page = new page_t();

        // initialize mutex
        pthread_mutexattr_t mutexAttr;
        pthread_mutexattr_init(&mutexAttr);
        pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&mutex, &mutexAttr);
    }

    ~block_t(){
        delete page;
    }
};

struct buf_ctrl_t {
    int nextBlockId = 1;
    int bufferNumber = -1;

    block_t* freeBlock = nullptr;

    block_t* headBlock = nullptr;
    block_t* tailBlock = nullptr;

    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
};

buf_ctrl_t* buffer_get_ctrl();

void buffer_print();

/**
 * Allocate the buffer pool with the given number of entries (i.e., num_buf).
 * On success, return 0. Otherwise, return a non-zero value.
 */
int buffer_init(int num_buf);

/**
 * Flush the entire buffer and deference all opened files.
 */
int buffer_shutdown();

// Open existing database file or create one if not existed.
int64_t buffer_open_table_file(const char* pathname);

// Stop referencing the database file
void buffer_close_table_files();

// Initialize empty buffer blocks
int buffer_scale(int num_buf);

// Allocate an in-memory page from the free page list
pagenum_t buffer_alloc_page(int64_t table_id);

// Free an in-memory page to the free page list
void buffer_free_page(int64_t table_id, pagenum_t pagenum);

/**
 * Read an in-memory page into the in-memory page structure(dest)
 * then pin the buffer block
 */
void buffer_read_page(int64_t table_id, pagenum_t pagenum, page_t* dest);

// Write an in-memory page(src) to the in-disk page
void buffer_write_page(int64_t table_id, pagenum_t pagenum, const page_t* src);

// Find corresponding buffer block
block_t* buffer_find_block(int64_t table_id, pagenum_t pagenum);

// Check whether it is pinned or not
bool buffer_is_pinned(block_t* block);

// Pin the buffer block
int buffer_pin(block_t* block);

// Remove a pin of buffer block
int buffer_unpin(int64_t table_id, pagenum_t pagenum);

// Write buffer into file system and remove
int buffer_flush(block_t* block);

// Get empty block in free or LRU list
block_t* buffer_empty_block();

void buffer_pop_chain(block_t* block);

void buffer_head_chain(block_t* block);

#endif //DB_BUFFER_H