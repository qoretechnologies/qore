# Generic Partition Support in SqlUtil

## Status

Draft / proposal. No partition lifecycle API exists in SqlUtil today; this document
specifies a portable one.

Phase 1 is deliberately limited to **range partitions**. The data model leaves room for
list/hash/sub-partitioning later, but those methods are not part of the initial contract
because their DDL, naming, and drop semantics are not yet uniformly specified across the
target drivers.

## Motivation

Qorus archiving needs **time-range (monthly) partitions** so that retiring old data is a
near-instant catalog operation (drop a partition) instead of a row-by-row `DELETE` over a
large table. The same primitive — "roll a new period forward, drop the oldest period" — must
work uniformly across the database backends Qorus runs on (PostgreSQL, Oracle, MySQL).

## Current state

Partition handling in SqlUtil today is fragmentary and read-only:

- **PostgreSQL** — partitioned parent tables (`pg_class.relkind = 'p'`) are now visible to
  `checkExistenceImpl()` / `describeImpl()` (they query `pg_class`/`pg_namespace` instead of
  `pg_statio_all_tables`, which omits relations with no physical storage). This is the
  prerequisite that lets SqlUtil *see* a partitioned table at all; there is no DDL.
- **Oracle** — a SELECT-time `"partition"` qualifier only
  (`OracleSqlUtilBase.qm`, `OracleSelectOptions`): `selectRows({"partition": "p1"})`.
- **MySQL / SQLite / Firebird / MSSQL** — `partition` is at most a reserved word.
- **No driver** generates add / drop / truncate / detach / exchange-partition DDL.

## Design principles

1. **`dropPartition()` is uniform.** It removes the named partition *and its data*, with
   identical observable semantics on every supported driver. Drivers must not weaken or
   strengthen this (for example, PostgreSQL must not silently leave the data behind as a
   standalone table).
2. **Driver-unique capabilities are exposed through their own API, never overloaded onto a
   generic method.** PostgreSQL can *detach* a partition into a standalone table without
   dropping it — that is surfaced as a separate, capability-gated `detachPartition()`, not as
   a flag on `dropPartition()`.
3. **No imposed creation model.** Some databases auto-create partitions on insert (Oracle
   interval partitioning); others require explicit pre-creation (PostgreSQL declarative).
   The API supports **both** and forces neither: explicit `addPartition()`, an optional
   declared auto-strategy, and an idempotent `ensurePartition()` that the archiving job can
   call regardless of how partitions come into existence.
4. **Lifecycle ops are imperative, not part of `align()`.** Schema alignment must never
   create or drop partitions as a side effect — dropping a partition destroys data and must
   always be an explicit, intentional call.
5. **Capability-gated with clear failures.** Unsupported backends throw a well-defined
   exception rather than silently no-op'ing or emulating with non-equivalent semantics.
6. **Names are identifiers, not SQL fragments.** Public methods accept driver partition
   identifiers. Implementations must quote identifiers through existing driver helpers and
   must never concatenate a caller-provided name into SQL unescaped.

## Phase-1 scope

Phase 1 supports only range partitions:

- one partitioning method: `"range"`;
- one or more key columns;
- inclusive lower and exclusive upper bounds;
- optional default / catch-all range partition where the driver supports it;
- optional auto-create strategy for drivers with native interval partitioning.

List/hash partitions, composite sub-partitioning, partition exchange, and non-equivalent detach
emulations are deferred — see [`sqlutil-partitions-deferred.md`](sqlutil-partitions-deferred.md).
Future work should add method-specific spec hashdecls rather than overloading the range-oriented
fields below.

## Capability flags

Mirrors the existing `supportsTablespacesImpl()` pattern. Default implementations on
`AbstractTable` return `False`; drivers override. In phase 1, `supportsPartitions()` means
that the driver/server supports range-partition DDL through this API.

```qore
#! True if the driver/server supports range partitioning DDL through SqlUtil
bool supportsPartitions();

#! True if a partition can be detached into a standalone table without dropping its data
bool supportsPartitionDetach();

#! True if the backend can auto-create range partitions on insert (for example Oracle interval)
bool supportsAutoPartitioning();
```

| Capability            | PostgreSQL 10+ | Oracle 12c+ | MySQL 5.7+ | SQLite | Firebird | MSSQL (later) |
|-----------------------|:--------------:|:-----------:|:----------:|:------:|:--------:|:-------------:|
| `supportsPartitions`        | yes      | yes         | yes        | no     | no       | yes (phase 2) |
| `supportsPartitionDetach`   | yes      | no¹         | no         | no     | no       | no            |
| `supportsAutoPartitioning`  | no²      | yes         | no         | no     | no       | no            |

¹ Oracle can approximate detach via `EXCHANGE PARTITION`, but it requires a pre-existing
target table with an identical shape, so the semantics are not equivalent; left unsupported
in phase 1 rather than emulated.
² Native declarative partitioning has no built-in interval auto-create (pre-`pg_partman`).

## Data model

> **Implementation convention:** every hash in the implementation (hashdecl members, method
> parameters, return types, locals) must be a typed hash — `hash<auto>`, a templated
> `hash<string, T>`, or a `hash<SomeHashdecl>` — and never a bare `hash`. A bare `hash`
> forces expensive runtime type-stripping on every assignment; the typed forms avoid it. All
> hashdecls and signatures in this document already follow this rule.

