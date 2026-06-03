# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreKMeansProcessor class definition

/** QoreKMeansProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that segments data into groups using k-means clustering
/** Accumulates records in windows and uses the ML::KMeans algorithm to
    assign each record to a cluster centroid. Supports online learning:
    the first window fits the model, subsequent windows update it incrementally.

*/
public class QoreKMeansProcessor inherits AbstractDataProviderMLProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreKMeansProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Field Names",
                "short_desc": "Numeric fields to use for grouping",
                "desc": "A comma-separated list of numeric field names from the input records "
                    "to use for grouping; for example: \"age,income,spending_score\"; "
                    "at least 1 field is required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records to group together",
                "desc": "The number of records to collect before assigning them to groups; "
                    "all records in a window are analyzed together; must be at least 2.\n\n"
                    "**Sizing guidance**: use `50`\u2013`200` for exploratory grouping, `500`+ for "
                    "stable group assignments; should be much larger than `k`",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 100,
            },
            "k": <DataProviderOptionInfo>{
                "display_name": "Number of Groups",
                "short_desc": "How many groups to create",
                "desc": "The number of groups (clusters) to divide the data into; you must "
                    "specify this in advance; must be at least `1` and no more than the "
                    "number of records in the window.\n\n"
                    "**How to choose**: start with `3`\u2013`5` for most use cases (e.g. customer "
                    "segments, risk tiers); increase if results don't separate well; for "
                    "example, `3` for low/medium/high grouping, `5` for finer segmentation",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 3,
            },
            "online": <DataProviderOptionInfo>{
                "display_name": "Incremental Learning",
                "short_desc": "Continuously improve the model with new data",
                "desc": "When enabled, the first batch of records creates the initial model "
                    "and each subsequent batch refines it, allowing the groups to evolve over "
                    "time; when disabled, each batch creates a completely new model; "
                    "default: `True` (enabled)",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "output_mode": <DataProviderOptionInfo>{
                "display_name": "Output Mode",
                "short_desc": "How to return clustering results",
                "desc": "Controls the output representation. `events` preserves the existing "
                    "one-event-per-record structure with nested `record_values` hashes. "
                    "`dataframe` returns a flat DataFrame block with the input fields, "
                    "`cluster`, `distance`, and `centroid_distance_<n>` columns, which is "
                    "faster and easier to use in columnar analytics pipelines; default: "
                    "`events`",
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
                    "alongside cluster assignment columns. Disable this when downstream "
                    "steps only need clustering results to reduce output size and improve "
                    "throughput; default: `True`",
                "type": AbstractDataProviderTypeMap."*bool",
                "default_value": True,
                "example_value": False,
            },
            "include_centroid_distances": <DataProviderOptionInfo>{
                "display_name": "Include Centroid Distances",
                "short_desc": "Whether DataFrame output includes every centroid distance",
                "desc": "When `output_mode` is `dataframe`, include `centroid_distance_<n>` "
                    "columns for each cluster centroid. Disable this when downstream "
                    "steps only need the winning `cluster` and nearest `distance` values; "
                    "default: `True`",
                "type": AbstractDataProviderTypeMap."*bool",
                "default_value": True,
                "example_value": False,
            },
        };
    }

    private {
        #! Configured field names
        list<string> fields;

        #! Output column names for centroid distances
        list<string> centroid_distance_fields;

        #! Configured window size
        int window_size;

        #! Number of clusters
        int k;

        #! Online learning mode
        bool online = True;

        #! Retained KMeans instance for online learning
        *ML::KMeans kmeans;

        #! Accumulated records
        list<hash<auto>> buffer;

        #! Output representation mode
        string output_mode = "events";

        #! Whether flat DataFrame output includes original feature columns
        bool include_input_fields = True;

        #! Whether flat DataFrame output includes all per-centroid distances
        bool include_centroid_distances = True;

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
        window_size = copts.window_size;
        if (window_size < 2) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 2; got %d", window_size);
        }
        k = copts.k;
        if (k < 1) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("k must be >= 1; got %d", k);
        }
        for (int i = 0; i < k; ++i) {
            push centroid_distance_fields, sprintf("centroid_distance_%d", i);
        }
        if (window_size < k) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size (%d) must be >= k (%d); KMeans requires at least "
                    "as many data points as clusters", window_size, k);
        }
        if (exists copts.online) {
            online = copts.online;
        }
        output_mode = copts.output_mode;
        if (exists copts.include_input_fields) {
            include_input_fields = copts.include_input_fields;
        }
        if (exists copts.include_centroid_distances) {
            include_centroid_distances = copts.include_centroid_distances;
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "kmeans-clustering";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Segments data into k groups using k-means clustering; accumulates records "
            "in windows, clusters them, and emits one event per record with cluster "
            "assignment; supports online learning for incremental model updates";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits clustered records when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreKMeansProcessor: not configured; provide constructor options";
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
        if (buffer.lsize() >= k) {
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
        *auto result = processCompleteShapeWindows(records, shape, k, dataframe_output);
        if (exists result) {
            return AbstractDataProviderMLProcessor::convertBulkOutputShape(result, shape, dataframe_output);
        }

        return AbstractDataProviderMLProcessor::convertBulkOutputShape(
            processShapeRowsFallback(records, shape, dataframe_output), shape, dataframe_output);
    }

    #! Processes records in bulk hash-of-lists format
    private *hash<auto> processRecordsBulkImpl(hash<auto> records) {
        updatePendingOutputShape(DataProvider::DataProviderShape::HashOfLists);
        *auto result = processCompleteShapeWindows(records, DataProvider::DataProviderShape::HashOfLists, k, False);
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

    #! Processes a window of records through KMeans
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

        # fit or update the model
        if (!kmeans) {
            kmeans = new ML::KMeans({"k": k});
            kmeans.fitMatrix(matrix);
        } else if (online) {
            kmeans.updateMatrix(matrix);
        } else {
            kmeans = new ML::KMeans({"k": k});
            kmeans.fitMatrix(matrix);
        }

        # predict cluster assignments
        list<hash<ML::KMeansResult>> predictions = kmeans.predictMatrix(matrix);

        # emit one event per record
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            foreach string f in (fields) {
                record_values{f} = DataProvider::AbstractDataProvider::getFieldValue(records[i], f);
            }
            push events, <KMeansClusteringEventInfo>{
                "event_type": "kmeans-clustering",
                "cluster": predictions[i].cluster,
                "distance": predictions[i].distance,
                "centroid_distances": predictions[i].centroid_distances,
                "record_values": record_values,
            };
        }
        return events;
    }

    #! Processes complete shaped windows through KMeans
    private *auto processCompleteShapeWindows(auto records, DataProvider::DataProviderShape shape, int min_size,
            bool dataframe_output) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreKMeansProcessor: not configured; provide constructor options";
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
        buffer<int64> clusters = buffer<int64>::sized(count);
        buffer<float64> distances = buffer<float64>::sized(count);
        result{"cluster"} = clusters;
        result{"distance"} = distances;
        if (include_centroid_distances) {
            foreach string f in (centroid_distance_fields) {
                buffer<float64> values = buffer<float64>::sized(count);
                result{f} = values;
            }
        }
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
        if (!kmeans) {
            kmeans = new ML::KMeans({"k": k});
            kmeans.fitMatrix(matrix);
        } else if (online) {
            kmeans.updateMatrix(matrix);
        } else {
            kmeans = new ML::KMeans({"k": k});
            kmeans.fitMatrix(matrix);
        }

        hash<auto> predictions = kmeans.predictMatrixColumns(matrix, {
            "include_centroid_distances": include_centroid_distances,
        });
        hash<auto> result;
        addInputFieldsToDataFrameResult(\result, records, shape, count);
        if (include_input_fields && shape != DataProvider::DataProviderShape::DataFrameBlock) {
            copyMatrixInputFields(\result, matrix, 0);
        }
        result{"cluster"} = predictions.cluster;
        result{"distance"} = predictions.distance;
        if (include_centroid_distances) {
            list<auto> centroid_columns = predictions.centroid_distances;
            for (int j = 0; j < centroid_distance_fields.lsize(); ++j) {
                result{centroid_distance_fields[j]} = centroid_columns[j];
            }
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
        if (!kmeans) {
            kmeans = new ML::KMeans({"k": k});
            kmeans.fitMatrix(matrix);
        } else if (online) {
            kmeans.updateMatrix(matrix);
        } else {
            kmeans = new ML::KMeans({"k": k});
            kmeans.fitMatrix(matrix);
        }

        list<hash<ML::KMeansResult>> predictions = kmeans.predictMatrix(matrix);
        for (int i = 0; i < matrix.lsize(); ++i) {
            hash<auto> record_values;
            for (int j = 0; j < fields.lsize(); ++j) {
                record_values{fields[j]} = matrix[i][j];
            }
            push result{"event_type"}, "kmeans-clustering";
            push result{"cluster"}, predictions[i].cluster;
            push result{"distance"}, predictions[i].distance;
            push result{"centroid_distances"}, predictions[i].centroid_distances;
            push result{"record_values"}, record_values;
        }
    }

    #! Appends one matrix window to a flat DataFrame result
    private appendMatrixWindowDataFrameResult(reference<hash<auto>> result, list<list<float>> matrix, int offset,
            bool copy_input_fields) {
        if (!kmeans) {
            kmeans = new ML::KMeans({"k": k});
            kmeans.fitMatrix(matrix);
        } else if (online) {
            kmeans.updateMatrix(matrix);
        } else {
            kmeans = new ML::KMeans({"k": k});
            kmeans.fitMatrix(matrix);
        }

        list<hash<ML::KMeansResult>> predictions = kmeans.predictMatrix(matrix);
        for (int i = 0; i < matrix.lsize(); ++i) {
            int row = offset + i;
            if (copy_input_fields) {
                for (int j = 0; j < fields.lsize(); ++j) {
                    result{fields[j]}[row] = matrix[i][j];
                }
            }
            result{"cluster"}[row] = predictions[i].cluster;
            result{"distance"}[row] = predictions[i].distance;
            if (include_centroid_distances) {
                for (int j = 0; j < predictions[i].centroid_distances.lsize(); ++j) {
                    result{centroid_distance_fields[j]}[row] = predictions[i].centroid_distances[j];
                }
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
        return new HashDeclDataType(TypedHash::forName("DataProviderML::KMeansClusteringEventInfo"));
    }
}
}
