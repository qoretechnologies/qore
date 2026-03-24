# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractDataProviderRecordIterator class definition

/** AbstractDataProviderRecordIterator.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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

# assume local scope for variables, do not use "$" signs
%modern
# require type definitions everywhere
#! strict argument handling
# enable all warnings

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
/** @defgroup dataprovider_logic_ops DataProvider Generic Logic Operators
    These are the generic logic operators that can be used using generic data provider APIs; all logical operations
    support logic short-circuiting
*/
#/@{
#! AND logic
public const DP_OP_AND = "&&";

#! OR logic
public const DP_OP_OR = "||";
#/@}

/** @defgroup dataprovider_generic_search_ops DataProvider Generic Search Operators
    These are the generic operators that can be used for searching using the generic data provider search APIs
*/
#/@{
#! Identifies the regex operator for use in generic search criteria
/** @see dp_op_like()
*/
public const DP_SEARCH_OP_REGEX = "regex";

#! Identifies the generic data provider less than (<) operator for use in generic search criteria
/** @see dp_op_lt()
*/
public const DP_SEARCH_OP_LT = "<";

#! Identifies the generic data provider less than or equals (<=) operator for use in generic search criteria
/** @see dp_op_le()
*/
public const DP_SEARCH_OP_LE = "<=";

#! Identifies the generic data provider greater than operator (>) for use in generic search criteria
/** @see dp_op_gt()
*/
public const DP_SEARCH_OP_GT = ">";

#! Identifies the generic data provider greater than or equals operator (>=) for use in generic search criteria
/** @see dp_op_ge()
*/
public const DP_SEARCH_OP_GE = ">=";

#! Identifies the generic data provider not equals operator (!= or <>) for use in generic search criteria
/** @see dp_op_ne()
*/
public const DP_SEARCH_OP_NE = "!=";

#! Identifies the generic data provider equals operator (=) for use in generic search criteria
/** @see dp_op_eq()
*/
public const DP_SEARCH_OP_EQ = "==";

#! Identifies the generic data provider \c "between" operator for use in generic search criteria
/** @see dp_op_between()
*/
public const DP_SEARCH_OP_BETWEEN = "between";

#! Identifies the generic data provider \c "in" operator for use in generic search criteria
/** @see dp_op_in()
*/
public const DP_SEARCH_OP_IN = "in";

#! Identifies the generic data provider \c "not" operator for use in generic search criteria
/** @see dp_op_not()
*/
public const DP_SEARCH_OP_NOT = "!";
#/@}

/** @defgroup dataprovider_extended_ops DataProvider Extended Operators
    These are extended operators for data manipulation, comparison, and conversion in DPQL expressions

    @since DataProvider 3.3
*/
#/@{
#! Absolute equality operator (===): returns True if both type and value are equal
public const DP_OP_ABSOLUTE_EQ = "===";

#! Absolute not-equal operator (!==): returns True if type or value differ
public const DP_OP_ABSOLUTE_NE = "!==";

#! Addition operator (+): adds values (numbers, strings, dates, lists)
public const DP_OP_PLUS = "+";

#! Subtraction operator (-): subtracts the second value from the first
public const DP_OP_MINUS = "-";

#! Multiplication operator (*): multiplies numeric values
public const DP_OP_MULT = "*";

#! Division operator (/): divides the first numeric value by the second
public const DP_OP_DIV = "/";

#! String starts-with operator
public const DP_OP_STARTS_WITH = "starts-with";

#! String ends-with operator
public const DP_OP_ENDS_WITH = "ends-with";

#! String contains operator
public const DP_OP_CONTAINS = "contains";

#! List contains element operator
public const DP_OP_LIST_CONTAINS = "listContains";

#! String contains any of a list of substrings
public const DP_OP_CONTAINS_ANY = "contains-any";

#! List contains any of a list of values
public const DP_OP_LIST_CONTAINS_ANY = "listContainsAny";

#! Find value in hash/list by dot-notation path
public const DP_OP_FIND_IN = "findIn";

#! Has a "true" value (non-null, non-empty)
public const DP_OP_HAS_VALUE = "has-value";

