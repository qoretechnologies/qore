# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CDSRestDataProviderDefs definitions

/** CDSRestDataProviderDefs.qc Copyright 2023 - 2025 Qore Technologies, s.r.o.

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

#! contains all public definitions in the CDSRestDataProvider module
public namespace CDSRestDataProvider {
#! Common OData record information used by both CDS and Business Central
public hashdecl ODataRecordInfo {
    #! The record type for the object
    hash<string, AbstractDataField> record_type;

    #! The original field type names
    hash<string, string> field_types;

    #! Hash of date/time fields for this record
    hash<string, bool> date_time_fields;

    #! Key field name (e.g., "id" for BC, or entity-specific for CDS)
    string key;

    #! Entity set/table name for API calls
    string entity_name;
}

#! Query operator info hash as returned by all \c query_op_funcs "operator functions"
public hashdecl QueryOperatorInfo {
    string op;   #!< the operator string code
    auto arg;    #!< optional argument
}


/** @defgroup query_ops Query Operators
    These are the operators that can be used in queries
*/
#/@{
#! like/contains operator
const QUERY_OP_CONTAINS = "contains";

#! like/contains operator
const QUERY_OP_ENDSWITH = "endsWith";

#! like/contains operator
const QUERY_OP_STARTSWITH = "startsWith";

#! the Query less than (<) operator for use in queries
/** @see query_op_lt()
*/
public const QUERY_OP_LT = "<";

#! the Query less than or equals (<=) operator for use in queries
/** @see query_op_le()
*/
public const QUERY_OP_LE = "<=";

#! the Query greater than operator (>) for use in queries
/** @see query_op_gt()
*/
public const QUERY_OP_GT = ">";

#! the Query greater than or equals operator (>=) for use in queries
/** @see query_op_ge()
*/
public const QUERY_OP_GE = ">=";

#! the Query not equals operator (!= or <>) for use in queries
/** @see query_op_ne()
*/
public const QUERY_OP_NE = "!=";

#! the Query equals operator (=) for use in queries
/** @see query_op_eq()
*/
public const QUERY_OP_EQ = "==";

#! the Query \c "not" operator for use in queries
/** @see query_op_not()
*/
public const QUERY_OP_NOT = "!";

#! to combine Query expressions with \c "or" for use in queries
/** @see query_wquery_op_or()
*/
public const QUERY_OP_OR = "||";
#/@}

/** @defgroup query_op_funcs Query Operator Functions
    These are the operators that can be used in CDS queries
    - query_op_like(): for the \c "like" operator
    - query_op_ge(): for the \c ">=" operator when comparing column values to immediate values
    - query_op_gt(): for the \c ">" operator when comparing column values to immediate values
    - query_op_le(): for the \c "<=" operator when comparing column values to immediate values
    - query_op_lt(): for the \c "<" operator when comparing column values to immediate values
    - query_op_ne(): for the \c "!=" or \c "<>" operator when comparing column values to immediate values
    - query_op_eq(): for the \c "=" operator when comparing column values to immediate values
    - query_op_not(): for negating other operators
    - query_wop_or(): for combining Query expressions with \c "or"
*/
#/@{
#! returns an @ref QueryOperatorInfo hash
public hash<QueryOperatorInfo> sub query_make_op(string op, auto arg) {
    return new hash<QueryOperatorInfo>({"op": op, "arg": arg});
}

#! returns an @ref QueryOperatorInfo hash for the \c "contains" operator with the given argument for use in cds queries
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = provider.searchRecords({"Name": query_op_like("%smith%")});
    @endcode

    @param str the argument for the operator

    @return an @ref QueryOperatorInfo hash for use in cds queries
*/
public hash<QueryOperatorInfo> sub query_op_like(string str) {
    return query_make_op(QUERY_OP_CONTAINS, str);
}

#! returns an @ref QueryOperatorInfo hash for the \c "contains" operator with the given argument for use in cds queries
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = provider.searchRecords({"Name": query_op_contains("%smith%")});
    @endcode

    @param str the argument for the operator

    @return an @ref QueryOperatorInfo hash for use in cds queries
*/
public hash<QueryOperatorInfo> sub query_op_contains(string str) {
    return query_make_op(QUERY_OP_CONTAINS, str);
}

#! returns an @ref QueryOperatorInfo hash for the \c "<" operator with the given argument for use in cds queries when comparing column values to immediate values
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = provider.searchRecords({"Name": query_op_lt("Zebra")});
    @endcode

    @param arg the argument for the operator

    @return an @ref QueryOperatorInfo hash for use in cds queries
