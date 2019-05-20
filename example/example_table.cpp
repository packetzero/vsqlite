#include <vsqlite/vsqlite.h>
#include <vsqlite/vsqlite_tables.h>

using namespace vsqlite;

static const SPSchemaId SCHEMA_USERS = std::make_shared<SchemaId>("users");

static const SPFieldDef FUSERNAME = FieldDef::alloc(TSTRING, "username", SCHEMA_USERS);
static const SPFieldDef FUSERID = FieldDef::alloc(TUINT32, "userid", SCHEMA_USERS);

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
  int query(SPQueryContext context, TableListener &listener) override {
    DynMap row;
    row[FUSERID] = 500;
    row[FUSERNAME] = "bob";
    TLStatus status = listener.onRow(context, row);
    if (status == TL_STATUS_ABORT) return -1;

    row[FUSERID] = 0;
    row[FUSERNAME] = "root";
     status = listener.onRow(context, row);
    if (status == TL_STATUS_ABORT) return -1;

    return 0;
  }
private:
};

SPVirtualTable newMyUsersTable() { return std::make_shared<MyUsersTable>(); }
