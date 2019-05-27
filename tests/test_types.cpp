#include <gtest/gtest.h>
#include <string>

#include "test_table1.h"

static uint32_t gCount = 0;
static std::shared_ptr<T1Table> spTable;

class TypesTest : public ::testing::Test {
protected:
  virtual void SetUp() override {
    
    if (gCount++ == 0) {
      vsqlite = vsqlite::VSQLiteInstance();
      spTable = std::make_shared<T1Table>();
      int status = vsqlite->add(spTable);
      ASSERT_EQ(0, status);
    }
  }
  virtual void TearDown() override {
    
  }
  
  vsqlite::SPVSQLite vsqlite;
  vsqlite::SimpleQueryListener listener;
};

/*
 // name, u32val, dval, i64val, active
 {"alpha", 0xaaaa, 0.123, 555444333222111L, true }
 ,{"beta", 0xbbbb, 1.1, 111222333444555L, true }
 ,{"charlie", 0xcccc, 2.2, -555444333222111L, false }
 ,{"delta", 0xdddd, 3.33, -111222333444555L, true }
 */
TEST_F(TypesTest, simple) {
  int rv = vsqlite->query("SELECT * FROM t1", listener);
  ASSERT_EQ(0, rv);
  ASSERT_EQ(T1Table::getRawData().size(), listener.results.size());

  // get column ids

  auto FNAME = listener.columnForName("name");
  auto FU32VAL = listener.columnForName("u32val");
  auto FDVAL = listener.columnForName("dval");
  auto FI64VAL = listener.columnForName("longo");
  auto FACTIVE = listener.columnForName("is_active");
  
  // check row 0 values
  auto &row = listener.results[0];
  EXPECT_EQ(0xaaaa, (int)row[FU32VAL]);
  EXPECT_EQ("alpha", row[FNAME].as_s());
  EXPECT_EQ(true, (bool)row[FACTIVE]);
  EXPECT_EQ(0.123, (double)row[FDVAL]);
  EXPECT_EQ(555444333222111L, (int64_t)row[FI64VAL]);
  
  // check row 1 values
  auto &row1 = listener.results[1];
  EXPECT_EQ(0xbbbb, (int)row1[FU32VAL]);
  EXPECT_EQ("beta", row1[FNAME].as_s());
  EXPECT_EQ(true, (bool)row1[FACTIVE]);
  EXPECT_EQ(1.1, (double)row1[FDVAL]);
  EXPECT_EQ(111222333444555L, (int64_t)row1[FI64VAL]);

  // check row 2 values
  auto &row2 = listener.results[2];
  EXPECT_EQ(0xcccc, (int)row2[FU32VAL]);
  EXPECT_EQ("charlie", row2[FNAME].as_s());
  EXPECT_EQ(false, (bool)row2[FACTIVE]);
  EXPECT_EQ(2.2, (double)row2[FDVAL]);
  EXPECT_EQ(-555444333222111L, (int64_t)row2[FI64VAL]);

  // "delta", 0xdddd, 3.33, -111222333444555L, true
  auto &row3 = listener.results[3];
  EXPECT_EQ(0xdddd, (int)row3[FU32VAL]);
  EXPECT_EQ("delta", row3[FNAME].as_s());
  EXPECT_EQ(true, (bool)row3[FACTIVE]);
  EXPECT_EQ(3.33, (double)row3[FDVAL]);
  EXPECT_EQ(-111222333444555L, (int64_t)row3[FI64VAL]);
}
