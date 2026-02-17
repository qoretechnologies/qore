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

    @since %DataProviderML 1.0
*/
public class QoreOnnxModelProcessor inherits DataProvider::AbstractDataProvider {
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
                    "model in the order listed; for example: \"feature1,feature2,feature3\"",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Batch Size",
                "short_desc": "Number of records to process at once",
                "desc": "The number of records to collect before sending them to the model "
                    "for inference as a batch; larger batches may be more efficient but "
                    "use more memory; must be at least 1",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
            },
            "input_tensor": <DataProviderOptionInfo>{
                "display_name": "Input Tensor Name",
                "short_desc": "Which model input to use (advanced)",
                "desc": "The name of the specific model input to feed data into; only needed "
                    "for models with multiple inputs; if not specified, the model's first "
                    "input is used automatically, which is correct for most models",
                "type": AbstractDataProviderTypeMap."*string",
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
                    "only used when 'CUDA' is listed in execution_providers; defaults to \"0\"",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "intra_op_threads": <DataProviderOptionInfo>{
                "display_name": "Intra-Op Threads",
                "short_desc": "Threads within each model operation",
                "desc": "The number of threads used to speed up individual model operations "
                    "(e.g. a single matrix multiplication); increasing this helps with large "
                    "models that have big operations; defaults to 1",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "inter_op_threads": <DataProviderOptionInfo>{
                "display_name": "Inter-Op Threads",
                "short_desc": "Threads across independent operations",
                "desc": "The number of threads used to run independent model operations in "
                    "parallel (e.g. two branches of a neural network that don't depend on "
                    "each other); 0 means the runtime chooses automatically; defaults to 0",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "graph_optimization_level": <DataProviderOptionInfo>{
                "display_name": "Graph Optimization",
                "short_desc": "Level of model graph optimization",
                "desc": "Controls how aggressively the runtime optimizes the model graph "
                    "before running inference; higher levels can improve speed but take "
                    "longer to initialize; defaults to 'All'",
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
                    "model loading or inference issues; defaults to 'Warning'",
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

        #! ONNX model path
        string model_path;

        #! ONNX model instance
        ML::OnnxModel model;

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
        fields = map trim($1), copts.fields.split(","), trim($1).val();
        if (!fields) {
            throw "CONSTRUCTOR-ERROR", "no valid field names provided in 'fields' option";
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

        buffer = ();
        configured = True;
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
        # Build a matrix from fields: list of list of float
        list<list<float>> matrix;
        foreach hash<auto> rec in (records) {
            list<float> row;
            foreach string f in (fields) {
                push row, rec{f}.toFloat();
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
                record_values{f} = records[i]{f};
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

    #! Does not support bulk API
    private bool supportsBulkApiImpl() {
        return False;
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
