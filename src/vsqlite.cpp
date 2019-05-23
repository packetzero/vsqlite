
#include "vsqlite_impl.h"

#define TRACE if (0)

namespace vsqlite {
  int VSQLiteImpl::query(const std::string sql, QueryListener &listener /*std::vector<DynMap> &results*/) {

      sqlite3_stmt *pStmt = nullptr;
      int rv = sqlite3_prepare_v2(_db, sql.c_str(), sql.size(), &pStmt, nullptr);
      if (rv != SQLITE_OK) {
        listener.onQueryError(sqlite3_errstr(rv));
//        listener.onQueryError(sqlite3_errmsg(_db));
        return -1;
      }

      std::vector<SPFieldDef> columns;

      while (true) {
        rv = sqlite3_step(pStmt);
        TRACE fprintf(stderr, "step rv=%d\n", rv);
        if (rv == SQLITE_DONE || sqlite3_data_count(pStmt) == 0) { break; }
        if (rv == SQLITE_ROW) {
          if (columns.empty()) {
            if (_populateColumns(pStmt, columns)) {
              break; // ERROR
            }
          }

          DynMap row;
          if (_populateRow(pStmt, columns, row)) {
            // empty?
          } else {
            if (listener.onResultRow(row)) {
              // listener wants us to abort
              break;
            }
          }


        } else {
          // error
          break;
        }
      }

      sqlite3_finalize(pStmt);

      return 0;
    }

 //----------------------------------------------------------------------
 // populates DynVal with typed value from 'val'
 //----------------------------------------------------------------------
 void getSqliteValue(sqlite3_value *val, DynVal &dest) {

   auto sqltype = sqlite3_value_type(val);
   if (sqltype == SQLITE_NULL) {
     return;
   }
   switch(sqltype) {
     case SQLITE_INTEGER:
       dest = sqlite3_value_int(val);
       break;
     case SQLITE_FLOAT:
       dest = sqlite3_value_double(val);
       break;
     case SQLITE_TEXT:
       dest = (const char *)(sqlite3_value_text(val));
       break;
     case SQLITE_BLOB:
     default:
       assert(false); // TODO
       break;
   }
 }

 //----------------------------------------------------------------------
 // This is a static function that does argument checks before
 // invoking the AppFunction.func()
 //----------------------------------------------------------------------
  static void _funcWrapper(sqlite3_context* context,int argc,sqlite3_value** argv) {
    AppFunction* pFunc = (AppFunction*)sqlite3_user_data(context);
    if (nullptr == pFunc) {
      sqlite3_result_error(context, "App user_data missing", 0);
      return;
    }
    auto &argdefs = pFunc->expectedArgs();
    if (argdefs.size() != argc) {
      sqlite3_result_error(context, "number of arguments does not match expected", argc);
      return;
    }

    // get args
    std::vector<DynVal> argvals;

    for (int i=0; i < argc; i++) {
      argvals.push_back(DynVal());

      getSqliteValue(argv[i], argvals[i]);

      // convert to expected type

      if (TNONE != argdefs[i] && argdefs[i] != argvals[i].type()) {
        DYN_TO(argvals[i], argdefs[i], argvals[i]);
      }
    }

    // call it

    std::string errmsg;
    DynVal retval = pFunc->func(argvals, errmsg);

    // report result. TODO: share this with table result reporting?

    if (!errmsg.empty()) {
      sqlite3_result_error(context, errmsg.c_str(), -1 /* strlen */);
    } else {
      switch(retval.type()) {
        case TNONE:
          sqlite3_result_null(context);
          break;
        case TUINT64:
        case TINT64:
        case TUINT32:
          sqlite3_result_int64(context, retval.as_i64());
          break;
        case TUINT8:
        case TINT8:
        case TUINT16:
        case TINT16:
        case TINT32:
          sqlite3_result_int(context, retval.as_i32());
          break;
        case TFLOAT32:
        case TFLOAT64:
          sqlite3_result_double(context, retval.as_double());
          break;
        case TSTRING: {
          std::string s = retval.as_s();
          sqlite3_result_text(context, s.c_str(), s.size(), SQLITE_TRANSIENT);
          break;
        }
        case TBYTES:
          // TODO
        default:
          assert(false);
          sqlite3_result_null(context);
      }
    }
  }

