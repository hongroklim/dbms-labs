#include "gtest/gtest.h"

#include "files/file.h"
#include "pages/page.h"

#include <iostream>
#include <fcntl.h>

class FileManagerTest : public ::testing::Test {
public:
    static const char * pathname;

protected:
    int64_t table_id{};
    int fd{};

    void SetUp() override {
        remove(FileManagerTest::pathname);
        table_id = file_open_table_file(FileManagerTest::pathname);
        fd = file_get_fd(table_id);
    }

    void TearDown() override {
        file_close_table_files();
    }
};

const char * FileManagerTest::pathname = "file.db";

/**
 * When a file is newly created, a file of 10MiB is created and The number of pages
 * corresponding to 10MiB should be created. Check the "Number of pages" entry in the
 * header page.
 */
TEST(FileinitializerTest, initialization){
    // ensure that there is no database file
    remove(FileManagerTest::pathname);

    // file does not exist
    EXPECT_EQ(open(FileManagerTest::pathname, O_RDONLY), -1);

    // open file
    int64_t table_id = file_open_table_file(FileManagerTest::pathname);
    int fd = file_get_fd(table_id);
    EXPECT_GT(fd, 0);

    // the number of total pages
    uint64_t num = 0;
    lseek(fd, sizeof(pagenum_t), SEEK_SET);
    read(fd, (char*)&num, sizeof(pagenum_t));
    EXPECT_EQ(num, INITIAL_DB_FILE_SIZE / PAGE_SIZE);

    // get file size
    struct stat fStat {};
    fstat(fd, &fStat);
    EXPECT_EQ(fStat.st_size, INITIAL_DB_FILE_SIZE);

    // close
    file_close_table_files();

    // then remove database file
    EXPECT_EQ(remove(FileManagerTest::pathname), 0);
}

/**
 * extend the size of database file. file is extended as much as
 * initial size of database file, then next free page number and
 * total page number are updated.
 */
TEST_F(FileManagerTest, scaleFileSize){
    // trigger scaling up
    pagenum_t num = 0;
    lseek(fd, 0, SEEK_SET);
    write(fd, &num, sizeof(pagenum_t));
    fdatasync(fd);

    // trick as no free pages then allocate pages
    file_alloc_page(table_id);

    // the number of total pages
    lseek(fd, sizeof(pagenum_t), SEEK_SET);
    read(fd, (char*)&num, sizeof(pagenum_t));
    EXPECT_EQ(num, 2 * INITIAL_DB_FILE_SIZE / PAGE_SIZE);

    // get file size
    struct stat fStat {};
    fstat(fd, &fStat);
    EXPECT_EQ(fStat.st_size, 2 * INITIAL_DB_FILE_SIZE);
}

/**
 * check all free pages at initial state. count the free pages
 * the number defined in the header page, then compare the counts
 * with the number of all pages in header
 */
TEST_F(FileManagerTest, chainingFreePages){
    // get total number of pages
    pagenum_t expectedCnt;
    lseek(fd, sizeof(pagenum_t), SEEK_SET);
    read(fd, (char*)&expectedCnt, sizeof(pagenum_t));

    // get last value
    pagenum_t lastVal;
    lseek(fd, -4096, SEEK_END);
    read(fd, (char*)&lastVal, sizeof(pagenum_t));

    EXPECT_EQ(lastVal, 0);

    // get the first number in header page
    pagenum_t nextPageNum;
    lseek(fd, 0, SEEK_SET);
    read(fd, (char*)&nextPageNum, sizeof(pagenum_t));

    expectedCnt--;  // exclude header page

    // iterate free page list
    pagenum_t actualCnt = 0;
    while(nextPageNum > 0){
        lseek(fd, nextPageNum*PAGE_SIZE, SEEK_SET);
        read(fd, (char*)&nextPageNum, sizeof(pagenum_t));

        actualCnt++;
    }

    EXPECT_EQ(expectedCnt, actualCnt);
}

/**
 * Allocate two pages by calling file_alloc_page() twice, and free one of them by calling
 * file_free_page(). After that, iterate the free page list by yourself and check if only the freed
 * page is inside the free list.
 */
TEST_F(FileManagerTest, pageManagement){
    // allocate two pages
    pagenum_t allocPage1 = file_alloc_page(table_id),
              allocPage2 = file_alloc_page(table_id);

    // free one of them
    file_free_page(table_id, allocPage1);

    // get total number of pages
    pagenum_t expectedCnt;
    lseek(fd, sizeof(pagenum_t), SEEK_SET);
    read(fd, (char*)&expectedCnt, sizeof(pagenum_t));

    // exclude header page and allocated page
    expectedCnt -= 2;

    // get the first number in header page
    pagenum_t nextPageNum;
    lseek(fd, 0, SEEK_SET);
    read(fd, (char*)&nextPageNum, sizeof(pagenum_t));

    // iterate free page list
    pagenum_t actualCnt = 0;
    while(nextPageNum > 0){
        lseek(fd, nextPageNum*4096, SEEK_SET);
        read(fd, (char*)&nextPageNum, sizeof(pagenum_t));

        actualCnt++;
    }

    EXPECT_EQ(expectedCnt, actualCnt);
}

/**
 * After allocating a new page, write any 4096 bytes of value (e.g., "aaaa...") to the page by
 * calling the file_write_page() function. After reading the page again by calling the
 * file_read_page() function, check whether the read page and the written content are the
 * same.
 */
TEST_F(FileManagerTest, pageIO){
    auto* input = new page_t();
    auto*output = new page_t();

    for(int i=1; i<=4096; i++){
        input->data[i-1] = static_cast<char>(i);
    }

    // allocate
    pagenum_t pNum = file_alloc_page(table_id);

    // write
    file_write_page(table_id, pNum, input);

    // read
    file_read_page(table_id, pNum, output);

    // compare all elements both arrays
    for(int i=0; i<=4095; i++){
        EXPECT_EQ(input->data[i], output->data[i])
            << "index of " << i << " : " << input->data[i] << ", " << output->data[i];
        
    }
}