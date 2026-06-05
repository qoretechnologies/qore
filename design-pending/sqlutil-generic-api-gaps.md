# Generic SqlUtil API Gaps

This document tracks generic SqlUtil API improvements identified from the Qorus archive raw-SQL
audit in `/tmp/qorus-archive-raw-sql-sqlutil-gaps.md`. Partition-specific deferred work remains in
[`sqlutil-partitions-deferred.md`](sqlutil-partitions-deferred.md).

## Status

The first slice adds a generic create-table-as-select API on `AbstractTable`:

- `createAsSelectCommit(source, select_hash, opt)`
- `createAsSelect(source, select_hash, opt)`
- `createAsSelectWithInfo(source, select_hash, opt)`
- `getCreateAsSelectSql(source, select_hash, args, opt)`
- `getCreateAsSelectSqlWithInfo(source, select_hash, opt)`

The default implementation emits `CREATE TABLE target AS SELECT ...`. MS SQL Server overrides this
with `SELECT ... INTO target FROM ...`. `replace` is rejected for now because portable replacement
semantics require safe drop behavior first.

The API intentionally follows the existing `insertFromSelect()` contract: source and target tables
are expected to use the same datasource, and the operation is executed entirely inside the database
server.

The second slice adds `AbstractTable::DropTableOptions` for `dropCommit()`, `drop()`, and
`getDropSql()`:

- `if_exists` is generic and returns no SQL for a missing table before attempting table
  introspection; drivers with native syntax may also emit `DROP TABLE IF EXISTS`.
- `cascade` and `force` are supported where the driver has equivalent semantics: PostgreSQL emits
  `CASCADE`; Oracle emits `CASCADE CONSTRAINTS`.
- `cascade_constraints` is supported on Oracle and rejected elsewhere so callers do not get silent,
  non-equivalent drops.
- MS SQL Server, SQLite, MySQL/MariaDB, and Firebird reject cascade-like options for table drops.

## Implementation Plan

1. **Materialized query tables / CTAS** - implemented first because it removes the largest raw-SQL
   shape from archive conversion map creation. The current API covers table creation from a
   structured SqlUtil select hash, bind arguments, SQL/data callbacks, commit and no-commit
   variants, and SQL/result metadata via `SqlResultInfo`.
2. **Drop-for-replacement options** - implemented for `if_exists`, `cascade`,
   `cascade_constraints`, and `force` where equivalent backend semantics are available.
   Driver-managed FK-check suspension remains deferred because it needs an execution API that can
   guarantee restoration after errors, not just a generated SQL list.
3. **Set-wise insert-select improvements** - evolve `insertFromSelect()` into a richer
   `insertSelect()`-style API that supports target column mapping, joins, computed expressions,
   external bind values, and reliable affected-row metadata. This should reuse the same structured
   select representation needed by conversion copy jobs.
4. **Structured aggregate select over joined sources** - expose min/max and other aggregates over
   the same joined/computed source specs used by insert-select, so archive conversion can compute
   partition spans without raw SQL.
5. **Partition-aware index and primary-key options** - add driver-neutral schema fields for local
   partition indexes where possible, with Oracle-specific `LOCAL` and `USING INDEX` support where
   the generic model cannot be fully portable.
6. **Partition maintenance preferences** - add a table maintenance API for statistics preferences,
   starting with Oracle `DBMS_STATS` incremental and granularity preferences and explicit
   unsupported/no-op behavior on other drivers.
7. **Partition exchange / switch** - add a capability-gated lifecycle API only after table-shape
   validation and per-driver semantics are specified. Oracle `EXCHANGE PARTITION` and SQL Server
   `ALTER TABLE ... SWITCH PARTITION` are similar high-throughput primitives but are not identical.

## Open Questions

- Whether CTAS should grow optional index creation in the same method or remain a table-only
  primitive combined with existing index APIs.
- Whether source/target datasource identity should be actively enforced in both CTAS and the older
  `insertFromSelect()` APIs. Enforcing it would make failures clearer, but it can be a
  compatibility change if existing callers rely on equivalent but distinct datasource objects.
- Whether SQL Server CTAS should support selected driver options for filegroups or partition
  schemes once the target table is partitioned.
