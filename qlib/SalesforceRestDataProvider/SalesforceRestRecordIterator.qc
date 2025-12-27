# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SalesforceRestRecordIterator class definition

/** SalesforceRestRecordIterator.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SalesforceRestDataProvider module
public namespace SalesforceRestDataProvider {
#! Defines the record iterator class for Table-based iterators
public class SalesforceRestRecordIterator inherits AbstractDataProviderRecordIterator {
    public {
    }

    private:internal {
        #! The REST client object for API calls
        SalesforceRestClient rest;

        #! current object name
        string name;

        #! The record info for this object
        hash<SalesforceRestRecordInfo> record_info;

        #! record iterator
        ListHashIterator i;
    }

    #! creates the iterator
    /** @param rest the REST client connection to the server
        @param name the name of the object
        @param record_info info about the current object
        @param where_cond the search conditions
        @param search_options search options; assumed to have already been processed for validity before this call
    */
    constructor(SalesforceRestClient rest, string name, hash<SalesforceRestRecordInfo> record_info,
            *hash<auto> where_cond, *hash<auto> search_options)
            : AbstractDataProviderRecordIterator(search_options.requires_result, search_options.max_records) {
        self.rest = rest;
        self.name = name;
        self.record_info = record_info;

        # make query
        string soql = makeQuery(where_cond, search_options);
        hash<auto> info;
        try {
            i = new ListHashIterator(rest.get("query?q=" + soql, NOTHING, \info).body.records);
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = {
                "soql": soql,
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
        hash<auto> rv =  i.getValue() - "attributes";
        # convert JSON strings to native Qore date/time values
        map rv{$1} = date(rv{$1}), keys record_info.date_time_fields;
        map rv{$1} = date(rv{$1}), keys record_info.date_fields;
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

    #! Returns "or" clauses
    string getOrClause(list<auto> arglist) {
        list<auto> l = map $1, (map doWhereExpression($1), arglist), $1;
        return l ? ("(" + (foldl $1 + " or " + $2, l) + ")") : "";
    }

    #! Returns the SQL for a request
    string makeQuery(*hash<auto> where_cond, *hash<auto> search_options) {
        string soql = sprintf("select %s from %s", getColumnNames(search_options.columns), name);
        if (where_cond) {
            soql += " where " + doWhereExpression(where_cond, search_options);
        }
        if (search_options.scope) {
            soql += " using scope " + search_options.scope;
        }
        if (search_options.groupby) {
            soql += " group by " + getColumnNames(search_options.groupby);
        }
        if (search_options.having) {
            soql += " having " + doWhereExpression(search_options.having);
        }
        if (search_options.orderby) {
            soql += " order by " + getOrderBy(search_options.orderby);
            if (search_options.orderby_desc) {
                soql += " desc";
            }
        }
        if (search_options.limit) {
            soql += sprintf(" limit %d", search_options.limit);
        }
        if (search_options.hasKey("offset")) {
            if (!search_options.limit)
                throw "SELECT-ERROR", sprintf("select 'offset' supplied (%d) without limit", search_options.offset);
            soql += sprintf(" offset %d", search_options.offset);
        }
        if (search_options.forview) {
            soql += " for view";
        }
        if (search_options.forreference) {
            soql += " for reference";
        }

        return soql;
    }

    #! Returns the record description, if available
    /** @return the record type of the iterator
    */
    *hash<string, AbstractDataField> getRecordType() {
        return record_info.record_type;
    }

    string getArgValue(string key, auto value) {
        switch (record_info.field_types{key}) {
            case "datetime":
            case "date":
            case "time":
                return doDateTimeValue(key, value);

            case "int":
                return sprintf("%d", value);

            case "double":
                return sprintf("%g", value);

            case "boolean":
                return value ? "true" : "false";

            default:
                break;
        }

        return sprintf("'%s'", value);
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

                    case NT_HASH:
                        if (!col.cop) {
                            throw "COLUMN-ERROR", sprintf("missing column operator in \"columns\" hash element "
                                "%d/%d; the \"columns\" select option must be assigned to a list of strings giving "
                                "the column names or a list of column operator hashes as returned from column "
                                "operator functions; this element is missing hash key \"cop\" giving the column "
                                "operator; keys provided: %y", $# + 1, column_names.size(), col.keys());
                        }
                        try {
                            string val = doColumnOperatorIntern(col.cop, col.arg, col.column);
                            exp_list += val;
                        } catch (hash<ExceptionInfo> ex) {
                            # rethrow exception with additional contextual information
                            throw ex.err, sprintf("%s: in \"columns\" hash element %d/%d: %s", get_ex_pos(ex), $# + 1,
                                column_names.size(), ex.desc), ex.arg;
                        }
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

    private string doColumnOperatorIntern(auto cop, auto arg, auto cve) {
        if (cve) {
            cve = getColumnExpressionIntern(cve);
        }

        if (cop.typeCode() != NT_STRING) {
            throw "SELECT-ERROR", sprintf("invalid column operator code %y; use column operator functions to "
                "create valid column specifications with supported column operators", cop);
        }

        *hash<auto> cmd = DefaultSoqlCopMap{cop};
        if (!cmd) {
            throw "SELECT-ERROR", sprintf("unknown column operator %y: expecting one of: %y", cop,
                keys DefaultSoqlCopMap);
        }

        if (cmd.arg) {
            string at = arg.type();
            switch (cmd.arg) {
                default: {
                    if (at != cmd.arg)
                        throw "SELECT-ERROR", sprintf("invalid argument to column operator %y; got type %y, "
                            "expecting %y", cop, at, cmd.arg);
                }
            }
        }

        if (cmd.dostring) {
            remove record_info.date_time_fields;
            remove record_info.date_fields;
        }

        if (!exists cve && !cmd.nocolumn && !cmd.columnargs) {
            throw "SELECT-ERROR", sprintf("column operator %y requires a column argument name but none was provided",
                cop);
        }

        return cmd.code(cve, arg);
    }

    private string getColumnExpressionIntern(auto cvc) {
        switch (cvc.typeCode()) {
            case NT_HASH:
                return doColumnOperatorIntern(cvc.cop, cvc.arg, cvc.column);
            case NT_STRING:
                return cvc;
        }
        throw "SELECT-ERROR", sprintf("column operator hash %y has an invalid \"column\" key; expecting a string "
            "column specification or a column operator description hash", cvc);
    }

    private string doWhereExpression(hash<auto> where_cond, *hash<auto> search_options) {
        # Check if this is a DataProvider expression
        if (where_cond.exp) {
            return processExpressionArg(cast<hash<DataProviderExpression>>(where_cond));
        }

        list<string> clauses;
        foreach hash<auto> i in (where_cond.pairIterator()) {
            clauses += doWhereExpressionIntern(i.key, i.value);
        }
        return foldl $1 + " and " + $2, clauses;
    }

    #! Processes a DataProvider expression argument and returns the SOQL string
    /** @param exp the expression to process
        @return the SOQL string for the expression
    */
    string processExpressionArg(hash<DataProviderExpression> exp) {
        *hash<auto> expinfo = SoqlExpressions{exp.exp};
        if (!expinfo) {
            throw "WHERE-ERROR", sprintf("expression references unknown operator %y; known operators: %y",
                exp.exp, keys SoqlExpressions);
        }

        # Handle logical operators (AND, OR) - pass all args to impl
        if (exp.exp == DP_OP_AND || exp.exp == DP_OP_OR) {
            return call_function_args(expinfo.impl, (self, exp.args));
        }

        # For comparison operators, first argument must be a field reference
        if (exp.args.size() < 1 || !(exp.args[0] instanceof hash<DataProviderFieldReference>)) {
            throw "WHERE-ERROR", sprintf("the first argument to expression %y must be a field reference",
                exp.exp);
        }
        string field = cast<hash<DataProviderFieldReference>>(exp.args[0]).field;

        # For IN, INCLUDES, EXCLUDES operators, collect all values into a single list
        if (exp.exp == DP_SEARCH_OP_IN || exp.exp == SOQL_OP_INCLUDES || exp.exp == SOQL_OP_EXCLUDES) {
            list<auto> values = ();
            for (int j = 1; j < exp.args.size(); ++j) {
                values += exp.args[j];
            }
            return call_function_args(expinfo.impl, (self, field, values));
        }

        # Build args list: (self, field, value1, value2, ...)
        list<auto> impl_args = (self, field);
        for (int j = 1; j < exp.args.size(); ++j) {
            push impl_args, exp.args[j];
        }

        return call_function_args(expinfo.impl, impl_args);
    }

    private string doWhereExpressionIntern(string key, auto value) {
        if (value.typeCode() == NT_HASH && value.op) {
            *hash<auto> op = DefaultSoqlOpMap{value.op};
            if (!op) {
                throw "WHERE-ERROR", sprintf("operator hash for where cond key %y has unknown operator "
                    "%y: expecting one of: %y (operator hash: %y)", key, value.op, keys DefaultSoqlOpMap,
                    value);
            }
            string column_name;
            if (op.recursive) {
                column_name = doWhereExpressionIntern(key, value.arg);
            } else {
                column_name = key;
            }
            if (!op.no_process_arg) {
                value.arg = getArgValue(key, value.arg);
            }
            return op.code(self, column_name, value.arg);
        }

        return sprintf("%s = %s", key, getArgValue(key, value));
    }

    private string doDateTimeValue(string key, auto value) {
        if (value.typeCode() == NT_STRING) {
            if (record_info.time_fields{key} && value =~ /^[0-9]+:/) {
                # add the start of the UNIX epoch to the time to parse it to a date value
                value = "1970-01-01T" + value;
            }
            value = date(value);
        } else if (value.typeCode() != NT_DATE) {
            throw "WHERE-ERROR", sprintf("cannot convert value of type %y to a date for field %y", value.type(), key);
        }

        if (record_info.date_time_fields{key}) {
            return value.format("YYYY-MM-DDTHH:mm:SSZ");
        }

        if (record_info.date_fields{key}) {
            return value.format("YYYY-MM-DD");
        }

        if (record_info.time_fields{key}) {
            return value.format("HH:mm:SS.msZ");
        }

        return sprintf("'%s'", value.format("YYYY-MM-DDTHH:mm:SS.xxZ"));
    }

    #! Increments the row pointer when retrieving rows from a select statement; returns @ref True if there is a row to retrieve, @ref False if not
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
