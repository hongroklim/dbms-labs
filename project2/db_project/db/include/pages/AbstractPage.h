#ifndef DB_ABSTRACTPAGE_H
#define DB_ABSTRACTPAGE_H

#include <page.h>

class AbstractPage {
private:
    void construct(int64_t p_table_id, pagenum_t p_pagenum, page_t* p_page);

protected:
    page_t* page{};
    int64_t table_id{};
    pagenum_t pagenum{};

    void setPage(page_t* p_page);
    void setTableId(int64_t p_table_id);
    void setPagenum(pagenum_t p_pagenum);

public:
    AbstractPage(){
        page = new page_t();
    };

    virtual ~AbstractPage(){
        delete page;
    }

    AbstractPage(int64_t p_table_id, pagenum_t p_pagenum, page_t* p_page);
    AbstractPage(int64_t p_table_id, pagenum_t p_pagenum);
    explicit AbstractPage(int64_t p_table_id);

    page_t* getPage();
    int64_t getTableId();
    pagenum_t getPagenum();

    void save();
    void drop();
};
#endif //DB_ABSTRACTPAGE_H
