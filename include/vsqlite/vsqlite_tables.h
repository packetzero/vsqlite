#pragma once

#include <map>
#include <vector>
#include <string>
#include <memory>
#include <set>

#include <dynobj.hpp>

namespace vsqlite {

struct TableDef;
typedef std::shared_ptr<TableDef> SPTableDef;
typedef std::shared_ptr<const TableDef> CSPTableDef;

enum ColOpt {
   INDEXED    = (1 << 1)
  ,REQUIRED   = (1 << 2)
  ,ADDITIONAL = (1 << 3)
  ,HIDDEN     = (1 << 4)
  ,ALIAS      = (1 << 8)
};

struct ColumnDef {
  SPFieldDef id;
  uint32_t options;
  std::string description;
  SPFieldDef aliased;
};

typedef std::shared_ptr<ColumnDef> SPColumnDef;

/*
 * no support for table aliases
 */
struct TableDef {
  SPSchemaId table_name;
  std::vector<ColumnDef> columns;
  std::vector<std::string> table_attrs;  // CACHEABLE,EVENT
};

enum ConstraintOp {
  OP_EQ = 2
};
  
struct Constraint {
  SPFieldDef columnId;
  ConstraintOp op;
  DynVal value;
};

struct QueryContext {
  virtual std::vector<Constraint> getConstraints() = 0;
  virtual std::set<SPFieldDef> getRequestedColumns() = 0;
};
typedef std::shared_ptr<QueryContext> SPQueryContext;

enum TLStatus {
  TL_STATUS_OK = 0,
  TL_STATUS_ABORT = 99
};

struct VirtualTable {

  virtual const TableDef& getTableDef() const = 0;

  /**
   * query virtual table
   */
  virtual int prepare(SPQueryContext context/*, TableListener &listener*/) = 0;

  virtual int next(DynMap &row, uint64_t rowId) = 0;
};
typedef std::shared_ptr<VirtualTable> SPVirtualTable;

struct QueryListener {
  virtual int onResultRow(DynMap &row) = 0;
  virtual void onQueryError(const std::string errmsg) = 0;
};

struct SimpleQueryListener : public QueryListener {
  int onResultRow(DynMap &row) override {
    results.push_back(row);
    return 0;
  }
  void onQueryError(const std::string errmsg) override { errmsgs.push_back(errmsg); }
  std::vector<DynMap> results;
  std::vector<std::string> errmsgs;
};

struct AppFunction {

  virtual const std::string name() const = 0;

  virtual const std::vector<DynType> &expectedArgs() const = 0;

  virtual DynVal func(const std::vector<DynVal> &args, std::string &errmsg) = 0;
};
typedef std::shared_ptr<AppFunction> SPAppFunction;

struct AppFunctionBase : public AppFunction {
  AppFunctionBase(const std::string name, const std::vector<DynType> argtypes):
    _name(name), _argtypes(argtypes) {}

  virtual ~AppFunctionBase() { }

  const std::string name() const override { return _name; }

  const std::vector<DynType> &expectedArgs() const override { return _argtypes; }

  const std::string _name;
  const std::vector<DynType> _argtypes;
};

struct VSQLite {

//  virtual int query(const std::string sql, std::vector<DynMap> &results) = 0;

  virtual int query(const std::string sql, QueryListener &results) = 0;

  virtual bool add(SPAppFunction spFunction) = 0;

  virtual void remove(SPAppFunction spFunction) = 0;

  virtual int add(SPVirtualTable spVirtualTable) = 0;

  virtual void remove(SPVirtualTable spVirtualTable) = 0;

};
typedef std::shared_ptr<VSQLite> SPVSQLite;

SPVSQLite VSQLiteInstance();


} // namespace
