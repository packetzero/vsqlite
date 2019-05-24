
#include "../include/vsqlite/vsqlite.h"


class T2RequiredTable : public vsqlite::VirtualTable {
public:

  T2RequiredTable() {
  }
  virtual ~T2RequiredTable() {}

  const SPFieldDef FPATH = FieldDef::alloc(TSTRING, "path");
  const SPFieldDef FPATHLEN = FieldDef::alloc(TUINT32, "pathlen");

  const vsqlite::TableDef &getTableDef() const override {
    static const vsqlite::TableDef def = {
       std::make_shared<SchemaId>("tpath_len"),
      {
        {FPATH, vsqlite::ColOpt::REQUIRED, "",0,{vsqlite::OP_EQ, vsqlite::OP_LIKE
          //, vsqlite::OP_GE, vsqlite::OP_LT

        }}
        ,{FPATHLEN, 0, ""}
      },
      { } // table_attrs
    };
    return def;
  }

  struct MyState {
    std::vector<std::string> _data;
    size_t _idx;
  };

  /**
   */
  void prepare(vsqlite::SPQueryContext context) override {
    _num_prepare_calls++;

    // make state
    
    auto spState = std::make_shared<MyState>();
    context->setUserData(spState);

    // gather index constraints for filtering data

    // NOTE: sqlite will filter out results that don't meet criteria.
    // so if path LIKE '/home/%' and we include '/var/log/' in the results
    // it will get removed from final query results by sqlite.

    // NOTE: OP_EQ is a special case, where sqlite will invoke xFilter,xNext
    // for each value.  OP_LIKE is more vague, and will cause xFilter for
    // each LIKE term and you return multiple results.  It's also a best-effort
    // case rather than OP_EQ, which should be exact lookup.

    for (auto constraint : context->getConstraints()) {

      if (constraint.columnId == FPATH && constraint.op == vsqlite::OP_EQ) {

        std::string path = constraint.value;
        spState->_data.push_back(path);
        _num_index_constraints++;

      } else if (constraint.columnId == FPATH && constraint.op == vsqlite::OP_LIKE) {

        std::string pat = constraint.value;
        auto pos = pat.find('%');
        if (pos != std::string::npos) {

          // find items that start with first part of constraint value
          // this is NOT the correct implementation of LIKE
          // in xBestIndex, OP_LIKE on strings will be accompanied by
          // OP_GE and OP_LT

          std::string prefix = pat.substr(0,pos);
          for (const std::string &item : rawData()) {
            if (item.find(prefix) == 0) {
              spState->_data.push_back(item);
            }
          }
          _num_index_constraints++;
        }
      } else if (constraint.columnId == FPATH &&
                 (constraint.op == vsqlite::OP_GE || constraint.op == vsqlite::OP_LT)) {

        // if original term is '/dev/%/pci0' constraints are:
        //  >= '/DEV/'    (notice upper case)
        //  < '/dev0'     ('0' is ascii '/' + 1)
        //  check should be:
        //    path >= '/DEV/' AND path < '/dev0'
        // NOTICE: the part after the '%' is not provided, so you should use
        //  GE and LT in conjunction with LIKE

        //_data.push_back(constraint.value.as_s() + "_" + std::to_string((int)constraint.op));
      }
    }
  }

  bool next(vsqlite::SPQueryContext context, DynMap &row) override {
    _num_next_calls++;

    std::shared_ptr<MyState> spState = std::static_pointer_cast<MyState>(context->getUserData());

    while (spState->_idx < spState->_data.size()) {
      std::string path = spState->_data[spState->_idx++];
      row[FPATH] = path;
      row[FPATHLEN] = (uint32_t)path.size();
      return true;
    }

    return false;
  }

  void reset() {
    _num_next_calls = 0;
    _num_prepare_calls = 0;
    _num_index_constraints = 0;
  }

  static std::set<std::string>& rawData() {
    static std::set<std::string> _rawData = {
      "/dev/fd0"
      ,"/dev/pci0"
      ,"/dev/usb0"
      ,"/dev/en0"
      ,"/dev/en1"
      ,"/dev/pci1"
      ,"/dev/ttys001"
      ,"/home/bob"
      ,"/home/elaine"
      ,"/root"
      ,"/home/staff"
    };
    return _rawData;
  }

  uint32_t _num_prepare_calls {0};
  uint32_t _num_next_calls {0};
  uint32_t _num_index_constraints {0};
};
