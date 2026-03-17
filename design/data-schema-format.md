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
| `blob` | Binary large object | `SqlUtil::BLOB` |
| `clob` | Character large object | `SqlUtil::CLOB` |
| `binary` | Binary data | `Type::Binary` |

### Column Properties

```yaml
columns:
  my_column:
    type: varchar          # Required: column type
    size: 240              # Optional: column size (required for varchar, char)
    scale: 2               # Optional: numeric scale
    notnull: true          # Optional: NOT NULL constraint
    default_value: "A"     # Optional: default value
    comment: "Description" # Optional: column comment
```

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

    indexes:                 # Optional: secondary indexes
      idx_name:
        columns: [name]
        unique: true

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

## Auto-Triggers

Auto-triggers generate standard database triggers for common patterns:

- **`created: true`** — populates a `created` column with the current timestamp on INSERT
- **`modified: true`** — populates a `modified` column with the current timestamp on INSERT and UPDATE
- **`sequence_columns`** — populates columns from sequences when NULL on INSERT

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

### Migration Actions

**Data-driven actions** (allowed at any access level):

| Action | Required Fields | Description |
|--------|----------------|-------------|
| `update` | `table`, `set`, `where` | Update rows |
| `insert` | `table`, `row` or `rows` | Insert rows |
| `delete` | `table`, `where` | Delete rows |
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

**Restricted actions** (require `trusted` or `admin` access level):

| Action | Required Fields | Description |
|--------|----------------|-------------|
| `sql` | `statements` | Execute raw SQL |
| `script` | `file` | Execute a .qr migration script |

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
