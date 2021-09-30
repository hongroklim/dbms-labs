#include "gtest/gtest.h"

#include "file.h"

#include <iostream>
#include <cstring>

class CastTest : public ::testing::Test {
protected:
    void page_read_value(const page_t* page, poff_t offset, void* dest, poff_t size){
        memcpy(dest, &page->data[offset], size);
    }

    void page_write_value(page_t* page, poff_t offset, const void* src, poff_t size){
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
    page_t page {};

    page_write_value(&page, 8, &input, sizeof(input));
    page_read_value(&page, 8, &output, sizeof(output));

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