# Data Schema Format Specification (.ysm / .jsm)

## Overview

The data schema format provides a pure data representation of database schemas using YAML (`.ysm`) or
JSON (`.jsm`) files. It is processed by the `Schema::DataSchema` class and managed via the `qschema`
CLI tool.

This format replaces the need for Qore code (`.qsm` modules) for schema definitions, making schemas
accessible to non-programmers and enabling schema management without code execution.

## File Extensions

- `.ysm` — YAML Schema Module (requires the `yaml` binary module)
- `.jsm` — JSON Schema Module (requires the `json` binary module)

## Top-Level Structure

```yaml
schema:           # Required: schema metadata
tables:           # Optional: table definitions
sequences:        # Optional: sequence definitions
types:            # Optional: type definitions
functions:        # Optional: function definitions
procedures:       # Optional: procedure definitions
packages:         # Optional: package definitions (Oracle)
materialized_views: # Optional: materialized view definitions
reference_data:   # Optional: reference/seed data
migrations:       # Optional: version-based migration steps
index_options:    # Optional: index creation options
column_options:   # Optional: column creation options
```

## Schema Metadata

```yaml
schema:
  name: MySchema              # Required: schema name
  version: "1.0.0"            # Required: version string
  datasource: my-ds           # Required for oload; ignored by qschema
                              #   (qschema takes the connection as a CLI arg; oload resolves
                              #    this alias via its -D<alias>=<url> option map)
  version_table: schema_info  # Optional: table holding version (default: schema_version)
  version_column: version     # Optional: column holding version (default: version)
  version_where:              # Optional: where clause to find version row
    name: MySchema            #   default: {name: <schema_name>}
```

## Column Types

Supported column types:

| Type | Description | GenericColumnInfo mapping |
|------|-------------|--------------------------|
| `int` | Integer | `Type::Int` |
| `varchar` | Variable-length string (requires `size`) | `SqlUtil::VARCHAR` |
| `char` | Fixed-length string (requires `size`) | `SqlUtil::CHAR` |
| `timestamp` | Date/time with timezone | `Type::Date` |
| `date` | Date only (Oracle/PostgreSQL native) | `Type::Date` + driver override |
| `number` | Numeric (optional `size`, `scale`) | `SqlUtil::NUMERIC` |
| `float` | Floating-point (double precision on pgsql) | `Type::Float` |
| `blob` | Binary large object | `SqlUtil::BLOB` |
| `clob` | Character large object | `SqlUtil::CLOB` |
| `text` | Native TEXT on pgsql/mysql; CLOB elsewhere | `SqlUtil::CLOB` + driver override |
| `binary` | Binary data | `Type::Binary` |
| `vector` | pgvector dense vector (pgsql only) | `native_type: vector` |
| `halfvec` | pgvector half-precision vector (pgsql only) | `native_type: halfvec` |
| `sparsevec` | pgvector sparse vector (pgsql only) | `native_type: sparsevec` |

When a PostgreSQL schema template uses any pgvector type, schema alignment emits
`CREATE EXTENSION IF NOT EXISTS vector` before table DDL if the extension is not already installed
in the target database.

### Column Properties

```yaml
columns:
  my_column:
    type: varchar          # Required: column type (or use driver: instead)
    size: 240              # Optional: column size (required for varchar, char)
    scale: 2               # Optional: numeric scale
    notnull: true          # Optional: NOT NULL constraint
    default_value: "A"     # Optional: default value
    comment: "Description" # Optional: column comment
    populate: {field: name} # Optional: backfill expression for adding to non-empty tables
```

### Populate Expressions

When adding a NOT NULL column to a non-empty table, the database rejects the `ALTER TABLE ADD
COLUMN` because existing rows would be NULL. The `populate` attribute automates the 3-step
migration pattern: add the column as nullable, backfill existing rows, then set NOT NULL.

The alignment layer only uses `populate` when the column is **genuinely new** (not already in the
database). On subsequent alignments the column already exists and `populate` is ignored. The
backfill uses `WHERE col IS NULL` for idempotency.

Supported forms:

```yaml
# Field reference — copy from another column
populate: {field: name}
# → UPDATE t SET display_name = name WHERE display_name IS NULL

# Literal value — fill with a constant
populate: "active"
# → UPDATE t SET status = 'active' WHERE status IS NULL

# DPQL expression — rendered to SQL via the driver's expression map when possible,
# otherwise evaluated client-side and passed as a bind value
populate: {exp: upr, args: [{field: name}]}
# → UPDATE t SET upper_name = upper(name) WHERE upper_name IS NULL

populate: {exp: now}
# → UPDATE t SET created = current_timestamp WHERE created IS NULL

populate: {exp: coalesce, args: [{field: legacy_status}, "unknown"]}
# → UPDATE t SET status = coalesce(legacy_status, 'unknown') WHERE status IS NULL

# Cryptographic digest — portable DPQL expression that renders to each database's native
# hashing functions; args are (value, algorithm, output-encoding).  The output encoding is
# "hex" (the default), "base64", or "binary"; use "binary" to fill a binary/bytea column.
populate: {exp: digest, args: [{field: name}, "sha256", "binary"]}
# → UPDATE t SET name_sha256 = sha256(convert_to(name, 'UTF8')) WHERE name_sha256 IS NULL  (on pgsql)

# Per-driver SQL escape hatch — for DB-specific functions not in the expression map
populate:
  sql:
    pgsql: "digest(name, 'sha256')"
    oracle: "DBMS_CRYPTO.HASH(UTL_RAW.CAST_TO_RAW(name), 4)"
    mysql: "UNHEX(SHA2(name, 256))"
# → UPDATE t SET name_sha256 = digest(name, 'sha256') WHERE name_sha256 IS NULL  (on pgsql)
```

The `digest` expression maps to native server-side hashing per driver, raising `DIGEST-ERROR` for
combinations a database cannot support:

| Driver | Algorithms | Output encodings |
| --- | --- | --- |
| PostgreSQL | md5, sha224, sha256, sha384, sha512 | hex, base64, binary |
| Oracle | md5, sha1, sha256, sha384, sha512 | hex, binary |
| MySQL | md5, sha1, sha224, sha256, sha384, sha512 | hex, base64, binary |
| MS SQL Server | md5, sha1, sha256, sha512 | hex, binary |
| Firebird (4+) | md5, sha1, sha256, sha512 | hex, base64, binary |
| SQLite | *(none — no built-in hash functions)* | — |

`populate` works with or without `notnull`. If `notnull: true` is set, the constraint is applied
after the backfill. If `notnull` is not set, the backfill runs but no NOT NULL constraint is added.

## Table Definition

```yaml
tables:
  my_table:
    columns:                 # Required: column definitions
      id: {type: int, notnull: true}
      name: {type: varchar, size: 240}

    primary_key:             # Optional: primary key
      name: pk_my_table
      columns: [id]

    unique_constraints:      # Optional: named unique constraints (may be backed by an
      uk_my_table:           #   index of the same name)
        columns: [name]

    indexes:                 # Optional: secondary indexes
      idx_name:
        columns: [name]
        unique: true

      # Example: pgvector IVFFlat index with method / opclass / WITH storage options
      sk_chunks_embedding:
        columns: [embedding]
        method: ivfflat                       # access method (pgsql / mysql / oracle / mssql)
        with:                                 # method-specific storage options
          lists: 100
        opclass:                              # per-column operator class (pgsql only)
          embedding: vector_cosine_ops
        driver:                               # per-driver override escape hatch
          pgsql:
            method: ivfflat

    foreign_constraints:     # Optional: foreign key constraints
      fk_other:
        columns: [other_id]
        table: other_table
        target_columns: [id]  # Optional: defaults to same as columns

    auto_triggers:           # Optional: auto-generated triggers
      created: true          # Auto-set created timestamp on INSERT
      modified: true         # Auto-set modified timestamp on INSERT/UPDATE
      sequence_columns:      # Auto-populate from sequence when NULL
        id: seq_my_table

    triggers:                # Optional: raw trigger definitions
      driver:                # Per-driver trigger SQL
        pgsql: {trig_name: "trigger sql..."}

    driver:                  # Optional: driver-specific overrides
      oracle:
        columns:
          data: {type: clob, comment: "Oracle-specific"}
```

