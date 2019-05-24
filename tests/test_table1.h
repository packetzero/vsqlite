
#include "../include/vsqlite/vsqlite.h"


class T1Table : public vsqlite::VirtualTable {
public:
  struct RawData {
    std::string name;
    uint32_t u32val;
    double dval;
    int64_t i64val;
    bool active;
  };
  
  struct MyState {
    std::vector<RawData> _data;
    std::set<uint32_t> _wantedIds;
    bool _wantsAllFields {true};
    size_t _idx;
  };

  T1Table() {
  }
  virtual ~T1Table() {}

  const SPFieldDef FNAME = FieldDef::alloc(TSTRING, "name");
  const SPFieldDef FU32VAL = FieldDef::alloc(TUINT32, "u32val");
  const SPFieldDef FDVAL = FieldDef::alloc(TFLOAT64, "dval");
  const SPFieldDef FLONGO = FieldDef::alloc(TINT64, "longo");
  const SPFieldDef FACTIVE = FieldDef::alloc(TUINT8, "is_active");

  const SPFieldDef FU32VAL_ALIAS = FieldDef::alloc(TNONE, "dword");

  const vsqlite::TableDef &getTableDef() const override {
    static const vsqlite::TableDef def = {
       std::make_shared<SchemaId>("t1"),
      {
        {FU32VAL, vsqlite::ColOpt::INDEXED, "", 0, { vsqlite::OP_EQ }}
        ,{FNAME, vsqlite::ColOpt::INDEXED, "",0,{vsqlite::OP_EQ}} // advertises index, but doesn't implement it
        ,{FDVAL, 0, ""}
        ,{FLONGO, 0, ""}
        ,{FACTIVE, 0, ""}
        ,{FU32VAL_ALIAS, vsqlite::ColOpt::ALIAS, "", FU32VAL}
      },
      { } // table_attrs
    };
    return def;
  }

  /**
   * If index is used, prepare will be called once for each value
   * with the OP_EQ constraint.
   * For example if 'WHERE userid IN (2,7,9,11)' then prepare() will
   * be called 4 times.
   * If there are no index constraints, prepare() will be called
   * once, then calls to next() until it returns 1;
   */
  void prepare(vsqlite::SPQueryContext context) override {
    _num_prepare_calls++;

    // make state
    
    auto spState = std::make_shared<MyState>();
    context->setUserData(spState);

    // gather filter constraints

    for (auto constraint : context->getConstraints()) {
      if (constraint.columnId == FU32VAL && constraint.op == vsqlite::OP_EQ) {
        spState->_wantedIds.insert(constraint.value);
        _num_index_constraints++;
      }
    }

    // optimize: check which columns are being requested

    if (context->getRequestedColumns().count(FLONGO) == 0) {
      spState->_wantsAllFields = false;
    }

    // filter our data
    if (spState->_wantedIds.empty()) {
      get_all_data(spState->_data);
    } else {
      for (auto id : spState->_wantedIds) {
        RawData tmp;
        if (0 == filter_data_by_u32val(id, tmp)) {
          spState->_data.push_back(tmp);
        }
      }
    }
  }

  bool next(vsqlite::SPQueryContext context, DynMap &row) override {
    _num_next_calls++;
    
    std::shared_ptr<MyState> spState = std::static_pointer_cast<MyState>(context->getUserData());

    while (spState->_idx < spState->_data.size()) {
      RawData &pData = spState->_data[spState->_idx++];


      row[FU32VAL] = pData.u32val;
      row[FNAME] = pData.name;
      row[FDVAL] = pData.dval;
      row[FACTIVE] = pData.active;

      if (spState->_wantsAllFields) {
        row[FLONGO] = pData.i64val;
      }

      return true;
    }
  
    return false;
  }

  static const std::vector<RawData> &getRawData() {

    static const std::vector<RawData> _gRawData {
      // name, u32val, dval, i64val, active
      {"alpha", 0xaaaa, 0.123, 555444333222111L, true }
      ,{"beta", 0xbbbb, 1.1, 111222333444555L, true }
      ,{"charlie", 0xcccc, 2.2, -555444333222111L, false }
      ,{"delta", 0xdddd, 3.33, -111222333444555L, true }
    };
    return _gRawData;
  }

  // allow unit tests to reset counters

  void reset() {
    _num_next_calls = 0;
    _num_prepare_calls = 0;
    _num_index_constraints = 0;
  }

  uint32_t _num_prepare_calls {0};
  uint32_t _num_next_calls {0};
  uint32_t _num_index_constraints {0};

private:

  void get_all_data(std::vector<RawData> &dest) {

    for (auto &item : getRawData()) {
      dest.push_back(item);
    }
  }

  int filter_data_by_u32val(uint32_t u32val, RawData &dest) {
    for (size_t i = 0; i < getRawData().size(); i++) {
      if (getRawData()[i].u32val == u32val) {
        dest = getRawData()[i];
        return 0;
      }
    }
    return 1;
  }

};
