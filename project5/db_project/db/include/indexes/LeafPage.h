#ifndef DB_LEAFPAGE_H
#define DB_LEAFPAGE_H

#include "indexes/NodePage.h"

struct slot {
    uint index;
    int64_t key;
    uint16_t val_size;
    uint16_t offset;
};

class LeafPage : public NodePage {
private:
    int64_t getKey(uint index) override;
    int getSlotIndex(int64_t key);

    slot getSlot(uint index);
    void setSlot(uint index, int64_t key, uint16_t val_size, uint16_t offset);
    void setSlot(slot s);

    void del(slot s);
    void del(uint begin, uint end);

    void packSlot(uint removedIndex);
    void packValue(slot removedSlot);

    void readValue(slot s, char* ret_val);

public:
    // Constructor
    LeafPage(int64_t p_table_id, pagenum_t p_pagenum, page_t* p_page) : NodePage(p_table_id, p_pagenum, p_page) {}
    LeafPage(int64_t p_table_id, pagenum_t p_pagenum) : NodePage(p_table_id, p_pagenum) {}
    explicit LeafPage(int64_t p_table_id);

    // Downcasting
    static LeafPage* convert(NodePage* nodePage);

    // Amount of Free Space
    uint64_t getAmountOfFreeSpace() override;
    void setAmountOfFreeSpace(uint64_t freeAmount);

    // Right Page Number
    pagenum_t getRightPagenum();
    void setRightPagenum(pagenum_t p_pagenum);

    bool isInsertAvailable(uint key_num, uint16_t total_val_size);
    bool isInsertAvailable(uint16_t val_size);

    bool isKeyExists(int64_t key);
    uint findInsertionIndex(int64_t key);
    uint findSplitIndex(uint insertionIndex, uint16_t val_size);

    void insert(int64_t key, char* value, uint16_t val_size);
    void split(uint splitIndex, LeafPage* newLeaf);

    void update(int64_t key, char* values, uint16_t new_val_size, uint16_t* old_val_size);

    void readValue(int64_t key, char* ret_val, uint16_t* val_size);

    void del(int64_t key) override;

    bool isMergeAvailable(LeafPage* toBeMerged);
    void absorbAll(LeafPage* victim, bool isAppend);
    void redistribute(LeafPage* srcLeaf, bool fromLeft);

    void print() override;
};

#endif //DB_LEAFPAGE_H
