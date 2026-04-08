# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DataProvider expression implementations

/** DataProviderExpressions.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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

%modern

#! contains all public definitions in the DataProvider module
public namespace DataProvider {

/** @defgroup dataprovider_extended_expressions DataProvider Extended Expression Implementations

    Extended expression implementations for data manipulation, comparison, conversion, and more.

    These expressions are generic and have no external module dependencies (except for serialization
    functions which require the yaml, json, and xml modules respectively).

    @since DataProvider 3.3
*/

#! Eval helper: evaluates a single expression argument
/** @since DataProvider 3.3
*/
auto sub dp_eval_arg(hash<auto> rec, hash<DataProviderExpression> exp, int index,
        *hash<auto> template_context) {
    return AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[index], template_context);
}

#! Extended expression implementations for data providers
/** @since DataProvider 3.3
*/
public const ExtendedExpressionImplementations = {
    # --- Comparison extras ---
    DP_OP_ABSOLUTE_EQ: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_ABSOLUTE_EQ,
            "display_name": "absolute equals (===)",
            "short_desc": "Returns True if both type and value are equal",
            "desc": "Returns `True` if both arguments have the same type and the same value; no type "
                "conversions are performed",
            "symbol": "===",
            "groups": "Comparison",
            "args": (DataProviderSignatureAnyType, DataProviderSignatureAnyType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "A value to compare",
                    "desc": "A value to compare; types and values must be identical",
                    "label_after": "===",
                },
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "A value to compare",
                    "desc": "A value to compare; types and values must be identical",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return dp_eval_arg(rec, exp, 0, tc) === dp_eval_arg(rec, exp, 1, tc);
        },
    },
    DP_OP_ABSOLUTE_NE: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_ABSOLUTE_NE,
            "display_name": "absolute not equals (!==)",
            "short_desc": "Returns True if type or value differ",
            "desc": "Returns `True` if the arguments differ in type or value; no type conversions are performed",
            "symbol": "!==",
            "groups": "Comparison",
            "args": (DataProviderSignatureAnyType, DataProviderSignatureAnyType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "A value to compare",
                    "desc": "A value to compare; expression returns True if types or values differ",
                    "label_after": "!==",
                },
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "A value to compare",
                    "desc": "A value to compare; expression returns True if types or values differ",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return dp_eval_arg(rec, exp, 0, tc) !== dp_eval_arg(rec, exp, 1, tc);
        },
    },
    DP_OP_RANGE: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_RANGE,
            "display_name": "in range",
            "short_desc": "Returns True if the first argument is >= second and < third",
            "desc": "Returns `True` if the first argument is greater than or equal to the second and strictly "
                "less than the third",
            "symbol": "inRange",
            "render_template": "$arg[0] inRange $arg[1] and $arg[2]",
            "groups": "Comparison",
            "args": (DataProviderSignatureAnyType, DataProviderSignatureAnyType, DataProviderSignatureAnyType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to compare",
                    "desc": "The value to compare",
                    "label_after": "in range of",
                },
                <DataProviderArgInfo>{
                    "display_name": "Lower Bound",
                    "short_desc": "The lower bound (inclusive)",
                    "desc": "The lower bound; the value must be greater than or equal to this",
                    "label_after": "and",
                },
                <DataProviderArgInfo>{
                    "display_name": "Upper Bound",
                    "short_desc": "The upper bound (exclusive)",
                    "desc": "The upper bound; the value must be strictly less than this",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            auto arg0 = dp_eval_arg(rec, exp, 0, tc);
            return arg0 >= dp_eval_arg(rec, exp, 1, tc)
                && arg0 < dp_eval_arg(rec, exp, 2, tc);
        },
    },
    DP_OP_HAS_VALUE: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_HAS_VALUE,
            "display_name": "has value",
            "short_desc": "Returns True if the argument has a truthy value",
            "desc": "Returns `True` if the argument has a value that can be converted to boolean `True`",
            "symbol": "val",
            "render_template": "$arg[0].val()",
            "groups": "Comparison",
            "args": (DataProviderSignatureAnyType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value To Check",
                    "short_desc": "The value to check",
                    "desc": "The value to check as a boolean type",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return dp_eval_arg(rec, exp, 0, tc).val();
        },
    },

    # --- Arithmetic ---
    DP_OP_PLUS: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_PLUS,
            "display_name": "addition (+)",
            "short_desc": "Adds values and returns the result",
            "desc": "The plus (+) operator adds values and returns the result; works with numbers, strings, "
                "dates, and lists",
            "symbol": "+",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureAnyType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value To Add",
                    "short_desc": "A value to add",
                    "desc": "A value to add",
                },
            ),
            "varargs": True,
            "min_args": 2,
            "return_type": AbstractDataProviderTypeMap."any",
            "return_type_arg_priority": ("list", "string", "date", "number", "float", "int", "hash", "binary"),
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return foldl $1 + $2,
                (map AbstractDataProvider::evalGenericExpressionValue(rec, $1, tc), exp.args);
        },
    },
    DP_OP_MINUS: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_MINUS,
            "display_name": "subtraction (-)",
            "short_desc": "Subtracts the second value from the first",
            "desc": "The minus (-) operator subtracts one value from another and returns the result",
            "symbol": "-",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureAnyType, DataProviderSignatureAnyType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to subtract from",
                    "desc": "The value to subtract from (minuend)",
                    "label_after": "-",
                },
                <DataProviderArgInfo>{
                    "display_name": "Subtrahend",
                    "short_desc": "The value to subtract",
                    "desc": "The value to subtract (subtrahend)",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."any",
            "return_type_first_arg": True,
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return dp_eval_arg(rec, exp, 0, tc) - dp_eval_arg(rec, exp, 1, tc);
        },
    },
    DP_OP_MULT: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_MULT,
            "display_name": "multiplication (*)",
            "short_desc": "Multiplies numbers and returns the result",
            "desc": "The multiply (*) operator multiplies numbers and returns the result",
            "symbol": "*",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureNumberType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value To Multiply",
                    "short_desc": "A value to multiply",
                    "desc": "A value to multiply",
                },
            ),
            "varargs": True,
            "min_args": 2,
            "return_type": AbstractDataProviderTypeMap."any",
            "return_type_arg_priority": ("number", "float", "int"),
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return foldl $1 * $2,
                (map AbstractDataProvider::evalGenericExpressionValue(rec, $1, tc), exp.args);
        },
    },
    DP_OP_DIV: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_DIV,
            "display_name": "division (/)",
            "short_desc": "Divides the first number by the second",
            "desc": "The division (/) operator divides the first number by the second and returns the result.\n\n"
                "A `DIVISION-BY-ZERO` exception is thrown if the second argument is zero",
            "symbol": "/",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureNumberType, DataProviderSignatureNumberType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Dividend",
                    "short_desc": "The value to divide",
                    "desc": "The value to divide (dividend)",
                    "label_after": "/",
                },
                <DataProviderArgInfo>{
                    "display_name": "Divisor",
                    "short_desc": "The divisor",
                    "desc": "The divisor",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."any",
            "return_type_arg_priority": ("number", "float", "int"),
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return dp_eval_arg(rec, exp, 0, tc) / dp_eval_arg(rec, exp, 1, tc);
        },
    },

    # --- Ternary ---
    DP_OP_TERNARY: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_TERNARY,
            "display_name": "ternary (?:)",
            "short_desc": "Conditional expression: returns second arg if first is true, third otherwise",
            "desc": "Evaluates the first argument as a boolean; if true, returns the second argument; "
                "otherwise returns the third argument",
            "symbol": "?:",
            "render_template": "$arg[0] ? $arg[1] : $arg[2]",
            "groups": "Logical",
            "args": (DataProviderSignatureBoolType, DataProviderSignatureAnyType, DataProviderSignatureAnyType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Condition",
                    "short_desc": "The boolean condition to evaluate",
                    "desc": "The boolean condition to evaluate",
                    "label_after": "?",
                },
                <DataProviderArgInfo>{
                    "display_name": "True Value",
                    "short_desc": "The value to return if the condition is true",
                    "desc": "The value to return if the condition is true",
                    "label_after": ":",
                },
                <DataProviderArgInfo>{
                    "display_name": "False Value",
                    "short_desc": "The value to return if the condition is false",
                    "desc": "The value to return if the condition is false",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."any",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return dp_eval_arg(rec, exp, 0, tc) ? dp_eval_arg(rec, exp, 1, tc) : dp_eval_arg(rec, exp, 2, tc);
        },
    },

    # --- String operations ---
    DP_OP_STARTS_WITH: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_STARTS_WITH,
            "display_name": "starts with",
            "short_desc": "Returns True if the string starts with the given characters",
            "desc": "Returns `True` if the given value starts with the given characters when converted to a string",
            "symbol": "startsWith",
            "render_template": "$arg[0].startsWith($arg[1], $arg[2])",
            "groups": "Comparison",
            "args": (DataProviderSignatureAnyType, DataProviderSignatureAnyType, DataProviderSignatureBoolType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String Value",
                    "short_desc": "The string value to check",
                    "desc": "The string value to check",
                    "label_after": "starts with",
                },
                <DataProviderArgInfo>{
                    "display_name": "Start String",
                    "short_desc": "The characters to check at the beginning",
                    "desc": "The characters to check at the beginning of the first argument",
                },
                <DataProviderArgInfo>{
                    "display_name": "Ignore Case?",
                    "short_desc": "If True then case is ignored (default: True)",
                    "desc": "If `True` then case will be ignored when checking the string; default: `True`",
                    "default_value": True,
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            softstring str = dp_eval_arg(rec, exp, 0, tc);
            softstring chars = dp_eval_arg(rec, exp, 1, tc);
            if (dp_eval_arg(rec, exp, 2, tc)) {
                str = str.upr();
                chars = chars.upr();
            }
            return str.startsWith(chars);
        },
    },
    DP_OP_ENDS_WITH: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_ENDS_WITH,
            "display_name": "ends with",
            "short_desc": "Returns True if the string ends with the given characters",
            "desc": "Returns `True` if the given string ends with the given characters",
            "symbol": "endsWith",
            "render_template": "$arg[0].endsWith($arg[1], $arg[2])",
            "groups": "Comparison",
            "args": (DataProviderSignatureAnyType, DataProviderSignatureAnyType, DataProviderSignatureBoolType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String Value",
                    "short_desc": "The string value to check",
                    "desc": "The string value to check",
                    "label_after": "ends with",
                },
                <DataProviderArgInfo>{
                    "display_name": "End String",
                    "short_desc": "The characters to check at the end",
                    "desc": "The characters to check at the end of the first argument",
                },
                <DataProviderArgInfo>{
                    "display_name": "Ignore Case?",
                    "short_desc": "If True then case is ignored (default: True)",
                    "desc": "If `True` then case will be ignored; default: `True`",
                    "default_value": True,
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            softstring str = dp_eval_arg(rec, exp, 0, tc);
            softstring chars = dp_eval_arg(rec, exp, 1, tc);
            if (dp_eval_arg(rec, exp, 2, tc)) {
                str = str.upr();
                chars = chars.upr();
            }
            return str.endsWith(chars);
        },
    },
    DP_OP_CONTAINS: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_CONTAINS,
            "display_name": "contains",
            "short_desc": "Returns True if the string contains the given substring",
            "desc": "Returns `True` if the given string contains the given characters",
            "symbol": "contains",
            "render_template": "$arg[0] contains $arg[1]",
            "groups": "Comparison",
            "args": (DataProviderSignatureStringType, DataProviderSignatureStringType,
                DataProviderSignatureBoolType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String Value",
                    "short_desc": "The string value to check",
                    "desc": "The string value to check",
                    "label_after": "contains",
                },
                <DataProviderArgInfo>{
                    "display_name": "Substring",
                    "short_desc": "The substring to search for",
                    "desc": "The characters to check if they are anywhere in the first argument",
                },
                <DataProviderArgInfo>{
                    "display_name": "Ignore Case?",
                    "short_desc": "If True then case is ignored (default: True)",
                    "desc": "If `True` then case will be ignored; default: `True`",
                    "default_value": True,
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            softstring str = dp_eval_arg(rec, exp, 0, tc);
            softstring chars = dp_eval_arg(rec, exp, 1, tc);
            if (dp_eval_arg(rec, exp, 2, tc)) {
                str = str.upr();
                chars = chars.upr();
            }
            return str.contains(chars);
        },
    },
    DP_OP_LIST_CONTAINS: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_LIST_CONTAINS,
            "display_name": "list contains",
            "short_desc": "Returns True if the list contains the given element",
            "desc": "Returns `True` if the given list contains the given element",
            "symbol": "listContains",
            "render_template": "$arg[0] list-contains $arg[1]",
            "groups": "Comparison",
            "args": (DataProviderSignatureListType, DataProviderSignatureAnyType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "List Value",
                    "short_desc": "The list to check",
                    "desc": "The list value to check",
                    "label_after": "contains",
                },
                <DataProviderArgInfo>{
                    "display_name": "Element",
                    "short_desc": "The element to search for",
                    "desc": "The element to check if it is in the list",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            softlist<auto> l = dp_eval_arg(rec, exp, 0, tc);
            auto elem = dp_eval_arg(rec, exp, 1, tc);
            return inlist(elem, l);
        },
    },
    DP_OP_CONTAINS_ANY: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_CONTAINS_ANY,
            "display_name": "contains any",
            "short_desc": "Returns True if the string contains any of the given substrings",
            "desc": "Returns `True` if the given string contains any of the given strings",
            "symbol": "containsAny",
            "render_template": "$arg[0] contains-any $arg[1]",
            "groups": "Comparison",
            "args": (DataProviderSignatureStringType, DataProviderSignatureListType,
                DataProviderSignatureBoolType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String Value",
                    "short_desc": "The string value to check",
                    "desc": "The string value to check",
                    "label_after": "contains any of",
                },
                <DataProviderArgInfo>{
                    "display_name": "Strings",
                    "short_desc": "The substrings to search for",
                    "desc": "The strings to check if at least one is in the first argument",
                },
                <DataProviderArgInfo>{
                    "display_name": "Ignore Case?",
                    "short_desc": "If True then case is ignored (default: True)",
                    "desc": "If `True` then case will be ignored; default: `True`",
                    "default_value": True,
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            string str = dp_eval_arg(rec, exp, 0, tc);
            *list<auto> elems = dp_eval_arg(rec, exp, 1, tc);
            if (dp_eval_arg(rec, exp, 2, tc)) {
                str = str.upr();
                elems = map $1.upr(), elems;
            }
            foreach string chars in (elems) {
                if (str.contains(chars)) {
                    return True;
                }
            }
            return False;
        },
    },
    DP_OP_LIST_CONTAINS_ANY: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_LIST_CONTAINS_ANY,
            "display_name": "list contains any",
            "short_desc": "Returns True if the list contains any of the given values",
            "desc": "Returns `True` if the given list contains any of the given elements",
            "symbol": "listContainsAny",
            "render_template": "$arg[0] list-contains-any $arg[1]",
            "groups": "Comparison",
            "args": (DataProviderSignatureListType, DataProviderSignatureListType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "List Value",
                    "short_desc": "The list to check",
                    "desc": "The list value to check",
                    "label_after": "contains any of",
                },
                <DataProviderArgInfo>{
                    "display_name": "Values",
                    "short_desc": "The values to search for",
                    "desc": "The values to check if at least one is in the first list",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            softlist<auto> l = dp_eval_arg(rec, exp, 0, tc);
            *list<auto> vals = dp_eval_arg(rec, exp, 1, tc);
            foreach auto v in (vals) {
                if (l.contains(v)) {
                    return True;
                }
            }
            return False;
        },
    },
    DP_OP_FIND_IN: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_FIND_IN,
            "display_name": "find in",
            "short_desc": "Finds a value in a list/hash by dot-notation path",
            "desc": "Searches for a value in a list; the optional third argument provides a dot-notation path "
                "into list elements for comparison. Returns the matching element or `NOTHING`",
            "symbol": "findIn",
            "render_template": "findIn($arg[0], $arg[1], $arg[2])",
            "groups": "Comparison",
            "args": (DataProviderSignatureAnyType, DataProviderSignatureListType,
                DataProviderSignatureOptionalStringType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to search for",
                    "desc": "The value to search for",
                },
                <DataProviderArgInfo>{
                    "display_name": "List",
                    "short_desc": "The list to search in",
                    "desc": "The list to search in",
                },
                <DataProviderArgInfo>{
                    "display_name": "Value Location",
                    "short_desc": "Dot-notation path to the value in list elements",
                    "desc": "The location of the value in the list elements in dot notation",
                    "example_value": "account.id",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."any",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            auto val = dp_eval_arg(rec, exp, 0, tc);
            softlist<auto> l = dp_eval_arg(rec, exp, 1, tc);
            if (exp.args[2].val()) {
                string loc = dp_eval_arg(rec, exp, 2, tc);
                list<string> path = loc.split(".");
                foreach auto e in (l) {
                    auto v = e;
                    foreach string p in (path) {
                        if (v.typeCode() == NT_HASH) {
                            v = v{p};
                        } else {
                            v = NOTHING;
                            break;
                        }
                    }
                    if (v == val) {
                        return e;
                    }
                }
                return NOTHING;
            }
            return inlist(val, l) ? val : NOTHING;
        },
    },

    # --- Like operator ---
    DP_OP_LIKE: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_LIKE,
            "display_name": "like",
            "short_desc": "Returns True if the string matches the SQL LIKE pattern",
            "desc": "Returns `True` if the first argument matches the second using SQL LIKE pattern matching. "
                "`%` matches any sequence of characters, `_` matches any single character. "
                "An optional third argument controls case sensitivity (default: case-sensitive)",
            "symbol": "like",
            "render_template": "$arg[0] like $arg[1]",
            "groups": "Comparison",
            "args": (DataProviderSignatureStringType, DataProviderSignatureStringType,
                DataProviderSignatureOptionalBoolType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String Value",
                    "short_desc": "The string to match",
                    "desc": "The string value to match against the pattern",
                    "label_after": "like",
                },
                <DataProviderArgInfo>{
                    "display_name": "Pattern",
                    "short_desc": "LIKE pattern (% and _ wildcards)",
                    "desc": "The LIKE pattern; `%` matches any sequence of characters, `_` matches any "
                        "single character",
                    "example_value": "%example%",
                },
                <DataProviderArgInfo>{
                    "display_name": "Ignore Case?",
                    "short_desc": "If True then case is ignored (default: False)",
                    "desc": "If `True` then case will be ignored; default: `False`",
                    "default_value": False,
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            string str = dp_eval_arg(rec, exp, 0, tc);
            string pattern = dp_eval_arg(rec, exp, 1, tc);
            # Convert LIKE pattern to regex: escape regex metacharacters, then convert % and _
            # First escape all regex-special chars except % and _
            string re = regex_subst(pattern, "([.^$+?{}\\[\\]|()\\\\])", "\\\\$1", RE_Global);
            # Convert LIKE wildcards to regex equivalents
            re = regex_subst(re, "%", ".*", RE_Global);
            re = regex_subst(re, "_", ".", RE_Global);
            # Anchor the pattern
            re = "^" + re + "$";
            int opts;
            if (dp_eval_arg(rec, exp, 2, tc)) {
                opts = RE_Caseless;
            }
            return regex(str, re, opts);
        },
    },

    # --- Regex ---
    DP_OP_REGEX_MATCH: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_REGEX_MATCH,
            "display_name": "regex match (=~)",
            "short_desc": "Returns True if the string matches the regular expression",
            "desc": "Returns `True` if the first argument matches the second as a regular expression pattern",
            "symbol": "=~",
            "render_template": "$arg[0] =~ /$arg[1]/$arg[2]",
            "groups": "Comparison",
            "args": (DataProviderSignatureStringType, DataProviderSignatureStringType,
                DataProviderSignatureOptionalStringListValueType),
            "arg_info": RegularExpressionArgInfo,
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            string str = dp_eval_arg(rec, exp, 0, tc);
            string pattern = dp_eval_arg(rec, exp, 1, tc);
            int opts;
            if (*list<auto> options = dp_eval_arg(rec, exp, 2, tc)) {
                opts = foldl $1 | $2, (map RegexOpts{$1}, options);
            }
            return regex(str, pattern, opts);
        },
    },
    DP_OP_REGEX_NMATCH: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_REGEX_NMATCH,
            "display_name": "regex non-match (!~)",
            "short_desc": "Returns True if the string does not match the regular expression",
            "desc": "Returns `True` if the first argument does not match the second as a regular expression",
            "symbol": "!~",
            "render_template": "$arg[0] !~ /$arg[1]/$arg[2]",
            "groups": "Comparison",
            "args": (DataProviderSignatureStringType, DataProviderSignatureStringType,
                DataProviderSignatureOptionalStringListValueType),
            "arg_info": RegularExpressionArgInfo,
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            string str = dp_eval_arg(rec, exp, 0, tc);
            string pattern = dp_eval_arg(rec, exp, 1, tc);
            int opts;
            if (*list<auto> options = dp_eval_arg(rec, exp, 2, tc)) {
                opts = foldl $1 | $2, (map RegexOpts{$1}, options);
            }
            return !regex(str, pattern, opts);
        },
    },
    DP_OP_REGEX_SUBST: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_REGEX_SUBST,
            "display_name": "regex substitution",
            "short_desc": "Performs regex substitution on a string",
            "desc": "Performs regular expression substitution on the first string using the second as the "
                "match pattern and the third as the substitution string",
            "symbol": "regexSubst",
            "render_template": "regex_subst($arg[0], /$arg[1]/, $arg[2], $arg[3])",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureStringType, DataProviderSignatureStringType,
                DataProviderSignatureStringType, DataProviderSignatureOptionalStringListValueType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String Value",
                    "short_desc": "The string to process",
                    "desc": "The string value to be processed for substitution",
                },
                <DataProviderArgInfo>{
                    "display_name": "Regular Expression",
                    "short_desc": "The regular expression to match",
                    "desc": "The regular expression pattern to match",
                },
                <DataProviderArgInfo>{
                    "display_name": "Substitution String",
                    "short_desc": "The replacement string",
                    "desc": "The replacement string; use `$1`, `$2`, etc for captured groups",
                    "label_after": "/",
                },
                <DataProviderArgInfo>{
                    "display_name": "Regex Options",
                    "short_desc": "Optional regex flags",
                    "desc": "Regular expression options",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            string str = dp_eval_arg(rec, exp, 0, tc);
            string pattern = dp_eval_arg(rec, exp, 1, tc);
            string subst = dp_eval_arg(rec, exp, 2, tc);
            int opts;
            if (*list<auto> options = dp_eval_arg(rec, exp, 3, tc)) {
                opts = foldl $1 | $2, (map RegexOpts{$1}, options);
            }
            return regex_subst(str, pattern, subst, opts);
        },
    },
    DP_OP_REPLACE: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_REPLACE,
            "display_name": "string replace",
            "short_desc": "Replaces all occurrences of a string pattern",
            "desc": "Replaces all occurrences of a pattern string with a substitution string",
            "symbol": "replace",
            "render_template": "replace($args)",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureStringType, DataProviderSignatureStringType,
                DataProviderSignatureStringType, DataProviderSignatureOptionalIntType,
                DataProviderSignatureOptionalIntType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String Value",
                    "short_desc": "The string to process",
                    "desc": "The string value to be processed for replacements",
                },
                <DataProviderArgInfo>{
                    "display_name": "Pattern",
                    "short_desc": "The pattern to replace",
                    "desc": "The pattern to be replaced",
                },
                <DataProviderArgInfo>{
                    "display_name": "Substitution",
                    "short_desc": "The replacement string",
                    "desc": "The string to substitute for the pattern",
                },
                <DataProviderArgInfo>{
                    "display_name": "Start Character",
                    "short_desc": "Start position (default: 0)",
                    "desc": "The start character position; negative values are offsets from the end; default: `0`",
                    "default_value": 0,
                },
                <DataProviderArgInfo>{
                    "display_name": "End Character",
                    "short_desc": "End position (default: -1)",
                    "desc": "The end character position; negative values are offsets from the end; default: `-1`",
                    "default_value": -1,
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            string str = dp_eval_arg(rec, exp, 0, tc);
            string pattern = dp_eval_arg(rec, exp, 1, tc);
            string subst = dp_eval_arg(rec, exp, 2, tc);
            *int start = dp_eval_arg(rec, exp, 3, tc);
            *int end = dp_eval_arg(rec, exp, 4, tc);
            return replace(str, pattern, subst, start, end);
        },
    },
    DP_OP_UNACCENT: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_UNACCENT,
            "display_name": "remove accents",
            "short_desc": "Removes accent characters from a string",
            "desc": "Returns the string with accented characters replaced by unaccented equivalents",
            "symbol": "unaccent",
            "render_template": "$arg[0].unaccent()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureStringType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String",
                    "short_desc": "The string to process",
                    "desc": "The string to remove accented characters from",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return dp_eval_arg(rec, exp, 0, tc).unaccent();
        },
    },
    DP_OP_TO_LOWER: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_TO_LOWER,
            "display_name": "to lower case",
            "short_desc": "Converts a string to lower case",
            "desc": "Returns the string converted to all lower case",
            "symbol": "lwr",
            "render_template": "$arg[0].lwr()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureStringType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String",
                    "short_desc": "The string to convert",
                    "desc": "The string to convert to lower case",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return dp_eval_arg(rec, exp, 0, tc).lwr();
        },
    },
    DP_OP_TO_UPPER: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_TO_UPPER,
            "display_name": "to upper case",
            "short_desc": "Converts a string to upper case",
            "desc": "Returns the string converted to all upper case",
            "symbol": "upr",
            "render_template": "$arg[0].upr()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureStringType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String",
                    "short_desc": "The string to convert",
                    "desc": "The string to convert to upper case",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return dp_eval_arg(rec, exp, 0, tc).upr();
        },
    },
    DP_OP_JOIN: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_JOIN,
            "display_name": "join",
            "short_desc": "Joins list elements with a separator string",
            "desc": "Returns a string made from joining all list elements with a separator string",
            "symbol": "join",
            "render_template": "$arg[0].join($arg[1])",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureListType, DataProviderSignatureStringType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "List",
                    "short_desc": "The list to join",
                    "desc": "The list whose elements will be joined",
                    "label_after": "join with",
                },
                <DataProviderArgInfo>{
                    "display_name": "Separator",
                    "short_desc": "The separator string",
                    "desc": "The string to use between elements",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            softlist<auto> l = dp_eval_arg(rec, exp, 0, tc);
            string sep = dp_eval_arg(rec, exp, 1, tc);
            return l.join(sep);
        },
    },
    DP_OP_SIZE: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_SIZE,
            "display_name": "size",
            "short_desc": "Returns the byte size or element count",
            "desc": "Returns the size of the operand; for strings returns byte count, for lists/hashes returns "
                "element count, for binary returns byte count",
            "symbol": "size",
            "render_template": "$arg[0].size()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureAnyType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to get the size of",
                    "desc": "The value to report the size for",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."int",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return dp_eval_arg(rec, exp, 0, tc).size();
        },
    },
    DP_OP_LENGTH: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_LENGTH,
            "display_name": "length",
            "short_desc": "Returns the character length or element count",
            "desc": "Returns the length of the operand; for strings returns character count (not byte count), "
                "for lists/hashes returns element count",
            "symbol": "length",
            "render_template": "$arg[0].length()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureAnyType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to get the length of",
                    "desc": "The value to report the length for",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."int",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            auto val = dp_eval_arg(rec, exp, 0, tc);
            return val.typeCode() == NT_STRING ? val.length() : val.size();
        },
    },
    DP_OP_CHAR_WIDTH: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_CHAR_WIDTH,
            "display_name": "character width",
            "short_desc": "Returns the display character width",
            "desc": "Returns the display character width of a string",
            "symbol": "width",
            "render_template": "$arg[0].width()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureStringType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String",
                    "short_desc": "The string to measure",
                    "desc": "The string to report the character width for",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."int",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return dp_eval_arg(rec, exp, 0, tc).width();
        },
    },
    DP_OP_CHAR_ENCODING: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_CHAR_ENCODING,
            "display_name": "encoding",
            "short_desc": "Returns the character encoding name",
            "desc": "Returns the character encoding name of a string",
            "symbol": "encoding",
            "render_template": "$arg[0].encoding()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureStringType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String",
                    "short_desc": "The string to check",
                    "desc": "The string to report the encoding for",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return dp_eval_arg(rec, exp, 0, tc).encoding();
        },
    },

    # --- Functions ---
    DP_FUNC_SUBSTR: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_SUBSTR,
            "display_name": "substr",
            "short_desc": "Returns a substring",
            "desc": "Returns a substring from a string",
            "symbol": "substr()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureStringType, DataProviderSignatureIntType,
                DataProviderSignatureOptionalIntType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String Value",
                    "short_desc": "The string to extract from",
                    "desc": "The string value to extract the substring from",
                },
                <DataProviderArgInfo>{
                    "display_name": "Start",
                    "short_desc": "The starting character position (0-based)",
                    "desc": "The starting position; negative values are offsets from the end",
                },
                <DataProviderArgInfo>{
                    "display_name": "Length",
                    "short_desc": "Maximum number of characters",
                    "desc": "Maximum characters to return; omit for rest of string",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            string str = dp_eval_arg(rec, exp, 0, tc);
            int start = dp_eval_arg(rec, exp, 1, tc);
            *int len = exists exp.args[2] ? dp_eval_arg(rec, exp, 2, tc) : NOTHING;
            return substr(str, start, len);
        },
    },
    DP_FUNC_FORMAT_DATE: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_FORMAT_DATE,
            "display_name": "format_date",
            "short_desc": "Formats a date as a string",
            "desc": "Formats a date value as a string using a format template",
            "symbol": "format_date",
            "groups": "Date/Time",
            "args": (DataProviderSignatureDateType, DataProviderSignatureStringType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Date To Format",
                    "short_desc": "The date/time value to format",
                    "desc": "The date/time value to format",
                },
                <DataProviderArgInfo>{
                    "display_name": "Format String",
                    "short_desc": "The format template string",
                    "desc": "The format string; common codes: `YYYY` (year), `MM` (month), `DD` (day), "
                        "`HH` (hour), `mm` (minute), `SS` (second), `us` (microseconds)",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            date d = dp_eval_arg(rec, exp, 0, tc);
            string fmt = dp_eval_arg(rec, exp, 1, tc);
            return format_date(fmt, d);
        },
    },
    DP_FUNC_FORMAT_NUMBER: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_FORMAT_NUMBER,
            "display_name": "format_number",
            "short_desc": "Formats a number as a string",
            "desc": "Formats a number as a string with configurable separators and precision",
            "symbol": "format_number()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureAnyType, DataProviderSignatureOptionalStringType,
                DataProviderSignatureStringType, DataProviderSignatureIntType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Number",
                    "short_desc": "The number to format",
                    "desc": "The number value to format",
                },
                <DataProviderArgInfo>{
                    "display_name": "Thousands Separator",
                    "short_desc": "Thousands separator (default: ',')",
                    "desc": "The thousands separator character; default: `,`",
                    "default_value": ",",
                },
                <DataProviderArgInfo>{
                    "display_name": "Decimal Separator",
                    "short_desc": "Decimal separator (default: '.')",
                    "desc": "The decimal separator character; default: `.`",
                    "default_value": ".",
                },
                <DataProviderArgInfo>{
                    "display_name": "Precision",
                    "short_desc": "Digits after decimal (default: 2)",
                    "desc": "Number of digits after the decimal point; default: `2`",
                    "default_value": 2,
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            auto n = dp_eval_arg(rec, exp, 0, tc);
            *string ts = dp_eval_arg(rec, exp, 1, tc);
            int dec = dp_eval_arg(rec, exp, 3, tc);
            string ds;
            if (dec > 0) {
                ds = dp_eval_arg(rec, exp, 2, tc);
            } else {
                ds = ".";
            }
            string fmt = sprintf("%s%s%d", ts ?? "?", ds, dec);
            return format_number(fmt, n);
        },
    },
    DP_FUNC_SPLIT_REGEX: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_SPLIT_REGEX,
            "display_name": "split_regex",
            "short_desc": "Splits a string by regex pattern",
            "desc": "Splits a string according to a regular expression pattern and returns a list of substrings",
            "symbol": "split_regex()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureStringType, DataProviderSignatureStringType,
                DataProviderSignatureOptionalStringListValueType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String To Split",
                    "short_desc": "The string to split",
                    "desc": "The string to split",
                },
                <DataProviderArgInfo>{
                    "display_name": "Pattern",
                    "short_desc": "The regular expression pattern",
                    "desc": "The regular expression pattern to split by",
                },
                <DataProviderArgInfo>{
                    "display_name": "Regex Options",
                    "short_desc": "Regular expression options",
                    "desc": "Optional regular expression options",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."list",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            string str = dp_eval_arg(rec, exp, 0, tc);
            string pattern = dp_eval_arg(rec, exp, 1, tc);
            int opts;
            if (*list<auto> options = dp_eval_arg(rec, exp, 2, tc)) {
                opts = foldl $1 | $2, (map RegexOpts{$1}, options);
            }
            return str.splitRegex(pattern, opts);
        },
    },
    DP_FUNC_SPLIT_QUOTED: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_SPLIT_QUOTED,
            "display_name": "split_quoted",
            "short_desc": "Splits a string respecting quotes",
            "desc": "Splits a string by a separator, optionally respecting quoted fields",
            "symbol": "split_quoted()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureStringType, DataProviderSignatureStringType,
                DataProviderSignatureOptionalStringType, DataProviderSignatureOptionalBoolType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String To Split",
                    "short_desc": "The string to split",
                    "desc": "The string to split",
                },
                <DataProviderArgInfo>{
                    "display_name": "Separator",
                    "short_desc": "The separator string",
                    "desc": "The separator string",
                    "example_value": ",",
                },
                <DataProviderArgInfo>{
                    "display_name": "Quote Character",
                    "short_desc": "The quote character",
                    "desc": "The quote character for quoted fields",
                    "example_value": "\"",
                },
                <DataProviderArgInfo>{
                    "display_name": "Trim Unquoted?",
                    "short_desc": "Trim whitespace from unquoted fields",
                    "desc": "If true, unquoted fields are trimmed of leading/trailing whitespace",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."list",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            string str = dp_eval_arg(rec, exp, 0, tc);
            string sep = dp_eval_arg(rec, exp, 1, tc);
            *string quote = dp_eval_arg(rec, exp, 2, tc);
            *bool trim_unquoted = dp_eval_arg(rec, exp, 3, tc);
            return str.split(sep, quote, trim_unquoted);
        },
    },

    # --- Encoding/Decoding ---
    DP_OP_TO_HEX: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_TO_HEX,
            "display_name": "to hex",
            "short_desc": "Converts data to a hex string",
            "desc": "Returns the input converted to a hex string",
            "symbol": "toHex",
            "render_template": "$arg[0].toHex()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureDataType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to convert",
                    "desc": "The string or binary value to convert to a hex string",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return dp_eval_arg(rec, exp, 0, tc).toHex();
        },
    },
    DP_OP_TO_BASE64: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_TO_BASE64,
            "display_name": "to base64",
            "short_desc": "Converts data to a base64 string",
            "desc": "Returns the input data encoded as a base64 string",
            "symbol": "toBase64",
            "render_template": "$arg[0].toBase64($arg[1])",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureDataType, DataProviderSignatureOptionalIntType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to encode",
                    "desc": "The string or binary value to encode as base64",
                },
                <DataProviderArgInfo>{
                    "display_name": "Max Line Length",
                    "short_desc": "Maximum line length for output",
                    "desc": "Maximum line length; negative or omitted means no limit",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            int mll = dp_eval_arg(rec, exp, 1, tc) ?? -1;
            return dp_eval_arg(rec, exp, 0, tc).toBase64(mll);
        },
    },
    DP_OP_TO_BASE64_URL: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_TO_BASE64_URL,
            "display_name": "to base64-url",
            "short_desc": "Converts data to base64-URL encoding (RFC 4648)",
            "desc": "Returns the input data in base64-URL encoding per RFC 4648",
            "symbol": "toBase64Url",
            "render_template": "$arg[0].toBase64Url()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureDataType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to encode",
                    "desc": "The string or binary value to encode in base64-URL encoding",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return dp_eval_arg(rec, exp, 0, tc).toBase64Url();
        },
    },
    DP_FUNC_PARSE_HEX: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_PARSE_HEX,
            "display_name": "parse_hex",
            "short_desc": "Parses a hex string to binary",
            "desc": "Parses a hex string and returns the binary value",
            "symbol": "parse_hex()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureStringType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Hex String",
                    "short_desc": "The hex string to parse",
                    "desc": "The string of hexadecimal characters to parse to binary",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."binary",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return parse_hex_string(dp_eval_arg(rec, exp, 0, tc));
        },
    },
    DP_FUNC_PARSE_BASE64: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_PARSE_BASE64,
            "display_name": "parse_base64",
            "short_desc": "Parses a base64 string to binary",
            "desc": "Parses a base64-encoded string and returns the binary value",
            "symbol": "parse_base64()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureStringType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Base64 String",
                    "short_desc": "The base64 string to parse",
                    "desc": "The base64-encoded string to parse",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."binary",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return parse_base64_string(dp_eval_arg(rec, exp, 0, tc));
        },
    },
    DP_FUNC_PARSE_BASE64_URL: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_PARSE_BASE64_URL,
            "display_name": "parse_base64_url",
            "short_desc": "Parses a base64-URL string to binary",
            "desc": "Parses a base64-URL-encoded string (RFC 4648) and returns the binary value",
            "symbol": "parse_base64_url()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureDataType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Base64-URL String",
                    "short_desc": "The base64-URL string to parse",
                    "desc": "The base64-URL-encoded string to parse",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."binary",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return parse_base64_url_string(dp_eval_arg(rec, exp, 0, tc));
        },
    },
    DP_FUNC_PARSE_BASE64_TO_STRING: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_PARSE_BASE64_TO_STRING,
            "display_name": "parse_base64_to_string",
            "short_desc": "Parses a base64 string to a string",
            "desc": "Parses a base64-encoded string and returns a string with optional encoding",
            "symbol": "parse_base64_to_string()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureStringType, DataProviderSignatureOptionalStringType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Base64 String",
                    "short_desc": "The base64 string to parse",
                    "desc": "The base64-encoded string to parse",
                },
                <DataProviderArgInfo>{
                    "display_name": "Encoding",
                    "short_desc": "The string encoding (default: UTF-8)",
                    "desc": "The encoding for the output string; default: `UTF-8`",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return parse_base64_string_to_string(dp_eval_arg(rec, exp, 0, tc),
                dp_eval_arg(rec, exp, 1, tc));
        },
    },
    DP_FUNC_PARSE_BASE64_URL_TO_STRING: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_PARSE_BASE64_URL_TO_STRING,
            "display_name": "parse_base64_url_to_string",
            "short_desc": "Parses a base64-URL string to a string",
            "desc": "Parses a base64-URL-encoded string (RFC 4648) and returns a string",
            "symbol": "parse_base64_url_to_string()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureDataType, DataProviderSignatureOptionalStringType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Base64-URL String",
                    "short_desc": "The base64-URL string to parse",
                    "desc": "The base64-URL-encoded string to parse",
                },
                <DataProviderArgInfo>{
                    "display_name": "Encoding",
                    "short_desc": "The string encoding (default: UTF-8)",
                    "desc": "The encoding for the output string; default: `UTF-8`",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return parse_base64_url_string_to_string(dp_eval_arg(rec, exp, 0, tc),
                dp_eval_arg(rec, exp, 1, tc));
        },
    },
    DP_FUNC_ENCODE_URL: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_ENCODE_URL,
            "display_name": "encode_url",
            "short_desc": "URL-encodes a string",
            "desc": "Encodes a string with URL percent-encoding",
            "symbol": "encode_url()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureStringType, DataProviderSignatureOptionalBoolType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String",
                    "short_desc": "The string to encode",
                    "desc": "The string to encode with URL encoding",
                },
                <DataProviderArgInfo>{
                    "display_name": "Encode All?",
                    "short_desc": "Also encode RFC 3986 reserved characters",
                    "desc": "If `true`, also encodes reserved characters per RFC 3986",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            string val = dp_eval_arg(rec, exp, 0, tc);
            *bool all = dp_eval_arg(rec, exp, 1, tc);
            return encode_url(val, all);
        },
    },
    DP_FUNC_DECODE_URL: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_DECODE_URL,
            "display_name": "decode_url",
            "short_desc": "Decodes a URL-encoded string",
            "desc": "Decodes percent-encoded characters in a URL string",
            "symbol": "decode_url()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureStringType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String",
                    "short_desc": "The URL-encoded string to decode",
                    "desc": "The URL-encoded string to decode",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return decode_url(dp_eval_arg(rec, exp, 0, tc));
        },
    },

    # --- Crypto ---
    DP_FUNC_DIGEST: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_DIGEST,
            "display_name": "digest",
            "short_desc": "Computes a cryptographic digest (hex string)",
            "desc": "Returns the digest of the input data as a hex string",
            "symbol": "get_digest()",
            "groups": "Encryption and Hashing",
            "args": (DataProviderSignatureDataType, DataProviderSignatureStringType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to compute the digest for",
                    "desc": "The string or binary value to compute the digest for",
                },
                <DataProviderArgInfo>{
                    "display_name": "Digest Algorithm",
                    "short_desc": "The algorithm (e.g., 'sha256', 'md5')",
                    "desc": "The cryptographic algorithm to use",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            data val = dp_eval_arg(rec, exp, 0, tc);
            string d = dp_eval_arg(rec, exp, 1, tc);
            return digest(d, val).toHex();
        },
    },
    DP_FUNC_HMAC: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_HMAC,
            "display_name": "hmac",
            "short_desc": "Computes an HMAC (hex string)",
            "desc": "Returns the HMAC of the input data and key as a hex string",
            "symbol": "get_hmac()",
            "groups": "Encryption and Hashing",
            "args": (DataProviderSignatureDataType, DataProviderSignatureStringType,
                DataProviderSignatureDataType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to compute the HMAC for",
                    "desc": "The string or binary value to compute the HMAC for",
                },
                <DataProviderArgInfo>{
                    "display_name": "Digest Algorithm",
                    "short_desc": "The algorithm (e.g., 'sha256')",
                    "desc": "The cryptographic algorithm to use",
                },
                <DataProviderArgInfo>{
                    "display_name": "Key",
                    "short_desc": "The secret key",
                    "desc": "The secret key for the HMAC",
                    "sensitive": True,
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            data val = dp_eval_arg(rec, exp, 0, tc);
            string d = dp_eval_arg(rec, exp, 1, tc);
            data key = dp_eval_arg(rec, exp, 2, tc);
            return hmac(d, val, key).toHex();
        },
    },
    DP_FUNC_ENCRYPT: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_ENCRYPT,
            "display_name": "encrypt",
            "short_desc": "Encrypts data",
            "desc": "Returns encrypted data as a hex string",
            "symbol": "encrypt()",
            "groups": "Encryption and Hashing",
            "args": (DataProviderSignatureDataType, DataProviderSignatureStringType,
                DataProviderSignatureDataType, DataProviderSignatureOptionalDataType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to encrypt",
                    "desc": "The data to encrypt",
                },
                <DataProviderArgInfo>{
                    "display_name": "Cipher",
                    "short_desc": "The encryption algorithm",
                    "desc": "The encryption algorithm to use",
                },
                <DataProviderArgInfo>{
                    "display_name": "Key",
                    "short_desc": "The encryption key",
                    "desc": "The encryption key",
                    "sensitive": True,
                },
                <DataProviderArgInfo>{
                    "display_name": "IV",
                    "short_desc": "Optional initialization vector",
                    "desc": "Optional initialization vector or nonce",
                    "sensitive": True,
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            data val = dp_eval_arg(rec, exp, 0, tc);
            string cipher = dp_eval_arg(rec, exp, 1, tc);
            data key = dp_eval_arg(rec, exp, 2, tc);
            *data iv = dp_eval_arg(rec, exp, 3, tc);
            return encrypt(cipher, val, key, iv).toHex();
        },
    },
    DP_FUNC_DECRYPT: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_DECRYPT,
            "display_name": "decrypt",
            "short_desc": "Decrypts data",
            "desc": "Returns decrypted binary data",
            "symbol": "decrypt()",
            "groups": "Encryption and Hashing",
            "args": (DataProviderSignatureDataType, DataProviderSignatureStringType,
                DataProviderSignatureDataType, DataProviderSignatureOptionalDataType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to decrypt",
                    "desc": "The data to decrypt",
                },
                <DataProviderArgInfo>{
                    "display_name": "Cipher",
                    "short_desc": "The decryption algorithm",
                    "desc": "The decryption algorithm to use",
                },
                <DataProviderArgInfo>{
                    "display_name": "Key",
                    "short_desc": "The decryption key",
                    "desc": "The decryption key",
                    "sensitive": True,
                },
                <DataProviderArgInfo>{
                    "display_name": "IV",
                    "short_desc": "Optional initialization vector",
                    "desc": "Optional initialization vector or nonce",
                    "sensitive": True,
                },
            ),
            "return_type": AbstractDataProviderTypeMap."binary",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            data val = dp_eval_arg(rec, exp, 0, tc);
            string cipher = dp_eval_arg(rec, exp, 1, tc);
            data key = dp_eval_arg(rec, exp, 2, tc);
            *data iv = dp_eval_arg(rec, exp, 3, tc);
            return decrypt_to_binary(cipher, val, key, iv);
        },
    },

    # --- Type conversions ---
    DP_OP_TO_STRING: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_TO_STRING,
            "display_name": "to string",
            "short_desc": "Converts a value to a string",
            "desc": "Converts data directly to a string",
            "symbol": "toString",
            "render_template": "$arg[0].toString($arg[1])",
            "groups": "Data Conversion",
            "args": (DataProviderSignatureAnyType, DataProviderSignatureOptionalStringType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to convert",
                    "desc": "The value to convert to a string",
                },
                <DataProviderArgInfo>{
                    "display_name": "Encoding",
                    "short_desc": "String encoding (default: UTF-8)",
                    "desc": "The encoding for the output string; default: `UTF-8`",
                    "default_value": "UTF-8",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            auto val = dp_eval_arg(rec, exp, 0, tc);
            *data enc = dp_eval_arg(rec, exp, 1, tc);
            return val.toString(enc);
        },
    },
    DP_OP_TO_INT: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_TO_INT,
            "display_name": "to int",
            "short_desc": "Converts a value to an integer",
            "desc": "Converts data directly to an integer",
            "symbol": "toInt",
            "render_template": "$arg[0].toInt()",
            "groups": "Data Conversion",
            "args": (DataProviderSignatureAnyType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to convert",
                    "desc": "The value to convert to an integer",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."int",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return dp_eval_arg(rec, exp, 0, tc).toInt();
        },
    },
    DP_OP_TO_FLOAT: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_TO_FLOAT,
            "display_name": "to float",
            "short_desc": "Converts a value to a float",
            "desc": "Converts data directly to a floating-point number",
            "symbol": "toFloat",
            "render_template": "$arg[0].toFloat()",
            "groups": "Data Conversion",
            "args": (DataProviderSignatureAnyType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to convert",
                    "desc": "The value to convert to a float",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."float",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return dp_eval_arg(rec, exp, 0, tc).toFloat();
        },
    },
    DP_OP_TO_NUMBER: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_TO_NUMBER,
            "display_name": "to number",
            "short_desc": "Converts a value to an arbitrary-precision number",
            "desc": "Converts data directly to an arbitrary-precision floating-point number",
            "symbol": "toNumber",
            "render_template": "$arg[0].toNumber()",
            "groups": "Data Conversion",
            "args": (DataProviderSignatureAnyType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to convert",
                    "desc": "The value to convert to a number",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."number",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return dp_eval_arg(rec, exp, 0, tc).toNumber();
        },
    },
    DP_OP_TO_BOOL: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_TO_BOOL,
            "display_name": "to bool",
            "short_desc": "Converts a value to a boolean",
            "desc": "Converts data directly to a boolean",
            "symbol": "toBool",
            "render_template": "$arg[0].toBool()",
            "groups": "Data Conversion",
            "args": (DataProviderSignatureAnyType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to convert",
                    "desc": "The value to convert to a boolean",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return dp_eval_arg(rec, exp, 0, tc).toBool();
        },
    },
    DP_OP_TO_DATE: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_TO_DATE,
            "display_name": "to date",
            "short_desc": "Converts a value to a date",
            "desc": "Converts data to a date; ISO 8601 formats are supported implicitly",
            "symbol": "toDate",
            "render_template": "$arg[0].toDate($arg[1])",
            "groups": "Data Conversion",
            "args": (DataProviderSignatureAnyType, DataProviderSignatureOptionalStringType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to convert",
                    "desc": "The value to convert to a date",
                    "example_value": "2025-10-31T09:53:14.298750Z",
                },
                <DataProviderArgInfo>{
                    "display_name": "Mask",
                    "short_desc": "Optional date format mask",
                    "desc": "The date format mask (e.g., `DD/MM/YYYY HH:mm:SS`)",
                    "example_value": "DD/MM/YYYY HH:mm:SS",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."date",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            auto val = dp_eval_arg(rec, exp, 0, tc);
            *string mask = dp_eval_arg(rec, exp, 1, tc);
            int t = val.typeCode();
            switch (t) {
                case NT_HASH:
                case NT_LIST:
                case NT_BINARY:
                case NT_OBJECT:
                    throw "DATE-INPUT-ERROR", sprintf("Cannot convert value of type %y to date", val.fullType());
            }
            return exists mask ? date(val.toString(), mask) : date(val);
        },
    },

    # --- now() function ---
    DP_FUNC_NOW: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_NOW,
            "display_name": "now",
            "short_desc": "Returns the current date/time with microsecond precision",
            "desc": "Returns the current date/time with microsecond precision",
            "symbol": "now()",
            "groups": "Date/Time",
            "args": (),
            "return_type": AbstractDataProviderTypeMap."date",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return now_us();
        },
    },

    # --- Duration functions ---
    DP_FUNC_YEARS: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_YEARS,
            "display_name": "years",
            "short_desc": "Returns a relative date/time value representing the given number of years",
            "desc": "Returns a relative date/time value representing the given number of years; "
                "use with the `+` or `-` operators for date arithmetic",
            "symbol": "years()",
            "groups": "Date/Time",
            "args": (DataProviderSignatureIntType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Years",
                    "short_desc": "The number of years",
                    "desc": "The number of years for the duration",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."date",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return Qore::months(dp_eval_arg(rec, exp, 0, tc) * 12);
        },
    },
    DP_FUNC_MONTHS: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_MONTHS,
            "display_name": "months",
            "short_desc": "Returns a relative date/time value representing the given number of months",
            "desc": "Returns a relative date/time value representing the given number of months; "
                "use with the `+` or `-` operators for date arithmetic",
            "symbol": "months()",
            "groups": "Date/Time",
            "args": (DataProviderSignatureIntType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Months",
                    "short_desc": "The number of months",
                    "desc": "The number of months for the duration",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."date",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return Qore::months(dp_eval_arg(rec, exp, 0, tc));
        },
    },
    DP_FUNC_WEEKS: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_WEEKS,
            "display_name": "weeks",
            "short_desc": "Returns a relative date/time value representing the given number of weeks",
            "desc": "Returns a relative date/time value representing the given number of weeks; "
                "use with the `+` or `-` operators for date arithmetic",
            "symbol": "weeks()",
            "groups": "Date/Time",
            "args": (DataProviderSignatureIntType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Weeks",
                    "short_desc": "The number of weeks",
                    "desc": "The number of weeks for the duration",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."date",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return Qore::days(dp_eval_arg(rec, exp, 0, tc) * 7);
        },
    },
    DP_FUNC_DAYS: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_DAYS,
            "display_name": "days",
            "short_desc": "Returns a relative date/time value representing the given number of days",
            "desc": "Returns a relative date/time value representing the given number of days; "
                "use with the `+` or `-` operators for date arithmetic",
            "symbol": "days()",
            "groups": "Date/Time",
            "args": (DataProviderSignatureIntType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Days",
                    "short_desc": "The number of days",
                    "desc": "The number of days for the duration",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."date",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return Qore::days(dp_eval_arg(rec, exp, 0, tc));
        },
    },
    DP_FUNC_HOURS: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_HOURS,
            "display_name": "hours",
            "short_desc": "Returns a relative date/time value representing the given number of hours",
            "desc": "Returns a relative date/time value representing the given number of hours; "
                "use with the `+` or `-` operators for date arithmetic",
            "symbol": "hours()",
            "groups": "Date/Time",
            "args": (DataProviderSignatureIntType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Hours",
                    "short_desc": "The number of hours",
                    "desc": "The number of hours for the duration",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."date",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return Qore::hours(dp_eval_arg(rec, exp, 0, tc));
        },
    },
    DP_FUNC_MINUTES: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_MINUTES,
            "display_name": "minutes",
            "short_desc": "Returns a relative date/time value representing the given number of minutes",
            "desc": "Returns a relative date/time value representing the given number of minutes; "
                "use with the `+` or `-` operators for date arithmetic",
            "symbol": "minutes()",
            "groups": "Date/Time",
            "args": (DataProviderSignatureIntType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Minutes",
                    "short_desc": "The number of minutes",
                    "desc": "The number of minutes for the duration",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."date",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return Qore::minutes(dp_eval_arg(rec, exp, 0, tc));
        },
    },
    DP_FUNC_SECONDS: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_SECONDS,
            "display_name": "seconds",
            "short_desc": "Returns a relative date/time value representing the given number of seconds",
            "desc": "Returns a relative date/time value representing the given number of seconds; "
                "use with the `+` or `-` operators for date arithmetic",
            "symbol": "seconds()",
            "groups": "Date/Time",
            "args": (DataProviderSignatureIntType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Seconds",
                    "short_desc": "The number of seconds",
                    "desc": "The number of seconds for the duration",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."date",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return Qore::seconds(dp_eval_arg(rec, exp, 0, tc));
        },
    },
    DP_FUNC_MILLISECONDS: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_MILLISECONDS,
            "display_name": "milliseconds",
            "short_desc": "Returns a relative date/time value representing the given number of milliseconds",
            "desc": "Returns a relative date/time value representing the given number of milliseconds; "
                "use with the `+` or `-` operators for date arithmetic",
            "symbol": "milliseconds()",
            "groups": "Date/Time",
            "args": (DataProviderSignatureIntType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Milliseconds",
                    "short_desc": "The number of milliseconds",
                    "desc": "The number of milliseconds for the duration",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."date",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return Qore::milliseconds(dp_eval_arg(rec, exp, 0, tc));
        },
    },
    DP_FUNC_MICROSECONDS: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_MICROSECONDS,
            "display_name": "microseconds",
            "short_desc": "Returns a relative date/time value representing the given number of microseconds",
            "desc": "Returns a relative date/time value representing the given number of microseconds; "
                "use with the `+` or `-` operators for date arithmetic",
            "symbol": "microseconds()",
            "groups": "Date/Time",
            "args": (DataProviderSignatureIntType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Microseconds",
                    "short_desc": "The number of microseconds",
                    "desc": "The number of microseconds for the duration",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."date",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return Qore::microseconds(dp_eval_arg(rec, exp, 0, tc));
        },
    },

    # --- Date extraction functions ---
    DP_FUNC_GET_YEAR: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_GET_YEAR,
            "display_name": "get_year",
            "short_desc": "Extracts the year component from a date/time value",
            "desc": "Returns the year component from a date/time value as an integer",
            "symbol": "get_year()",
            "groups": "Date/Time",
            "args": (DataProviderSignatureDateType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Date",
                    "short_desc": "The date/time value",
                    "desc": "The date/time value to extract the year from",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."int",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return get_years(dp_eval_arg(rec, exp, 0, tc));
        },
    },
    DP_FUNC_GET_MONTH: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_GET_MONTH,
            "display_name": "get_month",
            "short_desc": "Extracts the month component from a date/time value",
            "desc": "Returns the month component from a date/time value as an integer (1-12)",
            "symbol": "get_month()",
            "groups": "Date/Time",
            "args": (DataProviderSignatureDateType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Date",
                    "short_desc": "The date/time value",
                    "desc": "The date/time value to extract the month from",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."int",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return get_months(dp_eval_arg(rec, exp, 0, tc));
        },
    },
    DP_FUNC_GET_DAY: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_GET_DAY,
            "display_name": "get_day",
            "short_desc": "Extracts the day component from a date/time value",
            "desc": "Returns the day-of-month component from a date/time value as an integer (1-31)",
            "symbol": "get_day()",
            "groups": "Date/Time",
            "args": (DataProviderSignatureDateType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Date",
                    "short_desc": "The date/time value",
                    "desc": "The date/time value to extract the day from",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."int",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return get_days(dp_eval_arg(rec, exp, 0, tc));
        },
    },
    DP_FUNC_GET_HOUR: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_GET_HOUR,
            "display_name": "get_hour",
            "short_desc": "Extracts the hour component from a date/time value",
            "desc": "Returns the hour component from a date/time value as an integer (0-23)",
            "symbol": "get_hour()",
            "groups": "Date/Time",
            "args": (DataProviderSignatureDateType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Date",
                    "short_desc": "The date/time value",
                    "desc": "The date/time value to extract the hour from",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."int",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return get_hours(dp_eval_arg(rec, exp, 0, tc));
        },
    },
    DP_FUNC_GET_MINUTE: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_GET_MINUTE,
            "display_name": "get_minute",
            "short_desc": "Extracts the minute component from a date/time value",
            "desc": "Returns the minute component from a date/time value as an integer (0-59)",
            "symbol": "get_minute()",
            "groups": "Date/Time",
            "args": (DataProviderSignatureDateType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Date",
                    "short_desc": "The date/time value",
                    "desc": "The date/time value to extract the minute from",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."int",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return get_minutes(dp_eval_arg(rec, exp, 0, tc));
        },
    },
    DP_FUNC_GET_SECOND: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_GET_SECOND,
            "display_name": "get_second",
            "short_desc": "Extracts the second component from a date/time value",
            "desc": "Returns the second component from a date/time value as an integer (0-59)",
            "symbol": "get_second()",
            "groups": "Date/Time",
            "args": (DataProviderSignatureDateType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Date",
                    "short_desc": "The date/time value",
                    "desc": "The date/time value to extract the second from",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."int",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return get_seconds(dp_eval_arg(rec, exp, 0, tc));
        },
    },

    # --- Null handling ---
    DP_OP_IS_NULL: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_IS_NULL,
            "display_name": "is null",
            "short_desc": "Returns True if the argument is NOTHING (null)",
            "desc": "Returns `True` if the argument is `NOTHING` (null)",
            "symbol": "isNull",
            "render_template": "$arg[0].isNull()",
            "groups": "Comparison",
            "args": (DataProviderSignatureAnyType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to check",
                    "desc": "The value to check for null",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            auto val = dp_eval_arg(rec, exp, 0, tc);
            return !exists val || val === NULL;
        },
    },
    DP_OP_IS_NOT_NULL: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_IS_NOT_NULL,
            "display_name": "is not null",
            "short_desc": "Returns True if the argument is not NOTHING (not null)",
            "desc": "Returns `True` if the argument is not `NOTHING` (not null)",
            "symbol": "isNotNull",
            "render_template": "$arg[0].isNotNull()",
            "groups": "Comparison",
            "args": (DataProviderSignatureAnyType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to check",
                    "desc": "The value to check for not null",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            auto val = dp_eval_arg(rec, exp, 0, tc);
            return exists val && val !== NULL;
        },
    },
    DP_OP_NOT_IN: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_NOT_IN,
            "display_name": "not in",
            "short_desc": "Returns True if the first argument is not found in the second argument",
            "desc": "Returns `True` if the first argument is not found in the second argument (list)",
            "symbol": "notIn",
            "render_template": "$arg[0] not in ($arg[1])",
            "groups": "Comparison",
            "args": (DataProviderSignatureAnyType, DataProviderSignatureAnyType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "A value to look for (needle)",
                    "desc": "A value to look for; first value must not be found in the second for the overall "
                        "expression to return `True`",
                    "label_after": "not in",
                },
                <DataProviderArgInfo>{
                    "display_name": "Search Source",
                    "short_desc": "The list to search in (haystack)",
                    "desc": "The list to search in (haystack)",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."bool",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return !inlist(dp_eval_arg(rec, exp, 0, tc), dp_eval_arg(rec, exp, 1, tc));
        },
    },
    DP_FUNC_COALESCE: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_COALESCE,
            "display_name": "coalesce",
            "short_desc": "Returns the first non-NOTHING argument",
            "desc": "Returns the value of the first argument that is not `NOTHING`; returns `NOTHING` if all "
                "arguments are `NOTHING`",
            "symbol": "coalesce()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureAnyType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "A value to check",
                    "desc": "A value to check; the first non-NOTHING value is returned",
                },
            ),
            "varargs": True,
            "min_args": 1,
            "return_type": AbstractDataProviderTypeMap."any",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            foreach auto arg in (exp.args) {
                auto val = AbstractDataProvider::evalGenericExpressionValue(rec, arg, tc);
                if (exists val && val !== NULL) {
                    return val;
                }
            }
            return NOTHING;
        },
    },
    DP_FUNC_NULLIF: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_NULLIF,
            "display_name": "nullif",
            "short_desc": "Returns NOTHING if the two arguments are equal, otherwise the first argument",
            "desc": "Returns `NOTHING` if the two arguments are equal; otherwise returns the first argument",
            "symbol": "nullif()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureAnyType, DataProviderSignatureAnyType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to return if not equal",
                    "desc": "The value to return if the two arguments are not equal",
                },
                <DataProviderArgInfo>{
                    "display_name": "Compare Value",
                    "short_desc": "The value to compare against",
                    "desc": "If this value equals the first argument, NOTHING is returned",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."any",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            auto val = dp_eval_arg(rec, exp, 0, tc);
            return val == dp_eval_arg(rec, exp, 1, tc) ? NOTHING : val;
        },
    },

    # --- Math functions ---
    DP_FUNC_ABS: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_ABS,
            "display_name": "abs",
            "short_desc": "Returns the absolute value of a number",
            "desc": "Returns the absolute value of a number",
            "symbol": "abs()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureNumberType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Number",
                    "short_desc": "The number to get the absolute value of",
                    "desc": "The number to get the absolute value of",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."any",
            "return_type_first_arg": True,
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return abs(dp_eval_arg(rec, exp, 0, tc));
        },
    },
    DP_FUNC_ROUND: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_ROUND,
            "display_name": "round",
            "short_desc": "Rounds a number to the given number of decimal places",
            "desc": "Rounds a number to the given number of decimal places; default precision is 0 "
                "(rounds to nearest integer)",
            "symbol": "round()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureNumberType, DataProviderSignatureOptionalIntType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Number",
                    "short_desc": "The number to round",
                    "desc": "The number to round",
                },
                <DataProviderArgInfo>{
                    "display_name": "Precision",
                    "short_desc": "Number of decimal places (default: 0)",
                    "desc": "The number of decimal places to round to; default: `0`",
                    "default_value": 0,
                },
            ),
            "return_type": AbstractDataProviderTypeMap."any",
            "return_type_first_arg": True,
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            auto val = dp_eval_arg(rec, exp, 0, tc);
            int prec = dp_eval_arg(rec, exp, 1, tc) ?? 0;
            return round(val, prec);
        },
    },
    DP_FUNC_FLOOR: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_FLOOR,
            "display_name": "floor",
            "short_desc": "Returns the largest integer not greater than the argument",
            "desc": "Returns the largest integer not greater than the argument (rounds down)",
            "symbol": "floor()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureNumberType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Number",
                    "short_desc": "The number to round down",
                    "desc": "The number to round down to the nearest integer",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."int",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return int(floor(dp_eval_arg(rec, exp, 0, tc)));
        },
    },
    DP_FUNC_CEIL: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_CEIL,
            "display_name": "ceil",
            "short_desc": "Returns the smallest integer not less than the argument",
            "desc": "Returns the smallest integer not less than the argument (rounds up)",
            "symbol": "ceil()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureNumberType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Number",
                    "short_desc": "The number to round up",
                    "desc": "The number to round up to the nearest integer",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."int",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return int(ceil(dp_eval_arg(rec, exp, 0, tc)));
        },
    },
    DP_OP_MOD: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Operator,
            "name": DP_OP_MOD,
            "display_name": "modulus (%)",
            "short_desc": "Returns the remainder of dividing the first argument by the second",
            "desc": "Returns the remainder of dividing the first argument by the second (modulus operator)",
            "symbol": "%",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureNumberType, DataProviderSignatureNumberType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Dividend",
                    "short_desc": "The value to divide",
                    "desc": "The value to divide (dividend)",
                    "label_after": "%",
                },
                <DataProviderArgInfo>{
                    "display_name": "Divisor",
                    "short_desc": "The divisor",
                    "desc": "The divisor",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."any",
            "return_type_arg_priority": ("number", "float", "int"),
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return dp_eval_arg(rec, exp, 0, tc) % dp_eval_arg(rec, exp, 1, tc);
        },
    },

    # --- String functions ---
    DP_FUNC_TRIM: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_TRIM,
            "display_name": "trim",
            "short_desc": "Trims whitespace from both sides of a string",
            "desc": "Returns the string with leading and trailing whitespace removed; optionally specify "
                "characters to trim instead of whitespace",
            "symbol": "trim()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureStringType, DataProviderSignatureOptionalStringType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String",
                    "short_desc": "The string to trim",
                    "desc": "The string to trim",
                },
                <DataProviderArgInfo>{
                    "display_name": "Characters",
                    "short_desc": "Characters to trim (default: whitespace)",
                    "desc": "Optional characters to trim instead of whitespace",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            string str = dp_eval_arg(rec, exp, 0, tc);
            *string chars = dp_eval_arg(rec, exp, 1, tc);
            return trim(str, chars);
        },
    },
    DP_FUNC_LTRIM: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_LTRIM,
            "display_name": "ltrim",
            "short_desc": "Trims whitespace from the left side of a string",
            "desc": "Returns the string with leading whitespace removed; optionally specify "
                "characters to trim instead of whitespace",
            "symbol": "ltrim()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureStringType, DataProviderSignatureOptionalStringType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String",
                    "short_desc": "The string to trim",
                    "desc": "The string to trim from the left",
                },
                <DataProviderArgInfo>{
                    "display_name": "Characters",
                    "short_desc": "Characters to trim (default: whitespace)",
                    "desc": "Optional characters to trim instead of whitespace",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            string str = dp_eval_arg(rec, exp, 0, tc);
            *string chars = dp_eval_arg(rec, exp, 1, tc);
            return ltrim(str, chars);
        },
    },
    DP_FUNC_RTRIM: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_RTRIM,
            "display_name": "rtrim",
            "short_desc": "Trims whitespace from the right side of a string",
            "desc": "Returns the string with trailing whitespace removed; optionally specify "
                "characters to trim instead of whitespace",
            "symbol": "rtrim()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureStringType, DataProviderSignatureOptionalStringType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String",
                    "short_desc": "The string to trim",
                    "desc": "The string to trim from the right",
                },
                <DataProviderArgInfo>{
                    "display_name": "Characters",
                    "short_desc": "Characters to trim (default: whitespace)",
                    "desc": "Optional characters to trim instead of whitespace",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            string str = dp_eval_arg(rec, exp, 0, tc);
            *string chars = dp_eval_arg(rec, exp, 1, tc);
            return rtrim(str, chars);
        },
    },
    DP_FUNC_SPLIT: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_SPLIT,
            "display_name": "split",
            "short_desc": "Splits a string by a plain separator",
            "desc": "Splits a string by a plain separator string and returns a list of substrings",
            "symbol": "split()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureStringType, DataProviderSignatureStringType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "String To Split",
                    "short_desc": "The string to split",
                    "desc": "The string to split",
                },
                <DataProviderArgInfo>{
                    "display_name": "Separator",
                    "short_desc": "The separator string",
                    "desc": "The separator string to split by",
                    "example_value": ",",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."list",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            string str = dp_eval_arg(rec, exp, 0, tc);
            string sep = dp_eval_arg(rec, exp, 1, tc);
            return str.split(sep);
        },
    },
    DP_FUNC_CONCAT: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_CONCAT,
            "display_name": "concat",
            "short_desc": "Concatenates multiple values as strings",
            "desc": "Concatenates all arguments as strings and returns the result",
            "symbol": "concat()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureAnyType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "A value to concatenate",
                    "desc": "A value to concatenate; all values are converted to strings",
                },
            ),
            "varargs": True,
            "min_args": 1,
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return foldl $1 + $2,
                (map string(AbstractDataProvider::evalGenericExpressionValue(rec, $1, tc)), exp.args);
        },
    },

    # --- Map functions ---
    DP_FUNC_MAP: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_MAP,
            "display_name": "map",
            "short_desc": "Applies an expression to each element of a list, returning a new list",
            "desc": "Applies the body expression to each element of the source list and returns a new list "
                "of results. Within the body expression, field references (`@field`) resolve against the "
                "current iteration element. An optional filter expression can be provided to skip elements "
                "that do not match",
            "symbol": "map()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureAnyType, DataProviderSignatureAnyType,
                DataProviderSignatureOptionalAnyType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Source List",
                    "short_desc": "The list to iterate over",
                    "desc": "The list to iterate over; each element becomes the context for the body expression",
                },
                <DataProviderArgInfo>{
                    "display_name": "Body Expression",
                    "short_desc": "Expression applied to each element",
                    "desc": "The expression to apply to each element; field references resolve against the "
                        "current element",
                },
                <DataProviderArgInfo>{
                    "display_name": "Filter Expression",
                    "short_desc": "Optional filter condition",
                    "desc": "Optional boolean expression; only elements where this evaluates to True are included",
                },
            ),

            "return_type": AbstractDataProviderTypeMap."list",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            softlist<auto> source = AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[0], tc);
            auto body = exp.args[1];
            *auto filter = exp.args.size() > 2 ? exp.args[2] : NOTHING;
            list<auto> result = ();
            foreach auto elem in (source) {
                hash<auto> iter_rec = elem.typeCode() == NT_HASH ? elem : {"value": elem};
                if (exists filter && filter.val()) {
                    if (!AbstractDataProvider::evalGenericExpressionValue(iter_rec, filter, tc)) {
                        continue;
                    }
                }
                result += AbstractDataProvider::evalGenericExpressionValue(iter_rec, body, tc);
            }
            return result;
        },
    },
    DP_FUNC_HASH_MAP: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "role": ER_All,
            "name": DP_FUNC_HASH_MAP,
            "display_name": "hash_map",
            "short_desc": "Maps a list to a hash by applying key and value expressions to each element",
            "desc": "Applies key and value expressions to each element of the source list and returns a hash. "
                "Within the expressions, field references (`@field`) resolve against the current iteration "
                "element. An optional filter expression can be provided to skip elements that do not match",
            "symbol": "hash_map()",
            "groups": "Data Manipulation",
            "args": (DataProviderSignatureAnyType, DataProviderSignatureAnyType,
                DataProviderSignatureAnyType, DataProviderSignatureOptionalAnyType),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Source List",
                    "short_desc": "The list to iterate over",
                    "desc": "The list to iterate over; each element becomes the context for the expressions",
                },
                <DataProviderArgInfo>{
                    "display_name": "Key Expression",
                    "short_desc": "Expression for the hash key",
                    "desc": "The expression that produces the hash key for each element; field references resolve "
                        "against the current element",
                },
                <DataProviderArgInfo>{
                    "display_name": "Value Expression",
                    "short_desc": "Expression for the hash value",
                    "desc": "The expression that produces the hash value for each element; field references resolve "
                        "against the current element",
                },
                <DataProviderArgInfo>{
                    "display_name": "Filter Expression",
                    "short_desc": "Optional filter condition",
                    "desc": "Optional boolean expression; only elements where this evaluates to True are included",
                },
            ),

            "return_type": AbstractDataProviderTypeMap."hash",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            softlist<auto> source = AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[0], tc);
            auto key_expr = exp.args[1];
            auto val_expr = exp.args[2];
            *auto filter = exp.args.size() > 3 ? exp.args[3] : NOTHING;
            hash<auto> result = {};
            foreach auto elem in (source) {
                hash<auto> iter_rec = elem.typeCode() == NT_HASH ? elem : {"value": elem};
                if (exists filter && filter.val()) {
                    if (!AbstractDataProvider::evalGenericExpressionValue(iter_rec, filter, tc)) {
                        continue;
                    }
                }
                auto k = AbstractDataProvider::evalGenericExpressionValue(iter_rec, key_expr, tc);
                auto v = AbstractDataProvider::evalGenericExpressionValue(iter_rec, val_expr, tc);
                result{k} = v;
            }
            return result;
        },
    },
};

