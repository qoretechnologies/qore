# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ModelExecutableAdapter.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Main QoreModelRegistry namespace
public namespace QoreModelRegistry {

#! Uniform execution adapter for native Qore ML and ONNX Runtime model objects
/** The registry can load different executable model types: native Qore ML
    models expose \c predict() / \c predictMatrix(), while ONNX Runtime models
    expose \c run() / \c runBatch(). This adapter gives registry consumers one
    small API surface and keeps model-type dispatch out of DataProviderML
    processors and application code.

    @since QoreModelRegistry 2.3
*/
public class ModelExecutableAdapter {
    private {
        #! Loaded executable model object
        object model;

        #! Registry manifest for the loaded version, when available
        *hash<ModelVersionInfo> manifest;

        #! Runtime identifier such as \c "qore-ml" or \c "onnxruntime"
        string runtime;
    }

    #! Creates an execution adapter around a loaded model object
    /** @param model the executable model object
        @param manifest optional registry manifest for the loaded model version

        @throw ML-REGISTRY-ERROR if \a model is not provided
    */
    constructor(object model, *hash<ModelVersionInfo> manifest) {
        if (!model) {
            throw "ML-REGISTRY-ERROR", "ModelExecutableAdapter requires an executable model object";
        }
        self.model = model;
        self.manifest = manifest;
        runtime = manifest ? (manifest.runtime ?? (manifest.execution ? manifest.execution.runtime : NOTHING)
            ?? inferRuntime(model)) : inferRuntime(model);
    }

    #! Returns the wrapped executable model object
    /** @return the wrapped model object
    */
    object getModel() {
        return model;
    }

    #! Returns the registry manifest, if one was supplied
    /** @return the model version manifest or \c NOTHING
    */
    *hash<ModelVersionInfo> getManifest() {
        return manifest;
    }

    #! Returns the loaded version ID, if known
    /** @return the version ID from the manifest or \c NOTHING
    */
    *string getVersionId() {
        return manifest ? manifest.version_id : NOTHING;
    }

    #! Returns the execution runtime identifier
    /** @return runtime identifier such as \c "qore-ml" or \c "onnxruntime"
    */
    string getRuntime() {
        return runtime;
    }

    #! Returns True if the wrapped model is an ONNX Runtime model
    /** @return True for \c ML::OnnxModel-backed adapters
    */
    bool isOnnx() {
        return runtime == "onnxruntime" || model instanceof ML::OnnxModel;
    }

    #! Returns model metadata when the wrapped model exposes it
    /** ONNX models return \c ML::OnnxModelInfo. Native Qore ML models return
        their \c getModelInfo() result when available.

        @return model metadata
    */
    hash<auto> getModelInfo() {
        return model.getModelInfo();
    }

    #! Runs a single prediction through the wrapped model
    /** For native Qore ML models this calls \c predict(input). For ONNX models
        \a input must be a tensor-name hash and this calls \c run(input).

        @param input input record or tensor-name hash

        @return model result
    */
    auto predict(hash<auto> input) {
        if (isOnnx()) {
            return model.run(input);
        }
        return model.predict(input);
    }

    #! Runs a batch of prediction requests through the wrapped model
    /** For ONNX models each batch element must be a tensor-name hash. Native
        models are called once per input record because not every native model
        exposes a matrix API.

        @param inputs input records or tensor-name hashes

        @return a list of model results
    */
    list<auto> predictBatch(list<hash<auto>> inputs) {
        if (isOnnx()) {
            return model.runBatch(inputs);
        }

        list<auto> rv;
        foreach hash<auto> input in (inputs) {
            push rv, model.predict(input);
        }
        return rv;
    }

    #! Runs model inference with explicit tensor inputs
    /** ONNX models receive \a inputs directly through \c run(). Native models
        receive the same hash through \c predict() and the return value is
        wrapped under \c prediction for a hash-shaped result.

        @param inputs tensor-name hash or native model input hash
        @param output_names optional ONNX output tensor names to materialize;
            omit to return all ONNX outputs

        @return output hash
    */
    hash<auto> run(hash<auto> inputs, *list<string> output_names) {
        if (isOnnx()) {
            return model.run(inputs, output_names);
        }
        if (exists output_names) {
            throw "ML-REGISTRY-ERROR",
                "output_names can only be used with ONNX Runtime registry models";
        }
        return {"prediction": model.predict(inputs)};
    }

