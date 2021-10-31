#ifndef DB_AHEADERSOURCE_H
#define DB_AHEADERSOURCE_H

#include "pages/page.h"

class AHeaderSource {
protected:
    int64_t table_id{};
public:
    AHeaderSource() {}
    AHeaderSource(int64_t p_table_id);
    virtual ~AHeaderSource();

    virtual void read(page_t* dest) {}
    virtual void write(const page_t* src) {}
};

#endif //DB_AHEADERSOURCE_H