# --- Module-conditional expression implementations (YAML) ---
#! YAML serialization expression implementations
/** @since DataProvider 3.3
*/
%ifndef NoYaml
public const YamlExpressionImplementations = {
    DP_FUNC_MAKE_YAML: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_MAKE_YAML,
            "display_name": "make_yaml",
            "short_desc": "Serializes a value to a YAML string",
            "desc": "Serializes a value to a YAML string",
            "symbol": "make_yaml()",
            "groups": "Serialization",
            "args": (DataProviderSignatureAnyType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to serialize",
                    "desc": "The value to serialize to YAML",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return make_yaml(dp_eval_arg(rec, exp, 0, tc));
        },
    },
    DP_FUNC_PARSE_YAML: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_PARSE_YAML,
            "display_name": "parse_yaml",
            "short_desc": "Deserializes a YAML string",
            "desc": "Deserializes a YAML string to data",
            "symbol": "parse_yaml()",
            "groups": "Serialization",
            "args": (DataProviderSignatureStringType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "YAML String",
                    "short_desc": "The YAML string to parse",
                    "desc": "The YAML string to deserialize",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."any",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return parse_yaml(dp_eval_arg(rec, exp, 0, tc));
        },
    },
};
%endif

# --- Module-conditional expression implementations (JSON) ---
#! JSON serialization expression implementations
/** @since DataProvider 3.3
*/
%ifndef NoJson
public const JsonExpressionImplementations = {
    DP_FUNC_MAKE_JSON: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_MAKE_JSON,
            "display_name": "make_json",
            "short_desc": "Serializes a value to a JSON string",
            "desc": "Serializes a value to a JSON string",
            "symbol": "make_json()",
            "groups": "Serialization",
            "args": (DataProviderSignatureAnyType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to serialize",
                    "desc": "The value to serialize to JSON",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return make_json(dp_eval_arg(rec, exp, 0, tc));
        },
    },
    DP_FUNC_PARSE_JSON: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_PARSE_JSON,
            "display_name": "parse_json",
            "short_desc": "Deserializes a JSON string",
            "desc": "Deserializes a JSON string to data",
            "symbol": "parse_json()",
            "groups": "Serialization",
            "args": (DataProviderSignatureStringType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "JSON String",
                    "short_desc": "The JSON string to parse",
                    "desc": "The JSON string to deserialize",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."any",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return parse_json(dp_eval_arg(rec, exp, 0, tc));
        },
    },
};
%endif