#! In range: >= lower bound and < upper bound
public const DP_OP_RANGE = "inRange";

#! Regex match operator (=~)
public const DP_OP_REGEX_MATCH = "regex-match";

#! Regex non-match operator (!~)
public const DP_OP_REGEX_NMATCH = "regex-nmatch";

#! Regex substitution operator
public const DP_OP_REGEX_SUBST = "regex-subst";

#! Simple string replacement
public const DP_OP_REPLACE = "replace";

#! Get byte size (or element count for hashes/lists)
public const DP_OP_SIZE = "size";

#! Get character length (or element count for hashes/lists)
public const DP_OP_LENGTH = "length";

#! Get display character width of a string
public const DP_OP_CHAR_WIDTH = "width";

#! Get character encoding name of a string
public const DP_OP_CHAR_ENCODING = "encoding";

#! Remove accents from a string
public const DP_OP_UNACCENT = "unaccent";

#! Convert string to lower case
public const DP_OP_TO_LOWER = "lwr";

#! Convert string to upper case
public const DP_OP_TO_UPPER = "upr";

#! Join list elements with a separator string
public const DP_OP_JOIN = "join";

#! Convert string/binary to hex string
public const DP_OP_TO_HEX = "to-hex";

#! Convert string/binary to base64 string
public const DP_OP_TO_BASE64 = "to-base64";

#! Convert string/binary to base64-URL string (RFC 4648)
public const DP_OP_TO_BASE64_URL = "to-base64-url";

#! Convert value to string
public const DP_OP_TO_STRING = "toString";

#! Convert value to integer
public const DP_OP_TO_INT = "toInt";

#! Convert value to boolean
public const DP_OP_TO_BOOL = "toBool";

#! Convert value to float
public const DP_OP_TO_FLOAT = "toFloat";

#! Convert value to arbitrary-precision number
public const DP_OP_TO_NUMBER = "toNumber";

#! Convert value to date
public const DP_OP_TO_DATE = "toDate";

#! Ternary conditional operator (condition ? true_val : false_val)
public const DP_OP_TERNARY = "ternary";
#/@}

/** @defgroup dpql_function_ops DPQL Function Operator Constants
    These constants define function operators for accessing data elements in DPQL expressions
*/
#/@{
#! Index function: access a single element by index (e.g., \c index(\@items, 0))
public const DP_FUNC_INDEX = "index";

#! Slice function: access multiple elements by indices/ranges (e.g., \c slice(\@items, 0..3))
public const DP_FUNC_SLICE = "slice";

#! Key function: access hash values by key (e.g., \c key(\@record, "name"))
public const DP_FUNC_KEY = "key";
#/@}

/** @defgroup dpql_extended_function_ops DPQL Extended Function Operator Constants
    These constants define extended function operators for DPQL expressions

    @since DataProvider 3.3
*/
#/@{
#! Substring extraction function
public const DP_FUNC_SUBSTR = "substr";

#! Format date as string
public const DP_FUNC_FORMAT_DATE = "format_date";

#! Format number as string
public const DP_FUNC_FORMAT_NUMBER = "format_number";

#! Split string by regular expression
public const DP_FUNC_SPLIT_REGEX = "split_regex";

#! Split string by separator, optionally respecting quotes
public const DP_FUNC_SPLIT_QUOTED = "split_quoted";

#! Parse hex string to binary
public const DP_FUNC_PARSE_HEX = "parse_hex";

#! Parse base64 string to binary
public const DP_FUNC_PARSE_BASE64 = "parse_base64";

#! Parse base64-URL string (RFC 4648) to binary
public const DP_FUNC_PARSE_BASE64_URL = "parse_base64_url";

#! Parse base64 string to string with optional encoding
public const DP_FUNC_PARSE_BASE64_TO_STRING = "parse_base64_to_string";

#! Parse base64-URL string (RFC 4648) to string with optional encoding
public const DP_FUNC_PARSE_BASE64_URL_TO_STRING = "parse_base64_url_to_string";

#! Compute cryptographic digest (returns hex string)
public const DP_FUNC_DIGEST = "digest";

#! Compute HMAC (returns hex string)
public const DP_FUNC_HMAC = "hmac";

