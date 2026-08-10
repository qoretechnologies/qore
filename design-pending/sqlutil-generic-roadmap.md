# Generic SqlUtil Roadmap

## Implemented baseline

A downstream archive raw-SQL audit no longer identifies a missing generic primitive for the common
set-wise and aggregate cases:

- create-as-select supports generic SQL, SQL Server `SELECT INTO`, commit/no-commit execution,
  callbacks, bind arguments, and `SqlResultInfo` preview/result variants;
- insert-select enforces exact datasource object identity, forwards structured-select resolution
  options, validates target columns and reliable projection cardinality, and provides non-executing
  SQL/argument preview through `getInsertFromSelectSql*()`;
- structured selects already combine joins, computed columns, `cop_min()` / `cop_max()`, grouping,
  having, and `*WithInfo()` result metadata;
- table drop options cover `if_exists`, `cascade`, `cascade_constraints`, and `force` where the
  backend has equivalent semantics;
- statistics preferences are exposed generically and implemented with Oracle
  `DBMS_STATS.SET_TABLE_PREFS`.

Partition architecture and its sole future roadmap live in
[`design/sqlutil-partitions.md`](../design/sqlutil-partitions.md) and
[`sqlutil-partitions-deferred.md`](sqlutil-partitions-deferred.md).

## Remaining generic questions

These are design questions rather than demonstrated missing archive primitives:

1. Whether create-as-select should optionally create indexes in the same call or remain a table-only
   primitive composed with the existing index APIs. Keeping the operations separate currently gives
   clearer transaction and error boundaries.
2. Whether SQL Server create-as-select needs explicit filegroup or partition-scheme placement. Such
   options should be added only with an archive schema that cannot be represented by creating the
   table and indexes through existing APIs.
3. Whether driver-managed foreign-key-check suspension needs a generic execution scope that can
   guarantee restoration after errors. A SQL-list generator alone is insufficient for that safety
   contract.

No separate aggregate API or parallel `insertSelect()` API is planned without a concrete structured
query that the current select hash and `insertFromSelect*()` methods cannot represent.
