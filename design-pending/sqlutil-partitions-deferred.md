# SqlUtil Partition Roadmap

This is the sole roadmap for partition features not yet implemented. The current architecture and
public contract are recorded in
[`design/sqlutil-partitions.md`](../design/sqlutil-partitions.md).

## Implemented baseline

Range partitioning is implemented for PostgreSQL, Oracle, MySQL/MariaDB, and SQL Server 2016+ with:

- capability flags, typed hashdecls, table-description keys, and semantic alignment checks;
- list/find/add/ensure/drop/truncate/detach lifecycle APIs with commit companions and spec overloads;
- deterministic concurrent-create recovery, exact callback behavior, and exception-safe cache
  invalidation;
- create DDL, introspection, description round-trip, DataSchema/SchemaReverse integration, and CLI
  export with opt-in physical partitions;
- driver-aware unique-key restrictions and Oracle local/global index, PK, unique-constraint,
  tablespace, and `compute_statistics` round-trip;
- exact datasource identity for CTAS/insert-select and structured aggregate coverage for archive
  conversion queries.
- reusable storage-shape validation and equivalent empty-target partition transfer through Oracle
  exchange and SQL Server switch.
- explicit nontransactional PostgreSQL 14+ concurrent detach, pending-state introspection, and
  deterministic `FINALIZE` recovery without retries or polling.

SQLite and Firebird correctly advertise no native partition support.

## 1. Additional partition methods

Only `range` is implemented. Future methods need method-specific typed specs and validation rather
than overloading range bounds:

- **list partitioning** — discrete value sets, including default/catch-all semantics;
- **hash partitioning** — modulus/remainder or backend bucket semantics;
- **composite/subpartitioning** — an explicit hierarchy such as range-then-hash, including
  introspection and lifecycle targeting at each level.

The main unresolved model choice is separate public hashdecls per method versus a discriminated
`PartitionSpec` family. The choice must preserve strict cardinality/type validation and description
round-trip.

## 2. Generic read-side partition qualification

Oracle currently exposes a driver-specific select option for reading a named partition. A generic
read qualifier should be introduced only with explicit per-driver capability checks; it must not
pretend that PostgreSQL child-table reads, Oracle partition clauses, and SQL Server partition
functions are identical when they are not.

## 3. High-cardinality targeted lookup

Current metadata loading is O(n) in partition count and caches all `PartitionInfo` values. This is
appropriate for hundreds or low thousands of partitions. Tables with tens of thousands need a
driver hook that can target canonical name or comparable bounds without first materializing the full
catalog. Generic behavior must remain deterministic and fall back to the current scan when a driver
cannot perform a targeted lookup.

## 4. Optional data-dependent metadata

Optional partition row-count and physical-size metadata may be useful for archive observability, but
collecting it can be expensive or approximate. It must be opt-in, label exact versus estimated values,
and define cache invalidation for truncate and data changes. Structural `listPartitions()` should stay
cheap by default.

## Implementation order

1. Generic read-side qualification with honest capabilities.
2. Targeted lookup hooks for high partition counts.
3. List/hash/subpartition type families and driver implementations.
4. Optional row-count/size metadata.

Each item requires SQL generation tests, negative/capability tests, live backend tests where
available, documentation/release notes, and a passing full SqlUtil regression run before the next
item begins.
