#include "headers/BufferHeaderSource.h"

#include <cstring>

#include "files/file.h"

BufferHeaderSource::BufferHeaderSource(int64_t p_table_id, page_t* page){
    table_id = p_table_id;
    headerPage = page;
}

void BufferHeaderSource::read(page_t* dest){
    memcpy(dest->data, headerPage->data, PAGE_SIZE);
}

void BufferHeaderSource::write(const page_t* src){
    memcpy(headerPage->data, src->data, PAGE_SIZE);
}