*/
public hash<QueryOperatorInfo> sub query_op_lt(auto arg) {
    return query_make_op(QUERY_OP_LT, arg);
}

#! returns an @ref QueryOperatorInfo hash for the \c "<=" operator with the given argument for use in cds queries when comparing column values to immediate values
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = provider.searchRecords({"Name": query_op_le("Zebra")});
    @endcode

    @param arg the argument for the operator

    @return an @ref QueryOperatorInfo hash for use in cds queries
*/
public hash<QueryOperatorInfo> sub query_op_le(auto arg) {
    return query_make_op(QUERY_OP_LE, arg);
}

#! returns an @ref QueryOperatorInfo hash for the \c ">" operator with the given argument for use in cds queries when comparing column values to immediate values
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = provider.searchRecords({"Name": query_op_gt("Apple")});
    @endcode

    @param arg the argument for the operator

    @return an @ref QueryOperatorInfo hash for use in cds queries
*/
public hash<QueryOperatorInfo> sub query_op_gt(auto arg) {
    return query_make_op(QUERY_OP_GT, arg);
}

#! returns an @ref QueryOperatorInfo hash for the \c ">=" operator with the given argument for use in cds queries when comparing column values to immediate values
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = provider.searchRecords({"Name": query_op_ge("Apple")});
    @endcode

    @param arg the argument for the operator

    @return an @ref QueryOperatorInfo hash for use in cds queries
*/
public hash<QueryOperatorInfo> sub query_op_ge(auto arg) {
    return query_make_op(QUERY_OP_GE, arg);
}

#! returns an @ref QueryOperatorInfo hash for the \c "!=" or \c "<>" operator with the given argument for use in cds queries when comparing column values to immediate values
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = provider.searchRecords({"Name": query_op_ne("Smith")});
    @endcode

    @param arg the argument for the operator
*/
public hash<QueryOperatorInfo> sub query_op_ne(auto arg) {
    return query_make_op(QUERY_OP_NE, arg);
}

#! returns an @ref QueryOperatorInfo hash for the \c "=" operator with the given argument for use in cds queries when comparing column values to immediate values
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = provider.searchRecords({"Name": query_op_eq("Smith")});
    @endcode

    @param arg the argument for the operator

    @return an @ref QueryOperatorInfo hash for use in cds queries
*/
public hash<QueryOperatorInfo> sub query_op_eq(auto arg) {
    return query_make_op(QUERY_OP_EQ, arg);
}

#! returns an @ref QueryOperatorInfo hash for the \c "not" operator; for use in cds queries
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = provider.searchRecords({"name": query_op_not(query_op_eq(name))});
    @endcode

    @return an @ref QueryOperatorInfo hash for use in cds queries
*/
public hash<QueryOperatorInfo> sub query_op_not(hash arg) {
    return query_make_op(QUERY_OP_NOT, arg);
}

#! returns an @ref QueryOperatorInfo hash with a fake \c "_OR_" column name; the list of arguments to the function is combined such that each hash in the list generates SQL expressions combined with \a "and" logic, and each of those clauses is combined with \a "or" logic; this is for use in cds queries
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = provider.searchRecords({"Id": id} + query_wop_or({"Type": "C", "ValidationFlag": query_op_ne("Y")}, {"ExpirationFlag": 'Y'}));
    @endcode

    @param h1 the first hash of expressions to combine with \c "or" logic
    @param h2 the second hash of expressions to combine with \c "or" logic
    @param ... remaining expressions to combine with \c "or" logic

    @return a hash with an @ref QueryOperatorInfo hash value keyed by a fake column name (\c "_OR_" with a numeric
    prefix for uniqueness) for use in a where operation description hash for use in cds queries

    @note a random prefix is used so that multiple uses of the function can be used to generate a single where clause
    hash
*/
public hash<string, hash<QueryOperatorInfo>> sub query_wop_or(hash<auto> h1, hash<auto> h2) {
    softlist<auto> l = h1;
    l += h2;
    if (argv)
        l += argv;
    return new hash<string, hash<QueryOperatorInfo>>((sprintf("%d:_OR_", rand() % 10000000): query_make_op(QUERY_OP_OR, l)));
}
#/@}

