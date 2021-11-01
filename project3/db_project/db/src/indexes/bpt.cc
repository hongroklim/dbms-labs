#include "indexes/bpt.h"

#include <set>
#include <string>
#include <iostream>

#include "buffers/buffer.h"
#include "pages/HeaderPage.h"
#include "indexes/InternalPage.h"
#include "indexes/LeafPage.h"

std::set<std::string> tbl_list;

bool table_is_initialized(int64_t table_id);

void table_init(int64_t table_id);

LeafPage* node_find_candidate_leaf(int64_t table_id, int64_t key, bool isExact);

pagenum_t node_find_leaf(int64_t table_id, int64_t key);

void node_insert_parent(NodePage* left, int64_t key, NodePage* right);

void leaf_split_insert(LeafPage* srcLeaf, LeafPage* newLeaf, int64_t key, char* value, uint16_t val_size);

int64_t internal_split_insert(InternalPage* srcNode, InternalPage* newNode, int64_t key, pagenum_t r_pagenum);

void node_delete_key(int64_t table_id, pagenum_t pagenum, int64_t key);

void node_reorganize(NodePage* nodePage);

void leaf_reorganize(LeafPage* node, LeafPage* sibling, int64_t nodeKey, int64_t siblingKey, bool leftMostNode);

void internal_reorganize(InternalPage* node, InternalPage* sibling, int64_t nodeKey, int64_t siblingKey, bool leftMostNode);

void internal_alter_key(int64_t table_id, pagenum_t pagenum, int64_t oldKey, int64_t newKey);

void internal_delete_leftmost(int64_t table_id, pagenum_t pagenum);

/*
 * Initialize your database management system.
 * Initialize and allocate anything you need.
 * The total number of tables is less than 20.
 * If success, return 0. Otherwise, return non-zero value.
 */
int init_db(int num_buf){
    return buffer_init(num_buf);
}

/*
 * Open existing data file using ‘pathname’ or create one if not existed.
 * If success, return the unique table id, which represents the own table in this database.
 * Otherwise, return negative value.
 */
int64_t open_table(char* pathname){
    if(tbl_list.size() >= MAX_TABLE_COUNT){
        std::cout << "the number of table is exceeded " << MAX_TABLE_COUNT << std::endl;
        return -1;
    }

    // get table_id
    int64_t table_id = buffer_open_table_file(pathname);

    // initialize if it is not
    if(!table_is_initialized(table_id)){
        table_init(table_id);
    }

    tbl_list.insert(pathname);
    return table_id;
}

bool table_is_initialized(int64_t table_id){
    // read header page
    HeaderPage headerPage(table_id);
    return headerPage.getRootPagenum() != 0;
}

void table_init(int64_t table_id){
    // allocate page and write
    LeafPage* leafPage = new LeafPage(table_id);
    leafPage->save();

    // modify root page number of header page
    HeaderPage headerPage(table_id);
    headerPage.setRootPagenum(leafPage->getPagenum());
    headerPage.save();

    delete leafPage;
}

/*
 * Insert input ‘key/value’ (record) with its size to data file at the right place.
 * If success, return 0. Otherwise, return non-zero value.
 */
int db_insert(int64_t table_id, int64_t key, char* value, uint16_t val_size){
    // validate size of value
    if(val_size < 50 || val_size > 112){
        std::cout << "value size must be between 50 and 112, but " << val_size << std::endl;
        return -1;
    }

    // find first. if duplicated, return -1
    char* tmpVal = new char[112];
    uint16_t tmpValSize;
    if(db_find(table_id, key, tmpVal, &tmpValSize) == 0){
        std::cout << "duplication is not allowed for " << key << std::endl;
        delete []tmpVal;
        return -2;
    }
    delete []tmpVal;

    int returnVal = 0;
    LeafPage* leafPage;
    try{
        // find candidate leaf node
        leafPage = node_find_candidate_leaf(table_id, key, false);
        if(leafPage == nullptr){
            throw std::runtime_error("failed to find candidate leaf");
        }

        // if there's enough room, insert
        if (leafPage->isInsertAvailable(val_size)) {
            leafPage->insert(key, value, val_size);
            leafPage->save();

        } else {
            // else, split then insert

            // create then set parent of new one
            LeafPage* newLeaf = new LeafPage(leafPage->getTableId());
            newLeaf->setParentPageNum(leafPage->getParentPagenum());

            leaf_split_insert(leafPage, newLeaf, key, value, val_size);

            node_insert_parent(leafPage, newLeaf->getLeftMostKey(), newLeaf);

            delete newLeaf;
        }

    }catch(std::exception& e){
        std::cout << e.what() << std::endl;
        returnVal = -4;
    }

    delete leafPage;    // the only delete leaf in all procedures
    return returnVal;
}