### Bound values

`bound_from` and `bound_to` use typed Qore values when the bound can be represented
portably. For multi-column range partitioning, the value is a list whose cardinality must
match the table's partition key. For driver-native expressions or sentinels such as
`MINVALUE` / `MAXVALUE`, use `bound_from_sql` / `bound_to_sql`; portable callers should
prefer typed values.

For `PartitionSpec` input, typed and native SQL values are mutually exclusive per bound side:
callers must not provide both `bound_from` and `bound_from_sql`, or both `bound_to` and
`bound_to_sql`. This prevents ambiguous DDL generation. `PartitionInfo` introspection may
include both typed values and native SQL text when the driver can provide both.

When introspecting, drivers should return both the typed values they can parse and the native
SQL text used by the database. The native text is required for diagnostics and round-trip
visibility because parsing partition bounds is not equally reliable on every backend.

### Schema-file bound coercion

Qore source schema descriptions may use native Qore values such as date literals. `.ysm` /
`.jsm` data-schema files cannot represent Qore date literals directly, so
`DataSchemaLoader` must define deterministic coercion for partition bounds:

- table/column driver overrides and column shorthand normalization run before partition bound
  coercion; `PartitionStrategy.columns` is resolved against the normalized column map first;
- JSON/YAML strings matching ISO date or datetime syntax are converted to Qore `date` values
  when the corresponding partition column is declared as a Qore/date-like column;
- otherwise JSON/YAML strings remain strings and are quoted as string values by the driver;
- numeric and boolean values retain their native JSON/YAML scalar type;
- composite bounds are coerced element by element against `PartitionStrategy.columns`;
- `bound_from_sql` / `bound_to_sql` are never coerced and are passed through as native SQL
  expressions.

If the loader cannot infer the target partition column type, it must leave the value unchanged
rather than guessing.

### `PartitionInfo` — one row per existing partition (introspection result)

```qore
public hashdecl PartitionInfo {
    string name;                  #! canonical public partition identifier for lifecycle calls
    string method = "range";      #! phase 1 only: "range"
    softlist<string> columns;     #! partition key column(s)
    int ordinal;                  #! stable lifecycle order; lower ranges first, default last

    *auto bound_from;             #! inclusive lower bound; scalar or list for composite keys
    *auto bound_to;               #! exclusive upper bound; scalar or list for composite keys
    *string bound_from_sql;       #! native lower-bound expression if available
    *string bound_to_sql;         #! native upper-bound expression if available
    *string bound_sql;            #! native complete bound clause for diagnostics / round-trip

    bool is_default = False;      #! catch-all / default partition

    *string schema;               #! schema that owns the partition object/relation, if exposed
    *string relation_name;        #! physical relation/table name, if distinct from name
    *string sql_name;             #! fully quoted SQL identifier for diagnostics only

    *hash<auto> driver;           #! driver-specific metadata, keyed by driver name
}
```

For PostgreSQL, `name` is the child table identifier used by lifecycle calls; `relation_name`
is also set to the child table name. For Oracle/MySQL, `name` is the database partition name
and `relation_name` is normally unset.

`method` and `columns` are convenience copies of the table-level `PartitionStrategy` facts
(constant across every partition of a table); they are denormalized into `PartitionInfo` so a
single partition descriptor is self-contained, not because they vary per partition.

`ordinal` is assigned by the driver during introspection and is the stable ordering key for
`listPartitions()`. When typed lower bounds are available, ordinal follows ascending lower
bound order with the default/catch-all partition last. When only native `bound_sql` is
available, the driver must still assign a deterministic ordinal from database metadata rather
than forcing callers to parse native SQL text.

`ordinal` is a position **within a single `listPartitions()` result**, recomputed on every
introspection — it shifts when partitions are added or dropped. It is an ordering aid, **not a
durable identity**: use `name` (or `schema`/`relation_name`) to refer to a partition across
calls, and never persist `ordinal` across structural changes.

### `PartitionSpec` — input to create / lookup operations

`PartitionSpec` intentionally does **not** repeat the partition method or key columns. Those
are table-level facts from `PartitionStrategy`. `addPartition()` and `ensurePartition()` must
validate each spec against the table's strategy.

```qore
public hashdecl PartitionSpec {
    *auto bound_from;             #! range lower bound (inclusive); scalar or composite list
    *auto bound_to;               #! range upper bound (exclusive); scalar or composite list
    *string bound_from_sql;       #! native lower-bound expression, if a typed value is not usable
    *string bound_to_sql;         #! native upper-bound expression, if a typed value is not usable
    bool is_default = False;      #! create / find the default catch-all partition
                                  #! (note: `default` is a Qore keyword and cannot be a hashdecl member)
    *hash<auto> driver;           #! driver-specific options (tablespace, PG child schema, etc.)
}
```

Validation rules:

- `PartitionStrategy.columns` must reference columns that are actually declared on the table;
  an unknown partition key column is a `DESCRIPTION-ERROR`, validated at `setupTable()` before
  any bound coercion or DDL generation (the PK-includes-partition-key rule and the schema-file
  bound coercion both depend on this resolving first);
- non-default range partitions require `bound_from`/`bound_from_sql` and
  `bound_to`/`bound_to_sql`;
- `is_default` partitions must not provide regular bounds;
- `bound_from` and `bound_from_sql` are mutually exclusive, as are `bound_to` and
  `bound_to_sql`;
