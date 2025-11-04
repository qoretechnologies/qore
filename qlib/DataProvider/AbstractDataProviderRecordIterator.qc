# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractDataProviderRecordIterator class definition

/** AbstractDataProviderRecordIterator.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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
%new-style
# require type definitions everywhere
%require-types
#! strict argument handling
%strict-args
# enable all warnings
%enable-all-warnings

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
public const DP_SEARCH_OP_EQ = "=";

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
};

#! Generic operator info hash as returned by all @ref dataprovider_generic_search_op_funcs "generic search operator functions"
public hashdecl SearchOperatorInfo {
    string op;   #!< the operator string code
    auto arg;    #!< optional argument
}

#! a hash of valid operators for use in generic search criteria
public const DefaultGenericSearchOpMap = {
    DP_SEARCH_OP_REGEX: (
        "code": bool sub (hash<auto> cmd, auto field_value, string field_name, hash<auto> rec) {
            return regex(field_value.toString(), cmd.arg.pattern, cmd.arg.options);
        },
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
public hash<SearchOperatorInfo> sub dp_make_op(string op, auto arg) {
    return new hash<SearchOperatorInfo>({"op": op, "arg": arg});
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
    return dp_make_op(DP_SEARCH_OP_REGEX, {
        "pattern": str,
    });
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
    return dp_make_op(DP_SEARCH_OP_REGEX, {
        "pattern": "^" + str + "$",
    });
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
    }

    #! Creates the object and optionally sets the bulk iteration block size
    constructor(*bool requires_result, *int max_records) {
        self.requires_result = requires_result ?? False;
        self.max_records = max_records;
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

    #! Returns @ref True if the iterator supports bulk operation
    /** @return @ref True if the iterator supports bulk operation

        @note This base class method returns @ref True by default and simulates a bulk iterator
    */
    bool supportsBulkApi() {
        return True;
    }

    #! Returns the bulk data interface if supported
    /** @return the bulk data interface if supported

        @note This base class method returns @ref DefaultBulkRecordIterface by default
    */
    *AbstractDataProviderBulkRecordInterface getBulkApi(int block_size = DefaultBlockSize) {
        return new DefaultBulkRecordIterface(block_size, self, requires_result, max_records);
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
    /** in case of a hash value, if the hash was created by a
        @ref dataprovider_generic_search_op_funcs "search operator function", then that search operation is applied to
        the input, otherwise a recursive partial match with only the keys given in the value to be checked is
        performed
    */
    private static bool matchGeneric(hash<auto> record, hash<DataProviderExpression> where_cond) {
        return AbstractDataProvider::evalGenericExpression(record, where_cond);
    }

    #! Checks if the current record matches the search criteria
    /** in case of a hash value, if the hash was created by a
        @ref dataprovider_generic_search_op_funcs "search operator function", then that search operation is applied to
        the input, otherwise a recursive partial match with only the keys given in the value to be checked is
        performed
    */
    private static bool matchGeneric(hash<auto> record, *hash<auto> where_cond) {
        foreach hash<auto> elem in (where_cond.pairIterator()) {
            if (elem.value.op && (*hash<auto> op = (DefaultGenericSearchOpMap{elem.value.op}
                ?? DefaultGenericSearchOpMap{OperatorAliasMap{elem.value.op}}))) {
                if (!AbstractDataProviderRecordIterator::evalOperator(elem.key, elem.value, op, record)) {
                    return False;
                }
            } else if (!AbstractDataProviderRecordIterator::matchGenericValue(record{elem.key}, elem.value)) {
                return False;
            }
        }
        return True;
    }

    #! Evaluates a generic search operator on the field value and record and returns the result
    private static bool evalOperator(string field, hash<auto> cmd, hash<auto> op, hash<auto> record) {
        auto field_value = record{field};
        if (op.recursive && cmd.arg.op
            && (*hash<auto> op0 = (DefaultGenericSearchOpMap{cmd.arg.op}
                ?? DefaultGenericSearchOpMap{OperatorAliasMap{cmd.arg.op}}))) {
            field_value = AbstractDataProviderRecordIterator::evalOperator(field, cmd.arg, op0, record);
        }
        return op.code(cmd, field_value, field, record);
    }

    #! Match a single value
    /** in case of a hash value, a recursive partial match with only the keys given in the value to be checked is
        performed
    */
    private static bool matchGenericValue(auto expects, auto val) {
        if (val.typeCode() == NT_HASH) {
            # in case of a hash, do a recursive partial match with only the given keys
            foreach hash<auto> i in (val.pairIterator()) {
                if (!AbstractDataProviderRecordIterator::matchGenericValue(expects{i.key}, i.value)) {
                    return False;
                }
            }
            return True;
        }
        return expects == val;
    }
}
}