#! Encrypt data
public const DP_FUNC_ENCRYPT = "encrypt";

#! Decrypt data
public const DP_FUNC_DECRYPT = "decrypt";

#! URL-encode a string
public const DP_FUNC_ENCODE_URL = "encode_url";

#! URL-decode a string
public const DP_FUNC_DECODE_URL = "decode_url";

#! Returns the current date/time with microsecond precision
public const DP_FUNC_NOW = "now";

#! Serialize value to YAML string
public const DP_FUNC_MAKE_YAML = "make_yaml";

#! Deserialize YAML string to data
public const DP_FUNC_PARSE_YAML = "parse_yaml";

#! Serialize value to JSON string
public const DP_FUNC_MAKE_JSON = "make_json";

#! Deserialize JSON string to data
public const DP_FUNC_PARSE_JSON = "parse_json";

#! Serialize value to XML string
public const DP_FUNC_MAKE_XML = "make_xml";

#! Deserialize XML string to data
public const DP_FUNC_PARSE_XML = "parse_xml";
#/@}

/** @defgroup dpql_expression_types DPQL Expression Type Constants
    These constants define special expression types for DPQL
*/
#/@{
#! Value expression type: evaluates and returns a field reference value
/** This expression type wraps a bare field reference and returns its evaluated value.
    It allows field references to be used as standalone expressions without requiring
    a comparison operator.

    @par Example:
    @code
    # "@name" parses to:
    {
        "exp": "value",
        "args": (<DataProviderFieldReference>{"field": "name"},),
    }
    @endcode

    @since DataProvider 3.3
*/
public const DP_EXPR_VALUE = "value";

#! Template expression operator
/** Used to wrap a standalone template reference as an expression.

    @par Example
    @code{.py}
    # "$static:name" parses to:
    {
        "exp": "template",
        "args": (<DataProviderTemplateReference>{"tmpl_context": "static", "tmpl_value": "name", "raw": "$static:name"},),
    }
    @endcode

    @since DataProvider 3.3
*/
public const DP_EXPR_TEMPLATE = "template";
#/@}

