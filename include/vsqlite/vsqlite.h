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

// these must match SQLITE_INDEX_CONSTRAINT_EQ, etc.
enum ConstraintOp {
  OP_EQ = 2
  ,OP_GT = 4
  ,OP_LE = 8
  ,OP_LT = 16
  ,OP_GE = 32
  ,OP_MATCH = 64
  ,OP_LIKE = 65
  ,OP_GLOB = 66
  ,OP_REGEX = 67
};

struct ColumnDef {
  SPFieldDef id;
  uint32_t options;
  std::string description;
  SPFieldDef aliased;
  // ops other than OP_EQ need to be added explicitly
  std::set<ConstraintOp> indexOpsImplemented;
};

typedef std::shared_ptr<ColumnDef> SPColumnDef;

/*
 * The TableDef is the static definition of your table schema.
 */
struct TableDef {
  SPSchemaId schemaId;
  std::vector<ColumnDef> columns;
  std::vector<std::string> table_attrs;  // CACHEABLE,EVENT
};

struct Constraint {
  SPFieldDef columnId;
  ConstraintOp op;
  DynVal value;
};

/*
 * A VirtualTable.prepare() method will be called to
 * filter a set of data based on the QueryContext.
 */
struct QueryContext {

  /**
   * Constraints should be provided for indexed columns
   * being filtered.
   */
  virtual std::vector<Constraint> getConstraints() = 0;

  /**
   * A list of columns being requested by the current
   * query.  This allows table implementations to optimize
   * out extra work needed to fetch certain columns of data.
   */
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
   * If index is used, prepare will be called once for each value
   * with the OP_EQ constraint.
   * For example if 'WHERE userid IN (2,7,9,11)' then prepare() will
   * be called 4 times.
   * If there are no index constraints, prepare() will be called
   * once, then calls to next() until it returns 1;
   */
  virtual void prepare(SPQueryContext context) = 0;

  /**
   * Called for each row of data.
   * The caller (vsqlite layer) will determine if next()
   * provides data if returns 0 OR row.empty().
   * @param data Gets set when data is available (out param)
   * @returns true if data is available, false if no more data.
   */
  virtual bool next(DynMap &row) = 0;
};
typedef std::shared_ptr<VirtualTable> SPVirtualTable;

/**
 * To receive results from vsqlite.query(), you need to implement
 * and provide a QueryListener implementation.  See the
 * SimpleQueryListener class for a default implementation.
 */
struct QueryListener {

  /*
   * Called for every data result row.
   */
  virtual TLStatus onResultRow(DynMap &row) = 0;

  /*
   * Called if a virtual table wants to report an error.
   */
  virtual void onQueryError(const std::string errmsg) = 0;
};

/**
 * Interface for a custom function to hook into vsqlite.
 */
struct AppFunction {

  virtual const std::string name() const = 0;

  virtual const std::vector<DynType> &expectedArgs() const = 0;

  /**
   * The actual function implementation called by database.
   * @param args Function argument values. size and types will match expectedArgs.
   * @param errmsg If there is an error condition, function can set errmsg.
   * @returns typed value. If the function could fail, then returns
   * empty DynVal() value where DynVal.valid() == false.  The vsqlite layer
   * will turn that into a null result.
   */
  virtual DynVal func(const std::vector<DynVal> &args, std::string &errmsg) = 0;
};
typedef std::shared_ptr<AppFunction> SPAppFunction;

/**
 * Interface to vsqlite database instance.
 */
struct VSQLite {

  /*
   * query the database.
   */
  virtual int query(const std::string sql, QueryListener &results) = 0;

  /*
   * add and remove application defined functions to db.
   */

  virtual bool add(SPAppFunction spFunction) = 0;

  virtual void remove(SPAppFunction spFunction) = 0;

  /*
   * add and remove application defined virtual tables
   */

  virtual int add(SPVirtualTable spVirtualTable) = 0;

  virtual void remove(SPVirtualTable spVirtualTable) = 0;

};
typedef std::shared_ptr<VSQLite> SPVSQLite;

/**
 * Get shared singleton instance of database.
 */
SPVSQLite VSQLiteInstance();

// ================== Implementations ==========================


/**
 * Base-class for custom functions to hook into vsqlite
 */
struct AppFunctionBase : public AppFunction {
  /*
   * Sets name and expectedArgs.
   */
  AppFunctionBase(const std::string name, const std::vector<DynType> argtypes):
    _name(name), _argtypes(argtypes) {}

  virtual ~AppFunctionBase() { }

  const std::string name() const override { return _name; }

  const std::vector<DynType> &expectedArgs() const override { return _argtypes; }

  const std::string _name;
  const std::vector<DynType> _argtypes;
};

/**
 * Basic implementation of QueryListener
 */
struct SimpleQueryListener : public QueryListener {
  TLStatus onResultRow(DynMap &row) override {
    results.push_back(row);
    return TL_STATUS_OK;
  }
  SPFieldDef columnForName(std::string name) {
    if (results.empty()) { return nullptr; }
    for (auto it = results[0].begin(); it != results[0].end(); it++) {
      if (it->first->name == name) {
        return it->first;
      }
    }
    return nullptr;
  }
  void onQueryError(const std::string errmsg) override { errmsgs.push_back(errmsg); }
  std::vector<DynMap> results;
  std::vector<std::string> errmsgs;
};

/**
 * Tests may need additional clean instances of the database.
 */
SPVSQLite VSQLiteNew();

std::string TableInfo(SPVirtualTable spTable);

std::string FunctionInfo(SPAppFunction spFunction);

} // namespace
