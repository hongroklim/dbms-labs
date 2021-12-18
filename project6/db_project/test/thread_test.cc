/**
 * Note : this code refers to TA's sample test script
 */
#include "gtest/gtest.h"

#include "indexes/bpt.h"
#include "trxes/trx.h"

#include <cstring>
#include <stdio.h>
#include <pthread.h>
#include <chrono>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <thread>

static const std::string CHARACTERS {
	"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"};

int64_t table_id = 0;
int THREAD_NUMBER = 30;
int KEY_COUNT = 3;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int err_cnt = 0;
int success_cnt = 0;

char* updateChars = const_cast<char*>(std::string(CHARACTERS, 1, 50).c_str());
int updateSize = std::string(CHARACTERS, 1, 50).size();

void* slock_func(void* arg){
    //long tid = (long)arg;
    int trx_id = trx_begin();
    std::this_thread::sleep_for(std::chrono::milliseconds((trx_id*rand())%200+1));
    std::cout << "    begin " << trx_id << std::endl;

    int64_t key;
    char* tml_val = new char[108];
    uint16_t tmp_val_size = 0;
    for(int i=1; i<=KEY_COUNT; i++){
        std::this_thread::sleep_for(std::chrono::milliseconds((trx_id*rand())%200+1));

        key = (rand()+trx_id)%KEY_COUNT+1;
        std::cout << "S     try " << trx_id << "," << key << std::endl;

        if(db_find(table_id, key, tml_val, &tmp_val_size, trx_id) == 0){

            std::cout << "S    lock " << trx_id << "," << key << std::endl;
            pthread_mutex_lock(&mutex);
            success_cnt++;
            pthread_mutex_unlock(&mutex);

        }else{
            std::cout << "S    fail " << trx_id << "," << key << std::endl;
            pthread_mutex_lock(&mutex);
            err_cnt++;
            pthread_mutex_unlock(&mutex);
            std::cout << "S release " << trx_id << " (fail)" << std::endl;
            return nullptr;
        }

    }

    trx_commit(trx_id);
    std::cout << "  release " << trx_id << std::endl;
    return nullptr;
}

void* xlock_func(void* arg){
    //long tid = (long)arg;
    int trx_id = trx_begin();
    std::this_thread::sleep_for(std::chrono::milliseconds((trx_id*rand())%200+1));
    std::cout << "    begin " << trx_id << std::endl;

    int64_t key;
	uint16_t tmp_val_size = 0;
	for(int i=1; i<=KEY_COUNT; i++){
        std::this_thread::sleep_for(std::chrono::milliseconds((trx_id*rand())%200+1));

        key = (rand()+trx_id*trx_id)%KEY_COUNT+1;
        std::cout << "X     try " << trx_id << "," << key << std::endl;

		if(db_update(table_id, key, const_cast<char*>(
				std::string(CHARACTERS, 1, 50).c_str()),
				std::string(CHARACTERS, 1, 50).size(),
				&tmp_val_size, trx_id) == 0){

            std::cout << "X    lock " << trx_id << "," << key << std::endl;
			pthread_mutex_lock(&mutex);
            success_cnt++;
			pthread_mutex_unlock(&mutex);

		}else{
            std::cout << "X    fail " << trx_id << "," << key << std::endl;
			pthread_mutex_lock(&mutex);
			err_cnt++;
			pthread_mutex_unlock(&mutex);
            std::cout << "X release " << trx_id << " (fail)" << std::endl;
            return nullptr;
		}

	}

    trx_commit(trx_id);
    std::cout << "  release " << trx_id << std::endl;
	return nullptr;
}

void* mlock_func(void* arg){
    int trx_id = trx_begin();

    int64_t key;
    char* tml_val = new char[108];
    uint16_t tmp_val_size = 0;

    for(int i=1; i<=5*KEY_COUNT; i++){
        std::this_thread::sleep_for(std::chrono::milliseconds((trx_id*rand())%100+1));
        key = (rand()+trx_id*trx_id)%KEY_COUNT+1;

        if(rand()%2 == 0){
            std::cout << "S     try " << trx_id << "," << key << std::endl;
            if(db_find(table_id, key, tml_val, &tmp_val_size, trx_id) == 0){
                std::cout << "S    lock " << trx_id << "," << key << std::endl;
            }else {
                std::cout << "S release " << trx_id << " (fail)" << std::endl;
                return nullptr;
            }

        }else{
            std::cout << "X     try " << trx_id << "," << key << std::endl;
            if(db_update(table_id, key, updateChars, updateSize, &tmp_val_size, trx_id) == 0){
                std::cout << "X    lock " << trx_id << "," << key << std::endl;
            }else{
                std::cout << "X release " << trx_id << " (fail)" << std::endl;
                return nullptr;
            }
        }
    }

    trx_commit(trx_id);
    std::cout << "  release " << trx_id << std::endl;
    return nullptr;
}

TEST(MainTest, main){
    if(init_db(20) != 0){
		std::cout << "Failed to initialize" << std::endl;
		return;
	}

    remove((char*)"table0");
	table_id = open_table((char*)"table0");
    if(table_id <= 0){
        std::cout << "Failed to open table" << std::endl;
		return;
    }
	
	std::cout << "[INSERT START]" << std::endl;

    for(int i=1; i<=KEY_COUNT; i++){
        db_insert(table_id, i,
                  const_cast<char*>(std::string(CHARACTERS, 50).c_str()),
                  std::string(CHARACTERS, 1, 50).size());
    }

	std::cout << "[INSERT END]" << std::endl;

	pthread_t	threads[THREAD_NUMBER];
	srand(time(nullptr));

    /*
    std::cout << "[SLOCK START]" << std::endl;
	for (long i = 0; i < THREAD_NUMBER; i++)
		pthread_create(&threads[i], 0, slock_func, (void*)i);

	for (long i = 0; i < THREAD_NUMBER; i++)
		pthread_join(threads[i], nullptr);
    std::cout << "[SLOCK END]" << std::endl;

    std::cout << "[XLOCK START]" << std::endl;
    for (long i = 0; i < THREAD_NUMBER; i++)
        pthread_create(&threads[i], 0, xlock_func, (void*)i);

    for (long i = 0; i < THREAD_NUMBER; i++)
        pthread_join(threads[i], nullptr);
    std::cout << "[XLOCK END]" << std::endl;
    */


    std::cout << "[MLOCK START]" << std::endl;
    for (long i = 0; i < THREAD_NUMBER; i++)
        pthread_create(&threads[i], 0, mlock_func, (void*)i);

    for (long i = 0; i < THREAD_NUMBER; i++)
        pthread_join(threads[i], nullptr);
    std::cout << "[MLOCK END]" << std::endl;


	shutdown_db();
	std::cout << "[EXECUTE FINISHED]" << std::endl;
	std::cout << "success : " << success_cnt << ", error : " << err_cnt << std::endl;
}