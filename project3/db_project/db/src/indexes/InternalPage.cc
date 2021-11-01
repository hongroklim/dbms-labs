#include "indexes/InternalPage.h"

#include <iostream>
#include <math.h>
#include <vector>
#include <algorithm>

InternalPage::InternalPage(int64_t p_table_id) : NodePage(p_table_id) {
    // set is leaf to 0
    setLeaf(false);

    setNumberOfKeys(0);
}

/**
 * create new internal page and delete existing nodePage.
 * page_t will be transferred to new one safely.
 */
InternalPage* InternalPage::convert(NodePage* nodePage){
    InternalPage* internalPage = new InternalPage(nodePage->getTableId(), nodePage->getPagenum(), nodePage->getPage());

    nodePage->deletePageRef();
    delete nodePage;

    return internalPage;
}

/**
 * If not exists, return negative value
 */
int InternalPage::getKeyIndex(int64_t key){
    int index = -1;
    uint keyNum = getNumberOfKeys();

    while(index < keyNum && getKey(index) < key)
        index++;

    return index;
}

int InternalPage::getMatchedKeyIndex(int64_t key){
    uint keyNum = getNumberOfKeys();
    int64_t tmpKey;

    for(int index = 0; index<keyNum; index++){
        tmpKey = getKey(index);

        if(tmpKey == key){
            return index;

        }else if(tmpKey > key){
            std::cout << pagenum << " failed to delete " << key << std::endl;
            break;
        }
    }

    return -1;
}

int64_t InternalPage::getKey(uint index){
    int64_t key;
    page_read_value(page, 128 + (index*16), &key, sizeof(int64_t));
    return key;
}

void InternalPage::del_to_end(uint beginIndex){
    if(beginIndex == 0){
        // remove leftmost pagenum
        page_write_value(page, 120, nullptr, 8);
    }
    
    // remove key and page number
    page_write_value(page, 128 + (16 * beginIndex), nullptr, 16 * (getNumberOfKeys() - beginIndex));

    setNumberOfKeys(beginIndex);
}

/**
 * parameter value should be exactly same with one of existing.
 */
uint InternalPage::getPagenumIndex(pagenum_t p_pagenum){
    pagenum_t numPage = getNumberOfKeys() + 1;

    for(uint i = 0; i < numPage; i++){
        if(getNodePagenumByIndex(i) == p_pagenum) return i;
    }

    std::cout << "failed to find " << p_pagenum << std::endl;
    return 0;
}

pagenum_t InternalPage::getNodePagenumByIndex(uint index){
    pagenum_t num;
    page_read_value(page, 120 + (16 * index), &num, sizeof(pagenum_t));
    return num;
}

void InternalPage::setKeyAndPagenum(uint index, int64_t key, pagenum_t p_pagenum){
    page_write_value(page, 128 + (16 * index), &key, sizeof(int64_t));
    page_write_value(page, 136 + (16 * index), &p_pagenum, sizeof(pagenum_t));
}

bool InternalPage::isInsertAvailable(uint key_num){
    uint totalNum = getNumberOfKeys();
    return key_num + totalNum <= 248 - 1;
}

bool InternalPage::isInsertAvailable(){
    return isInsertAvailable(1);
}

void InternalPage::insert_init(pagenum_t l_pagenum, int64_t key, pagenum_t r_pagenum){
    page_write_value(page, 120, &l_pagenum, sizeof(pagenum_t)); // left page number
    page_write_value(page, 128, &key, sizeof(int64_t));         // key
    page_write_value(page, 136, &r_pagenum, sizeof(pagenum_t)); // right page number

    setNumberOfKeys(1);
}

void InternalPage::insert(int64_t key, pagenum_t r_pagenum){
    // get destination of index
    uint insertionIndex = findInsertionIndex(key), keyNum = getNumberOfKeys();
    // note. insertion index is next to existing key

    for(uint keyIndex = keyNum; keyIndex > insertionIndex; keyIndex--){
        // shift key and pagenum to right
        page_move_value(page, 128 + (16 * keyIndex), 128 + (16 * (keyIndex - 1)), 16);
    }

    // write key and pagenum
    setKeyAndPagenum(insertionIndex, key, r_pagenum);

    // set child node's parent page number
    NodePage childNode(getTableId(), r_pagenum);
    childNode.setParentPageNum(getPagenum());
    childNode.save();

    // increase the number of keys
    setNumberOfKeys(keyNum + 1);
}

uint64_t InternalPage::getAmountOfFreeSpace() {
    return 4096 - 128 - (getNumberOfKeys() * 16);
}

pagenum_t InternalPage::getNodePagenumExact(int64_t key){
    int index = 0;
    uint keyNum = getNumberOfKeys();

    if(key < getKey(0)){
        return getNodePagenumByIndex(0);
    }

    while(index < keyNum && getKey(index) <= key)
        index++;

    return getNodePagenumByIndex(index);
}

/**
 * 0 [0 1] [1 2] ...
 */
pagenum_t InternalPage::getNodePagenum(int64_t key){
    int index = 0;
    uint keyNum = getNumberOfKeys();

    while(index < keyNum && getKey(index) < key)
        index++;

    return getNodePagenumByIndex(index);
}

uint InternalPage::findInsertionIndex(int64_t key){
    int index = 0;
    uint keyNum = getNumberOfKeys();

    while(index < keyNum && getKey(index) < key)
        index++;

    return index;
}

