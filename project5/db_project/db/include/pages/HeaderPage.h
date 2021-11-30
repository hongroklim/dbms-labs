#ifndef DB_HEADERPAGE_H
#define DB_HEADERPAGE_H

#include "pages/AbstractPage.h"

class HeaderPage : public AbstractPage {
public:
    explicit HeaderPage(int64_t p_table_id) : AbstractPage(p_table_id, 0) {};

    pagenum_t getFreePagenum();
    void setFreePagenum(pagenum_t freePagenum);

    pagenum_t getNumberOfPages();
    void setNumberOfPages(pagenum_t pNum);

    pagenum_t getRootPagenum();
    void setRootPagenum(pagenum_t rootPagenum);
};

#endif //DB_HEADERPAGE_H
