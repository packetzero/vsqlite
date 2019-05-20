#include <vsqlite/vsqlite.h>
#include <vsqlite/vsqlite_tables.h>

using namespace vsqlite;

static const SPSchemaId SCHEMA_USERS = std::make_shared<SchemaId>("users");

static const SPFieldDef FUSERNAME = FieldDef::alloc(TSTRING, "username", SCHEMA_USERS);
static const SPFieldDef FUSERID = FieldDef::alloc(TUINT32, "userid", SCHEMA_USERS);

struct RawData {
  int id;
  std::string name;
};

class MyUsersTable : public VirtualTable {
public:
  MyUsersTable() {
  }
  virtual ~MyUsersTable() {}

  const TableDef &getTableDef() const override {
    static const TableDef def = {
      SCHEMA_USERS,
      {}, // table_aliases
      {
        {FUSERID, ColOpt::INDEXED, "ID of the user"},
        {FUSERNAME, 0, "username"}
      },
      { {"uid", "userid"} }, // column aliases
      { } // table_attrs
    };
    return def;
  }
  
  

  /**
   * query virtual table
   */
  int prepare(SPQueryContext context) override {
    // here you would gather:
    //  - index constraints, such as userids requested
    //  - check which columns are being requested
    _data = {
      { 500, "bob"}
      ,{0, "root"}
    };

    return 0;
  }
  int next(DynMap &row, uint64_t rowId) override {
    if (rowId < 0 || rowId >= _data.size()) {
      return 1;
    }

    RawData &pData = _data[rowId];
    row[FUSERID] = pData.id;
    row[FUSERNAME] = pData.name;

    return 0;
  }
private:
  std::vector<RawData> _data;
};

SPVirtualTable newMyUsersTable() { return std::make_shared<MyUsersTable>(); }
