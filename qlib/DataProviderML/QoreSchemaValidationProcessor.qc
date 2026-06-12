# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreSchemaValidationProcessor class definition

/** QoreSchemaValidationProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProviderML module
public namespace DataProviderML {
#! Pipeline processor that validates each record against a schema and enriches with validation status
/** Validates each record against a user-defined schema specifying type constraints, required fields,
    value ranges, string length limits, and allowed values. Based on the on_violation setting, invalid
    records are either enriched with validation error details, silently dropped, or cause an exception.

*/
public class QoreSchemaValidationProcessor inherits AbstractDataProviderMLProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreSchemaValidationProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "schema": <DataProviderOptionInfo>{
                "display_name": "Validation Rules",
                "short_desc": "Schema defining validation rules for each field",
                "desc": "A hash where each key is a field name and the value is a hash of "
                    "validation rules to enforce on that field:\n\n"
                    "- **`type`** (*string*): expected data type — use `\"int\"` for quantities "
                    "and counts, `\"float\"` for prices and measurements, `\"string\"` for names "
                    "and codes, `\"bool\"` for flags, `\"date\"` for timestamps\n"
                    "- **`required`** (*bool*): use `required: True` for fields that must always "
                    "be present — e.g., `customer_id`, `order_date`\n"
                    "- **`min`** / **`max`** (*float*): numeric bounds — use `min: 0, max: 150` "
                    "for age to catch data entry errors, or `min: 0` for prices to reject "
                    "negative amounts\n"
                    "- **`min_length`** / **`max_length`** (*int*): string length limits — use "
                    "`min_length: 1` to reject empty strings, or `max_length: 255` to enforce "
                    "database column limits\n"
                    "- **`allowed_values`** (*list\\<auto>*): enumeration check — use "
                    "`allowed_values: (\"active\", \"inactive\")` for status codes, or "
                    "`allowed_values: (1, 2, 3, 4, 5)` for rating fields\n\n"
                    "**Example**:\n"
                    "`{\"age\": {\"type\": \"int\", \"required\": True, \"min\": 0, "
                    "\"max\": 150}, \"name\": {\"type\": \"string\", \"required\": True, "
                    "\"min_length\": 1}, \"status\": {\"allowed_values\": "
                    "(\"active\", \"inactive\")}}`",
                "type": AbstractDataProviderTypeMap."hash",
                "required": True,
            },
            "on_violation": <DataProviderOptionInfo>{
                "display_name": "On Violation",
                "short_desc": "What to do when validation fails",
                "desc": "Controls what happens when a record fails validation:\n"
                    "- **enrich**: pass the record through with `_validation_errors` and "
                    "`_is_valid` fields added (default)\n"
                    "- **drop**: silently drop invalid records\n"
                    "- **throw**: throw a VALIDATION-ERROR exception with the list of violations",
                "type": AbstractDataProviderTypeMap."*string",
                "required": False,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "enrich",
                        "display_name": "Enrich Record",
                        "desc": "Pass invalid records through with `_validation_errors` (list of "
                            "error strings) and `_is_valid` (bool) fields added. Valid records "
                            "also get `_is_valid` = True with an empty error list",
                    },
                    <AllowedValueInfo>{
                        "value": "drop",
                        "display_name": "Drop Invalid Records",
                        "desc": "Silently discard records that fail validation; only valid "
                            "records pass through to the next pipeline step",
                    },
                    <AllowedValueInfo>{
                        "value": "throw",
                        "display_name": "Throw Exception",
                        "desc": "Throw a VALIDATION-ERROR exception when a record fails "
                            "validation; stops the pipeline immediately on the first invalid "
                            "record",
                    },
                ),
            },
        };
    }

    private {
        #! Configured validation schema
        hash<auto> schema;

        #! On violation behavior
        string on_violation = "enrich";

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        schema = copts.schema;
        if (!schema) {
            throw "CONSTRUCTOR-ERROR", "schema must be a non-empty hash";
        }
        if (exists copts.on_violation) {
            on_violation = copts.on_violation;
        }
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "schema-validation";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Validates records against a defined schema with type checks, required fields, "
            "value ranges, string length limits, and allowed value constraints; invalid records "
            "can be enriched with error details, dropped, or cause an exception";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Validates a single record against the schema
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreSchemaValidationProcessor: not configured; provide constructor options";
        }

        list<string> violations = validateRecord(record);
        return finishRecord(record, violations);
    }

    #! Validates a single row record
    private list<string> validateRecord(auto record) {
        list<string> violations = ();

        foreach string field_name in (keys schema) {
            hash<auto> rules = schema{field_name};
            auto val = DataProvider::AbstractDataProvider::getFieldValue(record, field_name);
            appendFieldViolations(\violations, field_name, rules, val);
        }

        return violations;
    }

    #! Applies the configured violation policy to one row
    private *auto finishRecord(auto record, list<string> violations) {
        switch (on_violation) {
            case "enrich": {
                hash<auto> result = {} + record;
                result._validation_errors = violations;
                result._is_valid = (violations.lsize() == 0);
                return result;
            }
            case "drop": {
                if (violations) {
                    return;
                }
                return record;
            }
            case "throw": {
                if (violations) {
                    throw "VALIDATION-ERROR",
                        sprintf("record failed validation: %s", violations.join("; ")),
                        violations;
                }
                return record;
            }
        }

        return record;
    }

    #! Adds validation errors for one field value
    private appendFieldViolations(reference<list<string>> violations, string field_name,
            hash<auto> rules, *auto val) {
        # check required
        bool is_required = rules.required.toBool();
        if (!exists val) {
            if (is_required) {
                push violations,
                    sprintf("field '%s': required but missing or null", field_name);
            }
            # skip other checks for missing values
            return;
        }

        # check type
        if (exists rules.type) {
            string expected_type = rules.type;
            string actual_type = val.type();
            bool type_ok = False;
            switch (expected_type) {
                case "string":
                    type_ok = (actual_type == "string");
                    break;
                case "int":
                    type_ok = (actual_type == "integer");
                    break;
                case "float":
                    type_ok = (actual_type == "float" || actual_type == "integer");
                    break;
                case "bool":
                    type_ok = (actual_type == "bool");
                    break;
                case "date":
                    type_ok = (actual_type == "date");
                    break;
                default:
                    type_ok = (actual_type == expected_type);
                    break;
            }
            if (!type_ok) {
                push violations,
                    sprintf("field '%s': expected type %s, got %s",
                        field_name, expected_type, actual_type);
            }
        }

        # check numeric range
        if (exists rules.min) {
            if (val.toFloat() < rules.min.toFloat()) {
                push violations,
                    sprintf("field '%s': value %g is below minimum %g",
                        field_name, val.toFloat(), rules.min.toFloat());
            }
        }
        if (exists rules.max) {
            if (val.toFloat() > rules.max.toFloat()) {
                push violations,
                    sprintf("field '%s': value %g exceeds maximum %g",
                        field_name, val.toFloat(), rules.max.toFloat());
            }
        }

        # check string length
        if (exists rules.min_length) {
            if (string(val).length() < rules.min_length.toInt()) {
                push violations,
                    sprintf("field '%s': length %d is below minimum length %d",
                        field_name, string(val).length(), rules.min_length.toInt());
            }
        }
        if (exists rules.max_length) {
            if (string(val).length() > rules.max_length.toInt()) {
                push violations,
                    sprintf("field '%s': length %d exceeds maximum length %d",
                        field_name, string(val).length(), rules.max_length.toInt());
            }
        }

        # check allowed values
        if (exists rules.allowed_values) {
            bool found = False;
            foreach auto av in (rules.allowed_values) {
                if (val == av) {
                    found = True;
                    break;
                }
            }
            if (!found) {
                push violations,
                    sprintf("field '%s': value %y not in allowed values %y",
                        field_name, val, rules.allowed_values);
            }
        }
    }

    #! Processes records in bulk hash-of-lists format without row materialization when possible
    private *hash<auto> processRecordsBulkImpl(hash<auto> records) {
        updatePendingOutputShape(DataProvider::DataProviderShape::HashOfLists);
        *auto result = processRecordsShapeImpl(records, DataProvider::DataProviderShape::HashOfLists);
        return exists result ? cast<hash>(result) : NOTHING;
    }

    #! Processes an explicitly negotiated pipeline shape without row materialization when possible
    private *auto processRecordsShapeImpl(auto records, DataProvider::DataProviderShape shape) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreSchemaValidationProcessor: not configured; provide constructor options";
        }

        updatePendingOutputShape(shape);
        if (shape == DataProvider::DataProviderShape::RowRecords) {
            return processRecordImpl(records);
        }

        *hash<auto> columns = getShapeColumns(records, shape);
        if (!exists columns) {
            return processShapeRowsFallback(records, shape);
        }

        int count = DataProvider::AbstractDataProcessor::shapeRecordCount(records, shape);
        if (!count) {
            return;
        }

        list<list<string>> all_errors = ();
        list<bool> valid = ();
        list<int> selected_indices = ();
        for (int row = 0; row < count; ++row) {
            list<string> violations = ();
            foreach string field_name in (keys schema) {
                hash<auto> rules = schema{field_name};
                *auto val = getColumnRowValue(columns, field_name, row, count);
                appendFieldViolations(\violations, field_name, rules, val);
            }
            bool is_valid = violations.lsize() == 0;
            push all_errors, violations;
            push valid, is_valid;
            if (is_valid) {
                push selected_indices, row;
            }
        }

        switch (on_violation) {
            case "enrich":
                return addValidationColumns(records, shape, all_errors, valid);
            case "drop":
                return selectRows(records, shape, selected_indices);
            case "throw": {
                for (int i = 0; i < all_errors.lsize(); ++i) {
                    if (all_errors[i]) {
                        throw "VALIDATION-ERROR",
                            sprintf("record failed validation: %s", all_errors[i].join("; ")),
                            all_errors[i];
                    }
                }
                return records;
            }
        }

        return records;
    }

    #! Returns top-level columns needed by the schema, or NOTHING when row fallback is required
    private *hash<auto> getShapeColumns(auto records, DataProvider::DataProviderShape shape) {
        list<string> fields = keys schema;
        foreach string field in (fields) {
            if (field.find(".") >= 0) {
                return;
            }
        }

        if (shape == DataProvider::DataProviderShape::HashOfLists) {
            hash<auto> columns;
            foreach string field in (fields) {
                if (records.hasKey(field)) {
                    columns{field} = records{field};
                }
            }
            return columns;
        }

        return DataProvider::AbstractDataProcessor::getTopLevelShapeColumns(records, shape, fields);
    }

    #! Uses the row-oriented implementation when a column fast path is not safe
    private *auto processShapeRowsFallback(auto records, DataProvider::DataProviderShape shape) {
        list<auto> rows = DataProvider::AbstractDataProcessor::convertShape(
            records, shape, DataProvider::DataProviderShape::RowRecords);
        *list<auto> results;
        foreach auto row in (rows) {
            *auto row_result = processRecordImpl(row);
            if (exists row_result) {
                if (row_result.typeCode() == NT_LIST) {
                    results += row_result;
                } else {
                    push results, row_result;
                }
            }
        }
        if (!results) {
            return;
        }
        hash<auto> bulk = DataProvider::AbstractDataProcessor::recordsToHashList(results);
        return AbstractDataProviderMLProcessor::convertBulkOutputShape(bulk, shape);
    }

    #! Reads one value from a shaped column, preserving missing-field semantics
    private static *auto getColumnRowValue(hash<auto> columns, string field_name, int row, int count) {
        if (!columns.hasKey(field_name)) {
            return;
        }

        auto column = columns{field_name};
        switch (column.typeCode()) {
            case NT_BUFFER:
            case NT_LIST:
                if (column.lsize() != count) {
                    throw "PROCESSOR-SHAPE-ERROR",
                        sprintf("schema field %y has %d values but the shaped block has %d rows",
                            field_name, column.lsize(), count);
                }
                return column[row];
        }
        return column;
    }

    #! Appends validation status columns while preserving the input shape
    private auto addValidationColumns(auto records, DataProvider::DataProviderShape shape,
            list<list<string>> errors, list<bool> valid) {
        return DataProvider::AbstractDataProcessor::appendShapeColumns(records, shape, {
            "_validation_errors": errors,
            "_is_valid": valid,
        });
    }

    #! Selects valid rows for drop mode while preserving the input shape
    private auto selectRows(auto records, DataProvider::DataProviderShape shape, list<int> selected_indices) {
        return DataProvider::AbstractDataProcessor::selectShapeRows(records, shape, selected_indices);
    }

    #! Supports shaped bulk API
    private bool supportsBulkApiImpl() {
        return True;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_TRANSFORM;
    }

    #! Returns NOTHING — output type is dynamic (input record enriched with validation fields)
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return NOTHING;
    }
}
}
