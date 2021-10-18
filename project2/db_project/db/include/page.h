#ifndef DB_PAGE_H_
#define DB_PAGE_H_

#include <iostream>
#include <stdint.h>

// Size of each page : 4KiB
#define PAGE_SIZE (4 * 1024)

typedef uint64_t pagenum_t;
typedef uint16_t poff_t;

struct page_t {
    bool* data;

    page_t(){
        data = new bool[4096];
    }

    ~page_t(){
        delete []data;
    }
};

void page_read_value(const page_t* page, uint16_t offset, void* dest, uint16_t size);

void page_write_value(page_t* page, uint16_t offset, const void* src, uint16_t size);

void page_move_value(page_t* page, uint16_t dest_offset, uint16_t src_offset, uint16_t size);

#endif //DB_PAGE_H_
