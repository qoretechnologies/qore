# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DataSchemaTableMigration.qc Data-preserving DataSchema table migrations

/** DataSchemaTableMigration.qc Copyright 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

public namespace Schema {
public namespace DataSchemaTableMigration {

#! Supported migration contract version
public const int ContractVersion = 1;

#! Hard safety bounds for a reviewed migration
public const int MaxColumns = 128;
public const int MaxIndexes = 64;
public const int MaxBatchSize = 10000;

#! Validates and normalizes a reviewed migration contract.
/** The contract is deliberately closed and additive.  It cannot drop a
    column, table, index, or constraint, and it cannot execute raw SQL.

    @param raw untrusted contract embedded in a reviewed remediation plan
    @return normalized contract
    @throw DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR on invalid input

    @par Example
    A reviewed contract maps legacy column @c id to canonical column
    @c instruction_id and declares an artifact-bound quarantine table.  The
    returned contract contains only the closed identifiers, types, mappings,
    constraints, indexes, and bounded backfill controls accepted by execute().
*/
public hash<auto> sub validate_contract(hash<auto> raw) {
    validate_keys(raw, ("contract_version", "migration_id", "table",
        "canonical_columns", "column_mappings", "backfill",
        "constraints_and_indexes"), "migration contract");
    require_type(raw, "contract_version", NT_INT, "migration contract");
    require_type(raw, "migration_id", NT_STRING, "migration contract");
    require_type(raw, "table", NT_STRING, "migration contract");
    require_type(raw, "canonical_columns", NT_LIST,
        "migration contract");
    require_type(raw, "column_mappings", NT_LIST, "migration contract");
    require_type(raw, "backfill", NT_HASH, "migration contract");
    require_type(raw, "constraints_and_indexes", NT_HASH,
        "migration contract");
    if (int(raw.contract_version ?? 0) != ContractVersion) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
            "unsupported remediation migration contract version %y",
            raw.contract_version);
    }
    string table = require_identifier(raw.table, "table");
    string migration_id = require_identifier(raw.migration_id,
        "migration_id", True);
    int batch_size = int(raw.backfill.batch_size ?? 0);
    if (batch_size < 1 || batch_size > MaxBatchSize) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
            "backfill.batch_size must be between 1 and %d", MaxBatchSize);
    }

    list<auto> raw_columns = as_list(raw.canonical_columns);
    list<auto> raw_mappings = as_list(raw.column_mappings);
    if (!raw_columns || raw_columns.size() > MaxColumns
            || raw_columns.size() != raw_mappings.size()) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR",
            "canonical_columns and column_mappings must be non-empty, "
                "equally sized, and within the column limit";
    }

    hash<string, hash<auto>> columns = {};
    list<auto> canonical_columns = ();
    foreach auto raw_spec in (raw_columns) {
        hash<auto> spec = as_hash(raw_spec);
        validate_keys(spec, ("name", "type", "notnull", "primary_key",
            "missing_live", "size", "scale", "default_value"),
            "canonical column");
        require_type(spec, "name", NT_STRING, "canonical column");
        require_type(spec, "type", NT_STRING, "canonical column");
        require_type(spec, "notnull", NT_BOOLEAN, "canonical column",
            True);
        require_type(spec, "primary_key", NT_BOOLEAN, "canonical column",
            True);
        require_type(spec, "missing_live", NT_BOOLEAN, "canonical column",
            True);
        require_type(spec, "size", NT_INT, "canonical column", True);
        require_type(spec, "scale", NT_INT, "canonical column", True);
        string name = require_identifier(spec.name, "canonical column name");
        if (columns{name}) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
                "duplicate canonical column %y", name);
        }
        string type = normalize_type(spec.type);
        hash<auto> normalized = {
            "name": name,
            "type": type,
            "notnull": (spec.notnull ?? False).toBool(),
            "primary_key": (spec.primary_key ?? False).toBool(),
            "missing_live": (spec.missing_live ?? False).toBool(),
        };
        int size = int(spec.size ?? 0);
        if (exists spec.size && (size < 1 || size > 1000000)) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
                "canonical column %y has an invalid size", name);
        }
        if (size) {
            normalized.size = size;
        }
        int scale = int(spec.scale ?? 0);
        if (scale < 0 || scale > 1000) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
                "canonical column %y has an invalid scale", name);
        }
        if (scale) {
            normalized.scale = scale;
        }
        if (exists spec.default_value) {
            normalized.default_value = bounded_scalar(spec.default_value,
                sprintf("canonical column %y default", name));
        }
        columns{name} = normalized;
        canonical_columns += normalized;
    }

    hash<string, bool> mapped = {};
    list<auto> mappings = ();
    bool empty_table_initialization = False;
    foreach auto raw_mapping in (raw_mappings) {
        hash<auto> mapping = as_hash(raw_mapping);
        validate_keys(mapping, ("target", "source", "source_kind",
            "conversion"), "column mapping");
        require_type(mapping, "target", NT_STRING, "column mapping");
        require_type(mapping, "source_kind", NT_STRING, "column mapping");
        require_type(mapping, "source", NT_STRING, "column mapping",
            True);
        require_type(mapping, "conversion", NT_STRING, "column mapping",
            True);
        string target = require_identifier(mapping.target,
            "mapping target");
        if (!columns{target} || mapped{target}) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
                "mapping target %y is unknown or duplicated", target);
        }
        mapped{target} = True;
        string source_kind = (mapping.source_kind ?? "").toString();
        if (!inlist(source_kind, ("existing_column", "legacy_column",
                "constant_null", "schema_default",
                "empty_table_initialization"))) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
                "mapping for %y has unsupported source_kind %y", target,
                source_kind);
        }
        hash<auto> normalized = {
            "target": target,
            "source_kind": source_kind,
            "conversion": normalize_conversion(mapping.conversion),
        };
        if (inlist(source_kind, ("existing_column", "legacy_column"))) {
            normalized.source = require_identifier(mapping.source,
                sprintf("mapping source for %s", target));
        } else if (source_kind == "constant_null"
                && columns{target}.notnull.toBool()) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
                "mandatory column %y cannot use constant_null", target);
        } else if (source_kind == "schema_default") {
            if (!exists columns{target}.default_value) {
                throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
                    "mapping for %y requests a schema default that is not "
                    "present in canonical_columns", target);
            }
        } else if (source_kind == "empty_table_initialization") {
            if (!columns{target}.missing_live.toBool()) {
                throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
                    "existing canonical column %y cannot use empty-table "
                    "initialization", target);
            }
            empty_table_initialization = True;
        }
        mappings += normalized;
    }

    hash<auto> constraints = as_hash(raw.constraints_and_indexes);
    validate_keys(constraints, ("primary_key", "indexes"),
        "constraints_and_indexes");
    require_type(constraints, "primary_key", NT_HASH,
        "constraints_and_indexes");
    require_type(constraints, "indexes", NT_LIST,
        "constraints_and_indexes", True);
    hash<auto> primary_key = as_hash(constraints.primary_key);
    validate_keys(primary_key, ("name", "columns"), "primary key");
    require_type(primary_key, "name", NT_STRING, "primary key", True);
    require_type(primary_key, "columns", NT_LIST, "primary key");
    list<string> pk_columns = identifiers(primary_key.columns,
        "primary key columns");
    if (!pk_columns) {
        foreach hash<auto> spec in (canonical_columns) {
            if (spec.primary_key.toBool()) {
                pk_columns += spec.name;
            }
        }
    }
    if (pk_columns.size() != 1 || !columns{pk_columns[0]}) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR",
            "the reviewed additive migration requires one canonical "
                "primary-key column";
    }
    if (empty_table_initialization) {
        *hash<auto> key_mapping;
        foreach hash<auto> mapping in (mappings) {
            if (mapping.target == pk_columns[0]) {
                key_mapping = mapping;
                break;
            }
        }
        if (!key_mapping
                || key_mapping.source_kind != "empty_table_initialization") {
            throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
                "empty-table initialization requires canonical key %y to "
                "use the same evidence-bound mapping", pk_columns[0]);
        }
    }
    string pk_name = primary_key.name.val()
        ? require_identifier(primary_key.name, "primary key name")
        : "pk_" + table;

    list<auto> indexes = ();
    list<auto> raw_indexes = as_list(constraints.indexes);
    if (raw_indexes.size() > MaxIndexes) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
            "the migration exceeds the %d-index limit", MaxIndexes);
    }
    hash<string, bool> index_names = {};
    foreach auto raw_index in (raw_indexes) {
        hash<auto> index = as_hash(raw_index);
        validate_keys(index, ("name", "columns", "unique"), "index");
        require_type(index, "name", NT_STRING, "index");
        require_type(index, "columns", NT_LIST, "index");
        require_type(index, "unique", NT_BOOLEAN, "index", True);
        string name = require_identifier(index.name, "index name");
        if (index_names{name}) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
                "duplicate index %y", name);
        }
        index_names{name} = True;
        list<string> index_columns = identifiers(index.columns,
            sprintf("columns for index %s", name));
        if (!index_columns) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
                "index %y has no columns", name);
        }
        foreach string column in (index_columns) {
            if (!columns{column}) {
                throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
                    "index %y references unknown canonical column %y",
                    name, column);
            }
        }
        indexes += {
            "name": name,
            "columns": index_columns,
            "unique": (index.unique ?? False).toBool(),
        };
    }

    hash<auto> backfill = as_hash(raw.backfill);
    validate_keys(backfill, ("batch_size", "quarantine_table",
        "preserve_legacy_columns"), "backfill");
    require_type(backfill, "batch_size", NT_INT, "backfill");
    require_type(backfill, "quarantine_table", NT_STRING, "backfill");
    require_type(backfill, "preserve_legacy_columns", NT_BOOLEAN,
        "backfill", True);
    if (exists backfill.preserve_legacy_columns
            && !backfill.preserve_legacy_columns.toBool()) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR",
            "backfill.preserve_legacy_columns must be true";
    }
    string quarantine_table = require_identifier(backfill.quarantine_table,
        "backfill.quarantine_table");
    if (quarantine_table == table) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR",
            "the quarantine table must differ from the source table";
    }

    return {
        "contract_version": ContractVersion,
        "migration_id": migration_id,
        "table": table,
        "canonical_columns": canonical_columns,
        "column_mappings": mappings,
        "backfill": {
            "batch_size": batch_size,
            "quarantine_table": quarantine_table,
            "preserve_legacy_columns": True,
        },
        "constraints_and_indexes": {
            "primary_key": {
                "name": pk_name,
                "columns": pk_columns,
            },
            "indexes": indexes,
        },
    };
}

