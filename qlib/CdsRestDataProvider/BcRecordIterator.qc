# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BcRecordIterator class definition

/** BcRecordIterator.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! contains Business Central object record information
hashdecl BcRecordInfo {
    #! The record type for the object
    hash<string, AbstractDataField> record_type;

    #! The original field type names
    hash<string, string> field_types;

    #! Hash of date/time fields for this record
    hash<string, bool> date_time_fields;

    #! Key field name (usually "id" for BC)
    string key = "id";

    #! Entity name
    string entity_name;
}

#! Defines the record iterator class for Business Central entity iterators
public class BcRecordIterator inherits AbstractDataProviderRecordIterator {
    public {
    }

    private:internal {
        #! The REST client object for API calls
        CdsRestClient::BusinessCentralRestClient rest;

        #! The company ID
        string companyId;

        #! Current entity name
        string name;

        #! Record info for the entity
        hash<BcRecordInfo> record_info;

        #! Record iterator
        Qore::ListHashIterator i;

        #! Next page URL for pagination
        *string nextLink;
    }

    #! creates the iterator
    /** @param rest the REST client connection
        @param companyId the company ID
        @param name the name of the entity
        @param record_info record information describing the entity
        @param where_cond the where clause for the query, if any
        @param search_options search options; assumed to have already been processed for validity before this call
    */
    constructor(CdsRestClient::BusinessCentralRestClient rest, string companyId, string name,
            hash<BcRecordInfo> record_info, *hash<auto> where_cond, *hash<auto> search_options)
            : AbstractDataProviderRecordIterator(search_options.requires_result, search_options.max_records) {
        self.rest = rest;
        self.companyId = companyId;
        self.name = name;
        self.record_info = record_info;

        # make query
        *string query = makeQuery(where_cond, search_options);
        hash<auto> info;
        try {
            string uri = sprintf("companies(%s)/%s", companyId, name);
            if (query) {
                uri += sprintf("?%s", query);
            }
            hash<auto> resp = rest.get(uri, NOTHING, \info);
            i = new ListHashIterator(resp.body.value ?? ());
            # Save next link for pagination
            nextLink = resp.body."@odata.nextLink";
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

    #! Returns "or" clauses
    string getOrClause(list<auto> arglist) {
        list<auto> l = map $1, (map doWhereExpression($1), arglist), $1;
        return l ? ("(" + (foldl $1 + " or " + $2, l) + ")") : "";
    }

    #! Returns the filter for a request
    *string makeQuery(*hash<auto> where_cond, *hash<auto> search_options) {
        string filter;
        if (search_options.columns) {
            filter = "$select=" + getColumnNames(search_options.columns);
        }
        if (where_cond) {
            if (filter) {
                filter += "&";
            }
            filter += "$filter=" + doWhereExpression(where_cond, search_options);
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

    #! Returns the record description, if available
    /** @return the record type of the iterator
    */
    *hash<string, DataProvider::AbstractDataField> getRecordType() {
        return record_info.record_type;
    }

    string getArgValue(string key, auto value) {
        switch (record_info.field_types{key}) {
            case "int":
                return sprintf("%d", value);

            case "double":
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

    private string doWhereExpression(hash<auto> where_cond, *hash<auto> search_options) {
        list<string> clauses;
        foreach hash<auto> i in (where_cond.pairIterator()) {
            clauses += doWhereExpressionIntern(i.key, i.value);
        }
        return foldl $1 + " and " + $2, clauses;
    }

    private string doWhereExpressionIntern(string key, auto value) {
        if (value.typeCode() == NT_HASH && value.op) {
            *hash<auto> op = DefaultQueryOpMap{value.op};
            if (!op) {
                throw "WHERE-ERROR", sprintf("operator hash for where cond key %y has unknown operator "
                    "%y: expecting one of: %y (operator hash: %y)", key, value.op, keys DefaultQueryOpMap,
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

        return sprintf("%s eq %s", key, getArgValue(key, value));
    }

    #! Increments the row pointer when retrieving rows from a select statement
    /**
        @return @ref True if there is a row to retrieve, @ref False if not (no more rows to be retrieved)
    */
    private bool nextImpl() {
        if (i.next()) {
            return True;
        }

        # Check for next page
        if (nextLink) {
            hash<auto> info;
            try {
                hash<auto> resp = rest.get(nextLink, NOTHING, \info);
                i = new ListHashIterator(resp.body.value ?? ());
                nextLink = resp.body."@odata.nextLink";
                return i.next();
            } catch (hash<ExceptionInfo> ex) {
                # Handle pagination errors gracefully
                nextLink = NOTHING;
            }
        }

        return False;
    }
}
}
