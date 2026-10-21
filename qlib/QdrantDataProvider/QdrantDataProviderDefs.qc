# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantDataProvider definitions

/** QdrantDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the QdrantDataProvider module
public namespace QdrantDataProvider {

#! Qdrant filter expression definitions for use with DataProvider search expressions
/** These expressions enable advanced search functionality with AND/OR/NOT operators
    and comparison operators that work with the DataProvider expression interface.

    They translate DataProvider expression trees into Qdrant filter JSON structures.
*/
public const QdrantExpressions = {
    # Logical operators with nesting support
    DP_OP_AND: {
        "exp": AbstractDataProvider::GenericExpressions{DP_OP_AND},
        "impl": hash<auto> sub (QdrantSearchRecordIterator iter, list<auto> args) {
            return {"must": map iter.processExpressionArg($1), args};
        },
    },
    DP_OP_OR: {
        "exp": AbstractDataProvider::GenericExpressions{DP_OP_OR},
        "impl": hash<auto> sub (QdrantSearchRecordIterator iter, list<auto> args) {
            return {"should": map iter.processExpressionArg($1), args};
        },
    },
    DP_SEARCH_OP_NOT: {
        "exp": AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_NOT},
        "impl": hash<auto> sub (QdrantSearchRecordIterator iter, list<auto> args) {
            return {"must_not": map iter.processExpressionArg($1), args};
        },
    },

    # Comparison operators
    DP_SEARCH_OP_EQ: {
        "exp": AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_EQ},
        "impl": hash<auto> sub (string field, auto value) {
            return {"key": field, "match": {"value": value}};
        },
    },
    DP_SEARCH_OP_NE: {
        "exp": AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_NE},
        "impl": hash<auto> sub (string field, auto value) {
            return {"must_not": ({"key": field, "match": {"value": value}},)};
        },
    },
    DP_SEARCH_OP_LT: {
        "exp": AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_LT},
        "impl": hash<auto> sub (string field, auto value) {
            return {"key": field, "range": {"lt": value}};
        },
    },
    DP_SEARCH_OP_LE: {
        "exp": AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_LE},
        "impl": hash<auto> sub (string field, auto value) {
            return {"key": field, "range": {"lte": value}};
        },
    },
    DP_SEARCH_OP_GT: {
        "exp": AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_GT},
        "impl": hash<auto> sub (string field, auto value) {
            return {"key": field, "range": {"gt": value}};
        },
    },
    DP_SEARCH_OP_GE: {
        "exp": AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_GE},
        "impl": hash<auto> sub (string field, auto value) {
            return {"key": field, "range": {"gte": value}};
        },
    },
    DP_SEARCH_OP_BETWEEN: {
        "exp": AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_BETWEEN},
        "impl": hash<auto> sub (string field, auto lo, auto hi) {
            return {"key": field, "range": {"gt": lo, "lt": hi}};
        },
    },
    DP_SEARCH_OP_IN: {
        "exp": AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_IN},
        "impl": hash<auto> sub (string field, softlist<auto> values) {
            return {"key": field, "match": {"any": values}};
        },
    },
};

#! Writes payload key paths in the key syntax of Qdrant filters
/** In a Qdrant filter, a payload key path is a list of keys separated by dots, so a key that contains a dot, or any
    character other than an ASCII letter, a digit, \c "_" or \c "-", is written in double quotes:
    <tt>"Unit.Price"</tt> (with the quotes) is the key \c Unit.Price, and <tt>order."Unit.Price"</tt> is that key
    in the object \c order.  A quoted key has no escapes in Qdrant, so a key with a double quote or a backslash
    cannot be named in a filter.

    @par Example:
    @code{.py}
# 'order."Unit.Price"'
*string key = QdrantPayloadKey::getPath(("order", "Unit.Price"));
    @endcode

    @since %QdrantDataProvider 1.0
*/
public class QdrantPayloadKey {
    #! Returns the Qdrant payload key path that names the given keys, one in the other
    /** @param key_list the keys of the path: the first is a key of the payload, and each one after it a key of the
        object named before it

        @return the payload key path, or no value if a key has a double quote or a backslash, which Qdrant cannot
        name
    */
    static *string getPath(list<string> key_list) {
        list<string> path = ();
        foreach string key in (key_list) {
            *string item = QdrantPayloadKey::getKey(key);
            if (!exists item) {
                return;
            }
            push path, item;
        }
        return path.join(".");
    }

    #! Returns the Qdrant payload key path that names one key of the payload
    /** @param key the key

        @return the key as it is written in a Qdrant filter: as it is, if it has only ASCII letters, digits, \c "_" and
        \c "-", otherwise in double quotes; no value if it has a double quote or a backslash, which Qdrant cannot
        name
    */
    static *string getKey(string key) {
        if (key =~ /^[A-Za-z0-9_-]+$/ && !key.contains("\n")) {
            return key;
        }
        if (key.contains("\"") || key.contains("\\")) {
            return;
        }
        return "\"" + key + "\"";
    }
}
}
