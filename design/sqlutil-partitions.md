# SqlUtil Partition Architecture

## Status

Implemented in SqlUtil 3.0.0. This record describes the shipped range-partition contract and the
reasons behind it. Public method and hashdecl documentation is generated from `qlib/SqlUtil`; future
work is tracked only in
[`design-pending/sqlutil-partitions-deferred.md`](../design-pending/sqlutil-partitions-deferred.md).

The implementation is covered by live PostgreSQL, Oracle, MySQL/MariaDB, and SQL Server tests plus
schema-loader, reverse-schema, callback, cache, concurrency, and negative tests.

## Scope and invariants

The portable contract currently supports range partitioning with inclusive lower and exclusive
upper bounds. The following invariants are deliberate:

1. `dropPartition()` removes the partition and its data on every supported backend.
2. Non-equivalent operations are separate and capability-gated. PostgreSQL detach is exposed as
   `detachPartition()` rather than as a drop option.
3. Explicit partition creation and native automatic creation coexist. `ensurePartition()` is the
   idempotent entry point for callers that should work with either model.
4. Partition lifecycle changes are imperative. `align()` compares the strategy but never creates or
   drops physical partitions.
5. Unsupported operations fail with `PARTITION-NOT-SUPPORTED`; invalid specs or lifecycle state fail
   with `PARTITION-ERROR`.
6. Public names are identifiers, not SQL fragments. Drivers quote and validate them with their
   normal identifier helpers.
7. Structural partition caches are cleared after successful DDL and on uncertain callback/DDL
   failures. Cleanup uses scoped `on_error clearPartitionCache();` handlers where no translation is
   needed.

## Driver capabilities

| Capability | PostgreSQL 10+ | Oracle 12c+ | MySQL 5.7+ / MariaDB | SQL Server 2016+ | SQLite / Firebird |
|---|:---:|:---:|:---:|:---:|:---:|
| Range partition lifecycle | yes | yes | yes | yes | no |
| Native auto materialization | no | yes (interval) | no | no | no |
| Detach to standalone table | yes | no | no | no | no |
| Default/catch-all range | yes | `MAXVALUE` | `MAXVALUE` | implicit edge ranges | no |
| Composite range key | yes | yes | yes | no | no |

SQL Server uses partition functions and schemes rather than durable child objects. SqlUtil exposes
logical finite ranges as `p1`, `p2`, and so on and records the physical partition number under
`PartitionInfo.driver.mssql.partition_number`. Because those logical names can shift after split or
merge operations, bounds-based lifecycle overloads are preferred there.

## Public data model

The implementation uses the following public types. `PartitionSpec` deliberately contains both
creation fields and introspection-only optional identity fields so descriptions can round-trip
without a second parallel type hierarchy.

```qore
public enum PartitionLookupStatus : string {
    Found = "found",
    NotFound = "not_found",
    Uncomparable = "uncomparable",
}

public enum PartitionVerifiedBy : string {
    Bounds = "bounds",
    Name = "name",
    Auto = "auto",
}

public hashdecl PartitionStrategy {
    string method = "range";
    softlist<string> columns;
    *hash<auto> auto;
    *hash<auto> driver;
}

public hashdecl PartitionSpec {
    *string name;
    string method = "range";
    *softlist<string> columns;
    *auto bound_from;
    *auto bound_to;
    *string bound_from_sql;
    *string bound_to_sql;
    *string bound_sql;
    bool is_default = False;
    *string schema;
    *string relation_name;
    *string sql_name;
    *hash<auto> driver;
}

public hashdecl PartitionInfo inherits PartitionSpec {
    int ordinal;
}

public hashdecl PartitionLookupResult {
    PartitionLookupStatus status;
    *PartitionInfo partition;
    *string reason;
}

public hashdecl PartitionEnsureResult {
    bool created = False;
    bool materialized = False;
    bool deferred_auto = False;
    PartitionVerifiedBy verified_by = PartitionVerifiedBy::Bounds;
    bool bounds_verified = False;
    *PartitionInfo partition;
}
```

