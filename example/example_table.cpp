#include <vsqlite/vsqlite_tables.h>

using namespace vsqlite;

static const SPFieldDef FUSERNAME = FieldDef::alloc(TSTRING, "username");
static const SPFieldDef FUSERID = FieldDef::alloc(TUINT32, "userid");
static const SPFieldDef FUID_ALIAS = FieldDef::alloc(TNONE, "uid");
static const SPFieldDef FHOME = FieldDef::alloc(TSTRING, "home");

struct RawData {
  int id;
  std::string name;
};

// pretend operating system calls to get data needed for table

static void os_enum_users(std::vector<RawData> &dest);
static int os_get_user_by_id(uint32_t userid, RawData &dest);
static std::string os_get_user_home(uint32_t userid);

// 'users' table implementation

class MyUsersTable : public VirtualTable {
public:
  MyUsersTable() {
  }
  virtual ~MyUsersTable() {}

  const TableDef &getTableDef() const override {
    static const TableDef def = {
      std::make_shared<SchemaId>("users"),
      {
        {FUSERID, ColOpt::INDEXED, "ID of the user", 0, { OP_EQ, OP_LIKE }}
        ,{FUSERNAME, 0, "username"}
        ,{FHOME, 0, "home"}
        ,{FUID_ALIAS, ColOpt::ALIAS, "", FUSERID}
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
  void prepare(SPQueryContext context) override {

    // reset state

    _idx = 0;
    _data.clear();
    _wantedUserids.clear();
    _wantsHome = true;

    // gather filter constraints

    for (auto constraint : context->getConstraints()) {
      if (constraint.columnId == FUSERID && constraint.op == OP_EQ) {
        _wantedUserids.insert(constraint.value);
      }
    }

    // optimize: check which columns are being requested

    if (context->getRequestedColumns().count(FHOME) == 0) {
      _wantsHome = false;
    }

    // filter our data
    if (_wantedUserids.empty()) {
      os_enum_users(_data);
    } else {
      for (auto userid : _wantedUserids) {
        RawData tmp;
        if (0 == os_get_user_by_id(userid, tmp)) {
          _data.push_back(tmp);
        }
      }
    }

    return 0;
  }

  bool next(DynMap &row) override {
    while (_idx < _data.size()) {
      RawData &pData = _data[_idx++];


      row[FUSERID] = pData.id;
      row[FUSERNAME] = pData.name;

      // optimize : if 'home' column not in query, no need to call.

      if (_wantsHome) {
        row[FHOME] = os_get_user_home(pData.id);
      }

      return true;
    }

    return false;
  }

private:
  std::vector<RawData> _data;
  std::set<uint32_t> _wantedUserids;
  size_t _idx;
  bool _wantsHome;
};

SPVirtualTable newMyUsersTable() { return std::make_shared<MyUsersTable>(); }


// super-secret internals of fake os calls

static std::vector<RawData> _gOsPrivateFakeUserData {
  {0, "root"}
  ,{500, "bob"}
  ,{501, "salamanca"}
  ,{502, "jimmy"}
};

static void os_enum_users(std::vector<RawData> &dest) {

  for (auto &item : _gOsPrivateFakeUserData) {
    dest.push_back(item);
  }
}

static int os_get_user_by_id(uint32_t userid, RawData &dest) {
  for (size_t i = 0; i < _gOsPrivateFakeUserData.size(); i++) {
    if (_gOsPrivateFakeUserData[i].id == userid) {
      dest = _gOsPrivateFakeUserData[i];
      return 0;
    }
  }
  return 1;
}

static std::string os_get_user_home(uint32_t userid) {
  if (userid == 0) {
    return "/root";
  }
  RawData tmp;
  if (os_get_user_by_id(userid, tmp)) {
    return "";
  }
  return "/home/" + tmp.name;
}
