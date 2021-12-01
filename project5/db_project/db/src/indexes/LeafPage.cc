#include "indexes/LeafPage.h"

#include <vector>
#include <algorithm>
#include <iostream>

LeafPage::LeafPage(int64_t p_table_id) : NodePage(p_table_id) {
    setLeaf(true);

    // set amount of free space
    setAmountOfFreeSpace(4096 - 128);

    // set right sibling page number
    setRightPagenum(0);
}

LeafPage* LeafPage::convert(NodePage* nodePage){
    LeafPage* leafPage = new LeafPage(nodePage->getTableId(), nodePage->getPagenum(), nodePage->getPage());

    nodePage->deletePageRef();
    delete nodePage;

    return leafPage;
}

uint64_t LeafPage::getAmountOfFreeSpace(){
    uint64_t num;
    page_read_value(page, 112, &num, sizeof(uint64_t));
    return num;
}

void LeafPage::setAmountOfFreeSpace(uint64_t freeAmount){
    if(freeAmount > 4096){
        throw std::overflow_error("exceeded free amount "+ freeAmount);
    }
    page_write_value(page, 112, &freeAmount, sizeof(uint64_t));
}

pagenum_t LeafPage::getRightPagenum(){
    pagenum_t num;
    page_read_value(page, 120, &num, sizeof(pagenum_t));
    return num;
}

void LeafPage::setRightPagenum(pagenum_t p_pagenum){
    page_write_value(page, 120, &p_pagenum, sizeof(pagenum_t));
}

int64_t LeafPage::getKey(uint index){
    int64_t key;
    page_read_value(page, 128 + (index*12), &key, sizeof(int64_t));
    return key;
}

/*
 * check capacity of the number of keys and amount of free space
 */
bool LeafPage::isInsertAvailable(uint key_num, uint16_t total_val_size){
    uint totalKeyNum = getNumberOfKeys();
    uint16_t freeSpace = getAmountOfFreeSpace();
    return key_num + totalKeyNum <= 64 && (key_num * 12) + total_val_size <= freeSpace;
}

bool LeafPage::isInsertAvailable(uint16_t val_size){
    return isInsertAvailable(1, val_size);
}

slot LeafPage::getSlot(uint index){
    slot s{};
    s.index = index;

    s.key = getKey(index);
    page_read_value(page, 128 + (index * 12) + 8, &s.val_size, sizeof(uint16_t));
    page_read_value(page, 128 + (index * 12) + 10, &s.offset, sizeof(uint16_t));

    return s;
}

void LeafPage::setSlot(uint index, int64_t key, uint16_t val_size, uint16_t offset){
    //std::cout << pagenum << " " << index << " " << key << " " << val_size << " " << offset << std::endl;
    page_write_value(page, 128 + (index * 12) + 0, &key, sizeof(int64_t));
    page_write_value(page, 128 + (index * 12) + 8, &val_size, sizeof(uint16_t));
    page_write_value(page, 128 + (index * 12) + 10, &offset, sizeof(uint16_t));
}

void LeafPage::setSlot(slot s){
    setSlot(s.index, s.key, s.val_size, s.offset);
}

int LeafPage::getSlotIndex(int64_t key){
    int index, keyNum = getNumberOfKeys();

    for(index = 0; index < keyNum; index++)
        if(getKey(index) == key)
            return index;

    return -1;
}

bool LeafPage::isKeyExists(int64_t key){
    return getSlotIndex(key) > -1;
}

uint LeafPage::findInsertionIndex(int64_t key){
    uint index = 0, keyNum = getNumberOfKeys();

    while(index < keyNum && getKey(index) < key){
        index++;
    }

    return index;
}

uint LeafPage::findSplitIndex(uint insertionIndex, uint16_t val_size){
    uint index = 0, i = 0, keyNum = getNumberOfKeys();
    uint16_t valSize, leftSize = 0;

    // threshold is (4096 - 128) / 2; half of free space
    while(i < keyNum && leftSize < 1984){
        // assuming new value is inserted
        if(i == insertionIndex){
            leftSize += val_size;
            if(leftSize >= 1984) break;
            else index++;
        }

        // get value size then append
        page_read_value(page, 128 + (i * 12) + 8, &valSize, sizeof(uint16_t));
        leftSize += valSize;

        i++;        // increase one of loop
        index++;    // increase one of split (considering insertion)
    }

    return index;
}

/**
 * the page should be guaranteed enough space to insert
 */
void LeafPage::insert(int64_t key, char* value, uint16_t val_size){
    uint insertionIndex = findInsertionIndex(key);
    uint keyNum = getNumberOfKeys();

    // shift slots to right
    for(uint i = keyNum; i > insertionIndex; i--){
        page_move_value(page, 128+(i*12), 128+((i-1)*12), 12);
    }

    // calculate insertion offset then set slot
    uint16_t freeAmount = getAmountOfFreeSpace();
    uint16_t insertionOffset = 128 + (12 * keyNum) + freeAmount - val_size;

    setSlot(insertionIndex, key, val_size, insertionOffset);

    // set value
    page_write_value(page, insertionOffset, value, val_size);

    // modify header
    setNumberOfKeys(keyNum + 1);
    setAmountOfFreeSpace(freeAmount - val_size - 12);
}