#! Inspects the exact migration target without changing it.
/** This read-only check validates the closed contract against current table,
    column, source-mapping, key, and index metadata.  The executing path calls
    the same check again after durable approval reservation and performs its
    full-table conversion scan immediately before the first schema or data
    change.

    @param ds target datasource
    @param raw reviewed contract
    @return bounded current-target projection and normalized contract

    @throw DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR when the reviewed contract
        is malformed or outside the closed additive grammar
    @throw DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR when current schema
        metadata no longer supports the reviewed change

    @par Example
    A contract mapping canonical key @c instruction_id from legacy key @c id
    returns the current canonical and mapped-source column names, but rejects
    the target if @c id is no longer uniquely indexed.
*/
public hash<auto> sub inspect_target(Qore::SQL::AbstractDatasource ds,
        hash<auto> raw) {
    hash<auto> prepared = prepare_target(ds, raw);
    return {
        "contract_version": ContractVersion,
        "status": "checked",
        "contract": prepared.contract,
        "table": prepared.contract.table,
        "active_rows": prepared.active_rows,
        "empty_table_initialization":
            prepared.empty_table_initialization,
        "canonical_key": prepared.canonical_key,
        "cursor": prepared.cursor,
        "current_canonical_columns": prepared.current_canonical_columns,
        "missing_canonical_columns": prepared.missing_canonical_columns,
        "mapped_source_columns": prepared.mapped_source_columns,
    };
}