  //----------------------------------------------------------------------
  // add custom function
  //----------------------------------------------------------------------
    bool VSQLiteImpl::add(SPAppFunction spFunction) {

      if (nullptr == spFunction) { return true; }

      int rv = sqlite3_create_function(_db,
                          spFunction->name().c_str(),
                          spFunction->expectedArgs().size(),
                          SQLITE_UTF8 | SQLITE_DETERMINISTIC,
                          spFunction.get(),
                          _funcWrapper,
                          nullptr,
                          nullptr);
      if (rv != SQLITE_OK) {
        return true;
      }

      _funcs.push_back(spFunction);

      return false;
    }

    //----------------------------------------------------------------------
    // remove function
    //----------------------------------------------------------------------
    void VSQLiteImpl::remove(SPAppFunction spFunction) {
      // TODO

    }

    //--------------------------------------------------------------------
    // https://sqlite.org/c3ref/c_blob.html
    //--------------------------------------------------------------------
    static inline DynType toDynType(int sqliteType) {
      switch(sqliteType) {
        case SQLITE_INTEGER: return TINT64;
        case SQLITE_FLOAT: return TFLOAT64;
        case SQLITE_BLOB: return TBYTES;
        case SQLITE_NULL: return TNONE;
        default:
        case SQLITE_TEXT: return TSTRING;
      }
    }

    //--------------------------------------------------------------------
    // fills in row data
    //--------------------------------------------------------------------
    bool VSQLiteImpl::_populateRow(sqlite3_stmt *pStmt, std::vector<SPFieldDef> &columns, DynMap &row) {
      if (columns.size() != sqlite3_data_count(pStmt)) {
        return true;
      }
      for (int i=0; i < columns.size(); i++) {
        SPFieldDef column = columns[i];
        switch(column->typeId) {
          case TINT64:
            row[column] = sqlite3_column_int64(pStmt, i);
            break;
          case TFLOAT64:
            row[column] = sqlite3_column_double(pStmt, i);
            break;
          case TSTRING:{
            std::string val;
            val.assign((const char *)sqlite3_column_text(pStmt, i), sqlite3_column_bytes(pStmt, i));
            row[column] = val;
            break;
          }
          case TBYTES: {
            std::vector<uint8_t> blob;
            auto p = sqlite3_column_blob(pStmt, i);
            blob.resize(sqlite3_column_bytes(pStmt, i));
            memcpy(blob.data(), p, blob.size());
            row[column] = DynVal(blob);
            break;
          }
          case TNONE:
          default:
            row[column] = DynVal();
        }
      }
      return false;
    }

  //--------------------------------------------------------------------
  // fills in column names and types using sqlite3_ accessors
  // returns true on error or columns.empty()
  //--------------------------------------------------------------------
  bool VSQLiteImpl::_populateColumns(sqlite3_stmt *pStmt, std::vector<SPFieldDef> &columns) {
    for (int i=0; i < sqlite3_column_count(pStmt); i++) {
      int sqliteType = sqlite3_column_type(pStmt, i);
      std::string name;
      const char *pname = sqlite3_column_name(pStmt, i);
      if (nullptr != pname) { name = std::string(pname); }
      SPFieldDef fieldId = FieldDef::alloc(toDynType(sqliteType), name);
      columns.push_back(fieldId);
    }
    return columns.empty();
  }

  static std::shared_ptr<VSQLiteImpl> gInstance;

  SPVSQLite VSQLiteInstance() {
    if (nullptr == gInstance) {
      gInstance = std::make_shared<VSQLiteImpl>();
    }
    return gInstance;
  }

} // namespace
