# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreSearchReplaceProcessor class definition

/** QoreSearchReplaceProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProvider module
public namespace DataProvider {
#! Pipeline processor that performs search and replace on a string field in each record
public class QoreSearchReplaceProcessor inherits AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreSearchReplaceProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "field": <DataProviderOptionInfo>{
                "display_name": "Field Name",
                "short_desc": "Field to apply search and replace on",
                "desc": "The name of the record field to apply the search and replace operation on; "
                    "the field must contain a string value; "
                    "supports dot-notation for nested fields (e.g. `address.street`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "search": <DataProviderOptionInfo>{
                "display_name": "Search Pattern",
                "short_desc": "Text or regex pattern to search for",
                "desc": "The search string or regular expression pattern to find in the field value",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "replace": <DataProviderOptionInfo>{
                "display_name": "Replacement String",
                "short_desc": "Replacement text",
                "desc": "The replacement string to substitute for matches",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "is_regex": <DataProviderOptionInfo>{
                "display_name": "Use Regex",
                "short_desc": "Use regex matching instead of literal",
                "desc": "If `True`, the search pattern is treated as a regular expression; "
                    "if `False`, it is treated as a literal string",
                "type": AbstractDataProviderTypeMap."bool",
                "required": False,
                "default_value": False,
            },
            "regex_options": <DataProviderOptionInfo>{
                "display_name": "Regex Options",
                "short_desc": "Regular expression options for matching",
                "desc": "A list of regular expression option names to apply when performing "
                    "regex matching (e.g. `RE_Global`, `RE_Caseless`)",
                "type": AbstractDataProviderTypeMap."*list",
                "required": False,
                "element_allowed_values": RegularExpressionArgInfo[2].allowed_values,
            },
        };
    }

    private {
        #! Field name
        string field;

        #! Search pattern
        string search;

        #! Replacement string
        string replace_str;

        #! Regex flag
        bool is_regex;

        #! Regex options bitfield
        int regex_opts = 0;

        #! Flag indicating if the processor has been configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.field.val()) {
            field = copts.field;
            search = copts.search;
            replace_str = copts.replace;
            is_regex = copts.is_regex ?? False;
            if (*list<auto> ropts = copts.regex_options) {
                regex_opts = foldl $1 | $2, (map RegexOpts{$1}, ropts);
            }
            configured = True;
        }
    }

    #! Returns the name of this data provider
    string getName() {
        return "search-replace";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Performs search and replace on a string field in each record; supports both literal "
            "string matching and regular expression patterns with configurable regex options";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record and returns the result
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR", "QoreSearchReplaceProcessor: not configured; provide 'field', "
                "'search', and 'replace' options";
        }
        # if the field doesn't exist or is not a string, pass through unchanged
        auto field_val = AbstractDataProvider::getFieldValue(record, field);
        if (!exists field_val || field_val.typeCode() != NT_STRING) {
            return record;
        }
        string val = field_val;
        if (is_regex) {
            val = regex_subst(val, search, replace_str, regex_opts);
        } else {
            if (regex_opts & RE_Global) {
                val = replace(val, search, replace_str);
            } else {
                # replace first occurrence only
                int pos = val.find(search);
                if (pos >= 0) {
                    val = val.substr(0, pos) + replace_str + val.substr(pos + search.length());
                }
            }
        }
        return record + {field: val};
    }

    #! Indicates that this processor does not support the bulk API
    private bool supportsBulkApiImpl() {
        return False;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_TRANSFORM;
    }
}
}