#! Inspects whether the live schema matches the reviewed migration outcome.
/** This read-only check uses the same closed contract and target validation as
    @ref inspect_target(), then verifies the canonical columns, managed
    nullability, canonical-key uniqueness, and reviewed indexes expected after
    a successful migration.  It does not inspect or return row values and
    never changes the datasource.

    @param ds target datasource
    @param raw reviewed contract
    @return bounded, language-independent current schema alignment

    @throw DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR when the reviewed contract
        is malformed or outside the closed additive grammar
    @throw DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR when current metadata
        cannot be safely compared with the reviewed contract

    @par Example
    A table still missing every contract-managed canonical column returns
    @c not_aligned; a table with only some of them returns
    @c partially_aligned; a table with every reviewed schema element returns
    @c aligned.
*/
public hash<auto> sub inspect_alignment(Qore::SQL::AbstractDatasource ds,
        hash<auto> raw) {
    hash<auto> prepared = prepare_target(ds, raw);
    hash<auto> contract = prepared.contract;
    hash<auto> live = prepared.live_description;
    hash<auto> columns = prepared.live_columns;
    hash<auto> indexes = as_hash(live.indexes);
    list<string> managed = ();
    list<string> managed_present = ();
    list<string> missing = prepared.missing_canonical_columns;
    bool columns_match = !missing;
    foreach hash<auto> spec in (contract.canonical_columns) {
        if (!(spec.missing_live ?? False).toBool()) {
            continue;
        }
        managed += spec.name;
        hash<auto> current = as_hash(columns{spec.name});
        if (current) {
            managed_present += spec.name;
        }
        if (!current || !column_type_compatible(current, spec)
                || (spec.notnull ?? False).toBool()
                    && !(current.notnull ?? False).toBool()) {
            columns_match = False;
        }
    }

    string canonical_key =
        contract.constraints_and_indexes.primary_key.columns[0];
    list<string> primary_key = identifiers(
        as_hash(live.primary_key).columns, "current primary key");
    bool canonical_key_unique = primary_key == (canonical_key,);
    if (!canonical_key_unique) {
        string unique_name = bounded_identifier(
            contract.constraints_and_indexes.primary_key.name
                + "_schema_unique");
        canonical_key_unique = indexes{unique_name}
            && index_matches(indexes{unique_name}, (canonical_key,), True);
    }

    bool reviewed_indexes_present = True;
    foreach hash<auto> index in (
            contract.constraints_and_indexes.indexes) {
        if (!indexes{index.name}
                || !index_matches(indexes{index.name}, index.columns,
                    index.unique.toBool())) {
            reviewed_indexes_present = False;
            break;
        }
    }

    list<auto> verification = (
        alignment_check("canonical-columns-current", columns_match,
            columns_match ? "canonical_columns_match_reviewed_outcome"
                : "canonical_columns_missing_or_incomplete"),
        alignment_check("canonical-key-unique", canonical_key_unique,
            canonical_key_unique ? "canonical_key_is_unique"
                : "canonical_key_uniqueness_missing"),
        alignment_check("reviewed-indexes-current",
            reviewed_indexes_present,
            reviewed_indexes_present ? "reviewed_indexes_present"
                : "reviewed_indexes_missing")
    );
    bool aligned = columns_match && canonical_key_unique
        && reviewed_indexes_present;
    string status = aligned ? "aligned"
        : managed_present ? "partially_aligned" : "not_aligned";
    return {
        "contract_version": ContractVersion,
        "status": status,
        "table": contract.table,
        "migration_id": contract.migration_id,
        "required_canonical_columns":
            (map $1.name, contract.canonical_columns.iterator()),
        "present_canonical_columns":
            prepared.current_canonical_columns,
        "missing_canonical_columns": missing,
        "managed_canonical_columns": managed,
        "verification": verification,
    };
}

#! Executes a reviewed additive migration through SqlUtil.
/** The migration is idempotent and bounded in memory.  Existing columns are
    never dropped or narrowed.  Invalid rows are copied to the unique
    quarantine table before they are removed from active processing.  DML is
    committed in restartable batches; every DDL action is independently
    idempotent because SqlUtil commits DDL per operation on supported drivers.

    @param ds target datasource
    @param raw reviewed contract
    @param verbose nonzero to print progress without row values
    @return number of material changes

    @throw DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR when the reviewed contract
        is malformed or outside the closed additive grammar
    @throw DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR when live schema or
        full-table evidence no longer supports the reviewed change
    @throw DATA-SCHEMA-TABLE-MIGRATION-VALIDATION-ERROR when post-change row
        or schema reconciliation fails

    @par Example
    After explicit approval, an execution adds @c instruction_id, backfills
    it from unique legacy key @c id, retains @c id, quarantines unconvertible
    rows, and returns the exact number of material schema and row changes.
*/
public int sub execute(Qore::SQL::AbstractDatasource ds, hash<auto> raw,
        int verbose = 0) {
    return execute_with_report(ds, raw, verbose).change_count;
}

