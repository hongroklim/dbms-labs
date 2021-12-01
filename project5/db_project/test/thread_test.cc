/**
 * Note : this code refers to TA's sample test script
 */
#include "gtest/gtest.h"

#include "indexes/bpt.h"
#include "trxes/trx.h"

#include <cstring>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

static const std::string CHARACTERS {
	"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"};

int64_t table_id = 0;
int THREAD_NUMBER = 3;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int err_cnt = 0;
int success_cnt = 0;

void* xlock_func(void* arg){
    int trx_id = trx_begin();

	uint16_t tmp_val_size = 0;
	if(db_update(table_id, 1, const_cast<char*>(
			std::string(CHARACTERS, 1, 50).c_str()),
			std::string(CHARACTERS, 1, 50).size(),
			&tmp_val_size, trx_id) != 0){
		
		pthread_mutex_lock(&mutex);
		err_cnt++;
		pthread_mutex_unlock(&mutex);
	}

    trx_commit(trx_id);

	pthread_mutex_lock(&mutex);
	success_cnt++;
	pthread_mutex_unlock(&mutex);
	return NULL;
};

TEST(MainTest, main){
    if(init_db(20) != 0){
		std::cout << "Failed to initialize" << std::endl;
		return;
	}

	table_id = open_table((char*)"table2");
    if(table_id <= 0){
        std::cout << "Failed to open table" << std::endl;
		return;
    }

	std::cout << "[INSERT START]" << std::endl;

    db_insert(table_id, 1, 
		const_cast<char*>(std::string(CHARACTERS, 50).c_str()),
		std::string(CHARACTERS, 1, 50).size());

	db_insert(table_id, 2, const_cast<char*>(
		std::string(CHARACTERS, 5, 60).c_str()),
		std::string(CHARACTERS, 5, 60).size());

	std::cout << "[INSERT END]" << std::endl;

	pthread_t	threads[THREAD_NUMBER];
	srand(time(NULL));

	for (int i = 0; i < THREAD_NUMBER; i++) {
		pthread_create(&threads[i], 0, xlock_func, NULL);
	}

	for (int i = 0; i < THREAD_NUMBER; i++) {
		pthread_join(threads[i], NULL);
	}

	shutdown_db();
	std::cout << "[EXECUTE FINISHED]" << std::endl;
	std::cout << "success : " << success_cnt << ", error : " << err_cnt << std::endl;
}