## Table Partitioning

Tables can declare portable range, list, or explicit hash partitioning with `partition_strategy`
and optional initial physical partitions with `partitions`. A recursive `subpartition` strategy and
matching `subpartitions` definitions are supported when the database advertises equivalent semantics.

```yaml
tables:
  archive_events:
    columns:
      id: {type: int, notnull: true}
      event_ts:
        type: date
        notnull: true
        driver:
          mysql: {native_type: datetime}
      payload: {type: text}

    primary_key:
      name: pk_archive_events
      columns: [id, event_ts]

    partition_strategy:
      method: range
      columns: [event_ts]

    partitions:
      archive_events_202601:
        bound_from: "2026-01-01"
        bound_to: "2026-02-01"

      archive_events_202602:
        bound_from_sql: "date '2026-02-01'"
        bound_to_sql: "date '2026-03-01'"

      archive_events_default:
        is_default: true
```

`partition_strategy.columns` defines the partition key. `partitions` is keyed by partition name;
each partition can also set `name`, but it must match the hash key. `bound_from` is inclusive and
`bound_to` is exclusive. Use scalar bounds for single-column keys and lists for multi-column keys.
When the effective driver-specific partition key column has type `date` or `timestamp`, string
`bound_from` and `bound_to` values in `YYYY-MM-DD` or ISO datetime form
(`YYYY-MM-DD[T ]HH:mm:SS[.ffffff][Z|+/-HH:mm]`) are normalized to typed dates when the table is set up.
Other strings remain strings. SQL expressions are never parsed and must use `bound_from_sql`,
`bound_to_sql`, or `bound_sql`.

List specs use exactly one of `values` (a nonempty typed list) or `values_sql`; a default list
partition uses only `is_default: true`. Explicit hash specs use `modulus` and `remainder`, with
`modulus > 0` and `0 <= remainder < modulus`. For example, PostgreSQL list-to-hash partitioning can
be described as:

```yaml
partition_strategy:
  method: list
  columns: [region]
  subpartition:
    method: hash
    columns: [customer_id]

partitions:
  region_eu:
    values: [eu, uk]
    subpartitions:
      region_eu_h0: {method: hash, modulus: 2, remainder: 0}
      region_eu_h1: {method: hash, modulus: 2, remainder: 1}
```

Portable method support is capability-gated: PostgreSQL supports range/list/hash and recursive
subpartitions; Oracle and MySQL/MariaDB support range and list at the top level; SQL Server supports
range only. Native hash algorithms on Oracle/MySQL are not treated as equivalent to explicit
modulus/remainder placement.

Partition metadata supports the same `driver` override pattern used elsewhere in schema hashes:

```yaml
partition_strategy:
  method: range
  columns: [event_ts]
  driver:
    mysql:
      columns: [event_ts]

partitions:
  archive_events_202601:
    bound_to: "2026-02-01"
    driver:
      oracle:
        bound_to_sql: "timestamp '2026-02-01 00:00:00'"
```

Database-specific restrictions still apply. For MySQL and MariaDB, every primary key or unique key
on a partitioned table must include the partition key, and `timestamp` columns are not allowed in
`RANGE COLUMNS` partition keys; use a `date` column or set the MySQL/MariaDB native type to
`datetime` when the partition key needs date/time values.

Schema alignment creates declared initial partitions with the table and can reverse-engineer
partitioned tables. Partition lifecycle operations such as adding, dropping, truncating, detaching,
or comparing physical partitions are exposed by `SqlUtil`; schema diff/alignment does not currently
treat changes under `partitions` as an automatic partition lifecycle migration. By default
reverse-engineering exports only `partition_strategy`; use `SchemaReverse` option
`with_partitions: true` or `qschema export --with-partitions` to include the physical `partitions`
hash.

## Auto-Triggers

Auto-triggers generate standard database triggers for common patterns:

