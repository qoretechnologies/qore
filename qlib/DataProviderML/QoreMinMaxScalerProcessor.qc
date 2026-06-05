# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreMinMaxScalerProcessor class definition

/** QoreMinMaxScalerProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProviderML module
public namespace DataProviderML {
#! Pipeline processor that scales numeric features to a configurable range
/** Accumulates records in windows and uses the ML::MinMaxScaler to transform
    each record's selected fields into a bounded range (default [0, 1]). The first
    window fits the scaler (learning per-feature min/max); subsequent windows apply
    the already-fitted scaler.

*/
public class QoreMinMaxScalerProcessor inherits AbstractDataProviderMLProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreMinMaxScalerProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Fields to Normalize",
                "short_desc": "Which numeric fields to convert to the target range",
                "desc": "The numeric fields from your data to normalize. For example, select "
                    "`temperature`, `humidity`, and `wind_speed` to convert them all to the "
                    "same range for easy comparison.\n\n"
                    "At least 1 field is required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Batch Size",
                "short_desc": "How many records to collect before processing",
                "desc": "How many records to collect before processing them as a batch. The "
                    "first batch is used to learn the minimum and maximum of each field; all "
                    "subsequent batches are normalized using those learned values.\n\n"
                    "**Tip**: use at least `50` for reliable results; `200`+ if your data "
                    "has occasional extreme values (outliers)",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 100,
            },
            "feature_min": <DataProviderOptionInfo>{
                "display_name": "Target Minimum",
                "short_desc": "Lowest value in the output range",
                "desc": "The lowest value that normalized output will have. For example, set "
                    "to `0.0` to have all values start at zero, or `-1.0` for a range centered "
                    "around zero; default: `0.0`",
                "type": AbstractDataProviderTypeMap."*float",
                "example_value": 0.0,
            },
            "feature_max": <DataProviderOptionInfo>{
                "display_name": "Target Maximum",
                "short_desc": "Highest value in the output range",
                "desc": "The highest value that normalized output will have. For example, set "
                    "to `1.0` for a 0-to-1 range, or `100.0` for a percentage scale; must be "
                    "greater than the target minimum; default: `1.0`",
                "type": AbstractDataProviderTypeMap."*float",
                "example_value": 1.0,
            },
            "output_mode": <DataProviderOptionInfo>{
                "display_name": "Output Mode",
                "short_desc": "How to return normalized results",
                "desc": "Controls the output representation. `events` preserves the existing "
                    "one-event-per-record structure with nested `record_values` and "
                    "`scaled_values` hashes. `dataframe` returns a flat DataFrame block with "
                    "the input fields and `scaled_<field>` columns, which is faster and easier "
                    "to use in columnar analytics pipelines; default: `events`",
                "type": AbstractDataProviderTypeMap."string",
                "default_value": "events",
                "example_value": "dataframe",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "events", "display_name": "Event Records"},
                    <AllowedValueInfo>{"value": "dataframe", "display_name": "DataFrame Columns"},
                ),
            },
            "include_input_fields": <DataProviderOptionInfo>{
                "display_name": "Include Input Fields",
                "short_desc": "Whether DataFrame output includes original feature columns",
                "desc": "When `output_mode` is `dataframe`, include the original input fields "
                    "alongside the `scaled_<field>` columns. Disable this when downstream "
                    "steps only need normalized columns to reduce output size and improve "
                    "throughput; default: `True`",
                "type": AbstractDataProviderTypeMap."*bool",
                "default_value": True,
                "example_value": False,
            },
        };
    }

    private {
        #! Configured field names
        list<string> fields;

        #! Output column names for scaled fields
        list<string> scaled_fields;

        #! Configured window size
        int window_size;

        #! Target range minimum
        float feature_min = 0.0;

        #! Target range maximum
        float feature_max = 1.0;

        #! Retained MinMaxScaler instance (fitted on first window)
        *ML::MinMaxScaler scaler;

        #! Accumulated records
        list<hash<auto>> buffer;

        #! Output representation mode
        string output_mode = "events";

        #! Whether flat DataFrame output includes original feature columns
        bool include_input_fields = True;

        #! True while a shaped bulk block is dispatching through processRecordImpl()
        bool processing_shaped_bulk = False;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        fields = DataProvider::AbstractDataProvider::parseFieldList(copts.fields);
        if (!fields) {
            throw "CONSTRUCTOR-ERROR", "no valid field names provided in 'fields' option";
        }
        foreach string f in (fields) {
            push scaled_fields, sprintf("scaled_%s", f);
        }
        window_size = copts.window_size;
        if (window_size < 2) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 2; got %d", window_size);
        }
        if (exists copts.feature_min) {
            feature_min = copts.feature_min;
        }
        if (exists copts.feature_max) {
            feature_max = copts.feature_max;
        }
        if (feature_min >= feature_max) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("feature_min (%.6f) must be less than feature_max (%.6f)",
                    feature_min, feature_max);
        }
        output_mode = copts.output_mode;
        if (exists copts.include_input_fields) {
            include_input_fields = copts.include_input_fields;
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "min-max-scaler";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Scales numeric features to a configurable range (default [0, 1]); "
            "accumulates records in windows, fits the scaler on the first window, "
            "and applies it to all subsequent windows";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits scaled records when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreMinMaxScalerProcessor: not configured; provide constructor options";
        }
        if (!processing_shaped_bulk) {
            updatePendingOutputShape(DataProvider::DataProviderShape::RowRecords);
        }
        push buffer, record;
        if (buffer.lsize() >= window_size) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        return;
    }

    #! Flushes remaining buffered data
    private *list<auto> buildFlushRows() {
        if (!configured || !buffer) {
            return;
        }
        if (buffer.lsize() >= 2) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        buffer = ();
        return;
    }

    #! Flushes remaining buffered data as row records for direct callers
    private *list<auto> flushRecordsImpl() {
        *list<auto> results = buildFlushRows();
        clearPendingOutputShape();
        return results;
    }

    #! Flushes remaining buffered data in the negotiated pipeline shape
    private flushImpl(code enqueue) {
        *list<auto> results = buildFlushRows();
        DataProvider::DataProviderShape output_shape = pending_output_shape;
        clearPendingOutputShape();
        DataProvider::AbstractDataProcessor::emitRecordListShape(results, output_shape, enqueue);
    }

    #! Processes shaped records without first materializing row hashes when complete windows are available
    private *auto processRecordsShapeImpl(auto records, DataProvider::DataProviderShape shape) {
        updatePendingOutputShape(shape);
        if (shape == DataProvider::DataProviderShape::RowRecords) {
            return processRecordImpl(records);
        }

        bool dataframe_output = output_mode == "dataframe";
        *auto result = processCompleteShapeWindows(records, shape, 2, dataframe_output);
        if (exists result) {
            return AbstractDataProviderMLProcessor::convertBulkOutputShape(result, shape, dataframe_output);
        }

        return AbstractDataProviderMLProcessor::convertBulkOutputShape(
            processShapeRowsFallback(records, shape, dataframe_output), shape, dataframe_output);
    }

    #! Processes records in bulk hash-of-lists format
    private *hash<auto> processRecordsBulkImpl(hash<auto> records) {
        updatePendingOutputShape(DataProvider::DataProviderShape::HashOfLists);
        *auto result = processCompleteShapeWindows(records, DataProvider::DataProviderShape::HashOfLists, 2, False);
        if (exists result) {
            return cast<hash>(result);
        }
        *auto fallback = processShapeRowsFallback(records, DataProvider::DataProviderShape::HashOfLists, False);
        if (!exists fallback) {
            return;
        }
        return cast<hash>(fallback);
    }

    #! Uses the row-oriented implementation as a semantics-preserving fallback
    private *auto processShapeRowsFallback(auto records, DataProvider::DataProviderShape shape, bool dataframe_output) {
        list<auto> rows = DataProvider::AbstractDataProcessor::convertShape(
            records, shape, DataProvider::DataProviderShape::RowRecords);
        *list<auto> results;
        processing_shaped_bulk = True;
        on_exit {
            processing_shaped_bulk = False;
        }
        foreach auto row in (rows) {
            *auto row_result = processRecordImpl(row);
            if (exists row_result) {
                if (row_result.typeCode() == NT_LIST) {
                    results += row_result;
                } else {
                    push results, row_result;
                }
            }
        }
        if (!results) {
            return;
        }
        hash<auto> bulk = DataProvider::AbstractDataProcessor::recordsToHashList(results);
        if (dataframe_output) {
            return DataProvider::AbstractDataProcessor::getDataFrameClass().newObject(bulk);
        }
        return bulk;
    }

    #! Processes a window of records through MinMaxScaler
    private list<auto> processWindow(list<hash<auto>> records) {
        # extract matrix from fields
        list<list<float>> matrix;
        foreach hash<auto> rec in (records) {
            list<float> row;
            foreach string f in (fields) {
                push row, DataProvider::AbstractDataProvider::getFieldValue(rec, f).toFloat();
            }
            push matrix, row;
        }

        list<list<float>> scaled;
        if (!scaler) {
            # first window: create scaler with range options, fit and transform
            scaler = new ML::MinMaxScaler({
                "feature_min": feature_min,
                "feature_max": feature_max,
            });
            scaler.fitMatrix(matrix);
            scaled = scaler.transformMatrix(matrix);
        } else {
            # subsequent windows: apply the already-fitted scaler
            scaled = scaler.transformMatrix(matrix);
        }

        # emit one event per record
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            hash<auto> scaled_values;
            for (int j = 0; j < fields.lsize(); ++j) {
                string f = fields[j];
                record_values{f} = DataProvider::AbstractDataProvider::getFieldValue(records[i], f);
                scaled_values{f} = scaled[i][j];
            }
            push events, <MinMaxScalerEventInfo>{
                "event_type": "min-max-scaler",
                "record_values": record_values,
                "scaled_values": scaled_values,
                "feature_min": feature_min,
                "feature_max": feature_max,
            };
        }
        return events;
    }

    #! Processes complete shaped windows through MinMaxScaler
    private *auto processCompleteShapeWindows(auto records, DataProvider::DataProviderShape shape, int min_size,
            bool dataframe_output) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreMinMaxScalerProcessor: not configured; provide constructor options";
        }
        if (buffer) {
            return;
        }

        int count = DataProvider::AbstractDataProcessor::shapeRecordCount(records, shape);
        if (count < min_size || count % window_size) {
            return;
        }

        list<list<float>> matrix = matrixFromShape(records, shape);
        if (dataframe_output && count == window_size) {
            return processSingleWindowDataFrameResult(records, shape, matrix, count);
        }

        hash<auto> result;
        bool copy_input_fields = include_input_fields && shape != DataProvider::DataProviderShape::DataFrameBlock;
        if (dataframe_output) {
            result = makeDataFrameResult(records, shape, count);
        }
        for (int start = 0; start < count; start += window_size) {
            list<list<float>> window = count == window_size
                ? matrix
                : matrix[start..(start + window_size - 1)];
            if (dataframe_output) {
                appendMatrixWindowDataFrameResult(\result, window, start, copy_input_fields);
            } else {
                appendMatrixWindowEventResult(\result, window);
            }
        }
        if (dataframe_output) {
            return DataProvider::AbstractDataProcessor::getDataFrameClass().newObject(result);
        }
        return result;
    }

    #! Creates dense DataFrame output columns
    private hash<auto> makeDataFrameResult(auto records, DataProvider::DataProviderShape shape, int count) {
        hash<auto> result;
        addInputFieldsToDataFrameResult(\result, records, shape, count);
        foreach string f in (scaled_fields) {
            buffer<float64> scaled_values = buffer<float64>::sized(count);
            result{f} = scaled_values;
        }
        return result;
    }

    #! Adds unchanged input columns to a DataFrame result when requested
    private addInputFieldsToDataFrameResult(reference<hash<auto>> result, auto records,
            DataProvider::DataProviderShape shape, int count) {
        if (!include_input_fields) {
            return;
        }
        for (int j = 0; j < fields.lsize(); ++j) {
            auto field_values = shape == DataProvider::DataProviderShape::DataFrameBlock
                ? records.getColumnBuffer(fields[j])
                : buffer<float64>::sized(count);
            result{fields[j]} = field_values;
        }
    }

    #! Processes a full DataFrame window with native dense column output
    private auto processSingleWindowDataFrameResult(auto records, DataProvider::DataProviderShape shape,
            list<list<float>> matrix, int count) {
        hash<auto> result;
        addInputFieldsToDataFrameResult(\result, records, shape, count);
        if (include_input_fields && shape != DataProvider::DataProviderShape::DataFrameBlock) {
            copyMatrixInputFields(\result, matrix, 0);
        }

        list<auto> scaled_columns;
        if (!scaler) {
            scaler = new ML::MinMaxScaler({
                "feature_min": feature_min,
                "feature_max": feature_max,
            });
            scaled_columns = scaler.fitTransformMatrixColumns(matrix);
        } else {
            scaled_columns = scaler.transformMatrixColumns(matrix);
        }
        for (int j = 0; j < scaled_fields.lsize(); ++j) {
            result{scaled_fields[j]} = scaled_columns[j];
        }
        return DataProvider::AbstractDataProcessor::getDataFrameClass().newObject(result);
    }

    #! Copies matrix input values to preallocated DataFrame output columns
    private copyMatrixInputFields(reference<hash<auto>> result, list<list<float>> matrix, int offset) {
        for (int i = 0; i < matrix.lsize(); ++i) {
            int row = offset + i;
            for (int j = 0; j < fields.lsize(); ++j) {
                result{fields[j]}[row] = matrix[i][j];
            }
        }
    }

    #! Extracts numeric input fields as an ML matrix from a shaped block
    private list<list<float>> matrixFromShape(auto records, DataProvider::DataProviderShape shape) {
        if (shape == DataProvider::DataProviderShape::DataFrameBlock) {
            return cast<list<list<float>>>(records.toMatrix(fields));
        }

        if (shape == DataProvider::DataProviderShape::BufferColumns) {
            auto df = DataProvider::AbstractDataProcessor::convertShape(
                records, shape, DataProvider::DataProviderShape::DataFrameBlock)[0];
            return cast<list<list<float>>>(df.toMatrix(fields));
        }

        list<list<float>> matrix;
        int count = DataProvider::AbstractDataProcessor::shapeRecordCount(records, shape);
        for (int i = 0; i < count; ++i) {
            list<float> row;
            foreach string f in (fields) {
                push row, records{f}[i].toFloat();
            }
            push matrix, row;
        }
        return matrix;
    }

    #! Appends one matrix window to a hash-of-lists event result
    private appendMatrixWindowEventResult(reference<hash<auto>> result, list<list<float>> matrix) {
        list<list<float>> scaled;
        if (!scaler) {
            scaler = new ML::MinMaxScaler({
                "feature_min": feature_min,
                "feature_max": feature_max,
            });
            scaler.fitMatrix(matrix);
            scaled = scaler.transformMatrix(matrix);
        } else {
            scaled = scaler.transformMatrix(matrix);
        }

        for (int i = 0; i < matrix.lsize(); ++i) {
            hash<auto> record_values;
            hash<auto> scaled_values;
            for (int j = 0; j < fields.lsize(); ++j) {
                string f = fields[j];
                record_values{f} = matrix[i][j];
                scaled_values{f} = scaled[i][j];
            }
            push result{"event_type"}, "min-max-scaler";
            push result{"record_values"}, record_values;
            push result{"scaled_values"}, scaled_values;
            push result{"feature_min"}, feature_min;
            push result{"feature_max"}, feature_max;
        }
    }

    #! Appends one matrix window to a flat DataFrame result
    private appendMatrixWindowDataFrameResult(reference<hash<auto>> result, list<list<float>> matrix, int offset,
            bool copy_input_fields) {
        list<list<float>> scaled;
        if (!scaler) {
            scaler = new ML::MinMaxScaler({
                "feature_min": feature_min,
                "feature_max": feature_max,
            });
            scaler.fitMatrix(matrix);
            scaled = scaler.transformMatrix(matrix);
        } else {
            scaled = scaler.transformMatrix(matrix);
        }

        for (int i = 0; i < matrix.lsize(); ++i) {
            int row = offset + i;
            for (int j = 0; j < fields.lsize(); ++j) {
                if (copy_input_fields) {
                    result{fields[j]}[row] = matrix[i][j];
                }
                result{scaled_fields[j]}[row] = scaled[i][j];
            }
        }
    }

    #! Supports shaped bulk API
    private bool supportsBulkApiImpl() {
        return True;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_AGGREGATE;
    }

    #! Returns the output type
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return new HashDeclDataType(
            TypedHash::forName("DataProviderML::MinMaxScalerEventInfo"));
    }
}
}
