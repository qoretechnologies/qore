# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ColumnarExpressionFilter class definition

/** ColumnarExpressionFilter.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProvider module
public namespace DataProvider {

#! Shared DPQL/DataProviderExpression filter engine for columnar data
/** Applies @ref DataProviderExpression "DataProviderExpression" trees to DataFrame and
    @ref Qore::SQL::ColumnarResult "ColumnarResult" blocks.  Common predicates are compiled to DataFrame row masks
    or dense \c buffer<bool> masks; unsupported but valid expressions fall back to row evaluation so callers keep
    DPQL correctness even when a predicate cannot be vectorized yet.

    @since DataProvider 3.5
*/
public class ColumnarExpressionFilter {
    #! Filters a DataFrame block by an expression
    /** @param df the DataFrame block to filter
        @param expr prepared or unprepared filter expression

        @return a filtered DataFrame block

        @since DataProvider 3.5
    */
    static auto filterDataFrame(auto df, hash<DataProviderExpression> expr) {
        return filterDataFrameExpression(df, expr);
    }

    #! Filters a ColumnarResult block by an expression
    /** @param records the ColumnarResult block to filter
        @param expr prepared or unprepared filter expression

        @return a filtered ColumnarResult block

        @since DataProvider 3.5
    */
    static auto filterBufferColumns(auto records, hash<DataProviderExpression> expr) {
        *auto mask = buildBufferMask(records, expr);
        if (exists mask) {
            return records.filter(mask);
        }

        auto df = AbstractDataProcessor::getDataFrameClass().newObject(records.toColumnHash());
        return filterDataFrameExpression(df, expr).toColumnarResult();
    }

    #! Filters a DataFrame block by one expression
    private static auto filterDataFrameExpression(auto df, hash<DataProviderExpression> expr) {
        *auto mask = buildDataFrameMask(df, expr);
        if (exists mask) {
            return df.filter(mask);
        }

        string op = OperatorAliasMap{expr.exp} ?? expr.exp;
        if (op == DP_OP_AND || expr.exp == "AND") {
            auto filtered = df;
            foreach auto arg in (expr.args) {
                if (!(arg instanceof hash<DataProviderExpression>)) {
                    return filterDataFrameViaRows(df, expr);
                }
                filtered = filterDataFrameExpression(filtered, arg);
            }
            return filtered;
        }

        *hash<auto> comparison = getDataFrameComparison(expr, op);
        if (!comparison) {
            return filterDataFrameViaRows(df, expr);
        }
        return applyDataFrameComparison(df, expr, comparison.field, comparison.op, comparison.value);
    }

    #! Builds a vectorized DataFrame row mask, if the expression is supported
    private static *auto buildDataFrameMask(auto df, hash<DataProviderExpression> expr) {
        string op = OperatorAliasMap{expr.exp} ?? expr.exp;
        if (op == DP_OP_AND || op == DP_OP_OR) {
            *auto combined;
            foreach auto arg in (expr.args) {
                if (!(arg instanceof hash<DataProviderExpression>)) {
                    return;
                }
                *auto mask = buildDataFrameMask(df, arg);
                if (!exists mask) {
                    return;
                }
                combined = !exists combined
                    ? mask
                    : (op == DP_OP_AND ? combined.intersect(mask) : combined.unionWith(mask));
            }
            return combined;
        }

        if (op == DP_SEARCH_OP_NOT) {
            if (expr.args.size() != 1 || !(expr.args[0] instanceof hash<DataProviderExpression>)) {
                return;
            }
            *auto mask = buildDataFrameMask(df, expr.args[0]);
            if (exists mask) {
                return mask.invert();
            }
            return;
        }

        *string null_field = getNullCheckField(expr, op);
        if (null_field) {
            return df.column(null_field).compare(op == DP_OP_IS_NULL ? "==" : "!=", NOTHING);
        }

        *hash<auto> between = getBetweenComparison(expr, op);
        if (between) {
            return buildDataFrameBetweenMask(df, between{"field"}, between{"low"}, between{"high"});
        }

        *hash<auto> in_comparison = getInComparison(expr, op);
        if (in_comparison) {
            return buildDataFrameInMask(df, in_comparison{"field"}, in_comparison{"values"},
                in_comparison{"negated"});
        }

        *hash<auto> string_predicate = getStringPredicate(expr, op);
        if (string_predicate) {
            return buildDataFrameStringMask(df, string_predicate.field, string_predicate.op,
                string_predicate.value, string_predicate.ignore_case);
        }

        *hash<auto> pattern_predicate = getPatternPredicate(expr, op);
        if (pattern_predicate) {
            return buildDataFramePatternMask(df, pattern_predicate.field, pattern_predicate.pattern,
                pattern_predicate.options, pattern_predicate.negated);
        }

        *hash<auto> comparison = getDataFrameComparison(expr, op);
        if (!comparison) {
            return;
        }
        return buildDataFrameComparisonMask(df, comparison.field, comparison.op, comparison.value);
    }

