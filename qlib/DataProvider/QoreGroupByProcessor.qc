# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreGroupByProcessor class definition

/** QoreGroupByProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Parsed aggregate specification for the group-by processor
/** @since %DataProvider 3.3
*/
public hashdecl GroupByAggSpec {
    #! Output field name for the aggregate result
    string output_name;
    #! Aggregate operation: count, sum, avg, min, max, first, last
    string op;
    #! Source field name (NOTHING for count())
    *string field;
}

#! Pipeline processor that groups records by field values and computes aggregate functions per group
/** Groups records by one or more field values and computes aggregate functions (count, sum, avg,
    min, max, first, last) per group. All records are accumulated during processing; results are
    emitted on flush with one output record per group containing the group field values and
    computed aggregates.

    @par Aggregate Expression Format
    Comma-separated expressions in the format @c name=op(field) or @c op(field):
    - @c count() — count of records in the group
    - @c sum(field) — sum of numeric values
    - @c avg(field) — average of numeric values
    - @c min(field) — minimum numeric value
    - @c max(field) — maximum numeric value
    - @c first(field) — first value seen for the field
    - @c last(field) — last value seen for the field

    Output name defaults to @c op_field (or just @c count for count()).

    @par Example
    @code{.qore}
    # Group sales by region, computing total and average
    QoreGroupByProcessor proc({
        "group_by": "region",
        "aggregates": "total=sum(amount),orders=count(),avg_amount=avg(amount)",
    });
    @endcode

    @since %DataProvider 3.3
*/
public class QoreGroupByProcessor inherits AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreGroupByProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "group_by": <DataProviderOptionInfo>{
                "display_name": "Group By Fields",
                "short_desc": "Field names to group records by",
                "desc": "A list of field names to group records by; "
                    "records with the same values for these fields are grouped together; "
                    "if not provided, all records are aggregated into a single group; "
                    "supports dot-notation for nested fields (e.g. `address.city`)",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "input_field_names": True,
            },
            "aggregates": <DataProviderOptionInfo>{
                "display_name": "Aggregate Expressions",
                "short_desc": "Aggregate expressions to compute per group",
                "desc": "Comma-separated aggregate expressions in the format "
                    "`name=op(field)` or `op(field)`.\n\n"
                    "Supported operations: `count()`, `sum(field)`, `avg(field)`, "
                    "`min(field)`, `max(field)`, `first(field)`, `last(field)`.\n\n"
                    "Output field name defaults to `op_field` (or `count` for `count()`). "
                    "Example: `total=sum(amount),orders=count(),avg_price=avg(price)`",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "total=sum(amount),orders=count(),avg=avg(amount)",
            },
        };

        #! Supported aggregate operations
        const SupportedOps = {
            "count": True,
            "sum": True,
            "avg": True,
            "min": True,
            "max": True,
            "first": True,
            "last": True,
        };
    }

    private {
        #! Parsed group-by field names (empty list = all records in one group)
        list<string> group_by_fields;

        #! Parsed aggregate specifications
        list<hash<GroupByAggSpec>> agg_specs;

        #! Accumulated group state: string key -> group state hash with keys:
        #! - \c group_values (hash<auto>): the group-by field values for this group
        #! - \c record_count (int): number of records accumulated in this group
        #! - \c agg (hash<auto!>): per-aggregate accumulator state keyed by output name
        hash<string, hash<auto!>> groups;

        #! Preferred shape for the next pipeline flush output
        DataProviderShape pending_output_shape = DataProviderShape::RowRecords;

        #! True if the pending output shape has been set for the current accumulation window
        bool pending_output_shape_set = False;

        #! Runtime DataFrame class captured from DataFrameBlock input for shape-preserving flushes
        *Class pending_dataframe_class;

        #! Native DataFrame aggregate input for simple single-block DataFrameBlock inputs
        *auto pending_dataframe_native_input;

        #! Flag indicating if the processor has been configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        # parse group_by fields
        if (copts.group_by.val()) {
            group_by_fields = AbstractDataProvider::parseFieldList(copts.group_by);
        } else {
            group_by_fields = ();
        }
        # parse aggregate expressions
        agg_specs = parseAggregates(copts.aggregates);
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "group-by";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Groups records by one or more field values and computes aggregate functions "
            "(`count`, `sum`, `avg`, `min`, `max`, `first`, `last`) per group; emits one "
            "output record per group on flush";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Records the shape of accumulated input for shape-preserving pipeline flushes
    private updatePendingOutputShape(DataProviderShape shape) {
        if (!pending_output_shape_set) {
            pending_output_shape = cast<enum<DataProviderShape>>(shape);
            pending_output_shape_set = True;
            return;
        }
        if (pending_output_shape != shape) {
            pending_output_shape = cast<enum<DataProviderShape>>(DataProviderShape::RowRecords);
        }
    }

    #! Accumulates one logical record into the appropriate group
    private accumulateRecord(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR", "QoreGroupByProcessor: not configured; provide constructor options";
        }
        # build group key and capture group values
        hash<auto> gv;
        foreach string field in (group_by_fields) {
            auto val = AbstractDataProvider::getFieldValue(record, field);
            if (!exists val) {
                # skip records missing any group-by field
                return;
            }
            gv{field} = val;
        }
        string key = group_by_fields ? sprintf("%y", gv) : "";

        # create group if new
        if (!groups.hasKey(key)) {
            groups{key} = {
                "group_values": gv,
                "record_count": 0,
                "agg": initAccumulators(),
            };
        }

        hash<auto!> grp = groups{key};
        grp.record_count++;
        hash<auto!> agg = grp.agg;

        # update accumulators
        foreach hash<GroupByAggSpec> spec in (agg_specs) {
            if (spec.op == "count") {
                continue;
            }
            string agg_key = spec.output_name;
            hash<auto!> acc = agg{agg_key};
            if (spec.op == "first") {
                auto fv = AbstractDataProvider::getFieldValue(record, spec.field);
                if (!acc.set && exists fv) {
                    acc.value = fv;
                    acc.set = True;
                }
            } else if (spec.op == "last") {
                auto fv = AbstractDataProvider::getFieldValue(record, spec.field);
                if (exists fv) {
                    acc.value = fv;
                    acc.set = True;
                }
            } else {
                # numeric operations: sum, avg, min, max
                *float fval = coerceNumeric(AbstractDataProvider::getFieldValue(record, spec.field));
                if (!exists fval) {
                    continue;
                }
                if (spec.op == "sum" || spec.op == "avg") {
                    acc.sum = acc.sum.toFloat() + fval;
                    acc.count = acc.count.toInt() + 1;
                } else if (spec.op == "min") {
                    if (!acc.initialized || fval < acc.val.toFloat()) {
                        acc.val = fval;
                        acc.initialized = True;
                    }
                } else if (spec.op == "max") {
                    if (!acc.initialized || fval > acc.val.toFloat()) {
                        acc.val = fval;
                        acc.initialized = True;
                    }
                }
            }
            agg{agg_key} = acc;
        }

        grp.agg = agg;
        groups{key} = grp;
    }

    #! Accumulates a record into the appropriate group
    private *auto processRecordImpl(auto record) {
        updatePendingOutputShape(DataProviderShape::RowRecords);
        materializePendingNativeInput();
        accumulateRecord(record);
        # accumulation only — never emit during processRecord
        return;
    }

    #! Accumulates hash-of-lists records without converting the whole block to a row list first
    private accumulateHashOfLists(hash<auto> records) {
        HashListIterator it(records);
        while (it.next()) {
            accumulateRecord(it.getValue());
        }
    }

    #! Returns True if a field can be read from a column block without row materialization
    private bool isColumnBlockField(string field) {
        return field.find(".") < 0;
    }

    #! Returns True if all configured group and aggregate fields can use column-block accumulation
    private bool canUseColumnBlockAccumulator() {
        return (all isColumnBlockField($1), group_by_fields)
            && (all !$1.field || isColumnBlockField($1.field), agg_specs);
    }

    #! Returns the value at a logical row from a list, buffer, or scalar column
    static *auto getColumnBlockValue(auto column, int index) {
        switch (column.typeCode()) {
            case NT_LIST:
            case NT_BUFFER:
                return column[index];
        }
        return column;
    }

    #! Accumulates a hash of list/buffer/scalar columns directly by row index
    private bool accumulateColumnBlock(hash<auto> columns) {
        if (!canUseColumnBlockAccumulator()) {
            return False;
        }

        int row_count = AbstractDataProcessor::shapeRecordCount(columns);
        for (int i = 0; i < row_count; ++i) {
            hash<auto> gv;
            bool skip_row = False;
            foreach string field in (group_by_fields) {
                if (!columns.hasKey(field)) {
                    skip_row = True;
                    break;
                }
                gv{field} = getColumnBlockValue(columns{field}, i);
            }
            if (skip_row) {
                continue;
            }

            string key = group_by_fields ? sprintf("%y", gv) : "";
            if (!groups.hasKey(key)) {
                groups{key} = {
                    "group_values": gv,
                    "record_count": 0,
                    "agg": initAccumulators(),
                };
            }

            hash<auto!> grp = groups{key};
            grp.record_count++;
            hash<auto!> agg = grp.agg;

            foreach hash<GroupByAggSpec> spec in (agg_specs) {
                if (spec.op == "count") {
                    continue;
                }
                if (!columns.hasKey(spec.field)) {
                    continue;
                }

                string agg_key = spec.output_name;
                hash<auto!> acc = agg{agg_key};
                auto value = getColumnBlockValue(columns{spec.field}, i);
                if (spec.op == "first") {
                    if (!acc.set) {
                        acc.value = value;
                        acc.set = True;
                    }
                } else if (spec.op == "last") {
                    acc.value = value;
                    acc.set = True;
                } else {
                    *float fval = coerceNumeric(value);
                    if (!exists fval) {
                        continue;
                    }
                    if (spec.op == "sum" || spec.op == "avg") {
                        acc.sum = acc.sum.toFloat() + fval;
                        acc.count = acc.count.toInt() + 1;
                    } else if (spec.op == "min") {
                        if (!acc.initialized || fval < acc.val.toFloat()) {
                            acc.val = fval;
                            acc.initialized = True;
                        }
                    } else if (spec.op == "max") {
                        if (!acc.initialized || fval > acc.val.toFloat()) {
                            acc.val = fval;
                            acc.initialized = True;
                        }
                    }
                }
                agg{agg_key} = acc;
            }

            grp.agg = agg;
            groups{key} = grp;
        }

        return True;
    }

    #! Accumulates a dense ColumnarResult directly by row index
    private bool accumulateColumnarResult(auto records) {
        return accumulateColumnBlock(records.toColumnHash());
    }

    #! Builds one result record per group with computed aggregates
    private *list<auto> buildFlushRows() {
        if (!configured || !groups) {
            return;
        }

        list<auto> results;
        foreach hash<auto> g in (groups.pairIterator()) {
            hash<auto!> grp = g.value;
            hash<auto!> result = grp.group_values ?? {};
            hash<auto!> agg = grp.agg;
            foreach hash<GroupByAggSpec> spec in (agg_specs) {
                string agg_key = spec.output_name;
                if (spec.op == "count") {
                    result{agg_key} = grp.record_count;
                } else if (spec.op == "sum") {
                    result{agg_key} = agg{agg_key}.sum;
                } else if (spec.op == "avg") {
                    int cnt = agg{agg_key}.count.toInt();
                    result{agg_key} = cnt > 0
                        ? agg{agg_key}.sum.toFloat() / cnt
                        : 0.0;
                } else if (spec.op == "min" || spec.op == "max") {
                    result{agg_key} = agg{agg_key}.initialized
                        ? agg{agg_key}.val
                        : NOTHING;
                } else if (spec.op == "first" || spec.op == "last") {
                    result{agg_key} = agg{agg_key}.set
                        ? agg{agg_key}.value
                        : NOTHING;
                }
            }
            push results, result;
        }

        return results;
    }

    #! Clears accumulated group and shape state after a flush
    private clearAccumulatedState() {
        remove groups;
        pending_output_shape = cast<enum<DataProviderShape>>(DataProviderShape::RowRecords);
        pending_output_shape_set = False;
        remove pending_dataframe_class;
        remove pending_dataframe_native_input;
    }

    #! Converts deferred native DataFrame input into the regular accumulator before mixed input is processed
    private materializePendingNativeInput() {
        if (!exists pending_dataframe_native_input) {
            return;
        }
        auto input = pending_dataframe_native_input;
        remove pending_dataframe_native_input;
        accumulateHashOfLists(input.toColumnHash());
    }

    #! Builds a native DataFrame aggregate result from deferred single-block input
    private *auto buildPendingNativeResult() {
        if (!exists pending_dataframe_native_input) {
            return;
        }
        *auto result = tryNativeDataFrameAggregate(pending_dataframe_native_input);
        if (exists result) {
            return result;
        }
        materializePendingNativeInput();
        return;
    }

    #! Emits one record per group with computed aggregates for direct row-record callers
    private *list<auto> flushRecordsImpl() {
        *auto native_result = buildPendingNativeResult();
        if (exists native_result) {
            auto result = native_result;
            clearAccumulatedState();
            return result.toRecords();
        }

        *list<auto> results = buildFlushRows();
        if (!results) {
            return;
        }
        clearAccumulatedState();
        return results;
    }

    #! Emits accumulated results in the negotiated pipeline shape when possible
    private flushImpl(code enqueue) {
        *auto native_result = buildPendingNativeResult();
        if (exists native_result) {
            auto result = native_result;
            DataProviderShape output_shape = pending_output_shape;
            clearAccumulatedState();

            switch (output_shape) {
                case DataProviderShape::DataFrameBlock:
                    enqueue(result);
                    return;
                case DataProviderShape::BufferColumns:
                    enqueue(result.toColumnarResult());
                    return;
                case DataProviderShape::HashOfLists:
                    enqueue(result.toColumnHash());
                    return;
                case DataProviderShape::RowRecords:
                    map enqueue($1), result.toRecords();
                    return;
            }
            map enqueue($1), result.toRecords();
            return;
        }

        *list<auto> results = buildFlushRows();
        if (!results) {
            return;
        }
        DataProviderShape output_shape = pending_output_shape;
        *Class dataframe_class = pending_dataframe_class;
        clearAccumulatedState();

        switch (output_shape) {
            case DataProviderShape::DataFrameBlock:
                enqueue(dataframe_class.newObject(results));
                return;
            case DataProviderShape::BufferColumns:
                enqueue(AbstractDataProcessor::getDataFrameClass().newObject(results).toColumnarResult());
                return;
            case DataProviderShape::HashOfLists:
                enqueue(AbstractDataProcessor::recordsToHashList(results));
                return;
            case DataProviderShape::RowRecords:
                map enqueue($1), results;
                return;
        }
        map enqueue($1), results;
    }

    #! Processes records in hash-of-lists form by accumulating each logical row
    private *hash<auto> processRecordsBulkImpl(hash<auto> records) {
        updatePendingOutputShape(DataProviderShape::HashOfLists);
        accumulateHashOfLists(records);
        return;
    }

    #! Processes data in an explicitly negotiated shape
    /** @param records the input records
        @param shape the negotiated input shape

        @return @ref nothing because grouped results are emitted on flush

        @since DataProvider 3.5
    */
    private *auto processRecordsShapeImpl(auto records, DataProviderShape shape) {
        switch (shape) {
            case DataProviderShape::DataFrameBlock:
                updatePendingOutputShape(DataProviderShape::DataFrameBlock);
                pending_dataframe_class = Class::getClass(records);
                if (!groups.size() && !exists pending_dataframe_native_input && canUseNativeDataFrameAggregate()) {
                    pending_dataframe_native_input = records;
                    return;
                }
                materializePendingNativeInput();
                accumulateHashOfLists(records.toColumnHash());
                return;
            case DataProviderShape::HashOfLists:
                updatePendingOutputShape(DataProviderShape::HashOfLists);
                materializePendingNativeInput();
                if (!accumulateColumnBlock(records)) {
                    accumulateHashOfLists(records);
                }
                return;
            case DataProviderShape::BufferColumns:
                updatePendingOutputShape(DataProviderShape::BufferColumns);
                if (AbstractDataProcessor::isColumnarResult(records)) {
                    if (!groups.size() && !exists pending_dataframe_native_input && canUseNativeDataFrameAggregate()) {
                        auto df = AbstractDataProcessor::getDataFrameClass().newObject(records.toColumnHash());
                        pending_dataframe_native_input = df;
                        return;
                    }
                    materializePendingNativeInput();
                    if (!accumulateColumnarResult(records)) {
                        accumulateHashOfLists(AbstractDataProcessor::recordsToHashList(records.toRows()));
                    }
                } else {
                    materializePendingNativeInput();
                    if (!accumulateColumnBlock(records)) {
                        accumulateHashOfLists(AbstractDataProcessor::bufferColumnsToHashList(records));
                    }
                }
                return;
            case DataProviderShape::RowRecords:
                return processRecordImpl(records);
        }
        throw "PROCESSOR-SHAPE-ERROR", sprintf("%s cannot process unsupported data shape %s",
            self.className(), AbstractDataProcessor::shapeName(shape));
    }

    #! GroupBy supports bulk API and accumulates each logical row from the block
    private bool supportsBulkApiImpl() {
        return True;
    }

    #! Returns supported input shapes
    /** @since DataProvider 3.5
    */
    private list<DataProviderShape> supportedInputShapesImpl() {
        return (DataProviderShape::DataFrameBlock, DataProviderShape::BufferColumns,
            DataProviderShape::HashOfLists, DataProviderShape::RowRecords);
    }

    #! Returns supported output shapes
    /** Grouped results are emitted on flush.  Pipeline flush preserves a DataFrame or hash-of-lists
        carrier when the current accumulation window used one consistently; direct flushRecords()
        still returns row records for compatibility.

        @since DataProvider 3.5
    */
    private list<DataProviderShape> supportedOutputShapesImpl() {
        return (DataProviderShape::DataFrameBlock, DataProviderShape::BufferColumns,
            DataProviderShape::HashOfLists, DataProviderShape::RowRecords);
    }

    #! Returns True when a field name can be evaluated directly by DataFrame native methods
    private bool isNativeDataFrameField(string field) {
        return field.find(".") < 0;
    }

    #! Returns True if the aggregate set can use the native DataFrame aggregation path
    private bool canUseNativeDataFrameAggregate() {
        foreach string field in (group_by_fields) {
            if (!isNativeDataFrameField(field)) {
                return False;
            }
        }

        hash<auto> output_names;
        foreach string field in (group_by_fields) {
            output_names{field} = True;
        }
        foreach hash<GroupByAggSpec> spec in (agg_specs) {
            if (output_names{spec.output_name}) {
                return False;
            }
            output_names{spec.output_name} = True;

            switch (spec.op) {
                case "count":
                    break;
                case "sum":
                case "avg":
                case "min":
                case "max":
                    if (!spec.field || !isNativeDataFrameField(spec.field)) {
                        return False;
                    }
                    break;
                default:
                    return False;
            }
        }
        return True;
    }

    #! Adds or renames one native DataFrame aggregate output column
    private auto addNativeAggregateOutput(*auto result, auto grouped, hash<GroupByAggSpec> spec) {
        if (spec.op == "count") {
            auto counts = grouped.count();
            if (!exists result) {
                result = counts;
                if (spec.output_name != "count") {
                    result.renameColumn("count", spec.output_name);
                }
            } else {
                result.addColumn(spec.output_name, counts.getColumn("count"));
            }
            return result;
        }

        string native_op = spec.op == "avg" ? "mean" : spec.op;
        hash<auto> agg_spec = {spec.field: native_op};
        auto one = grouped.agg(agg_spec);
        if (spec.output_name != spec.field) {
            one.renameColumn(spec.field, spec.output_name);
        }

        if (!exists result) {
            return one;
        }
        result.addColumn(spec.output_name, one.getColumn(spec.output_name));
        return result;
    }

    #! Attempts a native DataFrame group-by aggregate for simple aggregate specs
    private *auto tryNativeDataFrameAggregate(auto records) {
        if (!canUseNativeDataFrameAggregate()) {
            return;
        }

        auto grouped = records.groupBy(group_by_fields);
        *auto result;
        foreach hash<GroupByAggSpec> spec in (agg_specs) {
            result = addNativeAggregateOutput(result, grouped, spec);
        }
        return result;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_AGGREGATE;
    }

    #! Returns NOTHING because the output schema is dynamic
    /** The output type depends on the configured group_by fields and aggregate expressions,
        so it cannot be determined statically. Downstream processors will not receive type
        information from this processor.
    */
    private *AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return NOTHING;
    }

    #! Initializes accumulator state for a new group
    private hash<auto!> initAccumulators() {
        hash<auto!> agg = {};
        foreach hash<GroupByAggSpec> spec in (agg_specs) {
            if (spec.op == "count") {
                # count uses group record_count directly
                continue;
            } else if (spec.op == "sum" || spec.op == "avg") {
                agg{spec.output_name} = {"sum": 0.0, "count": 0};
            } else if (spec.op == "min" || spec.op == "max") {
                agg{spec.output_name} = {"initialized": False};
            } else if (spec.op == "first" || spec.op == "last") {
                agg{spec.output_name} = {"set": False};
            }
        }
        return agg;
    }

    #! Coerces a value to float if numeric, returns NOTHING otherwise
    static *float coerceNumeric(auto value) {
        int tc = value.typeCode();
        if (tc == NT_INT || tc == NT_FLOAT || tc == NT_NUMBER) {
            return value.toFloat();
        }
        if (tc == NT_STRING && regex(value, "^-?\\d+(\\.\\d+)?([eE][+-]?\\d+)?$")) {
            return value.toFloat();
        }
        return NOTHING;
    }

    #! Parses comma-separated aggregate expressions
    /** @param agg_str aggregate expression string
        @return list of parsed aggregate specs: {output_name, op, *field}
    */
    static list<hash<GroupByAggSpec>> parseAggregates(string agg_str) {
        list<hash<GroupByAggSpec>> result;
        list<string> specs = agg_str.split(",");
        foreach string spec in (specs) {
            spec = trim(spec);
            if (!spec.val()) {
                continue;
            }
            push result, parseOneAggregate(spec);
        }
        if (!result) {
            throw "CONSTRUCTOR-ERROR", "no valid aggregate expressions found in 'aggregates' option";
        }
        return result;
    }

    #! Parses a single aggregate expression like "name=op(field)" or "op(field)"
    static hash<GroupByAggSpec> parseOneAggregate(string spec) {
        int open_paren = spec.find("(");
        int close_paren = spec.find(")");
        if (open_paren < 0 || close_paren < 0 || close_paren <= open_paren) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("invalid aggregate expression %y; expected format: 'name=op(field)' or 'op(field)'", spec);
        }

        # check for name= prefix before the opening paren
        *string output_name;
        string before_paren = spec.substr(0, open_paren);
        int eq_pos = before_paren.find("=");
        string op;
        if (eq_pos >= 0) {
            output_name = trim(before_paren.substr(0, eq_pos));
            op = trim(before_paren.substr(eq_pos + 1));
            if (!output_name.val()) {
                throw "CONSTRUCTOR-ERROR",
                    sprintf("empty output name in aggregate expression %y", spec);
            }
        } else {
            op = trim(before_paren);
        }

        # extract field name from between parens
        *string field = trim(spec.substr(open_paren + 1, close_paren - open_paren - 1));
        if (!field.val()) {
            field = NOTHING;
        }

        # validate operation
        if (!SupportedOps.hasKey(op)) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("unknown aggregate operation %y in expression %y; "
                    "supported: count, sum, avg, min, max, first, last", op, spec);
        }

        # validate field requirement
        if (op != "count" && !field) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("aggregate operation %y requires a field name in expression %y", op, spec);
        }

        # determine default output name
        if (!output_name) {
            output_name = field ? op + "_" + field : op;
        }

        return <GroupByAggSpec>{
            "output_name": output_name,
            "op": op,
            "field": field,
        };
    }
}
}
