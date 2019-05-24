#include <gtest/gtest.h>
#include <string>

#include "test_table1.h"
#include "table_req1.h"

static uint32_t gCount = 0;
static std::shared_ptr<T1Table> spTable;
static std::shared_ptr<T2RequiredTable> spTable2;

class IndexTest : public ::testing::Test {
protected:
  virtual void SetUp() override {

    vsqlite = vsqlite::VSQLiteInstance();

    if (gCount++ == 0) {
      spTable = std::make_shared<T1Table>();
      int status = vsqlite->add(spTable);
      ASSERT_EQ(0, status);

      spTable2 = std::make_shared<T2RequiredTable>();
      status = vsqlite->add(spTable2);
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

/*
 * duplicate the above test again to make sure state is cleared
 */
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


/*
 * Query for items with constraints on a column that is not indexed.
 * (This exposes osquery bug 5379)
 * Expected:
 *  one prepare() call
 *   next() calls for each element in data + 1
 */
TEST_F(IndexTest, select_in_not_indexed) {
  int rv = vsqlite->query("SELECT * FROM t1 WHERE is_active=true", listener);
  ASSERT_EQ(0, rv);
  ASSERT_EQ(1, spTable->_num_prepare_calls);
  ASSERT_EQ(0, spTable->_num_index_constraints);
  ASSERT_EQ((T1Table::getRawData().size()+1), spTable->_num_next_calls);

  // only 3 of the items should match (sqlite3 will filter them out)
  EXPECT_EQ(3, listener.results.size());
}


/*
 */
TEST_F(IndexTest, required_like_missing) {
  // path column is REQUIRED, this should fail
  int rv = vsqlite->query("SELECT * FROM tpath_len", listener);
  ASSERT_EQ(-1, rv);
  ASSERT_FALSE(listener.errmsgs.empty());
}

TEST_F(IndexTest, required_like) {
  int rv = vsqlite->query("SELECT * FROM tpath_len WHERE path LIKE '/dev/%0' OR path LIKE '/home/%'", listener);
  ASSERT_EQ(0, rv);
  ASSERT_EQ(2, spTable2->_num_prepare_calls); // one for each LIKE term
  ASSERT_EQ(2, spTable2->_num_index_constraints);
  ASSERT_EQ(12, spTable2->_num_next_calls);

  EXPECT_EQ(7, listener.results.size());

  // again to check for consistency
  
  listener.results.clear();
  rv = vsqlite->query("SELECT * FROM tpath_len WHERE path LIKE '/dev/%0' OR path LIKE '/home/%'", listener);

  ASSERT_EQ(0, rv);
  ASSERT_EQ(4, spTable2->_num_prepare_calls); // one for each LIKE term
  ASSERT_EQ(4, spTable2->_num_index_constraints);
  ASSERT_EQ(24, spTable2->_num_next_calls);

  EXPECT_EQ(7, listener.results.size());
}

/*
 * I found this test case by accident.
 * The table state gets confused.
 * The subselect only has u32val column used, but outer select
 * has all columns.  The implementation was acting as if not all
 * columns were requested.
 */
TEST_F(IndexTest, sub_select_index_colsused) {
  int rv = vsqlite->query("SELECT * FROM t1 WHERE u32val IN (SELECT u32val FROM t1)", listener);
  ASSERT_EQ(0, rv);
  EXPECT_EQ((T1Table::getRawData().size()+1), spTable->_num_prepare_calls);
  EXPECT_EQ((T1Table::getRawData().size()), spTable->_num_index_constraints);
  int num_next_calls = 0;
  // called once for each + 1 to get indexes in subquery
  num_next_calls += (T1Table::getRawData().size() + 1);
  // called twice for each indexed data item
  num_next_calls += (T1Table::getRawData().size()) * 2;

  EXPECT_EQ((T1Table::getRawData().size()*3+1), spTable->_num_next_calls);
  
  EXPECT_EQ((T1Table::getRawData().size()), listener.results.size());
}