- composite bounds must have the same number of values as `PartitionStrategy.columns`;
- drivers whose DDL only names an upper bound (Oracle/MySQL range partitioning) still use
  `bound_from` for validation, lookup, and overlap detection;
- implementations must reject overlapping ranges, duplicate names, and specs incompatible
  with the table strategy;
- implementations must surface driver restrictions on unique/primary keys for partitioned
  tables as `PARTITION-ERROR` or `DESCRIPTION-ERROR`, not as late SQL surprises where the
  restriction can be validated earlier. In particular, both PostgreSQL and MySQL **require
  every primary-key and unique constraint to include all `PartitionStrategy.columns`**; this
  must be validated at `setupTable()` (description time), not deferred to `CREATE`.

### Table-level partition strategy

Added as an optional `partition_strategy` key in the table-description hash so a partitioned
table can be created and re-described round-trip. Absent for non-partitioned tables.

```qore
public hashdecl PartitionStrategy {
    string method = "range";       #! phase 1 only: "range"
    softlist<string> columns;      #! partition key column(s)
    *hash<auto> auto;              #! declared auto-create policy where supported
                                   #! portable example: {"interval_months": 1}
    *hash<auto> driver;            #! driver-specific strategy options
}
```

The `auto` member is how principle 3 is honoured declaratively: a table may declare an
auto-create policy (Oracle interval) *or* leave it unset and have partitions created
explicitly. `supportsAutoPartitioning()` gates whether `auto` is meaningful for the driver.

Initial partitions are represented by an optional top-level `partitions` key in the table
description:

```qore
"partition_strategy": <PartitionStrategy>{
    "method": "range",
    "columns": "created",
},
"partitions": {
    "y2026m07": <PartitionSpec>{
        "bound_from": 2026-07-01,
        "bound_to": 2026-08-01,
    },
},
```

`partitions` is used when creating a table and when describing an existing table. It is not
processed by `align()` as a lifecycle delta; creating, dropping, or truncating partitions
remains an explicit lifecycle operation.

**`align()` and `partition_strategy`.** While the `partitions` *list* is never an alignment
delta, the `partition_strategy` itself participates in comparison. Converting a table's
partitioning — non-partitioned ↔ partitioned, or a changed method/key columns — cannot be done
by in-place `ALTER` on any target driver (it is a table rebuild plus data migration). Therefore
`getAlignSql()` must **compare `partition_strategy` and, on any mismatch, throw a clear
`PARTITION-ERROR`** (with both the existing and desired strategy in the message) rather than
emit DDL or silently ignore the difference. Re-partitioning is an explicit, out-of-band
operation, not an alignment step. Two partitioned tables with identical `partition_strategy`
align normally on their non-partition structure (columns, indexes, etc.).

### Table-description integration

Implementation must add the following top-level table-description keys:

- `partition_strategy`: `PartitionStrategy`;
- `partitions`: `hash<string, PartitionSpec>`.

The keys must be added to `AbstractTable::TableDescriptionHashOptions`; otherwise
`setupTable()` will reject table descriptions before any driver-specific code can run.
Driver-specific option merging must also apply to `partition_strategy`, `partitions`, and
each individual `PartitionSpec`, following the existing `driver` sub-hash pattern.

`AbstractTable::getDescriptionHash()` is extended from its current argumentless form to:

```qore
hash<auto> getDescriptionHash(*hash<auto> opt);
```

The new option hash is backward-compatible: callers that pass no options get the existing
description shape plus `partition_strategy` for partitioned tables. The first generic option
is `with_partitions` (`bool`, default `False`). When `with_partitions` is `False`,
`getDescriptionHash()` omits the `partitions` list; when `True`, it materializes the current
partition list as `hash<string, PartitionSpec>`.

Implementation should add a small `GetDescriptionOptions` option map, initially containing
only `with_partitions`, and validate it with the same `OPTION-ERROR` behavior used by other
SqlUtil option-taking methods. The name is intentionally distinct from the existing
`TableDescriptionHashOptions` const (which lists the allowed description *content* keys) to
avoid confusion: `GetDescriptionOptions` governs the `getDescriptionHash()` *method* options.
Existing subclasses that do not override description behavior remain source-compatible because
the argument is optional.

An archive table can have thousands of monthly partitions, so embedding every `PartitionSpec`
in the default description would make routine `describe()` / round-trip calls large and slow.
`partition_strategy` alone is enough to re-create a correctly partitioned but empty parent on
drivers that support parent-only partitioned-table creation (PostgreSQL phase 1). Drivers that
require at least one partition definition at create time, or require an auto strategy to
bootstrap the table, must document and validate that requirement during `setupTable()` /
`getCreateTableSql()`. `SchemaReverse`'s `with_partitions` toggle (below) mirrors this same
default-off behaviour, so description-level and reverse-engineering output stay consistent.

## API

All DDL-producing methods follow the established SqlUtil shape: a public method takes the
table lock, delegates to a driver `…Impl()`, and wraps generated SQL through
`AbstractDatabase::doCallback()`.

### Transaction management

SqlUtil's established convention is a **method pair**: the bare verb executes with no
transaction management (e.g. `AbstractTable::drop()`), and a `…Commit()` companion wraps it
with `on_exit ds.commit(); on_error ds.rollback()` (e.g. `dropCommit()`, `truncateCommit()`,
`createCommit()`). The partition lifecycle methods follow the same convention:

- the bare executors (`addPartition()`, `dropPartition()`, `truncatePartition()`,
  `detachPartition()`, `ensurePartition()`) execute within the **caller's** transaction and do
  **not** commit, exactly like `drop()`;
- committing companions `addPartitionCommit()`, `dropPartitionCommit()`,
  `truncatePartitionCommit()`, `detachPartitionCommit()`, and `ensurePartitionCommit()` provide
  the managed form.

Two driver realities must be documented on these methods rather than smoothed over: Oracle and
MySQL DDL commits implicitly (so the bare form cannot truly stay uncommitted there), and
PostgreSQL `DETACH … CONCURRENTLY` cannot run inside a transaction at all (see Open items).
The methods do not promise identical rollback semantics across drivers; they promise the same
naming/contract split as the rest of SqlUtil.

### Caching

`listPartitions()` / `getPartitionStrategy()` cache their results like `getIndexes()` /
`describe()` do today. Every lifecycle public method that mutates partition structure
(`addPartition`, `dropPartition`, `detachPartition`, and `ensurePartition` when it creates)
must, after a successful execution, invalidate the table's cached partition list (and, where
partition changes affect it, the table's describe/`getDescriptionHash()` cache).
`truncatePartition()` mutates data, not structure; it does not need to invalidate the
structural partition list unless future `PartitionInfo` metadata includes row counts,
statistics, or other data-dependent fields. Drivers must not leave stale partition metadata
visible after a successful lifecycle call; this is asserted by the cache-invalidation tests
below.

### Name and identifier semantics

Portable API methods accept the canonical partition identifier exposed as `PartitionInfo.name`,
not a raw SQL fragment:

- PostgreSQL: canonical relation identity encoded as `schema.relation_name` when a schema is
  needed, otherwise `relation_name`;
- Oracle/MySQL: partition identifier.

Driver-specific physical placement (for example a PostgreSQL child schema) belongs in
`PartitionSpec.driver.<driver>` or `PartitionStrategy.driver.<driver>`. Implementations must
quote all generated identifiers through existing driver helpers and must verify PostgreSQL
child relations through the parent relationship before generating destructive SQL.

For PostgreSQL, `PartitionInfo.schema` and `PartitionInfo.relation_name` are the authoritative
identity fields. `PartitionInfo.name` is a convenience canonical key for hashes and lifecycle
calls. It must be generated from those fields, never from already quoted SQL text, and must be
parsed back through the same SqlUtil identifier parser used for table names. If either
component contains characters that make a dotted string ambiguous, the implementation must
require the caller to use the introspected `PartitionInfo.name` or a driver option carrying
separate `schema` / `relation_name` values; SQL-quoted strings are not accepted as API names.

Callers that receive a `PartitionInfo` from `listPartitions()` or `findPartitionBySpec()`
must be able to pass `PartitionInfo.name` directly to `dropPartition()`,
`truncatePartition()`, and `detachPartition()`. For manually named PostgreSQL partitions,
`addPartition(name, spec, opt)` accepts the desired child relation name; if the child schema
is not the parent schema, it must be supplied through a driver option and reflected back in
`PartitionInfo.schema` / `PartitionInfo.name` after introspection.

For PostgreSQL in particular, `getDropPartitionSql()` must confirm that `name` is a partition
of `self` before returning `DROP TABLE child`; a standalone table with the same name must not
be droppable through `dropPartition()`.

### Introspection and lookup

```qore
public enum PartitionLookupStatus : string {
    Found = "found",
    NotFound = "not_found",
    Uncomparable = "uncomparable",
}

public enum PartitionVerifiedBy : string {
    Bounds = "bounds",   #! verified by matching typed bounds
    Name = "name",       #! verified by canonical partition name only
    Auto = "auto",       #! covered by a declared auto-create strategy
}

public hashdecl PartitionLookupResult {
    PartitionLookupStatus status;
    *PartitionInfo partition;
    *string reason;
}

#! Returns the partitions of this table keyed by PartitionInfo.name, inserted in
#! PartitionInfo.ordinal order; empty hash if not partitioned
hash<string, PartitionInfo> listPartitions();      # -> getPartitionsImpl()

#! Returns the table's partition strategy, or NOTHING if the table is not partitioned
*PartitionStrategy getPartitionStrategy();

#! Finds a materialized partition by normalized bounds/default status, not by name
PartitionLookupResult findPartitionBySpec(hash<PartitionSpec> spec);
```

`PartitionLookupStatus::Found` requires `partition`; `NotFound` must not set `partition`;
`Uncomparable` should set `reason` with the missing capability or unparsed bound that made a
deterministic comparison impossible.

`listPartitions()` returns range partitions in `PartitionInfo.ordinal` order. Qore hashes
preserve insertion order, so this is a stable contract drivers must honour: it makes
"oldest = first" reliable and lets archiving sweep "drop every partition with
`bound_to <= cutoff`" deterministically without re-sorting. Ordinal normally follows typed
lower-bound order (default/catch-all partition last); when typed bounds are unavailable, the
driver still supplies a deterministic ordinal from database metadata.

On PostgreSQL, `getPartitionsImpl()` walks `pg_inherits` → `pg_class` from the parent (now
discoverable thanks to the `relkind 'p'` fix) and reads each child's bounds via
`pg_get_expr(relpartbound, …)`. The implementation should expose the native bound clause in
`bound_sql` even if typed parsing is incomplete.