#! a hash of valid operators for use in queries
public const DefaultQueryOpMap = {
    QUERY_OP_CONTAINS: (
        "code": string sub (object i, string cn, auto arg) {
            return sprintf("contains(%s, %s)", cn, arg);
        },
    ),
    QUERY_OP_ENDSWITH: (
        "code": string sub (object i, string cn, auto arg) {
            return sprintf("endswith(%s, %s)", cn, arg);
        },
    ),
    QUERY_OP_STARTSWITH: (
        "code": string sub (object i, string cn, auto arg) {
            return sprintf("startswith(%s, %s)", cn, arg);
        },
    ),
    QUERY_OP_LT: (
        "code": string sub (object i, string cn, auto arg) {
            return sprintf("%s lt %s", cn, arg);
        },
    ),
    QUERY_OP_LE: (
        "code": string sub (object i, string cn, auto arg) {
            return sprintf("%s le %s", cn, arg);
        },
    ),
    QUERY_OP_GT: (
        "code": string sub (object i, string cn, auto arg) {
            return sprintf("%s gt %s", cn, arg);
        },
    ),
    QUERY_OP_GE: (
        "code": string sub (object i, string cn, auto arg) {
            return sprintf("%s ge %s", cn, arg);
        },
    ),
    QUERY_OP_NE: (
        "code": string sub (object i, string cn, auto arg) {
            return sprintf("%s ne %s", cn, arg);
        },
    ),
    QUERY_OP_EQ: (
        "code": string sub (object i, string cn, auto arg) {
            return sprintf("%s eq %s", cn, arg);
        },
    ),
    QUERY_OP_NOT: (
        "recursive": True,
        "code": string sub (object i, string cn, auto arg) {
            return sprintf("not (%s)", cn);
        },
    ),
    QUERY_OP_OR: (
        "code": string sub (object i, string cn, list<auto> arg) {
            return i.getOrClause(arg);
        },
    ),
};
#/@}

/** @defgroup odata_expression_ops OData Expression Operators
    These are the operator constants for OData expression support
*/
#/@{
#! OData contains operator
public const ODATA_OP_CONTAINS = "contains";

#! OData startsWith operator
public const ODATA_OP_STARTSWITH = "startsWith";

#! OData endsWith operator
public const ODATA_OP_ENDSWITH = "endsWith";

#! OData null check operator
public const ODATA_OP_NULL = "null";

#! OData notNull check operator
public const ODATA_OP_NOT_NULL = "notNull";
#/@}

