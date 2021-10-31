#include "gtest/gtest.h"

#include <vector>
#include "indexes/bpt.h"
#include "buffers/buffer.h"

#include "indexes/InternalPage.h"
#include "indexes/LeafPage.h"

TEST(Initialize, init){
    // Init
    int num = 20;
    EXPECT_EQ(buffer_init(num), 0);

    // after initialized, next id is increased
    buf_ctrl_t* bf = buffer_get_ctrl();
    EXPECT_EQ(bf->nextBlockId, 1 + num);

    // traverse from the head
    int cnt = 0;
    block_t* block = bf->freeBlock;
    while(block != nullptr){
        EXPECT_EQ(block->block_id, num - (++cnt) + 1);
        block = block->next;
    }
    EXPECT_EQ(cnt, num);

    EXPECT_EQ(buffer_shutdown(), 0);
}

class BufferTest : public ::testing::Test {
private:
    std::string t = "buffer_test.db";

protected:
    const int NUM_BUF = 4;
    int64_t table_id;
    buf_ctrl_t* bf;

    void SetUp() override {
        remove(t.c_str());
        
        // initialize
        EXPECT_EQ(init_db(NUM_BUF), 0);

        // get buffer ctrl
        bf = buffer_get_ctrl();
        EXPECT_EQ(bf->nextBlockId, NUM_BUF+1);

        // open table file
        table_id = open_table(const_cast<char*>(t.c_str()));
        EXPECT_GT(table_id, 0);
    }

    void TearDown() override {
        EXPECT_EQ(shutdown_db(), 0);
    }

    int getReservedCnt(){
        int cntHead = 0;

        block_t* block = bf->headBlock;
        while(block != nullptr){
            cntHead++;
            block = block->next;
        }

        int cntTail = 0;

        block = bf->tailBlock;
        while(block != nullptr){
            cntTail++;
            block = block->prev;
        }

        EXPECT_EQ(cntHead, cntTail);

        return cntHead;
    }

    int getFreeCnt(){
        int cnt = 0;

        block_t* block = bf->freeBlock;
        while(block != nullptr){
            cnt++;
            block = block->next;
        }

        return cnt;
    }

    void verifyTotalSize(){
        // confirm all counts
        EXPECT_EQ(getFreeCnt() + getReservedCnt(), NUM_BUF)
            << "free : " << getFreeCnt() << ", reserved : " << getReservedCnt() << std::endl;

        if(getReservedCnt() == 1){
            // if there is only one reserved, it must be header page
            EXPECT_EQ(bf->headBlock->pagenum, 0);
        }
    }
};

TEST_F(BufferTest, init){
    std::cout << "init successful" << std::endl;
}

TEST_F(BufferTest, findEmpty){
    block_t* block = buffer_find_block(1, 1);
    EXPECT_EQ(block == nullptr, true);
}

TEST_F(BufferTest, alloc){
    pagenum_t pagenum = buffer_alloc_page(table_id);

    block_t* block = bf->headBlock;
    EXPECT_EQ(block->table_id, table_id);
    EXPECT_EQ(block->pagenum, pagenum);
}

TEST_F(BufferTest, allocThenFree){
    pagenum_t pagenum = buffer_alloc_page(table_id);

    int blockId = bf->headBlock->block_id;
    EXPECT_GT(blockId, 0);

    EXPECT_NE(bf->headBlock == nullptr, true);
    EXPECT_NE(bf->freeBlock->block_id, blockId);

    // free page
    buffer_free_page(table_id, pagenum);

    verifyTotalSize();
}

TEST_F(BufferTest, multipleAlloc){
    pagenum_t pagenum;
    std::vector<pagenum_t> pagenums = {};
    for(int i=0; i<NUM_BUF; i++){
        pagenum = buffer_alloc_page(table_id);
        buffer_unpin(table_id, pagenum);

        // append into the list
        pagenums.push_back(pagenum);
    }

    verifyTotalSize();
    EXPECT_EQ(bf->freeBlock == nullptr, true);
}

TEST_F(BufferTest, traverseMultipleAlloc){
    pagenum_t pagenum;
    std::vector<pagenum_t> pagenums = {};
    for(int i=0; i<NUM_BUF; i++){
        pagenum = buffer_alloc_page(table_id);
        buffer_unpin(table_id, pagenum);

        // append into the list
        pagenums.push_back(pagenum);
    }

    verifyTotalSize();
}

TEST_F(BufferTest, exceedingBuffer){
    pagenum_t pagenum;
    std::vector<pagenum_t> pagenums = {};
    for(int i=0; i<NUM_BUF; i++){
        pagenum = buffer_alloc_page(table_id);
        buffer_unpin(table_id, pagenum);

        // append into the list
        pagenums.push_back(pagenum);
    }

    // get the last id of blocks
    int blockId = bf->tailBlock->block_id;

    // then allocate
    buffer_alloc_page(table_id);
    
    // the tail's buffer block is reused
    EXPECT_EQ(bf->headBlock->block_id, blockId);
}

TEST_F(BufferTest, unpinInTheMiddle){
    pagenum_t pagenum;
    std::vector<pagenum_t> pagenums = {};
    for(int i=0; i<NUM_BUF; i++){
        pagenum = buffer_alloc_page(table_id);
        buffer_unpin(table_id, pagenum);

        // append into the list
        pagenums.push_back(pagenum);
    }

    // set all blocks pinned
    block_t* block = bf->headBlock;
    while(block != nullptr){
        block->is_pinned = true;
        block = block->next;
    }

    // get the last id of blocks
    int blockId = bf->tailBlock->prev->block_id;
    buffer_unpin(table_id, pagenums.at(2));

    // then allocate
    buffer_alloc_page(table_id);
    
    verifyTotalSize();
}