`findPartitionBySpec()` exists because auto-created partitions may have driver-generated
names. Qorus archiving code finds the partition for a month by bounds, then — on a `Found`
result — drops it by `result.partition.name`.

Matching semantics: `findPartitionBySpec()` compares **typed** bounds when both the spec and
the candidate partition expose them (`bound_from`/`bound_to`), plus `is_default` status. When
a partition exposes only native bound text (`bound_sql` and no parsed typed bounds — possible
on backends where bound parsing is incomplete), it cannot be reliably compared: the method must
return `PartitionLookupStatus::Uncomparable` rather than guess. `NotFound` is reserved for a
successful comparison that found no matching partition. Callers that need determinism when
lookup is uncomparable should fall back to a deterministic partition naming scheme and drop by
known canonical name.

### Lifecycle — SQL generators (for batching / preview) + convenience executors

```qore
public hashdecl PartitionEnsureResult {
    bool created;              #! True if SqlUtil executed DDL to create a partition
    bool materialized;         #! True if a concrete partition exists after the call
    bool deferred_auto = False;#! True if no DDL was needed because an auto policy covers it
    PartitionVerifiedBy verified_by;  #! how the result was verified
    bool bounds_verified;             #! convenience alias: == (verified_by == PartitionVerifiedBy::Bounds)
    *PartitionInfo partition;  #! existing or newly-created partition when materialized
}

# --- SQL generators (return softlist<auto> of DDL statements) ---
softlist<auto> getAddPartitionSql(string name, hash<PartitionSpec> spec, *hash<auto> opt);
softlist<auto> getDropPartitionSql(string name, *hash<auto> opt);
softlist<auto> getTruncatePartitionSql(string name, *hash<auto> opt);
softlist<auto> getDetachPartitionSql(string name, *hash<auto> opt);   # detach-capable only

# --- imperative executors: bare form runs in the caller's transaction (no commit), like drop() ---
#! create a partition; throws PARTITION-ERROR if it already exists
addPartition(string name, hash<PartitionSpec> spec, *hash<auto> opt);

#! create the partition only if needed; returns whether DDL ran and/or a partition exists
PartitionEnsureResult ensurePartition(string name, hash<PartitionSpec> spec, *hash<auto> opt);

#! remove the partition AND its data — identical semantics on every supported driver
dropPartition(string name, *hash<auto> opt);

#! empty the partition, keeping the partition object
truncatePartition(string name, *hash<auto> opt);

#! PostgreSQL-style: detach into a standalone table without dropping data.
#! Returns the resulting standalone AbstractTable. Throws PARTITION-NOT-SUPPORTED where
#! supportsPartitionDetach() is False. The returned table is constructed against the same
#! datasource with the detached relation's name/schema; it is NOT implicitly added to the
#! parent's Tables cache (the caller owns its lifecycle).
AbstractTable detachPartition(string name, *hash<auto> opt);

# --- committing companions: wrap the bare form with on_exit commit / on_error rollback ---
addPartitionCommit(string name, hash<PartitionSpec> spec, *hash<auto> opt);
PartitionEnsureResult ensurePartitionCommit(string name, hash<PartitionSpec> spec, *hash<auto> opt);
dropPartitionCommit(string name, *hash<auto> opt);
truncatePartitionCommit(string name, *hash<auto> opt);
AbstractTable detachPartitionCommit(string name, *hash<auto> opt);
```

`verified_by` is a `PartitionVerifiedBy` enum value. `bounds_verified` is a derived convenience
alias — it is `True` exactly when `verified_by == PartitionVerifiedBy::Bounds`, never set
independently — so callers may test either; they must always agree.

`ensurePartition()` is the method the Qorus monthly job should call to "roll forward":

1. call `findPartitionBySpec(spec)`;
2. if lookup returns `Found`, return the matched partition with `created = False`,
   `materialized = True`, `bounds_verified = True`, `verified_by = PartitionVerifiedBy::Bounds`;
3. if lookup returns `Uncomparable`, fall back to the canonical name: `ensurePartition()` is
   given `name`, so check `listPartitions(){name}` — if a partition with that exact name
   exists, return it with `created = False`, `materialized = True`,
   `bounds_verified = False`, `verified_by = PartitionVerifiedBy::Name`; only if the name is also absent (and the
   auto strategy in step 4 does not cover it) proceed to create. Name fallback is deterministic
   identity resolution, not a bound match; callers that require bound proof can reject
   `bounds_verified = False`. Throw `PARTITION-ERROR` only if neither bounds nor name can be
   resolved deterministically;
4. if the backend has an auto-create strategy that covers the requested range and no
   materialized partition exists yet, return `created = False`, `materialized = False`,
   `deferred_auto = True`, `bounds_verified = False`, `verified_by = PartitionVerifiedBy::Auto`;
5. otherwise create the partition and return it with `created = True`,
   `materialized = True`, `bounds_verified = True`, `verified_by = PartitionVerifiedBy::Bounds` when the created
   partition can be resolved by bounds, or `bounds_verified = False`, `verified_by = PartitionVerifiedBy::Name`
   when only canonical-name verification is possible.