    #! Builds a DataFrame row mask for one scalar comparison, if supported
    private static *auto buildDataFrameComparisonMask(auto df, string field, string op, auto value) {
        switch (op) {
            case DP_SEARCH_OP_EQ:
            case DP_SEARCH_OP_NE:
            case DP_SEARCH_OP_GT:
            case DP_SEARCH_OP_GE:
            case DP_SEARCH_OP_LT:
            case DP_SEARCH_OP_LE:
                return df.column(field).compare(op, value);
        }
        return;
    }

    #! Builds a DataFrame row mask for one between expression, if supported
    private static *auto buildDataFrameBetweenMask(auto df, string field, auto low, auto high) {
        auto lower = df.column(field).compare(DP_SEARCH_OP_GE, low);
        auto upper = df.column(field).compare(DP_SEARCH_OP_LE, high);
        return lower.intersect(upper);
    }

    #! Builds a DataFrame row mask for one in/not-in expression, if supported
    private static *auto buildDataFrameInMask(auto df, string field, list<auto> values, bool negated) {
        *auto combined;
        foreach auto value in (values) {
            auto mask = df.column(field).compare(DP_SEARCH_OP_EQ, value);
            combined = !exists combined ? mask : combined.unionWith(mask);
        }
        return exists combined ? (negated ? combined.invert() : combined) : NOTHING;
    }

    #! Builds a boolean buffer/list mask for a ColumnarResult, if supported
    private static *auto buildBufferMask(auto records, hash<DataProviderExpression> expr) {
        string op = OperatorAliasMap{expr.exp} ?? expr.exp;
        if (op == DP_OP_AND || op == DP_OP_OR) {
            *auto combined;
            foreach auto arg in (expr.args) {
                if (!(arg instanceof hash<DataProviderExpression>)) {
                    return;
                }
                *auto mask = buildBufferMask(records, arg);
                if (!exists mask) {
                    return;
                }
                combined = !exists combined ? mask : records.combineMasks(combined, mask, op);
            }
            return combined;
        }

        if (op == DP_SEARCH_OP_NOT) {
            if (expr.args.size() != 1 || !(expr.args[0] instanceof hash<DataProviderExpression>)) {
                return;
            }
            *auto mask = buildBufferMask(records, expr.args[0]);
            if (exists mask) {
                return records.invertMask(mask);
            }
            return;
        }

        *string null_field = getNullCheckField(expr, op);
        if (null_field) {
            return records.nullMask(null_field, op == DP_OP_IS_NOT_NULL);
        }

        *hash<auto> between = getBetweenComparison(expr, op);
        if (between) {
            return buildBufferBetweenMask(records, between{"field"}, between{"low"}, between{"high"});
        }

        *hash<auto> in_comparison = getInComparison(expr, op);
        if (in_comparison) {
            return buildBufferInMask(records, in_comparison{"field"}, in_comparison{"values"},
                in_comparison{"negated"});
        }

        *hash<auto> string_predicate = getStringPredicate(expr, op);
        if (string_predicate) {
            return buildBufferStringMask(records, string_predicate.field, string_predicate.op,
                string_predicate.value, string_predicate.ignore_case);
        }

        *hash<auto> pattern_predicate = getPatternPredicate(expr, op);
        if (pattern_predicate) {
            return buildBufferPatternMask(records, pattern_predicate.field, pattern_predicate.pattern,
                pattern_predicate.options, pattern_predicate.negated);
        }

        *hash<auto> comparison = getDataFrameComparison(expr, op);
        if (!comparison || !exists comparison.value || comparison.value.typeCode() == NT_NULL) {
            return;
        }
        return buildBufferComparisonMask(records, comparison.field, comparison.op, comparison.value);
    }

    #! Builds a boolean buffer mask for one dense column comparison
    private static *auto buildBufferComparisonMask(auto records, string field, string op, auto value) {
        auto column = records.getColumn(field);
        if (column.typeCode() != NT_BUFFER) {
            return;
        }

        switch (op) {
            case DP_SEARCH_OP_EQ:
                return column == value;
            case DP_SEARCH_OP_NE:
                return column != value;
            case DP_SEARCH_OP_GT:
                return column > value;
            case DP_SEARCH_OP_GE:
                return column >= value;
            case DP_SEARCH_OP_LT:
                return column < value;
            case DP_SEARCH_OP_LE:
                return column <= value;
        }
        return;
    }