    #! Runs ONNX inference with explicit tensor inputs and returns ML::Tensor outputs
    /** @param inputs tensor-name hash where each value is an @ref Qore::ML::Tensor
        @param output_names optional output tensor names to materialize; when
            omitted, all model outputs are returned

        @return output hash where each value is an @ref Qore::ML::Tensor

        @throw ML-REGISTRY-ERROR if the wrapped model is not an ONNX Runtime model

        @since QoreModelRegistry 2.3
    */
    hash<auto> runTensors(hash<auto> inputs, *list<string> output_names) {
        if (!isOnnx()) {
            throw "ML-REGISTRY-ERROR",
                "runTensors() requires an ONNX Runtime model";
        }
        return model.runTensors(inputs, output_names);
    }

    #! Runs a feature matrix through the wrapped model
    /** ONNX models receive the matrix as a single input tensor. Native models
        use \c predictMatrix() and return the result under \c prediction.

        @param matrix row-major feature matrix
        @param input_tensor optional ONNX input tensor name; when omitted, the
            first model input is used
        @param output_names optional ONNX output tensor names to materialize;
            omit to return all ONNX outputs

        @return output hash

        @throw ML-REGISTRY-ERROR if an ONNX model has no input tensors
    */
    hash<auto> runFeatureMatrix(list<list<float>> matrix, *string input_tensor,
            *list<string> output_names) {
        if (isOnnx()) {
            return tensorOutputsToRows(runFeatureMatrixTensors(matrix, input_tensor, output_names),
                matrix.lsize());
        }
        if (exists output_names) {
            throw "ML-REGISTRY-ERROR",
                "output_names can only be used with ONNX Runtime registry models";
        }
        return {"prediction": model.predictMatrix(matrix)};
    }

    #! Runs a feature matrix through an ONNX model and returns ML::Tensor outputs
    /** This method avoids converting dense ONNX Runtime outputs back to nested
        Qore lists. It is intended for DataProviderML and other bulk inference
        callers that can consume tensor-shaped results directly or split them
        into per-record outputs with stable row counts.

        @param matrix row-major feature matrix
        @param input_tensor optional ONNX input tensor name; when omitted, the
            first model input is used
        @param output_names optional ONNX output tensor names to materialize;
            omit to return all ONNX outputs

        @return output hash where each value is an @ref Qore::ML::Tensor

        @throw ML-REGISTRY-ERROR if the wrapped model is not an ONNX model, the
            model has no input tensors, or the input tensor type is unsupported

        @since QoreModelRegistry 2.3
    */
    hash<auto> runFeatureMatrixTensors(list<list<float>> matrix, *string input_tensor,
            *list<string> output_names) {
        if (!isOnnx()) {
            throw "ML-REGISTRY-ERROR",
                "runFeatureMatrixTensors() requires an ONNX Runtime model";
        }

        string tensor_name = resolveOnnxInputTensor(input_tensor);
        string dtype = resolveOnnxInputTensorDtype(tensor_name);
        ML::Tensor input(data: matrix, dtype: dtype);
        return model.runTensors({tensor_name: input}, output_names);
    }

    #! Runs a dense feature tensor through an ONNX model and returns ML::Tensor outputs
    /** This method is the lowest-copy registry execution path for callers that
        already have a dense, correctly typed @ref Qore::ML::Tensor. It avoids
        materializing row lists before invoking ONNX Runtime.

        @param input dense input tensor
        @param input_tensor optional ONNX input tensor name; when omitted, the
            first model input is used
        @param output_names optional ONNX output tensor names to materialize;
            omit to return all ONNX outputs

        @return output hash where each value is an @ref Qore::ML::Tensor

        @throw ML-REGISTRY-ERROR if the wrapped model is not an ONNX model, the
            model has no input tensors, or the input tensor dtype does not match
            the selected ONNX input

        @since QoreModelRegistry 2.3
    */
    hash<auto> runFeatureTensor(ML::Tensor input, *string input_tensor, *list<string> output_names) {
        if (!isOnnx()) {
            throw "ML-REGISTRY-ERROR",
                "runFeatureTensor() requires an ONNX Runtime model";
        }

        string tensor_name = resolveOnnxInputTensor(input_tensor);
        string dtype = resolveOnnxInputTensorDtype(tensor_name);
        if (input.getDtype() != dtype) {
            throw "ML-REGISTRY-ERROR",
                sprintf("input tensor %y expects dtype %y, but the supplied tensor has dtype %y",
                    tensor_name, dtype, input.getDtype());
        }
        return model.runTensors({tensor_name: input}, output_names);
    }