#! Operator alias map; maps from old operator names to canonical ones
public const OperatorAliasMap = {
    "AND": DP_OP_AND,
    "OR": DP_OP_OR,
    "NOT": DP_SEARCH_OP_NOT,
    "not": DP_SEARCH_OP_NOT,
    "=": DP_SEARCH_OP_EQ,
    "eq": DP_SEARCH_OP_EQ,
    "ne": DP_SEARCH_OP_NE,
    "lt": DP_SEARCH_OP_LT,
    "le": DP_SEARCH_OP_LE,
    "gt": DP_SEARCH_OP_GT,
    "ge": DP_SEARCH_OP_GE,
    "BETWEEN": DP_SEARCH_OP_BETWEEN,
    "IN": DP_SEARCH_OP_IN,
    # Extended operator aliases (since DataProvider 3.3)
    "EQUALS": DP_SEARCH_OP_EQ,
    "ABSOLUTE-EQUALS": DP_OP_ABSOLUTE_EQ,
    "NOT-EQUALS": DP_SEARCH_OP_NE,
    "ABSOLUTE-NOT-EQUALS": DP_OP_ABSOLUTE_NE,
    "LESS-THAN": DP_SEARCH_OP_LT,
    "LESS-THAN-OR-EQUALS": DP_SEARCH_OP_LE,
    "GREATER-THAN": DP_SEARCH_OP_GT,
    "GREATER-THAN-OR-EQUALS": DP_SEARCH_OP_GE,
    "PLUS": DP_OP_PLUS,
    "MINUS": DP_OP_MINUS,
    "MULTIPLY": DP_OP_MULT,
    "DIVIDE": DP_OP_DIV,
    "FORMAT-DATE": DP_FUNC_FORMAT_DATE,
    "FORMAT-NUMBER": DP_FUNC_FORMAT_NUMBER,
    "STARTS-WITH": DP_OP_STARTS_WITH,
    "ENDS-WITH": DP_OP_ENDS_WITH,
    "CONTAINS": DP_OP_CONTAINS,
    "LIST-CONTAINS": DP_OP_LIST_CONTAINS,
    "CONTAINS-ANY": DP_OP_CONTAINS_ANY,
    "LIST-CONTAINS-ANY": DP_OP_LIST_CONTAINS_ANY,
    "FIND-IN": DP_OP_FIND_IN,
    "HAS-VALUE": DP_OP_HAS_VALUE,
    "REGEX-MATCH": DP_OP_REGEX_MATCH,
    "REGEX-NMATCH": DP_OP_REGEX_NMATCH,
    "REGEX-SUBST": DP_OP_REGEX_SUBST,
    "REPLACE": DP_OP_REPLACE,
    "SUBSTR": DP_FUNC_SUBSTR,
    "IN-RANGE": DP_OP_RANGE,
    "SIZE": DP_OP_SIZE,
    "LENGTH": DP_OP_LENGTH,
    "WIDTH": DP_OP_CHAR_WIDTH,
    "ENCODING": DP_OP_CHAR_ENCODING,
    "UNACCENT": DP_OP_UNACCENT,
    "TO-LOWER": DP_OP_TO_LOWER,
    "TO-UPPER": DP_OP_TO_UPPER,
    "SPLIT-REGEX": DP_FUNC_SPLIT_REGEX,
    "SPLIT-QUOTED": DP_FUNC_SPLIT_QUOTED,
    "JOIN": DP_OP_JOIN,
    "TO-HEX": DP_OP_TO_HEX,
    "TO-BASE64": DP_OP_TO_BASE64,
    "TO-BASE64-URL": DP_OP_TO_BASE64_URL,
    "PARSE-HEX": DP_FUNC_PARSE_HEX,
    "PARSE-BASE64": DP_FUNC_PARSE_BASE64,
    "PARSE-BASE64-URL": DP_FUNC_PARSE_BASE64_URL,
    "PARSE-BASE64-TO-STRING": DP_FUNC_PARSE_BASE64_TO_STRING,
    "PARSE-BASE64-URL-TO-STRING": DP_FUNC_PARSE_BASE64_URL_TO_STRING,
    "DIGEST": DP_FUNC_DIGEST,
    "HMAC": DP_FUNC_HMAC,
    "ENCRYPT": DP_FUNC_ENCRYPT,
    "DECRYPT": DP_FUNC_DECRYPT,
    "TO-STRING": DP_OP_TO_STRING,
    "TO-INT": DP_OP_TO_INT,
    "TO-BOOL": DP_OP_TO_BOOL,
    "TO-FLOAT": DP_OP_TO_FLOAT,
    "TO-NUMBER": DP_OP_TO_NUMBER,
    "TO-DATE": DP_OP_TO_DATE,
    "MAKE-YAML": DP_FUNC_MAKE_YAML,
    "PARSE-YAML": DP_FUNC_PARSE_YAML,
    "MAKE-JSON": DP_FUNC_MAKE_JSON,
    "PARSE-JSON": DP_FUNC_PARSE_JSON,
    "MAKE-XML": DP_FUNC_MAKE_XML,
    "PARSE-XML": DP_FUNC_PARSE_XML,
    "ENCODE-URL": DP_FUNC_ENCODE_URL,
    "DECODE-URL": DP_FUNC_DECODE_URL,
    "TERNARY": DP_OP_TERNARY,
};

#! Generic operator info hash as returned by generic dp_op_* helper functions
public hashdecl SearchOperatorInfo {
    string op;   #!< the operator string code
    auto arg;    #!< optional argument
    int op_args = 1;
}

