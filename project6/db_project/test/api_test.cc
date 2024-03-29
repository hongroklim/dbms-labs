/*
 * NOTE : This test code is derived from TA's one(simple_test).
 */
// #include "api.h"
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

static constexpr int NUM_KEYS { 10000 };

static constexpr int MIN_VAL_SIZE { 46 };
static constexpr int MAX_VAL_SIZE { 108 };

static constexpr int BUFFER_SIZE { 10 };

TEST(ApiTest, main){
	//GTEST_SKIP();

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
			//std::cout << "insert " << kv.first << std::endl;
			if (db_insert(t.second, kv.first, const_cast<char*>(kv.second.c_str()), kv.second.size()) != 0) {
				goto func_exit;
			}
		}
		std::cout << "[INSERT END]\n";

        // TODO debug
        int trxId = trx_begin();
        //std::shuffle(key_value_pairs.begin(), key_value_pairs.end(), rng);

        std::cout << "[FIND START]\n";
		for (const auto& kv: key_value_pairs) {
			ret_size = 0;
			memset(ret_value, 0x00, MAX_VAL_SIZE);
            // std::cout << "find " << kv.first << std::endl;
            if (db_find(t.second, kv.first, ret_value, &ret_size, trxId) != 0) {
                std::cout << "failed to find " << kv.first << std::endl;
				goto func_exit;
			} else if (kv.second.size() != ret_size ||
					kv.second != std::string(ret_value, ret_size)) {
                std::cout << "failed to find correct " << kv.first << std::endl;
				goto func_exit;
			}
		}
		std::cout << "[FIND END]\n";

        // TODO debug
        trx_commit(trxId);
        goto func_exit;

        std::cout << "[DELETE START]\n";
		for (const auto& kv: key_value_pairs) {
            //std::cout << "delete " << kv.first << std::endl;
			if (db_delete(t.second, kv.first) != 0) {
                std::cout << "failed to delete " << kv.first << std::endl;
                goto func_exit;
			}
		}
        /*for (uint i=1; i<100; i++) {
            std::cout << "delete " << i << std::endl;
            if (db_delete(t.second, i) != 0) {
                goto func_exit;
            }
        }*/
        std::cout << "[DELETE END]\n";
        std::cout << "[FIND START AGAIN]\n";
		for (const auto& kv: key_value_pairs) {
			ret_size = 0;
			memset(ret_value, 0x00, MAX_VAL_SIZE);
			if (db_find(t.second, kv.first, ret_value, &ret_size, 0) == 0) {
				goto func_exit;
			}
		}
		std::cout << "[FIND END AGAIN]\n";
		std::cout << "[TABLE : " << t.first << " END]\n\n";
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
