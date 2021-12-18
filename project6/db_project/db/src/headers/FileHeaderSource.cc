#include "headers/FileHeaderSource.h"

#include "files/file.h"

void FileHeaderSource::read(page_t* dest){
    file_read_page(table_id, 0, dest);
}

void FileHeaderSource::write(const page_t* src){
    file_write_page(table_id, 0, src);
}