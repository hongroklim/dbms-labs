#include "page.h"

#include "file.h"
#include <cstring>

void page_read_value(const page_t* page, uint16_t offset, void* dest, uint16_t size){
    if(offset + size > PAGE_SIZE){
        throw std::overflow_error("exceeded page offest "+ (offset + size));
    }

    memcpy(dest, &page->data[offset], size);
}

void page_write_value(page_t* page, uint16_t offset, const void* src, uint16_t size){
    if(offset + size > PAGE_SIZE){
        throw std::overflow_error("exceeded page offest "+ (offset + size));
    }
    
    if(src == nullptr){
        memset(&page->data[offset], 0, size);
    }else{
        memcpy(&page->data[offset], src, size);
    }
}

void page_move_value(page_t* page, uint16_t dest_offset, uint16_t src_offset, uint16_t size){
    if(dest_offset + size > PAGE_SIZE){
        throw std::overflow_error("exceeded page offest " + (dest_offset + size));
    }

    // TODO if src and dest range don't conflict, change memcpy
    memmove(&page->data[dest_offset], &page->data[src_offset], size);
}