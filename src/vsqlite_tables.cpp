
#include "vsqlite_impl.h"
#include <assert.h>
#include <set>

#define TRACE if(0)

namespace vsqlite {

  std::string createStatement(const TableDef &td);

  /*
   * Information about a constraint is provided in xBestIndex,
   * but the constraint values are not provided until xFilter.
   */
  struct constraint_info_t {
    SPFieldDef columnId;
    int colIdx;
    int termIdx;
    unsigned char op;
  };

  /*
   * Basis of the QueryContext provided
   * to table prepare() methods.
   */
  struct QueryContextImpl : public QueryContext {
    virtual ~QueryContextImpl() {}
    std::vector<Constraint> getConstraints() override {
      return _constraints;
    }
    std::set<SPFieldDef> getRequestedColumns() override {
      return _colsUsed;
    }

    /*
     * Table implementations can set user data so they can
     * track state between prepare() and next() callbacks.
     */
    void setUserData(std::shared_ptr<void> spTablePrivate) override {
      _userData = spTablePrivate;
    }
    std::shared_ptr<void> getUserData() override {
      return _userData;
    }

    int _idxNum {0};    // matches value set in xBestIndex
    std::vector<constraint_info_t> _constraint_infos;
    std::set<SPFieldDef> _colsUsed;

    std::vector<Constraint> _constraints;
    std::shared_ptr<void> _userData;
  };


/*
 * state needed to track virtual table state.
 */
struct my_vtab : public sqlite3_vtab {
  my_vtab(VirtualTable *implementation) : sqlite3_vtab(), _implementation(implementation), _contexts() {} //_colsUsed(), _constraints() {}
  VirtualTable *_implementation;

  // following provided in xBestIndex
  //std::set<SPFieldDef> _colsUsed;
  //std::vector<constraint_info_t> _constraints;
  std::vector<std::shared_ptr<QueryContextImpl> > _contexts;
  uint64_t _rowId {0};
};

/*
 * Cursors are opened after xBestIndex
 */
struct my_vtab_cursor : public sqlite3_vtab_cursor {
  my_vtab_cursor(my_vtab *pvt) : sqlite3_vtab_cursor(), _pvt(pvt) {}

