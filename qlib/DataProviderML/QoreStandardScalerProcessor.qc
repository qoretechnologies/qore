# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreStandardScalerProcessor class definition

/** QoreStandardScalerProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that standardizes numeric features to zero mean and unit variance
/** Accumulates records in windows and uses the ML::StandardScaler to transform
    each record's selected fields to zero mean and unit variance. The first window
    fits the scaler (learning means and standard deviations); subsequent windows
    apply the already-fitted scaler.

*/
public class QoreStandardScalerProcessor inherits AbstractDataProviderMLProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreStandardScalerProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Fields to Scale",
                "short_desc": "Which numeric fields to put on the same scale",
                "desc": "The numeric fields from your data that should be adjusted to a common "
                    "scale. For example, select `age`, `income`, and `credit_score` to make "
                    "them comparable before grouping customers.\n\n"
                    "At least 1 field is required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Batch Size",
                "short_desc": "How many records to collect before processing",
                "desc": "How many records to collect before processing them as a batch. The "
                    "first batch is used to learn the typical range of each field; all "
                    "subsequent batches are adjusted using those learned values.\n\n"
                    "**Tip**: use at least `50` for reliable results; `200`+ if you have "
                    "many fields or noisy data",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 100,
            },
            "output_mode": <DataProviderOptionInfo>{
                "display_name": "Output Mode",
                "short_desc": "How to return scaled results",
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
                    "steps only need the scaled columns to reduce output size and improve "
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

        #! Retained StandardScaler instance (fitted on first window)
        *ML::StandardScaler scaler;

        #! Accumulated records
        list<hash<auto>> buffer;

        #! Output representation mode
        string output_mode = "events";

        #! Whether flat DataFrame output includes original feature columns
        bool include_input_fields = True;

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
        output_mode = copts.output_mode;
        if (exists copts.include_input_fields) {
            include_input_fields = copts.include_input_fields;
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "standard-scaler";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Standardizes numeric features to zero mean and unit variance; "
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
                "QoreStandardScalerProcessor: not configured; provide constructor options";
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
    private *list<auto> flushRecordsImpl() {
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

    #! Processes shaped records without first materializing row hashes when complete windows are available
    private *auto processRecordsShapeImpl(auto records, DataProvider::DataProviderShape shape) {
        if (shape == DataProvider::DataProviderShape::RowRecords) {
            return processRecordImpl(records);
        }

        *auto result = processCompleteShapeWindows(records, shape, 2, output_mode == "dataframe");
        if (exists result) {
            return result;
        }

        return processShapeRowsFallback(records, shape, output_mode == "dataframe");
    }

    #! Processes records in bulk hash-of-lists format
    private *hash<auto> processRecordsBulkImpl(hash<auto> records) {
        *auto result = processCompleteShapeWindows(records, DataProvider::DataProviderShape::HashOfLists, 2, False);
        if (exists result) {
            return cast<hash<auto>>(result);
        }
        *auto fallback = processShapeRowsFallback(records, DataProvider::DataProviderShape::HashOfLists, False);
        if (!exists fallback) {
            return;
        }
        return cast<hash<auto>>(fallback);
    }

    #! Uses the row-oriented implementation as a semantics-preserving fallback
    private *auto processShapeRowsFallback(auto records, DataProvider::DataProviderShape shape, bool dataframe_output) {
        list<auto> rows = DataProvider::AbstractDataProcessor::convertShape(
            records, shape, DataProvider::DataProviderShape::RowRecords);
        *list<auto> results;
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

    #! Processes a window of records through StandardScaler
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
            # first window: fit and transform
            scaler = new ML::StandardScaler();
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
            push events, <StandardScalerEventInfo>{
                "event_type": "standard-scaler",
                "record_values": record_values,
                "scaled_values": scaled_values,
            };
        }
        return events;
    }

    #! Processes complete shaped windows through StandardScaler
    private *auto processCompleteShapeWindows(auto records, DataProvider::DataProviderShape shape, int min_size,
            bool dataframe_output) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreStandardScalerProcessor: not configured; provide constructor options";
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

        list<auto> scaled_columns;
        if (!scaler) {
            scaler = new ML::StandardScaler();
            scaled_columns = scaler.fitTransformMatrixColumns(matrix);
        } else {
            scaled_columns = scaler.transformMatrixColumns(matrix);
        }
        for (int j = 0; j < scaled_fields.lsize(); ++j) {
            result{scaled_fields[j]} = scaled_columns[j];
        }
        return DataProvider::AbstractDataProcessor::getDataFrameClass().newObject(result);
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
            scaler = new ML::StandardScaler();
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
            push result{"event_type"}, "standard-scaler";
            push result{"record_values"}, record_values;
            push result{"scaled_values"}, scaled_values;
        }
    }

    #! Appends one matrix window to a flat DataFrame result
    private appendMatrixWindowDataFrameResult(reference<hash<auto>> result, list<list<float>> matrix, int offset,
            bool copy_input_fields) {
        list<list<float>> scaled;
        if (!scaler) {
            scaler = new ML::StandardScaler();
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
            TypedHash::forName("DataProviderML::StandardScalerEventInfo"));
    }
}
}