- **`created: true`** — populates a `created` column with the current timestamp on INSERT
- **`modified: true`** — populates a `modified` column with the current timestamp on INSERT and UPDATE
- **`sequence_columns`** — populates columns from sequences when NULL on INSERT
- **`insert_only: true`** — restrict the generated trigger to fire on INSERT only; useful for
  append-only tables and for tables that carry a `modified` column the application maintains by
  hand (without this key, a trigger with `modified: true` also fires on UPDATE)

Trigger SQL is generated automatically for each database driver (PostgreSQL, Oracle, MySQL, MSSQL)
by the `SqlUtil::generate_auto_triggers()` function.

## Reference Data

Reference data is seed/configuration data that is managed alongside the schema:

```yaml
reference_data:
  strict:          # Only these rows allowed (others are deleted)
    table_name:
      columns: [col1, col2]
      rows:
        - [value1, value2]

  normal:          # Must exist (additional rows OK)
    table_name:
      columns: [col1, col2]
      rows:
        - [value1, value2]

  create_only:     # Only inserted on initial schema creation
    ...

  insert_only:     # Inserted if missing, never updated
    ...
```

### DPQL Expressions in Reference Data

Values in reference data rows can use DPQL expressions (from `DataProvider::GenericExpressionImplementations`):

```yaml
rows:
  - [MyApp, "1.0.0", {exp: "now"}]                        # Current timestamp
  - [total, {exp: "+", args: [10, 20]}]                    # Arithmetic: 30
  - [greeting, {exp: "upr", args: ["hello"]}]             # String: "HELLO"
  - [code, {exp: "substr", args: ["ABCDEF", 0, 3]}]       # String: "ABC"
  - [encoded, {exp: "to-base64", args: ["data"]}]          # Base64 encoding
  - [flag, {exp: "ternary", args: [true, "yes", "no"]}]   # Ternary: "yes"
```

Common expressions: `now`, `+`, `-`, `*`, `/`, `upr`, `lwr`, `substr`, `length`,
`toInt`, `toString`, `toNumber`, `to-base64`, `parse_base64`, `ternary`.

See `DataProvider::GenericExpressionImplementations` for the full list of available expressions.

## Migrations

Migrations define version-based data transformations:

```yaml
migrations:
  - version: "1.1.0"
    description: "Normalize status codes"
    pre_align:           # Executed BEFORE DDL changes
      - action: update
        table: users
        set: {new_status: "A"}
        where: {status: "Active"}
    post_align:          # Executed AFTER DDL changes
      - action: update
        table: users
        set: {display_name: "unknown"}
        where: {display_name: null}
```

A migration's `version` is the schema version it advances the database **to**: it runs whenever
the database's current (source) version is in the range `(current_version, target_version]`.

### Source-Version Gating

A migration runs for **every** source version older than its `version` — including versions that
predate the objects the migration touches. A `pre_align` step in particular runs before any DDL,
so a step that operates on a table introduced in a later version will fail when upgrading from a
version where that table did not yet exist (the table has not been created yet).

`min_source_version` and `max_source_version` gate a migration (or an individual step) on the
**source** (current) database version. Both bounds are inclusive and compared against the database's
current version, not the target:

```yaml
migrations:
  - version: "1.4.0"
    description: "Backfill embedding_model before adding NOT NULL"
    # the collections table was introduced at 1.2.0; databases older than that create it fresh
    # (already NOT NULL with a default) during DDL alignment and need no backfill
    min_source_version: "1.2.0"
    pre_align:
      - action: update
        table: collections
        set: {embedding_model: "default"}
        where: {embedding_model: null}
```

Gating may also be applied per step, which is useful when a single migration mixes steps that apply
to different source-version ranges:

```yaml
post_align:
  - action: update
    table: users
    min_source_version: "1.1.0"   # only for databases that already had this column
    set: {tier: "free"}
    where: {tier: null}
```

