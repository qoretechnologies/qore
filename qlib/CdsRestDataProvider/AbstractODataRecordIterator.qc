# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractODataRecordIterator class definition

/** AbstractODataRecordIterator.qc Copyright 2023 - 2025 Qore Technologies, s.r.o.

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

#! contains all public definitions in the CdsRestDataProvider module
public namespace CdsRestDataProvider {
#! Abstract base class for OData record iterators
/** Provides common OData query building logic used by both CDS and Business Central
*/
public class AbstractODataRecordIterator inherits AbstractDataProviderRecordIterator {
    private {
        #! The REST client object for API calls
        CdsRestClient::CdsRestClient rest;

        #! Record info for the entity
        hash<ODataRecordInfo> record_info;

        #! Record iterator
        Qore::ListHashIterator i;

        #! Next page URL for pagination (used by BC)
        *string nextLink;
    }

    #! Creates the iterator
    /** @param rest the REST client connection
        @param record_info record information describing the entity
        @param search_options search options; assumed to have already been processed for validity before this call
    */
    constructor(CdsRestClient::CdsRestClient rest, hash<ODataRecordInfo> record_info, *hash<auto> search_options)
            : AbstractDataProviderRecordIterator(search_options.requires_result, search_options.max_records) {
        self.rest = rest;
        self.record_info = record_info;
    }

    #! Returns @ref True if the iterator is valid
    bool valid() {
        return i.valid();
    }

    #! Returns a single record if the iterator is valid
    /** @throw INVALID-ITERATOR the iterator is not pointing at a valid element
    */
    hash<auto> getValue() {
        hash<auto> rv = i.getValue();
        # convert JSON strings to native Qore date/time values
        map rv{$1} = date(rv{$1}), keys record_info.date_time_fields, rv{$1}.val();
        return rv;
    }

    #! Returns the value of the given field in the current row, if the iterator is valid
    /** @param key the name of the field

        @return the value of the given field in the current row, if the iterator is valid

        @throw FIELD-ERROR invalid or unknown field name
    */
    auto memberGate(string key) {
        hash<auto> row = getValue();
        auto rv = row{key};
        if (!exists rv && !row.hasKey(key)) {
            throw "FIELD-ERROR", sprintf("the current record does not have field %y; valid fields: %y", key,
                keys row);
        }
        return rv;
    }

    #! Returns "or" clauses (legacy support)
    string getOrClause(list<auto> arglist) {
        list<auto> l = map $1, (map doWhereExpressionLegacy($1), arglist), $1;
        return l ? ("(" + (foldl $1 + " or " + $2, l) + ")") : "";
    }

    #! Returns the filter for a request
    /** @param where_cond the where condition (can be legacy hash format or DataProviderExpression)
        @param search_options search options
        @return the query string for the OData request
    */
    *string makeQuery(*hash<auto> where_cond, *hash<auto> search_options) {
        string filter;
        if (search_options.columns) {
            filter = "$select=" + getColumnNames(search_options.columns);
        }
        if (where_cond) {
            if (filter) {
                filter += "&";
            }
            # Check if this is a DataProviderExpression or legacy format
            if (where_cond.exp) {
                filter += "$filter=" + processExpressionArg(cast<hash<DataProviderExpression>>(where_cond));
            } else {
                # Legacy hash format
                filter += "$filter=" + doWhereExpressionLegacy(where_cond, search_options);
            }
        }
        if (search_options.groupby) {
            if (filter) {
                filter += "&";
            }
            filter += "$groupby=" + getColumnNames(search_options.groupby);
        }
        if (search_options.orderby) {
            if (filter) {
                filter += "&";
            }
            filter += "$orderby=" + getOrderBy(search_options.orderby);
            if (search_options.orderby_desc) {
                filter += " desc";
            }
        }
        if (search_options.limit) {
            if (filter) {
                filter += "&";
            }
            filter += sprintf("$top=%d", search_options.limit);
        }

        return filter;
    }

    #! Processes an expression argument recursively
    /** @param exp the DataProviderExpression to process
        @return the OData filter string for the expression
    */
    string processExpressionArg(hash<DataProviderExpression> exp) {
        *hash<auto> expinfo = ODataExpressions{exp.exp};
        if (!expinfo) {
            throw "WHERE-ERROR", sprintf("expression references unknown operator %y; known operators: %y",
                exp.exp, keys ODataExpressions);
        }

        # Handle logical operators (AND, OR) - pass all args to impl
        if (exp.exp == DP_OP_AND || exp.exp == DP_OP_OR) {
            return call_function_args(expinfo.impl, (self, exp.args));
        }

        # Handle NOT operator - pass the inner expression
        if (exp.exp == DP_SEARCH_OP_NOT) {
            if (exp.args.size() < 1 || !(exp.args[0] instanceof hash<DataProviderExpression>)) {
                throw "WHERE-ERROR", "NOT operator requires a single expression argument";
            }
            return call_function_args(expinfo.impl, (self, exp.args[0]));
        }

        # For comparison operators, first argument must be a field reference
        if (exp.args.size() < 1 || !(exp.args[0] instanceof hash<DataProviderFieldReference>)) {
            throw "WHERE-ERROR", sprintf("the first argument to expression %y must be a field reference",
                exp.exp);
        }
        string field = cast<hash<DataProviderFieldReference>>(exp.args[0]).field;

        # Build args list: (self, field, value1, value2, ...)
        list<auto> impl_args = (self, field);
        for (int i = 1; i < exp.args.size(); ++i) {
            impl_args += exp.args[i];
        }

        return call_function_args(expinfo.impl, impl_args);
    }