    #! Builds a boolean buffer mask for one dense between expression
    private static *auto buildBufferBetweenMask(auto records, string field, auto low, auto high) {
        auto column = records.getColumn(field);
        if (column.typeCode() != NT_BUFFER) {
            return;
        }
        return records.combineMasks(column >= low, column <= high, DP_OP_AND);
    }

    #! Builds a boolean buffer mask for one dense in/not-in expression
    private static *auto buildBufferInMask(auto records, string field, list<auto> values, bool negated) {
        auto column = records.getColumn(field);
        if (column.typeCode() != NT_BUFFER) {
            return;
        }

        *auto combined;
        foreach auto value in (values) {
            auto mask = column == value;
            combined = !exists combined ? mask : records.combineMasks(combined, mask, DP_OP_OR);
        }
        if (!exists combined) {
            return;
        }
        return negated ? records.invertMask(combined) : combined;
    }

    #! Builds a DataFrame row mask for one string predicate that ColumnRef supports directly
    private static *auto buildDataFrameStringMask(auto df, string field, string op, auto value, bool ignore_case) {
        if (ignore_case) {
            return buildDataFrameStringListMask(df, field, op, value, True);
        }

        switch (op) {
            case DP_OP_CONTAINS:
                return df.column(field).compare("contains", value);
            case DP_OP_STARTS_WITH:
                return df.column(field).compare("startswith", value);
            case DP_OP_ENDS_WITH:
                return df.column(field).compare("endswith", value);
        }
        return;
    }

    #! Builds a DataFrame row mask for one case-insensitive string predicate
    private static *auto buildDataFrameStringListMask(auto df, string field, string op, auto value, bool ignore_case) {
        softstring needle = value;
        if (ignore_case) {
            needle = needle.upr();
        }

        list<bool> mask = ();
        foreach auto raw in (df.column(field).values()) {
            if (!exists raw || raw.typeCode() == NT_NULL) {
                mask += False;
                continue;
            }
            softstring haystack = raw;
            if (ignore_case) {
                haystack = haystack.upr();
            }

            bool match;
            switch (op) {
                case DP_OP_CONTAINS:
                    match = haystack.contains(needle);
                    break;
                case DP_OP_STARTS_WITH:
                    match = haystack.startsWith(needle);
                    break;
                case DP_OP_ENDS_WITH:
                    match = haystack.endsWith(needle);
                    break;
                default:
                    return;
            }
            mask += match;
        }
        return makeDataFrameRowMask(mask);
    }

    #! Builds a DataFrame row mask for one LIKE or regex predicate
    private static *auto buildDataFramePatternMask(auto df, string field, string pattern, int options,
            bool negated) {
        list<bool> mask = ();
        foreach auto raw in (df.column(field).values()) {
            if (!exists raw || raw.typeCode() == NT_NULL) {
                mask += negated;
                continue;
            }
            softstring value = raw;
            bool match = regex(value, pattern, options);
            mask += negated ? !match : match;
        }
        return makeDataFrameRowMask(mask);
    }

    #! Creates a DataFrame RowMask without a compile-time dependency on the dataframe module
    private static auto makeDataFrameRowMask(list<bool> mask) {
        load_module("dataframe");
        return Class::forName("DataFrame::RowMask").newObject(mask);
    }

    #! Builds a boolean buffer mask for one string predicate
    private static *auto buildBufferStringMask(auto records, string field, string op, auto value, bool ignore_case) {
        auto column = records.getColumn(field);
        if (column.typeCode() != NT_BUFFER && column.typeCode() != NT_LIST) {
            return;
        }

        softstring needle = value;
        if (ignore_case) {
            needle = needle.upr();
        }

        list<bool> mask = ();
        foreach auto raw in (column) {
            if (!exists raw || raw.typeCode() == NT_NULL) {
                mask += False;
                continue;
            }
            softstring haystack = raw;
            if (ignore_case) {
                haystack = haystack.upr();
            }

            bool match;
            switch (op) {
                case DP_OP_CONTAINS:
                    match = haystack.contains(needle);
                    break;
                case DP_OP_STARTS_WITH:
                    match = haystack.startsWith(needle);
                    break;
                case DP_OP_ENDS_WITH:
                    match = haystack.endsWith(needle);
                    break;
                default:
                    return;
            }
            mask += match;
        }
        return new buffer<bool>(cast<list<bool>>(mask));
    }

