#ifndef DB_VNODEPAGE_H
#define DB_VNODEPAGE_H

typedef unsigned int uint;

#include "pages/AbstractPage.h"

class NodePage : public AbstractPage {
protected:
    const uint64_t FREE_SPACE_THRESHOLD = 2500;
    virtual int64_t getKey(uint index) { return 0; }

    void setLeaf(bool isLeaf);
    void setNumberOfKeys(uint keyNum);
    
public:
    // Constructor
    NodePage(int64_t p_table_id, pagenum_t p_pagenum, page_t* p_page) : AbstractPage(p_table_id, p_pagenum, p_page) {}
    NodePage(int64_t p_table_id, pagenum_t p_pagenum) : AbstractPage(p_table_id, p_pagenum) {}
    explicit NodePage(int64_t p_table_id);
    NodePage() = default;

    // Setter
    void setParentPageNum(pagenum_t parentnum);

    // Getter
    pagenum_t getParentPagenum();
    uint getNumberOfKeys();
    bool isLeaf();
    int64_t getLeftMostKey();

    // Method
    bool isRearrangeRequired();
    void deletePageRef();

    virtual uint64_t getAmountOfFreeSpace() { return 0; }
    virtual void del(int64_t key) {}
    virtual void print() {}
    // TODO bind slot and pagenum into single generic
};

#endif //DB_VNODEPAGE_H