**Concurrency (TOCTOU).** Qorus archiving may run the roll-forward concurrently on multiple
instances, so the existence check and the `CREATE` race. `ensurePartition()`
must be deterministic, not check-then-hope: if the `CREATE` fails because the partition already
exists (driver "relation/partition already exists" error), it must **treat that as success** —
re-resolve via `findPartitionBySpec()`, and if that is still `Uncomparable`, via the canonical
name in `listPartitions(){name}` — and return `created = False`, `materialized = True` when
either resolves. The returned `verified_by` / `bounds_verified` fields must reflect which
resolution path succeeded. Only if neither bounds nor name resolve does it throw
`PARTITION-ERROR`. It must distinguish the specific duplicate error from other failures (which
still propagate). This is the deterministic resolution; no retry loop or polling is used.

### Per-driver mapping of the uniform operations

| Operation                | PostgreSQL                                   | Oracle                              | MySQL                              |
|--------------------------|----------------------------------------------|-------------------------------------|------------------------------------|
| `addPartition`           | `CREATE TABLE child PARTITION OF parent FOR VALUES FROM … TO …` | `ALTER TABLE … ADD PARTITION … VALUES LESS THAN …` | `ALTER TABLE … ADD PARTITION (…)` |
| `dropPartition` (uniform)| verify child belongs to parent, then `DROP TABLE child` | `ALTER TABLE … DROP PARTITION p`    | `ALTER TABLE … DROP PARTITION p`  |
| `truncatePartition`      | `TRUNCATE TABLE child`                        | `ALTER TABLE … TRUNCATE PARTITION p`| `ALTER TABLE … TRUNCATE PARTITION p` |
| `detachPartition`        | `ALTER TABLE … DETACH PARTITION child` → standalone table | unsupported (throws)    | unsupported (throws)               |

This is what principle 1 buys: on PostgreSQL `dropPartition()` deliberately uses
`DROP TABLE child` (data gone), matching Oracle/MySQL `DROP PARTITION`. The detach-and-keep
behaviour is only reachable through the explicit, capability-gated `detachPartition()`.

For Oracle/MySQL range partitioning, DDL often declares only the upper bound. The portable
spec still carries both lower and upper bounds so SqlUtil can identify the partition
unambiguously, verify that the intended range is contiguous with existing partitions, and
avoid dropping the wrong driver-generated partition.

Default-partition caveat: when a default/catch-all partition exists, `addPartition()` is not a
pure metadata operation on every backend. PostgreSQL must scan the default partition for rows
that would belong in the new range and takes a strong lock while doing so; Oracle/MySQL behave
differently again. In MySQL, a catch-all `MAXVALUE` range partition generally means adding a
new upper range requires `ALTER TABLE … REORGANIZE PARTITION …` rather than plain
`ADD PARTITION`. Implementations should document this per driver and surface row-conflicts or
unsupported catch-all reshaping as `PARTITION-ERROR` rather than letting raw DDL errors escape.

### Action codes

No new `AC_*` constants are required for phase 1. Use the existing integer action codes from
`AbstractDatabase` with object type `"partition"`:

```qore
getAddPartitionSql()      -> AC_Add,      type "partition"
getDropPartitionSql()     -> AC_Drop,     type "partition"
getTruncatePartitionSql() -> AC_Truncate, type "partition"
getDetachPartitionSql()   -> AC_Modify,   type "partition"
```

If a future callback consumer needs to distinguish detach from other modifications without
looking at the object type/name/table context, add a new integer action code and update
`ActionMap`, `ActionDescMap`, and `ActionLetterMap` together.

## Exceptions

- `PARTITION-NOT-SUPPORTED` — operation requested on a driver/server without the capability
  (for example `detachPartition()` on Oracle, any partition op on SQLite).
- `PARTITION-ERROR` — semantic error (for example `addPartition()` for a name that already
  exists, dropping a non-existent partition, a spec with invalid range bounds, or a range that
  overlaps an existing partition).
- `DESCRIPTION-ERROR` — invalid partition metadata in a table-description hash.
- `OPTION-ERROR` — invalid partition lifecycle option.

## Abstract `…Impl()` methods drivers override

Defaults on `AbstractTable` throw `PARTITION-NOT-SUPPORTED` for lifecycle SQL generation and
return `False` for capabilities. Partition-capable drivers override:

```qore
private *PartitionStrategy getPartitionStrategyImpl();
private hash<string, PartitionInfo> getPartitionsImpl();
private PartitionLookupResult findPartitionBySpecImpl(hash<PartitionSpec> spec);
private bool partitionAutoStrategyCoversSpecImpl(hash<PartitionSpec> spec);

private softlist<auto> getAddPartitionSqlImpl(string name, hash<PartitionSpec> spec, *hash<auto> opt);
private softlist<auto> getDropPartitionSqlImpl(string name, *hash<auto> opt);
private softlist<auto> getTruncatePartitionSqlImpl(string name, *hash<auto> opt);
private softlist<auto> getDetachPartitionSqlImpl(string name, *hash<auto> opt);

private bool supportsPartitionsImpl();
private bool supportsPartitionDetachImpl();
private bool supportsAutoPartitioningImpl();
```

`findPartitionBySpecImpl()` may be implemented generically by scanning `listPartitions()` if
the driver returns canonical typed bounds. Drivers with lossy or native-only bound metadata
should override it so they can return `Uncomparable` with a useful reason instead of silently
missing a partition.