/**
 * split leaf then return new leaf.
 * new leaf's parent is same with source's one
 */
void leaf_split_insert(LeafPage* srcLeaf, LeafPage* newLeaf, int64_t key, char* value, uint16_t val_size){
    // find insertion index and split index
    uint insertionIdx = srcLeaf->findInsertionIndex(key);
    uint splitIndex = srcLeaf->findSplitIndex(insertionIdx, val_size);

    if(insertionIdx < splitIndex){
        // split-1 then insert into source leaf
        srcLeaf->split(splitIndex - 1, newLeaf);
        srcLeaf->insert(key, value, val_size);

    }else{
        // split then insert into new keaf
        srcLeaf->split(splitIndex, newLeaf);
        newLeaf->insert(key, value, val_size);
    }

    srcLeaf->save();
    newLeaf->save();
}

void node_insert_parent(NodePage* left, int64_t key, NodePage* right){
    pagenum_t parentPagenum = left->getParentPagenum();

    if(parentPagenum == 0){
        // create new internal node
        InternalPage* newRootNode = new InternalPage(left->getTableId());
        newRootNode->setParentPageNum(parentPagenum);

        // modify root page number of header page
        HeaderPage headerPage(newRootNode->getTableId());
        headerPage.setRootPagenum(newRootNode->getPagenum());
        headerPage.save();

        // insert left pagenum, key and right pagenum
        newRootNode->insert_init(left->getPagenum(), key, right->getPagenum());
        
        // set children's parent pagenum
        left->setParentPageNum(newRootNode->getPagenum());
        right->setParentPageNum(newRootNode->getPagenum());
        left->save();
        right->save();

        newRootNode->save();
        delete newRootNode;
        return;
    }

    // parent is exist
    InternalPage* parentNode = new InternalPage(left->getTableId(), parentPagenum);

    if(parentNode->isInsertAvailable()){
        // simple case : enough room
        parentNode->insert(key, right->getPagenum());

        right->setParentPageNum(parentNode->getPagenum());
        right->save();
    }else{
        // hard case : split a node

        // create new internal node and set parent pagenum
        InternalPage* newParentNode = new InternalPage(parentNode->getTableId());
        newParentNode->setParentPageNum(parentNode->getParentPagenum());

        // split then insert
        int64_t newParentKey = internal_split_insert(parentNode, newParentNode, key, right->getPagenum());

        // insert into parent
        node_insert_parent(parentNode, newParentKey, newParentNode);

        newParentNode->save();
        delete newParentNode;
    }

    parentNode->save();
    delete parentNode;
}

int64_t internal_split_insert(InternalPage* srcNode, InternalPage* newNode, int64_t key, pagenum_t r_pagenum){
    uint insertionIdx = srcNode->findInsertionIndex(key);
    uint splitIndex = srcNode->findSplitIndex(insertionIdx);

    // keep key reference
    int64_t splitKey;

    if(insertionIdx < splitIndex){
        // split-1 then insert into exist
        splitKey = srcNode->getKey(splitIndex-1);
        srcNode->split(splitIndex - 1, newNode);
        srcNode->insert(key, r_pagenum);

    }else{
        // split then insert into new one
        splitKey = srcNode->getKey(splitIndex);
        srcNode->split(splitIndex, newNode);
        newNode->insert(key, r_pagenum);

        if(insertionIdx == 0){

        }
    }

    // delete leftmost
    newNode->delLeftmostPage();

    return insertionIdx >= splitIndex && key < splitKey ? key : splitKey;
}

/*
 * Find the record containing input ‘key’.
 * If found matching ‘key’, store matched ‘value’ string in ret_val and matched ‘size’ in val_size.
 * If success, return 0. Otherwise, return non-zero value.
 */
int db_find(int64_t table_id, int64_t key, char* ret_val, uint16_t* val_size){
    // find candidate leaf node
    pagenum_t pagenum = node_find_leaf(table_id, key);
    if(pagenum == 0){
        return -3;
    }

    // read leaf and value
    LeafPage* leafPage = new LeafPage(table_id, pagenum);
    leafPage->readValue(key, ret_val, val_size);

    delete leafPage;
    return 0;
}