This is distinct from the `populate` column attribute (see [Populate Expressions](#populate-expressions)):
`populate` backfills a **genuinely new** column during DDL alignment, while a source-version-gated
migration handles backfilling an **existing** column before a constraint change, only for databases
where that column already exists.

### Migration Actions

**Data-driven actions** (allowed at any access level):

| Action | Required Fields | Description |
|--------|----------------|-------------|
| `update` | `table`, `set`, `where` or `where_all` | Update rows (`where_all: true` to update all) |
| `insert` | `table`, `row` or `rows` | Insert rows |
| `delete` | `table`, `where` or `where_all` | Delete rows (`where_all: true` to delete all) |
| `insert_from_select` | `table`, `columns`, `source_table`, `select` | Copy data between tables |
| `add_column` | `table`, `column`, `type` | Add a column |
| `drop_column` | `table`, `column` | Drop a column |
| `modify_column` | `table`, `column`, `type` | Modify a column |
| `rename_column` | `table`, `from`, `to` | Rename a column |
| `add_index` | `table`, `name`, `columns`, `unique` | Add an index |
| `drop_index` | `table`, `name` | Drop an index |
| `add_constraint` | `table`, `name`, `constraint_type`, ... | Add a constraint (unique/foreign/check) |
| `drop_constraint` | `table`, `name` | Drop a constraint |
| `rename_table` | `table`, `to` | Rename a table |
| `add_trigger` | `table`, `name`, `source` | Add a trigger |
| `drop_trigger` | `table`, `name` | Drop a trigger |
| `reconcile_table` | `contract` | Apply a closed, additive, data-preserving table reconciliation with bounded backfill, quarantine, indexes, and verification |

**Restricted actions** (require `trusted` or `admin` access level):

| Action | Required Fields | Description |
|--------|----------------|-------------|
| `sql` | `statements` | Execute raw SQL |
| `script` | `file` | Execute a .qr migration script |

#### Additive table reconciliation

`reconcile_table` is a closed, database-independent migration action for
reconciling an existing table with a reviewed canonical definition without
dropping or renaming live data. It is designed for migrations that need more
control than normal table alignment: a complete conversion preflight, bounded
restartable backfill, quarantine of rows that cannot be converted, and
deterministic post-migration verification.

The action accepts one `contract`:

```yaml
- action: reconcile_table
  contract:
    contract_version: 1
    migration_id: users-canonical-id
    table: users
    canonical_columns:
      - name: user_id
        type: int
        notnull: true
        primary_key: true
        missing_live: true
      - name: display_name
        type: string
        size: 240
        notnull: true
        missing_live: true
    column_mappings:
      - target: user_id
        source: id
        source_kind: legacy_column
        conversion: numeric_compatible
      - target: display_name
        source: name
        source_kind: legacy_column
        conversion: to_string
    backfill:
      batch_size: 1000
      quarantine_table: users_schema_quarantine
    constraints_and_indexes:
      primary_key:
        name: pk_users
        columns: [user_id]
      indexes:
        - name: idx_users_display_name
          columns: [display_name]
          unique: false
```

The contract fields are:

| Field | Meaning |
|-------|---------|
| `contract_version` | Must be `1` |
| `migration_id` | Stable identifier used to make quarantine handling idempotent |
| `table` | Existing table to reconcile |
| `canonical_columns` | Closed list of canonical column definitions; supported logical types are `date`, `number`, `int`, `float`, `bool`, `string`, and `binary` |
| `canonical_columns[].missing_live` | Records that the reviewed evidence found the column absent; only these columns may be added or have their nullability tightened |
| `column_mappings` | Exactly one source mapping for every canonical column |
| `backfill.batch_size` | Number of rows processed per restartable transaction, from 1 through 10,000 |
| `backfill.quarantine_table` | Separate table that preserves rows that fail conversion before removing them from active processing |
| `backfill.preserve_legacy_columns` | Optional safety invariant; when present it must be `true`. Normalized contracts always include it so they can be validated and executed again without weakening legacy-column preservation |
| `constraints_and_indexes.primary_key` | One canonical key column and its reviewed constraint name |
| `constraints_and_indexes.indexes` | Optional bounded list of reviewed secondary indexes |

Mapping `source_kind` values are `existing_column`, `legacy_column`,
`constant_null`, `schema_default`, and `empty_table_initialization`.
`empty_table_initialization` is accepted only when the table is still empty at
execution time. Supported conversion labels are `identity`, `to_timestamp`,
`numeric_compatible`, `to_number`, `string_size_validation`, `to_string`,
`validated_cast`, `none`, and `schema_default`; all conversions still use the
closed target type and size checks.

Before its first material change, the action validates identifiers and bounds,
checks the current table and source columns, requires a unique single-column
cursor for non-empty tables, validates existing index definitions, and scans
every active row for conversion safety. DML is committed in bounded,
restartable batches. A row that cannot be converted is first copied to the
quarantine table with the migration ID and reason, then removed from the
active table in the same transaction. Existing columns are never dropped or
narrowed, and legacy columns are retained.

Successful execution returns bounded aggregate evidence: material change
count, before/after active and quarantine row counts, and deterministic
verification results. It does not return row values. Re-executing the same
contract is idempotent.

Embedding applications can pass `ddl_change_callback` in
`SchemaOptionInfo`, or to `Schema::execute_migration_action()`. The callback
receives the actual datasource object and the affected table names after
committed DDL. This lets an application associate the change with its own
named datasource without putting application-specific names in the
DataSchema. An empty affected-table list means that all cached metadata for
that datasource must be invalidated; this is used for raw SQL and scripts
whose exact DDL targets cannot be determined. If a DDL operation fails, the
callback is invoked conservatively because some database drivers auto-commit
DDL before reporting a later error.

### Step Dependencies

Steps execute in declaration order by default. Optional dependency declarations allow explicit ordering:

```yaml
post_align:
  - id: widen_column
    action: add_column
    table: users
    column: new_status
    type: {type: char, size: 1}

  - id: populate
    depends_on: [widen_column]
    action: update
    table: users
    set: {new_status: "A"}
```

## Access Levels

| Level | Allowed |
|-------|---------|
| `untrusted` | Data-driven actions only; no raw SQL or scripts |
| `trusted` | All data-driven actions + raw SQL + scripts |
| `admin` | Everything including force operations |

## CLI Tool: qschema

```
qschema align <connection> <file>           # Create/upgrade database schema
qschema align --dry-run <connection> <file> # Show SQL without executing
qschema drop <connection> <file>            # Drop schema from database
qschema diff <connection> <file>            # Compare schema to database
qschema validate <file>                     # Validate schema file
qschema info [-v] <file>                    # Show schema summary
qschema export <connection> -o <file>       # Reverse-engineer database
qschema export --with-partitions <connection> -o <file>
```

### Color Output

`qschema` uses `Util::TerminalColor` for colorized terminal output (like `qdp`):
- Additions shown in **green** (`+`)
- Removals shown in **red** (`-`)
- Modifications shown in **yellow** (`~`)
- SQL statements shown in yellow
- Disable with `--no-color` or `-C`

### Dry Run Mode

`--dry-run` (or `-n`) shows the SQL that would be executed without actually modifying the database:

```
$ qschema align --dry-run pgsql:user/pass@host/db schema.ysm
Dry run: MySchema v1.0.0 against pgsql

Changes detected:
  + 3 additions
  ~ 1 modification

SQL:
  CREATE TABLE users (...);
  CREATE INDEX idx_users_email ON users (email);

Total: 4 changes needed
```

### Diff/Comparison Mode

`diff` compares a schema definition against the current database state and reports differences:

```
$ qschema diff pgsql:user/pass@host/db schema.ysm
Comparing MySchema v1.0.0 vs pgsql

Changes detected:
  + 2 additions
  - 1 removal
  = 5 unchanged

SQL:
  ALTER TABLE users ADD COLUMN email VARCHAR(500);
  DROP INDEX idx_old_name;

Total: 3 changes needed
```

Use `-v` for detailed per-object change descriptions.

## Meta-Schema Validation

Schema files are validated against a JSON Schema (Draft 2020-12) definition stored in
`Schema::DataSchemaJsonSchema`. This validates structure, required fields, column types,
and migration action types.