#! Executes a reviewed additive migration and returns bounded verification.
/** This function has the same execution and exception contract as
    @ref execute(), but returns aggregate row accounting and exact schema
    verification without exposing any row value.

    @param ds target datasource
    @param raw reviewed contract
    @param verbose nonzero to print progress without row values
    @return language-independent aggregate execution report

    @throw DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR when the reviewed contract
        is malformed or outside the closed additive grammar
    @throw DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR when the live target
        no longer matches the reviewed evidence before any change
    @throw DATA-SCHEMA-TABLE-MIGRATION-VALIDATION-ERROR when execution began
        but the committed outcome cannot be verified deterministically

    @par Example
    For two original rows where one legacy key cannot be converted, the report
    records one active row, one quarantined row, and passed schema, row, and
    index checks without returning either row value.
*/
public hash<auto> sub execute_with_report(
        Qore::SQL::AbstractDatasource ds, hash<auto> raw,
        int verbose = 0) {
    hash<auto> prepared = prepare_target(ds, raw);
    hash<auto> contract = prepared.contract;
    hash<string, hash<auto>> specs = map {$1.name: $1},
        contract.canonical_columns.iterator();
    hash<string, hash<auto>> mappings = map {$1.target: $1},
        contract.column_mappings.iterator();

    SqlUtil::AbstractTable table = prepared.table_object;
    hash<auto> live = prepared.live_description;
    hash<auto> live_columns = prepared.live_columns;
    hash<auto> column_definitions = prepared.column_definitions;
    string canonical_key = prepared.canonical_key;
    bool empty_table_initialization =
        prepared.empty_table_initialization;
    string cursor = prepared.cursor;
    int active_count_before = prepared.active_rows;
    int quarantine_count_before = 0;
    int original_count = active_count_before;
    hash<auto> profile = {
        "rows": 0,
        "invalid": 0,
    };
    *SqlUtil::AbstractTable quarantine;
    if (empty_table_initialization) {
        # prepare_target() already checked the evidence-bound empty-table
        # invariant without making a schema or data change.
    } else {
        list<string> selected_columns = conversion_columns(live_columns,
            contract, cursor);
        hash<auto> quarantine_state = inspect_quarantine_table(ds, contract,
            cursor);
        quarantine_count_before = quarantine_state.row_count;
        original_count += quarantine_count_before;
        profile = scan(table, selected_columns, cursor, contract,
            specs, False, NOTHING, verbose);
        if (profile.rows != active_count_before) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR", sprintf(
                "full scan counted %d rows but the table reported %d",
                profile.rows, active_count_before);
        }
        # The full conversion scan above is deliberately before the first
        # material change, including quarantine-table creation or indexing.
        quarantine = ensure_quarantine_table(ds, contract, live, cursor,
            verbose);
    }

    # missing_live is part of the signed, server-authored contract.  Keep
    # these columns managed across retries so an interruption after nullable
    # creation but before constraint tightening remains resumable.  Columns
    # that were already canonical when evidence was collected are never
    # modified by this migration.
    hash<string, bool> managed = {};
    int changes = 0;
    foreach hash<auto> spec in (contract.canonical_columns) {
        if (spec.missing_live.toBool()) {
            managed{spec.name} = True;
        }
        if (live_columns{spec.name}) {
            continue;
        }
        table.addColumn(spec.name, column_definitions{spec.name}, True);
        ++changes;
        if (verbose) {
            printf("added nullable canonical column %s.%s\n",
                contract.table, spec.name);
        }
    }

    hash<auto> applied = empty_table_initialization
        ? {"updated": 0, "quarantined": 0}
        : scan(table, conversion_columns(live_columns, contract, cursor),
            cursor, contract,
            specs, True, quarantine, verbose);
    changes += applied.updated + applied.quarantined;
    int active_count = table.rowCount();
    int quarantine_count = empty_table_initialization
        ? 0 : quarantine.rowCount();
    if (active_count + quarantine_count != original_count
            || quarantine_count
                != quarantine_count_before + profile.invalid
            || applied.quarantined != profile.invalid) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-VALIDATION-ERROR", sprintf(
            "row reconciliation failed: original=%d active=%d "
            "quarantined=%d expected_quarantined=%d",
            original_count, active_count, quarantine_count,
            profile.invalid);
    }

    table = (new SqlUtil::Table(ds, contract.table)).getTable();
    hash<auto> current = table.getDescriptionHash();
    hash<auto> current_indexes = as_hash(current.indexes);

    # Preserve a different legacy primary key.  A unique canonical-key index
    # supplies the required uniqueness without a committed drop/add window.
    list<string> current_pk = identifiers(as_hash(current.primary_key).columns,
        "current primary key");
    if (current_pk != (canonical_key,)) {
        string unique_name = bounded_identifier(
            contract.constraints_and_indexes.primary_key.name
                + "_schema_unique");
        if (current_indexes{unique_name}
                && !index_matches(current_indexes{unique_name},
                    (canonical_key,), True)) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-VALIDATION-ERROR", sprintf(
                "existing canonical-key index %y does not match the "
                    "reviewed unique definition", unique_name);
        }
        if (!current_indexes{unique_name}) {
            table.addIndex(unique_name, True, (canonical_key,));
            ++changes;
            current_indexes{unique_name} = {
                "columns": (canonical_key,),
                "unique": True,
            };
        }
    }
    foreach hash<auto> index in (
            contract.constraints_and_indexes.indexes) {
        if (current_indexes{index.name}
                && !index_matches(current_indexes{index.name},
                    index.columns, index.unique.toBool())) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-VALIDATION-ERROR", sprintf(
                "existing index %y does not match the reviewed definition",
                index.name);
        }
        if (current_indexes{index.name}) {
            continue;
        }
        table.addIndex(index.name, index.unique.toBool(), index.columns);
        ++changes;
        current_indexes{index.name} = index;
    }

    # Only tighten columns identified as absent by this reviewed migration.
    # Use the refreshed live definition so this operation changes nullability
    # only; it never narrows a type or size that appeared during a retry.
    foreach string name in (sort(keys managed)) {
        hash<auto> spec = specs{name};
        if (spec.notnull.toBool()) {
            hash<auto> live_spec = as_hash(current.columns{name});
            if (!(live_spec.notnull ?? False).toBool()) {
                table.modifyColumn(name, preserved_column_definition(live_spec),
                    False);
                ++changes;
            }
        }
    }

    list<auto> verification = verify_result(ds, contract, specs, cursor,
        empty_table_initialization, original_count, quarantine_count);
    if (verbose) {
        printf("migration %s complete: original=%d active=%d "
            "quarantined=%d updated=%d newly_quarantined=%d changes=%d\n",
            contract.migration_id, original_count, active_count,
            quarantine_count, applied.updated, applied.quarantined,
            changes);
    }
    return {
        "contract_version": ContractVersion,
        "status": "succeeded",
        "migration_id": contract.migration_id,
        "table": contract.table,
        "change_count": changes,
        "original_rows": original_count,
        "active_rows_before": active_count_before,
        "active_rows_after": active_count,
        "quarantine_rows_before": quarantine_count_before,
        "quarantine_rows_after": quarantine_count,
        "updated_rows": applied.updated,
        "newly_quarantined_rows": applied.quarantined,
        "empty_table_initialization": empty_table_initialization,
        "verification": verification,
    };
}