#! a hash of valid operators for use in generic search criteria
public const DefaultGenericSearchOpMap = {
    DP_SEARCH_OP_REGEX: (
        "code": bool sub (hash<auto> cmd, auto field_value, string field_name, hash<auto> rec) {
            return regex(field_value.toString(), cmd.arg[0], cmd.arg[1]);
        },
        "op_args": 2,
    ),
    DP_SEARCH_OP_LT: (
        "code": bool sub (hash<auto> cmd, auto field_value, string field_name, hash<auto> rec) {
            return field_value < cmd.arg;
        },
    ),
    DP_SEARCH_OP_LE: (
        "code": bool sub (hash<auto> cmd, auto field_value, string field_name, hash<auto> rec) {
            return field_value <= cmd.arg;
        },
    ),
    DP_SEARCH_OP_GT: (
        "code": bool sub (hash<auto> cmd, auto field_value, string field_name, hash<auto> rec) {
            return field_value > cmd.arg;
        },
    ),
    DP_SEARCH_OP_GE: (
        "code": bool sub (hash<auto> cmd, auto field_value, string field_name, hash<auto> rec) {
            return field_value >= cmd.arg;
        },
    ),
    DP_SEARCH_OP_NE: (
        "code": bool sub (hash<auto> cmd, auto field_value, string field_name, hash<auto> rec) {
            return field_value != cmd.arg;
        },
    ),
    DP_SEARCH_OP_EQ: (
        "code": bool sub (hash<auto> cmd, auto field_value, string field_name, hash<auto> rec) {
            return field_value == cmd.arg;
        },
    ),
    DP_SEARCH_OP_BETWEEN: (
        "code": bool sub (hash<auto> cmd, auto field_value, string field_name, hash<auto> rec) {
            return field_value >= cmd.arg[0] && field_value <= cmd.arg[1];
        },
        "op_args": 2,
    ),
    DP_SEARCH_OP_IN: (
        "code": bool sub (hash<auto> cmd, auto field_value, string field_name, hash<auto> rec) {
            return cmd.arg ? (cmd.arg{field_value} ?? False) : !field_value.val();
        },
    ),
    DP_SEARCH_OP_NOT: (
        "recursive": True,
        "code": bool sub (hash<auto> cmd, auto field_value, string field_name, hash<auto> rec) {
            return !field_value;
        },
    ),
};

/** @defgroup dataprovider_generic_search_op_funcs DataProvider Generic Search Operator Functions
    These are the generic operators that can be used for searching using the generic data provider search APIs:
    - dp_op_between(): for the \c "between" operator
    - dp_op_in(): for the \c "in" operator
    - dp_op_like(): for the \c "like" operator
    - dp_op_ge(): for the \c ">=" operator when comparing field values to immediate values
    - dp_op_gt(): for the \c ">" operator when comparing field values to immediate values
    - dp_op_le(): for the \c "<=" operator when comparing field values to immediate values
    - dp_op_lt(): for the \c "<" operator when comparing field values to immediate values
    - dp_op_ne(): for the \c "!=" or \c "<>" operator when comparing field values to immediate values
    - dp_op_eq(): for the \c "=" operator when comparing field values to immediate values
    - dp_op_not(): for negating other operators
    - dp_op_regex(): for the regular expression matches
*/
#/@{
#! returns an @ref SearchOperatorInfo hash
public hash<SearchOperatorInfo> sub dp_make_op(string op, auto arg, *int op_args) {
    int op_args_int = exists op_args ? op_args : (DefaultGenericSearchOpMap{op}.op_args ?? 1);
    return new hash<SearchOperatorInfo>({
        "op": op,
        "arg": arg,
        "op_args": op_args_int,
    });
}

#! returns an @ref SearchOperatorInfo hash for regular expression matches
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = dp.searchRecords({"name": dp_op_regex("%Smith%")});
    @endcode

    @param str the argument for the operator

    @return an @ref SearchOperatorInfo hash for use in generic search criteria
*/
public hash<SearchOperatorInfo> sub dp_op_regex(string str) {
    return dp_make_op(DP_SEARCH_OP_REGEX, str);
}

#! returns an @ref SearchOperatorInfo hash for the SQL-like \c "like" operator with the given argument
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = dp.searchRecords({"name": dp_op_like("%Smith%")});
    @endcode

    @param str the argument for the operator

    @return an @ref SearchOperatorInfo hash for use in generic search criteria
*/
public hash<SearchOperatorInfo> sub dp_op_like(string str) {
    # translate str to a regex: replace % -> .*
    str =~ s/%/.*/g;
    return dp_make_op(DP_SEARCH_OP_REGEX, "^" + str + "$");
}

