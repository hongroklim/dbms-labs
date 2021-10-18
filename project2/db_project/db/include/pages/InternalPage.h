#ifndef DB_INTERNALPAGE_H
#define DB_INTERNALPAGE_H

#include "pages/NodePage.h"

class InternalPage : public NodePage {
private:
    int getKeyIndex(int64_t key);
    int getMatchedKeyIndex(int64_t key);

    void setKeyAndPagenum(uint index, int64_t key, pagenum_t p_pagenum);

    void del_to_end(uint beginIndex);
    void concat(InternalPage* victim, uint keyNum, bool isAppend);

public:
    // Constructor
    InternalPage(int64_t p_table_id, pagenum_t p_pagenum, page_t* p_page) : NodePage(p_table_id, p_pagenum, p_page) {}
    InternalPage(int64_t p_table_id, pagenum_t p_pagenum) : NodePage(p_table_id, p_pagenum) {}
    explicit InternalPage(int64_t p_table_id);

    // Downcasting
    static InternalPage* convert(NodePage* nodePage);

    bool isInsertAvailable(uint key_num);
    bool isInsertAvailable();

    // Getter
    uint64_t getAmountOfFreeSpace() override;
    pagenum_t getNodePagenumExact(int64_t key);
    pagenum_t getNodePagenum(int64_t key);
    uint getPagenumIndex(pagenum_t p_pagenum);
    int64_t getKey(uint index) override;
    pagenum_t getNodePagenumByIndex(uint index);

    // split insertion
    uint findInsertionIndex(int64_t key);
    uint findSplitIndex(uint insertionIndex);

    // insertion
    void insert_init(pagenum_t l_pagenum, int64_t key, pagenum_t r_pagenum);
    void insert(int64_t key, pagenum_t r_pagenum);

    void split(uint splitIndex, InternalPage* newNode);

    void del(int64_t key) override;
    void alterKey(int64_t oldKey, int64_t newKey);
    void delLeftmostPage();

    bool isMergeAvailable(InternalPage* toBeMerged);
    void absorbAll(InternalPage* victim, bool isAppend);
    void redistribute(InternalPage* srcInternal, bool fromLeft);

    void print() override;
};

#endif //DB_INTERNALPAGE_H
