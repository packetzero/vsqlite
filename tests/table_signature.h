
#include "../include/vsqlite/vsqlite.h"

// A test table ("tsig") with schema similar to osql.signature

class TSignatureTable : public vsqlite::VirtualTable {
public:
  struct RawData {
    std::string path;
    uint32_t issigned;
    std::string cdhash;
    std::string identifier;
  };
  
  struct MyState {
    std::vector<RawData> _data;
    bool _wantsAllFields {true};
    size_t _idx;
  };

  std::map<std::string,RawData> _indexedData;
  TSignatureTable() {
    for (auto &item : getRawData()) {
      _indexedData[item.path] = item;
    }
  }
  virtual ~TSignatureTable() {}

  const SPFieldDef FPATH = FieldDef::alloc(TSTRING, "path");
  const SPFieldDef FSIGNED = FieldDef::alloc(TINT32, "signed");

  const SPFieldDef FCDHASH = FieldDef::alloc(TSTRING, "cdhash");
  const SPFieldDef FIDENTIFIER = FieldDef::alloc(TSTRING, "identifier");

  const vsqlite::TableDef &getTableDef() const override {
    static const vsqlite::TableDef def = {
       std::make_shared<SchemaId>("tsig"),
      {
        {FPATH, vsqlite::ColOpt::REQUIRED, "",0,{vsqlite::OP_EQ, vsqlite::OP_LIKE }}
        ,{FSIGNED, 0, ""}
        ,{FCDHASH, 0, ""}
        ,{FIDENTIFIER, 0, ""}
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

    // gather index constraints for filtering data
    // path is REQUIRED

    for (auto constraint : context->getConstraints()) {

      if (constraint.columnId == FPATH && constraint.op == vsqlite::OP_EQ) {

        std::string path = constraint.value;

        auto fit = _indexedData.find(path);
        if (fit != _indexedData.end()) {
          spState->_data.push_back(fit->second);
        }

        _num_index_constraints++;

      } else if (constraint.columnId == FPATH && constraint.op == vsqlite::OP_LIKE) {

        std::string pat = constraint.value;
        auto pos = pat.find('%');
        if (pos != std::string::npos) {

          std::string prefix = pat.substr(0,pos);
          for (const auto &item : getRawData()) {
            if (item.path.find(prefix) == 0) {
              spState->_data.push_back(item);
            }
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
      row[FSIGNED] = item.issigned;
      row[FIDENTIFIER] = item.identifier;
      row[FCDHASH] = item.cdhash; // TODO: only if requested
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
      // path, signed, cdhash, identifier
      {"/usr/bin/base64", 1, "e264d46afea7e77cbd81eca5ad245a3bcae99fe0", "com.apple.base64" }
      ,{"/usr/bin/bashbug", 0, "", "" }
      ,{"/usr/bin/binhex", 1, "c5c058f85e064bc4a9a6ef443a3a368ad6dc54ed","com.apple.binhex" }
      ,{"/usr/local/bin/hyperkit", 1, "51f09d57d4b0be63f7699cf420023adfa30baa3c", "com.docker" }
      ,{"/usr/local/bin/exiftool",0,"",""}
      ,{"/usr/local/bin/kubectl",1,"60b123cff9c66b6c5fd65ef48ea5a2431d4bae0d","kubectl"}
      ,{"/sbin/launchd",1,"3278354cbd9820dab665b077dab69b10cd38e64c","com.apple.xpc.launchd"}
    };
    return _gRawData;
  }

  uint32_t _num_prepare_calls {0};
  uint32_t _num_next_calls {0};
  uint32_t _num_index_constraints {0};
};