    #! Formats a value for use in OData filter expressions
    /** @param field the field name (used to look up type information)
        @param value the value to format
        @return the OData-formatted value string
    */
    string formatODataValue(string field, auto value) {
        switch (record_info.field_types{field}) {
            case "int":
            case "integer":
                return sprintf("%d", value);

            case "double":
            case "decimal":
                return sprintf("%g", value);

            case "bool":
            case "boolean":
                return value ? "true" : "false";

            case "guid":
                # GUIDs don't need quotes in OData
                return value.toString();

            case "datetime":
            case "date":
                # ISO 8601 format for dates
                if (value.typeCode() == NT_DATE) {
                    return value.format("YYYY-MM-DDTHH:mm:SS");
                }
                return sprintf("'%s'", value);

            default:
                break;
        }

        # Strings need single quotes, escape internal quotes
        return sprintf("'%s'", replace(value.toString(), "'", "''"));
    }

    #! Returns the record description, if available
    /** @return the record type of the iterator
    */
    *hash<string, DataProvider::AbstractDataField> getRecordType() {
        return record_info.record_type;
    }

    #! Formats a value for use in OData query
    string getArgValue(string key, auto value) {
        switch (record_info.field_types{key}) {
            case "int":
                return sprintf("%d", value);

            case "double":
            case "decimal":
                return sprintf("%g", value);

            case "bool":
                return value ? "true" : "false";

            case "guid":
                return value;

            default:
                break;
        }

        return sprintf("'%s'", value);
    }

    private string getOrderBy(softlist<string> coll) {
        return foldl $1 + "," + $2, (map $1, coll, checkColumnName($1));
    }

    private bool checkColumnName(string col) {
        if (record_info.record_type && !record_info.record_type{col}) {
            throw "COLUMN-ERROR", sprintf("column %y is unknown; known columns: %y", col,
                keys record_info.record_type);
        }
        return True;
    }

    private string getColumnNames(*softlist<auto> column_names) {
        if (column_names) {
            list<string> exp_list;
            foreach auto col in (column_names) {
                switch (col.typeCode()) {
                    case NT_STRING:
                        exp_list += col;
                        break;

                    default:
                        throw "COLUMN-ERROR", sprintf("cannot process column value of type %y (%y)", col.type(), col);
                }
            }

            return foldl $1 + "," + $2, exp_list;
        } else if (record_info.record_type) {
            return foldl $1 + "," + $2, keys record_info.record_type;
        }
        return "*";
    }

    #! Legacy where expression handler for backward compatibility
    private string doWhereExpressionLegacy(hash<auto> where_cond, *hash<auto> search_options) {
        list<string> clauses;
        foreach hash<auto> i in (where_cond.pairIterator()) {
            clauses += doWhereExpressionInternLegacy(i.key, i.value);
        }
        return foldl $1 + " and " + $2, clauses;
    }

    #! Legacy where expression handler for backward compatibility
    private string doWhereExpressionInternLegacy(string key, auto value) {
        if (value.typeCode() == NT_HASH && value.op) {
            *hash<auto> op = DefaultQueryOpMap{value.op};
            if (!op) {
                throw "WHERE-ERROR", sprintf("operator hash for where cond key %y has unknown operator "
                    "%y: expecting one of: %y (operator hash: %y)", key, value.op, keys DefaultQueryOpMap,
                    value);
            }
            string column_name;
            if (op.recursive) {
                column_name = doWhereExpressionInternLegacy(key, value.arg);
            } else {
                column_name = key;
            }
            if (!op.no_process_arg) {
                value.arg = getArgValue(key, value.arg);
            }
            return op.code(self, column_name, value.arg);
        }

        return sprintf("%s eq %s", key, getArgValue(key, value));
    }

    #! Increments the row pointer when retrieving rows from a select statement
    /** Subclasses can override to add pagination support
        @return @ref True if there is a row to retrieve, @ref False if not
    */
    private bool nextImpl() {
        return i.next();
    }
}
}
