#include "file.h"
#include "page.h"

#include <iostream>

#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <sys/stat.h>
#include <FdHolder.h>

// set of fd
FdHolder* fd_holder {};

bool file_is_initialized(int64_t table_id);

void file_init(int64_t table_id);

void file_scale(int64_t table_id);

int file_get_fd(int64_t table_id){
    return fd_holder->get_fd(table_id);
}

// Open existing database file or create one if not existed.
int64_t file_open_table_file(const char* pathname){
    if(fd_holder == nullptr || !fd_holder->is_initialized()){
        fd_holder = new FdHolder();
        fd_holder->construct();
    }

    int64_t table_id = fd_holder->get_table_id(pathname);
    if(table_id > 0){
        // file has already opened
        std::cout << pathname << " has been already opened."
                  <<" return existing fd." << std::endl;
        return table_id;
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

    // save pathname and fd
    table_id = fd_holder->insert(pathname, fd);

    if(!file_is_initialized(table_id)){
        // clear file first
        ftruncate(fd, 0);
        fsync(fd);

        // initialize database file
        file_init(table_id);
        file_scale(table_id);
    }

    return table_id;
}

// Check whether given path is initialized as database file
bool file_is_initialized(int64_t table_id){
    // get header page
    auto* headerPage = new page_t();
    try {
        file_read_page(table_id, 0, headerPage);
    } catch (...) {
        // fail to read header page
        std::cout << "fail to read header page properly, which is considered as not initialized." << std::endl;
        delete headerPage;
        return false;
    }

    // get the number of total pages
    pagenum_t totalPages;
    page_read_value(headerPage, sizeof(pagenum_t), &totalPages, sizeof(totalPages));

    delete headerPage;

    return totalPages > 0;
}

// Initialize header page of database file
void file_init(int64_t table_id){
    // set header page
    auto* headerPage = new page_t();

    page_write_value(headerPage, 0, nullptr, sizeof(pagenum_t));  // free page number
    page_write_value(headerPage, sizeof(pagenum_t), nullptr, sizeof(pagenum_t));  // the number of total pages

    // write into file
    file_write_page(table_id, 0, headerPage);

    delete headerPage;
}

// Scale up database file per default size
void file_scale(int64_t table_id){
    pagenum_t pNum = 0, maxPageNum = 0;
    auto* headerPage = new page_t();

    int fd = fd_holder->get_fd(table_id);

    // get current state of file
    file_read_page(table_id, 0, headerPage);
    page_read_value(headerPage, 0, &pNum, sizeof(pagenum_t));
    page_read_value(headerPage, sizeof(pagenum_t), &maxPageNum, sizeof(pagenum_t));

    // keep the current value then increase
    pagenum_t prevNum = pNum++;
    pagenum_t appendStartNum = maxPageNum <= 1 ? 1 : maxPageNum;

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
    auto* freePage = new page_t();
    pNum = appendStartNum;
    pagenum_t cnt = maxPageNum - 1;
    while(pNum < cnt){
        page_write_value(freePage, 0, &(++pNum), sizeof(pagenum_t));
        file_write_page(table_id, pNum-1, freePage);
    }

    // last free page links previous number
    page_write_value(freePage, 0, &prevNum, sizeof(pagenum_t));
    file_write_page(table_id, pNum, freePage);

    // update header page
    page_write_value(headerPage, 0, &appendStartNum, sizeof(pagenum_t));
    page_write_value(headerPage, sizeof(pagenum_t), &maxPageNum, sizeof(pagenum_t));

    // write into file
    file_write_page(table_id, 0, headerPage);

    delete headerPage;
    delete freePage;
}

// Allocate an on-disk page from the free page list
pagenum_t file_alloc_page(int64_t table_id){
    auto* headerPage = new page_t();
    auto* aPage = new page_t();
    pagenum_t allocPageNum, nextPageNum;

    // get free page number in header page
    file_read_page(table_id, 0, headerPage);
    page_read_value(headerPage, 0, &allocPageNum, sizeof(pagenum_t));

    if(allocPageNum == 0){
        // if there is no free page
        file_scale(table_id);

        // then read header page again
        file_read_page(table_id, 0, headerPage);
        page_read_value(headerPage, 0, &allocPageNum, sizeof(pagenum_t));
    }

    // access page to be allocated then pop the number
    file_read_page(table_id, allocPageNum, aPage);
    page_read_value(aPage, 0, &nextPageNum, sizeof(pagenum_t));    // get number
    page_write_value(aPage, 0, nullptr, PAGE_SIZE);       // then erase all contents
    file_write_page(table_id, allocPageNum, aPage);

    // modify chaining of free pages
    page_write_value(headerPage, 0, &nextPageNum, sizeof(pagenum_t));
    file_write_page(table_id, 0, headerPage);

    delete headerPage;
    delete aPage;

    return allocPageNum;
}

// Free an on-disk page to the free page list
void file_free_page(int64_t table_id, pagenum_t pagenum){
    auto* headerPage = new page_t();
    auto* aPage = new page_t();
    pagenum_t freePageNum;

    // get free page number in header page
    file_read_page(table_id, 0, headerPage);
    page_read_value(headerPage, 0, &freePageNum, sizeof(pagenum_t));

    // set next free page number
    page_write_value(aPage, 0, &freePageNum, sizeof(pagenum_t));
    file_write_page(table_id, pagenum, aPage);

    // modify chaining of free pages
    page_write_value(headerPage, 0, &pagenum, sizeof(pagenum_t));
    file_write_page(table_id, 0, headerPage);

    delete headerPage;
    delete aPage;
}

// move cursor by given file descriptor and pagenum
void file_seek_bytes(const int fd, const pagenum_t pagenum){
    // move offset
    off_t offset = lseek(fd, pagenum * PAGE_SIZE, SEEK_SET);
    if(offset != pagenum * PAGE_SIZE){
        std::cout << "lseek offset : " << offset << ", error : " << strerror(errno) << std::endl;
        throw std::runtime_error("exception in lseek");
    }
}

// Read an on-disk page into the in-memory page structure(dest)
void file_read_page(int64_t table_id, pagenum_t pagenum, page_t* dest){
    int fd = fd_holder->get_fd(table_id);

    // seek first
    file_seek_bytes(fd, pagenum);

    // read bytes
    ssize_t size = read(fd, dest->data, PAGE_SIZE);
    if(size != PAGE_SIZE){
        std::cout << "read size : " << size << ", " << strerror(errno) << std::endl;
        throw std::runtime_error("exception in read");
    }
}

// Write an in-memory page(src) to the on-disk page
void file_write_page(int64_t table_id, pagenum_t pagenum, const page_t* src){
    int fd = fd_holder->get_fd(table_id);

    // seek first
    file_seek_bytes(fd, pagenum);

    // write bytes
    ssize_t size = write(fd, src->data, PAGE_SIZE);
    if(size != PAGE_SIZE){
        std::cout << "write size : " << size << ", " << strerror(errno) << std::endl;
        throw std::runtime_error("exception in write");
    }

    // sync buffer
    fdatasync(fd);
}

// Stop referencing the database file
void file_close_table_files(){
    int * fd_list = fd_holder->get_fd_list();
    int fd_cnt = fd_holder->get_fd_cnt();

    // close file descriptors
    for(int i=0; i<fd_cnt; i++){
        if(close(fd_list[i]) == -1){
            std::cout << "fail to close : " << strerror(errno) << std::endl;
        }
    }

    // destruct FdHolder
    fd_holder->destroy();
}