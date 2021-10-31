#include "pages/HeaderPage.h"

pagenum_t HeaderPage::getFreePagenum(){
    pagenum_t num = 0;
    page_read_value(page, 0, &num, sizeof(pagenum_t));
    return num;
}

void HeaderPage::setFreePagenum(pagenum_t freePagenum){
    page_write_value(page, 0, &freePagenum, sizeof(pagenum_t));
}

pagenum_t HeaderPage::getNumberOfPages(){
    pagenum_t num = 0;
    page_read_value(page, 1*sizeof(pagenum_t), &num, sizeof(pagenum_t));
    return num;
}

void HeaderPage::setNumberOfPages(pagenum_t pNum){
    page_write_value(page, 1*sizeof(pagenum_t), &pNum, sizeof(pagenum_t));
}

pagenum_t HeaderPage::getRootPagenum(){
    pagenum_t num = 0;
    page_read_value(page, 2*sizeof(pagenum_t), &num, sizeof(pagenum_t));
    return num;
}

void HeaderPage::setRootPagenum(pagenum_t rootPagenum){
    page_write_value(page, 2*sizeof(pagenum_t), &rootPagenum, sizeof(pagenum_t));
}