#include "gtest/gtest.h"

#include "indexes/bpt.h"
#include "trxes/trx.h"

#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <ctime>
#include <algorithm>
#include <vector>
#include <random>
#include <unistd.h>
#include <utility>

using namespace std;

static const std::string CHARACTERS {
	"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"};

static const std::string BASE_TABLE_NAME { "table" };
static constexpr int NUM_TABLES { 1 };

static constexpr int NUM_KEYS { 100 };

static constexpr int MIN_VAL_SIZE { 85 };
static constexpr int MAX_VAL_SIZE { 108 };

static constexpr int BUFFER_SIZE { 20 };

TEST(LockTest, main){
	GTEST_SKIP();

	int64_t table_id;
	int64_t key;
	char* value;
	uint16_t size;

	char ret_value[MAX_VAL_SIZE];
	uint16_t ret_size;

	std::vector<std::pair<int64_t, std::string>> key_value_pairs {};
	std::vector<std::pair<std::string, int64_t>> tables {};

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<uint16_t> len_dis(MIN_VAL_SIZE, MAX_VAL_SIZE);
	std::uniform_int_distribution<int> char_dis(1, CHARACTERS.size());

	std::default_random_engine rng(rd());

	tables.reserve(NUM_TABLES);
	key_value_pairs.reserve(NUM_KEYS);

	auto helper_function = [] (auto& gen, auto& cd, auto& size) -> std::string {
		std::string ret_str;
		int index;
		ret_str.reserve(size);

		for (int i = 0; i < size; ++i) {
			index = cd(gen) - 1;
			ret_str += CHARACTERS[index];
		}
		return ret_str;
	};

	for (int i = 0; i < NUM_TABLES; ++i)
		tables.emplace_back(BASE_TABLE_NAME + to_string(i), 0);

	for (int i = 1; i <= NUM_KEYS; ++i) {
		size = len_dis(gen);
		key_value_pairs.emplace_back(i, helper_function(gen, char_dis, size));
	}

	std::cout << "[INIT START]\n";
	if (init_db(BUFFER_SIZE) != 0) {
		return;
	}
	std::cout << "[INIT END]\n\n";

    std::cout << "[OPEN TABLE START]\n";
	for (auto& t : tables) {
		remove(t.first.c_str());
		table_id = open_table(const_cast<char*>(t.first.c_str()));
		if (table_id < 0) {
			goto func_exit;
		} else {
			t.second = table_id;
		}
	}
	std::cout << "[OPEN TABLE END]\n\n";

	std::cout << "[TEST START]\n";
	for (const auto& t: tables) {
		std::cout << "[TABLE : " << t.first << " START]\n";
		std::sort(key_value_pairs.begin(), key_value_pairs.end(), std::less<>());
		// std::shuffle(key_value_pairs.begin(), key_value_pairs.end(), rng);
		std::cout << "[INSERT START]\n";
		for (const auto& kv: key_value_pairs) {
			// std::cout << "insert " << kv.first << std::endl;
			if (db_insert(t.second, kv.first, const_cast<char*>(kv.second.c_str()), kv.second.size()) != 0) {
				goto func_exit;
			}
		}
		std::cout << "[INSERT END]\n";

		std::cout << "[UPDATE START]\n";
		int trx_id = trx_begin();
		
        auto& kv = key_value_pairs.at(0);
		uint16_t old_val_size = 0;

		if (db_update(t.second, kv.first, const_cast<char*>(kv.second.substr(12).c_str()),
                kv.second.substr(12).size(), &old_val_size, trx_id) != 0) {
            std::cout << "failed to update " << kv.first << std::endl;
            goto func_exit;
        } else if (old_val_size != kv.second.size()) {
            std::cout << "failed to update " << kv.first << " (size)" << std::endl;
            goto func_exit;
        }

		//trx_commit(trx_id);
		trx_rollback(trx_id);
		std::cout << "[UPDATE END]\n";

		std::cout << "[UPDATE START]\n";
		trx_id = trx_begin();
		
        kv = key_value_pairs.at(0);
        old_val_size = 0;

		if (db_update(t.second, kv.first, const_cast<char*>(kv.second.substr(15).c_str()),
                kv.second.substr(15).size(), &old_val_size, trx_id) != 0) {
            std::cout << "failed to update " << kv.first << std::endl;
            goto func_exit;
        } else if (old_val_size != kv.second.size()) {
            std::cout << "failed to update " << kv.first << " (size)" << std::endl;
            goto func_exit;
        }

		trx_commit(trx_id);
		std::cout << "[UPDATE END]\n";
	}
	std::cout << "[TEST END]\n";

func_exit:

	std::cout << "[SHUTDOWN START]\n";
	if (shutdown_db() != 0) {
		return;
	}
	std::cout << "[SHUTDOWN END]\n";
	return;
}

TEST(ScenarioLockTest, xLock){
	if (init_db(BUFFER_SIZE) != 0) {
		std::cout << "Failed to initialize" << std::endl;
	}

		if (init_db(BUFFER_SIZE) != 0) {
		std::cout << "Failed to initialize" << std::endl;
		return;
	}

	int64_t table_id = open_table((char*)"table0");

	db_insert(table_id, 1, const_cast<char*>(std::string(CHARACTERS, 50).c_str()),
										std::string(CHARACTERS, 50).size());
	db_insert(table_id, 2, const_cast<char*>(std::string(CHARACTERS, 10, 60).c_str()),
										std::string(CHARACTERS, 10, 60).size());

	int trx_01 = trx_begin();
	int trx_02 = trx_begin();

	lock_test_append(1, trx_01, LOCK_TYPE_EXCLUSIVE, true);
	lock_test_append(1, trx_01, LOCK_TYPE_SHARED, true);
	lock_test_append(1, trx_01, LOCK_TYPE_SHARED, false);
	lock_test_append(1, trx_02, LOCK_TYPE_SHARED, false);

	EXPECT_EQ(lock_acquire(1, 1, 1, trx_02, LOCK_TYPE_EXCLUSIVE) != nullptr, true);

	lock_test_clear();
}

TEST(DeadlockTest, main){
	GTEST_SKIP();

	if (init_db(BUFFER_SIZE) != 0) {
		std::cout << "Failed to initialize" << std::endl;
		return;
	}

	int64_t table_id = open_table((char*)"table0");

	db_insert(table_id, 1, const_cast<char*>(std::string(CHARACTERS, 50).c_str()),
										std::string(CHARACTERS, 50).size());
	db_insert(table_id, 2, const_cast<char*>(std::string(CHARACTERS, 10, 60).c_str()),
										std::string(CHARACTERS, 10, 60).size());

	int trx_01 = trx_begin();
	int trx_02 = trx_begin();

	lock_acquire(1, 1, 1, trx_01, LOCK_TYPE_EXCLUSIVE);
	lock_test_append(1, trx_02, LOCK_TYPE_SHARED, false);

	lock_acquire(1, 1, 2, trx_02, LOCK_TYPE_EXCLUSIVE);
	
	// lock_test_append(20, trx_01, LOCK_TYPE_SHARED, false);
	EXPECT_EQ(lock_acquire(1, 1, 1, trx_01, LOCK_TYPE_SHARED) == nullptr, true);

	lock_test_clear();
}