    #! Builds a boolean buffer mask for one LIKE or regex predicate
    private static *auto buildBufferPatternMask(auto records, string field, string pattern, int options,
            bool negated) {
        auto column = records.getColumn(field);
        if (column.typeCode() != NT_BUFFER && column.typeCode() != NT_LIST) {
            return;
        }

        list<bool> mask = ();
        foreach auto raw in (column) {
            if (!exists raw || raw.typeCode() == NT_NULL) {
                mask += negated;
                continue;
            }
            softstring value = raw;
            bool match = regex(value, pattern, options);
            mask += negated ? !match : match;
        }
        return new buffer<bool>(cast<list<bool>>(mask));
    }

    #! Returns the field name for a unary null-check expression, if supported
    private static *string getNullCheckField(hash<DataProviderExpression> expr, string op) {
        if (op != DP_OP_IS_NULL && op != DP_OP_IS_NOT_NULL) {
            return;
        }
        if (expr.args.size() != 1 || !isTopLevelFieldReference(expr.args[0])) {
            return;
        }
        string field = expr.args[0].field;
        return field;
    }

    #! Returns vectorizable between-expression information, if supported
    private static *hash<auto> getBetweenComparison(hash<DataProviderExpression> expr, string op) {
        if (op != DP_SEARCH_OP_BETWEEN || expr.args.size() != 3
                || !isTopLevelFieldReference(expr.args[0])
                || !isScalarComparisonArg(expr.args[1]) || !isScalarComparisonArg(expr.args[2])) {
            return;
        }
        return {
            "field": expr.args[0].field,
            "low": unwrapComparisonValue(expr.args[1]),
            "high": unwrapComparisonValue(expr.args[2]),
        };
    }

    #! Returns vectorizable in/not-in expression information, if supported
    private static *hash<auto> getInComparison(hash<DataProviderExpression> expr, string op) {
        if ((op != DP_SEARCH_OP_IN && op != DP_OP_NOT_IN) || expr.args.size() < 2
                || !isTopLevelFieldReference(expr.args[0])) {
            return;
        }

        list<auto> values();
        if (expr.args.size() == 2) {
            auto arg = unwrapComparisonValue(expr.args[1]);
            if (arg.typeCode() == NT_LIST) {
                foreach auto value in (arg) {
                    if (!isScalarComparisonArg(value)) {
                        return;
                    }
                    push values, unwrapComparisonValue(value);
                }
            } else {
                if (!isScalarComparisonArg(expr.args[1])) {
                    return;
                }
                push values, arg;
            }
        } else {
            for (int i = 1; i < expr.args.size(); ++i) {
                if (!isScalarComparisonArg(expr.args[i])) {
                    return;
                }
                push values, unwrapComparisonValue(expr.args[i]);
            }
        }
        return {
            "field": expr.args[0].field,
            "values": values,
            "negated": op == DP_OP_NOT_IN,
        };
    }

    #! Returns vectorizable string predicate information, if supported
    private static *hash<auto> getStringPredicate(hash<DataProviderExpression> expr, string op) {
        if (op != DP_OP_CONTAINS && op != DP_OP_STARTS_WITH && op != DP_OP_ENDS_WITH) {
            return;
        }
        if (expr.args.size() < 2 || expr.args.size() > 3 || !isTopLevelFieldReference(expr.args[0])
                || !isScalarComparisonArg(expr.args[1])) {
            return;
        }

        bool ignore_case = False;
        if (expr.args.size() == 3) {
            if (!isScalarComparisonArg(expr.args[2])) {
                return;
            }
            ignore_case = unwrapComparisonValue(expr.args[2]) ? True : False;
        }

        return {
            "field": expr.args[0].field,
            "op": op,
            "value": unwrapComparisonValue(expr.args[1]),
            "ignore_case": ignore_case,
        };
    }

