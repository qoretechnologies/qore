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

        @return output hash
    */
    hash<auto> run(hash<auto> inputs) {
        if (isOnnx()) {
            return model.run(inputs);
        }
        return {"prediction": model.predict(inputs)};
    }

    #! Runs a feature matrix through the wrapped model
    /** ONNX models receive the matrix as a single input tensor. Native models
        use \c predictMatrix() and return the result under \c prediction.

        @param matrix row-major feature matrix
        @param input_tensor optional ONNX input tensor name; when omitted, the
            first model input is used

        @return output hash

        @throw ML-REGISTRY-ERROR if an ONNX model has no input tensors
    */
    hash<auto> runFeatureMatrix(list<list<float>> matrix, *string input_tensor) {
        if (isOnnx()) {
            string tensor_name = resolveOnnxInputTensor(input_tensor);
            return model.run({tensor_name: matrix});
        }
        return {"prediction": model.predictMatrix(matrix)};
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
}

} # namespace QoreModelRegistry
