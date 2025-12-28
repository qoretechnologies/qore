# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ServiceNowRestRecordIterator class definition

/** ServiceNowRestRecordIterator.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the ServiceNowRestDataProvider module
public namespace ServiceNowRestDataProvider {
#! Defines the record iterator class for Table-based iterators
public class ServiceNowRestRecordIterator inherits AbstractDataProviderRecordIterator {
    public {
    }

    private:internal {
        #! The REST client object for API calls
        ServiceNowRestClient::ServiceNowRestClient rest;

        #! current object name
        string name;

        #! Record info for the entity
        hash<ServiceNowRestRecordInfo> record_info;

        #! record iterator
        Qore::ListHashIterator i;
    }

    #! creates the iterator
    /** @param rest the REST client connection
        @param name the name of the entity
        @param record_info record information describing the entity
        @param where_cond the where clause for the query, if any
        @param search_options search options; assumed to have already been processed for validity before this call
    */
    constructor(ServiceNowRestClient::ServiceNowRestClient rest, string name,
            hash<ServiceNowRestRecordInfo> record_info, *hash<auto> where_cond, *hash<auto> search_options)
            : AbstractDataProviderRecordIterator(search_options.requires_result, search_options.max_records) {
        self.rest = rest;
        self.name = name;
        self.record_info = record_info;

        # make query
        *string query = makeQuery(cast<*hash<DataProviderExpression>>(where_cond), search_options);
        hash<auto> info;
        try {
            string uri = "table/" + name;
            if (query) {
                uri += sprintf("?%s", query);
            }

            i = new ListHashIterator(rest.get(uri, NOTHING, \info).body.response.result);
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = {
                "query": query,
            } + info{"response-code", "response-body"};
            throw ex.err, ex.desc, ex_arg;
        }
    }

    #! Returns @ref True if the iterator is valid
    /**
        @return @ref True if the iterator is valid
    */
    bool valid() {
        return i.valid();
    }

    #! Returns a single record if the iterator is valid
    /** @throw INVALID-ITERATOR the iterator is not pointing at a valid element
    */
    hash<auto> getValue() {
        # value may be NOTHING in case referenced columns do not exist
        hash<auto> rv = i.getValue() ?? {};
        # convert JSON strings to native Qore date/time values
        map rv{$1} = date(rv{$1}), keys record_info.date_time_fields, rv{$1}.val();
        # convert JSON strings to native boolean values
        map rv{$1} = rv{$1} == "true" ? True : False, keys record_info.bool_fields, rv{$1}.val();
        # convert JSON strings to native int values
        map rv{$1} = rv{$1}.toInt(), keys record_info.int_fields, rv{$1}.val();
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

    /*
    #! Returns "or" clauses
    string getOrClause(list<auto> arglist) {
        list<auto> l = map $1, (map doWhereExpression($1), arglist), $1;
        return l ? (foldl $1 + "^OR" + $2, l) : "";
    }
    */

    #! Returns the filter for a request
    *string makeQuery(*hash<DataProviderExpression> where_cond, *hash<auto> search_options) {
        string filter = "sysparm_exclude_reference_link=true&sysparm_suppress_pagination_header=true";
        if (search_options.columns) {
            filter += "&sysparm_fields=" + getColumnNames(search_options.columns);
        }
        if (where_cond) {
            if (filter) {
                filter += "&";
            }
            filter += "sysparm_query=" + doWhereExpression(where_cond, search_options);
        }
        if (search_options.orderby) {
            if (filter) {
                if (where_cond) {
                    filter += "^";
                } else {
                    filter += "&sysparm_query=";
                }
            } else {
                filter += "sysparm_query=";
            }
            filter += "ORDERBY" + getOrderBy(search_options.orderby);
        }
        if (search_options.orderbydesc) {
            if (filter) {
                if (where_cond || search_options.orderby) {
                    filter += "^";
                } else {
                    filter += "&sysparm_query=";
                }
            } else {
                filter += "sysparm_query=";
            }
            filter += "ORDERBYDESC" + getOrderBy(search_options.orderbydesc);
        }
        if (search_options.groupby) {
            if (filter) {
                if (where_cond || search_options.orderby || search_options.orderbydesc) {
                    filter += "^";
                } else {
                    filter += "&sysparm_query=";
                }
            } else {
                filter += "sysparm_query=";
            }
            filter += "GROUPBY" + getColumnNames(search_options.groupby);
        }
        if (search_options.limit) {
            if (filter) {
                filter += "&";
            }
            filter += sprintf("sysparm_limit=%d", search_options.limit);
        }
        if (search_options.offset) {
            if (filter) {
                filter += "&";
            }
            filter += sprintf("sysparm_offset=%d", search_options.offset);
        }

        return filter;
    }

    #! Returns the record description, if available
    /** @return the record type of the iterator
    */
    *hash<string, DataProvider::AbstractDataField> getRecordType() {
        return record_info.record_type;
    }

    private string getOrderBy(softlist<string> coll) {
        return foldl $1 + "," + $2, (map $1, coll, checkColumnName($1));
    }

    private bool checkColumnName(string col) {
        if (!record_info.record_type{col}) {
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
        } else {
            return foldl $1 + "," + $2, keys record_info.record_type;
        }
    }

    private string doWhereExpression(hash<DataProviderExpression> where_cond, *hash<auto> search_options) {
        return processExpressionArg(where_cond);
    }

    #! Processes a DataProvider expression argument and returns the ServiceNow query string
    /** @param exp the expression to process
        @return the ServiceNow query string for the expression
    */
    string processExpressionArg(hash<DataProviderExpression> exp) {
        *hash<auto> expinfo = ServiceNowTableDataProvider::Expressions{exp.exp};
        if (!expinfo) {
            throw "WHERE-ERROR", sprintf("expression references unknown operator %y; known operators: %y",
                exp.exp, keys ServiceNowTableDataProvider::Expressions);
        }

        # Handle logical operators (AND, OR) - process recursively
        if (exp.exp == DP_OP_AND) {
            if (!exists exp.args || !exp.args.size()) {
                # Empty AND is always true - return a tautology
                return "sys_id!=NULL";
            }
            list<string> clauses = map processExpressionArg($1), exp.args;
            return foldl $1 + "^" + $2, clauses;
        }
        if (exp.exp == DP_OP_OR) {
            if (!exists exp.args || !exp.args.size()) {
                # Empty OR is always false - return a contradiction
                return "sys_id=NULL";
            }
            list<string> clauses = map processExpressionArg($1), exp.args;
            return foldl $1 + "^OR" + $2, clauses;
        }

        # For comparison operators, first argument must be a field reference
        if (exp.args.size() < 1 || !(exp.args[0] instanceof hash<DataProviderFieldReference>)) {
            throw "WHERE-ERROR", sprintf("the first argument to expression %y must be a field reference",
                exp.exp);
        }
        string column = cast<hash<DataProviderFieldReference>>(exp.args[0]).field;

        # Build args for the impl function
        list<auto> args = (column,);
        for (int j = 1; j < exp.args.size(); ++j) {
            # If the argument is a list, flatten it for IN/NOTIN operators
            if (exp.args[j].typeCode() == NT_LIST) {
                args += map getArgValue(column, $1), exp.args[j];
            } else {
                push args, getArgValue(column, exp.args[j]);
            }
        }

        return call_function_args(expinfo.impl, args);
    }

    private string doWhereExpression(auto arg) {
        throw "WHERE-ERROR", sprintf("argument for where cond has type %y (%y); expecting an operator expression",
            arg.fullType(), arg);
    }

    private string getArgValue(string key, auto value) {
        switch (record_info.field_types{key}) {
            case "integer":
                return sprintf("%d", value);

            case "double":
                return sprintf("%g", value);

            case "boolean":
                return value ? "true" : "false";

            case "glide_date_time":
            case "glide_date":
                if (value.typeCode() == NT_DATE) {
                    return value.format("YYYY-MM-DD HH:mm:SS.xx");
                }
                # fall through

            default:
                break;
        }

        return value.toString();
    }

    #! Increments the row pointer when retrieving rows from a select statement
    /**
        @return @ref True if there is a row to retrieve, @ref False if not (no more rows to be retrieved)

        @note Exceptions could be thrown by the DBI driver when the statement is executed; see the relevant DBI driver
        docs for more information
    */
    private bool nextImpl() {
        return i.next();
    }
}
}
