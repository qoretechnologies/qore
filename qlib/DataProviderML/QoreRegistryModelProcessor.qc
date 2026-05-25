# -*- mode: qore; indent-tabs-mode: nil -*-

/*  QoreRegistryModelProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Main DataProviderML namespace
public namespace DataProviderML {

#! Pipeline processor that loads a pre-trained model from the model registry and runs inference
/** Loads a trained ML model from a @ref QoreModelRegistry::ModelRegistry "ModelRegistry"
    by name (and optional version/tag), then applies the registry execution adapter to
    each window of records.

    The model is loaded once at construction time and reused for all subsequent windows.
    Supports native Qore ML models and ONNX Runtime model packages loaded through the
    registry.

    @par Example:
    @code{.py}
    auto proc = new QoreRegistryModelProcessor({
        "registry_path": "/var/models",
        "model_name": "fraud-detector",
        "version": "latest",
        "fields": ("amount", "merchant_category", "hour_of_day"),
        "window_size": 100,
    });
    @endcode

    @since DataProviderML 1.2
*/
public class QoreRegistryModelProcessor inherits AbstractDataProviderMLProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreRegistryModelProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Supported \c backend_type values
        const BackendTypes = (
            <AllowedValueInfo>{"value": "filesystem", "display_name": "Filesystem"},
            <AllowedValueInfo>{"value": "database", "display_name": "Database (SqlUtil)"},
            <AllowedValueInfo>{"value": "rest", "display_name": "REST API"},
        );

        #! Constructor options
        /** qorus#244: extended with \c backend_type so this processor can
            consume DB- and REST-backed registries (e.g. the live Qorus
            ml_models / ml_versions / ml_blobs registry served by
            \c /api/v9/models), not just filesystem-backed ones.
        */
        const ConstructorOptions = {
            "backend_type": <DataProviderOptionInfo>{
                "display_name": "Backend Type",
                "short_desc": "Storage backend type for the model registry",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The type of storage backend to use: 'filesystem' (default), "
                    "'database' (e.g., the Qorus omq schema's ml_models tables), "
                    "or 'rest' (e.g., the Qorus /api/v9/models REST surface).",
                "allowed_values": BackendTypes,
                "default_value": "filesystem",
            },
            "registry_path": <DataProviderOptionInfo>{
                "display_name": "Registry Path",
                "short_desc": "Filesystem path to the model registry",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Base directory path for the filesystem-backed model "
                    "registry; required when backend_type is 'filesystem'.",
            },
            "registry_datasource": <DataProviderOptionInfo>{
                "display_name": "Registry Datasource",
                "short_desc": "Database datasource for database backend",
                "type": AbstractDataProviderTypeMap."any",
                "desc": "An AbstractDatasource object for database storage; "
                    "required when backend_type is 'database'.",
            },
            "registry_table_prefix": <DataProviderOptionInfo>{
                "display_name": "Registry Table Prefix",
                "short_desc": "Table name prefix for database backend",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Prefix for database table names (default 'ml_'); "
                    "only used with backend_type 'database'.",
            },
            "registry_rest_client": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "REST client object for REST backend",
                "type": AbstractDataProviderTypeMap."any",
                "desc": "A REST client object with get/post/put/del methods; "
                    "required when backend_type is 'rest'.",
            },
            "registry_rest_base_path": <DataProviderOptionInfo>{
                "display_name": "REST Base Path",
                "short_desc": "Base path for REST API endpoints",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Base URL path for model registry REST endpoints "
                    "(e.g., '/models'); required when backend_type is 'rest'.",
            },
            "model_name": <DataProviderOptionInfo>{
                "display_name": "Model Name",
                "short_desc": "Name of the model in the registry",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The registered model name to load for inference.",
                "required": True,
            },
            "version": <DataProviderOptionInfo>{
                "display_name": "Version",
                "short_desc": "Version ID or 'latest'",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The version to load. Use `latest` or omit for the most "
                    "recent version.",
            },
            "tag": <DataProviderOptionInfo>{
                "display_name": "Tag",
                "short_desc": "Load version by tag (e.g., 'production')",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Load the latest version with this tag instead of "
                    "specifying a version ID.",
            },
            "tenant_id": <DataProviderOptionInfo>{
                "display_name": "Tenant ID",
                "short_desc": "Tenant identifier for multi-tenant registries",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Optional tenant identifier to scope registry access.",
            },
            "fields": <DataProviderOptionInfo>{
                "display_name": "Input Fields",
                "short_desc": "Fields to use as model input features",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "desc": "List of field names from each record to pass as input "
                    "features to the model. Native Qore ML models receive a record "
                    "hash with these fields. ONNX models receive the fields as a "
                    "row-major feature matrix under `input_tensor`.",
                "required": True,
                "input_field_names": True,
            },
            "input_tensor": <DataProviderOptionInfo>{
                "display_name": "Input Tensor Name",
                "short_desc": "ONNX model input tensor name",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Name of the ONNX input tensor to receive the feature matrix. "
                    "When omitted, the first input tensor from the ONNX model metadata "
                    "is used. This option is ignored for native Qore ML models.",
            },
            "output_mode": <DataProviderOptionInfo>{
                "display_name": "Output Mode",
                "short_desc": "How to return registry model results",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Controls the output representation. `events` preserves the existing "
                    "one-event-per-record structure with nested `record_values` and "
                    "`prediction` hashes. For ONNX models, `columns` / `dataframe` returns "
                    "flat output columns without per-row event hash materialization, and "
                    "`tensor` returns raw `ML::Tensor` outputs. Default: `events`.",
                "default_value": "events",
                "example_value": "columns",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "events", "display_name": "Event Records"},
                    <AllowedValueInfo>{"value": "columns", "display_name": "Column Output"},
                    <AllowedValueInfo>{"value": "dataframe", "display_name": "DataFrame Columns"},
                    <AllowedValueInfo>{"value": "tensor", "display_name": "Tensor Output"},
                ),
            },
            "include_input_fields": <DataProviderOptionInfo>{
                "display_name": "Include Input Fields",
                "short_desc": "Whether compact output includes original feature columns",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "When `output_mode` is `columns` or `dataframe`, include the original "
                    "input feature columns alongside ONNX output columns. Disable this when "
                    "downstream steps only need model outputs to reduce output size and "
                    "improve throughput. Default: `False`.",
                "default_value": False,
                "example_value": False,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records per processing batch",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Number of records to accumulate before processing. "
                    "Default: 100.",
            },
        };
    }

    private {
        #! The loaded model execution adapter
        QoreModelRegistry::ModelExecutableAdapter model;

        #! Model name (for output events)
        string model_name;

        #! Version ID of the loaded model
        string loaded_version;

        #! Input field names
        list<string> fields;

        #! Optional ONNX input tensor name
        *string input_tensor;

        #! Active ONNX execution provider, when available
        *string execution_provider;

        #! Runtime/provider diagnostics for the loaded model
        *hash<auto> provider_diagnostics;

        #! Output representation mode
        string output_mode = "events";

        #! Whether compact column output includes original feature columns
        bool include_input_fields = False;

        #! Window size
        int window_size = 100;

        #! Record buffer
        list<hash<auto>> buffer();

        #! Whether the processor is configured
        bool configured = False;
    }

    #! Creates an unconfigured processor for navigation
    constructor() {
    }

    #! Creates a configured processor that loads a model from the registry
    /** @param options constructor options (see @ref ConstructorOptions)
    */
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);

        model_name = copts.model_name;
        fields = DataProvider::AbstractDataProvider::parseFieldList(copts.fields);
        input_tensor = copts.input_tensor;
        output_mode = copts.output_mode ?? "events";
        if (exists copts.include_input_fields) {
            include_input_fields = copts.include_input_fields;
        }
        if (copts.window_size) {
            window_size = copts.window_size;
        }

        # qorus#244: build the registry options from the backend-typed
        # constructor options and delegate to ModelRegistry's built-in
        # backend dispatch (see ModelRegistry::createBackend).  This lets
        # the same processor consume a filesystem path, a Qorus omq
        # datasource, or the Qorus /api/v9/models REST surface without
        # subclassing.
        hash<auto> reg_opts = {
            "backend_type": copts.backend_type ?? "filesystem",
            "path": copts.registry_path,
            "datasource": copts.registry_datasource,
            "table_prefix": copts.registry_table_prefix,
            "rest_client": copts.registry_rest_client,
            "rest_base_path": copts.registry_rest_base_path,
            "tenant_id": copts.tenant_id,
        };
        QoreModelRegistry::ModelRegistry registry(reg_opts);

        if (copts.tag) {
            model = registry.loadExecutableAdapterByTag(model_name, copts.tag);
            loaded_version = model.getVersionId();
        } else {
            model = registry.loadExecutableAdapter(model_name, copts.version);
            loaded_version = model.getVersionId();
        }
        if (model.isOnnx()) {
            provider_diagnostics = model.getProviderDiagnostics();
            execution_provider = provider_diagnostics.active_provider;
        }

        configured = True;
    }

    string getName() {
        return "registry-model";
    }

    *string getDesc() {
        return "Runs inference using a pre-trained model loaded from the model registry";
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    private bool supportsBulkApiImpl() {
        return True;
    }

    private int getProcessorCapabilitiesImpl() {
        return DPC_AGGREGATE;
    }

    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return new HashDeclDataType(
            TypedHash::forName("DataProviderML::RegistryModelEventInfo"));
    }

    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "registry-model processor not configured; provide constructor options";
        }

        push buffer, record;
        if (buffer.lsize() >= window_size) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        return;
    }

    private *list<auto> flushRecordsImpl() {
        if (!configured || !buffer) {
            return;
        }
        if (buffer.lsize() > 0) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        return;
    }

    private list<auto> processWindow(list<hash<auto>> records) {
        if (model.isOnnx()) {
            return processOnnxWindow(records);
        }

        list<auto> events();

        foreach hash<auto> rec in (records) {
            # Extract input features from the record
            hash<auto> input;
            foreach string f in (fields) {
                input{f} = rec{f};
            }

            # Run prediction
            auto prediction = model.predict(input);

            push events, <RegistryModelEventInfo>{
                "event_type": "registry-model",
                "model_name": model_name,
                "version_id": loaded_version,
                "record_values": input,
                "prediction": prediction,
            };
        }

        return events;
    }

    private list<auto> processOnnxWindow(list<hash<auto>> records) {
        list<list<float>> matrix;
        foreach hash<auto> rec in (records) {
            list<float> row;
            foreach string f in (fields) {
                push row, DataProvider::AbstractDataProvider::getFieldValue(rec, f).toFloat();
            }
            push matrix, row;
        }

        hash<auto> outputs = model.runFeatureMatrix(matrix, input_tensor);

        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            foreach string f in (fields) {
                record_values{f} = DataProvider::AbstractDataProvider::getFieldValue(records[i], f);
            }

            hash<auto> model_output;
            foreach string okey in (keys outputs) {
                auto oval = outputs{okey};
                if (oval.typeCode() == NT_LIST && oval.lsize() == records.lsize()) {
                    model_output{okey} = oval[i];
                } else {
                    model_output{okey} = oval;
                }
            }

            push events, <RegistryModelEventInfo>{
                "event_type": "registry-model",
                "model_name": model_name,
                "version_id": loaded_version,
                "record_values": record_values,
                "execution_provider": execution_provider,
                "provider_diagnostics": provider_diagnostics,
                "prediction": model_output,
            };
        }

        return events;
    }

    #! Processes records in bulk hash-of-lists format
    private *hash<auto> processRecordsBulkImpl(hash<auto> records) {
        updatePendingOutputShape(DataProvider::DataProviderShape::HashOfLists);
        *auto result = processCompleteShapeWindows(records, DataProvider::DataProviderShape::HashOfLists);
        if (exists result) {
            if (isColumnOutputMode()) {
                return cast<hash<auto>>(convertColumnOutputShape(cast<hash<auto>>(result),
                    DataProvider::DataProviderShape::HashOfLists));
            }
            return cast<hash<auto>>(result);
        }
        *auto fallback = processShapeRowsFallback(records, DataProvider::DataProviderShape::HashOfLists);
        if (!exists fallback) {
            return;
        }
        return cast<hash<auto>>(fallback);
    }

    #! Processes records in an explicitly negotiated pipeline shape
    private *auto processRecordsShapeImpl(auto records, DataProvider::DataProviderShape shape) {
        updatePendingOutputShape(shape);
        if (shape == DataProvider::DataProviderShape::RowRecords) {
            return processRecordImpl(records);
        }

        *auto result = processCompleteShapeWindows(records, shape);
        if (exists result) {
            if (isTensorOutputMode()) {
                return result;
            }
            if (isColumnOutputMode()) {
                return convertColumnOutputShape(cast<hash<auto>>(result), shape);
            }
            return AbstractDataProviderMLProcessor::convertBulkOutputShape(result, shape);
        }
        return processShapeRowsFallback(records, shape);
    }

    #! Uses the row-oriented implementation as a semantics-preserving fallback
    private *auto processShapeRowsFallback(auto records, DataProvider::DataProviderShape shape) {
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
        return AbstractDataProviderMLProcessor::convertBulkOutputShape(bulk, shape);
    }

    #! Processes complete shaped ONNX windows with one tensor inference call per window
    private *auto processCompleteShapeWindows(auto records, DataProvider::DataProviderShape shape) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "registry-model processor not configured; provide constructor options";
        }
        if (!model.isOnnx() || buffer) {
            return;
        }

        int count = DataProvider::AbstractDataProcessor::shapeRecordCount(records, shape);
        if (!count || count % window_size) {
            return;
        }

        if (count == window_size && (isTensorOutputMode() || (isColumnOutputMode() && !include_input_fields))) {
            *ML::Tensor direct_input = inputTensorFromSingleDenseColumn(records, shape, count);
            if (exists direct_input) {
                hash<auto> outputs = model.runFeatureTensor(direct_input, input_tensor);
                if (isTensorOutputMode()) {
                    return outputs;
                }

                hash<auto> result;
                foreach string okey in (keys outputs) {
                    appendTensorOutputColumns(\result, okey, cast<ML::Tensor>(outputs{okey}), count, True);
                }
                return result;
            }
        }

        list<list<float>> matrix = matrixFromShape(records, shape);
        if (isTensorOutputMode()) {
            return processTensorShapeWindows(matrix, count);
        }

        if (isColumnOutputMode()) {
            hash<auto> result;
            addInputFieldColumns(\result, records, shape, matrix);
            for (int start = 0; start < count; start += window_size) {
                list<list<float>> window = count == window_size
                    ? matrix
                    : matrix[start..(start + window_size - 1)];
                appendTensorWindowColumnResult(\result, window, count == window_size);
            }
            return result;
        }

        hash<auto> result;
        for (int start = 0; start < count; start += window_size) {
            list<list<float>> window = count == window_size
                ? matrix
                : matrix[start..(start + window_size - 1)];
            appendTensorWindowEventResult(\result, window);
        }
        return result;
    }

    private bool isColumnOutputMode() {
        return output_mode == "columns" || output_mode == "dataframe";
    }

    private bool isTensorOutputMode() {
        return output_mode == "tensor";
    }

    private auto convertColumnOutputShape(hash<auto> columns, DataProvider::DataProviderShape shape) {
        if (shape == DataProvider::DataProviderShape::BufferColumns) {
            return columns;
        }
        list<auto> converted = DataProvider::AbstractDataProcessor::convertShape(
            columns,
            cast<enum<DataProvider::DataProviderShape>>(DataProvider::DataProviderShape::BufferColumns),
            cast<enum<DataProvider::DataProviderShape>>(shape));
        return shape == DataProvider::DataProviderShape::RowRecords ? converted : converted[0];
    }

    private *ML::Tensor inputTensorFromSingleDenseColumn(auto records, DataProvider::DataProviderShape shape,
            int count) {
        if (fields.lsize() != 1 || fields[0].find(".") >= 0) {
            return;
        }

        *auto column;
        switch (shape) {
            case DataProvider::DataProviderShape::DataFrameBlock:
                column = records.getColumnBuffer(fields[0]);
                break;
            case DataProvider::DataProviderShape::BufferColumns:
                if (DataProvider::AbstractDataProcessor::isColumnarResult(records)) {
                    column = records.getColumn(fields[0]);
                } else if (records.hasKey(fields[0])) {
                    column = records{fields[0]};
                }
                break;
            case DataProvider::DataProviderShape::HashOfLists:
                if (records.hasKey(fields[0])) {
                    column = records{fields[0]};
                }
                break;
        }

        string dtype = model.getOnnxInputTensorDtype(input_tensor);
        if (!exists column || column.typeCode() != NT_BUFFER
                || column.fullType() != sprintf("buffer<%s>", dtype)) {
            return;
        }
        return new ML::Tensor(column, (count, 1), dtype);
    }

    #! Extracts numeric input fields as a model matrix from a shaped block
    private list<list<float>> matrixFromShape(auto records, DataProvider::DataProviderShape shape) {
        if (shape == DataProvider::DataProviderShape::DataFrameBlock) {
            return cast<list<list<float>>>(records.toMatrix(fields));
        }

        if (shape == DataProvider::DataProviderShape::BufferColumns) {
            auto df = DataProvider::AbstractDataProcessor::convertShape(
                records,
                cast<enum<DataProvider::DataProviderShape>>(shape),
                cast<enum<DataProvider::DataProviderShape>>(DataProvider::DataProviderShape::DataFrameBlock))[0];
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

    private addInputFieldColumns(reference<hash<auto>> result, auto records,
            DataProvider::DataProviderShape shape, list<list<float>> matrix) {
        if (!include_input_fields) {
            return;
        }

        foreach string f in (fields) {
            if (f.find(".") >= 0) {
                continue;
            }
            switch (shape) {
                case DataProvider::DataProviderShape::DataFrameBlock:
                    result{f} = records.getColumnBuffer(f);
                    break;
                case DataProvider::DataProviderShape::BufferColumns:
                    if (DataProvider::AbstractDataProcessor::isColumnarResult(records)) {
                        result{f} = records.getColumn(f);
                    } else if (records.hasKey(f)) {
                        result{f} = records{f};
                    }
                    break;
                case DataProvider::DataProviderShape::HashOfLists:
                    if (records.hasKey(f)) {
                        result{f} = records{f};
                    }
                    break;
            }
        }

        for (int j = 0; j < fields.lsize(); ++j) {
            if (result.hasKey(fields[j])) {
                continue;
            }
            list<float> values;
            for (int i = 0; i < matrix.lsize(); ++i) {
                push values, matrix[i][j];
            }
            result{fields[j]} = values;
        }
    }

    #! Appends one registry ONNX tensor inference window to a hash-of-lists result
    private appendTensorWindowEventResult(reference<hash<auto>> result, list<list<float>> matrix) {
        hash<auto> outputs = model.runFeatureMatrix(matrix, input_tensor);
        int count = matrix.lsize();

        for (int i = 0; i < count; ++i) {
            hash<auto> record_values;
            for (int j = 0; j < fields.lsize(); ++j) {
                record_values{fields[j]} = matrix[i][j];
            }

            hash<auto> model_output;
            foreach string okey in (keys outputs) {
                auto oval = outputs{okey};
                if (oval.typeCode() == NT_LIST && oval.lsize() == count) {
                    model_output{okey} = oval[i];
                } else {
                    model_output{okey} = oval;
                }
            }

            push result{"event_type"}, "registry-model";
            push result{"model_name"}, model_name;
            push result{"version_id"}, loaded_version;
            push result{"record_values"}, record_values;
            push result{"execution_provider"}, execution_provider;
            push result{"provider_diagnostics"}, provider_diagnostics;
            push result{"prediction"}, model_output;
        }
    }

    #! Appends compact column output for one registry ONNX tensor inference window
    private appendTensorWindowColumnResult(reference<hash<auto>> result, list<list<float>> matrix,
            bool single_window) {
        hash<auto> outputs = model.runFeatureMatrixTensors(matrix, input_tensor);
        int count = matrix.lsize();
        foreach string okey in (keys outputs) {
            appendTensorOutputColumns(\result, okey, cast<ML::Tensor>(outputs{okey}), count, single_window);
        }
    }

    private appendTensorOutputColumns(reference<hash<auto>> result, string name, ML::Tensor tensor,
            int row_count, bool single_window) {
        list<int> shape = tensor.getShape();
        if ((shape.lsize() == 1 && shape[0] == row_count)
                || (shape.lsize() == 2 && shape[0] == row_count && shape[1] == 1)) {
            if (single_window && !result.hasKey(name)) {
                result{name} = tensor.getBuffer();
            } else {
                appendColumnValues(\result, name, tensor.getBuffer().toList());
            }
            return;
        }

        auto nested = tensor.toList();
        if (shape.lsize() == 2 && shape[0] == row_count) {
            for (int j = 0; j < shape[1]; ++j) {
                list<auto> values;
                for (int i = 0; i < row_count; ++i) {
                    push values, nested[i][j];
                }
                appendColumnValues(\result, sprintf("%s_%d", name, j), values);
            }
            return;
        }

        if (nested.typeCode() == NT_LIST && nested.lsize() == row_count) {
            appendColumnValues(\result, name, nested);
            return;
        }

        appendColumnValues(\result, name,
            DataProvider::AbstractDataProcessor::repeatValue(nested, row_count));
    }

    private appendColumnValues(reference<hash<auto>> result, string name, list<auto> values) {
        if (!result.hasKey(name)) {
            result{name} = ();
        }
        result{name} += values;
    }

    private hash<auto> processTensorShapeWindows(list<list<float>> matrix, int count) {
        if (count == window_size) {
            return model.runFeatureMatrixTensors(matrix, input_tensor);
        }

        list<hash<auto>> windows;
        for (int start = 0; start < count; start += window_size) {
            list<list<float>> window = matrix[start..(start + window_size - 1)];
            push windows, model.runFeatureMatrixTensors(window, input_tensor);
        }
        return {
            "row_count": count,
            "window_size": window_size,
            "windows": windows,
        };
    }
}

} # namespace DataProviderML