#! OData expression definitions for use with DataProvider search expressions
/** These expressions are used by both Dynamics 365 CDS and Business Central since they share
    standard OData 4.0 query syntax.
*/
public const ODataExpressions = {
    # Logical operators with nesting support
    DP_OP_AND: {
        "exp": AbstractDataProvider::GenericExpressions{DP_OP_AND},
        "impl": string sub (object iter, list<auto> args) {
            # Recursively process arguments and join with " and "
            list<string> clauses = map iter.processExpressionArg($1), args;
            return "(" + (foldl $1 + " and " + $2, clauses) + ")";
        },
    },
    DP_OP_OR: {
        "exp": AbstractDataProvider::GenericExpressions{DP_OP_OR},
        "impl": string sub (object iter, list<auto> args) {
            # Recursively process arguments and join with " or "
            list<string> clauses = map iter.processExpressionArg($1), args;
            return "(" + (foldl $1 + " or " + $2, clauses) + ")";
        },
    },

    # Comparison operators - reuse generic expressions and add OData-specific arg_info with example values
    DP_SEARCH_OP_EQ: {
        "exp": cast<hash<DataProviderExpressionInfo>>(AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_EQ} + {
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Field",
                    "short_desc": "The field to compare",
                    "desc": "The field to compare against the value",
                    "label_after": "==",
                },
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to compare against",
                    "desc": "The value to compare against; the field must equal this value for the expression to "
                        "return `True`",
                    "example_value": "Smith",
                },
            ),
        }),
        "impl": string sub (object iter, string field, auto value) {
            return sprintf("%s eq %s", field, iter.formatODataValue(field, value));
        },
    },
    DP_SEARCH_OP_NE: {
        "exp": cast<hash<DataProviderExpressionInfo>>(AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_NE} + {
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Field",
                    "short_desc": "The field to compare",
                    "desc": "The field to compare against the value",
                    "label_after": "!=",
                },
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to compare against",
                    "desc": "The value to compare against; the field must not equal this value for the expression to "
                        "return `True`",
                    "example_value": "closed",
                },
            ),
        }),
        "impl": string sub (object iter, string field, auto value) {
            return sprintf("%s ne %s", field, iter.formatODataValue(field, value));
        },
    },
    DP_SEARCH_OP_LT: {
        "exp": cast<hash<DataProviderExpressionInfo>>(AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_LT} + {
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Field",
                    "short_desc": "The field to compare",
                    "desc": "The field to compare against the value",
                    "label_after": "<",
                },
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The upper bound value",
                    "desc": "The upper bound value; the field must be strictly less than this value for the "
                        "expression to return `True`",
                    "example_value": 1000,
                },
            ),
        }),
        "impl": string sub (object iter, string field, auto value) {
            return sprintf("%s lt %s", field, iter.formatODataValue(field, value));
        },
    },
    DP_SEARCH_OP_LE: {
        "exp": cast<hash<DataProviderExpressionInfo>>(AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_LE} + {
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Field",
                    "short_desc": "The field to compare",
                    "desc": "The field to compare against the value",
                    "label_after": "<=",
                },
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The upper bound value (inclusive)",
                    "desc": "The upper bound value; the field must be less than or equal to this value for the "
                        "expression to return `True`",
                    "example_value": 1000,
                },
            ),
        }),
        "impl": string sub (object iter, string field, auto value) {
            return sprintf("%s le %s", field, iter.formatODataValue(field, value));
        },
    },
    DP_SEARCH_OP_GT: {
        "exp": cast<hash<DataProviderExpressionInfo>>(AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_GT} + {
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Field",
                    "short_desc": "The field to compare",
                    "desc": "The field to compare against the value",
                    "label_after": ">",
                },
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The lower bound value",
                    "desc": "The lower bound value; the field must be strictly greater than this value for the "
                        "expression to return `True`",
                    "example_value": 100,
                },
            ),
        }),
        "impl": string sub (object iter, string field, auto value) {
            return sprintf("%s gt %s", field, iter.formatODataValue(field, value));
        },
    },
    DP_SEARCH_OP_GE: {
        "exp": cast<hash<DataProviderExpressionInfo>>(AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_GE} + {
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Field",
                    "short_desc": "The field to compare",
                    "desc": "The field to compare against the value",
                    "label_after": ">=",
                },
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The lower bound value (inclusive)",
                    "desc": "The lower bound value; the field must be greater than or equal to this value for the "
                        "expression to return `True`",
                    "example_value": 100,
                },
            ),
        }),
        "impl": string sub (object iter, string field, auto value) {
            return sprintf("%s ge %s", field, iter.formatODataValue(field, value));
        },
    },

    # NOT operator (wraps another expression)
    DP_SEARCH_OP_NOT: {
        "exp": AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_NOT},
        "impl": string sub (object iter, hash<DataProviderExpression> inner_exp) {
            return sprintf("not (%s)", iter.processExpressionArg(inner_exp));
        },
    },

    # BETWEEN operator - reuse generic expression and add OData-specific arg_info with example values
    DP_SEARCH_OP_BETWEEN: {
        "exp": cast<hash<DataProviderExpressionInfo>>(AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_BETWEEN} + {
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Field",
                    "short_desc": "The field to compare",
                    "desc": "The field to compare; must be between the lower and upper bounds for the expression to "
                        "return `True`",
                    "label_after": "between",
                },
                <DataProviderArgInfo>{
                    "display_name": "Lower Bound",
                    "short_desc": "The lower bound (inclusive)",
                    "desc": "The lower bound for the field value (inclusive)",
                    "example_value": 100,
                    "label_after": "and",
                },
                <DataProviderArgInfo>{
                    "display_name": "Upper Bound",
                    "short_desc": "The upper bound (inclusive)",
                    "desc": "The upper bound for the field value (inclusive)",
                    "example_value": 1000,
                },
            ),
        }),
        "impl": string sub (object iter, string field, auto lower, auto upper) {
            return sprintf("(%s ge %s and %s le %s)",
                field, iter.formatODataValue(field, lower),
                field, iter.formatODataValue(field, upper));
        },
    },

    # IN operator - reuse generic expression and add OData-specific arg_info with example values
    DP_SEARCH_OP_IN: {
        "exp": cast<hash<DataProviderExpressionInfo>>(AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_IN} + {
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Field",
                    "short_desc": "The field to check",
                    "desc": "The field whose value will be checked against the list",
                    "label_after": "in",
                },
                <DataProviderArgInfo>{
                    "display_name": "Values",
                    "short_desc": "The list of values to match against",
                    "desc": "A list of values; the field must match one of these values for the expression to "
                        "return `True`",
                    "example_value": ("active", "pending"),
                },
            ),
        }),
        "impl": string sub (object iter, string field, list<auto> values) {
            # Use OData 4.0 in() syntax
            string value_list = foldl $1 + "," + $2,
                (map iter.formatODataValue(field, $1), values);
            return sprintf("%s in (%s)", field, value_list);
        },
    },

    # String operators
    ODATA_OP_CONTAINS: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": "contains",
            "display_name": "contains",
            "short_desc": "Returns True if the field contains the substring",
            "desc": "Returns `True` if the field value contains the specified substring; uses OData `contains()` "
                "function",
            "symbol": "contains",
            "groups": "String",
            "render_template": "contains($arg[0], $arg[1])",
            "args": (DataProviderSignatureFieldType, DataProviderSignatureStringValueType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Field",
                    "short_desc": "The string field to search in",
                    "desc": "The string field to search for the substring",
                },
                <DataProviderArgInfo>{
                    "display_name": "Substring",
                    "short_desc": "The substring to search for",
                    "desc": "The substring to search for in the field value",
                    "example_value": "smith",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": string sub (object iter, string field, string value) {
            return sprintf("contains(%s,%s)", field, iter.formatODataValue(field, value));
        },
    },
    ODATA_OP_STARTSWITH: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": "startsWith",
            "display_name": "starts with",
            "short_desc": "Returns True if the field starts with the prefix",
            "desc": "Returns `True` if the field value starts with the specified prefix; uses OData `startswith()` "
                "function",
            "symbol": "startsWith",
            "groups": "String",
            "render_template": "startswith($arg[0], $arg[1])",
            "args": (DataProviderSignatureFieldType, DataProviderSignatureStringValueType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Field",
                    "short_desc": "The string field to check",
                    "desc": "The string field to check for the prefix",
                },
                <DataProviderArgInfo>{
                    "display_name": "Prefix",
                    "short_desc": "The prefix to match",
                    "desc": "The prefix that the field value must start with",
                    "example_value": "A",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": string sub (object iter, string field, string value) {
            return sprintf("startswith(%s,%s)", field, iter.formatODataValue(field, value));
        },
    },
    ODATA_OP_ENDSWITH: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": "endsWith",
            "display_name": "ends with",
            "short_desc": "Returns True if the field ends with the suffix",
            "desc": "Returns `True` if the field value ends with the specified suffix; uses OData `endswith()` "
                "function",
            "symbol": "endsWith",
            "groups": "String",
            "render_template": "endswith($arg[0], $arg[1])",
            "args": (DataProviderSignatureFieldType, DataProviderSignatureStringValueType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Field",
                    "short_desc": "The string field to check",
                    "desc": "The string field to check for the suffix",
                },
                <DataProviderArgInfo>{
                    "display_name": "Suffix",
                    "short_desc": "The suffix to match",
                    "desc": "The suffix that the field value must end with",
                    "example_value": ".com",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": string sub (object iter, string field, string value) {
            return sprintf("endswith(%s,%s)", field, iter.formatODataValue(field, value));
        },
    },

    # NULL checks
    ODATA_OP_NULL: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": "null",
            "display_name": "is null",
            "short_desc": "Returns True if the field is null",
            "desc": "Returns `True` if the field value is null; translates to OData `field eq null`",
            "symbol": "null",
            "groups": "Comparison",
            "render_template": "$arg[0] eq null",
            "args": (DataProviderSignatureFieldType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Field",
                    "short_desc": "The field to check for null",
                    "desc": "The field to check; returns `True` if the field value is null",
                    "label_after": "eq null",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": string sub (object iter, string field) {
            return sprintf("%s eq null", field);
        },
    },
    ODATA_OP_NOT_NULL: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": "notNull",
            "display_name": "is not null",
            "short_desc": "Returns True if the field is not null",
            "desc": "Returns `True` if the field value is not null; translates to OData `field ne null`",
            "symbol": "notNull",
            "groups": "Comparison",
            "render_template": "$arg[0] ne null",
            "args": (DataProviderSignatureFieldType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Field",
                    "short_desc": "The field to check for not null",
                    "desc": "The field to check; returns `True` if the field value is not null",
                    "label_after": "ne null",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": string sub (object iter, string field) {
            return sprintf("%s ne null", field);
        },
    },
};
#/@}
}
