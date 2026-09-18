# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

namespace Priv {
    # Immutable module resources are read during initialization, before sandboxed action execution.
    const TextractTypes = parse_json(File::readTextFile(get_script_dir() + "/textract-types.json"));
    const TextractModel = parse_json(File::readTextFile(get_script_dir() + "/textract-service-2.json"));
}

public namespace AwsTextractDataProvider {
#! Native AWS JSON contracts with shared schema-backed presentation and serialization
/** The generated OpenAPI resource contains component types only, never invented REST endpoints.
    Native operations and wire conversion use the pinned official AWS JSON 1.1 model.
*/
public class AwsTextractSchema {
    private {
        static *OpenApi3::OpenApi3Schema schema;
        static hash<string, AbstractDataProviderType> types;
        static Mutex lck();
        static Mutex legacy_lck();
        static hash<string, HashDataType> legacy_types;
    }

    #! Returns the compatible document action form with complete nested query and adapter types
    /** @param action one of the original two document action IDs
        @return typed fields retaining the original option identities
    */
    static HashDataType getLegacyRequestType(string action) {
        AutoLock al(legacy_lck);
        if (!legacy_types{action}) {
            HashDataType type("AwsTextract-" + action);
            type.addQoreFields(action == "analyze-document" ? AwsTextractAnalyzeDocumentRequestDataType::Fields
                : AwsTextractDetectDocumentTextRequestDataType::Fields);
            if (action == "analyze-document") {
                type.replaceField(new QoreDataField(AwsTextractAnalyzeDocumentRequestDataType::Fields.queries
                    + {"name": "queries", "type": getType("Queries").getOrNothingType()}));
                HashDataType native = cast<HashDataType>(getType("AnalyzeDocumentRequest"));
                type.addField(native.getField("HumanLoopConfig").getRenamedField("human_loop_config", "Human Review"));
                type.addField(native.getField("AdaptersConfig").getRenamedField("adapters_config", "Adapters"));
            }
            legacy_types{action} = type;
        }
        return legacy_types{action};
    }

    #! Returns a complete reusable type from the pinned model
    /** @param name official shape name
        @return a shared schema-backed type
        @throw AWS-TEXTRACT-SCHEMA-ERROR unsupported import boundary
    */
    static AbstractDataProviderType getType(string name) {
        AutoLock al(lck);
        if (!schema) {
            if (Priv::TextractTypes."x-qore-textract-boundary".version != 1) {
                throw "AWS-TEXTRACT-SCHEMA-ERROR", "unsupported Textract type boundary";
            }
            OpenApi3::OpenApi3Schema parsed("textract-types.json", Priv::TextractTypes);
            hash<string, AbstractDataProviderType> complete = {};
            foreach string shape in (keys Priv::TextractTypes.components.schemas) {
                complete{shape} = parsed.getTypeForReference(shape);
            }
            foreach hash<auto> entry in (complete.pairIterator()) {
                decorate(entry.key, entry.value);
            }
            # Publish only after decoration is complete: readers never see schema mutation.
            types = complete;
            schema = parsed;
        }
        if (!types.hasKey(name)) {
            throw "AWS-TEXTRACT-SCHEMA-ERROR", "unknown imported shape " + name;
        }
        return types{name};
    }

    # Native list enums need field-level choices as well as a schema-backed element type.
    private static decorate(string name, AbstractDataProviderType type, int depth = 0) {
        if (depth > 32) { throw "AWS-TEXTRACT-SCHEMA-ERROR", "model nesting exceeds the import boundary"; }
        hash<auto> shape = Priv::TextractModel.shapes{name};
        if (shape.type == "structure") {
            foreach hash<auto> member in (shape.members.pairIterator()) {
                AbstractDataField field = type.getFields(){member.key}.copy();
                cast<HashDataType>(type).replaceField(field);
                hash<auto> child = Priv::TextractModel.shapes{member.value.shape};
                if (child{"enum"}) {
                    field.setAllowedValues(map <AllowedValueInfo>{"value": $1,
                        "display_name": AbstractDataField::getDisplayName($1 =~ /^[A-Z_]+$/ ? $1.lwr() : $1)}, child{"enum"});
                }
                if (child.type == "list") {
                    *list<string> choices = Priv::TextractModel.shapes{child.member.shape}{"enum"};
                    if (name == "CreateAdapterRequest" && member.key == "FeatureTypes") { choices = ("QUERIES",); }
                    if (choices) {
                        field.setElementAllowedValues(map <AllowedValueInfo>{"value": $1,
                            "display_name": AbstractDataField::getDisplayName($1 =~ /^[A-Z_]+$/ ? $1.lwr() : $1)}, choices);
                    }
                }
                decorate(member.value.shape, field.getType(), depth + 1);
            }
        } else if (shape.type == "list") {
            decorate(shape.member.shape, type.getElementType(), depth + 1);
        }
    }