#! returns an @ref SearchOperatorInfo hash for the \c "<" operator with the given argument
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = dp.searchRecords({"name": dp_op_lt("Zebra")});
    @endcode

    @param arg the argument for the operator

    @return an @ref SearchOperatorInfo hash for use in generic search criteria
*/
public hash<SearchOperatorInfo> sub dp_op_lt(auto arg) {
    return dp_make_op(DP_SEARCH_OP_LT, arg);
}

#! returns an @ref SearchOperatorInfo hash for the \c "<=" operator with the given argument
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = dp.searchRecords({"name": dp_op_le("Zebra")});
    @endcode

    @param arg the argument for the operator

    @return an @ref SearchOperatorInfo hash for use in generic search criteria
*/
public hash<SearchOperatorInfo> sub dp_op_le(auto arg) {
    return dp_make_op(DP_SEARCH_OP_LE, arg);
}

#! returns an @ref SearchOperatorInfo hash for the \c ">" operator with the given argument
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = dp.searchRecords({"name": dp_op_gt("Apple")});
    @endcode

    @param arg the argument for the operator

    @return an @ref SearchOperatorInfo hash for use in generic search criteria
*/
public hash<SearchOperatorInfo> sub dp_op_gt(auto arg) {
    return dp_make_op(DP_SEARCH_OP_GT, arg);
}

#! returns an @ref SearchOperatorInfo hash for the \c ">=" operator with the given argument
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = dp.searchRecords({"name": dp_op_ge("Apple")});
    @endcode

    @param arg the argument for the operator

    @return an @ref SearchOperatorInfo hash for use in generic search criteria
*/
public hash<SearchOperatorInfo> sub dp_op_ge(auto arg) {
    return dp_make_op(DP_SEARCH_OP_GE, arg);
}

#! returns an @ref SearchOperatorInfo hash for the \c "!=" or \c "<>" operator with the given argument
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = dp.searchRecords({"name": dp_op_ne("Smith")});
    @endcode

    @param arg the argument for the operator

    @return an @ref SearchOperatorInfo hash for use in generic search criteria
*/
public hash<SearchOperatorInfo> sub dp_op_ne(auto arg) {
    return dp_make_op(DP_SEARCH_OP_NE, arg);
}

#! returns an @ref SearchOperatorInfo hash for the \c "=" operator with the given argument
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = dp.searchRecords({"name": dp_op_eq("Smith")});
    @endcode

    @param arg the argument for the operator

    @return an @ref SearchOperatorInfo hash for use in generic search criteria
*/
public hash<SearchOperatorInfo> sub dp_op_eq(auto arg) {
    return dp_make_op(DP_SEARCH_OP_EQ, arg);
}

#! returns an @ref SearchOperatorInfo hash for the \c "between" operator with the given arguments
/** Neither of which can be @ref null or @ref nothing

    @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = dp.searchRecords({"name": dp_op_between("US", "UX")});
    @endcode

    @param l the lower bound for the \c "between" operator
    @param r the upper bound for the \c "between" operator

    @return an @ref SearchOperatorInfo hash for use in generic search criteria
*/
public hash<SearchOperatorInfo> sub dp_op_between(auto l, auto r) {
    return dp_make_op(DP_SEARCH_OP_BETWEEN, (l, r));
}

#! returns an @ref SearchOperatorInfo hash for the \c "in" operator with all arguments passed to the function
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = dp.searchRecords({"name": dp_op_in(200, 300, 500, 9)});
    @endcode

    @return an @ref SearchOperatorInfo hash for use in generic search criteria
*/
public hash<SearchOperatorInfo> sub dp_op_in() {
    return dp_make_op(DP_SEARCH_OP_IN, argv);
}

#! returns an @ref SearchOperatorInfo hash for the \c "in" operator with the given argument list as the first argument
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = dp.searchRecords({"name": dp_op_in(name_list)});
    @endcode

    @param args a list of values for the \c "in" operator

    @return an @ref SearchOperatorInfo hash for use in generic search criteria
*/
public hash<SearchOperatorInfo> sub dp_op_in(list<auto> args) {
    return dp_make_op(DP_SEARCH_OP_IN, args);
}

