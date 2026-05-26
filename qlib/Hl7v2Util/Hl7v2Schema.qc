# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Hl7v2Schema.qc HL7 v2 schema loading, generation, and validation support

/*  Hl7v2Schema.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Hl7v2Util module namespace
public namespace Hl7v2Util {
    #! HL7 v2 schema field usage codes
    public enum Hl7v2SchemaUsage : string {
        Required = "R",
        Optional = "O",
        Conditional = "C",
    }

    #! HL7 v2 schema validation issue severity
    public enum Hl7v2ValidationSeverity : string {
        Error = "error",
        Warning = "warning",
    }

    #! HL7 v2 generated schema metadata
    public hashdecl Hl7v2SchemaMeta {
        string version;
        *string source;
        *string generated_from;
        *string generator;
        *string license;
        *string note;
    }

    #! HL7 v2 segment field definition
    public hashdecl Hl7v2FieldDef {
        int position;
        string name;
        string datatype;
        string usage;
        int min;
        string max_repetitions;
        bool repeating;
        *int max_length;
        *string desc;
    }

    #! HL7 v2 segment definition
    public hashdecl Hl7v2SegmentDef {
        string tag;
        string name;
        list<hash<Hl7v2FieldDef>> fields;
        *string desc;
    }

    #! HL7 v2 schema data hash format
    public hashdecl Hl7v2SchemaData {
        string version;
        hash<Hl7v2SchemaMeta> meta;
        hash<string, hash<Hl7v2SegmentDef>> segments;
    }

    #! HL7 v2 schema validation issue
    public hashdecl Hl7v2ValidationIssue {
        string severity;
        string code;
        string message;
        *string segment;
        *int segment_index;
        *int field;
        *string address;
    }

    #! Converts a value to a string without leaking message payloads in exceptions
    string sub hl7v2_schema_to_string(auto val) {
        return val === NOTHING ? "" : string(val);
    }

    #! Converts a value to an int for schema normalization
    int sub hl7v2_schema_to_int(auto val) {
        if (val === NOTHING) {
            return 0;
        }
        if (val.typeCode() == NT_INT) {
            return val;
        }

        string s = trim(string(val));
        if (s !~ /^-?\d+$/) {
            throw "HL7V2-SCHEMA-ERROR", sprintf("invalid integer value in HL7 v2 schema: %y", s);
        }
        return int(s);
    }

    #! Converts a value to a bool for schema normalization
    bool sub hl7v2_schema_to_bool(auto val) {
        if (val === NOTHING) {
            return False;
        }
        if (val.typeCode() == NT_BOOLEAN) {
            return val;
        }

        string s = tolower(trim(string(val)));
        if (s == "true" || s == "1") {
            return True;
        }
        if (s == "false" || s == "0" || !s) {
            return False;
        }
        throw "HL7V2-SCHEMA-ERROR", sprintf("invalid boolean value in HL7 v2 schema: %y", s);
    }

    #! Normalizes a field usage code
    string sub hl7v2_normalize_usage(auto usage, int min) {
        string u = hl7v2_schema_to_string(usage);
        if (!u) {
            return min > 0 ? Hl7v2SchemaUsage::Required : Hl7v2SchemaUsage::Optional;
        }
        if (u != Hl7v2SchemaUsage::Required && u != Hl7v2SchemaUsage::Optional
            && u != Hl7v2SchemaUsage::Conditional) {
            throw "HL7V2-SCHEMA-ERROR", sprintf("invalid HL7 v2 field usage code: %y", u);
        }
        return u;
    }

    #! Normalizes a field definition hash
    hash<Hl7v2FieldDef> sub hl7v2_normalize_field_def(hash val, int fallback_position) {
        int position = hl7v2_schema_to_int(val.position ?? fallback_position);
        if (position < 1) {
            throw "HL7V2-SCHEMA-ERROR", sprintf("invalid HL7 v2 field position: %d", position);
        }

        int min = hl7v2_schema_to_int(val.min);
        if (min < 0) {
            throw "HL7V2-SCHEMA-ERROR", sprintf("invalid minimum cardinality for field %d", position);
        }

        string max_repetitions = hl7v2_schema_to_string(val.max_repetitions ?? val.max ?? "1");
        if (!max_repetitions) {
            max_repetitions = "1";
        }
        if (max_repetitions != "*" && max_repetitions !~ /^\d+$/) {
            throw "HL7V2-SCHEMA-ERROR", sprintf("invalid maximum cardinality for field %d: %y",
                position, max_repetitions);
        }
        if (max_repetitions != "*" && int(max_repetitions) < min) {
            throw "HL7V2-SCHEMA-ERROR", sprintf("maximum cardinality is below minimum for field %d", position);
        }

        int max_length = hl7v2_schema_to_int(val.max_length);
        if (max_length < 0) {
            throw "HL7V2-SCHEMA-ERROR", sprintf("invalid maximum length for field %d: %d", position, max_length);
        }
        return <Hl7v2FieldDef>{
            "position": position,
            "name": hl7v2_schema_to_string(val.name),
            "datatype": hl7v2_schema_to_string(val.datatype),
            "usage": hl7v2_normalize_usage(val.usage, min),
            "min": min,
            "max_repetitions": max_repetitions,
            "repeating": val.repeating === NOTHING ? max_repetitions != "1" : hl7v2_schema_to_bool(val.repeating),
            "max_length": max_length > 0 ? max_length : NOTHING,
            "desc": val.desc ? hl7v2_schema_to_string(val.desc) : NOTHING,
        };
    }

    #! Normalizes a segment definition hash
    hash<Hl7v2SegmentDef> sub hl7v2_normalize_segment_def(hash val, string key) {
        string tag = hl7v2_schema_to_string(val.tag ?? key);
        Hl7v2Base::validateSegmentTag(tag);

        list<hash<Hl7v2FieldDef>> fields = ();
        int position = 0;
        if (val.fields) {
            foreach auto field in (val.fields) {
                ++position;
                push fields, hl7v2_normalize_field_def(cast<hash>(field), position);
            }
        }

        return <Hl7v2SegmentDef>{
            "tag": tag,
            "name": hl7v2_schema_to_string(val.name),
            "fields": fields,
            "desc": val.desc ? hl7v2_schema_to_string(val.desc) : NOTHING,
        };
    }

    #! HL7 v2 schema container and structural validator
    public class Hl7v2Schema {
        public {
            string version;
            hash<Hl7v2SchemaMeta> meta;
            hash<string, hash<Hl7v2SegmentDef>> segments;
        }

        #! Creates an empty schema for the given version
        constructor(string version) {
            if (!version) {
                throw "HL7V2-SCHEMA-ERROR", "HL7 v2 schema version must not be empty";
            }
            self.version = version;
            meta = <Hl7v2SchemaMeta>{"version": version};
            segments = new hash<string, hash<Hl7v2SegmentDef>>();
        }

        #! Creates a schema from a normalized or generated hash
        constructor(hash data) {
            hash<auto> meta_data = data.meta ?? {};
            version = hl7v2_schema_to_string(data.version ?? meta_data.version);
            if (!version) {
                throw "HL7V2-SCHEMA-ERROR", "HL7 v2 schema version must not be empty";
            }

            meta = <Hl7v2SchemaMeta>{
                "version": version,
                "source": meta_data.source,
                "generated_from": meta_data.generated_from,
                "generator": meta_data.generator,
                "license": meta_data.license,
                "note": meta_data.note,
            };
            segments = new hash<string, hash<Hl7v2SegmentDef>>();
            if (data.segments) {
                foreach string key in (keys data.segments) {
                    hash<Hl7v2SegmentDef> def = hl7v2_normalize_segment_def(cast<hash>(data.segments{key}), key);
                    segments{def.tag} = def;
                }
            }
        }

        #! Returns schema data as a serializable hash
        hash<Hl7v2SchemaData> toHash() {
            return <Hl7v2SchemaData>{
                "version": version,
                "meta": meta,
                "segments": segments,
            };
        }

        #! Returns True if the schema defines the segment tag
        bool hasSegment(string tag) {
            return exists segments{tag};
        }

        #! Returns a segment definition or NOTHING
        *hash<Hl7v2SegmentDef> getSegmentDef(string tag) {
            return segments{tag};
        }

        #! Returns sorted segment tags
        list<string> getSegmentTags() {
            return sort(keys segments);
        }

        #! Validates parsed message data against the schema
        list<hash<Hl7v2ValidationIssue>> validateMessage(hash<Hl7v2MessageInfo> message,
                *hash<auto> opts = NOTHING) {
            bool allow_unknown_segments = opts.allow_unknown_segments ?? True;
            bool allow_z_segments = opts.allow_z_segments ?? True;
            bool validate_required_fields = opts.validate_required_fields ?? True;
            bool validate_field_count = opts.validate_field_count ?? True;
            bool validate_repetitions = opts.validate_repetitions ?? True;
            bool validate_lengths = opts.validate_lengths ?? True;

            list<hash<Hl7v2ValidationIssue>> issues = ();
            foreach hash<Hl7v2Segment> segment in (message.segments) {
                *hash<Hl7v2SegmentDef> def = getSegmentDef(segment.tag);
                if (!def) {
                    if (segment.tag =~ /^Z/ && allow_z_segments) {
                        continue;
                    }
                    if (!allow_unknown_segments) {
                        push issues, makeIssue(Hl7v2ValidationSeverity::Error, "unknown-segment",
                            sprintf("segment %s is not defined in HL7 v2 schema %s", segment.tag, version),
                            segment, NOTHING);
                    }
                    continue;
                }

                hash<Hl7v2SegmentDef> segment_def = cast<hash<Hl7v2SegmentDef>>(def);
                issues += validateSegment(segment, segment_def, validate_required_fields, validate_field_count,
                    validate_repetitions, validate_lengths);
            }
            return issues;
        }

        #! Parses and validates message text against the schema
        list<hash<Hl7v2ValidationIssue>> validateMessageText(string data, *hash<auto> opts = NOTHING) {
            Hl7v2Base base();
            return validateMessage(base.parseMessage(data), opts);
        }

        #! Throws if message validation returns any error issues
        assertValidMessage(hash<Hl7v2MessageInfo> message, *hash<auto> opts = NOTHING) {
            list<hash<Hl7v2ValidationIssue>> issues = validateMessage(message, opts);
            list<string> errors = ();
            foreach hash<Hl7v2ValidationIssue> issue in (issues) {
                if (issue.severity == Hl7v2ValidationSeverity::Error) {
                    push errors, sprintf("%s: %s", issue.code, issue.message);
                }
            }
            if (errors) {
                throw "HL7V2-VALIDATION-ERROR",
                    sprintf("HL7 v2 message failed schema %s validation: %s", version, errors.join("; "));
            }
        }

        private list<hash<Hl7v2ValidationIssue>> validateSegment(hash<Hl7v2Segment> segment,
                hash<Hl7v2SegmentDef> def, bool validate_required_fields,
                bool validate_field_count, bool validate_repetitions, bool validate_lengths) {
            list<hash<Hl7v2ValidationIssue>> issues = ();
            if (validate_field_count && segment.fields.size() > def.fields.size()) {
                push issues, makeIssue(Hl7v2ValidationSeverity::Error, "unexpected-field",
                    sprintf("segment %s has field %d, but schema %s defines only %d fields",
                        segment.tag, segment.fields.size(), version, def.fields.size()),
                    segment, segment.fields.size());
            }

            foreach hash<Hl7v2FieldDef> field_def in (def.fields) {
                int offset = field_def.position - 1;
                bool has_field = offset >= 0 && offset < segment.fields.size();
                bool empty = !has_field || !segment.fields[offset].raw;
                bool required = field_def.min > 0 || field_def.usage == Hl7v2SchemaUsage::Required
                    || field_def.usage == "R";
                if (validate_required_fields && required && empty) {
                    push issues, makeIssue(Hl7v2ValidationSeverity::Error, "missing-required-field",
                        sprintf("%s-%d (%s) is required by schema %s", segment.tag, field_def.position,
                            field_def.name, version), segment, field_def.position);
                    continue;
                }
                if (!has_field) {
                    continue;
                }
                hash<Hl7v2Field> field_value = segment.fields[offset];
                if (validate_repetitions) {
                    *hash<Hl7v2ValidationIssue> issue = validateFieldRepetitions(segment, field_value, field_def);
                    if (issue) {
                        push issues, issue;
                    }
                }
                if (validate_lengths && field_def.max_length) {
                    *hash<Hl7v2ValidationIssue> issue = validateFieldLength(segment, field_value, field_def);
                    if (issue) {
                        push issues, issue;
                    }
                }
            }
            return issues;
        }

        private *hash<Hl7v2ValidationIssue> validateFieldRepetitions(hash<Hl7v2Segment> segment,
                hash<Hl7v2Field> field, hash<Hl7v2FieldDef> field_def) {
            int count = field.repetitions.size();
            if (field_def.max_repetitions != "*" && count > int(field_def.max_repetitions)) {
                return makeIssue(Hl7v2ValidationSeverity::Error, "too-many-repetitions",
                    sprintf("%s-%d (%s) has %d repetitions; schema %s allows %s", segment.tag,
                        field_def.position, field_def.name, count, version, field_def.max_repetitions),
                    segment, field_def.position);
            }
            return NOTHING;
        }

        private *hash<Hl7v2ValidationIssue> validateFieldLength(hash<Hl7v2Segment> segment,
                hash<Hl7v2Field> field, hash<Hl7v2FieldDef> field_def) {
            foreach auto repetition in (field.repetitions) {
                string value = flattenFieldValue(repetition);
                if (value.size() > field_def.max_length) {
                    return makeIssue(Hl7v2ValidationSeverity::Error, "field-too-long",
                        sprintf("%s-%d (%s) length %d exceeds schema maximum %d", segment.tag,
                            field_def.position, field_def.name, value.size(), field_def.max_length),
                        segment, field_def.position);
                }
            }
            return NOTHING;
        }

        private string flattenFieldValue(auto value) {
            if (value.typeCode() != NT_LIST) {
                return string(value);
            }
            list<string> values = ();
            foreach auto child in (value) {
                push values, flattenFieldValue(child);
            }
            return values.join("");
        }

        private hash<Hl7v2ValidationIssue> makeIssue(string severity, string code, string message,
                hash<Hl7v2Segment> segment, *int field) {
            return <Hl7v2ValidationIssue>{
                "severity": severity,
                "code": code,
                "message": message,
                "segment": segment.tag,
                "segment_index": segment.index,
                "field": field,
                "address": field ? sprintf("%s-%d", segment.tag, field) : segment.tag,
            };
        }
    }

    #! HL7 v2 schema registry
    public class Hl7v2SchemaRegistry {
        private {
            static hash<string, Hl7v2Schema> schema_cache = {};
            static Mutex schema_mutex();
        }

        #! Loads a schema from a Qore value file
        public static Hl7v2Schema loadFile(string path, bool reload = False) {
            string key = normalize_dir(path);
            if (!reload) {
                AutoLock al(schema_mutex);
                if (schema_cache{key}) {
                    return schema_cache{key};
                }
            }

            string contents = FileLocationHandler::getTextFileFromLocation(path);
            hash data = cast<hash>(Util::parse_to_qore_value(contents));
            Hl7v2Schema schema(data);

            AutoLock al(schema_mutex);
            schema_cache{key} = schema;
            return schema;
        }

        #! Loads a schema by version from a schema directory
        public static Hl7v2Schema loadVersion(string version, string schema_dir, bool reload = False) {
            if (!version) {
                throw "HL7V2-SCHEMA-ERROR", "HL7 v2 schema version must not be empty";
            }
            if (!schema_dir) {
                throw "HL7V2-SCHEMA-ERROR", "schema_dir must be provided when loading by version";
            }
            return loadFile(joinPath(joinPath(schema_dir, version), "schema.qv"), reload);
        }

        #! Registers a schema object in memory by version
        public static registerSchema(Hl7v2Schema schema) {
            AutoLock al(schema_mutex);
            schema_cache{"version:" + schema.version} = schema;
        }

        #! Returns a registered in-memory schema by version or NOTHING
        public static *Hl7v2Schema getRegisteredSchema(string version) {
            AutoLock al(schema_mutex);
            return schema_cache{"version:" + version};
        }

        private static string joinPath(string base, string leaf) {
            if (!base) {
                return leaf;
            }
            return base[base.size() - 1] == DirSep ? base + leaf : base + DirSep + leaf;
        }
    }

    #! Generator for HL7 v2 schema artifacts from v2+ FHIR StructureDefinition resources
    public class Hl7v2StructureDefinitionSchemaGenerator {
        private {
            const IgnoredElementNames = {
                "id": True,
                "meta": True,
                "implicitRules": True,
                "language": True,
                "text": True,
                "contained": True,
                "extension": True,
                "modifierExtension": True,
            };
        }

        #! Generates an HL7 v2 segment schema from StructureDefinition resource hashes
        Hl7v2Schema generateSchema(list<hash<auto>> resources, *hash<auto> opts = NOTHING) {
            string version = opts.version ?? inferVersion(resources);
            Hl7v2Schema schema(<Hl7v2SchemaData>{
                "version": version,
                "meta": <Hl7v2SchemaMeta>{
                    "version": version,
                    "source": opts.source,
                    "generated_from": "HL7 v2+ FHIR StructureDefinition",
                    "generator": "Hl7v2StructureDefinitionSchemaGenerator",
                    "license": opts.license,
                    "note": opts.note,
                },
                "segments": {},
            });

            foreach hash<auto> resource in (resources) {
                *hash<Hl7v2SegmentDef> segment = convertSegment(resource);
                if (segment) {
                    schema.segments{segment.tag} = segment;
                }
            }
            return schema;
        }

        private string inferVersion(list<hash<auto>> resources) {
            foreach hash<auto> resource in (resources) {
                if (resource.version) {
                    return string(resource.version);
                }
            }
            throw "HL7V2-SCHEMA-ERROR", "could not infer HL7 v2 schema version from StructureDefinitions";
        }

        private *hash<Hl7v2SegmentDef> convertSegment(hash<auto> resource) {
            if (resource.resourceType != "StructureDefinition") {
                return NOTHING;
            }

            string tag = resource.type ?? resource.name ?? resource.id ?? "";
            if (tag !~ /^[A-Z0-9]{3}$/) {
                return NOTHING;
            }
            if (!isSegmentResource(resource, tag)) {
                return NOTHING;
            }

            list<hash<Hl7v2FieldDef>> fields = ();
            int position = 0;
            foreach hash<auto> element in (getElements(resource)) {
                *hash<Hl7v2FieldDef> field = convertFieldElement(tag, element, ++position);
                if (!field) {
                    --position;
                    continue;
                }
                push fields, field;
            }
            if (!fields) {
                return NOTHING;
            }

            return <Hl7v2SegmentDef>{
                "tag": tag,
                "name": stripMarkdown(resource.description ?? resource.title ?? tag),
                "fields": fields,
                "desc": resource.description ? stripMarkdown(resource.description) : NOTHING,
            };
        }

        private bool isSegmentResource(hash<auto> resource, string tag) {
            if (resource.title && string(resource.title) =~ /\(Segment\)/) {
                return True;
            }
            if (resource.text.div && string(resource.text.div) =~ /Segment:/) {
                return True;
            }
            return resource.kind == "logical" && resource.type == tag && resource.derivation == "specialization";
        }

        private list<hash<auto>> getElements(hash<auto> resource) {
            auto element_list = resource.differential.element ?? resource.snapshot.element;
            list<hash<auto>> rv = ();
            if (!element_list) {
                return rv;
            }
            foreach auto element in (element_list) {
                hash<auto> e = cast<hash<auto>>(element);
                if (isFieldElement(resource.type, e)) {
                    push rv, e;
                }
            }
            return rv;
        }

        private bool isFieldElement(string tag, hash<auto> element) {
            string path = element.path ?? "";
            string prefix = tag + ".";
            if (path.substr(0, prefix.size()) != prefix) {
                return False;
            }
            string name = path.substr(prefix.size());
            return !IgnoredElementNames{name};
        }

        private *hash<Hl7v2FieldDef> convertFieldElement(string tag, hash<auto> element, int position) {
            string name = stripMarkdown(element.definition ?? element.short ?? element.path);
            if (!name) {
                name = element.path;
            }

            int min = hl7v2_schema_to_int(element.min);
            string max_repetitions = element.max ? string(element.max) : "1";
            string datatype = "-";
            if (element.type && element.type.size() && element.type[0].code) {
                datatype = trim(string(element.type[0].code));
            }

            int max_length = hl7v2_schema_to_int(element.maxLength);
            return <Hl7v2FieldDef>{
                "position": position,
                "name": name,
                "datatype": datatype,
                "usage": min > 0 ? Hl7v2SchemaUsage::Required : Hl7v2SchemaUsage::Optional,
                "min": min,
                "max_repetitions": max_repetitions,
                "repeating": max_repetitions != "1",
                "max_length": max_length > 0 ? max_length : NOTHING,
                "desc": name,
            };
        }

        private string stripMarkdown(string s) {
            string out = replace(s, "**", "");
            return trim(out);
        }
    }

%ifndef NoXml
    #! Generator for HL7 v2 schema artifacts from XML Schema package files
    public class Hl7v2XmlSchemaPackageGenerator {
        private {
            const AttrKey = "^attributes^";
            const GeneratedFrom = "HL7 v2 XML schema package";
            const GeneratorName = "Hl7v2XmlSchemaPackageGenerator";
            const DefaultNote = "Generated from a local user-supplied HL7 v2 XML schema package; no HL7 source "
                "artifact is bundled with Qore.";
        }

        #! Generates a schema from all \c .xsd files below the given directory
        Hl7v2Schema generateSchemaFromDirectory(string dir, *hash<auto> opts = NOTHING) {
            string source_dir = normalize_dir(dir);
            if (!is_dir(source_dir)) {
                throw "HL7V2-SCHEMA-GEN-ERROR", sprintf("HL7 v2 XML schema directory does not exist: %y",
                    source_dir);
            }
            hash<string, string> files = collectXsdFiles(source_dir);
            if (!files) {
                throw "HL7V2-SCHEMA-GEN-ERROR", sprintf("no HL7 v2 XML schema .xsd files found in %y",
                    source_dir);
            }
            return generateSchemaFromFiles(files, makeSourceOptions(opts, source_dir));
        }

        #! Generates a schema from a hash of file names to XML schema text
        Hl7v2Schema generateSchemaFromFiles(hash<string, string> files, *hash<auto> opts = NOTHING) {
            if (!files) {
                throw "HL7V2-SCHEMA-GEN-ERROR", "no HL7 v2 XML schema files supplied";
            }

            hash<string, hash<auto>> element_map;
            hash<string, hash<auto>> complex_types;
            hash<string, hash<auto>> simple_types;

            foreach string name in (sort(keys files)) {
                if (name !~ /\.xsd$/i) {
                    continue;
                }
                hash<auto> doc = parse_xml(files{name});
                hash<auto> schema = getSchemaNode(doc, name);
                mergeNamedNodes(\element_map, getChildren(schema, "element"), name);
                mergeNamedNodes(\complex_types, getChildren(schema, "complexType"), name);
                mergeNamedNodes(\simple_types, getChildren(schema, "simpleType"), name);
            }
            if (!element_map) {
                throw "HL7V2-SCHEMA-GEN-ERROR", "no top-level XML schema elements found in supplied files";
            }

            string version = opts.version ?? inferVersion(opts.source ?? "");
            hash<string, hash<Hl7v2SegmentDef>> segments;
            foreach string name in (sort(keys element_map)) {
                if (name !~ /^[A-Z0-9]{3}$/) {
                    continue;
                }
                *hash<Hl7v2SegmentDef> segment = convertSegment(element_map{name}, element_map, complex_types,
                    simple_types);
                if (segment) {
                    segments{segment.tag} = segment;
                }
            }
            if (!segments) {
                throw "HL7V2-SCHEMA-GEN-ERROR", "no HL7 v2 segment definitions found in supplied XML schemas";
            }

            return new Hl7v2Schema(<Hl7v2SchemaData>{
                "version": version,
                "meta": <Hl7v2SchemaMeta>{
                    "version": version,
                    "source": opts.source,
                    "generated_from": GeneratedFrom,
                    "generator": GeneratorName,
                    "license": opts.license,
                    "note": opts.note ?? DefaultNote,
                },
                "segments": segments,
            });
        }

%ifndef NoZip
        #! Generates a schema from a local ZIP file containing HL7 v2 XML schema files
        Hl7v2Schema generateSchemaFromZip(string path, *hash<auto> opts = NOTHING) {
            if (!is_file(path)) {
                throw "HL7V2-SCHEMA-GEN-ERROR", sprintf("HL7 v2 XML schema ZIP file does not exist: %y", path);
            }

            hash<string, string> files;
            ZipFile zip(path, "r");
            foreach hash<ZipEntryInfo> entry in (zip.entries()) {
                if (entry.name =~ /\.xsd$/i) {
                    files{entry.name} = zip.readText(entry.name);
                }
            }
            if (!files) {
                throw "HL7V2-SCHEMA-GEN-ERROR", sprintf("no HL7 v2 XML schema .xsd entries found in %y", path);
            }
            return generateSchemaFromFiles(files, makeSourceOptions(opts, path));
        }
%endif

        private hash<auto> makeSourceOptions(*hash<auto> opts, string source) {
            hash<auto> rv = opts ?? {};
            if (!rv.source) {
                rv.source = source;
            }
            return rv;
        }

        private hash<string, string> collectXsdFiles(string dir) {
            hash<string, string> rv;
            Dir d();
            d.chdir(dir);
            foreach string file in (sort(d.listFiles())) {
                if (file =~ /\.xsd$/i) {
                    string path = dir + DirSep + file;
                    rv{path} = ReadOnlyFile::readTextFile(path);
                }
            }
            foreach string subdir in (sort(d.listDirs())) {
                rv += collectXsdFiles(dir + DirSep + subdir);
            }
            return rv;
        }

        private string inferVersion(string source) {
            *list<*string> parts = regex_extract(source, "(2\\.[0-9]+(?:\\.[0-9]+)?)");
            if (parts) {
                return parts[0];
            }
            throw "HL7V2-SCHEMA-GEN-ERROR",
                "HL7 v2 schema version must be supplied when it cannot be inferred from the source path";
        }

        private hash<auto> getSchemaNode(hash<auto> doc, string source) {
            foreach string key in (keys doc) {
                if (localName(key) == "schema") {
                    return cast<hash<auto>>(doc{key});
                }
            }
            throw "HL7V2-SCHEMA-GEN-ERROR", sprintf("XML schema root element not found in %y", source);
        }

        private mergeNamedNodes(reference<hash<string, hash<auto>>> target, list<hash<auto>> nodes, string source) {
            foreach hash<auto> node in (nodes) {
                string name = getAttr(node, "name");
                if (name) {
                    target{name} = node;
                }
            }
        }

        private *hash<Hl7v2SegmentDef> convertSegment(hash<auto> element,
                hash<string, hash<auto>> element_map, hash<string, hash<auto>> complex_types,
                hash<string, hash<auto>> simple_types) {
            string tag = getAttr(element, "name");
            *hash<auto> complex_type = getElementComplexType(element, complex_types);
            if (!complex_type) {
                return NOTHING;
            }

            list<hash<auto>> refs = getSequenceElementRefs(cast<hash<auto>>(complex_type));
            list<hash<Hl7v2FieldDef>> fields = ();
            foreach hash<auto> ref_node in (refs) {
                *hash<Hl7v2FieldDef> field = convertFieldRef(tag, ref_node, element_map, simple_types);
                if (field) {
                    push fields, field;
                }
            }
            if (!fields) {
                return NOTHING;
            }

            return <Hl7v2SegmentDef>{
                "tag": tag,
                "name": getDocumentation(element) ?? tag,
                "fields": fields,
                "desc": getDocumentation(element),
            };
        }

        private *hash<auto> getElementComplexType(hash<auto> element, hash<string, hash<auto>> complex_types) {
            auto complex_type = getChild(element, "complexType");
            if (complex_type) {
                return cast<hash<auto>>(complex_type);
            }

            string type = cleanTypeName(getAttr(element, "type"));
            if (type) {
                return complex_types{type};
            }
            return NOTHING;
        }

        private list<hash<auto>> getSequenceElementRefs(hash<auto> complex_type) {
            auto sequence = getChild(complex_type, "sequence");
            if (!sequence) {
                return ();
            }
            list<hash<auto>> rv = ();
            foreach hash<auto> element in (getChildren(cast<hash<auto>>(sequence), "element")) {
                if (getAttr(element, "ref")) {
                    push rv, element;
                }
            }
            return rv;
        }

        private *hash<Hl7v2FieldDef> convertFieldRef(string tag, hash<auto> ref_node,
                hash<string, hash<auto>> element_map, hash<string, hash<auto>> simple_types) {
            string ref = cleanTypeName(getAttr(ref_node, "ref"));
            *list<*string> parts = regex_extract(ref, "^" + tag + "\\.([0-9]+)$");
            if (!parts) {
                return NOTHING;
            }

            int position = int(parts[0]);
            string min_occurs = getAttr(ref_node, "minOccurs");
            int min = hl7v2_schema_to_int(min_occurs ? min_occurs : "1");
            string max_occurs = getAttr(ref_node, "maxOccurs");
            string max_repetitions = normalizeMaxOccurs(max_occurs ? max_occurs : "1");
            *hash<auto> field_element = element_map{ref};
            string name = field_element ? getDocumentation(cast<hash<auto>>(field_element)) : NOTHING;
            if (!name) {
                name = sprintf("%s-%d", tag, position);
            }
            string datatype = field_element ? getFieldType(cast<hash<auto>>(field_element), simple_types) : "-";
            int max_length = field_element ? getMaxLength(cast<hash<auto>>(field_element), simple_types) : 0;

            return <Hl7v2FieldDef>{
                "position": position,
                "name": name,
                "datatype": datatype,
                "usage": min > 0 ? Hl7v2SchemaUsage::Required : Hl7v2SchemaUsage::Optional,
                "min": min,
                "max_repetitions": max_repetitions,
                "repeating": max_repetitions != "1",
                "max_length": max_length > 0 ? max_length : NOTHING,
                "desc": name,
            };
        }

        private string getFieldType(hash<auto> element, hash<string, hash<auto>> simple_types) {
            string type = cleanTypeName(getAttr(element, "type"));
            if (type) {
                if (simple_types{type}) {
                    string base = getRestrictionBase(simple_types{type});
                    if (base) {
                        return base;
                    }
                }
                return type;
            }

            auto simple_type = getChild(element, "simpleType");
            if (simple_type) {
                string base = getRestrictionBase(cast<hash<auto>>(simple_type));
                if (base) {
                    return base;
                }
            }
            return "-";
        }

        private int getMaxLength(hash<auto> element, hash<string, hash<auto>> simple_types) {
            auto simple_type = getChild(element, "simpleType");
            if (simple_type) {
                int len = getSimpleTypeMaxLength(cast<hash<auto>>(simple_type));
                if (len > 0) {
                    return len;
                }
            }

            string type = cleanTypeName(getAttr(element, "type"));
            if (type && simple_types{type}) {
                return getSimpleTypeMaxLength(simple_types{type});
            }
            return 0;
        }

        private int getSimpleTypeMaxLength(hash<auto> simple_type) {
            auto restriction = getChild(simple_type, "restriction");
            if (!restriction) {
                return 0;
            }
            auto max_length = getChild(cast<hash<auto>>(restriction), "maxLength");
            if (!max_length) {
                return 0;
            }
            return hl7v2_schema_to_int(getAttr(cast<hash<auto>>(max_length), "value"));
        }

        private string getRestrictionBase(hash<auto> simple_type) {
            auto restriction = getChild(simple_type, "restriction");
            if (!restriction) {
                return "";
            }
            return cleanTypeName(getAttr(cast<hash<auto>>(restriction), "base"));
        }

        private string normalizeMaxOccurs(string val) {
            if (!val || val == "1") {
                return "1";
            }
            if (val == "unbounded") {
                return "*";
            }
            return string(hl7v2_schema_to_int(val));
        }

        private *string getDocumentation(hash<auto> node) {
            auto annotation = getChild(node, "annotation");
            if (!annotation) {
                return NOTHING;
            }
            auto documentation = getChild(cast<hash<auto>>(annotation), "documentation");
            if (!documentation) {
                return NOTHING;
            }
            return cleanDocumentation(documentation);
        }

        private *string cleanDocumentation(auto doc) {
            if (doc.typeCode() == NT_LIST) {
                foreach auto entry in (doc) {
                    *string value = cleanDocumentation(entry);
                    if (value) {
                        return value;
                    }
                }
                return NOTHING;
            }
            if (doc.typeCode() == NT_HASH) {
                hash h = cast<hash>(doc);
                if (h{"^value^"}) {
                    return trim(string(h{"^value^"}));
                }
                return NOTHING;
            }

            string value = trim(string(doc));
            return value ? value : NOTHING;
        }

        private auto getChild(hash<auto> node, string child_name) {
            foreach string key in (keys node) {
                if (localName(key) == child_name) {
                    return node{key};
                }
            }
            return NOTHING;
        }

        private list<hash<auto>> getChildren(hash<auto> node, string child_name) {
            list<hash<auto>> rv = ();
            foreach string key in (keys node) {
                if (localName(key) != child_name) {
                    continue;
                }
                auto val = node{key};
                if (val.typeCode() == NT_LIST) {
                    foreach auto item in (val) {
                        push rv, cast<hash<auto>>(item);
                    }
                } else {
                    push rv, cast<hash<auto>>(val);
                }
            }
            return rv;
        }

        private string getAttr(hash<auto> node, string name) {
            hash attrs = cast<hash>(node{AttrKey} ?? {});
            return attrs{name} ? string(attrs{name}) : "";
        }

        private string cleanTypeName(string name) {
            if (!name) {
                return "";
            }
            *list<*string> parts = regex_extract(name, "^(?:[^:]+:)?(.+)$");
            return parts ? parts[0] : name;
        }

        private string localName(string name) {
            *list<*string> parts = regex_extract(name, "^(?:[^:]+:)?(.+)$");
            return parts ? parts[0] : name;
        }
    }
%endif

    #! Serializes schema data as a deterministic Qore value string
    public class Hl7v2SchemaValueSerializer {
        #! Serializes any schema-compatible value
        public static string serialize(auto val) {
            if (val === NOTHING) {
                throw "HL7V2-SCHEMA-SERIALIZATION-ERROR", "cannot serialize NOTHING value";
            }
            switch (val.typeCode()) {
                case NT_STRING:
                    return serializeString(val);
                case NT_INT:
                    return sprintf("%d", val);
                case NT_FLOAT:
                    return sprintf("%.25g", val);
                case NT_BOOLEAN:
                    return val ? "true" : "false";
                case NT_LIST:
                    return serializeList(val);
                case NT_HASH:
                    return serializeHash(val);
            }
            throw "HL7V2-SCHEMA-SERIALIZATION-ERROR", sprintf("cannot serialize values of type %y", val.type());
        }

        #! Serializes a schema object
        public static string serializeSchema(Hl7v2Schema schema) {
            return serialize(schema.toHash());
        }

        private static string serializeString(string val) {
            string out;
            int len = val.size();
            for (int i = 0; i < len; ++i) {
                string ch = val[i];
                int code = ord(ch);
                if (ch == "\\") {
                    out += "\\\\";
                } else if (ch == "\"") {
                    out += "\\\"";
                } else if (code == 0x0a) {
                    out += "\\n";
                } else if (code == 0x0d) {
                    out += "\\r";
                } else if (code == 0x09) {
                    out += "\\t";
                } else if (code < 0x20 || code == 0x7f) {
                    out += sprintf("\\x%02x", code);
                } else {
                    out += ch;
                }
            }
            return "\"" + out + "\"";
        }

        private static string serializeList(list val) {
            if (!val) {
                return "()";
            }
            list<string> out = ();
            foreach auto item in (val) {
                push out, serialize(item);
            }
            return "(" + out.join(",") + ")";
        }

        private static string serializeHash(hash val) {
            if (!val) {
                return "{}";
            }
            list<string> out = ();
            foreach string key in (sort(keys val)) {
                if (val{key} !== NOTHING) {
                    push out, serializeString(key) + "=" + serialize(val{key});
                }
            }
            return "{" + out.join(",") + "}";
        }
    }
}
