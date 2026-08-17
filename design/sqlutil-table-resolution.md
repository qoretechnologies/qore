# SqlUtil Table Name Resolution

## Status

Implemented in SqlUtil 3.2.0. This record describes the shipped contract for turning a table *name*
appearing in a query into a live `AbstractTable`, and why the resolution is pluggable rather than
cached inside SqlUtil. Public API documentation is generated from `qlib/SqlUtil`; the behaviour is
covered by `examples/test/qlib/SqlUtil/SqlUtilSelectGeneration.qtest`.

## Why a name has to be resolved at all

A query can refer to another table in two ways:

| Form | Table given as |
|---|---|
| `join_inner(table_object, ...)` | an `AbstractTable` |
| `join_inner("table_name", ...)` | a name |
| `op_in_select(table_object, sh)` | an `AbstractTable` |
| `op_in_select("table_name", sh)` | a name |
| `in_select` advanced expression | **always** a name |

The last row is the important one. `DataProviderExpression` is *data*: it is serialized, stored, sent
between processes, and produced by parsing DPQL text, so its `in_select` operator declares its table
argument as `DataProviderSignatureStringValueType` — a string. A caller that already holds a resolved
table object must therefore downgrade it to a name, and SqlUtil has to turn that name back into an
object.

Resolving a name means creating an `AbstractTable`, and a new table object reads its **entire
description** — columns, primary key, indexes, constraints — from the database the first time SQL is
generated for it. Rendering the SQL for a query with two named sub-selects issued eleven catalog
queries *per generation* before this contract existed, which is why SQL generation, not execution,
dominated the cost of such queries.

## Why the cache does not live in SqlUtil

`AbstractTable` keeps its description for its whole lifetime; that is the documented model, and the
owner of the object decides when to discard it. A cache held inside SqlUtil breaks that model,
because the cached objects are not reachable by the code that knows the schema changed:

- keyed per parent table, the same child is resolved once per parent (N parents x M names objects);
- an application invalidating its own registry entry for table `T` cannot reach the copy of `T` held
  by SqlUtil, so SQL generated for a query naming `T` keeps using the old description.

The second point is a correctness defect, not just waste: after an `alter table`, generation either
fails with `COLUMN-ERROR` for a new column or silently emits SQL built from stale metadata.

So SqlUtil retains nothing by default. Resolution is delegated instead.

## Resolution order

`AbstractTable::getSubtableFromString()` resolves in this order:

1. the `"tablecode"` callback option, if present. Note that `"tablecode"` is a
   `SqlDataCallbackOptions` member, i.e. a *SQL data callback* option — it reaches generation only
   through create-as-select and set-operation APIs and **cannot be passed to an ordinary select
   call**. It is therefore not a usable hook for the sub-selects of a normal query, which is why the
   registered resolver exists.
2. the resolver registered with `sqlutil_register_table_resolver()`, if any.
3. otherwise a new table object, created and discarded per call.

The resolver signature is `AbstractTable (AbstractDatasource ds, string name)` — the connection the
query runs on and the name exactly as written in the query.

## Choosing a resolver

**An application with its own table registry should register it.** Its own invalidation then governs
sub-select resolution as well, there is one cache rather than two, and no object is duplicated. This
is the intended integration: it mirrors `sqlutil_register_ds_serializer()` /
`sqlutil_register_ds_deserializer()`, which already let an application own datasource
serialization.

**An application without one can register `sqlutil_get_cached_table()`**, the caching resolver
shipped with this module. It keys on the connection description
(`AbstractSqlUtilBase::makeDatasourceDesc()`, i.e. `driver:user@db%host:port`) and the table name,
resolves outside its lock, and lets a thread that loses a resolution race adopt the winner's object.
Its cache is discarded with `sqlutil_clear_table_cache(*ds, *name)`, which is the reason the cache is
opt-in: a cache nobody can address cannot be corrected.

## Invariants

1. Nothing is cached unless a resolver is registered. A default installation never serves a stale
   description.
2. `"tablecode"` always wins, so existing callers that pass it are unaffected.
3. A resolver is consulted for *every* name SqlUtil resolves — joins and sub-selects alike, at any
   nesting depth.
4. The resolver is process-wide state. Code that installs one temporarily (tests, tools) must restore
   it, and `sqlutil_clear_table_cache()` with no arguments empties the built-in cache entirely.
5. Resolved tables are shared between threads, so a resolver must be thread-safe and the objects it
   returns must be safe to generate SQL from concurrently — which `AbstractTable` is.

## Measurements

Generation of dashboard-shaped selects against PostgreSQL 18, 300 iterations, best of 5, with the
recursive-reference scan fix of the same release already in place:

| shape | no resolver | caching resolver |
|---|---|---|
| simple `where` (no named sub-select) | 1.11 ms | 1.10 ms |
| join + sub-select | 10.33 ms | 3.76 ms |
| anti-join + 2 sub-selects | 22.12 ms | 4.03 ms |

Catalog queries per generation after the first call: 11 -> 0.

## Related files

- `qlib/SqlUtil/SqlUtil.qm` — `sqlutil_register_table_resolver()`, `sqlutil_get_table_resolver()`,
  `sqlutil_get_cached_table()`, `sqlutil_clear_table_cache()`, `TableCache`, `sqlutil_resolve_table()`.
- `qlib/SqlUtil/AbstractTable.qc` — `getSubtableFromString()` (the single resolution point) and the
  join-resolution call site in `getSelectSqlStringIntern()`.
- `qlib/SqlUtil/SqlUtil.qm` — `OP_IN_SELECT` (both the where-operator and advanced-expression forms).
- `examples/test/qlib/SqlUtil/SqlUtilSelectGeneration.qtest` — resolution order, no-stale-description
  guarantee, cache invalidation, concurrency, and generation-cost tests.
