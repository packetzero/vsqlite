#include <gtest/gtest.h>
#include <string>

#include "../include/vsqlite/vsqlite.h"

struct Function_power : public vsqlite::AppFunctionBase {
  Function_power() : vsqlite::AppFunctionBase("power", { TFLOAT64, TFLOAT64 }) {}

  DynVal func(const std::vector<DynVal> &args, std::string &errmsg) override {
    return pow((double)args[0], (double)args[1]);
  }
};

struct Function_sqrt : public vsqlite::AppFunctionBase {
  Function_sqrt() : vsqlite::AppFunctionBase("sqrt", { TFLOAT64 }) {}

  DynVal func(const std::vector<DynVal> &args, std::string &errmsg) override {
    return sqrt((double)args[0]);
  }
};

struct Function_pi : public vsqlite::AppFunctionBase {
  Function_pi() : vsqlite::AppFunctionBase("pi", { }) {}
  
  DynVal func(const std::vector<DynVal> &args, std::string &errmsg) override {
    return 3.1415;
  }
};

struct Function_hola : public vsqlite::AppFunctionBase {
  Function_hola() : vsqlite::AppFunctionBase("hola", { TSTRING }) {}
  
  DynVal func(const std::vector<DynVal> &args, std::string &errmsg) override {
    return "Hola " + args[0].as_s();
  }
};


static uint32_t gCount = 0;

class FunctionTest : public ::testing::Test {
protected:
  virtual void SetUp() override {

    vsqlite = vsqlite::VSQLiteInstance();

    if (gCount++ == 0) {
      int status = vsqlite->add(std::make_shared<Function_power>());
      ASSERT_EQ(0, status);

      status = vsqlite->add(std::make_shared<Function_sqrt>());
      ASSERT_EQ(0, status);

      status = vsqlite->add(std::make_shared<Function_pi>());
      ASSERT_EQ(0, status);
    }
  }
  virtual void TearDown() override {

  }

  vsqlite::SPVSQLite vsqlite;
  vsqlite::SimpleQueryListener listener;
};


TEST_F(FunctionTest, simple_pow) {
  int rv = vsqlite->query("SELECT power(2,10) as val", listener);
  ASSERT_EQ(0, rv);
  ASSERT_EQ(1, listener.results.size());
  DynMap &row = listener.results[0];
  SPFieldDef colId = listener.columnForName("val");
  ASSERT_FALSE(nullptr == colId);
  ASSERT_EQ(TFLOAT64, row[colId].type());
  ASSERT_EQ(1024,(int)row[colId]);
}

TEST_F(FunctionTest, pow_pow) {
  int rv = vsqlite->query("SELECT power(2,power(2,3)) as val", listener);
  ASSERT_EQ(0, rv);
  ASSERT_EQ(1, listener.results.size());
  DynMap &row = listener.results[0];
  SPFieldDef colId = listener.columnForName("val");
  ASSERT_FALSE(nullptr == colId);
  ASSERT_EQ(TFLOAT64, row[colId].type());
  ASSERT_EQ(256.0,(double)row[colId]);
  ASSERT_EQ(256,(int)row[colId]);
}

TEST_F(FunctionTest, simple_noargs) {
  int rv = vsqlite->query("SELECT pi() as val", listener);
  ASSERT_EQ(0, rv);
  ASSERT_EQ(1, listener.results.size());
  DynMap &row = listener.results[0];
  SPFieldDef colId = listener.columnForName("val");
  ASSERT_FALSE(nullptr == colId);
  ASSERT_EQ(TFLOAT64, row[colId].type());
  ASSERT_EQ(3.1415,(double)row[colId]);
}

TEST_F(FunctionTest, add_remove) {
  int rv = vsqlite->query("SELECT hola('Bob') as val", listener);
  EXPECT_NE(0, rv);
  // register
  auto func = std::make_shared<Function_hola>();
  rv = vsqlite->add(func);
  EXPECT_EQ(0, rv);
  rv = vsqlite->query("SELECT hola('Bob') as val", listener);
  // should succeed
  ASSERT_EQ(0, rv);
  ASSERT_EQ(1, listener.results.size());
  auto colId = listener.columnForName("val");
  EXPECT_EQ("Hola Bob", listener.results[0][colId].as_s());

  // unregister
  vsqlite->remove(func);

  // try again
  rv = vsqlite->query("SELECT hola('Bob') as val", listener);
  // should fail
  EXPECT_NE(0, rv);

}
