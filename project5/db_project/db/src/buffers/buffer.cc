#include "buffers/buffer.h"

#include <cstring>
#include <iomanip>

#include "files/file.h"
#include "headers/BufferHeaderSource.h"

buf_ctrl_t* bf;

buf_ctrl_t* buffer_get_ctrl(){
    return bf;
}

void buffer_print(){
    int cnt = 0;

    block_t* block = bf->headBlock;
    while(block != nullptr){
        std::cout << std::setw(3) << block->pagenum
                << "("
                << std::setw(2) << block->block_id << ", "
                << buffer_is_pinned(block)
                << ") ";
        cnt++;
        block = block->next;
    }

    if(cnt > 0){
        std::cout << std::endl;
    }
}

int buffer_init(int num_buf){
    if(bf != nullptr){
        std::cout << "buffer manager has been already initialized." << std::endl;
        return -1;
    }

    if(num_buf <= 0){
        std::cout << "the number of buffers must be greater than zero." << std::endl;
        return -2;
    }

    // initialize buffer metadata
    bf = new buf_ctrl_t();

    // initialize free blocks
    return num_buf == buffer_scale(num_buf) ? 0 : -3;
}

int buffer_scale(int num_buf){
    int cnt = 0;

    block_t* block1 = nullptr;
    block_t* block2 = nullptr;
    
    // create the first one
    block1 = bf->freeBlock;

    for(int i=0; i<num_buf; i++){
        // preserve existing then create new one
        block2 = block1;

        try{
            block1 = new block_t(bf->nextBlockId++);

        }catch(...){
            // if failed to increase, stop at this moment
            bf->freeBlock = block2;
            bf->bufferNumber += cnt;
            return cnt;
        }
        
        // create double link; in case of existing free page is nullptr
        if(block2 != nullptr){
            block1->next = block2;
            block2->prev = block1;
        }

        cnt++;
    }

    // set head and buffer number
    bf->freeBlock = block1;
    bf->bufferNumber += cnt;

    return cnt;
}

// Open existing database file or create one if not existed.
int64_t buffer_open_table_file(const char* pathname){
    return file_open_table_file(pathname);
}

// Stop referencing the database file
void buffer_close_table_files(){
    file_close_table_files();
}

// Allocate an in-memory page from the free page list
pagenum_t buffer_alloc_page(int64_t table_id){
    page_t* headerPage = new page_t();
    buffer_read_page(table_id, 0, headerPage);

    block_t* block = buffer_empty_block();
    if(block == nullptr){
        throw std::runtime_error("failed to allocate buffer block.");
    }

    // allocate new pagenum
    BufferHeaderSource* hs = new BufferHeaderSource(table_id, headerPage);
    pagenum_t pagenum = file_alloc_page(table_id, hs);
    
    // free occupation
    buffer_write_page(table_id, 0, headerPage);
    buffer_unpin(table_id, 0);
    delete hs;
    delete headerPage;

    // set metadata
    block->table_id = table_id;
    block->pagenum = pagenum;
    block->is_dirty = false;

    // chain in LRU list
    pthread_mutex_lock(&bf->mutex);
    buffer_head_chain(block);
    pthread_mutex_unlock(&bf->mutex);

    return pagenum;
}

// Free an in-memory page to the free page list
void buffer_free_page(int64_t table_id, pagenum_t pagenum){
    page_t* headerPage = new page_t();
    buffer_read_page(table_id, 0, headerPage);

    block_t* block = buffer_find_block(table_id, pagenum);
    if(block != nullptr){
        // clear buffer block
        page_write_value(block->page, 0, nullptr, PAGE_SIZE);
        block->table_id = 0;
        block->pagenum = 0;
        block->is_dirty = false;
        block->lockCnt = 0;

        // modify neighbors
        buffer_pop_chain(block);

        // move on to free list
        block->next = bf->freeBlock;
        bf->freeBlock = block;
    }

    BufferHeaderSource* hs = new BufferHeaderSource(table_id, headerPage);
    file_free_page(table_id, pagenum, hs);
    
    // free occupation
    buffer_write_page(table_id, 0, headerPage);
    buffer_unpin(table_id, 0);
    delete hs;
    delete headerPage;

}

// Read an in-memory page into the in-memory page structure(dest)
void buffer_read_page(int64_t table_id, uint64_t pagenum, page_t* dest){
    // lock the Buffer Manager first
    pthread_mutex_lock(&bf->mutex);

    block_t* block;
    try{
        // find in buffer blocks first
        block = buffer_find_block(table_id, pagenum);

        // if not exists
        if(block == nullptr){
            block = buffer_empty_block();
            if(block == nullptr){
                throw std::runtime_error("failed to allocate buffer block.");
            }

            // set metadata
            block->table_id = table_id;
            block->pagenum = pagenum;
            block->is_dirty = false;

            // read page from the file system
            file_read_page(table_id, pagenum, block->page);
        }else{ // block != nullptr
            // if exists, pop the block in neighbors
            buffer_pop_chain(block);
        }

    }catch(std::exception &e){
        std::cout << e.what() << std::endl;
        pthread_mutex_unlock(&bf->mutex);
        return;
    }

    // chain in LRU list then unlock the buffer
    buffer_head_chain(block);
    pthread_mutex_unlock(&bf->mutex);

    // Waiting for the block's mutex
    buffer_pin(block);

    // write into parameter
    memcpy(dest->data, block->page->data, PAGE_SIZE);
}