LeafPage* node_find_candidate_leaf(int64_t table_id, int64_t key, bool isExact){
    // starts with root page
    HeaderPage headerPage(table_id);
    pagenum_t pagenum = headerPage.getRootPagenum();

    NodePage* nodePage;
    LeafPage* leafPage = nullptr;
    InternalPage* internalPage;

    while(true){
        // break if it reaches the leaf
        nodePage = new NodePage(table_id, pagenum);
        if(nodePage->isLeaf()){
            // encounter leaf
            leafPage = LeafPage::convert(nodePage);
            //leafPage->print();
            break;
        }

        // convert internal node
        internalPage = InternalPage::convert(nodePage);
        //internalPage->print();

        // get next pagenum
        if(key < internalPage->getLeftMostKey() || !isExact){
            pagenum = internalPage->getNodePagenum(key);
        }else{
            pagenum = internalPage->getNodePagenumExact(key);
        }

        // failed to find leaf
        if(pagenum == 0){
            std::cout << "failed to find a candidate" << std::endl;
            leafPage = nullptr;
            break;

        }else if(internalPage->getPagenum() == pagenum){
            std::cout << "infinite loop is detected " << pagenum << std::endl;
            leafPage = nullptr;
            break;
        }

        delete internalPage;
    }
    
    return leafPage;
}

pagenum_t node_find_leaf(int64_t table_id, int64_t key){
    LeafPage* leafPage = node_find_candidate_leaf(table_id, key, true);

    pagenum_t pnum = leafPage != nullptr && leafPage->isKeyExists(key) ? leafPage->getPagenum() : 0;
    delete leafPage;

    return pnum;
}

/*
 * Find the matching record and delete it if found.
 * If success, return 0. Otherwise, return non-zero value.
 */
int db_delete(int64_t table_id, int64_t key){
    // find candidate leaf node
    pagenum_t pagenum = node_find_leaf(table_id, key);
    if(pagenum == 0){
        // if not exists, return failure
        return -3;
    }

    int retVal = 0;
    try{
        node_delete_key(table_id, pagenum, key);
    }catch(std::exception &e){
        std::cout << e.what() << std::endl;
        retVal = -2;
    }

    return retVal;
}

void node_delete_key(int64_t table_id, pagenum_t pagenum, int64_t key){
    NodePage* nodePage = new NodePage(table_id, pagenum);

    // in order to implement virtual methods
    if(nodePage->isLeaf()){
        nodePage = LeafPage::convert(nodePage);
    }else{
        nodePage = InternalPage::convert(nodePage);
    }

    // delete entry
    nodePage->del(key);
    nodePage->save();

    node_reorganize(nodePage);

    if(nodePage->getPagenum() != 0){
        // if nodePage is deleted since it has no element,
        // empty instance will be returned
        delete nodePage;
    }
}

void node_reorganize(NodePage* nodePage){
    if(nodePage->getParentPagenum() == 0 && !nodePage->isLeaf() && nodePage->getNumberOfKeys() == 0){
        std::cout << "pull up the only node under " << nodePage->getPagenum() << std::endl;
        // instead of convert
        InternalPage* oldRootNode = InternalPage::convert(nodePage);

        // move up child to node
        HeaderPage headerPage(oldRootNode->getTableId());
        headerPage.setRootPagenum(oldRootNode->getNodePagenumByIndex(0));
        headerPage.save();

        // change parent pagenum
        NodePage newRootNode(headerPage.getTableId(), headerPage.getRootPagenum());
        newRootNode.setParentPageNum(0);
        newRootNode.save();

        // set empty
        nodePage = new NodePage();
        return;
    }

    // if root node or no need to change
    if(nodePage->getParentPagenum() == 0 || !nodePage->isRearrangeRequired()){
        return;
    }

    // get parent of node and find its index
    InternalPage* parentPage = new InternalPage(nodePage->getTableId(), nodePage->getParentPagenum());
    uint nodeIndex = parentPage->getPagenumIndex(nodePage->getPagenum());
    int16_t nodeKey = nodeIndex == 0 ? 0 : parentPage->getKey(nodeIndex-1);

    // calculate sibling index, pagenum
    uint siblingIndex = nodeIndex == 0 ? 1 : nodeIndex - 1;
    pagenum_t siblingPagenum = parentPage->getNodePagenumByIndex(siblingIndex);
    int16_t siblingKey = parentPage->getKey(siblingIndex-1);

    if(!nodePage->isLeaf())
        parentPage->print();
    
    delete parentPage;

    if(nodePage->isLeaf()){
        // reorganize leaf page
        LeafPage* siblingPage = new LeafPage(nodePage->getTableId(), siblingPagenum);
        leaf_reorganize((LeafPage*)nodePage, siblingPage, nodeKey, siblingKey, nodeIndex == 0);
        delete siblingPage;

    }else{
        // reorganize internal page
        InternalPage* siblingPage = new InternalPage(nodePage->getTableId(), siblingPagenum);
        internal_reorganize((InternalPage*)nodePage, siblingPage, nodeKey, siblingKey, nodeIndex == 0);
        delete siblingPage;
    }
}

