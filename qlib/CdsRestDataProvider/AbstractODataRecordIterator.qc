# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractODataRecordIterator class definition

/** AbstractODataRecordIterator.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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
public class AbstractODataRecordIterator inherits AbstractDataProviderRecordIterator<hash<auto>> {
    private {
        #! The REST client object for API calls
        CdsRestClient::CdsRestClientIo rest;

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
    constructor(CdsRestClient::CdsRestClientIo rest, hash<ODataRecordInfo> record_info, *hash<auto> search_options)
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
        @return the query string for the OData request, with its options as they are written in OData, not encoded
        for a URL; see @ref makeQueryOptions() and @ref encodeQueryOptions() for the query of a request

        @throw VALUE-ERROR a value cannot be written as an OData literal: a string whose encoding is invalid or that
        has a NUL character, or a value that the field's type has no literal for
        @throw WHERE-ERROR a field cannot be named in OData
        @throw COLUMN-ERROR a column cannot be named in OData
    */
    *string makeQuery(*hash<auto> where_cond, *hash<auto> search_options) {
        return AbstractODataRecordIterator::joinQueryOptions(makeQueryOptions(where_cond, search_options));
    }

    #! Returns the query for the given query options as they are written in OData, not encoded for a URL
    /** @param opts the query options and their values

        @return the query, or @ref nothing if there are no options

        @since CdsRestDataProvider 2.2
    */
    static *string joinQueryOptions(hash<string, string> opts) {
        if (opts) {
            return (map $1.key + "=" + $1.value, opts.pairIterator()).join("&");
        }
    }

    #! Returns the query options for a request, as they are written in OData
    /** Each value is the value of one query option; @ref encodeQueryOptions() writes them as the query of a request

        @param where_cond the where condition (can be legacy hash format or DataProviderExpression)
        @param search_options search options

        @return the query options (\c "$select", \c "$filter", \c "$groupby", \c "$orderby", \c "$top") and their
        values, as they are written in OData

        @throw VALUE-ERROR a value cannot be written as an OData literal: a string whose encoding is invalid or that
        has a NUL character, or a value that the field's type has no literal for
        @throw WHERE-ERROR a field cannot be named in OData
        @throw COLUMN-ERROR a column cannot be named in OData

        @since CdsRestDataProvider 2.2
    */
    hash<string, string> makeQueryOptions(*hash<auto> where_cond, *hash<auto> search_options) {
        hash<string, string> rv = {};
        if (search_options.columns) {
            rv."$select" = getColumnNames(search_options.columns);
        }
        if (where_cond) {
            # Check if this is a DataProviderExpression or legacy format
            if (where_cond.exp) {
                rv."$filter" = processExpressionArg(cast<hash<DataProviderExpression>>(where_cond));
            } else {
                # Legacy hash format
                rv."$filter" = doWhereExpressionLegacy(where_cond, search_options);
            }
        }
        if (search_options.groupby) {
            rv."$groupby" = getColumnNames(search_options.groupby);
        }
        if (search_options.orderby) {
            rv."$orderby" = getOrderBy(search_options.orderby);
            if (search_options.orderby_desc) {
                rv."$orderby" += " desc";
            }
        }
        if (search_options.limit) {
            rv."$top" = sprintf("%d", search_options.limit);
        }
        return rv;
    }

    #! Returns the query of a request for the given query options
    /** The value of each option is percent-encoded with
        @ref CdsRestDataProvider::CdsRestDataProviderBase::encodeQueryValue() "CdsRestDataProviderBase::encodeQueryValue()",
        so that no character of a value (such as \c "&" in a string literal) ends the option or starts another one

        @param opts the query options and their values, as they are written in OData

        @return the query, without the leading \c "?"

        @par Example:
        @code{.py}
string query = AbstractODataRecordIterator::encodeQueryOptions({"$filter": "name eq 'A&B'"});
# query: "$filter=name%20eq%20'A%26B'"
        @endcode

        @since CdsRestDataProvider 2.2
    */
    static string encodeQueryOptions(hash<string, string> opts) {
        return (map $1.key + "=" + CdsRestDataProviderBase::encodeQueryValue($1.value), opts.pairIterator())
            .join("&");
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
        hash<DataProviderFieldReference> field_ref = cast<hash<DataProviderFieldReference>>(exp.args[0]);
        # the field is a field path, written as an OData property path
        string field = AbstractODataRecordIterator::getPropertyPath(field_ref.field, field_ref.field_path);

        # Build args list: (self, field, value1, value2, ...)
        list<auto> impl_args = (self, field);
        for (int i = 1; i < exp.args.size(); ++i) {
            # a list argument (the values of "in") is one argument
            push impl_args, exp.args[i];
        }

        return call_function_args(expinfo.impl, impl_args);
    }

    #! Formats a value for use in OData filter expressions
    /** @param field the field name (used to look up type information)
        @param value the value to format
        @return the OData-formatted value string
    */
    string formatODataValue(string field, auto value) {
        return AbstractODataRecordIterator::getODataLiteral(value, record_info.field_types{field});
    }

    #! Returns the OData literal of a value
    /** The literal is chosen by the type of the field the value is compared with, if known, and otherwise by the
        type of the value:
        - a string is in single quotes, with each single quote doubled; a number compared with a string field is
          given as a string, and another value cannot be compared with a string field
        - an integer, a floating-point number or an arbitrary-precision number is a number (\c NaN, \c INF and
          \c -INF for the special floating-point values)
        - a boolean is \c true or \c false
        - a GUID is written without quotes, and a string given for a GUID field must be a GUID
        - a date is a \c DateTimeOffset in UTC (<tt>2024-01-02T03:04:05Z</tt>), and a string given for a date field
          must be an ISO-8601 date or date and time
        - a binary is <tt>binary'</tt><em>base64url</em><tt>'</tt>
        - @ref nothing and @ref null are \c null
        - a field reference is the OData property path of the field (see @ref getPropertyPath())

        @param value the value
        @param field_type the type of the field the value is compared with, if known: \c "string", \c "int",
        \c "integer", \c "float", \c "double", \c "decimal", \c "number", \c "bool", \c "boolean", \c "guid",
        \c "date", \c "datetime" or \c "binary"; any other type is ignored

        @return the OData literal of the value

        @throw VALUE-ERROR a string has an invalid encoding or a NUL character; the value has no literal of the
        field's type, or no OData literal at all (a relative date, a list, a hash or an object)
        @throw WHERE-ERROR a field reference names a field that OData cannot name

        @par Example:
        @code{.py}
string literal = AbstractODataRecordIterator::getODataLiteral("O'Brien");
# literal: "'O''Brien'"
        @endcode

        @since CdsRestDataProvider 2.2
    */
    static string getODataLiteral(auto value, *string field_type) {
        switch (value.typeCode()) {
            case NT_NOTHING:
            case NT_NULL:
                return "null";

            case NT_HASH:
                if (value instanceof hash<DataProviderFieldReference>) {
                    return AbstractODataRecordIterator::getPropertyPath(value.field, value.field_path);
                }
                throw "VALUE-ERROR", sprintf("a hash cannot be written as an OData literal (%y)", value);

            case NT_STRING:
            case NT_INT:
            case NT_FLOAT:
            case NT_NUMBER:
            case NT_BOOLEAN:
            case NT_DATE:
            case NT_BINARY:
                break;

            default:
                throw "VALUE-ERROR", sprintf("a value of type %y cannot be written as an OData literal", value.type());
        }

        switch (field_type) {
            case "int":
            case "integer":
                return AbstractODataRecordIterator::getODataIntLiteral(value);

            case "float":
            case "double":
            case "decimal":
            case "number":
                return AbstractODataRecordIterator::getODataNumberLiteral(value);

            case "bool":
            case "boolean":
                return AbstractODataRecordIterator::getODataBoolLiteral(value);

            case "guid":
                return AbstractODataRecordIterator::getODataGuidLiteral(value);

            case "date":
            case "datetime":
                return AbstractODataRecordIterator::getODataDateTimeLiteral(value);

            case "binary":
                if (value.typeCode() == NT_BINARY) {
                    return AbstractODataRecordIterator::getODataBinaryLiteral(value);
                }
                break;

            case "string":
                # a number compared with a string field is given as a string
                switch (value.typeCode()) {
                    case NT_STRING:
                        return AbstractODataRecordIterator::getODataStringLiteral(value);
                    case NT_INT:
                    case NT_FLOAT:
                    case NT_NUMBER:
                        return AbstractODataRecordIterator::getODataStringLiteral(string(value));
                }
                throw "VALUE-ERROR", sprintf("a value of type %y cannot be compared with a string field",
                    value.type());
        }

        switch (value.typeCode()) {
            case NT_INT:
                return string(value);
            case NT_FLOAT:
            case NT_NUMBER:
                return AbstractODataRecordIterator::getODataNumberLiteral(value);
            case NT_BOOLEAN:
                return value ? "true" : "false";
            case NT_DATE:
                return AbstractODataRecordIterator::getODataDateTimeLiteral(value);
            case NT_BINARY:
                return AbstractODataRecordIterator::getODataBinaryLiteral(value);
        }
        return AbstractODataRecordIterator::getODataStringLiteral(value);
    }

    #! Returns the OData string literal of a string: in single quotes, with each single quote doubled
    /** checkODataStringValue() first refuses a string whose encoding is invalid or that has a NUL character; the
        literal is in UTF-8

        @param str the string

        @return the OData string literal

        @throw VALUE-ERROR the string's encoding is invalid, or it has a NUL character

        @since CdsRestDataProvider 2.2
    */
    static string getODataStringLiteral(string str) {
        str = AbstractODataRecordIterator::checkODataStringValue(str);
        # the string is valid in its encoding, so replace() processes all of it
        return "'" + replace(str, "'", "''") + "'";
    }

    #! Checks that a string can be written in an OData query and returns it in UTF-8
    /** @param str the string

        @return the string in UTF-8

        @throw VALUE-ERROR the string's encoding is invalid, or it has a NUL character; such a string is refused
        before any query is made with it

        @since CdsRestDataProvider 2.2
    */
    static string checkODataStringValue(string str) {
        try {
            if (str.encoding() == "UTF-8") {
                # a conversion to the same encoding would not check it
                string check = convert_encoding(str, "UTF-16BE");
                remove check;
            } else {
                # the conversion refuses a string whose encoding is invalid
                str = convert_encoding(str, "UTF-8");
            }
        } catch (hash<ExceptionInfo> ex) {
            throw "VALUE-ERROR", sprintf("a string given to an OData query is not valid in its encoding (%s): %s: %s",
                str.encoding(), ex.err, ex.desc);
        }
        # checked after the encoding, as a search in a string with an invalid encoding raises another error
        if (str.find(chr(0)) >= 0) {
            throw "VALUE-ERROR", "a string with a NUL character cannot be given in an OData query";
        }
        return str;
    }

    #! Returns the OData literal of an integer value
    static private string getODataIntLiteral(auto value) {
        switch (value.typeCode()) {
            case NT_INT:
                return string(value);
            case NT_BOOLEAN:
                return value ? "1" : "0";
            case NT_FLOAT:
            case NT_NUMBER:
                if (value == int(value)) {
                    return string(int(value));
                }
                break;
            case NT_STRING: {
                string str = AbstractODataRecordIterator::checkODataStringValue(value);
                # only a string that is exactly the integer it converts to is accepted
                if (str == string(int(str))) {
                    return str;
                }
                break;
            }
        }
        throw "VALUE-ERROR", sprintf("the value %y is not an integer and cannot be compared with an integer field",
            value);
    }

    #! Returns the OData literal of a numeric value
    static private string getODataNumberLiteral(auto value) {
        switch (value.typeCode()) {
            case NT_INT:
                return string(value);
            case NT_FLOAT: {
                float f = value;
                if (f != f) {
                    return "NaN";
                }
                # only an infinite value is not zero when multiplied by zero
                if (f * 0.0 != 0.0) {
                    return f > 0.0 ? "INF" : "-INF";
                }
                # a finite float is written with digits, a sign, a decimal point and an exponent only
                return string(f);
            }
            case NT_NUMBER: {
                string str = string(value);
                switch (str) {
                    case "@NaN@": return "NaN";
                    case "@Inf@": return "INF";
                    case "-@Inf@": return "-INF";
                }
                # a finite number is written with digits, a sign, a decimal point and an exponent only
                return str;
            }
            case NT_STRING: {
                string str = AbstractODataRecordIterator::checkODataStringValue(value);
                if (str =~ /^-?[0-9]+(\.[0-9]+)?([eE][-+]?[0-9]+)?\z/) {
                    return str;
                }
                break;
            }
        }
        throw "VALUE-ERROR", sprintf("the value %y is not a number and cannot be compared with a numeric field",
            value);
    }

    #! Returns the OData literal of a boolean value
    static private string getODataBoolLiteral(auto value) {
        switch (value.typeCode()) {
            case NT_BOOLEAN:
            case NT_INT:
            case NT_FLOAT:
            case NT_NUMBER:
                return value ? "true" : "false";
            case NT_STRING: {
                string str = AbstractODataRecordIterator::checkODataStringValue(value).lwr();
                switch (str) {
                    case "true":
                    case "1":
                        return "true";
                    case "false":
                    case "0":
                        return "false";
                }
                break;
            }
        }
        throw "VALUE-ERROR", sprintf("the value %y is not a boolean and cannot be compared with a boolean field",
            value);
    }

    #! Returns the OData literal of a GUID
    static private string getODataGuidLiteral(auto value) {
        if (value.typeCode() == NT_STRING) {
            string str = AbstractODataRecordIterator::checkODataStringValue(value);
            if (str =~ /^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\z/) {
                return str;
            }
        }
        throw "VALUE-ERROR", sprintf("the value %y is not a GUID and cannot be compared with a GUID field", value);
    }

    #! Returns the OData literal of a date value
    static private string getODataDateTimeLiteral(auto value) {
        switch (value.typeCode()) {
            case NT_DATE: {
                if (value.relative()) {
                    throw "VALUE-ERROR", sprintf("the relative date %y cannot be written as an OData literal", value);
                }
                # written in UTC, so the comparison does not depend on the local time zone
                TimeZone utc(0);
                date d = utc.date(value);
                return d.format(d.microseconds() ? "YYYY-MM-DDTHH:mm:SS.xx" : "YYYY-MM-DDTHH:mm:SS") + "Z";
            }
            case NT_STRING: {
                string str = AbstractODataRecordIterator::checkODataStringValue(value);
                # an ISO-8601 date, optionally followed by a time with a zone
                if (str =~ /^[0-9]{4}-[0-9]{2}-[0-9]{2}/) {
                    string time = str.substr(10);
                    if (time == ""
                        || time =~ /^T[0-9]{2}:[0-9]{2}(:[0-9]{2}(\.[0-9]{1,12})?)?(Z|[-+][0-9]{2}:[0-9]{2})\z/) {
                        return str;
                    }
                }
                break;
            }
        }
        throw "VALUE-ERROR", sprintf("the value %y is not a date and cannot be compared with a date field", value);
    }

    #! Returns the OData literal of a binary value
    static private string getODataBinaryLiteral(binary value) {
        return "binary'" + make_base64_url_string(value) + "'";
    }

    #! Returns the record description, if available
    /** @return the record type of the iterator
    */
    *hash<string, DataProvider::AbstractDataField> getRecordType() {
        return record_info.record_type;
    }

    #! Formats a value for use in OData query
    /** @param key the field the value is compared with
        @param value the value

        @return the OData literal of the value; see @ref getODataLiteral()

        @throw VALUE-ERROR the value cannot be written as an OData literal
    */
    string getArgValue(string key, auto value) {
        return AbstractODataRecordIterator::getODataLiteral(value, record_info.field_types{key});
    }

    private string getOrderBy(softlist<string> coll) {
        return (map $1, coll, checkColumnName($1)).join(",");
    }

    private bool checkColumnName(string col) {
        AbstractODataRecordIterator::checkODataIdentifier(col, "COLUMN-ERROR", "column");
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
                        AbstractODataRecordIterator::checkODataIdentifier(col, "COLUMN-ERROR", "column");
                        exp_list += col;
                        break;

                    default:
                        throw "COLUMN-ERROR", sprintf("cannot process column value of type %y (%y)", col.type(), col);
                }
            }

            return exp_list.join(",");
        } else if (record_info.record_type) {
            return (keys record_info.record_type).join(",");
        }
        return "*";
    }

    #! Checks that a name is an OData identifier: a letter or an underscore followed by letters, digits and underscores
    /** The names of the properties of Dataverse and Business Central entities are such identifiers; any other name
        is refused rather than written in a query, where it could be read as more than one name

        @param name the name
        @param err the exception code
        @param what what the name is, for the exception message

        @throw err the name is not an OData identifier
        @throw VALUE-ERROR the name's encoding is invalid, or it has a NUL character

        @since CdsRestDataProvider 2.2
    */
    static checkODataIdentifier(string name, string err, string what) {
        AbstractODataRecordIterator::checkODataStringValue(name);
        if (name !~ /^[A-Za-z_][A-Za-z0-9_]*\z/) {
            throw err, sprintf("the %s %y cannot be named in OData: a name is a letter or an underscore followed by "
                "letters, digits and underscores", what, name);
        }
    }

    #! Legacy where expression handler for backward compatibility
    private string doWhereExpressionLegacy(hash<auto> where_cond, *hash<auto> search_options) {
        list<string> clauses;
        foreach hash<auto> i in (where_cond.pairIterator()) {
            clauses += doWhereExpressionInternLegacy(i.key, i.value);
        }
        return foldl $1 + " and " + $2, clauses;
    }

    #! Returns the OData property path of a field path: its keys joined with \c "/"
    /** OData names a navigation property and a property of the entity it leads to with a \c "/" (\c order.id is
        <tt>order/id</tt>).  A property name is an identifier: a letter or an underscore followed by letters, digits
        and underscores; it has neither a dot nor a slash, and a segment of a path with a dot is a cast to a type, so
        a key that is not an identifier cannot be named: the field is refused rather than sent as another name.

        @param field the field path
        @param field_path the keys of the path, if already read

        @return the OData property path

        @throw WHERE-ERROR a key of the field path is not an OData identifier
        @throw VALUE-ERROR the field's encoding is invalid, or it has a NUL character

        @par Example:
        @code{.py}
string path = AbstractODataRecordIterator::getPropertyPath("primarycontactid.fullname");
# path: "primarycontactid/fullname"
        @endcode

        @since CdsRestDataProvider 2.2
    */
    static string getPropertyPath(string field, *list<string> field_path) {
        AbstractODataRecordIterator::checkODataStringValue(field);
        list<string> path = field_path ?? AbstractDataProvider::getFieldPath(field);
        foreach string key in (path) {
            AbstractODataRecordIterator::checkODataStringValue(key);
            if (key !~ /^[A-Za-z_][A-Za-z0-9_]*\z/) {
                throw "WHERE-ERROR", sprintf("the field %y has the key %y, which OData cannot name: a property name "
                    "is a letter or an underscore followed by letters, digits and underscores; it has no dot or "
                    "slash, and a dot in a property path is a cast to a type", field, key);
            }
        }
        return path.join("/");
    }

    #! Legacy where expression handler for backward compatibility
    private string doWhereExpressionInternLegacy(string key, auto value) {
        *hash<auto> op;
        if (value.typeCode() == NT_HASH && value.op) {
            op = DefaultQueryOpMap{value.op};
            if (!op) {
                throw "WHERE-ERROR", sprintf("operator hash for where cond key %y has unknown operator "
                    "%y: expecting one of: %y (operator hash: %y)", key, value.op, keys DefaultQueryOpMap,
                    value);
            }
            # the key of an "or" operator is not a field, and its argument is a list of conditions
            if (value.op == QUERY_OP_OR) {
                return op.code(self, key, value.arg);
            }
        }
        # the key is a field path, written as an OData property path
        string property = AbstractODataRecordIterator::getPropertyPath(key);
        if (op) {
            # the argument of a recursive operator is another condition, not a value
            if (op.recursive) {
                return op.code(self, doWhereExpressionInternLegacy(key, value.arg), value.arg);
            }
            if (!op.no_process_arg) {
                value.arg = getArgValue(property, value.arg);
            }
            return op.code(self, property, value.arg);
        }

        return sprintf("%s eq %s", property, getArgValue(property, value));
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
