# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Hl7v2SchemaDataProvider.qc HL7 v2 schema data provider

/*  Hl7v2SchemaDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
    #! HL7 v2 schema data provider
    public class Hl7v2SchemaDataProvider inherits DataProvider::AbstractDataProvider {
        public {
            const ResponseType = Hl7v2SchemaResultDataType;

            const ConstructorOptions = {
                "path": <DataProviderOptionInfo>{
                    "display_name": "Schema Path",
                    "short_desc": "Path to a generated HL7 v2 schema artifact",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "Path or FileLocationHandler-compatible location for a generated HL7 v2 schema "
                        "artifact in Qore value format.",
                },
                "version": <DataProviderOptionInfo>{
                    "display_name": "Schema Version",
                    "short_desc": "HL7 v2 version to load",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "HL7 v2 version to load from `schema_dir`. Use this with `schema_dir` when schemas "
                        "are stored as `<schema_dir>/<version>/schema.qv`.",
                },
                "schema_dir": <DataProviderOptionInfo>{
                    "display_name": "Schema Directory",
                    "short_desc": "Root directory containing generated schemas",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "Root directory containing generated HL7 v2 schemas organized by version.",
                },
                "summary_only": <DataProviderOptionInfo>{
                    "display_name": "Summary Only",
                    "short_desc": "Return summary only",
                    "type": AbstractDataProviderTypeMap."bool",
                    "desc": "If `True`, return schema metadata and counts without the full schema hash.",
                    "default_value": True,
                },
                "reload": <DataProviderOptionInfo>{
                    "display_name": "Reload",
                    "short_desc": "Ignore in-memory schema cache",
                    "type": AbstractDataProviderTypeMap."bool",
                    "desc": "If `True`, ignore the in-memory schema cache and reload the schema artifact.",
                    "default_value": False,
                },
            };

            const ProviderInfo = <DataProviderInfo>{
                "name": "hl7v2schema",
                "desc": "HL7 v2 schema data provider",
                "type": "Hl7v2SchemaDataProvider",
                "supports_read": True,
                "constructor_options": ConstructorOptions,
            };
        }

        private {
            Hl7v2Schema schema;
            bool summary_only = True;
        }

        #! Creates the schema provider
        constructor(*hash<auto> opts) {
            bool reload = opts.reload ?? False;
            if (opts.path) {
                schema = Hl7v2SchemaRegistry::loadFile(opts.path, reload);
            } else if (opts.version && opts.schema_dir) {
                schema = Hl7v2SchemaRegistry::loadVersion(opts.version, opts.schema_dir, reload);
            } else {
                throw "HL7V2-SCHEMA-ERROR", "provide `path` or both `version` and `schema_dir`";
            }
            summary_only = opts.summary_only ?? True;
        }

        #! Returns the provider name
        string getName() {
            return "hl7v2schema";
        }

        private hash<DataProviderInfo> getStaticInfoImpl() {
            return ProviderInfo;
        }

        private *hash<string, DataProvider::AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
            return ResponseType.getFields();
        }

        private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
                *hash<auto> search_options) {
            bool effective_summary = search_options.summary_only ?? summary_only;
            return new Hl7v2SchemaDataProviderRecordIterator(schema, effective_summary);
        }
    }

    #! HL7 v2 schema record iterator
    class Hl7v2SchemaDataProviderRecordIterator inherits DataProvider::AbstractDataProviderRecordIterator {
        private {
            hash<auto> record;
            bool done = False;
        }

        constructor(Hl7v2Schema schema, bool summary_only) {
            record = {
                "version": schema.version,
                "meta": schema.meta,
                "counts": {
                    "segments": schema.segments.size(),
                    "fields": getFieldCount(schema),
                },
                "segment_tags": schema.getSegmentTags(),
                "summary_only": summary_only,
                "schema": summary_only ? NOTHING : schema.toHash(),
            };
        }

        bool valid() {
            return !done;
        }

        private bool nextImpl() {
            if (done) {
                return False;
            }
            done = True;
            return True;
        }

        hash<auto> getValue() {
            return record;
        }

        *hash<string, DataProvider::AbstractDataField> getRecordType() {
            return Hl7v2SchemaDataProvider::ResponseType.getFields();
        }

        private int getFieldCount(Hl7v2Schema schema) {
            int count = 0;
            foreach hash<Hl7v2SegmentDef> segment in (schema.segments.iterator()) {
                count += segment.fields.size();
            }
            return count;
        }
    }

    #! Returns constructor options forced to schema-summary mode
    hash<auto> sub hl7v2_schema_summary_options(*hash<auto> opts) {
        hash<auto> effective_opts = opts ?? {};
        effective_opts.summary_only = True;
        return effective_opts;
    }

    #! HL7 v2 schema summary provider
    public class Hl7v2SchemaSummaryDataProvider inherits Hl7v2SchemaDataProvider {
        constructor(*hash<auto> opts) : Hl7v2SchemaDataProvider(hl7v2_schema_summary_options(opts)) {
        }
    }

    #! Factory for HL7 v2 schema provider
    public class Hl7v2SchemaDataProviderFactory inherits DataProvider::AbstractDataProviderFactory {
        public {
            const FactoryInfo = <DataProviderFactoryInfo>{
                "name": "hl7v2schema",
                "desc": "HL7 v2 schema data provider factory",
                "children_can_support_records": True,
            };
        }

        string getName() {
            return "hl7v2schema";
        }

        private hash<DataProviderFactoryInfo> getInfoImpl() {
            return FactoryInfo;
        }

        private hash<DataProviderInfo> getProviderInfoImpl() {
            return Hl7v2SchemaDataProvider::ProviderInfo;
        }

        private Class getClassImpl() {
            return Class::forName("Hl7v2Util::Hl7v2SchemaDataProvider");
        }
    }
}