  // member variables
  my_vtab *_pvt;
  DynMap   _row;
  std::shared_ptr<QueryContextImpl> _context;
};


//----------------------------------------------------------------------
// create new virtual table instance
//----------------------------------------------------------------------
static int xConnect(sqlite3* db,
            void* pAux,
            int argc,
            const char* const* argv,
            sqlite3_vtab** ppVtab,
            char** pzErr) {

  my_vtab *pvt = new my_vtab((VirtualTable*)pAux);
  *ppVtab = pvt;

  const TableDef &tableDef = pvt->_implementation->getTableDef();
  auto sql = "CREATE TABLE " +  tableDef.schemaId->name + createStatement(tableDef);
  int rc = sqlite3_declare_vtab(db, sql.c_str());
  if (rc != SQLITE_OK) {
    fprintf(stderr, "sqlite3_declare_vtab failed with %d (%s): '%s'\n", rc,
           sqlite3_errmsg(db),
           sql.c_str());
    return rc;
  }
  return SQLITE_OK;
}

//----------------------------------------------------------------------
// open cursor
//----------------------------------------------------------------------
static int xOpen(sqlite3_vtab* tab, sqlite3_vtab_cursor** ppCursor) {
  auto pCur = new my_vtab_cursor((my_vtab*)tab);
  *ppCursor = pCur;
  return SQLITE_OK;
}

//----------------------------------------------------------------------
// get index of fieldId in columns
//----------------------------------------------------------------------
static int getIndexOfColumn(SPFieldDef fieldId, const TableDef &tableDef) {
  for (int i=0; i < tableDef.columns.size();i++) {
    if (tableDef.columns[i].id == fieldId) {
      return i;
    }
  }
  return -1;
}

//----------------------------------------------------------------------
// op
//----------------------------------------------------------------------
static inline std::string opString(unsigned char op) {
  switch (op) {
    case SQLITE_INDEX_CONSTRAINT_EQ:
      return "=";
    case SQLITE_INDEX_CONSTRAINT_GT:
      return ">";
    case SQLITE_INDEX_CONSTRAINT_LE:
      return "<=";
    case SQLITE_INDEX_CONSTRAINT_LT:
      return "<";
    case SQLITE_INDEX_CONSTRAINT_GE:
      return ">=";
    case SQLITE_INDEX_CONSTRAINT_LIKE:
      return "LIKE";
    case SQLITE_INDEX_CONSTRAINT_MATCH:
      return "MATCH";
    case SQLITE_INDEX_CONSTRAINT_GLOB:
      return "GLOB";
    case SQLITE_INDEX_CONSTRAINT_REGEXP:
      return "REGEX";
    default:
      break;
  }
  return "?";
}

//----------------------------------------------------------------------
// sqlite passes info on query, we respond by populating any
// indexes that the virtual table will handle.
//----------------------------------------------------------------------
static int xBestIndex(sqlite3_vtab* tab, sqlite3_index_info* pIdxInfo) {
  static int kConstraintIndexID = 1; // increments with each index used
  my_vtab* pVT = (my_vtab*)tab;
  int numRequiredConstraints = 0;
  int numIndexedConstraints = 0;
  int numRequiredColumns = 0;
  int numIndexedColumns = 0;
  int xFilterArgvIndex = 0;

  auto spContext = std::make_shared<QueryContextImpl>();
  spContext->_idxNum = kConstraintIndexID++;
//  pVT->_colsUsed.clear();
//  pVT->_constraints.clear();

  TRACE fprintf(stderr, "xBestIndex nConstraint:%d idxNum:%d idxFlags:0x%x\n", pIdxInfo->nConstraint, spContext->_idxNum,
                pIdxInfo->idxFlags);

  const TableDef & td = pVT->_implementation->getTableDef();

  // gather constraints

  if (pIdxInfo->nConstraint > 0) {
    for (size_t i = 0; i < static_cast<size_t>(pIdxInfo->nConstraint); ++i) {
      const sqlite3_index_info::sqlite3_index_constraint &constraint_info = pIdxInfo->aConstraint[i];

      if (constraint_info.iColumn >= td.columns.size()) {
        continue;
      }
      if (constraint_info.usable == 0) { continue; }

      // get column def, dealing with aliases

      const ColumnDef *pcoldef = &td.columns[constraint_info.iColumn];
      if (pcoldef->aliased) {
        int j = getIndexOfColumn(pcoldef->aliased, td);
        if (j < 0) {
          // TODO: error
          continue;
        }
        pcoldef = &td.columns[j];
      }

      TRACE fprintf(stderr, "   icol:%d (%s) op:%s termOffset:%d\n", constraint_info.iColumn, pcoldef->id->name.c_str(), opString(constraint_info.op).c_str(), constraint_info.iTermOffset );

      // does vtable support the constraint?

      if (pcoldef->indexOpsImplemented.empty()) {
        if (constraint_info.op != SQLITE_INDEX_CONSTRAINT_EQ) {
          continue;
        }
      } else {
        std::vector<int> dd;
        if (pcoldef->indexOpsImplemented.find((ConstraintOp)constraint_info.op) == pcoldef->indexOpsImplemented.end()) {
          continue;
        }
      }

      // mark use

      if (pcoldef->options & REQUIRED) {
        numRequiredConstraints ++;
      }
      if (pcoldef->options & (INDEXED | ADDITIONAL)) {
        numIndexedConstraints ++;
      }

      // save constraint

      pIdxInfo->aConstraintUsage[i].argvIndex = ++xFilterArgvIndex;

      spContext->_constraint_infos.push_back({pcoldef->id, constraint_info.iColumn, constraint_info.iTermOffset, constraint_info.op});
    }
  }

  // track columns requested, so vtables can optimize out work if needed

  for (int i=0; i < td.columns.size(); i++) {
    const ColumnDef *pcoldef = &td.columns[i];

    // handle column aliases

    if (pcoldef->aliased) {
      int actualColIdx = getIndexOfColumn(pcoldef->aliased, td);
      if (actualColIdx < 0) {
        // TODO: log error
        continue;
      }
      pcoldef = &td.columns[actualColIdx];
    } else {

      // only count these for non-aliased columns

      if (pcoldef->options & (INDEXED | ADDITIONAL)) {
        numIndexedColumns++;
      }
      if (pcoldef->options & (REQUIRED)) {
        numRequiredColumns++;
      }
    }

    // track columns used

    if (pIdxInfo->colUsed & (1LL << i)) {

      spContext->_colsUsed.insert(pcoldef->id);
      //fprintf(stderr, "column used:'%s'\n", pcoldef->id->name.c_str());
    }
  }

  if (numRequiredConstraints < numRequiredColumns) {
    if (pVT->zErrMsg != nullptr) {
      sqlite3_free(pVT->zErrMsg);
    }
    pVT->zErrMsg = sqlite3_mprintf("required constraint missing");
    return SQLITE_CONSTRAINT;
  }

  pIdxInfo->idxNum = static_cast<int>(spContext->_idxNum);//   kConstraintIndexID++);
  pVT->_contexts.push_back(spContext);

  if (xFilterArgvIndex > 0) {
    pIdxInfo->estimatedCost = 10000;
  }
  return SQLITE_OK;
}

//----------------------------------------------------------------------
// convenience function
//----------------------------------------------------------------------
Constraint _makeConstraint(constraint_info_t &cinfo, sqlite3_value *val) {
  Constraint c;
  c.columnId = cinfo.columnId;
  c.op = (ConstraintOp)cinfo.op;
  getSqliteValue(val, c.value);

  return c;
}

//----------------------------------------------------------------------
// call table's next() and advance rowId if data
// sqlite will call xEof, which  checks for pVC->_row.empty()
// and if NOT xEof, then will call xColumn to get all
// columns
//----------------------------------------------------------------------
  static inline void advanceRow(my_vtab_cursor* pVC) {
  pVC->_row.clear();
    TRACE fprintf(stderr, "calling next() idxNum:%d _rowId:%llu\n", pVC->_context->_idxNum, pVC->_pvt->_rowId);
    if (pVC->_pvt->_implementation->next(std::static_pointer_cast<QueryContext>(pVC->_context), pVC->_row) && !pVC->_row.empty()) {
    pVC->_pvt->_rowId++;
  }
}
#define MAX_CONTEXT_BACKLOG 20
//----------------------------------------------------------------------
// Called after xBestIndex, but not always?
// TODO: cleanup spContext that are created in xBestIndex, but
// sqlite chooses not to use (and then xFilter not called).
//----------------------------------------------------------------------
static int xFilter(sqlite3_vtab_cursor* psvCur,
                   int idxNum,
                   const char* idxStr,
                   int argc,
                   sqlite3_value** argv) {
  auto pVC = (my_vtab_cursor*)psvCur;
  auto pVT = pVC->_pvt;

  TRACE fprintf(stderr, "xFilter idxNum:%d argc:%d\n", idxNum, argc);
  std::shared_ptr<QueryContextImpl> spContext;

  // find matching context setup in xBestIndex
  // while we are at it, erase old contexts

  for (auto it = pVT->_contexts.begin(); it != pVT->_contexts.end();) {

    // erase 'old' context, based on idxNum.
    // How many can be in-flight at same time?
    // Can have several for a given query or subquery.

    if ((*it)->_idxNum < (idxNum - MAX_CONTEXT_BACKLOG)) {
      pVT->_contexts.erase(it++);
      continue;
    }

    if ((*it)->_idxNum == idxNum) {
      spContext = *it;
      break;
    }

    it++;
  }

  // did we find it?

  if (nullptr == spContext) {
    fprintf(stderr, "state ERROR : context not found in xFilter idxNum:%d\n", idxNum);
    return SQLITE_ERROR;
  }

  // xFilter will be called multiple times for OP_EQ indexes
  // with the same cursor, and thus the same Constraints state.
  // so clear constraints. each time.
  spContext->_constraints.clear();

  // add filter constraints to context

  if (argc > 0 && !spContext->_constraint_infos.empty()) {
    if (argc > spContext->_constraint_infos.size()) {
      // not good
    } else {
      for (int i=0; i < argc; i++) {
        TRACE fprintf(stderr, "     argv[%d]='%s'\n", i, sqlite3_value_text(argv[i]));
        spContext->_constraints.push_back(_makeConstraint(spContext->_constraint_infos[i], argv[i]));
      }
    }
  }

  // call vtable's prepare
  pVC->_context = spContext;
  pVC->_pvt->_implementation->prepare(spContext);

  // get first row, if there is one.

  advanceRow(pVC);

  return SQLITE_OK;
}

//----------------------------------------------------------------------
// advance cursor to next row
//----------------------------------------------------------------------
static int xNext(sqlite3_vtab_cursor* psvCur) {
  auto pVC = (my_vtab_cursor*)psvCur;

  advanceRow(pVC);

  return SQLITE_OK;
}

//----------------------------------------------------------------------
// to preserve types, a callback mechanism is used.
// for each column (0 ... n) call the sqlite3_result_$type()
// or sqlite3_result_null() or sqlite3_result_error()
//----------------------------------------------------------------------
static int xColumn(sqlite3_vtab_cursor* psvCur, sqlite3_context* ctx, int col) {
  auto pVC = (my_vtab_cursor*)psvCur;

  const TableDef &tableDef = pVC->_pvt->_implementation->getTableDef();
  if (col < 0 || col >= tableDef.columns.size()) {
    return SQLITE_ERROR;
  }

  // handle column alias

  if (tableDef.columns[col].aliased) {
    int actualColIdx = getIndexOfColumn(tableDef.columns[col].aliased, tableDef);
    if (actualColIdx < 0) {
      sqlite3_result_error(ctx, "unable to find column alias", -1);
      return SQLITE_ERROR;
    }
    col = actualColIdx;
  }

  const ColumnDef &colDef = tableDef.columns[col];

  auto fit = pVC->_row.find(colDef.id);
  if (fit == pVC->_row.end()) {
    sqlite3_result_null(ctx);
    return SQLITE_OK;
  }
  DynVal &val = fit->second;

  switch(val.type()) {
    case TSTRING:
      sqlite3_result_text(ctx, val.as_s().c_str(), -1, SQLITE_TRANSIENT);
      break;
    case TFLOAT32:
    case TFLOAT64:
      sqlite3_result_double(ctx, (double)val);
      break;
    case TINT32:
    case TUINT32:
    case TINT16:
    case TUINT16:
    case TINT8:
    case TUINT8:
      sqlite3_result_int(ctx, (int32_t)val);
      break;
    case TINT64:
    case TUINT64:
      sqlite3_result_int64(ctx, (int64_t)val);
      break;
    case TBYTES:
      assert(false);
    default:
      sqlite3_result_null(ctx);
  }

  return SQLITE_OK;
}

//----------------------------------------------------------------------
// is this end of rows for this cursor?
//----------------------------------------------------------------------
static int xEof(sqlite3_vtab_cursor* psvCur) {
  auto pVC = (my_vtab_cursor*)psvCur;

  return pVC->_row.empty();
}

//----------------------------------------------------------------------
// close cursor
//----------------------------------------------------------------------
int xClose(sqlite3_vtab_cursor* psvCur) {
  auto pVC = (my_vtab_cursor*)psvCur;
  //plan("Closing cursor (" + std::to_string(pCur->id) + ")");
  delete pVC;
  return SQLITE_OK;
}

//----------------------------------------------------------------
//
//----------------------------------------------------------------
int xDisconnect(sqlite3_vtab* psvTab) {
  auto pVT = (my_vtab*)psvTab;
  delete pVT;
  return SQLITE_OK;
}

int xRowid(sqlite3_vtab_cursor* psvCur, sqlite_int64* pRowid) {
  auto pVC = (my_vtab_cursor*)psvCur;
  *pRowid = pVC->_pvt->_rowId;
  return SQLITE_OK;
}

//----------------------------------------------------------------
// return pointer to singleton sqlite3_module for normal
// read-only virtual tables.
// https://sqlite.org/vtab.html
//----------------------------------------------------------------
static sqlite3_module *getReadOnlyTableModule() {
  static sqlite3_module _module = { };
  if (nullptr == _module.xFilter) {
    // initialize with function entrypoints
    _module.xBestIndex = xBestIndex;
    _module.xOpen = xOpen;
    _module.xClose = xClose;
    _module.xFilter = xFilter;
    _module.xNext = xNext;
    _module.xEof = xEof;
    _module.xColumn = xColumn;
    _module.xRowid = xRowid;

    // eponymous : make table available in all connections
    // without need for xCreate.  Use xConnect == xCreate
    // or xCreate is null.

    _module.xConnect = xConnect;
    _module.xDisconnect = xDisconnect;
    _module.xCreate = xConnect; //xCreate;
    _module.xDestroy = xDisconnect; //xDestroy;
  }
  return &_module;
}

void VSQLiteImpl::remove(SPVirtualTable spVirtualTable) {
  // remove from _tables list

  for (auto it = _tables.begin(); it != _tables.end(); it++) {
    if ((*it)->getTableDef().schemaId == spVirtualTable->getTableDef().schemaId) {
      _tables.erase(it);
      break;
    }
  }

  // drop table

  auto format = "DROP TABLE IF EXISTS temp." + spVirtualTable->getTableDef().schemaId->name;
  int rc = sqlite3_exec(_db, format.c_str(), nullptr, nullptr, 0);
  if (rc != SQLITE_OK) {
    //TODO: "Error detaching table: " << name << " (" << rc << ")";
  }
  
  auto sql = "DROP VIRTUAL TABLE temp." + spVirtualTable->getTableDef().schemaId->name;
  rc = sqlite3_exec(_db, sql.c_str(), nullptr, nullptr, 0);

}