hash<auto> sub prepare_target(Qore::SQL::AbstractDatasource ds,
        hash<auto> raw) {
    hash<auto> contract = validate_contract(raw);
    SqlUtil::AbstractTable table =
        (new SqlUtil::Table(ds, contract.table)).getTable();
    hash<auto> live = table.getDescriptionHash();
    hash<auto> live_columns = as_hash(live.columns);
    if (!live_columns) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR", sprintf(
            "target table %y has no readable columns", contract.table);
    }
    if (live_columns.size() > MaxColumns) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR", sprintf(
            "target table %y exceeds the %d-column safety limit",
            contract.table, MaxColumns);
    }

    list<string> mapped_sources = ();
    foreach hash<auto> mapping in (contract.column_mappings) {
        if (mapping.source.val() && !live_columns{mapping.source}) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR", sprintf(
                "verified source column %y for target %y is no longer "
                "present", mapping.source, mapping.target);
        }
        if (mapping.source.val()
                && !inlist(mapping.source, mapped_sources)) {
            mapped_sources += mapping.source;
        }
    }

    list<string> present = ();
    list<string> missing = ();
    hash<auto!> column_definitions();
    foreach hash<auto> spec in (contract.canonical_columns) {
        # Normalize every reviewed logical type through DataSchema before the
        # first possible mutation.  This is intentionally done for columns
        # that are already live as well: an interrupted retry may have only a
        # prefix of the reviewed columns, and discovering an invalid later
        # definition after adding an earlier column would leave a partially
        # applied migration.
        column_definitions{spec.name} =
            column_definition(contract.table, spec);
        if (live_columns{spec.name}) {
            if (!column_type_compatible(live_columns{spec.name}, spec)) {
                throw "DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR", sprintf(
                    "canonical column %y has live type %y, which is not "
                        "compatible with reviewed type %y",
                    spec.name, live_columns{spec.name}.qore_type
                        ?? live_columns{spec.name}.native_type,
                    spec.type);
            }
            present += spec.name;
        } else {
            if (!spec.missing_live.toBool()) {
                throw "DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR", sprintf(
                    "canonical column %y was present in the reviewed "
                        "evidence but is now missing", spec.name);
            }
            missing += spec.name;
        }
    }

    string canonical_key = contract.constraints_and_indexes.primary_key
        .columns[0];
    hash<string, hash<auto>> mappings = map {$1.target: $1},
        contract.column_mappings.iterator();
    hash<auto> key_mapping = mappings{canonical_key};
    bool empty_table_initialization =
        key_mapping.source_kind == "empty_table_initialization";
    string cursor = (key_mapping.source ?? "").toString();
    int active_rows = table.rowCount();
    if (empty_table_initialization) {
        if (active_rows) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR", sprintf(
                "migration %y was approved for an empty table, but %s now "
                "contains %d row%s; collect a new evidence-bound migration "
                "profile before making any change",
                contract.migration_id, contract.table, active_rows,
                active_rows == 1 ? "" : "s");
        }
    } else {
        if (!cursor.val()) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR", sprintf(
                "canonical key %y has no live source column", canonical_key);
        }
        list<string> live_pk = identifiers(as_hash(live.primary_key).columns,
            "live primary key");
        bool cursor_unique = live_pk == (cursor,);
        if (!cursor_unique) {
            foreach hash<auto> index in (as_hash(live.indexes).iterator()) {
                if ((index.unique ?? False).toBool()
                        && identifiers(index.columns, "live unique index")
                            == (cursor,)) {
                    cursor_unique = True;
                    break;
                }
            }
        }
        if (!cursor_unique) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR", sprintf(
                "safe bounded backfill requires mapping canonical key %y "
                "from a single-column live primary key or unique index; "
                "live key=%y, mapped source=%y", canonical_key, live_pk,
                cursor);
        }
    }
    validate_existing_indexes(as_hash(live.indexes), contract, canonical_key);
    return {
        "contract": contract,
        "table_object": table,
        "live_description": live,
        "live_columns": live_columns,
        "column_definitions": column_definitions,
        "active_rows": active_rows,
        "canonical_key": canonical_key,
        "cursor": cursor,
        "empty_table_initialization": empty_table_initialization,
        "current_canonical_columns": present,
        "missing_canonical_columns": missing,
        "mapped_source_columns": mapped_sources,
    };
}

hash<auto> sub scan(SqlUtil::AbstractTable table,
        list<string> selected_columns, string cursor, hash<auto> contract,
        hash<string, hash<auto>> specs, bool apply,
        *SqlUtil::AbstractTable quarantine, int verbose) {
    int rows = 0;
    int invalid = 0;
    int updated = 0;
    int quarantined = 0;
    list<string> quarantine_columns = ();
    SqlUtil::AbstractTable apply_quarantine;
    if (apply) {
        if (!exists quarantine) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-VALIDATION-ERROR",
                "an apply scan requires a quarantine table";
        }
        apply_quarantine = quarantine;
        quarantine_columns = sort(keys as_hash(
            apply_quarantine.getDescriptionHash().columns));
    }
    auto last;
    while (True) {
        hash<auto> select_options = {
            "columns": selected_columns,
            "limit": contract.backfill.batch_size,
            "orderby": cursor,
        };
        if (exists last) {
            select_options.where = {cursor: SqlUtil::op_gt(last)};
        }
        list<auto> page = table.selectRows(select_options) ?? ();
        if (!page) {
            break;
        }
        if (apply) {
            apply_page(table, apply_quarantine, page, cursor, contract, specs,
                quarantine_columns, \updated, \quarantined);
        } else {
            foreach hash<auto> row in (page) {
                hash<auto> conversion = convert_row(row, contract, specs);
                if (!conversion.valid.toBool()) {
                    ++invalid;
                }
            }
        }
        rows += page.size();
        last = as_hash(page[-1]){cursor};
        if (!is_present(last)) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR", sprintf(
                "live primary-key cursor %y contains NULL", cursor);
        }
        if (page.size() < contract.backfill.batch_size) {
            break;
        }
    }
    if (verbose > 1) {
        printf("%s scan complete: rows=%d invalid=%d\n",
            apply ? "migration" : "preflight", rows,
            apply ? quarantined : invalid);
    }
    return {
        "rows": rows,
        "invalid": apply ? quarantined : invalid,
        "updated": updated,
        "quarantined": quarantined,
    };
}

list<string> sub conversion_columns(hash<auto> live_columns,
        hash<auto> contract, string cursor) {
    hash<string, bool> selected = {cursor: True};
    foreach hash<auto> mapping in (contract.column_mappings) {
        if (mapping.source.val()) {
            selected{mapping.source} = True;
        }
        if (live_columns{mapping.target}) {
            selected{mapping.target} = True;
        }
    }
    return sort(keys selected);
}

hash<auto> sub alignment_check(string verification_id, bool passed,
        string reason_code) {
    return {
        "verification_id": verification_id,
        "status": passed ? "passed" : "failed",
        "reason_code": reason_code,
        "deterministic": True,
    };
}

