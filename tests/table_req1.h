
#include "../include/vsqlite/vsqlite_tables.h"


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
        {FPATH, vsqlite::ColOpt::REQUIRED, "",0,{vsqlite::OP_EQ, vsqlite::OP_LIKE}}
        ,{FPATHLEN, 0, ""}
      },
      { } // table_attrs
    };
    return def;
  }

  /**
   */
  int prepare(vsqlite::SPQueryContext context) override {
    _num_prepare_calls++;

    // reset state

    _idx = 0;
    _data.clear();

    // gather filter constraints

    for (auto constraint : context->getConstraints()) {
      if (constraint.columnId == FPATH && constraint.op == vsqlite::OP_EQ) {
        std::string path = constraint.value;
        _data.push_back(path);
        _num_index_constraints++;
      } else if (constraint.columnId == FPATH && constraint.op == vsqlite::OP_LIKE) {
        std::string pat = constraint.value;
        auto pos = pat.find('%');
        if (pos != std::string::npos) {
          std::string prefix = pat.substr(0,pos);
          _data.push_back(prefix + "/one");
          _data.push_back(prefix + "/two");
          _data.push_back(prefix + "/three");
          _num_index_constraints++;
        } else {
          _data.push_back(pat);
          _num_index_constraints++;
        }
      }
    }

    return 0;
  }

  int next(DynMap &row, uint64_t rowId) override {
    _num_next_calls++;

    while (_idx < _data.size()) {
      std::string path = _data[_idx++];
      row[FPATH] = path;
      row[FPATHLEN] = (uint32_t)path.size();
      return 0;
    }

    return 1;
  }

  void reset() {
    _num_next_calls = 0;
    _num_prepare_calls = 0;
    _num_index_constraints = 0;
  }

  uint32_t _num_prepare_calls {0};
  uint32_t _num_next_calls {0};
  uint32_t _num_index_constraints {0};

private:

  std::vector<std::string> _data;
  size_t _idx;
};
