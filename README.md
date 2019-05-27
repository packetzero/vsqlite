# vsqlite
C++11 library for using sqlite3 in-memory database to query virtual tables and custom functions.  The implementation is an off-shoot of time spent working with [osquery](https://osquery.io).  Using this simple library, an application can provide similar functionality with very few dependencies.

## Features
 - Data is typed
 - rows are reported one at a time

## Differences from sqlite3 and osquery
 - The osquery table `generate()` returns a `vector<map<string,string>>` for every row.  Whereas vsqlite tables report one row at a time.
 - Using [dyno](https://github.com/packetzero/dyno) DynMap rather than than a map<string,string> means that column names are not repeated for every row, and tables don't need to perform data type conversion (to string, back to type) for every row reported to sqlite.
 - Functions are higher level abstractions
 - Osquery supports extension tables using thrift IPC, this library does not.  It's up to the application to provide IPC proxies.

## Virtual Tables
Virtual table implementations implement the following simple interface.  The design is a high-level model of sqlite3's native model.  The prepare() call is used to filter data based on context's constraints, if there are any.  The next() method will be called until it returns false, indicating that there is no data left for the current constraints.  The table implementation can define a class or struct to keep track of current state and attaching an instance as user-data on the context inside prepare(), then getting the context user data inside next().
The TableDef lists the table name, column details (name, type, options, indexes).
*The prepare() method will be called once for a query with no constraints, but it could be call 1000 times for a single query. See the section on Indexes below.*
```
struct VirtualTable {

  virtual const TableDef& getTableDef() const = 0;

  virtual void prepare(SPQueryContext context) = 0;

  virtual bool next(SPQueryContext context, DynMap &row) = 0;
}
```

### Registering Table

```
  int status = vsqlite->add(std::make_shared<MyTableImplementation>());
```

### Example Function Implementation
Here is an example of hooking the C **pow()** function, which takes two double parameters into a sqlite3 function named **power**.  The `AppFunctionBase` class will take care of the function parameter count, and data type conversions.
```
struct Function_power : public vsqlite::AppFunctionBase {
  Function_power() : vsqlite::AppFunctionBase("power", { TFLOAT64, TFLOAT64 }) {}

  DynVal func(const std::vector<DynVal> &args, std::string &errmsg) override {
    return pow((double)args[0], (double)args[1]);
  }
};

int status = vsqlite->add(std::make_shared<Function_power>());
```

## Notes
- It's not thread-safe, run the single instance from a single thread.

## Table Indexes
If a table column is defined with `INDEX` , `REQUIRED`, or `ADDITIONAL` option, then sqlite will assume that your table implements the index for `OP_EQ`.  You can optionally specify additional operators such as `OP_LIKE`.  An index drastically changes the way a table's methods are called.  By specifying an OP_EQ index, you are telling sqlite that it's way faster for you to lookup a single row by value, than it is to return all rows and have sqlite do the filtering.  Accordingly, if a processes table has an index on the pid column, and the query looks like `SELECT * FROM processes WHERE pid in (4,6,2002,10,100,102)` then prepare() will be called 6 times, once per constraint value.  So your prepare implementation of the OP_EQ index should gather the data for that one value, the next() call will return that value.
```
   # example calls for indexed pid column
   prepare() pid=4
   next() return true
   next() return false
   prepare() pid=6
   next() return true
   next() return false
   prepare() pid=2002
   next() return false   # not found
   prepare() pid=10
   next() return false   # not found
   prepare() pid=100
   next() return true
   next() return false
   prepare() pid=102
   next() return true
   next() return false
```
Conversely, if there are no constraints, it might look like:
```
   prepare()
   next() return true  # row for pid=4
   next() return true  # row for pid=6
   next() return true  # row for pid=100
   next() return true  # row for pid=102
   next() return false
```
