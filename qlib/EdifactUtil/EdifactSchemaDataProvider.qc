# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file EdifactSchemaDataProvider.qc EDIFACT schema data provider

/*  EdifactSchemaDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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


#! The EdifactUtil namespace contains all the definitions in the EdifactUtil module
public namespace EdifactUtil {
    #! EDIFACT schema data provider
    /** Provides a data provider interface for loading EDIFACT directory schemas.
    */
    public class EdifactSchemaDataProvider inherits DataProvider::AbstractDataProvider {
        public {
            #! Constructor options
            const ConstructorOptions = {
                "release": <DataProviderOptionInfo>{
                    "display_name": "Release",
                    "short_desc": "EDIFACT release identifier",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "EDIFACT release identifier (ex: D01B).",
                    "required": True,
                },
                "dir": <DataProviderOptionInfo>{
                    "display_name": "Directory",
                    "short_desc": "Directory containing EDIFACT flat files",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "Directory containing EDMD/EDSD/EDCD/EDED/UNCL flat files for the release.",
                },
                "cache_dir": <DataProviderOptionInfo>{
                    "display_name": "Cache Directory",
                    "short_desc": "Override cache directory root",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "Overrides QORE_EDIFACT_DIRCACHE for this process.",
                },
                "skip_cache_read": <DataProviderOptionInfo>{
                    "display_name": "Skip Cache Read",
                    "short_desc": "Do not read from on-disk cache",
                    "type": AbstractDataProviderTypeMap."bool",
                    "desc": "If True, skip reading the on-disk cache.",
                    "default_value": False,
                },
                "cache_only": <DataProviderOptionInfo>{
                    "display_name": "Cache Only",
                    "short_desc": "Load only from cache",
                    "type": AbstractDataProviderTypeMap."bool",
                    "desc": "If True, load only from cache and throw on cache miss.",
                    "default_value": False,
                },
                "cache_error": <DataProviderOptionInfo>{
                    "display_name": "Cache Error Handling",
                    "short_desc": "Handle cache parse/type errors",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "How to handle cache parse/type errors: ignore, warn, or error.",
                    "default_value": "warn",
                },
                "summary_only": <DataProviderOptionInfo>{
                    "display_name": "Summary Only",
                    "short_desc": "Return summary only",
                    "type": AbstractDataProviderTypeMap."bool",
                    "desc": "If True, return only counts and segment index without full schema data.",
                    "default_value": True,
                },
                "reload": <DataProviderOptionInfo>{
                    "display_name": "Reload",
                    "short_desc": "Ignore in-memory registry and reload",
                    "type": AbstractDataProviderTypeMap."bool",
                    "desc": "If True, ignore the in-memory registry and reload.",
                    "default_value": False,
                },
                "strict": <DataProviderOptionInfo>{
                    "display_name": "Strict EDMD Validation",
                    "short_desc": "Enable strict EDMD validation",
                    "type": AbstractDataProviderTypeMap."bool",
                    "desc": "If True, validate EDMD files more strictly (segment table header, indentation, "
                        "and structural integrity).",
                    "default_value": False,
                },
            };

            #! Provider info
            const ProviderInfo = <DataProviderInfo>{
                "name": "edifactschema",
                "desc": "EDIFACT schema data provider",
                "type": "EdifactSchemaDataProvider",
                "supports_read": True,
                "supports_search_expressions": True,
                "constructor_options": ConstructorOptions,
            };
        }

        private {
            EdifactSchema m_schema;
            list<string> m_cache_warnings = ();
        }

        #! Creates the data provider
        constructor(*hash<auto> opts) {
            if (opts.cache_dir) {
                setenv("QORE_EDIFACT_DIRCACHE", opts.cache_dir);
            }

            hash<auto> load_opts = {};
            if (opts.dir) {
                load_opts.dir = opts.dir;
            }
            if (opts.reload) {
                load_opts.reload = True;
            }
            if (opts.skip_cache_read) {
                load_opts.skip_cache_read = True;
            }
            if (opts.cache_only) {
                load_opts.cache_only = True;
            }
            if (opts.cache_error) {
                load_opts.cache_error = opts.cache_error;
            }
            if ((load_opts.cache_error ?? "warn") == "warn") {
                load_opts.cache_warnings = \m_cache_warnings;
            }
            if (opts.strict) {
                load_opts.loader_opts = ("strict": True);
            }

            m_schema = EdifactSchemaRegistry::loadRelease(opts.release, load_opts);
        }

        #! Returns the data provider name
        string getName() {
            return "edifactschema";
        }

        #! Returns the data provider info
        hash<DataProviderInfo> getStaticInfoImpl() {
            return ProviderInfo;
        }

        #! Returns an iterator for the data
        AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
            bool summary_only = search_options.summary_only ?? True;
            return new EdifactSchemaDataProviderRecordIterator(m_schema, summary_only, m_cache_warnings);
        }
    }

    #! Record iterator for EDIFACT schema data provider
    class EdifactSchemaDataProviderRecordIterator inherits DataProvider::AbstractDataProviderRecordIterator {
        private {
            hash<auto> m_record;
            bool m_done = False;
        }

        constructor(EdifactSchema schema, bool summary_only, list<string> cache_warnings) {
            hash<string, list<hash<auto>>> index = {};
            foreach hash<EdifactMessageDef> msg in (schema.message_defs.iterator()) {
                if (msg.segment_index) {
                    index{msg.type} = msg.segment_index;
                }
            }
            m_record = {
                "release": schema.release,
                "meta": schema.meta,
                "counts": {
                    "elements": schema.element_defs.size(),
                    "composites": schema.composite_defs.size(),
                    "segments": schema.segment_defs.size(),
                    "messages": schema.message_defs.size(),
                },
                "index": index,
                "schema": summary_only ? NOTHING : schema.toHash(),
                "summary_only": summary_only,
                "cache_warnings": cache_warnings.size() ? cache_warnings : NOTHING,
            };
        }

        bool valid() {
            return !m_done;
        }

        bool nextImpl() {
            if (m_done) {
                return False;
            }
            m_done = True;
            return True;
        }

        hash<auto> getValue() {
            return m_record;
        }

        *hash<string, DataProvider::AbstractDataField> getRecordType() {
            return {
                "release": new DataProvider::QoreDataField("release", "EDIFACT release", StringType),
                "meta": new DataProvider::QoreDataField("meta", "Schema metadata", AutoType),
                "counts": new DataProvider::QoreDataField("counts", "Schema counts", AutoType),
                "index": new DataProvider::QoreDataField("index", "Segment index by message", AutoType),
                "summary_only": new DataProvider::QoreDataField("summary_only", "Summary-only response", BoolType),
                "schema": new DataProvider::QoreDataField("schema", "Schema data", AutoType),
            };
        }
    }

    #! EDIFACT schema summary data provider
    /** Hard-wires summary_only=True for app action use.
    */
    public class EdifactSchemaSummaryDataProvider inherits EdifactSchemaDataProvider {
        constructor(*hash<auto> opts)
            : EdifactSchemaDataProvider((opts ?? {}) + {"summary_only": True}) {
        }
    }

    #! Factory for EDIFACT schema data provider
    public class EdifactSchemaDataProviderFactory inherits DataProvider::AbstractDataProviderFactory {
        public {
            #! Factory info
            const FactoryInfo = <DataProviderFactoryInfo>{
                "name": "edifactschema",
                "desc": "EDIFACT schema data provider factory",
                "children_can_support_records": True,
            };
        }

        #! Returns the factory name
        string getName() {
            return "edifactschema";
        }

        #! Returns the factory info
        hash<DataProviderFactoryInfo> getInfoImpl() {
            return FactoryInfo;
        }

        #! Returns the provider info
        hash<DataProviderInfo> getProviderInfoImpl() {
            return EdifactSchemaDataProvider::ProviderInfo;
        }

        #! Returns the class for the data provider
        Class getClassImpl() {
            return Class::forName("EdifactUtil::EdifactSchemaDataProvider");
        }
    }

}