sub apply_page(SqlUtil::AbstractTable table,
        SqlUtil::AbstractTable quarantine, list<auto> page, string cursor,
        hash<auto> contract, hash<string, hash<auto>> specs,
        list<string> quarantine_columns,
        reference<int> updated, reference<int> quarantined) {
    SqlUtil::RestartableTransaction trans(table.getDriverName());
    while (True) {
        try {
            # Publish attempt-local counts only after apply_page_attempt() has
            # returned through its on_success commit handler.  A failed
            # commit therefore cannot leak rolled-back counts into the
            # reconciliation totals used by execute().
            hash<auto> result = apply_page_attempt(table, quarantine, page,
                cursor, contract, specs, quarantine_columns);
            updated += result.updated;
            quarantined += result.quarantined;
            trans.reset();
            return;
        } catch (hash<ExceptionInfo> ex) {
            if (trans.restartTransaction(ex)) {
                continue;
            }
            rethrow;
        }
    }
}

hash<auto> sub apply_page_attempt(SqlUtil::AbstractTable table,
        SqlUtil::AbstractTable quarantine, list<auto> page, string cursor,
        hash<auto> contract, hash<string, hash<auto>> specs,
        list<string> quarantine_columns) {
    int updated = 0;
    int quarantined = 0;
    on_error table.rollback();
    on_success table.commit();
    table.beginTransaction();
    foreach hash<auto> row in (page) {
        hash<auto> converted = convert_row(row, contract, specs);
        auto key_value = row{cursor};
        if (!converted.valid.toBool()) {
            hash<auto> where_cond = {
                cursor: key_value,
                "schema_migration_id": contract.migration_id,
            };
            if (!(quarantine.selectRows({
                    "columns": (cursor,),
                    "where": where_cond,
                    "limit": 1,
                }) ?? ())) {
                list<auto> full_rows = table.selectRows({
                    "where": {cursor: key_value},
                    "limit": 1,
                }) ?? ();
                if (full_rows.size() != 1) {
                    throw "DATA-SCHEMA-TABLE-MIGRATION-VALIDATION-ERROR",
                        sprintf("cannot load the unique source row for "
                            "quarantine cursor %y", key_value);
                }
                hash<auto> full_row = as_hash(full_rows[0]);
                hash<auto> copy = {};
                foreach string name in (quarantine_columns) {
                    if (exists full_row{name}) {
                        copy{name} = full_row{name};
                    }
                }
                copy.schema_migration_id = contract.migration_id;
                copy.schema_migration_reason = sprintf(
                    "canonical conversion failed for: %s",
                    as_list(converted.invalid_targets).join(", "));
                copy.schema_migration_quarantined_at = now_us();
                quarantine.insert(copy);
            }
            table.del({cursor: key_value});
            ++quarantined;
        } else {
            hash<auto> changes = {};
            foreach hash<auto> pair in (
                    as_hash(converted.values).pairIterator()) {
                if (row{pair.key} != pair.value) {
                    changes{pair.key} = pair.value;
                }
            }
            if (changes) {
                table.update(changes, {cursor: key_value});
                ++updated;
            }
        }
    }
    return {
        "updated": updated,
        "quarantined": quarantined,
    };
}

hash<auto> sub convert_row(hash<auto> row, hash<auto> contract,
        hash<string, hash<auto>> specs) {
    hash<auto> values = {};
    list<string> invalid_targets = ();
    foreach hash<auto> mapping in (contract.column_mappings) {
        hash<auto> spec = specs{mapping.target};
        auto value;
        if (mapping.source_kind == "constant_null"
                || mapping.source_kind == "empty_table_initialization") {
            value = NULL;
        } else if (mapping.source_kind == "schema_default") {
            value = spec.default_value;
        } else {
            value = row{mapping.source};
        }
        bool valid = True;
        auto converted = convert_value(value, spec, \valid);
        if (!valid) {
            invalid_targets += mapping.target;
        } else {
            values{mapping.target} = converted;
        }
    }
    return {
        "valid": !invalid_targets,
        "invalid_targets": invalid_targets,
        "values": values,
    };
}

auto sub convert_value(auto value, hash<auto> spec,
        reference<bool> valid) {
    if (!is_present(value)) {
        valid = !spec.notnull.toBool();
        return NULL;
    }
    try {
        switch (spec.type) {
            case "date": {
                *date converted = cast<*date>(value);
                valid = exists converted;
                return valid ? converted : NULL;
            }
            case "number": {
                *number converted = cast<*number>(value);
                valid = exists converted;
                return valid ? converted : NULL;
            }
            case "int": {
                *int converted = cast<*int>(value);
                valid = exists converted;
                return valid ? converted : NULL;
            }
            case "float": {
                *float converted = cast<*float>(value);
                valid = exists converted;
                return valid ? converted : NULL;
            }
            case "bool": {
                *bool converted = cast<*bool>(value);
                valid = exists converted;
                return valid ? converted : NULL;
            }
            case "string": {
                string converted = value.toString();
                valid = !spec.size || converted.size() <= int(spec.size);
                return valid ? converted : NULL;
            }
        }
        valid = True;
        return value;
    } catch (hash<ExceptionInfo> ex) {
        valid = False;
        return NULL;
    }
}

hash<auto> sub inspect_quarantine_table(Qore::SQL::AbstractDatasource ds,
        hash<auto> contract, string cursor) {
    string name = contract.backfill.quarantine_table;
    SqlUtil::AbstractTable table = (new SqlUtil::Table(ds, name)).getTable();
    if (!table.checkExistence()) {
        return {"exists": False, "row_count": 0};
    }
    hash<auto> description = table.getDescriptionHash();
    hash<auto> columns = as_hash(description.columns);
    foreach string required in (("schema_migration_id",
            "schema_migration_reason", "schema_migration_quarantined_at", cursor)) {
        if (!columns{required}) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR", sprintf(
                "existing quarantine table %y is missing %y", name,
                required);
        }
    }
    string index_name = bounded_identifier(
        "uk_" + name + "_migration_cursor");
    hash<auto> indexes = as_hash(description.indexes);
    list<string> index_columns = ("schema_migration_id", cursor);
    if (indexes{index_name}
            && !index_matches(indexes{index_name}, index_columns, True)) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR", sprintf(
            "existing quarantine index %y does not match the reviewed "
                "unique definition", index_name);
    }
    int row_count = table.rowCount();
    if (row_count && !indexes{index_name}) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR", sprintf(
            "existing non-empty quarantine table %y has no reviewed unique "
                "migration-cursor index", name);
    }
    return {
        "exists": True,
        "row_count": row_count,
    };
}