  static std::string columnTypeName(DynType t) {
    switch (t) {
      case TFLOAT32:
      case TFLOAT64:
        return "FLOAT";
      case TINT8:
      case TUINT8:
      case TINT16:
      case TUINT16:
      case TINT32:
      case TUINT32:
        return "INTEGER";
      case TINT64:
        return "BIGINT";
      case TUINT64:
        return "UNSIGNED BIGINT";
      case TBYTES:
        return "BLOB";
      case TNONE:
        return "UNKNOWN"; // alias columns?
      case TSTRING:
      default:
        return "TEXT";
    }
  }


//----------------------------------------------------------------------
// createStatement
// builds the body of sql 'CREATE STATEMENT'
// based on TableDef.  Includes all columns, indexes
//----------------------------------------------------------------------
std::string createStatement(const TableDef &td) {
  std::map<std::string, bool> epilog;
  bool indexed = false;
  std::vector<std::string> pkeys;

  std::string statement = "(";
  int i = -1;
  while (++i < (int)td.columns.size()) {
    if (i > 0) { statement += ", "; }

    const auto& column = td.columns.at(i);
    statement += '`' + column.id->name + "` ";
    statement += columnTypeName(column.id->typeId);
    auto& options = column.options;
    if (options & (ColOpt::INDEXED | ColOpt::ADDITIONAL)) {
      if (options & ColOpt::INDEXED) {
        indexed = true;
      }
      pkeys.push_back(column.id->name);
      epilog["WITHOUT ROWID"] = true;
    }
    if (options & (ColOpt::HIDDEN | ColOpt::ALIAS )) {
      statement += " HIDDEN";
    }
  }

  // If there are only 'additional' columns (rare), do not attempt a pkey.
  if (!indexed) {
    epilog["WITHOUT ROWID"] = false;
    pkeys.clear();
  }

  // Append the primary keys, if any were defined.
  if (!pkeys.empty()) {
    statement += ", PRIMARY KEY (";
    for (auto pkey = pkeys.begin(); pkey != pkeys.end();) {
      statement += '`' + std::move(*pkey) + '`';
      if (++pkey != pkeys.end()) {
        statement += ", ";
      }
    }
    statement += ')';
  }

  statement += ')';

  for (auto& ei : epilog) {
    if (ei.second) {
      statement += ' ' + std::move(ei.first);
    }
  }

  return statement;
}


//----------------------------------------------------------------
// Add table
//----------------------------------------------------------------
int VSQLiteImpl::add(SPVirtualTable spVirtualTable) {
  if (spVirtualTable == nullptr) {
    return true;
  }

  // already added?

  for (auto &item : _tables) {
    if (item->getTableDef().schemaId->name == spVirtualTable->getTableDef().schemaId->name) {
      return 0;
    }
  }

  auto &tableDef = spVirtualTable->getTableDef();
  auto tableName = tableDef.schemaId->name;
  int rc = sqlite3_create_module(
      _db, tableName.c_str(), getReadOnlyTableModule(), (void*)spVirtualTable.get());

  if (rc == SQLITE_OK || rc == SQLITE_MISUSE) {
    auto sql =
    "CREATE VIRTUAL TABLE temp." + tableName + " USING " + tableName;

    rc =
        sqlite3_exec(_db, sql.c_str(), nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
      fprintf(stderr, "sqlite3_exec failed with %d (%s): '%s'\n", rc,
             sqlite3_errmsg(_db),
             sql.c_str());

      return rc;
    }

  } else {
    assert(false);
    //LOG(ERROR) << "Error attaching table: " << tableName << " (" << rc << ")";
    return rc;
  }

  this->_tables.push_back(spVirtualTable);

  return 0;
}

} // namespace vsqlite
