#include <gtest/gtest.h>
#include <string>

#include "../include/vsqlite/vsqlite_tables.h"

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
  //ASSERT_EQ(listener.results[0]);
}
