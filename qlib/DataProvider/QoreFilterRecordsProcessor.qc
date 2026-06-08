# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreFilterRecordsProcessor class definition

/** QoreFilterRecordsProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that filters records based on a where_cond expression
/** Supports both individual record and bulk (hash-of-lists) processing.  When used in a pipeline,
    bulk mode allows the pipeline to avoid per-record queue enqueue/dequeue overhead, which is a
    significant performance improvement for high-throughput pipelines.

    @since DataProvider 3.3
*/
public class QoreFilterRecordsProcessor inherits AbstractDataProvider {
    public {
        #! Provider info
        const hash<DataProviderInfo> ProviderInfo = <DataProviderInfo>{
            "type": "QoreFilterRecordsProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_FILTER,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const hash<string, hash<DataProviderOptionInfo>> ConstructorOptions = {
            "where_cond": <DataProviderOptionInfo>{
                "display_name": "Filter Expression",
                "short_desc": "A structured filter expression",
                "desc": "A structured filter expression (`hash<DataProviderExpression>`) built by "
                    "the expression builder, or a DPQL string for text-based configuration",
                "type": (AbstractDataProviderTypeMap."*hash", AbstractDataProviderTypeMap."*string"),
                "server_expression_handling": True,
            },
            "dpql_where": <DataProviderOptionInfo>{
                "display_name": "DPQL Filter",
                "short_desc": "A DPQL string filter expression",
                "desc": "A DPQL string expression to parse and use as the filter condition; for example "
                    "`@status == \"active\" && @score >= 90`",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    private {
        #! The parsed and prepared where expression (field paths pre-computed)
        hash<DataProviderExpression> where_expr;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options, "where_cond|dpql_where");
        if (exists copts.where_cond && exists copts.dpql_where) {
            throw "CONSTRUCTOR-ERROR", "provide either `where_cond` or `dpql_where`, not both";
        }
        if (copts.where_cond) {
            where_expr = AbstractDataProvider::prepareExpression(copts.where_cond.typeCode() == NT_STRING
                ? DataProvider::parseDpqlExpression(copts.where_cond)
                : DataProvider::deserializeExpressionFromJson(copts.where_cond));
        } else if (copts.dpql_where) {
            where_expr = AbstractDataProvider::prepareExpression(DataProvider::parseDpqlExpression(copts.dpql_where));
        }
    }

    #! Returns the name of this data provider
    string getName() {
        return "filter";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Filters records based on a `where_cond` or `dpql_where` expression; "
            "records matching the condition are passed through, non-matching records are dropped";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns action options for the pipeline processor action with dynamic \c where_cond option
    /** The \c where_cond option is added with \c supports_expressions set to @ref True so the expression builder
        UI can render the expression builder widget.

        @param action action info for the pipeline processor action
        @param options sample option data that can be used to determine the final type

        @return action options including the dynamic \c where_cond option for the expression builder

        @since DataProvider 3.3
    */
    private *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForPipelineProcessorWithOptionsImpl(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        return action.options + {
            "where_cond": <ActionOptionInfo>{
                "display_name": "Filter Expression",
                "desc": "A boolean expression to filter records; matching records pass through, "
                    "non-matching records are dropped",
                "type": AbstractDataProviderTypeMap."*bool",
                "supports_templates": False,
                "supports_custom_values": False,
                "supports_expressions": True,
                "server_expression_handling": True,
                "default_view": UiOptionView::Expression,
                "preselected": True,
                "loc": "constructor",
            },
        };
    }

    #! Processes a single record and returns the result
    /** Handles both individual records (hash) and bulk data (hash-of-lists).
        In bulk mode, iterates through each record in the hash-of-lists, evaluates the filter expression,
        and returns a filtered hash-of-lists containing only matching records.

        @param record the input data — either a single record hash or a bulk hash-of-lists

        @return the input data if the filter matches (or a filtered subset for bulk), or @ref nothing if no records
        match

        @throws PROCESSOR-ERROR if no filter expression is configured
    */
    private *auto processRecordImpl(auto record) {
        if (!where_expr) {
            # No expression configured — pass all records through unfiltered
            return record;
        }
        # Bulk mode: hash-of-lists where first value is a list
        if (record.typeCode() == NT_HASH && record.firstValue().typeCode() == NT_LIST) {
            return filterBulk(record);
        }
        return AbstractDataProvider::evalGenericExpression(record, where_expr) ? record : NOTHING;
    }

    #! Processes data in an explicitly negotiated shape
    /** @param records the input data
        @param shape the input shape

        @return filtered data in the same shape when possible

        @since DataProvider 3.5
    */
    private *auto processRecordsShapeImpl(auto records, DataProviderShape shape) {
        if (!where_expr) {
            return records;
        }
        switch (shape) {
            case DataProviderShape::DataFrameBlock:
                return filterDataFrame(records);
            case DataProviderShape::BufferColumns:
                if (AbstractDataProcessor::isColumnarResult(records)) {
                    return filterBufferColumns(records);
                }
                if (AbstractDataProcessor::isHashOfBuffers(records)) {
                    auto df = AbstractDataProcessor::getDataFrameClass().newObject(records);
                    return filterBufferColumns(df.toColumnarResult());
                }
                return filterBulk(records);
            case DataProviderShape::HashOfLists:
                return filterBulk(records);
            case DataProviderShape::RowRecords:
                return AbstractDataProvider::evalGenericExpression(records, where_expr) ? records : NOTHING;
        }
        return processRecordImpl(records);
    }

    #! Filters bulk data (hash-of-lists) by evaluating the expression on each record
    /** @param records the bulk data to filter
        @return filtered hash-of-lists with only matching records, or @ref nothing if no records match

        @since DataProvider 3.3
    */
    private *hash<auto> filterBulk(hash<auto> records) {
        int total = records.firstValue().lsize();
        list<int> passing();
        HashListIterator it(records);
        int idx = 0;
        while (it.next()) {
            if (AbstractDataProvider::evalGenericExpression(it.getValue(), where_expr)) {
                push passing, idx;
            }
            ++idx;
        }
        if (!passing) {
            return;
        }
        if (passing.lsize() == total) {
            return records;
        }
        # Build filtered hash-of-lists using only the passing indices
        hash<auto> result;
        list<string> field_keys = keys records;
        foreach string key in (field_keys) {
            list<auto> vals = records{key};
            list<auto> filtered();
            foreach int i in (passing) {
                push filtered, vals[i];
            }
            result{key} = filtered;
        }
        return result;
    }

    #! Filters a DataFrame block
    /** @param df the DataFrame block

        @return a filtered DataFrame block

        @since DataProvider 3.5
    */
    private auto filterDataFrame(auto df) {
        return ColumnarExpressionFilter::filterDataFrame(df, where_expr);
    }

    #! Filters a ColumnarResult block with a compiled buffer mask when possible
    private auto filterBufferColumns(auto records) {
        return ColumnarExpressionFilter::filterBufferColumns(records, where_expr);
    }

    #! Returns @ref True — this processor supports bulk (hash-of-lists) data
    /** @since DataProvider 3.3
    */
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
    /** @since DataProvider 3.5
    */
    private list<DataProviderShape> supportedOutputShapesImpl() {
        return supportedInputShapesImpl();
    }

    #! Returns the input type as the output type (filter does not change record structure)
    /** @param input_type the output type from the preceding pipeline state

        @return the input type unchanged, since filtering only drops records without modifying them

        @since %DataProvider 3.4
    */
    private *AbstractDataProviderType getPipelineProcessorOutputTypeWithInputTypeImpl(
            *AbstractDataProviderType input_type) {
        return input_type;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_FILTER;
    }
}
}
