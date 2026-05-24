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
        *auto columnar_filtered = filterDataFrameWithBufferMask(df, expr);
        if (exists columnar_filtered) {
            return columnar_filtered;
        }

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

    #! Filters a DataFrame through ColumnarResult when the expression has a dense mask implementation
    private static *auto filterDataFrameWithBufferMask(auto df, hash<DataProviderExpression> expr) {
        auto records = df.toColumnarResult();
        *auto mask = buildBufferMask(records, expr);
        if (!exists mask) {
            return;
        }

        return Class::getClass(df).newObject(records.filter(mask).toColumnHash());
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

        *auto truthy_mask = buildDataFrameTruthyMask(df, expr, op);
        if (exists truthy_mask) {
            return truthy_mask;
        }

        *string null_field = getNullCheckField(expr, op);
        if (null_field) {
            return df.column(null_field).compare(op == DP_OP_IS_NULL ? "==" : "!=", NOTHING);
        }

        *hash<auto> range = getRangeComparison(expr, op);
        if (range) {
            return buildDataFrameRangeMask(df, range{"expr"}, range{"low"}, range{"high"});
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
            return buildDataFrameStringMask(df, string_predicate.expr, string_predicate.op,
                string_predicate.value, string_predicate.ignore_case, string_predicate.locale_options);
        }

        *hash<auto> pattern_predicate = getPatternPredicate(expr, op);
        if (pattern_predicate) {
            return buildDataFramePatternMask(df, pattern_predicate.expr, pattern_predicate.pattern,
                pattern_predicate.options, pattern_predicate.negated);
        }

        *hash<auto> comparison = getDataFrameComparison(expr, op);
        if (comparison) {
            return buildDataFrameComparisonMask(df, comparison.field, comparison.op, comparison.value);
        }

        *hash<auto> computed_comparison = getComputedComparison(expr, op);
        if (computed_comparison) {
            return buildDataFrameComputedComparisonMask(df, computed_comparison{"expr"}, computed_comparison{"op"},
                computed_comparison{"value"});
        }

        *hash<auto> vector_comparison = getVectorComparison(expr, op);
        if (vector_comparison) {
            return buildDataFrameVectorComparisonMask(df, vector_comparison{"lhs"}, vector_comparison{"op"},
                vector_comparison{"rhs"});
        }
        return;
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

    #! Builds a DataFrame row mask for an inRange expression, if supported
    private static *auto buildDataFrameRangeMask(auto df, auto value_expr, auto low, auto high) {
        auto lower = buildDataFrameComputedComparisonMask(df, value_expr, DP_SEARCH_OP_GE, low);
        auto upper = buildDataFrameComputedComparisonMask(df, value_expr, DP_SEARCH_OP_LT, high);
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

    #! Builds a DataFrame truthiness mask from a field or supported computed expression
    private static *auto buildDataFrameTruthyMask(auto df, hash<DataProviderExpression> expr, string op) {
        if ((op != DP_EXPR_VALUE && op != DP_OP_HAS_VALUE) || expr.args.size() != 1
                || !isVectorizableValueArg(expr.args[0])) {
            return;
        }

        hash<auto> column_cache;
        list<bool> mask = ();
        int rows = df.shape().rows;
        for (int i = 0; i < rows; ++i) {
            push mask, valueTruthy(evalDataFrameExpressionValue(df, expr.args[0], i, \column_cache));
        }
        return makeDataFrameRowMask(mask);
    }

    #! Builds a DataFrame row mask for one computed expression comparison
    private static *auto buildDataFrameComputedComparisonMask(auto df, auto value_expr, string op, auto value) {
        *auto mask = buildDataFrameBufferComputedComparisonMask(df, value_expr, op, value);
        if (exists mask) {
            return mask;
        }

        hash<auto> column_cache;
        return buildDataFrameExpressionComparisonMask(df, value_expr, op, value, \column_cache);
    }

    #! Builds a DataFrame row mask for one expression-to-expression comparison
    private static auto buildDataFrameVectorComparisonMask(auto df, auto lhs_expr, string op, auto rhs_expr) {
        *auto dense_mask = buildDataFrameBufferVectorComparisonMask(df, lhs_expr, op, rhs_expr);
        if (exists dense_mask) {
            return dense_mask;
        }

        hash<auto> column_cache;
        list<bool> mask = ();
        int rows = df.shape().rows;
        for (int i = 0; i < rows; ++i) {
            push mask, compareScalar(
                evalDataFrameExpressionValue(df, lhs_expr, i, \column_cache),
                op,
                evalDataFrameExpressionValue(df, rhs_expr, i, \column_cache));
        }
        return makeDataFrameRowMask(mask);
    }

    #! Builds a DataFrame row mask through ColumnarResult dense expression evaluation
    private static *auto buildDataFrameBufferComputedComparisonMask(auto df, auto value_expr, string op, auto value) {
        auto records = df.toColumnarResult();
        *auto mask = buildBufferComputedComparisonMask(records, value_expr, op, value);
        if (exists mask) {
            return makeDataFrameRowMaskFromMask(mask);
        }
        return;
    }

    #! Builds a DataFrame expression-to-expression row mask through ColumnarResult dense expression evaluation
    private static *auto buildDataFrameBufferVectorComparisonMask(auto df, auto lhs_expr, string op, auto rhs_expr) {
        auto records = df.toColumnarResult();
        *auto mask = buildBufferVectorComparisonMask(records, lhs_expr, op, rhs_expr);
        if (exists mask) {
            return makeDataFrameRowMaskFromMask(mask);
        }
        return;
    }

    #! Builds a DataFrame row mask from one expression compared with a scalar
    private static auto buildDataFrameExpressionComparisonMask(auto df, auto value_expr, string op, auto value,
            reference<hash<auto>> column_cache) {
        list<bool> mask = ();
        int rows = df.shape().rows;
        for (int i = 0; i < rows; ++i) {
            push mask, compareScalar(evalDataFrameExpressionValue(df, value_expr, i, \column_cache), op, value);
        }
        return makeDataFrameRowMask(mask);
    }

    #! Evaluates a supported expression against one DataFrame row without materializing row hashes
    private static auto evalDataFrameExpressionValue(auto df, auto arg, int row,
            reference<hash<auto>> column_cache) {
        if (isTopLevelFieldReference(arg)) {
            return getDataFrameColumnValues(df, arg.field, \column_cache)[row];
        }

        if (arg instanceof hash<DataProviderExpression>) {
            string op = OperatorAliasMap{arg.exp} ?? arg.exp;
            if (op == DP_EXPR_VALUE) {
                return evalDataFrameExpressionValue(df, arg.args[0], row, \column_cache);
            }
            if (isArithmeticOperator(op)) {
                auto result = evalDataFrameExpressionValue(df, arg.args[0], row, \column_cache);
                for (int i = 1; i < arg.args.size(); ++i) {
                    result = applyArithmetic(op, result,
                        evalDataFrameExpressionValue(df, arg.args[i], row, \column_cache));
                }
                return result;
            }
            switch (op) {
                case DP_OP_TO_LOWER: {
                    softstring value = evalDataFrameExpressionValue(df, arg.args[0], row, \column_cache);
                    return value.lwr();
                }
                case DP_OP_TO_UPPER: {
                    softstring value = evalDataFrameExpressionValue(df, arg.args[0], row, \column_cache);
                    return value.upr();
                }
                case DP_FUNC_COALESCE:
                    foreach auto sub_arg in (arg.args) {
                        auto value = evalDataFrameExpressionValue(df, sub_arg, row, \column_cache);
                        if (exists value && value !== NULL) {
                            return value;
                        }
                    }
                    return NOTHING;
            }
        }

        return unwrapComparisonValue(arg);
    }

    #! Returns cached DataFrame column values for expression evaluation
    private static list<auto> getDataFrameColumnValues(auto df, string field, reference<hash<auto>> column_cache) {
        if (!column_cache.hasKey(field)) {
            column_cache{field} = df.column(field).values();
        }
        return column_cache{field};
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

        *auto truthy_mask = buildBufferTruthyMask(records, expr, op);
        if (exists truthy_mask) {
            return truthy_mask;
        }

        *string null_field = getNullCheckField(expr, op);
        if (null_field) {
            return records.nullMask(null_field, op == DP_OP_IS_NOT_NULL);
        }

        *hash<auto> range = getRangeComparison(expr, op);
        if (range) {
            return buildBufferRangeMask(records, range{"expr"}, range{"low"}, range{"high"});
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
            return buildBufferStringMask(records, string_predicate.expr, string_predicate.op,
                string_predicate.value, string_predicate.ignore_case, string_predicate.locale_options);
        }

        *hash<auto> pattern_predicate = getPatternPredicate(expr, op);
        if (pattern_predicate) {
            return buildBufferPatternMask(records, pattern_predicate.expr, pattern_predicate.pattern,
                pattern_predicate.options, pattern_predicate.negated);
        }

        *hash<auto> comparison = getDataFrameComparison(expr, op);
        if (!comparison || !exists comparison.value || comparison.value.typeCode() == NT_NULL) {
            *hash<auto> computed_comparison = getComputedComparison(expr, op);
            if (computed_comparison) {
                return buildBufferComputedComparisonMask(records, computed_comparison{"expr"},
                    computed_comparison{"op"}, computed_comparison{"value"});
            }
            *hash<auto> vector_comparison = getVectorComparison(expr, op);
            if (vector_comparison) {
                return buildBufferVectorComparisonMask(records, vector_comparison{"lhs"},
                    vector_comparison{"op"}, vector_comparison{"rhs"});
            }
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
        value = normalizeColumnarComparisonValue(records, field, value);

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
        low = normalizeColumnarComparisonValue(records, field, low);
        high = normalizeColumnarComparisonValue(records, field, high);
        return records.combineMasks(column >= low, column <= high, DP_OP_AND);
    }

    #! Builds a boolean buffer mask for an inRange expression, if supported
    private static *auto buildBufferRangeMask(auto records, auto value_expr, auto low, auto high) {
        *auto lower = buildBufferComputedComparisonMask(records, value_expr, DP_SEARCH_OP_GE, low);
        if (!exists lower) {
            return;
        }
        *auto upper = buildBufferComputedComparisonMask(records, value_expr, DP_SEARCH_OP_LT, high);
        if (!exists upper) {
            return;
        }
        return records.combineMasks(lower, upper, DP_OP_AND);
    }

    #! Builds a boolean buffer mask for one dense in/not-in expression
    private static *auto buildBufferInMask(auto records, string field, list<auto> values, bool negated) {
        auto column = records.getColumn(field);
        if (column.typeCode() != NT_BUFFER) {
            return;
        }

        *auto combined;
        foreach auto value in (values) {
            value = normalizeColumnarComparisonValue(records, field, value);
            auto mask = column == value;
            combined = !exists combined ? mask : records.combineMasks(combined, mask, DP_OP_OR);
        }
        if (!exists combined) {
            return;
        }
        return negated ? records.invertMask(combined) : combined;
    }

    #! Converts Qore date comparison values to dense temporal microseconds for ColumnarResult fields
    private static auto normalizeColumnarComparisonValue(auto records, string field, auto value) {
        if (!AbstractDataProcessor::isColumnarResult(records)) {
            return value;
        }
        *hash<auto> schema = getColumnarResultFieldSchema(records, field);
        if (!isDenseTemporalSchema(schema) || !exists value || value.typeCode() == NT_NULL
                || value.typeCode() != NT_DATE) {
            return value;
        }
        return schema.kind == "duration" ? get_duration_microseconds(value) : get_epoch_microseconds(value);
    }

    #! Returns recursive schema metadata for a ColumnarResult field, if available
    private static *hash<auto> getColumnarResultFieldSchema(auto records, string field) {
        foreach hash<auto> schema in (records.schemaV2()) {
            if (schema.name == field) {
                return schema;
            }
        }
        return;
    }

    #! Returns True if schema represents dense int64 temporal storage
    private static bool isDenseTemporalSchema(*hash<auto> schema) {
        if (!schema) {
            return False;
        }
        return (schema.kind == "date" || schema.kind == "timestamp" || schema.kind == "duration")
            && (!schema.hasKey("buffer_type") || schema.buffer_type == "int64");
    }

    #! Builds a boolean buffer mask from a field or supported computed expression
    private static *auto buildBufferTruthyMask(auto records, hash<DataProviderExpression> expr, string op) {
        if ((op != DP_EXPR_VALUE && op != DP_OP_HAS_VALUE) || expr.args.size() != 1
                || !isVectorizableValueArg(expr.args[0])) {
            return;
        }

        *hash<auto> value = evalBufferExpressionValue(records, expr.args[0]);
        if (!value || !value.vector) {
            return;
        }
        return buildTruthyMaskFromVector(value.value);
    }

    #! Builds a boolean buffer mask for one computed expression comparison
    private static *auto buildBufferComputedComparisonMask(auto records, auto value_expr, string op, auto value) {
        *hash<auto> vector = evalBufferExpressionValue(records, value_expr);
        if (!vector || !vector.vector) {
            return;
        }
        return compareVectorToMask(vector.value, op, value);
    }

    #! Builds a boolean buffer mask for one expression-to-expression comparison
    private static *auto buildBufferVectorComparisonMask(auto records, auto lhs_expr, string op, auto rhs_expr) {
        *hash<auto> lhs = evalBufferExpressionValue(records, lhs_expr);
        *hash<auto> rhs = evalBufferExpressionValue(records, rhs_expr);
        if (!lhs || !rhs || !lhs.vector || !rhs.vector) {
            return;
        }
        return compareVectorsToMask(lhs.value, op, rhs.value);
    }

    #! Evaluates a supported expression against a ColumnarResult, preserving dense buffers when possible
    private static *hash<auto> evalBufferExpressionValue(auto records, auto arg) {
        if (isTopLevelFieldReference(arg)) {
            return {"value": records.getColumn(arg.field), "vector": True};
        }

        if (arg instanceof hash<DataProviderExpression>) {
            string op = OperatorAliasMap{arg.exp} ?? arg.exp;
            if (op == DP_EXPR_VALUE) {
                return evalBufferExpressionValue(records, arg.args[0]);
            }
            if (isArithmeticOperator(op)) {
                return evalBufferArithmeticExpression(records, arg, op);
            }
            switch (op) {
                case DP_OP_TO_LOWER:
                case DP_OP_TO_UPPER:
                case DP_FUNC_COALESCE:
                    return evalBufferExpressionAsList(records, arg);
            }
        }

        return {"value": unwrapComparisonValue(arg), "vector": False};
    }

    #! Evaluates a supported arithmetic expression against a ColumnarResult
    private static *hash<auto> evalBufferArithmeticExpression(auto records, hash<DataProviderExpression> expr,
            string op) {
        *hash<auto> result = evalBufferExpressionValue(records, expr.args[0]);
        if (!result) {
            return;
        }

        for (int i = 1; i < expr.args.size(); ++i) {
            *hash<auto> rhs = evalBufferExpressionValue(records, expr.args[i]);
            if (!rhs) {
                return;
            }
            if (result.vector || rhs.vector) {
                if (result.value.typeCode() == NT_BUFFER
                        && (!rhs.vector || rhs.value.typeCode() == NT_BUFFER)) {
                    result = {"value": applyArithmetic(op, result.value, rhs.value), "vector": True};
                    continue;
                }
                if (!result.vector && rhs.value.typeCode() == NT_BUFFER) {
                    result = {"value": applyArithmetic(op, result.value, rhs.value), "vector": True};
                    continue;
                }
                return evalBufferExpressionAsList(records, expr);
            }
            result = {"value": applyArithmetic(op, result.value, rhs.value), "vector": False};
        }
        return result;
    }

    #! Evaluates a supported expression as a list, preserving columnar execution without row hashes
    private static hash<auto> evalBufferExpressionAsList(auto records, auto arg) {
        hash<auto> column_cache;
        list<auto> values = ();
        int rows = records.numRows();
        for (int i = 0; i < rows; ++i) {
            push values, evalBufferExpressionValueAt(records, arg, i, \column_cache);
        }
        return {"value": values, "vector": True};
    }

    #! Evaluates a supported expression against one ColumnarResult row without materializing row hashes
    private static auto evalBufferExpressionValueAt(auto records, auto arg, int row,
            reference<hash<auto>> column_cache) {
        if (isTopLevelFieldReference(arg)) {
            return getBufferColumnValues(records, arg.field, \column_cache)[row];
        }

        if (arg instanceof hash<DataProviderExpression>) {
            string op = OperatorAliasMap{arg.exp} ?? arg.exp;
            if (op == DP_EXPR_VALUE) {
                return evalBufferExpressionValueAt(records, arg.args[0], row, \column_cache);
            }
            if (isArithmeticOperator(op)) {
                auto result = evalBufferExpressionValueAt(records, arg.args[0], row, \column_cache);
                for (int i = 1; i < arg.args.size(); ++i) {
                    result = applyArithmetic(op, result,
                        evalBufferExpressionValueAt(records, arg.args[i], row, \column_cache));
                }
                return result;
            }
            switch (op) {
                case DP_OP_TO_LOWER: {
                    softstring value = evalBufferExpressionValueAt(records, arg.args[0], row, \column_cache);
                    return value.lwr();
                }
                case DP_OP_TO_UPPER: {
                    softstring value = evalBufferExpressionValueAt(records, arg.args[0], row, \column_cache);
                    return value.upr();
                }
                case DP_FUNC_COALESCE:
                    foreach auto sub_arg in (arg.args) {
                        auto value = evalBufferExpressionValueAt(records, sub_arg, row, \column_cache);
                        if (exists value && value !== NULL) {
                            return value;
                        }
                    }
                    return NOTHING;
            }
        }

        return unwrapComparisonValue(arg);
    }

    #! Returns cached ColumnarResult column values for expression evaluation
    private static auto getBufferColumnValues(auto records, string field, reference<hash<auto>> column_cache) {
        if (!column_cache.hasKey(field)) {
            column_cache{field} = records.getColumn(field);
        }
        return column_cache{field};
    }

    #! Compares a vector against a scalar and returns a dense boolean mask
    private static *auto compareVectorToMask(auto vector, string op, auto value) {
        if (vector.typeCode() == NT_BUFFER && isNativeBufferComparisonOperator(op)
                && exists value && value.typeCode() != NT_NULL) {
            switch (op) {
                case DP_SEARCH_OP_EQ:
                    return vector == value;
                case DP_SEARCH_OP_NE:
                    return vector != value;
                case DP_SEARCH_OP_GT:
                    return vector > value;
                case DP_SEARCH_OP_GE:
                    return vector >= value;
                case DP_SEARCH_OP_LT:
                    return vector < value;
                case DP_SEARCH_OP_LE:
                    return vector <= value;
            }
        }

        if (vector.typeCode() != NT_BUFFER && vector.typeCode() != NT_LIST) {
            return;
        }
        list<bool> mask = ();
        foreach auto raw in (vector) {
            push mask, compareScalar(raw, op, value);
        }
        return new buffer<bool>(mask);
    }

    #! Compares two vectors and returns a dense boolean mask
    private static *auto compareVectorsToMask(auto lhs, string op, auto rhs) {
        if (lhs.typeCode() == NT_BUFFER && rhs.typeCode() == NT_BUFFER
                && isNativeBufferComparisonOperator(op)) {
            switch (op) {
                case DP_SEARCH_OP_EQ:
                    return lhs == rhs;
                case DP_SEARCH_OP_NE:
                    return lhs != rhs;
                case DP_SEARCH_OP_GT:
                    return lhs > rhs;
                case DP_SEARCH_OP_GE:
                    return lhs >= rhs;
                case DP_SEARCH_OP_LT:
                    return lhs < rhs;
                case DP_SEARCH_OP_LE:
                    return lhs <= rhs;
            }
        }

        if ((lhs.typeCode() != NT_BUFFER && lhs.typeCode() != NT_LIST)
                || (rhs.typeCode() != NT_BUFFER && rhs.typeCode() != NT_LIST)) {
            return;
        }

        if (lhs.size() != rhs.size()) {
            throw "EXPRESSION-FILTER-ERROR", sprintf(
                "cannot compare expression vectors with different lengths: %d and %d", lhs.size(), rhs.size());
        }

        list<bool> mask = ();
        for (int i = 0; i < lhs.size(); ++i) {
            push mask, compareScalar(lhs[i], op, rhs[i]);
        }
        return new buffer<bool>(mask);
    }

    #! Converts a vector to a dense truthiness mask
    private static *auto buildTruthyMaskFromVector(auto vector) {
        if (vector.typeCode() != NT_BUFFER && vector.typeCode() != NT_LIST) {
            return;
        }
        list<bool> mask = ();
        foreach auto raw in (vector) {
            push mask, valueTruthy(raw);
        }
        return new buffer<bool>(mask);
    }

    #! Builds a DataFrame row mask for one string predicate that ColumnRef supports directly
    private static *auto buildDataFrameStringMask(auto df, auto value_expr, string op, auto value, bool ignore_case,
            *hash<auto> locale_options) {
        if (isTopLevelFieldReference(value_expr)) {
            string field = value_expr.field;
            if (locale_options) {
                auto mask = buildLocaleStringPredicateMask(df.column(field).values(), op, value, locale_options);
                return makeDataFrameRowMask(mask.toList());
            }

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

        hash<auto> column_cache;
        auto values = evalDataFrameExpressionAsList(df, value_expr, \column_cache);
        *auto mask = buildStringPredicateMask(values, op, value, ignore_case, locale_options);
        return exists mask ? makeDataFrameRowMaskFromMask(mask) : NOTHING;
    }

    #! Evaluates a supported DataFrame expression as a list without materializing row hashes
    private static list<auto> evalDataFrameExpressionAsList(auto df, auto arg,
            reference<hash<auto>> column_cache) {
        list<auto> values = ();
        int rows = df.shape().rows;
        for (int i = 0; i < rows; ++i) {
            push values, evalDataFrameExpressionValue(df, arg, i, \column_cache);
        }
        return values;
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
                push mask, False;
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
            push mask, match;
        }
        return makeDataFrameRowMask(mask);
    }

    #! Builds a DataFrame row mask for one LIKE or regex predicate
    private static *auto buildDataFramePatternMask(auto df, auto value_expr, string pattern, int options,
            bool negated) {
        if (isTopLevelFieldReference(value_expr)) {
            return makeDataFrameRowMaskFromMask(
                buildPatternMask(df.column(value_expr.field).values(), pattern, options, negated));
        }

        hash<auto> column_cache;
        auto values = evalDataFrameExpressionAsList(df, value_expr, \column_cache);
        *auto mask = buildPatternMask(values, pattern, options, negated);
        return exists mask ? makeDataFrameRowMaskFromMask(mask) : NOTHING;
    }

    #! Creates a DataFrame RowMask without a compile-time dependency on the dataframe module
    private static auto makeDataFrameRowMask(list<bool> mask) {
        load_module("dataframe");
        return Class::forName("DataFrame::RowMask").newObject(mask);
    }

    #! Creates a DataFrame RowMask from a list<bool> or buffer<bool>
    private static auto makeDataFrameRowMaskFromMask(auto mask) {
        return makeDataFrameRowMask(mask.typeCode() == NT_BUFFER ? mask.toList() : mask);
    }

    #! Builds a boolean buffer mask for one string predicate
    private static *auto buildBufferStringMask(auto records, auto value_expr, string op, auto value, bool ignore_case,
            *hash<auto> locale_options) {
        if (isTopLevelFieldReference(value_expr) && !locale_options) {
            return records.stringPredicateMask(value_expr.field, op, value, ignore_case);
        }

        *hash<auto> vector = evalBufferExpressionValue(records, value_expr);
        if (!vector || !vector.vector) {
            return;
        }
        return buildStringPredicateMask(vector.value, op, value, ignore_case, locale_options);
    }

    #! Builds a boolean buffer mask for one string predicate over a vector
    private static *auto buildStringPredicateMask(auto values, string op, auto value, bool ignore_case,
            *hash<auto> locale_options) {
        if (values.typeCode() != NT_BUFFER && values.typeCode() != NT_LIST) {
            return;
        }

        if (locale_options) {
            return buildLocaleStringPredicateMask(values, op, value, locale_options);
        }

        softstring needle = value;
        if (ignore_case) {
            needle = needle.upr();
        }

        list<bool> mask = ();
        foreach auto raw in (values) {
            if (!exists raw || raw.typeCode() == NT_NULL) {
                push mask, False;
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
            push mask, match;
        }
        return new buffer<bool>(mask);
    }

    #! Builds a locale-sensitive string predicate mask with an on-demand i18n dependency
    private static auto buildLocaleStringPredicateMask(auto values, string op, auto value,
            hash<auto> locale_options) {
        try {
            load_module("i18n");
        } catch (hash<ExceptionInfo> ex) {
            throw "EXPRESSION-FILTER-ERROR", sprintf(
                "locale-sensitive string predicate %y requires the i18n module; failed to load it: %s: %s",
                op, ex.err, ex.desc);
        }
        softstring needle = value;
        return call_function_args("I18n::string_predicate_mask",
            (values, op, string(needle), string(locale_options.locale), locale_options));
    }

    #! Builds a boolean buffer mask for one LIKE or regex predicate
    private static *auto buildBufferPatternMask(auto records, auto value_expr, string pattern, int options,
            bool negated) {
        *hash<auto> vector = evalBufferExpressionValue(records, value_expr);
        if (!vector || !vector.vector) {
            return;
        }
        return buildPatternMask(vector.value, pattern, options, negated);
    }

    #! Builds a boolean buffer mask for one LIKE or regex predicate over a vector
    private static *auto buildPatternMask(auto values, string pattern, int options, bool negated) {
        if (values.typeCode() != NT_BUFFER && values.typeCode() != NT_LIST) {
            return;
        }

        list<bool> mask = ();
        foreach auto raw in (values) {
            if (!exists raw || raw.typeCode() == NT_NULL) {
                push mask, negated;
                continue;
            }
            softstring value = raw;
            bool match = regex(value, pattern, options);
            push mask, negated ? !match : match;
        }
        return new buffer<bool>(mask);
    }

    #! Returns True if the value has truthy generic expression semantics
    private static bool valueTruthy(auto value) {
        if (!exists value || value === NULL) {
            return False;
        }
        return value.val();
    }

    #! Compares scalar values using DataProvider comparison semantics
    private static bool compareScalar(auto lhs, string op, auto rhs) {
        bool lhs_null = !exists lhs || lhs === NULL;
        bool rhs_null = !exists rhs || rhs === NULL;
        if (lhs_null) {
            return op == DP_SEARCH_OP_EQ ? rhs_null
                : (op == DP_SEARCH_OP_NE ? !rhs_null
                : (op == DP_OP_ABSOLUTE_EQ ? rhs_null
                : (op == DP_OP_ABSOLUTE_NE ? !rhs_null : False)));
        }
        if (rhs_null) {
            return op == DP_SEARCH_OP_EQ ? False
                : (op == DP_SEARCH_OP_NE ? True
                : (op == DP_OP_ABSOLUTE_EQ ? False
                : (op == DP_OP_ABSOLUTE_NE ? True : False)));
        }

        switch (op) {
            case DP_SEARCH_OP_EQ:
                return lhs == rhs;
            case DP_SEARCH_OP_NE:
                return lhs != rhs;
            case DP_OP_ABSOLUTE_EQ:
                return lhs === rhs;
            case DP_OP_ABSOLUTE_NE:
                return lhs !== rhs;
            case DP_SEARCH_OP_GT:
                return lhs > rhs;
            case DP_SEARCH_OP_GE:
                return lhs >= rhs;
            case DP_SEARCH_OP_LT:
                return lhs < rhs;
            case DP_SEARCH_OP_LE:
                return lhs <= rhs;
        }
        throw "EXPRESSION-FILTER-ERROR", sprintf("operator %y cannot be used for a scalar comparison", op);
    }

    #! Applies one arithmetic operator to two values
    private static auto applyArithmetic(string op, auto lhs, auto rhs) {
        switch (op) {
            case DP_OP_PLUS:
                return lhs + rhs;
            case DP_OP_MINUS:
                return lhs - rhs;
            case DP_OP_MULT:
                return lhs * rhs;
            case DP_OP_DIV:
                return lhs / rhs;
            case DP_OP_MOD:
                return lhs % rhs;
        }
        throw "EXPRESSION-FILTER-ERROR", sprintf("operator %y cannot be used for arithmetic", op);
    }

    #! Returns True if the operator is arithmetic and can be evaluated column-wise
    private static bool isArithmeticOperator(string op) {
        switch (op) {
            case DP_OP_PLUS:
            case DP_OP_MINUS:
            case DP_OP_MULT:
            case DP_OP_DIV:
            case DP_OP_MOD:
                return True;
        }
        return False;
    }

    #! Returns True if the operator can be evaluated by native buffer comparisons
    private static bool isNativeBufferComparisonOperator(string op) {
        switch (op) {
            case DP_SEARCH_OP_EQ:
            case DP_SEARCH_OP_NE:
            case DP_SEARCH_OP_GT:
            case DP_SEARCH_OP_GE:
            case DP_SEARCH_OP_LT:
            case DP_SEARCH_OP_LE:
                return True;
        }
        return False;
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

    #! Returns vectorizable inRange-expression information, if supported
    private static *hash<auto> getRangeComparison(hash<DataProviderExpression> expr, string op) {
        if (op != DP_OP_RANGE || expr.args.size() != 3 || !isVectorizableValueArg(expr.args[0])
                || !isScalarComparisonArg(expr.args[1]) || !isScalarComparisonArg(expr.args[2])) {
            return;
        }
        return {
            "expr": expr.args[0],
            "low": unwrapComparisonValue(expr.args[1]),
            "high": unwrapComparisonValue(expr.args[2]),
        };
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
        if (expr.args.size() < 2 || expr.args.size() > 3 || !isVectorizableValueArg(expr.args[0])
                || !isScalarComparisonArg(expr.args[1])) {
            return;
        }

        bool ignore_case = False;
        *hash<auto> locale_options;
        if (expr.args.size() == 3) {
            if (!isScalarComparisonArg(expr.args[2])) {
                return;
            }
            auto options = unwrapComparisonValue(expr.args[2]);
            locale_options = getLocaleStringPredicateOptions(options, op);
            if (!locale_options) {
                ignore_case = options ? True : False;
            }
        }

        auto value_expr = expr.args[0];
        *string case_folded_field = getCaseFoldedTopLevelField(value_expr);
        if (case_folded_field && !locale_options) {
            value_expr = <DataProviderFieldReference>{"field": case_folded_field};
            ignore_case = True;
        }

        return {
            "expr": value_expr,
            "op": op,
            "value": unwrapComparisonValue(expr.args[1]),
            "ignore_case": ignore_case,
            "locale_options": locale_options,
        };
    }

    #! Returns validated locale options for string predicates, if supplied
    private static *hash<auto> getLocaleStringPredicateOptions(auto value, string op) {
        if (!exists value || value.typeCode() != NT_HASH) {
            return;
        }
        if (!value.hasKey("locale") || !exists value.locale || value.locale.typeCode() != NT_STRING) {
            throw "EXPRESSION-FILTER-ERROR", sprintf(
                "third argument to %y must be a bool ignore-case flag or a locale options hash with a string "
                "`locale` key; got %y", op, value);
        }
        return value;
    }

    #! Returns the field name for lwr(@field) or upr(@field), when this can be handled as ignore-case matching
    private static *string getCaseFoldedTopLevelField(auto arg) {
        if (!(arg instanceof hash<DataProviderExpression>)) {
            return;
        }

        string op = OperatorAliasMap{arg.exp} ?? arg.exp;
        if (op != DP_OP_TO_LOWER && op != DP_OP_TO_UPPER) {
            return;
        }
        if (arg.args.size() != 1 || !isTopLevelFieldReference(arg.args[0])) {
            return;
        }
        return arg.args[0].field;
    }

    #! Returns vectorizable pattern predicate information, if supported by BufferColumns
    private static *hash<auto> getPatternPredicate(hash<DataProviderExpression> expr, string op) {
        if (op == DP_OP_LIKE) {
            if (expr.args.size() < 2 || expr.args.size() > 3 || !isVectorizableValueArg(expr.args[0])
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
            auto value_expr = expr.args[0];
            *string case_folded_field = getCaseFoldedTopLevelField(value_expr);
            if (case_folded_field) {
                value_expr = <DataProviderFieldReference>{"field": case_folded_field};
                ignore_case = True;
            }
            string pattern = unwrapComparisonValue(expr.args[1]);
            string re = regex_subst(pattern, "([.^$+?{}\\[\\]|()\\\\])", "\\\\$1", RE_Global);
            re = regex_subst(re, "%", ".*", RE_Global);
            re = regex_subst(re, "_", ".", RE_Global);
            return {
                "expr": value_expr,
                "pattern": "^" + re + "$",
                "options": ignore_case ? RE_Caseless : 0,
                "negated": False,
            };
        }

        if (op == DP_OP_REGEX_MATCH || op == DP_OP_REGEX_NMATCH) {
            if (expr.args.size() < 2 || expr.args.size() > 3 || !isVectorizableValueArg(expr.args[0])
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
                "expr": expr.args[0],
                "pattern": unwrapComparisonValue(expr.args[1]),
                "options": opts,
                "negated": op == DP_OP_REGEX_NMATCH,
            };
        }

        return;
    }

    #! Returns vectorizable computed comparison information, if supported
    private static *hash<auto> getComputedComparison(hash<DataProviderExpression> expr, string op) {
        if (!isComparisonOperator(op) || expr.args.size() != 2) {
            return;
        }

        auto lhs = expr.args[0];
        auto rhs = expr.args[1];
        if (isVectorizableValueArg(lhs) && isScalarComparisonArg(rhs)) {
            return {"expr": lhs, "op": op, "value": unwrapComparisonValue(rhs)};
        }
        if (isVectorizableValueArg(rhs) && isScalarComparisonArg(lhs)) {
            *string inverted_op = invertComparisonOperator(op);
            if (!inverted_op) {
                return;
            }
            return {"expr": rhs, "op": inverted_op, "value": unwrapComparisonValue(lhs)};
        }
        return;
    }

    #! Returns vectorizable expression-to-expression comparison information, if supported
    private static *hash<auto> getVectorComparison(hash<DataProviderExpression> expr, string op) {
        if (!isComparisonOperator(op) || expr.args.size() != 2) {
            return;
        }

        auto lhs = expr.args[0];
        auto rhs = expr.args[1];
        if (isVectorizableValueArg(lhs) && isVectorizableValueArg(rhs)) {
            return {"lhs": lhs, "op": op, "rhs": rhs};
        }
        return;
    }

    #! Returns True if the operator is a scalar comparison operator
    private static bool isComparisonOperator(string op) {
        switch (op) {
            case DP_SEARCH_OP_EQ:
            case DP_SEARCH_OP_NE:
            case DP_OP_ABSOLUTE_EQ:
            case DP_OP_ABSOLUTE_NE:
            case DP_SEARCH_OP_GT:
            case DP_SEARCH_OP_GE:
            case DP_SEARCH_OP_LT:
            case DP_SEARCH_OP_LE:
                return True;
        }
        return False;
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

    #! Returns True if the argument can be evaluated column-wise
    private static bool isVectorizableValueArg(auto arg) {
        if (isTopLevelFieldReference(arg)) {
            return True;
        }
        if (!(arg instanceof hash<DataProviderExpression>)) {
            return False;
        }

        string op = OperatorAliasMap{arg.exp} ?? arg.exp;
        switch (op) {
            case DP_EXPR_VALUE:
                return arg.args.size() == 1 && isVectorizableValueArg(arg.args[0]);
            case DP_OP_TO_LOWER:
            case DP_OP_TO_UPPER:
                return arg.args.size() == 1 && isVectorizableValueArg(arg.args[0]);
            case DP_FUNC_COALESCE:
                return hasVectorizableValueArg(arg.args);
        }

        if (isArithmeticOperator(op)) {
            return arg.args.size() >= 2 && hasVectorizableValueArg(arg.args)
                && allArgsVectorizableOrScalar(arg.args);
        }
        return False;
    }

    #! Returns True if at least one expression argument is column-wise
    private static bool hasVectorizableValueArg(list<auto> args) {
        foreach auto arg in (args) {
            if (isVectorizableValueArg(arg)) {
                return True;
            }
        }
        return False;
    }

    #! Returns True if all expression arguments are either scalar or supported column-wise values
    private static bool allArgsVectorizableOrScalar(list<auto> args) {
        foreach auto arg in (args) {
            if (!isScalarComparisonArg(arg) && !isVectorizableValueArg(arg)) {
                return False;
            }
        }
        return True;
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
            case DP_OP_ABSOLUTE_EQ:
                return DP_OP_ABSOLUTE_EQ;
            case DP_OP_ABSOLUTE_NE:
                return DP_OP_ABSOLUTE_NE;
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