#! returns an @ref SearchOperatorInfo hash for the \c "not" operator
/** @par Example:
    @code{.py}
AbstractDataProviderRecordIterator i = dp.searchRecords({"name": dp_op_not(dp_op_in(200, 300, 500, 9))});
    @endcode

    @return an @ref SearchOperatorInfo hash for use in generic search criteria
*/
public hash<SearchOperatorInfo> sub dp_op_not(hash<auto> arg) {
    return dp_make_op(DP_SEARCH_OP_NOT, arg);
}
#/@}

#! Defines the abstract class for data provider iterators; the destructor releases the iterator
/** this class also implements generic search APIs that can be used to filter record sets for data providers that do
    not support native search / filtering APIs
*/
public class AbstractDataProviderRecordIterator inherits AbstractIterator {
    public {
        #! Default block size
        const DefaultBlockSize = 1000;
    }

    private {
        #! Number of records returned
        int record_count = 0;

        #! Requires result
        bool requires_result;

        #! Max records
        *int max_records;

        #! Record limit
        *int limit;
    }

    #! Creates the object and optionally sets the bulk iteration block size
    constructor(*bool requires_result, *int max_records, *int limit) {
        self.requires_result = requires_result ?? False;
        self.max_records = max_records;
        self.limit = limit;
    }

    #! Returns the number of records returned so far
    int getRecordCount() {
        return record_count;
    }

    #! Returns the 'requires_result' flag
    bool requiresResult() {
        return requires_result;
    }

    #! Returns the 'max_records' value
    *int getMaxRecords() {
        return max_records;
    }

    #! Returns the 'limit' value
    *int getLimit() {
        return limit;
    }

    #! Returns @ref True if the iterator supports bulk operation
    /** @return @ref True if the iterator supports bulk operation

        @note This base class method returns @ref True by default and simulates a bulk iterator
    */
    bool supportsBulkApi() {
        return True;
    }

    #! Returns the bulk data interface if supported
    /** @return the bulk data interface if supported

        @note This base class method returns @ref DefaultBulkRecordInterface by default
    */
    *AbstractDataProviderBulkRecordInterface getBulkApi(int block_size = DefaultBlockSize) {
        return new DefaultBulkRecordInterface(block_size, self, requires_result, max_records, limit);
    }

    #! Returns the record description, if available
    /** This base class method returns @ref nothing; reimplement in subclasses to provide a record type
    */
    *hash<string, AbstractDataField> getRecordType() {
    }

    #! Positions the iterator on the next record if possible, returns @ref True if successful
    /** @return @ref True if the iterator was advanced to the next record, @ref False if no more records are available

        @note Calling getValue() after this method returns @ref False results in an exception

        @throw NO-DATA no data available to return and \c requires_result is set
        @throw MAX-RECORDS-EXCEEDED the maximum number of records has been exceeded as set by \c max_records
    */
    bool next() {
        bool rv = nextImpl();
        if (rv) {
            ++record_count;
            if ((max_records > 0) && (record_count > max_records)) {
                throw "MAX-RECORDS-EXCEEDED", sprintf("The maximum number of records (max_records = %d) has been "
                    "exceeded", max_records);
            }
            if (limit && (record_count > limit)) {
                rv = False;
            }
        } else if (!record_count && requires_result) {
            throw "NO-DATA", "No records returrned from the search, but at least one record was required "
                "('requires_result' = True)";
        }
        return rv;
    }

    #! returns a single record if the iterator is valid
    /** @throw INVALID-ITERATOR the iterator is not pointing at a valid element
    */
    abstract hash<auto> getValue();

    #! Positions the iterator on the next record if possible, returns @ref True if successful
    /** @return @ref True if the iterator was advanced to the next record, @ref False if no more records are available
    */
    abstract private bool nextImpl();

    #! Returns the value of the given field in the current record, if the iterator is valid
    /** @param key the name of the field

        @return the value of the given field in the current record, if the iterator is valid

        @throw FIELD-ERROR invalid or unknown field name
    */
    private auto doMemberGate(string key) {
        hash<auto> record = getValue();
        auto rv = record{key};
        if (!exists rv && !record.hasKey(key)) {
            throw "FIELD-ERROR", sprintf("the current record does not have field %y; valid fields: %y", key,
                keys record);
        }
        return rv;
    }

