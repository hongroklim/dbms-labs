#include "pages/AbstractPage.h"

#include "buffers/buffer.h"

AbstractPage::AbstractPage(int64_t p_table_id, pagenum_t p_pagenum, page_t* p_page){
    construct(p_table_id, p_pagenum, p_page);
}

AbstractPage::~AbstractPage(){
    if(page != nullptr){
        buffer_unpin(table_id, pagenum);
        delete page;
    }
}

/**
 * read existing page
 */
AbstractPage::AbstractPage(int64_t p_table_id, pagenum_t p_pagenum){
    // read page
    auto* p_page = new page_t();
    buffer_read_page(p_table_id, p_pagenum, p_page);

    construct(p_table_id, p_pagenum, p_page);
}

/**
 * allocate new page then return
 */
AbstractPage::AbstractPage(int64_t p_table_id) {
    // allocate page
    pagenum_t p_pagenum = buffer_alloc_page(p_table_id);

    // read page
    page_t* p_page = new page_t();
    buffer_read_page(p_table_id, p_pagenum, p_page);

    construct(p_table_id, p_pagenum, p_page);
}

void AbstractPage::construct(int64_t p_table_id, pagenum_t p_pagenum, page_t* p_page){
    setTableId(p_table_id);
    setPagenum(p_pagenum);
    setPage(p_page);
}

page_t* AbstractPage::getPage(){
    return page;
}

void AbstractPage::setPage(page_t *p_page) {
    page = p_page;
}

int64_t AbstractPage::getTableId(){
    return table_id;
}

void AbstractPage::setTableId(int64_t p_table_id){
    table_id = p_table_id;
}

pagenum_t AbstractPage::getPagenum(){
    return pagenum;
}

void AbstractPage::setPagenum(pagenum_t p_pagenum){
    pagenum = p_pagenum;
}

void AbstractPage::save(){
    buffer_write_page(table_id, pagenum, page);
}

void AbstractPage::drop(){
    buffer_free_page(table_id, pagenum);
    
    delete page;
    page = nullptr;
}
