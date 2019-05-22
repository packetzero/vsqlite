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

/*
 * Query all items with no constraints.
 * I expect:
 *   one prepare() call
 *   one next() call for each data item + 1 to determine no more data
 */
TEST_F(IndexTest, select_all) {

  int rv = vsqlite->query("SELECT * FROM t1", listener);
  ASSERT_EQ(0, rv);
  ASSERT_EQ(1, spTable->_num_prepare_calls);
  ASSERT_EQ(T1Table::getRawData().size()+1, spTable->_num_next_calls);

  EXPECT_EQ(T1Table::getRawData().size(), listener.results.size());
}

TEST_F(IndexTest, select_all2) {

  int rv = vsqlite->query("SELECT * FROM t1", listener);
  ASSERT_EQ(0, rv);
  ASSERT_EQ(1, spTable->_num_prepare_calls);
  ASSERT_EQ(T1Table::getRawData().size()+1, spTable->_num_next_calls);

  EXPECT_EQ(T1Table::getRawData().size(), listener.results.size());
}

/*
 * Query for items in data.
 * u32val column is INDEXED, so I expect:
 *  one prepare() call for each element in IN() list
 *  two next() calls for each element in IN() list.
 *         (one for found item, another to determine no more items for same value)
 */
TEST_F(IndexTest, indexed_select_in) {

  int rv = vsqlite->query("SELECT * FROM t1 WHERE u32val IN (0xaaaa,0xbbbb)", listener);
  ASSERT_EQ(0, rv);
  ASSERT_EQ(2, spTable->_num_prepare_calls);
  ASSERT_EQ(4, spTable->_num_next_calls);

  EXPECT_EQ(2, listener.results.size());
}

/*
 * Query for items not in data.
 * u32val is INDEXED , so I expect :
 *   one prepare() call for each element in IN() list
 *   one next() call for each element in IN() list (no data found)
 */
TEST_F(IndexTest, indexed_select_in_empty) {

  int rv = vsqlite->query("SELECT * FROM t1 WHERE u32val IN (2,3,4)", listener);
  ASSERT_EQ(0, rv);
  ASSERT_EQ(3, spTable->_num_prepare_calls);
  ASSERT_EQ(3, spTable->_num_index_constraints);

  ASSERT_EQ(3, spTable->_num_next_calls);

  EXPECT_EQ(0, listener.results.size());
}

/*
 * Query for items in declared index, but index is not implemented.
 * name column is marked INDEXED, but not implemented
 * Expected:
 *  one prepare() call for each element in IN() list
 *  two next() calls for each element in IN() list.
 *         (one for found item, another to determine no more items for same value)
 */
TEST_F(IndexTest, indexed_select_in_not_implemented) {
  int list_size=4;
  int rv = vsqlite->query("SELECT * FROM t1 WHERE name IN ('beta','delta','xxx','yyy')", listener);
  ASSERT_EQ(0, rv);
  ASSERT_EQ(list_size, spTable->_num_prepare_calls);
  ASSERT_EQ(0, spTable->_num_index_constraints);
  ASSERT_EQ((T1Table::getRawData().size()+1)*list_size, spTable->_num_next_calls);

  // only 2 of the items should match (sqlite3 will filter them out)
  EXPECT_EQ(2, listener.results.size());
}