/**
 * without considering value to be inserted.
 * It should be decided by upper layer whether splitIndex -1 or splitIndex.
 */
void LeafPage::split(uint splitIndex, LeafPage* newLeaf){
    uint keyNum = getNumberOfKeys();

    slot s{};
    char value[112];

    for(uint i=splitIndex; i<keyNum; i++){
        // read slot and value
        s = getSlot(splitIndex);
        readValue(s, value);

        // insert into new leaf
        newLeaf->insert(s.key, value, s.val_size);

        // delete
        del(s);
    }

    // TODO del_to_end(splitIndex, keyNum);

    // change right sibling page number
    newLeaf->setRightPagenum(getRightPagenum());
    setRightPagenum(newLeaf->getPagenum());
}

void LeafPage::update(int64_t key, char* values, uint16_t new_val_size, uint16_t* old_val_size){
    // find the matched slot
    slot s = getSlot(getSlotIndex(key));

    // set new value
    page_write_value(page, s.offset, values, new_val_size);

    // return old val size
    *old_val_size = s.val_size;

    // set new val size
    s.val_size = new_val_size;
    setSlot(s);
}

void LeafPage::del(slot s){
    uint keyNum = getNumberOfKeys();
    uint16_t freeAmount = getAmountOfFreeSpace();
    
    // remove value
    page_write_value(page, s.offset, nullptr, s.val_size);
    packValue(s);

    // remove slot
    page_write_value(page, 128 + (s.index * 12), nullptr, 12);
    packSlot(s.index);

    // modify header
    setNumberOfKeys(keyNum - 1);
    setAmountOfFreeSpace(freeAmount + s.val_size + 12);
}

void LeafPage::del(int64_t key){
    // find slot
    int slotIdx = getSlotIndex(key);
    if(slotIdx < 0){
        return;
    }

    slot s = getSlot(slotIdx);

    // then delete
    del(s);
}

void LeafPage::del(uint begin, uint end){
    // TODO bulk delete


    // setNumberOfKeys(getNumberOfKeys() - end + begin - 1);
}

void LeafPage::packSlot(uint removedIndex){
    uint keyNum = getNumberOfKeys();

    // shift slots to left
    for(uint i=removedIndex+1; i<keyNum; i++){
        page_move_value(page, 128+((i-1)*12), 128+(i*12), 12);
    }
}

bool compareSlotOffset(slot s1, slot s2){
    // greater offset ahead first
    return s1.offset > s2.offset;
}

void LeafPage::packValue(slot removedSlot){
    uint keyNum = getNumberOfKeys();

    slot s{};
    std::vector<slot> vSlot;
    for(int i=0; i<keyNum; i++){
        // take slots following removed slot
        s = getSlot(i);
        if(s.offset < removedSlot.offset){
            vSlot.push_back(s);
        }
    }

    // sort slots
    sort(vSlot.begin(), vSlot.end(), compareSlotOffset);

    for(auto & item : vSlot){
        // shift slots' offsets
        page_move_value(page, item.offset + removedSlot.val_size, item.offset, item.val_size);

        // change offsets
        item.offset += removedSlot.val_size;
        setSlot(item);
    }
}

void LeafPage::readValue(slot s, char* ret_val){
    page_read_value(page, s.offset, ret_val, s.val_size);
}

void LeafPage::readValue(int64_t key, char* ret_val, uint16_t* val_size){
    // find slot
    slot s = getSlot(getSlotIndex(key));

    // set value
    readValue(s, ret_val);

    // set offset
    *val_size = s.val_size;
}

bool LeafPage::isMergeAvailable(LeafPage* toBeMerged){
    uint16_t total_val_size = 4096 - 128 - toBeMerged->getAmountOfFreeSpace();
    return isInsertAvailable(toBeMerged->getNumberOfKeys(), total_val_size);
}

void LeafPage::absorbAll(LeafPage* victim, bool isAppend){
    uint keyNum = victim->getNumberOfKeys();

    slot s{};
    char* value = new char[112];

    for(uint i=0; i<keyNum; i++){
        // read slot and value from victim
        s = victim->getSlot(i);
        victim->readValue(s, value);

        // insert into this leaf
        insert(s.key, value, s.val_size);
    }

    delete []value;
}

/*
 * get values from right side to fill threshold
 */
void LeafPage::redistribute(LeafPage* srcLeaf, bool fromLeft){
    uint16_t freeSpace = getAmountOfFreeSpace();
    uint popIndex = fromLeft ? srcLeaf->getNumberOfKeys() - 1 : 0;

    slot s{};
    char* value = new char[112];

    while(freeSpace >= FREE_SPACE_THRESHOLD){
        // read from the source
        s = srcLeaf->getSlot(popIndex);
        srcLeaf->readValue(s, value);
        srcLeaf->del(s);

        if(fromLeft) popIndex--;

        // insert
        insert(s.key, value, s.val_size);

        // increase free space
        freeSpace -= 12 + s.val_size;
    }

    delete []value;
}

void LeafPage::print() {
    std::cout << getPagenum() << " :";
    slot s{};
    for(int i=0; i<getNumberOfKeys(); i++){
        s = getSlot(i);
        std::cout << " " << s.key;
    }
    std::cout << std::endl;
}
