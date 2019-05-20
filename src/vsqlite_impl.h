#pragma once

#include "../include/vsqlite/vsqlite_tables.h"
#include <sqlite3.h>

namespace vsqlite {
  class VSQLiteImpl : public VSQLite {
  public:
    VSQLiteImpl() {
      sqlite3_open(":memory:", &_db);
    }
    virtual ~VSQLiteImpl() {
      if (_db) {
        sqlite3_close(_db);
      }
    }

    int query(const std::string sql, QueryListener &listener /*std::vector<DynMap> &results*/) override;

    bool add(SPAppFunction spFunction) override;

    void remove(SPAppFunction spFunction) override;

    int add(SPVirtualTable spVirtualTable) override;

    void remove(SPVirtualTable spVirtualTable) override;

  private:

    // ==================== private functions ===============

    //static void _funcWrapper(sqlite3_context* context,int argc,sqlite3_value** argv);


    //--------------------------------------------------------------------
    // fills in row data
    //--------------------------------------------------------------------
    bool _populateRow(sqlite3_stmt *pStmt, std::vector<SPFieldDef> &columns, DynMap &row);

    //--------------------------------------------------------------------
    // fills in column names and types using sqlite3_ accessors
    // returns true on error or columns.empty()
    //--------------------------------------------------------------------
    bool _populateColumns(sqlite3_stmt *pStmt, std::vector<SPFieldDef> &columns);

    // member variables
    sqlite3* _db {nullptr};
    std::vector<SPAppFunction> _funcs;
    std::vector<SPVirtualTable> _tables;
  };

} // namespace
