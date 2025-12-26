# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CDSRestDataProviderDefs definitions

/** CDSRestDataProviderDefs.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
 #! Query operator info hash as returned by all @ref query_op_funcs "operator functions"
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
}
