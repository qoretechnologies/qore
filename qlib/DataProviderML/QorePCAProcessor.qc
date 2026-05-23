# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QorePCAProcessor class definition

/** QorePCAProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that reduces data dimensionality using Principal Component Analysis
/** Accumulates records in windows and uses the ML::PCA algorithm to
    transform each record into principal components when the window is full
    or on flush.

*/
public class QorePCAProcessor inherits AbstractDataProviderMLProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QorePCAProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Field Names",
                "short_desc": "Numeric fields to reduce",
                "desc": "A comma-separated list of numeric field names from the input records "
                    "to compress into fewer dimensions; for example, 50 sensor readings can be "
                    "reduced to 3 combined values that capture the most important patterns; "
                    "at least 2 fields are required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records to analyze together",
                "desc": "The number of records to collect before analyzing patterns across all "
                    "the fields and producing reduced outputs; must be at least 2.\n\n"
                    "**Sizing guidance**: use at least `50` for reliable dimensionality "
                    "reduction, `200`+ for high-dimensional data (many input fields)",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 100,
            },
            "n_components": <DataProviderOptionInfo>{
                "display_name": "Number of Output Dimensions",
                "short_desc": "How many combined dimensions to keep",
                "desc": "The exact number of output dimensions to keep; if not set, the "
                    "algorithm automatically determines the number based on the variance "
                    "threshold; for example, setting this to `3` produces 3 values per record "
                    "regardless of how many input fields there are",
                "type": AbstractDataProviderTypeMap."*int",
                "example_value": 3,
            },
            "variance_threshold": <DataProviderOptionInfo>{
                "display_name": "Information Preservation",
                "short_desc": "How much of the original information to preserve",
                "desc": "A value between `0.0` and `1.0` specifying what fraction of the "
                    "total information (variance) must be preserved; the algorithm keeps just "
                    "enough output dimensions to meet this threshold; `0.95` means keep 95%% "
                    "of the information; only used when the number of output dimensions is "
                    "not set explicitly; default: `0.95`",
                "type": AbstractDataProviderTypeMap."*float",
                "example_value": 0.95,
            },
            "output_mode": <DataProviderOptionInfo>{
                "display_name": "Output Mode",
                "short_desc": "How to return PCA results",
                "desc": "Controls the output representation. `events` preserves the existing "
                    "one-event-per-record structure with nested `record_values` hashes. "
                    "`dataframe` returns a flat DataFrame block with the input fields, "
                    "`component_<n>` columns, and `total_explained_variance`, which is faster "
                    "and easier to use in columnar analytics pipelines; default: `events`",
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
                    "alongside `component_<n>` columns. Disable this when downstream steps "
                    "only need principal components to reduce output size and improve "
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

        #! Configured window size
        int window_size;

        #! Number of components (NOTHING = use variance threshold)
        *int n_components;

        #! Variance threshold
        float variance_threshold = 0.95;

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
        if (fields.lsize() < 2) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("at least 2 fields are required for PCA; got %d", fields.lsize());
        }
        window_size = copts.window_size;
        if (window_size < 2) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 2; got %d", window_size);
        }
        if (exists copts.n_components) {
            n_components = copts.n_components;
        }
        if (exists copts.variance_threshold) {
            variance_threshold = copts.variance_threshold;
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
        return "pca";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Reduces data dimensionality using Principal Component Analysis (PCA); "
            "accumulates records in windows, fits PCA, and transforms each record "
            "into principal components";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits transformed records when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QorePCAProcessor: not configured; provide constructor options";
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

    #! Processes a window of records through PCA
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

        # create PCA with appropriate options
        hash<auto> pca_opts;
        if (exists n_components) {
            pca_opts.n_components = n_components;
        } else {
            pca_opts.variance_threshold = variance_threshold;
        }
        ML::PCA pca(pca_opts);

        # fit and transform
        list<hash<ML::PCAResult>> results = pca.fitTransformMatrix(matrix);

        # emit one event per record
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            foreach string f in (fields) {
                record_values{f} = DataProvider::AbstractDataProvider::getFieldValue(records[i], f);
            }
            push events, <PCAEventInfo>{
                "event_type": "pca",
                "components": results[i].components,
                "total_explained_variance": results[i].total_explained_variance,
                "record_values": record_values,
            };
        }
        return events;
    }

    #! Processes complete shaped windows through PCA
    private *auto processCompleteShapeWindows(auto records, DataProvider::DataProviderShape shape, int min_size,
            bool dataframe_output) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QorePCAProcessor: not configured; provide constructor options";
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
        for (int start = 0; start < count; start += window_size) {
            list<list<float>> window = count == window_size
                ? matrix
                : matrix[start..(start + window_size - 1)];
            if (dataframe_output) {
                appendMatrixWindowDataFrameResult(\result, window, start, count, records, shape,
                    copy_input_fields);
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
    private hash<auto> makeDataFrameResult(auto records, DataProvider::DataProviderShape shape, int count,
            int component_count) {
        hash<auto> result;
        addInputFieldsToDataFrameResult(\result, records, shape, count);
        for (int i = 0; i < component_count; ++i) {
            buffer<float64> values = buffer<float64>::sized(count);
            result{sprintf("component_%d", i)} = values;
        }
        buffer<float64> total_explained_variance = buffer<float64>::sized(count);
        result{"total_explained_variance"} = total_explained_variance;
        return result;
    }

    #! Adds unchanged input columns to a DataFrame result when requested
    private addInputFieldsToDataFrameResult(reference<hash<auto>> result, auto records,
            DataProvider::DataProviderShape shape, int count) {
        if (!include_input_fields) {
            return;
        }
        foreach string f in (fields) {
            auto values = shape == DataProvider::DataProviderShape::DataFrameBlock
                ? records.getColumnBuffer(f)
                : buffer<float64>::sized(count);
            result{f} = values;
        }
    }

    #! Processes a full DataFrame window with native dense column output
    private auto processSingleWindowDataFrameResult(auto records, DataProvider::DataProviderShape shape,
            list<list<float>> matrix, int count) {
        hash<auto> pca_opts;
        if (exists n_components) {
            pca_opts.n_components = n_components;
        } else {
            pca_opts.variance_threshold = variance_threshold;
        }
        ML::PCA pca(pca_opts);
        hash<auto> pca_results = pca.fitTransformMatrixColumns(matrix);

        hash<auto> result;
        addInputFieldsToDataFrameResult(\result, records, shape, count);
        list<auto> component_columns = pca_results.components;
        for (int j = 0; j < component_columns.lsize(); ++j) {
            result{sprintf("component_%d", j)} = component_columns[j];
        }
        result{"total_explained_variance"} = pca_results.total_explained_variance;
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
        hash<auto> pca_opts;
        if (exists n_components) {
            pca_opts.n_components = n_components;
        } else {
            pca_opts.variance_threshold = variance_threshold;
        }
        ML::PCA pca(pca_opts);
        list<hash<ML::PCAResult>> pca_results = pca.fitTransformMatrix(matrix);

        for (int i = 0; i < matrix.lsize(); ++i) {
            hash<auto> record_values;
            for (int j = 0; j < fields.lsize(); ++j) {
                record_values{fields[j]} = matrix[i][j];
            }
            push result{"event_type"}, "pca";
            push result{"components"}, pca_results[i].components;
            push result{"total_explained_variance"}, pca_results[i].total_explained_variance;
            push result{"record_values"}, record_values;
        }
    }

    #! Appends one matrix window to a flat DataFrame result
    private appendMatrixWindowDataFrameResult(reference<hash<auto>> result, list<list<float>> matrix, int offset,
            int total_count, auto records, DataProvider::DataProviderShape shape, bool copy_input_fields) {
        hash<auto> pca_opts;
        if (exists n_components) {
            pca_opts.n_components = n_components;
        } else {
            pca_opts.variance_threshold = variance_threshold;
        }
        ML::PCA pca(pca_opts);
        list<hash<ML::PCAResult>> pca_results = pca.fitTransformMatrix(matrix);
        if (!result) {
            result = makeDataFrameResult(records, shape, total_count, pca_results[0].components.lsize());
        }

        for (int i = 0; i < matrix.lsize(); ++i) {
            int row = offset + i;
            if (copy_input_fields) {
                for (int j = 0; j < fields.lsize(); ++j) {
                    result{fields[j]}[row] = matrix[i][j];
                }
            }
            for (int j = 0; j < pca_results[i].components.lsize(); ++j) {
                result{sprintf("component_%d", j)}[row] = pca_results[i].components[j];
            }
            result{"total_explained_variance"}[row] = pca_results[i].total_explained_variance;
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
        return new HashDeclDataType(TypedHash::forName("DataProviderML::PCAEventInfo"));
    }
}
}
