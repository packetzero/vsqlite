
#include "../include/vsqlite/vsqlite.h"

// A test table ("tprocesses") with schema similar to osql.processes

class TProcessTable : public vsqlite::VirtualTable {
public:
  struct RawData {
    uint32_t pid;
    std::string path;
  };
  
  struct MyState {
    std::vector<RawData> _data;
    bool _wantsAllFields {true};
    size_t _idx;
  };

  std::map<uint32_t,RawData> _indexedData;
  TProcessTable() {
    for (auto &item : getRawData()) {
      _indexedData[item.pid] = item;
    }
  }
  virtual ~TProcessTable() {}

  const SPFieldDef FPATH = FieldDef::alloc(TSTRING, "path");
  const SPFieldDef FPID = FieldDef::alloc(TINT32, "pid");

  const vsqlite::TableDef &getTableDef() const override {
    static const vsqlite::TableDef def = {
       std::make_shared<SchemaId>("tprocess"),
      {
        {FPID, vsqlite::ColOpt::INDEXED, ""}
        ,{FPATH, 0, ""}
      },
      { } // table_attrs
    };
    return def;
  }

  /**
   */
  void prepare(vsqlite::SPQueryContext context) override {
    _num_prepare_calls++;

    // make state
    
    auto spState = std::make_shared<MyState>();
    context->setUserData(spState);

    if (context->getConstraints().empty()) {
      for (auto &item : getRawData()) {
        spState->_data.push_back(item);
      }
    } else {
      // gather index constraints for filtering data

      for (auto constraint : context->getConstraints()) {

        if (constraint.columnId == FPID && constraint.op == vsqlite::OP_EQ) {

          uint32_t pid = constraint.value;

          auto fit = _indexedData.find(pid);
          if (fit != _indexedData.end()) {
            spState->_data.push_back(fit->second);
          }

          _num_index_constraints++;
        }
      }
    }
  }

  bool next(vsqlite::SPQueryContext context, DynMap &row) override {
    _num_next_calls++;

    std::shared_ptr<MyState> spState = std::static_pointer_cast<MyState>(context->getUserData());

    if (spState->_idx < spState->_data.size()) {
      auto &item = spState->_data[spState->_idx++];
      row[FPATH] = item.path;
      row[FPID] = item.pid;
      return true;
    }

    return false;
  }

  void reset() {
    _num_next_calls = 0;
    _num_prepare_calls = 0;
    _num_index_constraints = 0;
  }

  static const std::vector<RawData> &getRawData() {
    
    static const std::vector<RawData> _gRawData {
      // pid,path
      {1,"/sbin/launchd"}
      ,{42,"/usr/sbin/syslogd"}
      ,{40000,"/bin/bash"}
      ,{41111,"/usr/bin/base64"}
      ,{44444,"/usr/local/bin/exiftool"}
    };
    return _gRawData;
  }

  uint32_t _num_prepare_calls {0};
  uint32_t _num_next_calls {0};
  uint32_t _num_index_constraints {0};
};

