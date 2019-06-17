#include <gtest/gtest.h>
#include <string>

#include "table_processes.h"
#include "table_signature.h"

static uint32_t gCount = 0;
static std::shared_ptr<TProcessTable> spProcessTable;
static std::shared_ptr<TSignatureTable> spSigTable;

class JoinTest : public ::testing::Test {
protected:
  virtual void SetUp() override {

    vsqlite = vsqlite::VSQLiteInstance();

    if (gCount++ == 0) {
      spProcessTable = std::make_shared<TProcessTable>();
      int status = vsqlite->add(spProcessTable);
      ASSERT_EQ(0, status);

      spSigTable = std::make_shared<TSignatureTable>();
      status = vsqlite->add(spSigTable);
      ASSERT_EQ(0, status);
    } else {
      spProcessTable->reset();
      spSigTable->reset();
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
TEST_F(JoinTest, select_all) {

  int rv = vsqlite->query("SELECT * FROM tprocess JOIN tsig USING (path)", listener);
  ASSERT_EQ(0, rv);
  ASSERT_EQ(1, spProcessTable->_num_prepare_calls);
  ASSERT_EQ(TProcessTable::getRawData().size()+1, spProcessTable->_num_next_calls);

  EXPECT_EQ(3, listener.results.size());
}

/*
 */
TEST_F(JoinTest, indexed_select_in) {
  
  int rv = vsqlite->query("SELECT * FROM tprocess LEFT JOIN tsig USING (PATH) WHERE path IN ('/usr/bin/base64','/bin/ls','/usr/local/bin/exiftool')", listener);
  ASSERT_EQ(0, rv);
  ASSERT_EQ(1, spProcessTable->_num_prepare_calls);
  ASSERT_EQ(TProcessTable::getRawData().size()+1, spProcessTable->_num_next_calls);

  ASSERT_EQ(2, spSigTable->_num_prepare_calls);
  ASSERT_EQ(4, spSigTable->_num_next_calls);


  EXPECT_EQ(2, listener.results.size());
}
/*

 */
TEST_F(JoinTest, not_select_in) {
  
  int rv = vsqlite->query("SELECT * FROM tprocess LEFT JOIN tsig USING (PATH) WHERE path LIKE '/usr/%' AND cdhash IN ('e264d46afea7e77cbd81eca5ad245a3bcae99fe0','c5c058f85e064bc4a9a6ef443a3a368ad6dc54ed','60b123cff9c66b6c5fd65ef48ea5a2431d4bae0d')", listener);
  ASSERT_EQ(0, rv);
  ASSERT_EQ(1, spProcessTable->_num_prepare_calls);
  ASSERT_EQ(TProcessTable::getRawData().size()+1, spProcessTable->_num_next_calls);
  
  ASSERT_EQ(3, spSigTable->_num_prepare_calls);
  ASSERT_EQ(2*2 + 1, spSigTable->_num_next_calls);

  EXPECT_EQ(1, listener.results.size());
}

