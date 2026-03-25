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
public class QoreSchemaValidationProcessor inherits DataProvider::AbstractDataProvider {
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
                "display_name": "Validation Schema",
                "short_desc": "Schema defining validation rules for each field",
                "desc": "A hash where each key is a field name and the value is a hash with "
                    "optional validation rules:\n"
                    "- `type` (*string): expected type (\"string\", \"int\", \"float\", \"bool\", "
                    "\"date\")\n"
                    "- `required` (*bool): whether the field must be present and non-null "
                    "(default False)\n"
                    "- `min` (*float): minimum numeric value\n"
                    "- `max` (*float): maximum numeric value\n"
                    "- `min_length` (*int): minimum string length\n"
                    "- `max_length` (*int): maximum string length\n"
                    "- `allowed_values` (*list<auto>): list of allowed values (enum check)\n\n"
                    "Example: `{\"age\": {\"type\": \"int\", \"required\": True, \"min\": 0, "
                    "\"max\": 150}, \"name\": {\"type\": \"string\", \"required\": True, "
                    "\"min_length\": 1}}`",
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

        list<string> violations = ();

        foreach string field_name in (keys schema) {
            hash<auto> rules = schema{field_name};
            auto val = DataProvider::AbstractDataProvider::getFieldValue(record, field_name);

            # check required
            bool is_required = rules.required.toBool();
            if (!exists val) {
                if (is_required) {
                    push violations,
                        sprintf("field '%s': required but missing or null", field_name);
                }
                # skip other checks for missing values
                continue;
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

    #! Does not support bulk API
    private bool supportsBulkApiImpl() {
        return False;
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