# --- Module-conditional expression implementations (XML) ---
#! XML serialization expression implementations
/** @since DataProvider 3.3
*/
%ifndef NoXml
public const XmlExpressionImplementations = {
    DP_FUNC_MAKE_XML: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_MAKE_XML,
            "display_name": "make_xml",
            "short_desc": "Serializes a value to an XML string",
            "desc": "Serializes a value to an XML string",
            "symbol": "make_xml()",
            "groups": "Serialization",
            "args": (DataProviderSignatureAnyType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "Value",
                    "short_desc": "The value to serialize",
                    "desc": "The value to serialize to XML",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."string",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return make_xml(dp_eval_arg(rec, exp, 0, tc));
        },
    },
    DP_FUNC_PARSE_XML: {
        "exp": <DataProviderExpressionInfo>{
            "type": DET_Function,
            "name": DP_FUNC_PARSE_XML,
            "display_name": "parse_xml",
            "short_desc": "Deserializes an XML string",
            "desc": "Deserializes an XML string to data",
            "symbol": "parse_xml()",
            "groups": "Serialization",
            "args": (DataProviderSignatureStringType,),
            "arg_info": (
                <DataProviderArgInfo>{
                    "display_name": "XML String",
                    "short_desc": "The XML string to parse",
                    "desc": "The XML string to deserialize",
                },
            ),
            "return_type": AbstractDataProviderTypeMap."hash",
        },
        "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> tc) {
            return parse_xml(dp_eval_arg(rec, exp, 0, tc));
        },
    },
};
%endif

} # namespace DataProvider
