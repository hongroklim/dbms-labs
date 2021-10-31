#include "gtest/gtest.h"

#include "file.h"
#include "page.h"
#include "idx.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <algorithm>

#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <sys/stat.h>
#include <indexes/LeafPage.h>

class CastTest : public ::testing::Test {
protected:
    void page_read_value(const page_t* page, uint16_t offset, void* dest, uint16_t size){
        memcpy(dest, &page->data[offset], size);
    }

    void page_write_value(page_t* page, uint16_t offset, const void* src, uint16_t size){
        memcpy(&page->data[offset], src, size);
    }
};

TEST_F(CastTest, uint64ToChar){
    uint64_t num1, num2;
    bool simpleBool, boolArr[8], boolArr2[16];

    // compare both types' sizes
    EXPECT_EQ(sizeof(num1), sizeof(boolArr));
    EXPECT_NE(sizeof(simpleBool), sizeof(boolArr));

    // convert between types
    num1 = 1234;
    num2 = 9876;
    memcpy(boolArr, &num1, sizeof(num1));
    memcpy(&num2, boolArr, sizeof(num1));

    EXPECT_EQ(num1, num2);

    // with offset
    num1 = 1111;
    num2 = 9999;
    memcpy(&boolArr2[8], &num1, sizeof(num1));   // num1 -> boolArr
    memcpy(&num2, &boolArr2[8], sizeof(num1));   // boolArr -> num2

    EXPECT_EQ(num1, num2);
}

TEST_F(CastTest, readValueFunc){
    uint64_t input = 1234, output;
    page_t* page = new page_t();

    page_write_value(page, 8, &input, sizeof(input));
    page_read_value(page, 8, &output, sizeof(output));

    EXPECT_EQ(input, output);
}

bool isNullptr(const void* input){
    return input == nullptr;
}

TEST(NullptrTest, voidPointer){
    EXPECT_TRUE(isNullptr(nullptr));

    int number = 0;
    EXPECT_FALSE(isNullptr(&number));

    char ch = '0';
    EXPECT_FALSE(isNullptr(&ch));
}

TEST(StdMapTest, getValue){
    std::map<int, int> intMap;

    int output = intMap[23];
    intMap[1] = 13;
    EXPECT_EQ(intMap[1], 13);
    EXPECT_EQ(intMap[13], NULL);
    EXPECT_EQ(output, 0);
}

TEST(AllocTest, malloc){
    char * cptr1 = strdup("abc");
    char * cptr2 = strdup(cptr1);
    EXPECT_EQ(std::string(cptr1), "abc");
    EXPECT_EQ(std::string(cptr2), "abc");
    free(cptr1);

    std::cout << sizeof(const char *) << std::endl;
    std::cout << sizeof(char *) << std::endl;
}

TEST(PageTest, init){
    auto* page = new page_t();
    char src = 'a', dest;

    page_write_value(page, 1, &src, sizeof(src));
    page_read_value(page, 1, &dest, sizeof(dest));

    EXPECT_EQ(src, dest);

    delete page;
}

class Mock{
public:
    Mock(){
        std::cout << "construct" << std::endl;
    }

    ~Mock(){
        std::cout << "destruct" << std::endl;
    }
};

TEST(ConstructorTest, reConstruct){
    Mock* mock1 = new Mock();
    Mock* mock2 = mock1;

    delete mock1;

    EXPECT_NE(mock1, nullptr);

    // delete mock2;
}

bool compSlotOffset(slot s1, slot s2){
    return s1.offset > s2.offset;
}

TEST(VectorSlotTest, shift){
    std::vector<slot> vSlot;
    vSlot.push_back(slot{0, 1, 11, 111});
    vSlot.push_back(slot{1, 2, 22, 222});

    sort(vSlot.begin(), vSlot.end(), compSlotOffset);

    EXPECT_EQ(vSlot[0].offset, 222);
    EXPECT_EQ(vSlot[1].offset, 111);
}

void assign111(uint16_t* value){
    *value = 111;
}

TEST(PointerTest, assign){
    uint16_t value;
    assign111(&value);

    std::cout << "value is " << value << std::endl;
    EXPECT_EQ(value, 111);
}

TEST(ExceptionTest, perror){
    uint16_t value = 1;
    perror("perror exception");   // simply print error
}