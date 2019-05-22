#include <gtest/gtest.h>
#include <string>

#include "test_table1.h"

static uint32_t gCount = 0;
static std::shared_ptr<T1Table> spTable;

class IndexTest : public ::testing::Test {
protected:
  virtual void SetUp() override {

    vsqlite = vsqlite::VSQLiteInstance();

    if (gCount++ == 0) {
      spTable = std::make_shared<T1Table>();
      int status = vsqlite->add(spTable);
      ASSERT_EQ(0, status);
    } else {
      spTable->reset();
    }
  }
  virtual void TearDown() override {
    
  }

  vsqlite::SPVSQLite vsqlite;
  vsqlite::SimpleQueryListener listener;

};


TEST_F(IndexTest, select_all) {

  int rv = vsqlite->query("SELECT * FROM t1", listener);
  ASSERT_EQ(0, rv);
  ASSERT_EQ(1, spTable->_num_prepare_calls);

  EXPECT_EQ(T1Table::getRawData().size(), listener.results.size());
}

TEST_F(IndexTest, select_all2) {
  
  int rv = vsqlite->query("SELECT * FROM t1", listener);
  ASSERT_EQ(0, rv);
  ASSERT_EQ(1, spTable->_num_prepare_calls);
  
  EXPECT_EQ(T1Table::getRawData().size(), listener.results.size());
}
