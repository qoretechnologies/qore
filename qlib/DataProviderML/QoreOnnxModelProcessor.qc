# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreOnnxModelProcessor class definition

/** QoreOnnxModelProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that runs ONNX model inference on record windows
/** Accumulates records in windows, extracts specified fields as a feature matrix,
    runs inference through an ONNX model, and emits results combining
    input record values with model outputs.

    @par Example:
    @code{.py}
# Create an ONNX processor for a regression model with 3 input features
QoreOnnxModelProcessor proc({
    "model_path": "/path/to/model.onnx",
    "fields": "temperature,humidity,pressure",
    "window_size": 10,
});

# With GPU acceleration
QoreOnnxModelProcessor gpu_proc({
    "model_path": "/path/to/model.onnx",
    "fields": "feature1,feature2",
    "window_size": 50,
    "execution_providers": "CUDA",
    "cuda_device_id": "0",
    "intra_op_threads": 4,
});
    @endcode

*/
public class QoreOnnxModelProcessor inherits AbstractDataProviderMLProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreOnnxModelProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "model_path": <DataProviderOptionInfo>{
                "display_name": "Model File Path",
                "short_desc": "Path to the pre-trained ONNX model",
                "desc": "The filesystem path to a pre-trained ONNX model file (.onnx); "
                    "ONNX is a standard format for machine learning models that can be "
                    "exported from frameworks like PyTorch, scikit-learn, and TensorFlow",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "fields": <DataProviderOptionInfo>{
                "display_name": "Input Field Names",
                "short_desc": "Record fields to feed into the model",
                "desc": "A comma-separated list of numeric field names from the input records "
                    "to pass to the model as input features; the fields are provided to the "
                    "model in the order listed; for example: \"feature1,feature2,feature3\". "
                    "Use either `fields` for a single ONNX input tensor or `input_map` for "
                    "models with multiple input tensors",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Batch Size",
                "short_desc": "Number of records to process at once",
                "desc": "The number of records to collect before sending them to the model "
                    "for inference as a batch; larger batches may be more efficient but "
                    "use more memory; must be at least 1.\n\n"
                    "**Sizing guidance**: `1` for real-time inference, `10`\u2013`100` for batch "
                    "efficiency, `1000`+ for maximum throughput on GPU",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 10,
            },
            "input_tensor": <DataProviderOptionInfo>{
                "display_name": "Input Tensor Name",
                "short_desc": "Which model input to use (advanced)",
                "desc": "The name of the specific model input to feed data into; only needed "
                    "for models with multiple inputs; if not specified, the model's first "
                    "input is used automatically, which is correct for most models",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "input_map": <DataProviderOptionInfo>{
                "display_name": "Input Tensor Field Map",
                "short_desc": "Map ONNX input tensors to record fields",
                "desc": "A hash mapping ONNX input tensor names to field lists. Use this for "
                    "models with multiple inputs, for example `{\"ids\": \"token_ids\", "
                    "\"mask\": \"attention_mask\"}` or `{ \"features\": (\"x\", \"y\") }`. "
                    "When `input_map` is set, each mapped tensor is built from its configured "
                    "fields and `input_tensor` is ignored.",
                "type": AbstractDataProviderTypeMap."*hash",
                "example_value": {"X": ("x", "y")},
            },
            "output_mode": <DataProviderOptionInfo>{
                "display_name": "Output Mode",
                "short_desc": "How to return ONNX inference results",
                "desc": "Controls the output representation. `events` preserves the existing "
                    "one-event-per-record structure with nested `record_values` and "
                    "`model_output` hashes. `columns` / `dataframe` returns flat output "
                    "columns without per-row event hash materialization. `tensor` returns "
                    "the raw `ML::Tensor` outputs for callers that can consume tensor "
                    "results directly. Default: `events`",
                "type": AbstractDataProviderTypeMap."string",
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
                "desc": "When `output_mode` is `columns` or `dataframe`, include the original "
                    "input feature columns alongside ONNX output columns. Disable this when "
                    "downstream steps only need model outputs to reduce output size and "
                    "improve throughput. Default: `False`",
                "type": AbstractDataProviderTypeMap."*bool",
                "default_value": False,
                "example_value": False,
            },
            "execution_providers": <DataProviderOptionInfo>{
                "display_name": "Execution Providers",
                "short_desc": "Comma-separated execution providers (e.g. CUDA,TensorRT)",
                "desc": "A comma-separated list of ONNX execution provider names in priority "
                    "order; providers are tried in order and the first available one is used; "
                    "CPU is always available as a fallback; for example: \"CUDA,CPU\" or "
                    "\"TensorRT,CUDA\"; if not specified, only CPU is used",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "cuda_device_id": <DataProviderOptionInfo>{
                "display_name": "CUDA Device ID",
                "short_desc": "GPU device index for CUDA provider",
                "desc": "The GPU device index to use with the CUDA execution provider; "
                    "only used when 'CUDA' is listed in `execution_providers`; "
                    "defaults to `\"0\"`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "intra_op_threads": <DataProviderOptionInfo>{
                "display_name": "Intra-Op Threads",
                "short_desc": "Threads within each model operation",
                "desc": "The number of threads used to speed up individual model operations "
                    "(e.g. a single matrix multiplication); increasing this helps with large "
                    "models that have big operations; defaults to `1`",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "inter_op_threads": <DataProviderOptionInfo>{
                "display_name": "Inter-Op Threads",
                "short_desc": "Threads across independent operations",
                "desc": "The number of threads used to run independent model operations in "
                    "parallel (e.g. two branches of a neural network that don't depend on "
                    "each other); `0` means the runtime chooses automatically; defaults to `0`",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "graph_optimization_level": <DataProviderOptionInfo>{
                "display_name": "Graph Optimization",
                "short_desc": "Level of model graph optimization",
                "desc": "Controls how aggressively the runtime optimizes the model graph "
                    "before running inference; higher levels can improve speed but take "
                    "longer to initialize; defaults to `All`",
                "type": AbstractDataProviderTypeMap."*int",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 0, "display_name": "Disabled"},
                    <AllowedValueInfo>{"value": 1, "display_name": "Basic"},
                    <AllowedValueInfo>{"value": 2, "display_name": "Extended"},
                    <AllowedValueInfo>{"value": 99, "display_name": "All (default)"},
                ),
            },
            "log_severity": <DataProviderOptionInfo>{
                "display_name": "Log Severity",
                "short_desc": "ONNX Runtime log verbosity level",
                "desc": "Controls how much diagnostic information the ONNX Runtime logs; "
                    "lower values produce more output, which is useful for troubleshooting "
                    "model loading or inference issues; defaults to `Warning`",
                "type": AbstractDataProviderTypeMap."*int",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 0, "display_name": "Verbose"},
                    <AllowedValueInfo>{"value": 1, "display_name": "Info"},
                    <AllowedValueInfo>{"value": 2, "display_name": "Warning (default)"},
                    <AllowedValueInfo>{"value": 3, "display_name": "Error"},
                    <AllowedValueInfo>{"value": 4, "display_name": "Fatal"},
                ),
            },
        };
    }

    private {
        #! Configured field names
        list<string> fields;

        #! Configured window size
        int window_size;

        #! Input tensor name
        *string input_tensor;

        #! Optional mapping of ONNX input tensor names to field lists
        *hash<string, list<string>> input_map;

        #! Dtype per ONNX input tensor
        hash<string, string> input_dtypes;

        #! Input tensor dtype for ML::Tensor construction
        string input_dtype = "float32";

        #! ONNX model path
        string model_path;

        #! ONNX model instance
        ML::OnnxModel model;

        #! Output representation mode
        string output_mode = "events";

        #! Whether compact column output includes original feature columns
        bool include_input_fields = False;

        #! Accumulated records
        list<hash<auto>> buffer;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        fields = ();
        if (exists copts.fields) {
            fields = DataProvider::AbstractDataProvider::parseFieldList(copts.fields);
        }
        if (exists copts.input_map) {
            input_map = QoreOnnxModelProcessor::parseInputMap(copts.input_map);
            fields = QoreOnnxModelProcessor::mergeInputMapFields(fields, input_map);
        }
        if (!fields) {
            throw "CONSTRUCTOR-ERROR",
                "no valid input field names provided; set 'fields' for a single input tensor "
                "or 'input_map' for one or more ONNX input tensors";
        }
        window_size = copts.window_size;
        if (window_size < 1) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 1; got %d", window_size);
        }
        model_path = copts.model_path;
        if (exists copts.input_tensor) {
            input_tensor = copts.input_tensor;
        }
        output_mode = copts.output_mode ?? "events";
        if (exists copts.include_input_fields) {
            include_input_fields = copts.include_input_fields;
        }

        # Build session config from options
        hash<ML::OnnxSessionConfig> session_config;
        if (exists copts.execution_providers) {
            list<hash<ML::OnnxProviderConfig>> provider_list;
            foreach string pname in (copts.execution_providers.split(",")) {
                string trimmed = trim(pname);
                if (!trimmed.val()) {
                    continue;
                }
                hash<ML::OnnxProviderConfig> pcfg = <ML::OnnxProviderConfig>{"name": trimmed};
                # Add CUDA device_id option if applicable
                if ((trimmed == "CUDA" || trimmed == "CUDAExecutionProvider")
                        && exists copts.cuda_device_id) {
                    pcfg.options = {"device_id": copts.cuda_device_id};
                }
                push provider_list, pcfg;
            }
            if (provider_list) {
                session_config.providers = provider_list;
            }
        }
        if (exists copts.intra_op_threads) {
            session_config.intra_op_threads = copts.intra_op_threads;
        }
        if (exists copts.inter_op_threads) {
            session_config.inter_op_threads = copts.inter_op_threads;
        }
        if (exists copts.graph_optimization_level) {
            session_config.graph_optimization_level = copts.graph_optimization_level;
        }
        if (exists copts.log_severity) {
            session_config.log_severity = copts.log_severity;
        }

        # Load the model
        if (session_config.providers || exists session_config.intra_op_threads
                || exists session_config.inter_op_threads
                || exists session_config.graph_optimization_level
                || exists session_config.log_severity) {
            model = new ML::OnnxModel(model_path, session_config);
        } else {
            model = new ML::OnnxModel(model_path);
        }

        # Determine the input tensor name
        if (!exists input_tensor) {
            list<hash<ML::OnnxTensorInfo>> inputs = model.getInputInfo();
            if (!inputs) {
                throw "CONSTRUCTOR-ERROR", "ONNX model has no input tensors";
            }
            input_tensor = inputs[0].name;
        }
        bool found_input_tensor = False;
        foreach hash<ML::OnnxTensorInfo> info in (model.getInputInfo()) {
            input_dtypes{info.name} = QoreOnnxModelProcessor::onnxTypeToTensorDtype(info.type);
            if (info.name == input_tensor) {
                input_dtype = input_dtypes{info.name};
                found_input_tensor = True;
            }
        }
        if (!found_input_tensor) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("ONNX model does not have an input tensor named %y; available inputs: %y",
                    input_tensor, keys input_dtypes);
        }
        if (exists input_map) {
            hash<string, list<string>> imap = input_map;
            foreach string tensor_name in (keys imap) {
                if (!input_dtypes.hasKey(tensor_name)) {
                    throw "CONSTRUCTOR-ERROR",
                        sprintf("input_map references unknown ONNX input tensor %y; available inputs: %y",
                            tensor_name, keys input_dtypes);
                }
            }
        }

        buffer = ();
        configured = True;
    }

    #! Converts ONNX metadata type names to ML::Tensor dtype names
    private static string onnxTypeToTensorDtype(string type) {
        switch (type) {
            case "float": return "float32";
            case "double": return "float64";
            case "int8": return "int8";
            case "int16": return "int16";
            case "int32": return "int32";
            case "int64": return "int64";
            case "bool": return "bool";
        }
        return "float32";
    }

    #! Parses the input tensor to field-list mapping
    private static hash<string, list<string>> parseInputMap(hash<auto> raw_map) {
        hash<string, list<string>> rv;
        foreach auto i in (raw_map.pairIterator()) {
            list<string> tensor_fields = DataProvider::AbstractDataProvider::parseFieldList(i.value);
            if (!tensor_fields) {
                throw "CONSTRUCTOR-ERROR",
                    sprintf("input_map entry %y does not contain any valid field names", i.key);
            }
            rv{i.key} = tensor_fields;
        }
        return rv;
    }

    #! Merges single-input fields with all mapped input fields for record metadata
    private static list<string> mergeInputMapFields(list<string> base, *hash<string, list<string>> mapped) {
        list<string> rv = base;
        if (!exists mapped) {
            return rv;
        }
        foreach auto i in (mapped.pairIterator()) {
            foreach string field in (i.value) {
                if (!rv.contains(field)) {
                    push rv, field;
                }
            }
        }
        return rv;
    }

    #! Returns the name of this data provider
    string getName() {
        return "onnx-model";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Runs ONNX model inference on data pipeline records; "
            "accumulates records in windows, extracts specified fields "
            "as features, runs the model, and emits inference results";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits results when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreOnnxModelProcessor: not configured; provide constructor options";
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
        list<auto> results = processWindow(buffer);
        buffer = ();
        return results;
    }

    #! Processes a window of records through the ONNX model
    private list<auto> processWindow(list<hash<auto>> records) {
        if (exists input_map) {
            return processMappedRecordWindow(records);
        }

        # Build a matrix from fields: list of list of float
        list<list<float>> matrix;
        foreach hash<auto> rec in (records) {
            list<float> row;
            foreach string f in (fields) {
                push row, DataProvider::AbstractDataProvider::getFieldValue(rec, f).toFloat();
            }
            push matrix, row;
        }

        # Run inference
        hash<auto> outputs = model.run({input_tensor: matrix});

        # Emit one event per record
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            foreach string f in (fields) {
                record_values{f} = DataProvider::AbstractDataProvider::getFieldValue(records[i], f);
            }
            # Build per-record output from model outputs
            hash<auto> model_output;
            foreach string okey in (keys outputs) {
                auto oval = outputs{okey};
                if (oval.typeCode() == NT_LIST && oval.lsize() == records.lsize()) {
                    # Output is a list with one element per record
                    model_output{okey} = oval[i];
                } else {
                    # Scalar or non-per-record output: include in all events
                    model_output{okey} = oval;
                }
            }
            push events, <OnnxInferenceEventInfo>{
                "event_type": "onnx-model",
                "model_path": model_path,
                "execution_provider": model.getProviderInfo(),
                "record_values": record_values,
                "model_output": model_output,
            };
        }
        return events;
    }

    #! Processes one row-oriented window with mapped ONNX input tensors
    private list<auto> processMappedRecordWindow(list<hash<auto>> records) {
        int count = records.lsize();
        hash<auto> outputs = runMappedTensorWindow(matricesFromRecordInputMap(records), 0, count, True);

        hash<auto> output_values;
        foreach string okey in (keys outputs) {
            output_values{okey} = tensorOutputToRows(cast<ML::Tensor>(outputs{okey}), count);
        }

        list<auto> events;
        for (int i = 0; i < count; ++i) {
            hash<auto> record_values;
            foreach string f in (fields) {
                record_values{f} = DataProvider::AbstractDataProvider::getFieldValue(records[i], f);
            }

            hash<auto> model_output;
            foreach string okey in (keys output_values) {
                auto oval = output_values{okey};
                if (oval.typeCode() == NT_LIST && oval.lsize() == count) {
                    model_output{okey} = oval[i];
                } else {
                    model_output{okey} = oval;
                }
            }

            push events, <OnnxInferenceEventInfo>{
                "event_type": "onnx-model",
                "model_path": model_path,
                "execution_provider": model.getProviderInfo(),
                "record_values": record_values,
                "model_output": model_output,
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

    #! Processes complete shaped windows with a single tensor inference call per window
    private *auto processCompleteShapeWindows(auto records, DataProvider::DataProviderShape shape) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreOnnxModelProcessor: not configured; provide constructor options";
        }
        if (buffer) {
            return;
        }

        int count = DataProvider::AbstractDataProcessor::shapeRecordCount(records, shape);
        if (!count) {
            return;
        }

        if (exists input_map) {
            return processMappedShapeWindows(records, shape, count);
        }

        if (count <= window_size && (isTensorOutputMode() || (isColumnOutputMode() && !include_input_fields))) {
            *ML::Tensor direct_input = inputTensorFromDenseColumns(records, shape, count, fields, input_dtype);
            if (exists direct_input) {
                hash<auto> outputs = model.runTensors({input_tensor: direct_input});
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

        hash<auto> result;
        if (isColumnOutputMode()) {
            addInputFieldColumns(\result, records, shape, matrix);
            for (int start = 0; start < count; start += window_size) {
                int end = start + window_size;
                if (end > count) {
                    end = count;
                }
                bool single_window = start == 0 && end == count;
                list<list<float>> window = single_window
                    ? matrix
                    : matrix[start..(end - 1)];
                appendTensorWindowColumnResult(\result, window, single_window);
            }
            return result;
        }

        for (int start = 0; start < count; start += window_size) {
            int end = start + window_size;
            if (end > count) {
                end = count;
            }
            bool single_window = start == 0 && end == count;
            list<list<float>> window = single_window
                ? matrix
                : matrix[start..(end - 1)];
            appendTensorWindowEventResult(\result, window);
        }
        return result;
    }

    #! Processes complete shaped windows for models with multiple mapped input tensors
    private hash<auto> processMappedShapeWindows(auto records, DataProvider::DataProviderShape shape, int count) {
        if (count <= window_size && (isTensorOutputMode() || (isColumnOutputMode() && !include_input_fields))) {
            *hash<auto> direct_inputs = inputTensorsFromMappedDenseColumns(records, shape, count);
            if (exists direct_inputs) {
                hash<auto> outputs = model.runTensors(direct_inputs);
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

        hash<auto> mapped_matrices = matricesFromInputMap(records, shape);
        if (isTensorOutputMode()) {
            return processMappedTensorShapeWindows(mapped_matrices, count);
        }

        list<list<float>> record_matrix = matrixFromShape(records, shape);
        hash<auto> result;
        if (isColumnOutputMode()) {
            addInputFieldColumns(\result, records, shape, record_matrix);
            for (int start = 0; start < count; start += window_size) {
                int end = start + window_size;
                if (end > count) {
                    end = count;
                }
                int window_count = end - start;
                bool single_window = start == 0 && end == count;
                hash<auto> outputs = runMappedTensorWindow(mapped_matrices, start, window_count, single_window);
                foreach string okey in (keys outputs) {
                    appendTensorOutputColumns(\result, okey, cast<ML::Tensor>(outputs{okey}), window_count,
                        single_window);
                }
            }
            return result;
        }

        for (int start = 0; start < count; start += window_size) {
            int end = start + window_size;
            if (end > count) {
                end = count;
            }
            appendMappedTensorWindowEventResult(\result, mapped_matrices, record_matrix, start, end - start);
        }
        return result;
    }

    #! True when compact column output is configured
    private bool isColumnOutputMode() {
        return output_mode == "columns" || output_mode == "dataframe";
    }

    #! True when raw tensor output is configured
    private bool isTensorOutputMode() {
        return output_mode == "tensor";
    }

    #! Converts compact output columns to the negotiated DataProvider shape
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

    #! Creates a dense input tensor directly from matching top-level dense columns when possible
    private *ML::Tensor inputTensorFromDenseColumns(auto records, DataProvider::DataProviderShape shape,
            int count, list<string> tensor_fields, string dtype) {
        *list<auto> columns = denseColumnsFromShape(records, shape, tensor_fields, dtype);
        if (!exists columns) {
            return;
        }

        return ML::Tensor::fromColumns(columns, dtype);
    }

    #! Creates mapped dense input tensors directly from dense columns when possible
    private *hash<auto> inputTensorsFromMappedDenseColumns(auto records,
            DataProvider::DataProviderShape shape, int count) {
        hash<auto> rv;
        hash<string, list<string>> imap = input_map;
        foreach string tensor_name in (keys imap) {
            *ML::Tensor tensor = inputTensorFromDenseColumns(records, shape, count,
                imap{tensor_name}, input_dtypes{tensor_name});
            if (!exists tensor) {
                return;
            }
            rv{tensor_name} = tensor;
        }
        return rv;
    }

    #! Returns matching dense top-level columns, or NOTHING when the fast path is not safe
    private *list<auto> denseColumnsFromShape(auto records, DataProvider::DataProviderShape shape,
            list<string> tensor_fields, string dtype) {
        if (!tensor_fields) {
            return;
        }

        string required_type = sprintf("buffer<%s>", dtype);
        list<auto> columns;
        foreach string field in (tensor_fields) {
            if (field.find(".") >= 0) {
                return;
            }

            *auto column;
            switch (shape) {
                case DataProvider::DataProviderShape::DataFrameBlock:
                    column = records.getColumnBuffer(field);
                    break;
                case DataProvider::DataProviderShape::BufferColumns:
                    if (DataProvider::AbstractDataProcessor::isColumnarResult(records)) {
                        column = records.getColumn(field);
                    } else if (records.hasKey(field)) {
                        column = records{field};
                    }
                    break;
                case DataProvider::DataProviderShape::HashOfLists:
                    if (records.hasKey(field)) {
                        column = records{field};
                    }
                    break;
            }

            if (!exists column || column.typeCode() != NT_BUFFER || column.fullType() != required_type) {
                return;
            }
            push columns, column;
        }
        return columns;
    }

    #! Extracts numeric input fields as a model matrix from a shaped block
    private list<list<float>> matrixFromShape(auto records, DataProvider::DataProviderShape shape) {
        return matrixFromShapeFields(records, shape, fields);
    }

    #! Extracts selected numeric input fields as a model matrix from a shaped block
    private list<list<float>> matrixFromShapeFields(auto records, DataProvider::DataProviderShape shape,
            list<string> tensor_fields) {
        if (shape == DataProvider::DataProviderShape::DataFrameBlock) {
            return cast<list<list<float>>>(records.toMatrix(tensor_fields));
        }

        if (shape == DataProvider::DataProviderShape::BufferColumns) {
            auto df = DataProvider::AbstractDataProcessor::convertShape(
                records,
                cast<enum<DataProvider::DataProviderShape>>(shape),
                cast<enum<DataProvider::DataProviderShape>>(DataProvider::DataProviderShape::DataFrameBlock))[0];
            return cast<list<list<float>>>(df.toMatrix(tensor_fields));
        }

        list<list<float>> matrix;
        int count = DataProvider::AbstractDataProcessor::shapeRecordCount(records, shape);
        for (int i = 0; i < count; ++i) {
            list<float> row;
            foreach string f in (tensor_fields) {
                push row, records{f}[i].toFloat();
            }
            push matrix, row;
        }
        return matrix;
    }

    #! Extracts each configured ONNX input tensor matrix from a shaped block
    private hash<auto> matricesFromInputMap(auto records, DataProvider::DataProviderShape shape) {
        hash<auto> rv;
        hash<string, list<string>> imap = input_map;
        foreach string tensor_name in (keys imap) {
            rv{tensor_name} = matrixFromShapeFields(records, shape, imap{tensor_name});
        }
        return rv;
    }

    #! Extracts each configured ONNX input tensor matrix from row records
    private hash<auto> matricesFromRecordInputMap(list<hash<auto>> records) {
        hash<auto> rv;
        hash<string, list<string>> imap = input_map;
        foreach string tensor_name in (keys imap) {
            rv{tensor_name} = matrixFromRecordFields(records, imap{tensor_name});
        }
        return rv;
    }

    #! Extracts selected numeric input fields as a model matrix from row records
    private static list<list<float>> matrixFromRecordFields(list<hash<auto>> records,
            list<string> tensor_fields) {
        list<list<float>> matrix;
        foreach hash<auto> rec in (records) {
            list<float> row;
            foreach string f in (tensor_fields) {
                push row, DataProvider::AbstractDataProvider::getFieldValue(rec, f).toFloat();
            }
            push matrix, row;
        }
        return matrix;
    }

    #! Adds original feature columns to compact output when requested
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

    #! Appends one tensor inference window to a hash-of-lists event result
    private appendTensorWindowEventResult(reference<hash<auto>> result, list<list<float>> matrix) {
        ML::Tensor input(matrix, NOTHING, input_dtype);
        hash<auto> outputs = model.runTensors({input_tensor: input});
        int count = matrix.lsize();

        hash<auto> output_values;
        foreach string okey in (keys outputs) {
            output_values{okey} = tensorOutputToRows(cast<ML::Tensor>(outputs{okey}), count);
        }

        for (int i = 0; i < count; ++i) {
            hash<auto> record_values;
            for (int j = 0; j < fields.lsize(); ++j) {
                record_values{fields[j]} = matrix[i][j];
            }
            hash<auto> model_output;
            foreach string okey in (keys output_values) {
                auto oval = output_values{okey};
                if (oval.typeCode() == NT_LIST && oval.lsize() == count) {
                    model_output{okey} = oval[i];
                } else {
                    model_output{okey} = oval;
                }
            }
            push result{"event_type"}, "onnx-model";
            push result{"model_path"}, model_path;
            push result{"execution_provider"}, model.getProviderInfo();
            push result{"record_values"}, record_values;
            push result{"model_output"}, model_output;
        }
    }

    #! Appends one mapped tensor inference window to a hash-of-lists event result
    private appendMappedTensorWindowEventResult(reference<hash<auto>> result, hash<auto> mapped_matrices,
            list<list<float>> record_matrix, int start, int count) {
        hash<auto> outputs = runMappedTensorWindow(mapped_matrices, start, count, False);

        hash<auto> output_values;
        foreach string okey in (keys outputs) {
            output_values{okey} = tensorOutputToRows(cast<ML::Tensor>(outputs{okey}), count);
        }

        for (int i = 0; i < count; ++i) {
            int row = start + i;
            hash<auto> record_values;
            for (int j = 0; j < fields.lsize(); ++j) {
                record_values{fields[j]} = record_matrix[row][j];
            }
            hash<auto> model_output;
            foreach string okey in (keys output_values) {
                auto oval = output_values{okey};
                if (oval.typeCode() == NT_LIST && oval.lsize() == count) {
                    model_output{okey} = oval[i];
                } else {
                    model_output{okey} = oval;
                }
            }
            push result{"event_type"}, "onnx-model";
            push result{"model_path"}, model_path;
            push result{"execution_provider"}, model.getProviderInfo();
            push result{"record_values"}, record_values;
            push result{"model_output"}, model_output;
        }
    }

    #! Appends compact column output for one tensor inference window
    private appendTensorWindowColumnResult(reference<hash<auto>> result, list<list<float>> matrix,
            bool single_window) {
        ML::Tensor input(matrix, NOTHING, input_dtype);
        hash<auto> outputs = model.runTensors({input_tensor: input});
        int count = matrix.lsize();
        foreach string okey in (keys outputs) {
            appendTensorOutputColumns(\result, okey, cast<ML::Tensor>(outputs{okey}), count, single_window);
        }
    }

    #! Appends one tensor output to compact column output
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

    #! Appends list values to a compact output column
    private appendColumnValues(reference<hash<auto>> result, string name, list<auto> values) {
        if (!result.hasKey(name)) {
            result{name} = ();
        }
        result{name} += values;
    }

    #! Runs shaped inference and returns raw tensor outputs
    private hash<auto> processTensorShapeWindows(list<list<float>> matrix, int count) {
        if (count <= window_size) {
            ML::Tensor input(matrix, NOTHING, input_dtype);
            return model.runTensors({input_tensor: input});
        }

        list<hash<auto>> windows;
        for (int start = 0; start < count; start += window_size) {
            int end = start + window_size;
            if (end > count) {
                end = count;
            }
            list<list<float>> window = matrix[start..(end - 1)];
            ML::Tensor input(window, NOTHING, input_dtype);
            push windows, model.runTensors({input_tensor: input});
        }
        return {
            "row_count": count,
            "window_size": window_size,
            "windows": windows,
        };
    }

    #! Runs shaped inference with mapped input tensors and returns raw tensor outputs
    private hash<auto> processMappedTensorShapeWindows(hash<auto> mapped_matrices, int count) {
        if (count <= window_size) {
            return runMappedTensorWindow(mapped_matrices, 0, count, True);
        }

        list<hash<auto>> windows;
        for (int start = 0; start < count; start += window_size) {
            int end = start + window_size;
            if (end > count) {
                end = count;
            }
            push windows, runMappedTensorWindow(mapped_matrices, start, end - start, False);
        }
        return {
            "row_count": count,
            "window_size": window_size,
            "windows": windows,
        };
    }

    #! Runs one mapped input tensor window
    private hash<auto> runMappedTensorWindow(hash<auto> mapped_matrices, int start, int count, bool single_window) {
        hash<auto> inputs;
        hash<string, list<string>> imap = input_map;
        foreach string tensor_name in (keys imap) {
            list<list<float>> matrix = cast<list<list<float>>>(mapped_matrices{tensor_name});
            list<list<float>> window = single_window && count == matrix.lsize()
                ? matrix
                : matrix[start..(start + count - 1)];
            inputs{tensor_name} = new ML::Tensor(window, NOTHING, input_dtypes{tensor_name});
        }
        return model.runTensors(inputs);
    }

    #! Converts a tensor output to row-addressable values where possible
    private auto tensorOutputToRows(ML::Tensor tensor, int row_count) {
        list<int> shape = tensor.getShape();
        if (shape.lsize() == 1 && shape[0] == row_count) {
            return tensor.getBuffer().toList();
        }
        if (shape.lsize() == 2 && shape[0] == row_count && shape[1] == 1) {
            return tensor.getBuffer().toList();
        }
        auto nested = tensor.toList();
        if (nested.typeCode() == NT_LIST && nested.lsize() == row_count) {
            return nested;
        }
        return nested;
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
        return new HashDeclDataType(TypedHash::forName("DataProviderML::OnnxInferenceEventInfo"));
    }
}
}
