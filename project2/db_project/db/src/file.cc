#include "file.h"

#include <iostream>
#include <fstream>
#include <set>

#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <sys/stat.h>

// set of fd
std::set<int> fdSet;

bool file_is_initialized(int fd);

void file_init(int fd);

void file_scale(int fd);

void page_read_value(const page_t* page, poff_t offset, void* dest, poff_t size);

void page_write_value(page_t* page, poff_t offset, const void* src, poff_t size);

// Open existing database file or create one if not existed.
int file_open_database_file(const char* pathname){
    if(pathname == nullptr || pathname[0] == '\0') {
        throw std::invalid_argument("pathname must not be empty");
    }

    // open file in R/W, SYNC and create if no exist
    int fd = open(pathname, O_RDWR | O_CREAT);
    if(fd == -1){
        std::cout << "fail to open file : " << strerror(errno) << std::endl;
        throw std::runtime_error("fail to open file");
    }

    // set privileges of file
    if(fchmod(fd, S_IRUSR | S_IWUSR) == -1 || fsync(fd) == -1){
        throw std::runtime_error("fail to change mode of file");
    }

    if(fdSet.find(fd) != fdSet.end()){
        // file has already opened
        std::cout << pathname << " has been already opened."
                      <<"return existing fd" << std::endl;
        return fd;
    }else{
        // insert into list
        fdSet.insert(fd);
    }

    if(!file_is_initialized(fd)){
        // clear file first
        ftruncate(fd, 0);
        fsync(fd);

        file_init(fd);
        file_scale(fd);
    }

    return fd;
}

// Check whether given path is initialized as database file
bool file_is_initialized(int fd){
    // get header page
    page_t headerPage {};
    try {
        file_read_page(fd, 0, &headerPage);
    } catch (...) {
        // fail to read header page
        std::cout << "fail to read header page properly, which is considered as not initialized." << std::endl;
        return false;
    }

    // get the number of total pages
    pagenum_t totalPages;
    page_read_value(&headerPage, sizeof(pagenum_t), &totalPages, sizeof(totalPages));

    return totalPages > 0;
}

// Initialize header page of database file
void file_init(int fd){
    // set header page
    page_t headerPage {};

    page_write_value(&headerPage, 0, nullptr, sizeof(pagenum_t));  // free page number
    page_write_value(&headerPage, sizeof(pagenum_t), nullptr, sizeof(pagenum_t));  // the number of total pages

    // write into file
    file_write_page(fd, 0, &headerPage);
}

// Scale up database file per default size
void file_scale(int fd){
    pagenum_t pNum = 0, maxPageNum = 0;
    page_t headerPage {};

    // get current state of file
    file_read_page(fd, 0, &headerPage);
    page_read_value(&headerPage, 0, &pNum, sizeof(pagenum_t));
    page_read_value(&headerPage, sizeof(pagenum_t), &maxPageNum, sizeof(pagenum_t));

    // keep the current value then increase
    pagenum_t prevNum = pNum++;

    // set max page number
    pagenum_t capacity = UINT64_MAX - maxPageNum;
    if(capacity == 0){
        throw std::runtime_error("can not increase database size");

    }else if(capacity > INITIAL_DB_FILE_SIZE / PAGE_SIZE){
        maxPageNum += INITIAL_DB_FILE_SIZE / PAGE_SIZE;

    }else{
        std::cout << "database file's capacity is " << capacity
            << "file will scale up to max" << std::endl;
        maxPageNum = UINT64_MAX;
    }

    // expand file size at first
    ftruncate(fd, maxPageNum * PAGE_SIZE);
    fsync(fd);

    // append free pages and link another
    page_t freePage {};
    pagenum_t cnt = maxPageNum - 1;
    while(pNum < cnt){
        page_write_value(&freePage, 0, &(++pNum), sizeof(pagenum_t));
        file_write_page(fd, pNum-1, &freePage);
    }

    // last free page links previous number
    page_write_value(&freePage, 0, &prevNum, sizeof(pagenum_t));
    file_write_page(fd, pNum, &freePage);

    // update header page
    page_write_value(&headerPage, 0, &(++prevNum), sizeof(pagenum_t));
    page_write_value(&headerPage, sizeof(pagenum_t), &maxPageNum, sizeof(pagenum_t));

    // write into file
    file_write_page(fd, 0, &headerPage);
}

