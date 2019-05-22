#include <vsqlite/vsqlite_tables.h>

#include "example_table.h"


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


int main(int argc, char *argv[])
{
  vsqlite::SPVSQLite vsqlite = vsqlite::VSQLiteInstance();

  vsqlite::SimpleQueryListener listener;

  std::vector<DynMap> results;

  vsqlite->add(std::make_shared<Function_power>());
  vsqlite->add(std::make_shared<Function_sqrt>());

  int status = vsqlite->add(newMyUsersTable());



//  status = vsqlite->query("SELECT 1 as num, power(8,2) as sixtyfour, sqrt(64) as ocho, 'some string value' as description, 4.25 as score", listener);
  //status = vsqlite->query("SELECT uid,username,userid FROM users WHERE username like '%o%'", listener);
//  status = vsqlite->query("SELECT username,userid,home FROM users WHERE userid IN (501,0,520)", listener);
  status = vsqlite->query("SELECT * FROM users", listener);
  if (!listener.errmsgs.empty()) {
    fprintf(stderr, "Error:%s\n", listener.errmsgs[0].c_str());
  } else if (listener.results.empty()) {
    printf("ERROR: no results");
  } else {
    int i = 0;
    for (DynMap row : listener.results) {
      printf("[%02d] ", i++);
      for (auto it = row.begin(); it != row.end(); it++) {
        printf("'%s':'%s',", it->first->name.c_str(), it->second.as_s().c_str());
      }
      puts("\n");
    }
  }

  return 0;
}