    #! Returns vectorizable pattern predicate information, if supported by BufferColumns
    private static *hash<auto> getPatternPredicate(hash<DataProviderExpression> expr, string op) {
        if (op == DP_OP_LIKE) {
            if (expr.args.size() < 2 || expr.args.size() > 3 || !isTopLevelFieldReference(expr.args[0])
                    || !isScalarComparisonArg(expr.args[1])) {
                return;
            }
            bool ignore_case = False;
            if (expr.args.size() == 3) {
                if (!isScalarComparisonArg(expr.args[2])) {
                    return;
                }
                ignore_case = unwrapComparisonValue(expr.args[2]) ? True : False;
            }
            string pattern = unwrapComparisonValue(expr.args[1]);
            string re = regex_subst(pattern, "([.^$+?{}\\[\\]|()\\\\])", "\\\\$1", RE_Global);
            re = regex_subst(re, "%", ".*", RE_Global);
            re = regex_subst(re, "_", ".", RE_Global);
            return {
                "field": expr.args[0].field,
                "pattern": "^" + re + "$",
                "options": ignore_case ? RE_Caseless : 0,
                "negated": False,
            };
        }

        if (op == DP_OP_REGEX_MATCH || op == DP_OP_REGEX_NMATCH) {
            if (expr.args.size() < 2 || expr.args.size() > 3 || !isTopLevelFieldReference(expr.args[0])
                    || !isScalarComparisonArg(expr.args[1])) {
                return;
            }
            int opts = 0;
            if (expr.args.size() == 3) {
                auto flags = unwrapComparisonValue(expr.args[2]);
                if (flags.typeCode() != NT_LIST) {
                    return;
                }
                opts = foldl $1 | $2, (map RegexOpts{$1}, flags);
            }
            return {
                "field": expr.args[0].field,
                "pattern": unwrapComparisonValue(expr.args[1]),
                "options": opts,
                "negated": op == DP_OP_REGEX_NMATCH,
            };
        }

        return;
    }

    #! Returns vectorizable DataFrame comparison information, if supported
    private static *hash<auto> getDataFrameComparison(hash<DataProviderExpression> expr, string op) {
        if (expr.args.size() != 2) {
            return;
        }

        auto lhs = expr.args[0];
        auto rhs = expr.args[1];
        if (isTopLevelFieldReference(lhs) && isScalarComparisonArg(rhs)) {
            return {"field": lhs.field, "op": op, "value": unwrapComparisonValue(rhs)};
        }
        if (isTopLevelFieldReference(rhs) && isScalarComparisonArg(lhs)) {
            *string inverted_op = invertComparisonOperator(op);
            if (!inverted_op) {
                return;
            }
            return {"field": rhs.field, "op": inverted_op, "value": unwrapComparisonValue(lhs)};
        }
        return;
    }

    #! Returns True if the argument is a top-level field reference
    private static bool isTopLevelFieldReference(auto arg) {
        return arg instanceof hash<DataProviderFieldReference> && arg.field.find(".") < 0;
    }

    #! Returns True if the argument can be used as a scalar comparison value
    private static bool isScalarComparisonArg(auto arg) {
        return !(arg instanceof hash<DataProviderExpression>)
            && !(arg instanceof hash<DataProviderFieldReference>);
    }

    #! Unwraps serialized literal expression arguments
    private static auto unwrapComparisonValue(auto arg) {
        if (arg.typeCode() == NT_HASH && arg.hasKey("value") && !arg.hasKey("field") && !arg.hasKey("exp")) {
            return arg.value;
        }
        return arg;
    }

    #! Inverts a comparison operator when the field reference is on the right side
    private static *string invertComparisonOperator(string op) {
        switch (op) {
            case DP_SEARCH_OP_EQ:
                return DP_SEARCH_OP_EQ;
            case DP_SEARCH_OP_NE:
                return DP_SEARCH_OP_NE;
            case DP_SEARCH_OP_GT:
                return DP_SEARCH_OP_LT;
            case DP_SEARCH_OP_GE:
                return DP_SEARCH_OP_LE;
            case DP_SEARCH_OP_LT:
                return DP_SEARCH_OP_GT;
            case DP_SEARCH_OP_LE:
                return DP_SEARCH_OP_GE;
        }
        return;
    }

    #! Applies a vectorized comparison to a DataFrame block
    private static auto applyDataFrameComparison(auto df, hash<DataProviderExpression> expr, string field, string op,
            auto value) {
        switch (op) {
            case DP_SEARCH_OP_EQ:
            case DP_SEARCH_OP_NE:
            case DP_SEARCH_OP_GT:
            case DP_SEARCH_OP_GE:
            case DP_SEARCH_OP_LT:
            case DP_SEARCH_OP_LE:
                return df.filter(field, op, value);
            case DP_OP_CONTAINS:
                return df.filter(field, "contains", value);
            case DP_OP_STARTS_WITH:
                return df.filter(field, "startswith", value);
            case DP_OP_ENDS_WITH:
                return df.filter(field, "endswith", value);
        }
        return filterDataFrameViaRows(df, expr);
    }

    #! Fallback path for expressions that are not vectorized yet
    private static auto filterDataFrameViaRows(auto df, hash<DataProviderExpression> expr) {
        list<auto> filtered();
        foreach hash<auto> rec in (df.toRecords()) {
            if (AbstractDataProvider::evalGenericExpression(rec, expr)) {
                push filtered, rec;
            }
        }
        if (!filtered) {
            return df.head(0);
        }
        return Class::getClass(df).newObject(filtered);
    }
}
}
