#include "gtest/gtest.h"

# include "idx.h"
# include "pages/NodePage.h"
# include "pages/HeaderPage.h"
# include "pages/InternalPage.h"


class IndexTest : public ::testing::Test {
protected:
    std::string str = "index_test.db";
    char * pathname = &str[0];
    int64_t table_id{};

    void SetUp() override {
        remove(pathname);

        EXPECT_EQ(init_db(), 0);

        table_id = open_table(pathname);
    }

    void TearDown() override {
        shutdown_db();
    }
};

TEST_F(IndexTest, init){
    // check root page number
    HeaderPage headerPage1(table_id);
    pagenum_t rootPagenum = headerPage1.getRootPagenum();
    EXPECT_GT(rootPagenum, 0);

    int64_t table_id_2 = open_table(pathname);
    EXPECT_EQ(table_id, table_id_2);

    // after reopening, root page number is the same
    HeaderPage headerPage2(table_id);
    EXPECT_EQ(headerPage2.getRootPagenum(), rootPagenum);
}

TEST_F(IndexTest, converType){
    NodePage* nodePage = new NodePage(table_id);
    InternalPage* internalPage = InternalPage::convert(nodePage);

    delete internalPage;
}

TEST_F(IndexTest, findEmpty){
    char * value = new char[112];
    uint16_t size;
    EXPECT_NE(db_find(table_id, 10, value, &size), 0);

    delete []value;
}

TEST_F(IndexTest, basicInsertion){
    // insert single value
    std::string str = "12345678901234567890123456789012345678901234567890";
    char * input = str.data();

    EXPECT_EQ(db_insert(table_id, 10, input, str.size()), 0);

    // find single value
    char* output = new char[112];
    uint16_t size;
    EXPECT_EQ(db_find(table_id, 10, output, &size), 0);
    EXPECT_EQ(size, str.size());

    delete []output;
}

TEST_F(IndexTest, basicDelete){
    // insert single value
    std::string str = "12345678901234567890123456789012345678901234567890";
    char * input = str.data();

    EXPECT_EQ(db_insert(table_id, 10, input, str.size()), 0);

    // then delete
    EXPECT_EQ(db_delete(table_id, 10), 0);

    char* output = new char[112];
    uint16_t size;

    // can not find the value
    EXPECT_NE(db_find(table_id, 10, output, &size), 0);
    delete []output;
}

TEST_F(IndexTest, DeleteThree){
    // insert single value
    std::string str = "12345678901234567890123456789012345678901234567890";
    char * input = str.data();

    EXPECT_EQ(db_insert(table_id, 10, input, str.size()), 0);
    EXPECT_EQ(db_insert(table_id, -1, input, str.size()), 0);
    EXPECT_EQ(db_insert(table_id, 13, input, str.size()), 0);

    char* output = new char[112];
    uint16_t size;

    EXPECT_EQ(db_find(table_id, 10, output, &size), 0);
    EXPECT_EQ(db_find(table_id, -1, output, &size), 0);
    EXPECT_EQ(db_find(table_id, 13, output, &size), 0);

    // then delete
    EXPECT_EQ(db_delete(table_id, 10), 0);
    EXPECT_EQ(db_delete(table_id, -1), 0);
    EXPECT_EQ(db_delete(table_id, 13), 0);

    // can not find the value
    EXPECT_NE(db_find(table_id, 10, output, &size), 0);
    EXPECT_NE(db_find(table_id, -1, output, &size), 0);
    EXPECT_NE(db_find(table_id, 13, output, &size), 0);

    delete []output;
}