uint InternalPage::findSplitIndex(uint insertionIndex){
    uint index = 0, i = 0, keyNum = getNumberOfKeys();

    // threshold is (4096 - 128) / 2; half of free space
    while(i < keyNum && index <= 124){
        // assuming new value is inserted
        if(i == insertionIndex){
            if(index > 124) break;
            else index++;
        }

        i++;        // increase one of loop
        index++;    // increase one of split (considering insertion)
    }

    return index;
}

void InternalPage::split(uint splitIndex, InternalPage* newNode){
    uint keyNum = getNumberOfKeys();
    int64_t key;
    pagenum_t p_pagenum;

    for(uint i=splitIndex; i<keyNum; i++){
        // read slot and value
        page_read_value(page, 128 + (16 * i), &key, sizeof(int64_t));
        page_read_value(page, 136 + (16 * i), &p_pagenum, sizeof(pagenum_t));

        // insert into new leaf
        newNode->insert(key, p_pagenum);
    }

    // bulk delete
    del_to_end(splitIndex);
}

void InternalPage::del(int64_t key) {
    int index = getMatchedKeyIndex(key);
    if(index == -1){
        return;
    }

    uint keyNum = getNumberOfKeys();
    page_move_value(page, 128 + 16*index, 128 + 16*(index+1), 16*(keyNum - index - 1));

    setNumberOfKeys(keyNum - 1);
}

void InternalPage::alterKey(int64_t oldKey, int64_t newKey){
    uint index = getMatchedKeyIndex(oldKey);
    page_write_value(page, 128 + (16 * index), &newKey, sizeof(int64_t));
}

bool InternalPage::isMergeAvailable(InternalPage* toBeMerged){
    // considering the new key of leftmost pagenum
    return isInsertAvailable(toBeMerged->getNumberOfKeys() + 1);
}

void InternalPage::changeChildParentPagenum(pagenum_t child_pagenum){
    // change the moved one's parent
    NodePage* childNode;
    childNode = new NodePage(getTableId(), child_pagenum);
    childNode->setParentPageNum(getPagenum());
    childNode->save();
    delete childNode;
}

// move entire elements from victim into this
void InternalPage::absorbAll(InternalPage* victim, int64_t hiddenKey, bool fromLeft){
    int64_t key;
    pagenum_t movePagenum;

    uint keyNum = getNumberOfKeys();
    uint victimNum = victim->getNumberOfKeys();
    
    if(fromLeft){
        // insert ahead
        page_move_value(page, 120+((victimNum+1)*16), 120, keyNum*16 + 8);   // shift first

        // insert the leftmost pagenum
        movePagenum = victim->getNodePagenumByIndex(0);
        page_write_value(page, 120, &movePagenum, sizeof(pagenum_t));
        changeChildParentPagenum(movePagenum);

        // insert remains
        for(uint i=0; i<victimNum; i++){
            key = victim->getKey(i);
            movePagenum = victim->getNodePagenumByIndex(i+1);
            setKeyAndPagenum(i, key, movePagenum);
            changeChildParentPagenum(movePagenum);
        }

        // insert hidden key
        page_write_value(page, 120+((victimNum+1)*16)-8, &hiddenKey, sizeof(int64_t));

    }else{
        // insert behind
        movePagenum = victim->getNodePagenumByIndex(0);
        setKeyAndPagenum(keyNum, hiddenKey, movePagenum);
        changeChildParentPagenum(movePagenum);

        // insert remains
        for(uint i=0; i<victimNum; i++){
            key = victim->getKey(i);
            movePagenum = victim->getNodePagenumByIndex(i+1);
            setKeyAndPagenum(keyNum + i+1, key, movePagenum);
            changeChildParentPagenum(movePagenum);
        }
    }
    
    // sum of keys, plus hidden key (derived from the parent)
    setNumberOfKeys(keyNum + victimNum + 1);
    victim->setNumberOfKeys(0);
}

// get a single element from neighbor
void InternalPage::absorbOne(InternalPage* neighbor, int64_t hiddenKey, bool fromLeft){
    pagenum_t movePagenum;
    if(fromLeft){
        // insert ahead
        page_move_value(page, 120+16, 120, getNumberOfKeys()*16 + 8);   // shift first
        
        movePagenum = neighbor->getNodePagenumByIndex(neighbor->getNumberOfKeys());
        page_write_value(page, 120, &movePagenum, sizeof(pagenum_t));
        page_write_value(page, 128, &hiddenKey, sizeof(int64_t));
        neighbor->delRightmostPage();

    }else{
        // insert behind
        movePagenum = neighbor->getNodePagenumByIndex(0);
        setKeyAndPagenum(getNumberOfKeys(), hiddenKey, movePagenum);
        neighbor->delLeftmostPage();
    }
    
    // change child's parent pagenum
    changeChildParentPagenum(movePagenum);

    // change the number of keys
    setNumberOfKeys(getNumberOfKeys()+1);
}

void InternalPage::print() {
    uint keyNum = getNumberOfKeys();
    std::cout << getPagenum() << " : "<< getNodePagenumByIndex(0);
    for(int i=0; i<keyNum; i++){
        std::cout << " (" << getKey(i) << ") ";
        std::cout << getNodePagenumByIndex(i+1);
    }
    std::cout << std::endl;
}

void InternalPage::delLeftmostPage(){
    uint keyNum = getNumberOfKeys();
    page_move_value(page, 120, 128 + 8, getNumberOfKeys() * 16 - 8);
    setNumberOfKeys(keyNum-1);
}

void InternalPage::delRightmostPage() {
    uint keyNum = getNumberOfKeys();
    page_write_value(page, 128 + (keyNum*16), nullptr, 16);
    setNumberOfKeys(keyNum-1);
}
