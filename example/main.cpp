#include <vsqlite/vsqlite.h>
#include <vsqlite/vsqlite_tables.h>

#include "example_table.h"

struct DoublerFunction : public vsqlite::AppFunction {
  
  const std::string name() const override { return "dub"; }
  
  const std::vector<DynType> &expectedArgs() const override {
    static std::vector<DynType> _argtypes = { TFLOAT64 };
    return _argtypes;
  }
  
  DynVal func(const std::vector<DynVal> &args, std::string &errmsg) override {
    return 2 * args[0].as_double();
  }
};

struct Function_sqrt : public vsqlite::AppFunctionBase {
  Function_sqrt() : vsqlite::AppFunctionBase("sqrt", { TFLOAT64 }) {}
  
  DynVal func(const std::vector<DynVal> &args, std::string &errmsg) override {
    return sqrt(args[0].as_double());
  }
};


int main(int argc, char *argv[])
{
  vsqlite::SPVSQLiteRegistry registry = vsqlite::VSQLiteRegistryInstance();

  int status = registry->add(newMyUsersTable());

  vsqlite::SPVSQLite vsqlite = vsqlite::VSQLiteInstance();

  vsqlite::SimpleQueryListener listener;
  
  std::vector<DynMap> results;
  
  vsqlite->add(std::make_shared<DoublerFunction>());
  vsqlite->add(std::make_shared<Function_sqrt>());

  status = vsqlite->query("SELECT 1 as num, dub(11) as dd, sqrt(64) as ocho, 'some string value' as description, 4.25 as score", listener); //results);
//  status = vsqlite->query("SELECT * FROM users WHERE name like '%o%'", results);

  if (listener.results.empty()) {
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
