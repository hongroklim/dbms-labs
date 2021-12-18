#ifndef DB_FILEHEADERSOURCE_H
#define DB_FILEHEADERSOURCE_H

#include "headers/AHeaderSource.h"
#include "pages/page.h"

class FileHeaderSource : public AHeaderSource {
public:
    FileHeaderSource(int64_t p_table_id) : AHeaderSource(p_table_id) {};
    
    void read(page_t* dest) override;
    void write(const page_t* src) override;
};

#endif //DB_FILEHEADERSOURCE_H