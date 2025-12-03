# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MewsRestDataProvider module definition

/** MewsRestTableDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the MewsRestDataProvider module
public namespace MewsRestDataProvider {
#! Table API config
hashdecl MewsTableApiConfig {
    #! The name of the table
    string table;

    #! Child data provider path
    string path;

    #! Data key in message body
    string body_key;

    #! Set of fields that can support date/time comparisons in requests
    hash<string, bool> comparison_fields;

    #! Set of fields that can support equality checks in requests
    hash<string, bool> equality_fields;

    #! Set of fields that can support list equality checks in requests
    hash<string, bool> equality_list_fields;

    #! Supported extent
    hash<string, bool> extent;

    #! Default extent
    hash<string, bool> default_extent;

    #! Maximum time difference for searches
    date max_delta = 3M;

    #! Does not support the "Limitation" option
    bool no_limitation;
}

#! The MewsRestTableDataProvider data provider class
public class MewsRestTableDataProvider inherits MewsRestDataProviderBase {
    public {
        #! Constructor options
        const ConstructorOptions = MewsRestDataProvider::ConstructorOptions + {
            "table": <DataProviderOptionInfo>{
                "display_name": "Table",
                "short_desc": "The table name",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The table name",
                "required": True,
            },
        };

        #! Supported Expressions
        const Expressions = (AbstractDataProvider::GenericExpressions{
            DP_OP_AND,
            DP_SEARCH_OP_GE,
            DP_SEARCH_OP_LE,
            DP_SEARCH_OP_GT,
            DP_SEARCH_OP_LT,
        }) + {
            DP_SEARCH_OP_EQ: <DataProviderExpressionInfo>{
                "type": DET_Operator,
                "name": DP_SEARCH_OP_EQ,
                "display_name": "equals (=)",
                "short_desc": "a value for equality comparisons; the type of the value should correspond to the "
                    "field type",
                "desc": "a value for equality comparisons; the type of the value should correspond to the field "
                    "type",
                "symbol": "==",
                "groups": "Comparison",
                "args": (DataProviderSignatureAnyType, DataProviderSignatureAnyType),
                "varargs": True,
                "return_type": AbstractDataProviderTypeMap."bool",
            },
        };

        #! Search options
        const SearchOptions = {
            "columns": <DataProviderOptionInfo>{
                "display_name": "Columns",
                "short_desc": "Columns to include in the output of the query",
                "type": AbstractDataProviderType::get(AbstractDataProviderType::anyType),
                "desc": "Columns to include in the output of the query; `select` argument for the query expression",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "The maximum number of records to return",
                "type": AbstractDataProviderType::get(IntType),
                "desc": "The maximum number of records to return",
            },
        };

        #! UTC time zone
        const UTC = new TimeZone("UTC");
    }

    private {
        #! The table config
        hash<MewsTableApiConfig> config;

        #! Record info for the table
        hash<string, AbstractDataField> record_type;

        #! Comparison ...Start fields
        hash<string, bool> comp_start;

        #! Comparison ...End fields
        hash<string, bool> comp_end;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options, hash<MewsTableApiConfig> config) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        self.config = config;
        rest = MewsRestDataProvider::getRestConnection(copts);
        setRecordTypeIntern();
    }

    #! Creates the object from the arguments
    constructor(MewsRestClient rest, hash<MewsTableApiConfig> config) : MewsRestDataProviderBase(rest){
        self.config = config;
        setRecordTypeIntern();
    }

    #! Returns the data provider name
    string getName() {
        return config.table;
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param where_cond the search criteria
        @param search_options the search options after processing by validateSearchOptions()

        @see requestSearchRecordsImpl()
    */
    private DataProvider::AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        AbstractDataProvider prov = sdp.getChildProviderPath(config.path);
        int limit = min(search_options.limit ?? MaxRecords, MaxRecords);
        *hash<auto> req = getSearchRequest(limit, \where_cond, search_options);
        *list<auto> l;
        while (True) {
            hash<auto> res = prov.doRequest({"body": req});
            *list<auto> bdata = res.body{config.body_key};
            debug("records retrieved: %d (total: %d limit: %d)", bdata.size(), l.size() + bdata.size(), limit);
            if (!res.body{config.body_key}) {
                break;
            }
            l += res.body{config.body_key};
            if (res.body{config.body_key}.size() < limit) {
                break;
            }

            if (search_options.limit) {
                int remaining = search_options.limit - l.size();
                if (!remaining) {
                    break;
                }
                if (remaining < limit) {
                    limit = remaining;
                }
            }
        }
        return new DefaultRecordIterator(new ListIterator(l), NOTHING, search_options, record_type);
    }

    private AbstractDataProviderBulkRecordInterface searchRecordsBulkImpl(int block_size = 1000,
            *hash<auto> where_cond, *hash<auto> search_options) {
        AbstractDataProvider prov = sdp.getChildProviderPath(config.path);
        int limit = min(block_size, search_options.limit ?? MaxRecords, MaxRecords);
        hash<auto> req = getSearchRequest(limit, \where_cond, search_options);
        return new MewsTableBulkRecordInterface(block_size, prov, req, config.body_key, record_type,
            NOTHING, search_options);
    }

    private *hash<string, DataProvider::AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return record_type;
    }

    #! Retrieves the Swagger data provider for the table data
    private AbstractDataProvider getTableDataProvider() {
        return sdp.getChildProviderPath(config.path);
    }

    #! Retrieve the record information
    private setRecordTypeIntern() {
        if (config.comparison_fields) {
            comp_start = map {$1 + "Start": True}, keys config.comparison_fields;
            comp_end = map {$1 + "End": True}, keys config.comparison_fields;
        }

        # get data provider for the call
        AbstractDataProvider prov = sdp.getChildProviderPath(config.path);

        record_type = prov.getResponseType()
            .getFields().body.getType()
            .getFields(){config.body_key}.getType()
            .getElementType().getFields();
    }

    private *hash<auto> getSearchRequest(int limit, reference<*hash<auto>> where_cond, *hash<auto> search_options) {
        hash<auto> req;
        if (!config.no_limitation) {
            req.Limitation.Count = limit;
        }
        if (where_cond) {
            processSearchArgs(\req, where_cond);
        }
        if (config.default_extent && !req.Extent) {
            req.Extent = config.default_extent;
        }
        return req;
    }

    private processSearchArgs(reference<hash<auto>> req, hash<auto> exp) {
        if (!exp.exp || !exp.args) {
            throw "EXPRESSION-ERROR", sprintf("cannot handle expression %y; expecting a data provider expression "
                "with \"exp\" and \"args\" keys", exp);
        }
        switch (exp.exp) {
            case DP_OP_AND:
                map processSearchArgs(\req, $1), exp.args;
                break;

            case DP_SEARCH_OP_GT:
            case DP_SEARCH_OP_LT:
            case DP_SEARCH_OP_GE:
            case DP_SEARCH_OP_LE:
                processComparisonArgs(\req, exp);
                break;

            case DP_SEARCH_OP_EQ:
                processEqualityArgs(\req, exp);
                break;

            default:
                throw "EXPRESSION-ERROR", sprintf("unsupported operator %y", exp.exp);
        }
    }

    #! Allows data providers to accept foreign or virtual field names
    bool searchAcceptsForeignField(string field) {
        return config.equality_list_fields{field} || config.equality_fields{field}
            || comp_start{field} || comp_end{field}
            || (field == "Extent" && config.extent);
    }

    private processEqualityArgs(reference<hash<auto>> req, hash<auto> exp) {
        auto val;
        *string field = exp.args[0].field;
        if (field) {
            val = exp.args[1];
        } else {
            field = exp.args[1].field;
            val = exp.args[0];
        }
        if (!field) {
            throw "EXPRESSION-ERROR", sprintf("expecting a field reference as one of the arguments to the %y "
                "operator; got %y instead", exp.exp, exp.args);
        }
        if (field == "Extent" && config.extent) {
            processExtent(\req, val);
            return;
        }
        if (config.equality_list_fields{field}) {
            string name = field + "s";
            softlist<softstring> l = val;
            req{name} += l;
        } else if (config.equality_fields{field}) {
            softstring v = val;
            req{field} = v;
        } else {
            # check for timestamp Start or End
            if (comp_start{field}) {
                string ts = field.substr(0, -5);
                # treat as >=
                req{ts}.StartUtc = val;
                if (!req{ts}.EndUtc) {
                    req{ts}.EndUtc = val + config.max_delta;
                }
            } else if (comp_end{field}) {
                string ts = field.substr(0, -3);
                # treat as <
                req{ts}.EndUtc = val + 1s;
                if (!req{ts}.StartUtc) {
                    req{ts}.StartUtc = val - config.max_delta;
                }
            } else {
                throw "EXPRESSION-ERROR", sprintf("field %y cannot be used with the %y operator", field, exp.exp);
            }
        }
    }

    private processExtent(reference<hash<auto>> req, auto val) {
        foreach auto v in (val) {
            if (v.typeCode() != NT_STRING) {
                throw "EXPRESSION-ERROR", sprintf("expecting a string or list of strings for the \"Extent\" value; "
                    "got type %y (value: %y) instead; valid Extents: %y", v.fullType(), v, keys config.extent);
            }
            if (!config.extent{v}) {
                throw "EXPRESSION-ERROR", sprintf("invalid Extent %y; valid Extents: %y", v, keys config.extent);
            }
            req.Extent{v} = True;
        }
    }

    private processComparisonArgs(reference<hash<auto>> req, hash<auto> exp) {
        *string field = exp.args[0].field;
        if (!field) {
            throw "EXPRESSION-ERROR", sprintf("expecting a field reference as the first argument to the %y "
                "operator; got %y instead", exp.exp, exp.args);
        }
        if (!config.comparison_fields{field}) {
            throw "EXPRESSION-ERROR", sprintf("field %y cannot be used with the %y operator; supported fields: %y",
                field, exp.exp, keys config.comparison_fields);
        }
        auto val = exp.args[1];
        if (val.typeCode() != NT_DATE) {
            val = date(val);
        }
        val = UTC.date(val);
        switch (exp.exp) {
            case DP_SEARCH_OP_GT:
                req{field}.StartUtc = val - 1s;
                if (!req{field}.EndUtc) {
                    req{field}.EndUtc = val + config.max_delta;
                }
                break;

            case DP_SEARCH_OP_GE:
                req{field}.StartUtc = val;
                if (!req{field}.EndUtc) {
                    req{field}.EndUtc = val + config.max_delta;
                }
                break;

            case DP_SEARCH_OP_LT:
                req{field}.EndUtc = val + 1s;
                if (!req{field}.StartUtc) {
                    req{field}.StartUtc = val - config.max_delta;
                }
                break;

            case DP_SEARCH_OP_LE:
                req{field}.EndUtc = val;
                if (!req{field}.StartUtc) {
                    req{field}.StartUtc = val - config.max_delta;
                }
                break;
        }
    }
}
}
