#ifndef DB_FDHOLDER_H
#define DB_FDHOLDER_H

#include <iostream>
#include <stdint.h>

class FdHolder {
private:
    const int SCALE_UNIT_CNT = 10;
    int alloc_cnt{};
    int next_table_id{};
    int fd_cnt{};

    const char ** pathname_list{};
    int64_t * table_id_list{};
    int * fd_list{};

    bool scale_capacity(bool is_force);

public:
    FdHolder(){
        alloc_cnt = 0;
    };

    bool is_initialized(){
        return alloc_cnt > 0;
    }

    void construct();
    void destroy();

    int64_t insert(const char * pathname, int fd);
    int64_t get_table_id(const char* pathname);
    int get_fd(int64_t table_id);

    int get_fd_cnt(){
        return fd_cnt;
    }

    int * get_fd_list(){
        return fd_list;
    }
};

#endif //DB_FDHOLDER_H