// Allocate an on-disk page from the free page list
pagenum_t file_alloc_page(int fd){
    page_t headerPage {}, aPage {};
    pagenum_t allocPageNum, nextPageNum;

    // get free page number in header page
    file_read_page(fd, 0, &headerPage);
    page_read_value(&headerPage, 0, &allocPageNum, sizeof(pagenum_t));

    if(allocPageNum == 0){
        // if there is no free page
        file_scale(fd);

        // then read header page again
        file_read_page(fd, 0, &headerPage);
        page_read_value(&headerPage, 0, &allocPageNum, sizeof(pagenum_t));
    }

    // access page to be allocated then pop the number
    file_read_page(fd, allocPageNum, &aPage);
    page_read_value(&aPage, 0, &nextPageNum, sizeof(pagenum_t));    // get number
    page_write_value(&aPage, 0, nullptr, PAGE_SIZE);       // then erase all contents
    file_write_page(fd, allocPageNum, &aPage);

    // modify chaining of free pages
    page_write_value(&headerPage, 0, &nextPageNum, sizeof(pagenum_t));
    file_write_page(fd, 0, &headerPage);

    return allocPageNum;
}

// Free an on-disk page to the free page list
void file_free_page(int fd, pagenum_t pagenum){
    page_t headerPage {}, aPage {};
    pagenum_t freePageNum;

    // get free page number in header page
    file_read_page(fd, 0, &headerPage);
    page_read_value(&headerPage, 0, &freePageNum, sizeof(pagenum_t));

    // set next free page number
    page_write_value(&aPage, 0, &freePageNum, sizeof(pagenum_t));
    file_write_page(fd, pagenum, &aPage);

    // modify chaining of free pages
    page_write_value(&headerPage, 0, &pagenum, sizeof(pagenum_t));
    file_write_page(fd, 0, &headerPage);
}

// move cursor by given file descriptor and pagenum
void file_seek_bytes(const int fd, const pagenum_t pagenum){
    // check file descriptor
    if(fdSet.find(fd) == fdSet.end()) {
        throw std::invalid_argument("file descriptor is not valid");
    }

    // move offset
    off_t offset = lseek(fd, pagenum * PAGE_SIZE, SEEK_SET);
    if(offset != pagenum * PAGE_SIZE){
        std::cout << "lseek offset : " << offset << ", error : " << strerror(errno) << std::endl;
        throw std::runtime_error("exception in lseek");
    }
}

// Read an on-disk page into the in-memory page structure(dest)
void file_read_page(int fd, pagenum_t pagenum, page_t* dest){
    // seek first
    file_seek_bytes(fd, pagenum);

    // read bytes
    ssize_t size = read(fd, &dest->data, PAGE_SIZE);
    if(size != PAGE_SIZE){
        std::cout << "read size : " << size << ", " << strerror(errno) << std::endl;
        throw std::runtime_error("exception in read");
    }
}

// Write an in-memory page(src) to the on-disk page
void file_write_page(int fd, pagenum_t pagenum, const page_t* src){
    // seek first
    file_seek_bytes(fd, pagenum);

    // write bytes
    ssize_t size = write(fd, &src->data, PAGE_SIZE);
    if(size != PAGE_SIZE){
        std::cout << "write size : " << size << ", " << strerror(errno) << std::endl;
        throw std::runtime_error("exception in write");
    }

    // sync buffer
    fdatasync(fd);
}

void page_read_value(const page_t* page, poff_t offset, void* dest, poff_t size){
    memcpy(dest, &page->data[offset], size);
}

void page_write_value(page_t* page, poff_t offset, const void* src, poff_t size){
    if(src == nullptr){
        memset(&page->data[offset], 0, size);
    }else{
        memcpy(&page->data[offset], src, size);
    }
}

// Stop referencing the database file
void file_close_database_file(){
    for(auto fd : fdSet){
        if(close(fd) == -1){
            std::cout << "fail to close : " << strerror(errno) << std::endl;
        }
    }
    fdSet.clear();
}