    #! Validates native input without coercing absent, null, empty, false, or zero values
    /** @param name official input shape name
        @param value native request value
        @param path diagnostic field path
        @throw AWS-TEXTRACT-INPUT-ERROR invalid request value
    */
    static validate(string name, auto value, string path = "request") {
        hash<auto> shape = Priv::TextractModel.shapes{name};
        string kind = shape.type;
        if (kind == "structure") {
            if (value.typeCode() != NT_HASH) {
                invalid(path, "must be an object");
            }
            foreach string required in (shape.required) {
                if (!value.hasKey(required)) {
                    invalid(path + "." + required, "is required");
                }
            }
            foreach hash<auto> member in (value.pairIterator()) {
                if (!shape.members.hasKey(member.key)) {
                    invalid(path + "." + member.key, "is not a supported field");
                }
                validate(shape.members{member.key}.shape, member.value, path + "." + member.key);
            }
            if (name == "Document" && int(value.hasKey("Bytes")) + int(value.hasKey("S3Object")) != 1) {
                invalid(path, "requires exactly one of Bytes or S3Object");
            }
            if (name == "DocumentLocation" && !value.hasKey("S3Object")) {
                invalid(path, "requires S3Object");
            }
            if (name == "AdapterVersionDatasetConfig" && !value.hasKey("ManifestS3Object")) {
                invalid(path, "requires ManifestS3Object");
            }
            if (name == "S3Object" && (!value.hasKey("Bucket") || !value.hasKey("Name"))) {
                invalid(path, "requires Bucket and Name");
            }
            if (name == "AnalyzeDocumentRequest" || name == "StartDocumentAnalysisRequest") {
                bool queries = inlist("QUERIES", value.FeatureTypes);
                if (queries != value.hasKey("QueriesConfig")) {
                    invalid(path, "QUERIES and QueriesConfig must be supplied together");
                }
                if (value.hasKey("AdaptersConfig") && !queries) {
                    invalid(path, "AdaptersConfig requires QUERIES");
                }
            }
            if (name == "CreateAdapterRequest" && value.FeatureTypes != ("QUERIES",)) {
                invalid(path + ".FeatureTypes", "custom adapters support only QUERIES");
            }
            if (value.hasKey("AfterCreationTime") && value.hasKey("BeforeCreationTime")
                    && value.AfterCreationTime > value.BeforeCreationTime) {
                invalid(path, "creation time range is reversed");
            }
            return;
        }
        if (kind == "list") {
            if (value.typeCode() != NT_LIST) {
                invalid(path, "must be a list");
            }
            bounds(shape, value.size(), path);
            if (name == "FeatureTypes" && !value.size()) {
                invalid(path, "requires at least one feature");
            }
            if (name == "DocumentPages" && (value.size() < 1 || value.size() > 2)) {
                invalid(path, "requires one or two identity document images");
            }
            foreach auto element in (value) {
                validate(shape.member.shape, element, path + "[" + $# + "]");
            }
            if (name == "QueryPages" || name == "AdapterPages") {
                foreach string page in (value) {
                    if (page == "*") {
                        if (value.size() != 1) { invalid(path, "all pages must be the only selector"); }
                    } else if (page !~ /^[1-9][0-9]*(-([1-9][0-9]*|\*))?$/) {
                        invalid(path, "contains an invalid page selector");
                    } else if (page =~ /^[0-9]+-[0-9]+$/) {
                        list<string> range = page.split("-");
                        if (int(range[0]) > int(range[1])) { invalid(path, "contains a reversed page range"); }
                    }
                }
            }
            return;
        }
        if (kind == "map") {
            if (value.typeCode() != NT_HASH) { invalid(path, "must be a map"); }
            bounds(shape, value.size(), path);
            foreach hash<auto> item in (value.pairIterator()) {
                validate(shape.key.shape, item.key, path + ".key");
                validate(shape.value.shape, item.value, path + ".value");
            }
            return;
        }
        if (kind == "blob") {
            if (value.typeCode() == NT_STRING) {
                string encoded = value;
                try { value = parse_base64_string(value); }
                catch () { invalid(path, "must contain valid base64 document bytes"); }
                if (make_base64_string(value) != encoded) { invalid(path, "must contain canonical base64"); }
            }
            if (value.typeCode() != NT_BINARY) { invalid(path, "must contain binary or base64 document bytes"); }
            bounds(shape, value.size(), path);
            return;
        }
        if (kind == "string") {
            if (value.typeCode() != NT_STRING) { invalid(path, "must be a string"); }
            bounds(shape, value.size(), path);
            if (shape{"enum"} && !inlist(value, shape{"enum"})) { invalid(path, "is not an allowed value"); }
            if (shape.pattern && !value.regex(shape.pattern)) { invalid(path, "does not match the required format"); }
            return;
        }
        if (kind == "timestamp") {
            if (value.typeCode() != NT_DATE) { invalid(path, "must be a date/time value"); }
            return;
        }
        if (kind == "boolean") {
            if (value.typeCode() != NT_BOOLEAN) { invalid(path, "must be a boolean"); }
            return;
        }
        if (kind == "integer" || kind == "long") {
            if (value.typeCode() != NT_INT) { invalid(path, "must be an integer"); }
        } else if (kind == "float" || kind == "double") {
            if (value.typeCode() != NT_INT && value.typeCode() != NT_FLOAT) { invalid(path, "must be numeric"); }
        } else {
            throw "AWS-TEXTRACT-SCHEMA-ERROR", "unsupported native shape type " + kind;
        }
        bounds(shape, value, path);
    }

    #! Converts only declared timestamp and blob positions across the native wire boundary
    /** @param name official shape name
        @param value request or response
        @param response convert epoch seconds to dates when set
        @return converted value, preserving unrelated objects and empty values
    */
    static auto convert(string name, auto value, bool response = False) {
        hash<auto> shape = Priv::TextractModel.shapes{name};
        if (shape.type == "timestamp") {
            if (response) {
                return gmtime(int(value), int(round((float(value) - int(value)) * 1000000.0)));
            }
            int micros = get_epoch_microseconds(value);
            return micros % 1000000 ? micros / 1000000.0 : micros / 1000000;
        }
        if (shape.type == "blob" && !response && value.typeCode() == NT_BINARY) {
            return make_base64_string(value);
        }
        if (shape.type == "structure" && value.typeCode() == NT_HASH) {
            hash<auto> converted = {};
            foreach hash<auto> member in (value.pairIterator()) {
                converted{member.key} = shape.members.hasKey(member.key)
                    ? convert(shape.members{member.key}.shape, member.value, response) : member.value;
            }
            return converted;
        } else if (shape.type == "list" && value.typeCode() == NT_LIST) {
            value = map convert(shape.member.shape, $1, response), value;
        } else if (shape.type == "map" && value.typeCode() == NT_HASH) {
            hash<auto> converted = {};
            foreach hash<auto> item in (value.pairIterator()) {
                converted{item.key} = convert(shape.value.shape, item.value, response);
            }
            return converted;
        }
        return value;
    }

    private static bounds(hash<auto> shape, softfloat length, string path) {
        if (shape.hasKey("min") && length < shape.min || shape.hasKey("max") && length > shape.max) {
            invalid(path, "is outside the documented size or value range");
        }
    }

    private static invalid(string path, string reason) {
        throw "AWS-TEXTRACT-INPUT-ERROR", path + " " + reason;
    }
}
}
