# SqlUtil Partition Support — Deferred & Future Work

Companion to [`sqlutil-partitions.md`](sqlutil-partitions.md). That document specifies the
**implemented** partition support; this one collects everything that is intentionally **out of
scope, deferred, or an open design question**, so the main design reads as the current contract
and the future work is tracked in one place.

## Implemented baseline (for reference)

Range partitioning is implemented for PostgreSQL, Oracle, and MySQL/MariaDB:
capability flags, hashdecls and table-description keys, `listPartitions()`,
`findPartitionBySpec()`, the `add` / `ensure` / `drop` / `truncate` / `detach` lifecycle (plus
`…Commit()` companions), `PARTITION BY` create DDL, `getDescriptionHash()` round-trip,
DataSchema/DataSchemaMetaSchema/SchemaReverse integration, `bin/qschema export --with-partitions`,
and the data-schema `bin/schema-reverse`. Everything below is **not** part of that contract.

---

## 1. Deferred partitioning methods

Only the `"range"` method is implemented. The following are deferred because their DDL, naming,
and drop/truncate semantics are not yet uniformly specified across drivers. Future work should add
**method-specific `PartitionSpec` spec hashdecls** (or a discriminated `PartitionSpec` family)
rather than overloading the range-oriented fields.

- **List partitioning** — discrete value-set partitions.
- **Hash partitioning** — even distribution across N buckets when there is no natural range key;
  the usual tool for spreading a very large table without time/range semantics.
- **Composite / sub-partitioning** — two-level (e.g. range-then-hash) for very large tables.

## 2. Deferred lifecycle operations

- **Partition exchange** — Oracle `EXCHANGE PARTITION` (and equivalents): swap a partition with a
  standalone table with no data movement. The standard high-throughput bulk-load / ETL pattern;
  deferred along with the metadata to validate that the exchange target has an identical shape.
- **Non-equivalent detach emulation on Oracle/MySQL** — Oracle can approximate `detachPartition()`
  via `EXCHANGE PARTITION`, but it requires a pre-existing target table of identical shape, so the
  semantics are not equivalent to PostgreSQL `DETACH`. Left unsupported (throws
  `PARTITION-NOT-SUPPORTED`) rather than emulated.
- **PostgreSQL `DETACH PARTITION … CONCURRENTLY`** — online detach that avoids the
  `ACCESS EXCLUSIVE` lock the current transactional `DETACH` takes, but it **cannot run inside a
  transaction**, so it does not fit the bare/`…Commit()` transaction model and is deferred. See
  §4 and the open question in §5.

## 3. Deferred backends and read-side unification

- **MSSQL** partition support (phase 3).
- **SQLite / Firebird** — no native partitioning; capability flags stay `False`.
- **Generic select-side `"partition"` option** — unify the existing Oracle read-side
  `"partition"` select qualifier into a driver-generic select option (currently Oracle-only).

## 4. Scalability / large-data caveats (current behavior; optimizations deferred)

The data path scales well — `drop` / `detach` / `truncate` are metadata-only DDL, not row scans —
but the following are known limits, deferred for later optimization:

- **`DETACH PARTITION` takes an `ACCESS EXCLUSIVE` lock** on the parent for its duration; on a
  hot, large table this is an availability cost. The online alternative
  (`DETACH … CONCURRENTLY`, §2) is deferred.
- **Default / catch-all partition + large data:** with a default partition present,
  `addPartition()` stops being a pure metadata op — PostgreSQL scans the default partition for
  rows belonging to the new range under a strong lock, and MySQL needs
  `ALTER TABLE … REORGANIZE PARTITION` instead of a plain `ADD PARTITION`. Current code surfaces
  the conflict as `PARTITION-ERROR`; no automatic reorganize/migration is performed.
- **Metadata layer is O(n) in partition count** (not in data volume): `getPartitionsImpl()`
  introspects all partitions per cache load, `findPartitionBySpec()` is a linear scan over them,
  partition ordering is `O(n log n)`, and `listPartitions()` materializes every `PartitionInfo`
  in memory with no streaming. Fine for hundreds–low-thousands of partitions; not tuned for
  extreme partition cardinality (tens of thousands), where a targeted lookup (e.g. by canonical
  name or a bounds index) would be needed.
- **No data-dependent `PartitionInfo` metadata** — row counts / sizes are not collected, so
  `truncatePartition()` is treated as not invalidating structural partition metadata. Adding
  row-count or size metadata is future work.

## 5. Open design questions

- Whether `detachPartition()` should expose PostgreSQL `DETACH … CONCURRENTLY` as an option
  (it cannot run in a transaction, so it would need a distinct, non-`…Commit()` entry point).
- Whether future list/hash support should use **separate hashdecls per method** or a single
  **discriminated `PartitionSpec` family**.

## 6. Future phasing roadmap

1. **Phase 3 — MSSQL and other backends as needed**, plus the generic select-side `"partition"`
   qualifier unification (§3).
2. **Future method support** — list / hash partitioning, only after method-specific
   `PartitionSpec` fields, validation rules, and drop/truncate semantics are specified (§1).
3. **Scalability follow-ups** — `DETACH … CONCURRENTLY`, targeted `findPartitionBySpec` lookup for
   high partition counts, partition exchange, and data-dependent partition metadata (§2, §4).
