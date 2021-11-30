#ifndef DB_BUFFERHEADERSOURCE_H
#define DB_BUFFERHEADERSOURCE_H

#include "headers/AHeaderSource.h"
#include "pages/page.h"
#include "buffers/buffer.h"

class BufferHeaderSource : public AHeaderSource {
private:
    page_t* headerPage;

public:
    BufferHeaderSource(int64_t p_table_id, page_t* page);

    void read(page_t* dest) override;
    void write(const page_t* src) override;
};

#endif //DB_BUFFERHEADERSOURCE_H