#include <gtest/gtest.h>
#include <string>

#include "test_table1.h"

class IndexTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    if (nullptr == vsqlite) {
      vsqlite = vsqlite::VSQLiteInstance();
      spTable = std::make_shared<T1Table>();
      int status = vsqlite->add(spTable);
      ASSERT_EQ(0, status);
    } else {
      spTable->reset();
    }
  }

  vsqlite::SPVSQLite vsqlite;
  std::shared_ptr<T1Table> spTable;
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