`partitionAutoStrategyCoversSpecImpl()` answers `ensurePartition()` step 4: it returns `True`
when the table's declared auto-create strategy (e.g. Oracle interval) will materialize a
partition covering the requested spec on insert, so no explicit `CREATE` is needed. Drivers
without auto-partitioning inherit the default `False`.

The table-create path (`getCreateTableSqlImpl()` and, where needed, create-misc SQL) is
extended in partition-capable drivers to emit `PARTITION BY …` and initial partition clauses
when the description carries `partition_strategy` and `partitions`, mirroring how
`data_tablespace` is appended today.

## Qorus archiving usage

```qore
# one-time: create the partitioned table (or describe an existing one)
# monthly roll-forward job — correct whether the backend auto-creates or not:
PartitionEnsureResult ensured = table.ensurePartition("y2026m07", <PartitionSpec>{
    "bound_from": 2026-07-01,
    "bound_to": 2026-08-01,
});

# archive the oldest month by bounds so driver-generated names are handled correctly:
PartitionLookupResult oldest = table.findPartitionBySpec(<PartitionSpec>{
    "bound_from": 2026-01-01,
    "bound_to": 2026-02-01,
});
if (oldest.status == PartitionLookupStatus::Found) {
    table.dropPartition(oldest.partition.name);
} else if (oldest.status == PartitionLookupStatus::Uncomparable) {
    throw "PARTITION-ERROR", oldest.reason ?? "partition bounds could not be compared";
}

# PostgreSQL only, when the data must be preserved to cold storage first:
if (table.supportsPartitionDetach()) {
    AbstractTable archived = table.detachPartition("y2026m01");
    # ... export `archived`, then archived.drop()
}
```

For PostgreSQL deployments with deterministic partition names, dropping by known name is
also valid:

```qore
table.dropPartition("y2026m01");
```

## Schema / SchemaReverse round-trip

Qorus schemas are frequently defined as `Schema`-module table descriptions (including `.ysm` /
`.jsm` data-schema files) and reverse-engineered from live databases with `SchemaReverse`. For
a partitioned archive table to round-trip — *define → align → reverse-engineer → re-align* —
the schema tooling must be partition-aware. Two of these modules **do not pass new
table-description keys through transparently** (verified in source), so they need explicit
changes; the rest are pass-through.

| Module | File | Behavior today | Change required |
|--------|------|----------------|-----------------|
| `AbstractSchema` | `qlib/Schema/AbstractSchema.qc` | passes the whole table hash to `db.getAlignSql()` | none (transparent once SqlUtil whitelists the keys) |
| `DataSchema` | `qlib/Schema/DataSchema.qc` | delegates to loader/parent | none |
| `DataSchemaLoader` | `qlib/Schema/DataSchemaLoader.qc` (`normalizeTable()`, ~L202) | rebuilds into a fresh `hash<auto!>` with an **explicit per-key allow-list**; unlisted keys are silently dropped | **add** `partition_strategy` and `partitions` pass-through |
| `DataSchemaMetaSchema` | `qlib/Schema/DataSchemaMetaSchema.qc` (`tableDefinition`, ~L259) | JSON-Schema (Draft 2020-12) validation of `.ysm`/`.jsm`; unknown table keys are currently allowed by JSON Schema defaults | **add** `partitionStrategy` / `partitionSpec` `$defs` and `partition_strategy` / `partitions` table properties for validation, documentation, and future strictness |
| `SchemaReverse` | `qlib/SchemaReverse.qm` (`TableReverse.toQore()`, ~L508) | **explicitly enumerates** columns/indexes/primary_key/triggers/foreign_constraints | **add** extraction via `getPartitionStrategy()` / `listPartitions()` |

Requirements for the changes:

- **`DataSchemaLoader::normalizeTable()`** — add pass-through of `partition_strategy` and
  `partitions` (the latter keyed by partition name → `PartitionSpec`), and apply the same
  shorthand/normalization conventions used for columns where applicable. Without this, a schema
  file's partition definitions are dropped before they ever reach SqlUtil.
- **`DataSchemaMetaSchema`** — add `partitionStrategy` and `partitionSpec` definitions matching
  the hashdecls in this document (note: `is_default`, not `default`; range-only `method` enum
  for phase 1) and reference them from `tableDefinition` properties named
  `partition_strategy` and `partitions`. The current meta-schema does not reject unknown table
  keys because it does not set `additionalProperties: false`, but explicit definitions are
  still required for schema documentation, editor validation, and any future strict mode.
- **`SchemaReverse::TableReverse`** — extract partitioning into the emitted description. Because
  reverse-engineering an archive table can surface thousands of monthly partitions, gate the
  per-partition emission behind a **`with_partitions` Qore option (default off)**: by default
  emit `partition_strategy` only (enough to re-create a correctly partitioned but empty parent
  only on drivers that support parent-only creation, such as PostgreSQL in phase 1) and emit
  the individual `partitions` list only when explicitly requested. Bounds that cannot be
  represented portably must be emitted as `bound_from_sql` / `bound_to_sql` (with `bound_sql`
  retained for diagnostics), consistent with the introspection contract above.
  Implementation must add option plumbing because SchemaReverse constructors currently do not
  carry per-table reverse options: add an optional options hash to `TableReverse`,
  `TablesReverse`, top-level `SchemaReverse`, and `get_object()` / CLI creation paths; the CLI
  spelling should be `--with-partitions`, mapped to Qore option `with_partitions`.