    #! Checks if the current record matches the search criteria
    /** in case of a hash value, if the hash was created by a generic dp_op_* helper function, then that search
        operation is applied to
        the input, otherwise a recursive partial match with only the keys given in the value to be checked is
        performed
    */
    private static bool matchGeneric(hash<auto> record, hash<DataProviderExpression> where_cond,
            *hash<auto> template_context) {
        return AbstractDataProvider::evalGenericExpression(record, where_cond, template_context);
    }

    #! Checks if the current record matches the search criteria
    /** in case of a hash value, if the hash was created by a generic dp_op_* helper function, then that search
        operation is applied to
        the input, otherwise a recursive partial match with only the keys given in the value to be checked is
        performed
    */
    private static bool matchGeneric(hash<auto> record, *hash<auto> where_cond,
            *hash<auto> template_context) {
        foreach hash<auto> elem in (where_cond.pairIterator()) {
            # Template references and other typed hashdecls are matched directly as values
            if (!(elem.value instanceof hash<DataProviderTemplateReference>)
                && elem.value.op && (*hash<auto> op = (DefaultGenericSearchOpMap{elem.value.op}
                    ?? DefaultGenericSearchOpMap{OperatorAliasMap{elem.value.op}}))) {
                if (!AbstractDataProviderRecordIterator::evalOperator(elem.key, elem.value, op, record,
                        template_context)) {
                    return False;
                }
            } else if (!AbstractDataProviderRecordIterator::matchGenericValue(record{elem.key}, elem.value,
                    template_context)) {
                return False;
            }
        }
        return True;
    }

    #! Evaluates a generic search operator on the field value and record and returns the result
    private static bool evalOperator(string field, hash<auto> cmd, hash<auto> op, hash<auto> record,
            *hash<auto> template_context) {
        auto field_value = record{field};
        if (op.recursive && cmd.arg.op
            && (*hash<auto> op0 = (DefaultGenericSearchOpMap{cmd.arg.op}
                ?? DefaultGenericSearchOpMap{OperatorAliasMap{cmd.arg.op}}))) {
            field_value = AbstractDataProviderRecordIterator::evalOperator(field, cmd.arg, op0, record,
                template_context);
        }
        # Resolve template references in the operator argument
        hash<auto> resolved_cmd = cmd;
        resolved_cmd.arg = AbstractDataProviderRecordIterator::resolveTemplateValue(cmd.arg, template_context);
        return op.code(resolved_cmd, field_value, field, record);
    }

    #! Resolves template references in a value
    /** Recursively resolves any @ref DataProviderTemplateReference values found in the input.
        For lists, each element is resolved individually.

        @param val the value to resolve
        @param template_context optional context hash passed through to template callbacks

        @return the resolved value

        @since DataProvider 3.3
    */
    private static auto resolveTemplateValue(auto val, *hash<auto> template_context) {
        if (val instanceof hash<DataProviderTemplateReference>) {
            return AbstractDataProvider::evalGenericExpressionValue({}, val, template_context);
        }
        if (val.typeCode() == NT_LIST) {
            return map AbstractDataProviderRecordIterator::resolveTemplateValue($1, template_context), val;
        }
        return val;
    }

    #! Match a single value
    /** in case of a hash value, a recursive partial match with only the keys given in the value to be checked is
        performed
    */
    private static bool matchGenericValue(auto expects, auto val, *hash<auto> template_context) {
        # Resolve template references in the value
        if (val instanceof hash<DataProviderTemplateReference>) {
            val = AbstractDataProvider::evalGenericExpressionValue({}, val, template_context);
        }
        if (val.typeCode() == NT_HASH) {
            # in case of a hash, do a recursive partial match with only the given keys
            foreach hash<auto> i in (val.pairIterator()) {
                if (!AbstractDataProviderRecordIterator::matchGenericValue(expects{i.key}, i.value,
                        template_context)) {
                    return False;
                }
            }
            return True;
        }
        return expects == val;
    }
}
}