void leaf_reorganize(LeafPage* node, LeafPage* sibling, int64_t nodeKey, int64_t siblingKey, bool leftMostNode){
    std::cout << "reorg leaves " << node->getPagenum() << "(" << nodeKey << ")" << " & " << sibling->getPagenum() << "(" << siblingKey << ") ";

    if(sibling->isMergeAvailable(node)){
        // absorb all values
        std::cout << "absorb (all)" << std::endl;
        sibling->absorbAll(node, !leftMostNode);
        sibling->save();

        pagenum_t rightmost = node->getRightPagenum();
        node->drop();

        // modify outside nodes
        if(leftMostNode){
            // TODO change node's neighbor's right pagenum to sibling
            internal_delete_leftmost(sibling->getTableId(), sibling->getParentPagenum());
        }else{
            sibling->setRightPagenum(rightmost);
            node_delete_key(sibling->getTableId(), sibling->getParentPagenum(), nodeKey);
        }

    }else{
        // redistribute
        std::cout << "redistribute (one)" << std::endl;
        node->redistribute(sibling, !leftMostNode);

        node->save();
        sibling->save();

        if(leftMostNode){
            // when sibling is right side
            internal_alter_key(sibling->getTableId(), sibling->getParentPagenum(), siblingKey, sibling->getLeftMostKey());
        }else{
            // when node is right side
            internal_alter_key(node->getTableId(), node->getParentPagenum(), nodeKey, node->getLeftMostKey());
        }
    }
}

void internal_reorganize(InternalPage* node, InternalPage* sibling, int64_t nodeKey, int64_t siblingKey, bool leftMostNode){
    std::cout << "reorg internals " << node->getPagenum() << "(" << nodeKey << ")" << " & " << sibling->getPagenum() << "(" << siblingKey << ") ";
    //node->print();
    //sibling->print();
    if(sibling->isMergeAvailable(node)){
        // absorb all values
        std::cout << "absorb (all)" << std::endl;

        int64_t hiddenKey = leftMostNode ? siblingKey : nodeKey;

        sibling->absorbAll(node, hiddenKey, leftMostNode);
        sibling->save();

        //node->print();
        //sibling->print();

        node->drop();

        // modify outside nodes
        if(leftMostNode){
            internal_delete_leftmost(sibling->getTableId(), sibling->getParentPagenum());
        }else{
            node_delete_key(sibling->getTableId(), sibling->getParentPagenum(), nodeKey);
        }

    }else{
        // redistribute
        std::cout << "redistribute (one)" << std::endl;

        // keep the original values
        int64_t hiddenKey = leftMostNode ? siblingKey : nodeKey;
        int64_t tobeKey = leftMostNode ? sibling->getKey(0) : sibling->getKey(sibling->getNumberOfKeys()-1);
        
        node->absorbOne(sibling, hiddenKey, !leftMostNode);

        //node->print();
        //sibling->print();

        sibling->save();
        node->save();

        // change parent's key value
        internal_alter_key(node->getTableId(), node->getParentPagenum(), hiddenKey, tobeKey);
    }
}

void internal_alter_key(int64_t table_id, pagenum_t pagenum, int64_t oldKey, int64_t newKey){
    InternalPage internalPage(table_id, pagenum);

    internalPage.alterKey(oldKey, newKey);
    internalPage.save();
}

void internal_delete_leftmost(int64_t table_id, pagenum_t pagenum){
    InternalPage* internalPage = new InternalPage(table_id, pagenum);

    // it will delete the leftmost pagenum and key.
    internalPage->delLeftmostPage();
    internalPage->save();

    // reorganize
    node_reorganize(internalPage);

    delete internalPage;
}

/*
 * Shutdown your database management system.
 * Clean up everything.
 * If success, return 0. Otherwise, return non-zero value.
 */
int shutdown_db(){
    // TODO free pathname list
    tbl_list.clear();

    if(buffer_shutdown() != 0){
        return -2;
    }

    try{
        buffer_close_table_files();
        return 0;

    }catch(...){
        return -1;
    }
}