list<auto> sub verify_result(Qore::SQL::AbstractDatasource ds,
        hash<auto> contract, hash<string, hash<auto>> specs, string cursor,
        bool empty_table_initialization, int original_count,
        int expected_quarantine_count) {
    SqlUtil::AbstractTable table =
        (new SqlUtil::Table(ds, contract.table)).getTable();
    hash<auto> description = table.getDescriptionHash();
    hash<auto> columns = as_hash(description.columns);
    foreach string name in (sort(keys specs)) {
        hash<auto> spec = specs{name};
        hash<auto> live = as_hash(columns{name});
        if (!live || !column_type_compatible(live, spec)
                || (spec.missing_live.toBool() && spec.notnull.toBool()
                    && !(live.notnull ?? False).toBool())) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-VALIDATION-ERROR", sprintf(
                "canonical column %y does not match the reviewed type or "
                    "managed nullability after migration", name);
        }
    }

    string canonical_key = contract.constraints_and_indexes.primary_key
        .columns[0];
    list<string> primary_key = identifiers(
        as_hash(description.primary_key).columns, "final primary key");
    hash<auto> indexes = as_hash(description.indexes);
    if (primary_key != (canonical_key,)) {
        string unique_name = bounded_identifier(
            contract.constraints_and_indexes.primary_key.name
                + "_schema_unique");
        if (!indexes{unique_name}
                || !index_matches(indexes{unique_name}, (canonical_key,),
                    True)) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-VALIDATION-ERROR", sprintf(
                "canonical key %y has no reviewed unique constraint after "
                    "migration", canonical_key);
        }
    }
    foreach hash<auto> index in (
            contract.constraints_and_indexes.indexes) {
        if (!indexes{index.name}
                || !index_matches(indexes{index.name}, index.columns,
                    index.unique.toBool())) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-VALIDATION-ERROR", sprintf(
                "reviewed index %y is missing or changed after migration",
                index.name);
        }
    }

    int active_count = table.rowCount();
    int quarantine_count = 0;
    if (!empty_table_initialization) {
        SqlUtil::AbstractTable quarantine = (new SqlUtil::Table(ds,
            contract.backfill.quarantine_table)).getTable();
        if (!quarantine.checkExistence()) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-VALIDATION-ERROR", sprintf(
                "reviewed quarantine table %y is missing after migration",
                contract.backfill.quarantine_table);
        }
        quarantine_count = quarantine.rowCount();
        if (quarantine_count != expected_quarantine_count) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-VALIDATION-ERROR", sprintf(
                "quarantine row count changed during final verification: "
                    "expected=%d actual=%d", expected_quarantine_count,
                quarantine_count);
        }
        hash<auto> profile = scan(table,
            conversion_columns(columns, contract, cursor), cursor, contract,
            specs, False, NOTHING, 0);
        if (profile.invalid || profile.rows != active_count) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-VALIDATION-ERROR", sprintf(
                "active-row validation failed after migration: rows=%d "
                    "scanned=%d invalid=%d", active_count, profile.rows,
                profile.invalid);
        }
    }
    if (active_count + quarantine_count != original_count) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-VALIDATION-ERROR", sprintf(
            "final row reconciliation failed: original=%d active=%d "
                "quarantined=%d", original_count, active_count,
            quarantine_count);
    }
    return (
        {
            "verification_id": "canonical-columns-present",
            "status": "passed",
            "reason_code":
                "canonical_columns_and_managed_constraints_verified",
            "deterministic": True,
        },
        {
            "verification_id": "backfill-reconciled",
            "status": "passed",
            "reason_code": "active_and_quarantine_rows_reconciled",
            "deterministic": True,
        },
        {
            "verification_id": "constraints-indexes-present",
            "status": "passed",
            "reason_code": "constraints_and_indexes_match_reviewed_contract",
            "deterministic": True,
        }
    );
}

SqlUtil::AbstractTable sub ensure_quarantine_table(
        Qore::SQL::AbstractDatasource ds, hash<auto> contract,
        hash<auto> live, string cursor, int verbose) {
    string name = contract.backfill.quarantine_table;
    SqlUtil::AbstractTable table = (new SqlUtil::Table(ds, name)).getTable();
    if (table.checkExistence()) {
        hash<auto> description = table.getDescriptionHash();
        hash<auto> columns = as_hash(description.columns);
        foreach string required in (("schema_migration_id",
                "schema_migration_reason", "schema_migration_quarantined_at", cursor)) {
            if (!columns{required}) {
                throw "DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR",
                    sprintf("existing quarantine table %y is missing %y",
                        name, required);
            }
        }
        ensure_quarantine_index(table, name, cursor,
            as_hash(description.indexes));
        return table;
    }
    hash<auto> description = {
        "columns": as_hash(live.columns) + {
            "schema_migration_id": {
                "qore_type": Type::String,
                "size": 96,
            },
            "schema_migration_reason": {
                "qore_type": Type::String,
                "size": 1000,
            },
            "schema_migration_quarantined_at": {
                "qore_type": Type::Date,
            },
        },
    };
    table = (new SqlUtil::Table(ds, description, name)).getTable();
    table.create();
    ensure_quarantine_index(table, name, cursor, {});
    if (verbose) {
        printf("created quarantine table %s\n", name);
    }
    return table;
}

sub ensure_quarantine_index(SqlUtil::AbstractTable table, string table_name,
        string cursor, hash<auto> indexes) {
    string index_name = bounded_identifier(
        "uk_" + table_name + "_migration_cursor");
    list<string> columns = ("schema_migration_id", cursor);
    if (indexes{index_name}) {
        if (!index_matches(indexes{index_name}, columns, True)) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR", sprintf(
                "existing quarantine index %y does not match the reviewed "
                    "unique definition", index_name);
        }
        return;
    }
    table.addIndex(index_name, True, columns);
}

hash<auto> sub column_definition(string table, hash<auto> spec) {
    # This hash receives heterogeneous assignments below.  Without the
    # assignment-safe declaration, the initial string-valued type field
    # narrows later size/scale assignments to string and rejects their
    # contract-defined integer values.
    hash<auto!> raw = {
        # DataSchemaLoader maps the closed shorthand to the SqlUtil type
        # object/constant required by each driver; passing the shorthand as
        # qore_type would incorrectly ask SqlUtil to resolve values such as
        # "int" instead of the portable "integer" type.
        # The remediation contract uses language-independent logical types,
        # while DataSchema accepts a smaller portable physical vocabulary.
        # Preserve time-of-day for logical dates and use bounded VARCHAR only
        # when the reviewed string has an explicit size; unbounded strings
        # use DataSchema's portable TEXT/CLOB abstraction.
        "type": physical_column_type(spec),
    };
    foreach string field in (("size", "scale", "default_value")) {
        if (exists spec{field}) {
            raw{field} = spec{field};
        }
    }
    return Schema::DataSchemaLoader::normalizeColumn(table, spec.name, raw);
}