    #! Returns the ML::Tensor dtype expected by an ONNX input tensor
    /** @param input_tensor optional ONNX input tensor name; when omitted, the
            first model input is used

        @return dtype name such as \c float32, \c float64, \c int64, or \c bool

        @throw ML-REGISTRY-ERROR if the wrapped model is not an ONNX model, the
            model has no input tensors, or the input tensor type is unsupported

        @since QoreModelRegistry 2.3
    */
    string getOnnxInputTensorDtype(*string input_tensor) {
        if (!isOnnx()) {
            throw "ML-REGISTRY-ERROR",
                "getOnnxInputTensorDtype() requires an ONNX Runtime model";
        }
        return resolveOnnxInputTensorDtype(resolveOnnxInputTensor(input_tensor));
    }

    #! Returns execution-provider and tensor metadata diagnostics
    /** The returned hash is intentionally untyped so new runtime-specific
        diagnostics can be added without changing the adapter API.

        @return diagnostic information including runtime, version, ONNX
            provider information, and input/output tensor metadata when
            available

        @since QoreModelRegistry 2.3
    */
    hash<auto> getProviderDiagnostics() {
        hash<auto> rv = {
            "runtime": runtime,
            "is_onnx": isOnnx(),
            "version_id": getVersionId(),
        };

        if (manifest) {
            rv{"manifest_runtime"} = manifest.runtime;
            rv{"artifact_kind"} = manifest.artifact_kind;
            rv{"framework"} = manifest.framework;
            rv{"execution"} = manifest.execution;
        }

        if (isOnnx()) {
            hash<auto> info = getModelInfo();
            rv{"active_provider"} = info.active_provider;
            rv{"available_providers"} = ML::ml_get_onnx_providers();
            rv{"inputs"} = info.inputs;
            rv{"outputs"} = info.outputs;
        }

        return rv;
    }

    #! Returns ONNX Runtime inference counters for ONNX models
    /** @return inference statistics for ONNX models, or an empty hash for
        native Qore ML models

        @since QoreModelRegistry 2.4
    */
    hash<auto> getInferenceStats() {
        if (!isOnnx()) {
            return {};
        }
        return model.getInferenceStats();
    }

    #! Resets ONNX Runtime inference counters for ONNX models
    /** @since QoreModelRegistry 2.4
    */
    nothing resetInferenceStats() {
        if (isOnnx()) {
            model.resetInferenceStats();
        }
    }

    #! Ends ONNX Runtime profiling and returns the generated profile path
    /** @return generated profile file path or NOTHING when profiling is not active

        @since QoreModelRegistry 2.4
    */
    *string endProfiling() {
        if (!isOnnx()) {
            return;
        }
        return model.endProfiling();
    }

    #! Infers a runtime identifier from the object class
    private static string inferRuntime(object model) {
        if (model instanceof ML::OnnxModel) {
            return "onnxruntime";
        }
        return "qore-ml";
    }

    #! Resolves an ONNX input tensor name
    private string resolveOnnxInputTensor(*string input_tensor) {
        if (input_tensor.val()) {
            return input_tensor;
        }
        list<hash<ML::OnnxTensorInfo>> inputs = model.getInputInfo();
        if (!inputs) {
            throw "ML-REGISTRY-ERROR", "ONNX model has no input tensors";
        }
        return inputs[0].name;
    }

    #! Resolves the ML::Tensor dtype needed for the ONNX input tensor
    private string resolveOnnxInputTensorDtype(string tensor_name) {
        foreach hash<ML::OnnxTensorInfo> input in (model.getInputInfo()) {
            if (input.name == tensor_name) {
                return onnxTypeToTensorDtype(input.type);
            }
        }

        throw "ML-REGISTRY-ERROR",
            sprintf("ONNX model does not have an input tensor named '%s'", tensor_name);
    }

    #! Maps ONNX Runtime tensor type names to ML::Tensor dtype names
    private static string onnxTypeToTensorDtype(string onnx_type) {
        if (onnx_type == "float") {
            return "float32";
        }
        if (onnx_type == "double") {
            return "float64";
        }
        if (onnx_type == "int64" || onnx_type == "int32"
                || onnx_type == "int16" || onnx_type == "int8"
                || onnx_type == "bool") {
            return onnx_type;
        }

        throw "ML-REGISTRY-ERROR",
            sprintf("ONNX input tensor type '%s' cannot be represented as an ML::Tensor dtype",
                onnx_type);
    }

    #! Converts tensor outputs to row-addressable values while preserving compact scalar rows
    private static hash<auto> tensorOutputsToRows(hash<auto> outputs, int row_count) {
        hash<auto> rv;
        foreach string okey in (keys outputs) {
            rv{okey} = tensorOutputToRows(cast<ML::Tensor>(outputs{okey}), row_count);
        }
        return rv;
    }

    #! Converts a tensor output to the row shape returned by OnnxModel::run() where possible
    private static auto tensorOutputToRows(ML::Tensor tensor, int row_count) {
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
}

} # namespace QoreModelRegistry
