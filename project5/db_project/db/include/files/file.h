#ifndef DB_FILE_H_
#define DB_FILE_H_

#include <iostream>
#include <stdint.h>
#include "pages/page.h"
#include "headers/AHeaderSource.h"

// Size of database file : 10MiB
#define INITIAL_DB_FILE_SIZE (1 * 1024 * 1024)

// Open existing database file or create one if not existed.
int64_t file_open_table_file(const char* pathname);

// Allocate an on-disk page from the free page list
uint64_t file_alloc_page(int64_t table_id, AHeaderSource* hs);
uint64_t file_alloc_page(int64_t table_id);

// Free an on-disk page to the free page list
void file_free_page(int64_t table_id, uint64_t pagenum, AHeaderSource* hs);
void file_free_page(int64_t table_id, uint64_t pagenum);

// Read an on-disk page into the in-memory page structure(dest)
void file_read_page(int64_t table_id, uint64_t pagenum, page_t* dest);

// Write an in-memory page(src) to the on-disk page
void file_write_page(int64_t table_id, uint64_t pagenum, const page_t* src);

// Stop referencing the database file
void file_close_table_files();

int file_get_fd(int64_t table_id);

#endif //DB_FILE_H_