// Write an in-memory page(src) to the in-disk page
void buffer_write_page(int64_t table_id, uint64_t pagenum, const page_t* src){
    // find in buffer blocks first
    block_t* block = buffer_find_block(table_id, pagenum);
    if(block == nullptr){
        throw std::invalid_argument("failed to find page in buffers");
    }

    // write into buffer
    memcpy(block->page->data, src->data, PAGE_SIZE);

    block->is_dirty = true;
}

block_t* buffer_find_block(int64_t table_id, uint64_t pagenum){
    block_t* block = bf->headBlock;

    while(block != nullptr){
        if(block->pagenum == pagenum && block->table_id == table_id){
            return block;
        }else{
            block = block->next;
        }
    }

    return nullptr;
}

bool buffer_is_pinned(block_t* block){
    return block->lockCnt > 0;
}

int buffer_pin(block_t* block){
    pthread_mutex_lock(&block->mutex);
    block->lockCnt++;

    // TODO debug
    /*
    if(block->pagenum != 0){
        std::cout << std::setw(3) << block->pagenum << " : ";
        buffer_print();
    }
     */

    return 0;
}

int buffer_pin(int64_t table_id, pagenum_t pagenum){
    block_t* block = buffer_find_block(table_id, pagenum);
    if(block != nullptr){
        buffer_pin(block);
    }else{
        std::cout << "[pin] failed to find buffer of " << pagenum  << " (pagenum)" << std::endl;
    }

    return 0;
}

// Remove a pin of buffer block
int buffer_unpin(int64_t table_id, uint64_t pagenum){
    // TODO debug
    /*
    if(pagenum != 0){
        std::cout << std::setw(3) << pagenum << " : unpin" << std::endl;
    }
     */

    block_t* block = buffer_find_block(table_id, pagenum);
    if(block == nullptr){
        std::cout << "[unpin] failed to find buffer of " << pagenum  << " (pagenum)" << std::endl;
        //buffer_print();
        return -1;
    }else if(buffer_is_pinned(block)){
        // set unpinned
        block->lockCnt--;
        pthread_mutex_unlock(&block->mutex);
    }else{
        std::cout << "buffer of " << pagenum  << " (pagenum) has been already unlocked" << std::endl;
    }

    return 0;
}

// Write buffer into file system and clear
int buffer_flush(block_t* block){
    if(block->is_dirty){
        file_write_page(block->table_id, block->pagenum, block->page);
    }

    return 0;
}

block_t* buffer_empty_block(){
    block_t* block = bf->freeBlock;
    if(block != nullptr){
        // try popping empty block at first
        bf->freeBlock = block->next;
        
        return block;

    }else{
        // if no remains, take one from LRU list
        block = bf->tailBlock;

        while(block != nullptr){
            if(!buffer_is_pinned(block)){
                // rechaining neighbors
                buffer_pop_chain(block);

                // make no chains
                block->prev = nullptr;
                block->next = nullptr;

                // flush existing one
                buffer_flush(block);

                return block;
            }else{
                block = block->prev;
            }
        }

        return nullptr;
    }
}

void buffer_pop_chain(block_t* block){
    if(bf->headBlock == block){
        // if it is on the head
        bf->headBlock = block->next;

    }else if(bf->tailBlock == block){
        // if it is no the tail
        bf->tailBlock = block->prev;
    }
    
    if(block->prev != nullptr)
        block->prev->next = block->next;
    
    if(block->next != nullptr)
        block->next->prev = block->prev;
}

void buffer_head_chain(block_t* block){
    if(bf->headBlock == block)
        return;

    // chain in the head
    block->prev = nullptr;
    block->next = bf->headBlock;
    if(block->next != nullptr)
        block->next->prev = block;
    
    // set head
    bf->headBlock = block;

    // if this is the only block, set tail
    if(bf->tailBlock == nullptr)
        bf->tailBlock = block;
}

/**
 * Flush the entire buffer and deference all opened files.
 */
int buffer_shutdown(){
    block_t* tmpBlock = nullptr;
    block_t* victimBlock = nullptr;
    
    // delete free buffers
    victimBlock = bf->freeBlock;
    while(victimBlock != nullptr){
        tmpBlock = victimBlock;
        
        // delete then shift pointer
        delete victimBlock;

        victimBlock = tmpBlock->next;
    }

    // delete reserved buffers
    victimBlock = bf->headBlock;
    while(victimBlock != nullptr){
        tmpBlock = victimBlock;

        // flush then delete
        buffer_flush(victimBlock);
        delete victimBlock;

        victimBlock = tmpBlock->next;
    }
    
    delete bf;
    bf = nullptr;

    return 0;
}