`SqlUtilDataProvider` and other consumers that round-trip descriptions inherit partition
awareness automatically once `partition_strategy` / `partitions` are in
`TableDescriptionHashOptions` and `getDescriptionHash()` — they do not maintain their own
key allow-lists.

## Phasing

1. **Phase 1 — abstraction + PostgreSQL range partitions.** Capability flags, hashdecls,
   table-description keys, abstract methods, `listPartitions()`, `findPartitionBySpec()`,
   `addPartition`/`ensurePartition`/`dropPartition`/`truncatePartition`/`detachPartition`
   (plus their `…Commit()` companions), `PARTITION BY` in create DDL, and
   `getDescriptionHash()` round-trip support. **Schema tooling (same phase, since it gates
   defining partitioned tables from schema files):** `DataSchemaLoader::normalizeTable()`
   pass-through, `DataSchemaMetaSchema` definitions, and `SchemaReverse::TableReverse`
   extraction with the `with_partitions` / `--with-partitions` toggle.
2. **Phase 2 — Oracle + MySQL range partitions.** Implement uniform ops; Oracle adds interval
   `auto` strategy (`supportsAutoPartitioning`); both throw `PARTITION-NOT-SUPPORTED` for
   `detachPartition`.
3. **Phase 3 and beyond — MSSQL / other backends, list/hash partitioning, partition exchange,
   and scalability follow-ups** are tracked in
   [`sqlutil-partitions-deferred.md`](sqlutil-partitions-deferred.md).

## Tests

Phase 1 tests should cover:

- creating a range-partitioned PostgreSQL table from a table-description hash;
- describing a partitioned parent and round-tripping `partition_strategy` / `partitions`
  through `getDescriptionHash()`, including that `partitions` is omitted by default and
  included only when the `with_partitions` option is set on
  `getDescriptionHash({"with_partitions": True})`;
- `getDescriptionHash()` remains backward-compatible when called without options and rejects
  unknown description options with `OPTION-ERROR`;
- `listPartitions()` returns range partitions in `PartitionInfo.ordinal` order (default
  partition last), including a native-bound case where ordinal is available but typed bounds
  are not;
- `listPartitions()` and `findPartitionBySpec()` with typed bounds, raw `bound_sql`, and all
  `PartitionLookupStatus` outcomes (`Found`, `NotFound`, `Uncomparable`);
- `ensurePartition()` idempotence for an existing partition, creation of a missing one, and
  correct `PartitionEnsureResult` flags, including `bounds_verified` and `verified_by`;
- dropping the oldest partition and verifying data removal;
- truncating a partition while retaining the partition object;
- detach round-trip on PostgreSQL, including the returned standalone `AbstractTable`;
- negative tests for unsupported drivers, duplicate names, missing partitions, invalid bounds,
  overlapping ranges, default partition misuse, composite-bound cardinality mismatch, a
  `partition_strategy.columns` entry that is not a declared column, and a PK/UNIQUE constraint
  that omits a partition key column (both `DESCRIPTION-ERROR` at `setupTable()`);
- `ensurePartition()` with an `Uncomparable` bound lookup resolving by canonical name via
  `listPartitions(){name}` and returning `verified_by = PartitionVerifiedBy::Name`, `bounds_verified = False`;
- quoted identifiers and schema-qualified parent tables;
- PostgreSQL refusal to drop a standalone table that is not a child partition of `self`;
- callback SQL generation/action metadata for add/drop/truncate/detach;
- cache invalidation after add/drop/detach and after `ensurePartition()` creates (stale
  partition structure must not survive a successful lifecycle call);
- `truncatePartition()` retains the partition object and does not invalidate structural
  partition metadata unless data-dependent partition metadata is later added;
- bare-vs-`…Commit()` transaction behavior (bare executor leaves the change uncommitted in the
  caller's transaction where the driver allows it; `…Commit()` commits/rolls back);
- `ensurePartition()` concurrent-create race: a second caller racing the `CREATE` resolves to
  `created = False`, `materialized = True` rather than throwing;
- driver restrictions around primary/unique keys on partitioned tables, validated at
  `setupTable()` (PK/UNIQUE must include the partition key);
- **Schema round-trip:** a `.ysm`/`.jsm` file declaring `partition_strategy` + `partitions`
  validates against `DataSchemaMetaSchema` and survives `DataSchemaLoader::normalizeTable()`
  (keys not dropped); a partitioned table created through a `Schema` aligns correctly;
- **Schema bound coercion:** ISO date/datetime strings in `.ysm`/`.jsm` partition bounds are
  coerced according to the target partition column type, native SQL bounds are not coerced,
  and ambiguous typed-plus-SQL bound input is rejected;
- **SchemaReverse:** reverse-engineering a partitioned table emits `partition_strategy` by
  default and the full `partitions` list only with Qore option `with_partitions` / CLI flag
  `--with-partitions`, with non-portable bounds emitted as `bound_*_sql`;
- **SchemaReverse option plumbing:** `TableReverse`, `TablesReverse`, top-level
  `SchemaReverse`, `get_object()`, and CLI construction all pass the reverse options through
  consistently.

## Open items

Open design questions and all deferred / future scope (additional methods, backends, partition
exchange, `DETACH … CONCURRENTLY`, and scalability follow-ups) are collected in
[`sqlutil-partitions-deferred.md`](sqlutil-partitions-deferred.md).
