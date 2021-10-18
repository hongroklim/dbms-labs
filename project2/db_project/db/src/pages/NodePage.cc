#include "pages/NodePage.h"

NodePage::NodePage(int64_t p_table_id) : AbstractPage(p_table_id) {
    setNumberOfKeys(0);
}

void NodePage::deletePageRef(){
    setPage(nullptr);
}

bool NodePage::isLeaf(){
    int num;
    page_read_value(page, 8, &num, sizeof(int));
    return num == 1;
}

void NodePage::setLeaf(bool isLeaf){
    int num = isLeaf ? 1 : 0;
    page_write_value(page, 8, &num, sizeof(int));
}

pagenum_t NodePage::getParentPagenum(){
    pagenum_t num = 0;
    page_read_value(page, 0, &num, sizeof(pagenum_t));
    return num;
}

void NodePage::setParentPageNum(pagenum_t parentnum){
    page_write_value(page, 0, &parentnum, sizeof(pagenum_t));
}

uint NodePage::getNumberOfKeys(){
    uint num = 0;
    page_read_value(page, 12, &num, sizeof(uint));
    return num;
}

void NodePage::setNumberOfKeys(uint keyNum){
    page_write_value(page, 12, &keyNum, sizeof(uint));
}

int64_t NodePage::getLeftMostKey(){
    int64_t num;
    page_read_value(page, 128, &num, sizeof(int64_t));
    return num;
}

bool NodePage::isRearrangeRequired() {
    return FREE_SPACE_THRESHOLD <= getAmountOfFreeSpace();
}
