#include <vsqlite/vsqlite.h>

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

std::vector<vsqlite::SPAppFunction> gFunctions;
std::vector<vsqlite::SPVirtualTable> gTables;



static int usage(char *arg0) {
  printf("usage: %s \"<sql>\"\n", arg0);
  printf(" virtual tables:\n");
  for (auto spTable : gTables) {
    printf("   %s\n", vsqlite::TableInfo(spTable).c_str());
  }
  printf(" functions:\n");
  for (auto spFunction : gFunctions) {
    printf("   %s\n", vsqlite::FunctionInfo(spFunction).c_str());
  }
  printf("\n");
  return 0;
}

int main(int argc, char *argv[])
{
  gFunctions.push_back(std::make_shared<Function_power>());
  gFunctions.push_back(std::make_shared<Function_sqrt>());
  gTables.push_back(newMyUsersTable());

  if (argc < 2) {
    return usage(argv[0]);
  }

  vsqlite::SPVSQLite vsqlite = vsqlite::VSQLiteInstance();

  vsqlite::SimpleQueryListener listener;

  for (auto spFunction : gFunctions) {
    int status = vsqlite->add(spFunction);
    if (status) {
      fprintf(stderr, "Failed to add function:'%s'\n", spFunction->name().c_str());
    }
  }

  for (auto spTable : gTables) {
    int status = vsqlite->add(spTable);
    if (status) {
      fprintf(stderr, "Failed to add table:'%s'\n", spTable->getTableDef().schemaId->name.c_str());
    }
  }


  std::string sql = argv[1];

  int status = vsqlite->query(sql, listener);

  if (!listener.errmsgs.empty()) {
    fprintf(stderr, "Error:%s\n", listener.errmsgs[0].c_str());
  } else if (listener.results.empty()) {
    if (status != 0) {
      printf("ERROR: %d\n", status);
    } else {
      printf("No results");
    }
  } else {
    // print out results
    int i = 0;
    for (DynMap row : listener.results) {
      printf("[%02d] ", i++);
      for (auto it = row.begin(); it != row.end(); it++) {
        std::string value = (it->second.valid() ? it->second.as_s() : "null");
        printf("'%s':'%s',", it->first->name.c_str(), value.c_str());
      }
      puts("");
    }
  }

  return 0;
}