`ordinal` is recomputed for each `listPartitions()` result and is only an ordering aid. Canonical
`name`, or the `schema` and `relation_name` pair where exposed, is the identity.

Typed bounds are preferred. Native `bound_*_sql` and `bound_sql` fields preserve expressions that
cannot be represented portably. Composite typed bounds must match the strategy column cardinality.

### Automatic policy representation

`PartitionStrategy.auto` is a typed policy hash, not a boolean. Oracle accepts an interval SQL
expression under `expression` or `interval`; catalog introspection normalizes semantically
equivalent interval expressions for alignment. Other current drivers reject an automatic policy.
An auto-covered `ensurePartition()` result has `deferred_auto = True`, `materialized = False`, and
`verified_by = PartitionVerifiedBy::Auto` until an insert causes Oracle to materialize it.

## Table descriptions and round-trip

Partitioned table descriptions use:

```qore
{
    "partition_strategy": {
        "method": "range",
        "columns": "created",
    },
    "partitions": {
        "y2026m07": {
            "bound_from": 2026-07-01,
            "bound_to": 2026-08-01,
        },
    },
}
```

`getDescriptionHash()` always includes `partition_strategy` when present. Physical partitions are
included only with `getDescriptionHash({"with_partitions": True})`, avoiding accidental expansion
of descriptions for tables with many partitions. Driver overrides are applied to the strategy and
to every individual spec before validation.

Descriptions also round-trip driver-specific index/constraint placement. Oracle uses
`driver.oracle.partition_scope` (`local`, `global`, or `default`) with `index_tablespace` and
`compute_statistics`; local unique indexes and local PK/unique constraints must include every
partition key column. SQL Server aligned keys are placed on the partition scheme, MySQL keys are
inherently tied to the partitioned table, and PostgreSQL parent/child index attachment is not
misrepresented as Oracle-style locality.

## Validation contract

Normalization is separated from creation validation so lookup specs are not rejected for omitting
creation-only fields. Creation validates:

- strategy method and declared key columns;
- typed/native bound exclusivity;
- scalar/composite cardinality and exact ISO date coercion from JSON/YAML schemas;
- lower-before-upper ordering, contiguity, overlap, duplicate name, and catch-all rules;
- driver restrictions on primary keys, unique constraints, and unique indexes;
- option and driver-override types.

PostgreSQL and MySQL require every unique key to contain all partition columns. SQL Server enforces
the rule for aligned unique keys but supports explicitly nonaligned unique keys. Oracle global keys
may omit partition columns; Oracle local unique keys may not.

`align()` compares normalized strategy semantics (method, ordered columns, normalized automatic
policy, and structurally meaningful driver configuration). Any real mismatch raises
`PARTITION-ERROR`; repartitioning requires an explicit data-migration workflow.

## Public lifecycle APIs

SQL generators return one SQL string because each currently exposed public operation is represented
as one driver operation at the callback boundary:

```qore
*hash<PartitionStrategy> getPartitionStrategy();
hash<string, PartitionInfo> listPartitions();
hash<PartitionLookupResult> findPartitionBySpec(hash<auto> spec);

string getAddPartitionSql(hash<auto> spec, *hash<auto> opt);
hash<PartitionInfo> addPartition(hash<auto> spec, *hash<auto> opt);
hash<PartitionInfo> addPartitionCommit(hash<auto> spec, *hash<auto> opt);

string getDropPartitionSql(string name, *hash<auto> opt);
string getDropPartitionSql(hash<auto> spec, *hash<auto> opt);
dropPartition(string name, *hash<auto> opt);
dropPartition(hash<auto> spec, *hash<auto> opt);
dropPartitionCommit(string name, *hash<auto> opt);
dropPartitionCommit(hash<auto> spec, *hash<auto> opt);

string getTruncatePartitionSql(string name, *hash<auto> opt);
string getTruncatePartitionSql(hash<auto> spec, *hash<auto> opt);
truncatePartition(string name, *hash<auto> opt);
truncatePartition(hash<auto> spec, *hash<auto> opt);
truncatePartitionCommit(string name, *hash<auto> opt);
truncatePartitionCommit(hash<auto> spec, *hash<auto> opt);

string getDetachPartitionSql(string name, *hash<auto> opt);
string getDetachPartitionSql(hash<auto> spec, *hash<auto> opt);
AbstractTable detachPartition(string name, *hash<auto> opt);
AbstractTable detachPartition(hash<auto> spec, *hash<auto> opt);
AbstractTable detachPartitionCommit(string name, *hash<auto> opt);
AbstractTable detachPartitionCommit(hash<auto> spec, *hash<auto> opt);

hash<PartitionEnsureResult> ensurePartition(hash<auto> spec, *hash<auto> opt);
hash<PartitionEnsureResult> ensurePartitionCommit(hash<auto> spec, *hash<auto> opt);
```

The bare executors do not manage transactions; `*Commit()` companions commit on success and roll
back on error. Oracle and MySQL DDL can commit implicitly, which is documented rather than hidden.
Callbacks run exactly once per public operation. If an executing callback handles the SQL, the
executor does not execute it again.

`ensurePartition()` resolves by comparable bounds, then canonical name when bounds are native-only,
then automatic policy. Concurrent duplicate-create errors are recovered deterministically through a
savepoint on transactional-DDL drivers and a single metadata re-resolution; there is no retry loop or
polling.

## Driver mapping

| Operation | PostgreSQL | Oracle | MySQL/MariaDB | SQL Server |
|---|---|---|---|---|
| add | `CREATE TABLE ... PARTITION OF` | `ADD PARTITION ... LESS THAN` | `ADD PARTITION` | scheme `NEXT USED` + function `SPLIT RANGE` |
| drop | verify child, then `DROP TABLE child` | `DROP PARTITION` | `DROP PARTITION` | truncate physical partition + `MERGE RANGE` |
| truncate | `TRUNCATE child` | `TRUNCATE PARTITION` | `TRUNCATE PARTITION` | `TRUNCATE TABLE ... WITH (PARTITIONS)` |
| detach | `DETACH PARTITION` | unsupported | unsupported | unsupported |

For Oracle and MySQL, lower bounds that are not present in DDL are still required where necessary for
contiguity and overlap validation. SQL Server duplicate split recovery intentionally avoids a
savepoint because a failed split can leave no corresponding open transaction while metadata remains
queryable.

## Schema tooling

The complete schema pipeline understands partition metadata:

- `DataSchemaLoader` preserves strategy/spec keys and coerces exact ISO dates according to normalized
  partition-column types;
- `DataSchemaMetaSchema` validates the range strategy and spec shapes;
- `SchemaReverse` and `bin/schema-reverse` emit strategy by default and physical specs with
  `with_partitions` / `--with-partitions`;
- `bin/qschema export --with-partitions` exposes the same opt-in behavior;
- `AbstractSchema`, `DataSchema`, and SqlUtil data-provider consumers pass descriptions through.

## Test coverage and completed phasing

The implementation was delivered in tested slices: metadata/round-trip, lookup-vs-creation
validation, semantic alignment, callback/cache lifecycle, driver key restrictions, Oracle local
indexes, and generic set-wise API hardening. Each slice was exercised against the affected live
databases before commit. Current integration coverage includes bounds/default/SQL-only cases,
composite keys, concurrency races, callbacks, cache invalidation, transaction companions, schema
round-trip, reverse alignment, local/global indexes, and unsupported-driver errors.

List/hash/subpartition methods, exchange/switch, online PostgreSQL detach, generic read qualification,
high-cardinality lookup, and optional data-dependent metadata remain in the deferred roadmap.