string sub physical_column_type(hash<auto> spec) {
    switch (spec.type) {
        case "date":
            return "timestamp";
        case "string":
            return spec.size ? "varchar" : "text";
        case "int":
        case "number":
        case "float":
        case "bool":
        case "binary":
            return spec.type;
    }
    # validate_contract() owns the public closed grammar.  Keep this defensive
    # guard so future grammar additions cannot silently reach DDL without an
    # explicit portable physical representation.
    throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
        "canonical column %y has no portable physical representation for "
            "type %y", spec.name, spec.type);
}

bool sub column_type_compatible(hash<auto> live, hash<auto> reviewed) {
    auto raw_type = live.qore_type ?? live.native_type;
    if (!raw_type.val()) {
        return False;
    }
    try {
        string live_type = normalize_type(raw_type);
        string reviewed_type = reviewed.type;
        # The evidence contract treats an integer source as a lossless
        # representation of the broader canonical number type.  Preserve the
        # same rule at both executor preflight and post-migration verification
        # so an approved plan cannot pass evidence analysis and then fail on
        # the unchanged existing column.
        return live_type == reviewed_type
            || (reviewed_type == "number" && live_type == "int");
    } catch (hash<ExceptionInfo> ex) {
        return False;
    }
}

hash<auto> sub preserved_column_definition(hash<auto> live) {
    hash<auto> rv = {};
    foreach string key in (("native_type", "qore_type", "size", "scale",
            "default_value", "comment")) {
        if (exists live{key}) {
            rv{key} = live{key};
        }
    }
    return rv;
}

bool sub index_matches(hash<auto> live, list<string> columns, bool unique) {
    return identifiers(live.columns, "live index columns") == columns
        && (live.unique ?? False).toBool() == unique;
}

sub validate_existing_indexes(hash<auto> live_indexes, hash<auto> contract,
        string canonical_key) {
    string unique_name = bounded_identifier(
        contract.constraints_and_indexes.primary_key.name
            + "_schema_unique");
    if (live_indexes{unique_name}
            && !index_matches(live_indexes{unique_name}, (canonical_key,),
                True)) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR", sprintf(
            "existing canonical-key index %y does not match the reviewed "
                "unique definition", unique_name);
    }
    foreach hash<auto> index in (
            contract.constraints_and_indexes.indexes) {
        if (live_indexes{index.name}
                && !index_matches(live_indexes{index.name}, index.columns,
                    index.unique.toBool())) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR", sprintf(
                "existing index %y does not match the reviewed definition",
                index.name);
        }
    }
}

string sub normalize_type(auto raw) {
    string type = raw.toString().lwr();
    switch (type) {
        case "timestamp":
        case "date":
            return "date";
        case "integer":
        case "int":
            return "int";
        case "numeric":
        case "decimal":
        case "number":
            return "number";
        case "double":
        case "float":
            return "float";
        case "boolean":
        case "bool":
            return "bool";
        case "varchar":
        case "text":
        case "richtext":
        case "string":
            return "string";
        case "binary":
            return type;
    }
    throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
        "unsupported canonical column type %y", raw);
}

string sub normalize_conversion(auto raw) {
    string conversion = (raw ?? "identity").toString();
    if (!inlist(conversion, ("identity", "to_timestamp",
            "numeric_compatible", "to_number", "string_size_validation",
            "to_string", "validated_cast", "none", "schema_default"))) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
            "unsupported conversion %y", conversion);
    }
    return conversion;
}

string sub require_identifier(auto raw, string field,
        bool allow_dash = False) {
    string value = (raw ?? "").toString();
    string pattern = allow_dash
        ? "^[A-Za-z_][A-Za-z0-9_-]*$"
        : "^[A-Za-z_][A-Za-z0-9_]*$";
    if (!value.val() || !regex(value, pattern)) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
            "%s must be a safe identifier", field);
    }
    return value;
}

string sub bounded_identifier(string value) {
    if (value !~ /^[A-Za-z_][A-Za-z0-9_]*$/) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR",
            "generated index name is not a safe identifier";
    }
    return value.size() > 63
        ? value.substr(0, 54) + "_"
            + digest("sha256", value).toHex().substr(0, 8)
        : value;
}

list<string> sub identifiers(auto raw, string field) {
    list<auto> values = as_list(raw);
    list<string> rv = ();
    foreach auto value in (values) {
        string identifier = require_identifier(value, field);
        if (inlist(identifier, rv)) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
                "%s contains duplicate identifier %y", field, identifier);
        }
        rv += identifier;
    }
    return rv;
}

auto sub bounded_scalar(auto value, string field) {
    if (inlist(value.typeCode(), (NT_HASH, NT_LIST, NT_OBJECT))) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
            "%s must be a scalar value", field);
    }
    if (value.typeCode() == NT_STRING && value.toString().size() > 1000) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
            "%s exceeds the scalar length limit", field);
    }
    return value;
}

bool sub is_present(auto value) {
    switch (value.typeCode()) {
        case NT_NOTHING:
        case NT_NULL:
            return False;
        case NT_STRING:
        case NT_HASH:
        case NT_LIST:
            return value.val() ? True : False;
    }
    return True;
}

hash<auto> sub as_hash(auto value) {
    return value.typeCode() == NT_HASH ? (value) : {};
}

list<auto> sub as_list(auto value) {
    return value.typeCode() == NT_LIST ? (value) : ();
}

sub validate_keys(hash<auto> value, list<string> allowed, string label) {
    foreach string key in (keys value) {
        if (!inlist(key, allowed)) {
            throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
                "%s contains unsupported field %y", label, key);
        }
    }
}

sub require_type(hash<auto> value, string field, int expected_type,
        string label, bool optional = False) {
    if (!exists value{field}) {
        if (optional) {
            return;
        }
        throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
            "%s is missing required field %y", label, field);
    }
    if (value{field}.typeCode() != expected_type) {
        throw "DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR", sprintf(
            "%s field %y has an invalid type", label, field);
    }
}

} # namespace DataSchemaTableMigration
} # namespace Schema
