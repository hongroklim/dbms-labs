#include "files/FdHolder.h"

bool FdHolder::scale_capacity(bool is_force){
    if(!is_force && fd_cnt < alloc_cnt){
        return false;
    }

    alloc_cnt += SCALE_UNIT_CNT;
    std::cout << "increase capacity of FdHolder to " << alloc_cnt << std::endl;

    // TODO backup pointers before realloc
    pathname_list = (const char**)realloc(pathname_list, sizeof(const char*) * alloc_cnt);
    fd_list = (int*)realloc(fd_list, sizeof(int) * alloc_cnt);

    return true;
}

void FdHolder::construct(){
    alloc_cnt = SCALE_UNIT_CNT;
    fd_cnt = 0;

    pathname_list = (const char**)malloc(sizeof(const char*) * alloc_cnt);
    fd_list = (int*)malloc(sizeof(int) * alloc_cnt);
}

int64_t FdHolder::insert(const char * pathname, int fd){
    // check capacity
    scale_capacity(false);

    // insert values
    pathname_list[fd_cnt] = pathname;
    //table_id_list[fd_cnt] = next_table_id;
    fd_list[fd_cnt] = fd;

    // increase numbers
    ++fd_cnt;
    return this->get_table_id(pathname);
}

bool FdHolder::is_table_exists(const char* pathname){
    for(int i=0; i<fd_cnt; i++){
        if(pathname == pathname_list[i]){
            return true;
        }
    }

    std::cout << "fail to get fd by pathname : " << pathname << std::endl;
    return false;
}

int64_t FdHolder::get_table_id(const char* pathname){
    return std::stoi(std::string(pathname).substr(4));
}

int FdHolder::get_fd(int64_t table_id){
    if(table_id == 0){
        throw std::invalid_argument("table_id must not be 0(zero)");
    }

    for(int i=0; i<fd_cnt; i++){
        if(table_id == get_table_id(pathname_list[i])){
            return fd_list[i];
        }
    }

    std::cout << "fail to get fd by table id : " << table_id << std::endl;
    throw std::runtime_error("fail to get fd");
}

void FdHolder::destroy(){
    alloc_cnt = 0;
    fd_cnt = 0;

    delete []pathname_list;
    delete []fd_list;
}