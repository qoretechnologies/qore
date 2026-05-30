/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_OnnxModel.h

    Qore ml module - OnnxModel class

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_MODULE_ML_QC_ONNXMODEL_H
#define _QORE_MODULE_ML_QC_ONNXMODEL_H

#include <qore/Qore.h>
#include <qore/QoreFuture.h>

#include "QC_Tensor.h"

#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <deque>
#include <atomic>

#ifdef HAVE_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#include <unordered_map>
#endif

DLLEXPORT extern qore_classid_t CID_ONNXMODEL;
DLLEXPORT extern qore_classid_t CID_ONNXRUNOPTIONS;
DLLEXPORT extern qore_classid_t CID_ONNXIOBINDING;
DLLEXPORT extern qore_classid_t CID_ONNXSESSIONPOOL;
DLLLOCAL extern QoreClass* QC_ONNXMODEL;
DLLLOCAL extern QoreClass* QC_ONNXRUNOPTIONS;
DLLLOCAL extern QoreClass* QC_ONNXIOBINDING;
DLLLOCAL extern QoreClass* QC_ONNXSESSIONPOOL;

// ONNX hashdecls (defined by the qpp-generated registration code in ql_ml.cpp)
extern const TypedHashDecl* hashdeclOnnxTensorInfo;
extern const TypedHashDecl* hashdeclOnnxModelInfo;
extern const TypedHashDecl* hashdeclOnnxProviderConfig;
extern const TypedHashDecl* hashdeclOnnxDeviceBindingConfig;
extern const TypedHashDecl* hashdeclOnnxSessionConfig;
extern const TypedHashDecl* hashdeclOnnxProviderDiagnostic;

DLLLOCAL void preinitOnnxModelClass();
DLLLOCAL void preinitOnnxRunOptionsClass();
DLLLOCAL void preinitOnnxIoBindingClass();
DLLLOCAL void preinitOnnxSessionPoolClass();
DLLLOCAL QoreClass* initOnnxModelClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initOnnxRunOptionsClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initOnnxIoBindingClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initOnnxSessionPoolClass(QoreNamespace& ns);

#ifdef HAVE_ONNXRUNTIME

//! Returns available ONNX Runtime providers as a Qore list
DLLLOCAL QoreListNode* qore_ml_get_onnx_providers(ExceptionSink* xsink);

//! Returns known ONNX Runtime provider option metadata
DLLLOCAL QoreHashNode* qore_ml_get_onnx_provider_options(ExceptionSink* xsink);

//! Saves an optimized ONNX or ORT artifact by loading the input model with ONNX Runtime
DLLLOCAL QoreHashNode* qore_ml_onnx_optimize_model(const char* input_path, const char* output_path,
    const QoreHashNode* config, bool ort_format, ExceptionSink* xsink);

//! Input/output tensor metadata
struct TensorMeta {
    std::string name;
    ONNXTensorElementDataType element_type;
    std::vector<int64_t> shape;
    std::vector<std::string> symbolic_shape;
};

//! Execution provider diagnostic information
struct OnnxProviderDiagnostic {
    std::string name;
    std::unordered_map<std::string, std::string> options;
    std::string error;
    bool requested = false;
    bool required = false;
    bool available = false;
    bool appended = false;
    bool auto_selected = false;
    bool active = false;
    bool cpu_fallback = false;
};

//! Output device placement for provider-managed device binding
enum class OnnxOutputDevice {
    Cpu,        //!< force CPU output memory
    Provider,   //!< use the active non-CPU provider's device memory
    Explicit,   //!< a named provider/device (see device_name/device_id)
};

//! Parsed OnnxSessionConfig.device_binding policy
struct OnnxDeviceBindingPolicy {
    bool enabled = false;
    OnnxOutputDevice default_output_device = OnnxOutputDevice::Provider;
    std::string device_name;        //!< normalized provider name when default_output_device == Explicit
    int64_t device_id = -1;         //!< explicit device id, or -1 for provider default
    bool allow_host_fallback = false;
    bool materialize_outputs = false;
    bool require_zero_copy_inputs = false;
    bool require_zero_copy_outputs = false;
};

struct OnnxBoundOrtValue;
class QoreOnnxIoBinding;
class QoreOnnxRunOptions;

//! ONNX Model wrapper class
class QoreOnnxModel : public AbstractPrivateData {
public:
    //! Constructor — loads the ONNX model from the given file path
    DLLLOCAL QoreOnnxModel(const char* model_path, ExceptionSink* xsink);

    //! Constructor — loads the ONNX model with session configuration
    DLLLOCAL QoreOnnxModel(const char* model_path, const QoreHashNode* config, ExceptionSink* xsink);

    //! Constructor — loads the ONNX model from an in-memory byte buffer
    /** @param model_data pointer to the raw ONNX model bytes
        @param model_data_len length of the buffer in bytes
        @param xsink exception sink

        The buffer is copied into ONNX Runtime, so the caller may free it
        immediately after construction.
    */
    DLLLOCAL QoreOnnxModel(const void* model_data, size_t model_data_len, ExceptionSink* xsink);

    //! Constructor — loads the ONNX model from memory with session configuration
    DLLLOCAL QoreOnnxModel(const void* model_data, size_t model_data_len,
        const QoreHashNode* config, ExceptionSink* xsink);

    //! Returns the active execution provider name
    DLLLOCAL const std::string& getActiveProvider() const { return active_provider; }

    //! Returns the providers available in the linked ONNX Runtime build
    DLLLOCAL QoreListNode* getProviders(ExceptionSink* xsink) const;

    //! Returns known execution-provider option metadata
    DLLLOCAL QoreHashNode* getProviderOptions(ExceptionSink* xsink) const;

    //! Returns explicitly requested providers in normalized form
    DLLLOCAL QoreListNode* getRequestedProviders(ExceptionSink* xsink) const;

    //! Returns structured provider diagnostics
    DLLLOCAL QoreListNode* getProviderDiagnostics(ExceptionSink* xsink) const;

    //! Returns the effective provider report for this session
    DLLLOCAL QoreHashNode* getEffectiveProviderReport(ExceptionSink* xsink) const;

    //! Run inference with named input tensors
    /** @param inputs hash where keys are tensor names, values are data (scalars, lists, or nested lists)
        @param xsink exception sink
        @return hash of output tensors (keys = output names, values = data)
    */
    DLLLOCAL QoreHashNode* run(const QoreHashNode* inputs, ExceptionSink* xsink);

    //! Run inference with named input tensors and selected outputs
    DLLLOCAL QoreHashNode* run(const QoreHashNode* inputs, const QoreListNode* output_names,
        ExceptionSink* xsink);

    //! Run inference and return outputs as ML::Tensor objects
    DLLLOCAL QoreHashNode* runTensors(const QoreHashNode* inputs, ExceptionSink* xsink);

    //! Run inference and return selected outputs as ML::Tensor objects
    DLLLOCAL QoreHashNode* runTensors(const QoreHashNode* inputs, const QoreListNode* output_names,
        ExceptionSink* xsink);

    //! Creates a reusable ONNX Runtime I/O binding for this model
    DLLLOCAL QoreObject* createBinding(QoreProgram* pgm, ExceptionSink* xsink);

    //! Runs a reusable ONNX Runtime I/O binding
    DLLLOCAL QoreHashNode* runBound(const QoreObject* binding_obj, const QoreObject* options_obj,
        bool return_tensors, ExceptionSink* xsink);

    //! Ends ONNX Runtime profiling and returns the generated profile path
    DLLLOCAL QoreStringNode* endProfiling(ExceptionSink* xsink);

    //! Returns Qore-side inference counters and latency stats
    DLLLOCAL QoreHashNode* getInferenceStats(ExceptionSink* xsink) const;

    //! Resets Qore-side inference counters and latency stats
    DLLLOCAL void resetInferenceStats();

    //! Saves an optimized ONNX or ORT artifact from the original model source
    DLLLOCAL QoreHashNode* saveOptimized(const char* output_path, const QoreHashNode* config,
        bool ort_format, ExceptionSink* xsink) const;

    //! Run batch inference
    /** @param batch list of input hashes
        @param xsink exception sink
        @return list of output hashes
    */
    DLLLOCAL QoreListNode* runBatch(const QoreListNode* batch, ExceptionSink* xsink);

    //! Run batch inference with selected outputs
    DLLLOCAL QoreListNode* runBatch(const QoreListNode* batch, const QoreListNode* output_names,
        ExceptionSink* xsink);

    //! Get model info
    DLLLOCAL QoreHashNode* getModelInfo(ExceptionSink* xsink) const;

    //! Get input tensor info
    DLLLOCAL QoreListNode* getInputInfo(ExceptionSink* xsink) const;

    //! Get output tensor info
    DLLLOCAL QoreListNode* getOutputInfo(ExceptionSink* xsink) const;

    //! Whether the model is loaded
    DLLLOCAL bool isLoaded() const { return session != nullptr; }

private:
    friend class QoreOnnxIoBinding;

    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::Session> session;
    Ort::AllocatorWithDefaultOptions allocator;

    std::vector<TensorMeta> input_meta;
    std::vector<TensorMeta> output_meta;

    std::vector<std::string> available_providers;
    std::vector<std::string> requested_providers;
    std::vector<std::string> required_providers;
    std::vector<OnnxProviderDiagnostic> provider_diagnostics;

    std::string source_model_path;
    std::vector<char> source_model_data;
    std::string source_load_model_format;

    std::string active_provider;
    bool auto_provider_selected = false;
    bool explicit_provider_config = false;
    bool allow_cpu_fallback = true;
    bool fail_on_provider_fallback = false;
    bool cpu_fallback_used = false;
    OnnxDeviceBindingPolicy device_binding;
    bool profiling_enabled = false;
    std::string profiling_file_prefix;
    std::string last_profile_file;

    mutable std::mutex stats_mutex;
    uint64_t inference_run_count = 0;
    uint64_t inference_batch_items = 0;
    double total_inference_ms = 0.0;
    double last_inference_ms = 0.0;
    double max_inference_ms = 0.0;

    //! Initialize the Ort::Env and populate session options from the config hash
    /** Shared between the path-based and memory-based configured constructors.
        @return true if env and options were populated; false if an exception was raised
    */
    DLLLOCAL bool initEnvAndOptions(Ort::SessionOptions& session_options,
        const QoreHashNode* config, ExceptionSink* xsink);

    //! Configure session options from a config hash
    DLLLOCAL void configureSession(Ort::SessionOptions& opts, const QoreHashNode* config,
        ExceptionSink* xsink);

    //! Configure non-provider session options from a config hash
    DLLLOCAL void configureBaseSessionOptions(Ort::SessionOptions& opts,
        const QoreHashNode* config, ExceptionSink* xsink);

    //! Configure provider behavior from a config hash
    DLLLOCAL void configureProviderPolicy(const QoreHashNode* config, ExceptionSink* xsink);

    //! Parse and validate the device_binding policy from a config hash
    DLLLOCAL void configureDeviceBinding(const QoreHashNode* config, ExceptionSink* xsink);

    //! Resolves the target device for a provider-managed output binding.
    /** @param device optional explicit {kind, device_id} hash; NOTHING uses the
            device_binding policy / active provider
        @param out filled with the resolved device when the result is true
        @return true if outputs should be bound to device memory (see \a out);
            false for CPU memory (either policy-selected or host fallback)
    */
    DLLLOCAL bool resolveOutputDeviceInfo(const QoreHashNode* device,
        QoreBufferDeviceInfo& out, ExceptionSink* xsink) const;

    //! Create a path-based session, falling back to CPU if an auto-selected provider fails
    DLLLOCAL void createSessionFromPath(const char* model_path, Ort::SessionOptions& opts,
        const QoreHashNode* config, ExceptionSink* xsink);

    //! Create an in-memory session, falling back to CPU if an auto-selected provider fails
    DLLLOCAL void createSessionFromMemory(const void* model_data, size_t model_data_len,
        Ort::SessionOptions& opts, const QoreHashNode* config, ExceptionSink* xsink);

    //! Auto-detect and append the best available GPU execution provider
    DLLLOCAL void autoDetectProvider(Ort::SessionOptions& opts);

    //! Returns true if a provider is in the ONNX Runtime available provider list
    DLLLOCAL bool isProviderAvailable(const std::string& name) const;

    //! Normalize a user provider alias to an ONNX Runtime provider name
    DLLLOCAL static std::string normalizeProviderName(const std::string& name);

    //! Returns a readable provider list for diagnostics
    DLLLOCAL std::string availableProvidersString() const;

    //! Records or updates provider diagnostics
    DLLLOCAL OnnxProviderDiagnostic& providerDiagnostic(const std::string& name);

    //! Marks the provider as appended and active
    DLLLOCAL void markProviderAppended(const std::string& name, bool auto_selected);

    //! Records a provider error
    DLLLOCAL void markProviderError(const std::string& name, const std::string& error);

    //! Validates required provider policy after provider configuration
    DLLLOCAL void validateRequiredProviders(ExceptionSink* xsink) const;

    //! Append an execution provider to session options
    DLLLOCAL void appendProvider(Ort::SessionOptions& opts, const std::string& name,
        const std::unordered_map<std::string, std::string>& provider_opts,
        ExceptionSink* xsink);

    //! Load model metadata (input/output names, types, shapes)
    DLLLOCAL void loadMetadata(ExceptionSink* xsink);

    //! Convert ONNX element type to string
    DLLLOCAL static const char* elementTypeToString(ONNXTensorElementDataType type);

    //! Convert an Ort::Value output tensor to Qore data
    DLLLOCAL QoreValue convertOutputTensor(Ort::Value& tensor, ExceptionSink* xsink);

    //! Convert an Ort::Value output tensor to an ML::Tensor object
    DLLLOCAL QoreValue convertOutputTensorToTensor(Ort::Value&& tensor, ExceptionSink* xsink);

    //! Shared inference implementation
    DLLLOCAL QoreHashNode* runImpl(const QoreHashNode* inputs, bool return_tensors,
        const QoreListNode* output_names, ExceptionSink* xsink);

    //! Resolves output metadata requested by the caller
    DLLLOCAL std::vector<const TensorMeta*> selectOutputMeta(const QoreListNode* output_names,
        ExceptionSink* xsink) const;

    //! Finds an input tensor by name
    DLLLOCAL const TensorMeta* findInputMeta(const char* name) const;

    //! Finds an output tensor by name
    DLLLOCAL const TensorMeta* findOutputMeta(const char* name) const;

    //! Creates an Ort::Value and backing lifetime holders for an input value
    DLLLOCAL std::unique_ptr<OnnxBoundOrtValue> prepareInputValue(const TensorMeta& meta,
        QoreValue value, ExceptionSink* xsink);

    //! Creates an Ort::Value and backing lifetime holders for a preallocated output tensor
    DLLLOCAL std::unique_ptr<OnnxBoundOrtValue> prepareOutputTensorValue(const TensorMeta& meta,
        const QoreObject* tensor_obj, ExceptionSink* xsink);

    //! Converts bound outputs to a Qore hash
    DLLLOCAL QoreHashNode* collectBoundOutputs(Ort::IoBinding& binding, bool return_tensors,
        ExceptionSink* xsink);
    DLLLOCAL void recordInference(double elapsed_ms, uint64_t batch_items);

    //! Build a QoreHashNode with OnnxTensorInfo from a TensorMeta
    DLLLOCAL QoreHashNode* tensorMetaToHash(const TensorMeta& meta, ExceptionSink* xsink) const;

    //! Flatten a nested Qore list into a flat vector of floats
    DLLLOCAL void flattenToFloats(const QoreValue& val, std::vector<float>& out,
        ExceptionSink* xsink);

    //! Flatten a nested Qore list into a flat vector of doubles
    DLLLOCAL void flattenToDoubles(const QoreValue& val, std::vector<double>& out,
        ExceptionSink* xsink);

    //! Flatten a nested Qore list into a flat vector of float16 values
    DLLLOCAL void flattenToFloat16(const QoreValue& val, std::vector<Ort::Float16_t>& out,
        ExceptionSink* xsink);

    //! Flatten a nested Qore list into a flat vector of bfloat16 values
    DLLLOCAL void flattenToBFloat16(const QoreValue& val, std::vector<Ort::BFloat16_t>& out,
        ExceptionSink* xsink);

    //! Flatten a nested Qore list into a flat vector of int32_t
    DLLLOCAL void flattenToInt32(const QoreValue& val, std::vector<int32_t>& out,
        ExceptionSink* xsink);

    //! Flatten a nested Qore list into a flat vector of int16_t
    DLLLOCAL void flattenToInt16(const QoreValue& val, std::vector<int16_t>& out,
        ExceptionSink* xsink);

    //! Flatten a nested Qore list into a flat vector of int8_t
    DLLLOCAL void flattenToInt8(const QoreValue& val, std::vector<int8_t>& out,
        ExceptionSink* xsink);

    //! Flatten a nested Qore list into a flat vector of uint32_t
    DLLLOCAL void flattenToUInt32(const QoreValue& val, std::vector<uint32_t>& out,
        ExceptionSink* xsink);

    //! Flatten a nested Qore list into a flat vector of uint16_t
    DLLLOCAL void flattenToUInt16(const QoreValue& val, std::vector<uint16_t>& out,
        ExceptionSink* xsink);

    //! Flatten a nested Qore list into a flat vector of uint8_t
    DLLLOCAL void flattenToUInt8(const QoreValue& val, std::vector<uint8_t>& out,
        ExceptionSink* xsink);

    //! Flatten a nested Qore list into a flat vector of int64_t
    DLLLOCAL void flattenToInt64(const QoreValue& val, std::vector<int64_t>& out,
        ExceptionSink* xsink);

    //! Flatten a nested Qore list into a flat vector of uint64_t
    DLLLOCAL void flattenToUInt64(const QoreValue& val, std::vector<uint64_t>& out,
        ExceptionSink* xsink);

    //! Flatten a nested Qore list into a flat vector of bool bytes
    DLLLOCAL void flattenToBools(const QoreValue& val, std::vector<uint8_t>& out,
        ExceptionSink* xsink);

    //! Flatten a nested Qore list into a flat vector of strings
    DLLLOCAL void flattenToStrings(const QoreValue& val, std::vector<std::string>& out,
        ExceptionSink* xsink);

    //! Reshape a flat float vector into a nested Qore list based on shape
    DLLLOCAL QoreValue reshapeOutput(const float* data, const std::vector<int64_t>& shape,
        size_t& offset);

    //! Reshape a flat double vector into a nested Qore list
    DLLLOCAL QoreValue reshapeOutput(const double* data, const std::vector<int64_t>& shape,
        size_t& offset);

    //! Reshape flat float16 data (converted to float)
    DLLLOCAL QoreValue reshapeOutputFloat16(const Ort::Float16_t* data,
        const std::vector<int64_t>& shape, size_t& offset, ExceptionSink* xsink);

    //! Reshape flat bfloat16 data (converted to float)
    DLLLOCAL QoreValue reshapeOutputBFloat16(const Ort::BFloat16_t* data,
        const std::vector<int64_t>& shape, size_t& offset, ExceptionSink* xsink);

    //! Reshape flat int32 data (converted to int64 for Qore)
    DLLLOCAL QoreValue reshapeOutputInt32(const int32_t* data, const std::vector<int64_t>& shape,
        size_t& offset);

    //! Reshape flat int16 data (converted to int64 for Qore)
    DLLLOCAL QoreValue reshapeOutputInt16(const int16_t* data, const std::vector<int64_t>& shape,
        size_t& offset, ExceptionSink* xsink);

    //! Reshape flat int8 data (converted to int64 for Qore)
    DLLLOCAL QoreValue reshapeOutputInt8(const int8_t* data, const std::vector<int64_t>& shape,
        size_t& offset, ExceptionSink* xsink);

    //! Reshape flat uint32 data (converted to int64 for Qore)
    DLLLOCAL QoreValue reshapeOutputUInt32(const uint32_t* data, const std::vector<int64_t>& shape,
        size_t& offset, ExceptionSink* xsink);

    //! Reshape flat uint16 data (converted to int64 for Qore)
    DLLLOCAL QoreValue reshapeOutputUInt16(const uint16_t* data, const std::vector<int64_t>& shape,
        size_t& offset, ExceptionSink* xsink);

    //! Reshape flat uint8 data (converted to int64 for Qore)
    DLLLOCAL QoreValue reshapeOutputUInt8(const uint8_t* data, const std::vector<int64_t>& shape,
        size_t& offset, ExceptionSink* xsink);

    //! Reshape flat int64 data
    DLLLOCAL QoreValue reshapeOutputInt64(const int64_t* data, const std::vector<int64_t>& shape,
        size_t& offset);

    //! Reshape flat uint64 data
    DLLLOCAL QoreValue reshapeOutputUInt64(const uint64_t* data, const std::vector<int64_t>& shape,
        size_t& offset, ExceptionSink* xsink);

    //! Reshape flat bool data
    DLLLOCAL QoreValue reshapeOutputBool(const bool* data, const std::vector<int64_t>& shape,
        size_t& offset, ExceptionSink* xsink);

    //! Reshape flat string data
    DLLLOCAL QoreValue reshapeOutputString(Ort::Value& tensor, const std::vector<int64_t>& shape,
        size_t& offset, ExceptionSink* xsink);

    //! Infer shape from Qore data (for dynamic shapes)
    DLLLOCAL std::vector<int64_t> inferShape(const QoreValue& val, const TensorMeta& meta,
        ExceptionSink* xsink);
};

//! ONNX Runtime per-run options wrapper
class QoreOnnxRunOptions : public AbstractPrivateData {
public:
    DLLLOCAL QoreOnnxRunOptions(const QoreHashNode* options, ExceptionSink* xsink);

    DLLLOCAL void setTerminate(ExceptionSink* xsink);
    DLLLOCAL void unsetTerminate(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* getInfo(ExceptionSink* xsink) const;

private:
    friend class QoreOnnxModel;
    friend class QoreOnnxIoBinding;

    Ort::RunOptions options;
    std::string run_tag;
    int log_severity_level = -1;
    int log_verbosity_level = -1;
    bool terminated = false;
    std::unordered_map<std::string, std::string> config_entries;
};

//! ONNX Runtime reusable I/O binding wrapper
class QoreOnnxIoBinding : public AbstractPrivateData {
public:
    DLLLOCAL QoreOnnxIoBinding(QoreOnnxModel* model, ExceptionSink* xsink);
    DLLLOCAL virtual ~QoreOnnxIoBinding();

    DLLLOCAL void bindInput(const char* name, QoreValue value, ExceptionSink* xsink);
    DLLLOCAL void bindInputs(const QoreHashNode* inputs, ExceptionSink* xsink);
    DLLLOCAL void bindOutput(const char* name, ExceptionSink* xsink);
    DLLLOCAL void bindOutputs(ExceptionSink* xsink);
    DLLLOCAL void bindOutputDevice(const char* name, const QoreHashNode* device, ExceptionSink* xsink);
    DLLLOCAL void bindOutputsDevice(const QoreHashNode* device, ExceptionSink* xsink);
    DLLLOCAL void bindOutputTensor(const char* name, const QoreObject* tensor_obj,
        ExceptionSink* xsink);
    DLLLOCAL void clearInputs();
    DLLLOCAL void clearOutputs();
    DLLLOCAL QoreHashNode* run(const QoreObject* options_obj, bool return_tensors,
        ExceptionSink* xsink);
    DLLLOCAL QoreListNode* getBoundInputNames(ExceptionSink* xsink) const;
    DLLLOCAL QoreListNode* getBoundOutputNames(ExceptionSink* xsink) const;

private:
    mutable std::mutex mutex;
    QoreOnnxModel* model = nullptr;
    std::unique_ptr<Ort::IoBinding> binding;
    std::vector<std::unique_ptr<OnnxBoundOrtValue>> bound_inputs;
    std::vector<std::unique_ptr<OnnxBoundOrtValue>> bound_outputs;
    std::vector<std::string> bound_input_names;
    std::vector<std::string> bound_output_names;

    DLLLOCAL void bindInputUnlocked(const char* name, QoreValue value, ExceptionSink* xsink);
    DLLLOCAL void bindOutputUnlocked(const char* name, ExceptionSink* xsink);
    DLLLOCAL void bindOutputDeviceUnlocked(const char* name, const QoreHashNode* device,
        ExceptionSink* xsink);
    DLLLOCAL void bindOutputTensorUnlocked(const char* name, const QoreObject* tensor_obj,
        ExceptionSink* xsink);
    DLLLOCAL void clearInputsUnlocked();
    DLLLOCAL void clearOutputsUnlocked();
    DLLLOCAL QoreOnnxRunOptions* getRunOptions(const QoreObject* options_obj,
        ExceptionSink* xsink) const;
};

//! Bounded pool of immutable ONNX Runtime sessions
class QoreOnnxSessionPool : public AbstractPrivateData {
public:
    DLLLOCAL QoreOnnxSessionPool(const char* model_path, const QoreHashNode* session_config,
        const QoreHashNode* pool_options, ExceptionSink* xsink);
    DLLLOCAL QoreOnnxSessionPool(const void* model_data, size_t model_data_len,
        const QoreHashNode* session_config, const QoreHashNode* pool_options,
        ExceptionSink* xsink);
    DLLLOCAL virtual ~QoreOnnxSessionPool();

    DLLLOCAL QoreHashNode* run(const QoreHashNode* inputs, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* run(const QoreHashNode* inputs, const QoreListNode* output_names,
        ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* runTensors(const QoreHashNode* inputs, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* runTensors(const QoreHashNode* inputs, const QoreListNode* output_names,
        ExceptionSink* xsink);
    DLLLOCAL QoreListNode* runBatch(const QoreListNode* batch, ExceptionSink* xsink);
    DLLLOCAL QoreListNode* runBatch(const QoreListNode* batch, const QoreListNode* output_names,
        ExceptionSink* xsink);
    DLLLOCAL QoreObject* runAsync(const QoreHashNode* inputs, QoreProgram* pgm, ExceptionSink* xsink);
    DLLLOCAL QoreObject* runAsync(const QoreHashNode* inputs, const QoreListNode* output_names,
        QoreProgram* pgm, ExceptionSink* xsink);
    DLLLOCAL QoreObject* runTensorsAsync(const QoreHashNode* inputs, QoreProgram* pgm, ExceptionSink* xsink);
    DLLLOCAL QoreObject* runTensorsAsync(const QoreHashNode* inputs, const QoreListNode* output_names,
        QoreProgram* pgm, ExceptionSink* xsink);
    DLLLOCAL QoreObject* runBatchAsync(const QoreListNode* batch, QoreProgram* pgm, ExceptionSink* xsink);
    DLLLOCAL QoreObject* runBatchAsync(const QoreListNode* batch, const QoreListNode* output_names,
        QoreProgram* pgm, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* getPoolStats(ExceptionSink* xsink) const;
    DLLLOCAL void resetPoolStats();
    DLLLOCAL QoreHashNode* getModelInfo(ExceptionSink* xsink);
    DLLLOCAL QoreListNode* getInputInfo(ExceptionSink* xsink);
    DLLLOCAL QoreListNode* getOutputInfo(ExceptionSink* xsink);

private:
    struct Lease {
        DLLLOCAL Lease(QoreOnnxSessionPool& pool, size_t index) : pool(pool), index(index) {}
        DLLLOCAL ~Lease();

        QoreOnnxSessionPool& pool;
        size_t index;
    };

    std::vector<std::unique_ptr<QoreOnnxModel>> sessions;
    std::vector<bool> in_use;

    mutable std::mutex mutex;
    std::condition_variable cv;

    int64_t max_sessions = 1;
    int64_t max_concurrent_runs = 1;
    int64_t queue_depth = 64;
    int64_t timeout_ms = 0;
    int64_t dynamic_batch_size = 0;
    int64_t dynamic_batch_wait_ms = 0;

    uint64_t total_runs = 0;
    uint64_t total_batches = 0;
    uint64_t total_batch_items = 0;
    uint64_t total_waits = 0;
    uint64_t total_timeouts = 0;
    uint64_t total_rejections = 0;
    uint64_t active_runs = 0;
    uint64_t max_active_runs_observed = 0;
    uint64_t waiting_callers = 0;
    uint64_t max_waiting_callers_observed = 0;

    enum class AsyncOp {
        Run,
        RunTensors,
        RunBatch,
    };

    struct AsyncRequest {
        DLLLOCAL AsyncRequest(const QoreHashNode* inputs, const QoreListNode* output_names,
            QorePromise* promise);
        DLLLOCAL ~AsyncRequest();

        QoreValue payload;
        QoreValue output_names;
        std::string output_signature;
        QorePromise* promise;
    };

    struct SingleAsyncParams {
        DLLLOCAL SingleAsyncParams(QoreOnnxSessionPool* pool, QoreValue payload,
            QoreValue output_names, QorePromise* promise, AsyncOp op);
        DLLLOCAL ~SingleAsyncParams();

        QoreOnnxSessionPool* pool;
        QoreValue payload;
        QoreValue output_names;
        QorePromise* promise;
        AsyncOp op;
    };

    mutable std::mutex async_mutex;
    std::condition_variable async_cv;
    std::deque<std::unique_ptr<AsyncRequest>> async_requests;

    std::atomic<uint64_t> async_submitted{0};
    std::atomic<uint64_t> async_completed{0};
    std::atomic<uint64_t> async_errors{0};
    std::atomic<uint64_t> async_batches{0};
    std::atomic<uint64_t> async_batch_items{0};

    DLLLOCAL void parsePoolOptions(const QoreHashNode* pool_options, ExceptionSink* xsink);
    DLLLOCAL std::unique_ptr<Lease> acquire(ExceptionSink* xsink);
    DLLLOCAL void release(size_t index);
    DLLLOCAL int64_t getPositiveOption(const QoreHashNode* options, const char* name,
        int64_t current, bool allow_zero, ExceptionSink* xsink);
    DLLLOCAL void validateReady(ExceptionSink* xsink) const;
    DLLLOCAL size_t findAvailableUnlocked() const;
    DLLLOCAL void recordRun(size_t batch_items);
    DLLLOCAL QoreObject* submitSingleAsync(QoreValue payload, QoreValue output_names, AsyncOp op,
        const QoreTypeInfo* future_type, QoreProgram* pgm, ExceptionSink* xsink);
    DLLLOCAL QoreObject* submitDynamicRunAsync(const QoreHashNode* inputs,
        const QoreListNode* output_names, QoreProgram* pgm, ExceptionSink* xsink);
    DLLLOCAL void processDynamicRunQueue(ExceptionSink* xsink);
    DLLLOCAL void resolveAsyncRequest(AsyncRequest& request, QoreValue value, ExceptionSink* xsink);
    DLLLOCAL void rejectAsyncBatch(std::vector<std::unique_ptr<AsyncRequest>>& batch, ExceptionSink& err);

    DLLLOCAL static void singleAsyncThread(ExceptionSink* xsink, void* arg);
    DLLLOCAL static void dynamicRunAsyncThread(ExceptionSink* xsink, void* arg);
};

#else // !HAVE_ONNXRUNTIME

//! Stub class — always defined so the OnnxModel Qore class compiles without ONNX Runtime.
//! Constructors throw MISSING-FEATURE-ERROR at the Qore level; these methods are never called.
class QoreOnnxModel : public AbstractPrivateData {
public:
    DLLLOCAL QoreHashNode* run(const QoreHashNode*, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreHashNode* run(const QoreHashNode*, const QoreListNode*, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreHashNode* runTensors(const QoreHashNode*, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreHashNode* runTensors(const QoreHashNode*, const QoreListNode*, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreObject* createBinding(QoreProgram*, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreHashNode* runBound(const QoreObject*, const QoreObject*, bool, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreListNode* runBatch(const QoreListNode*, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreListNode* runBatch(const QoreListNode*, const QoreListNode*, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreHashNode* getModelInfo(ExceptionSink*) const { return nullptr; }
    DLLLOCAL QoreListNode* getInputInfo(ExceptionSink*) const { return nullptr; }
    DLLLOCAL QoreListNode* getOutputInfo(ExceptionSink*) const { return nullptr; }
    DLLLOCAL bool isLoaded() const { return false; }
    DLLLOCAL const std::string& getActiveProvider() const {
        static std::string empty;
        return empty;
    }
    DLLLOCAL QoreListNode* getProviders(ExceptionSink*) const { return nullptr; }
    DLLLOCAL QoreHashNode* getProviderOptions(ExceptionSink*) const { return nullptr; }
    DLLLOCAL QoreListNode* getRequestedProviders(ExceptionSink*) const { return nullptr; }
    DLLLOCAL QoreListNode* getProviderDiagnostics(ExceptionSink*) const { return nullptr; }
    DLLLOCAL QoreHashNode* getEffectiveProviderReport(ExceptionSink*) const { return nullptr; }
};

class QoreOnnxRunOptions : public AbstractPrivateData {
public:
    DLLLOCAL QoreOnnxRunOptions(const QoreHashNode*, ExceptionSink*) {}
    DLLLOCAL void setTerminate(ExceptionSink*) {}
    DLLLOCAL void unsetTerminate(ExceptionSink*) {}
    DLLLOCAL QoreHashNode* getInfo(ExceptionSink*) const { return nullptr; }
};

class QoreOnnxIoBinding : public AbstractPrivateData {
public:
    DLLLOCAL void bindInput(const char*, QoreValue, ExceptionSink*) {}
    DLLLOCAL void bindInputs(const QoreHashNode*, ExceptionSink*) {}
    DLLLOCAL void bindOutput(const char*, ExceptionSink*) {}
    DLLLOCAL void bindOutputs(ExceptionSink*) {}
    DLLLOCAL void bindOutputTensor(const char*, const QoreObject*, ExceptionSink*) {}
    DLLLOCAL void clearInputs() {}
    DLLLOCAL void clearOutputs() {}
    DLLLOCAL QoreHashNode* run(const QoreObject*, bool, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreListNode* getBoundInputNames(ExceptionSink*) const { return nullptr; }
    DLLLOCAL QoreListNode* getBoundOutputNames(ExceptionSink*) const { return nullptr; }
};

class QoreOnnxSessionPool : public AbstractPrivateData {
public:
    DLLLOCAL QoreHashNode* run(const QoreHashNode*, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreHashNode* run(const QoreHashNode*, const QoreListNode*, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreHashNode* runTensors(const QoreHashNode*, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreHashNode* runTensors(const QoreHashNode*, const QoreListNode*, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreListNode* runBatch(const QoreListNode*, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreListNode* runBatch(const QoreListNode*, const QoreListNode*, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreObject* runAsync(const QoreHashNode*, QoreProgram*, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreObject* runAsync(const QoreHashNode*, const QoreListNode*, QoreProgram*, ExceptionSink*) {
        return nullptr;
    }
    DLLLOCAL QoreObject* runTensorsAsync(const QoreHashNode*, QoreProgram*, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreObject* runTensorsAsync(const QoreHashNode*, const QoreListNode*, QoreProgram*, ExceptionSink*) {
        return nullptr;
    }
    DLLLOCAL QoreObject* runBatchAsync(const QoreListNode*, QoreProgram*, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreObject* runBatchAsync(const QoreListNode*, const QoreListNode*, QoreProgram*, ExceptionSink*) {
        return nullptr;
    }
    DLLLOCAL QoreHashNode* getPoolStats(ExceptionSink*) const { return nullptr; }
    DLLLOCAL void resetPoolStats() {}
    DLLLOCAL QoreHashNode* getModelInfo(ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreListNode* getInputInfo(ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreListNode* getOutputInfo(ExceptionSink*) { return nullptr; }
};

#endif // HAVE_ONNXRUNTIME

DLLLOCAL QoreObject* qore_ml_onnx_run_options_to_object(QoreOnnxRunOptions* options,
    QoreProgram* pgm, ExceptionSink* xsink);
DLLLOCAL QoreObject* qore_ml_onnx_io_binding_to_object(QoreOnnxIoBinding* binding,
    QoreProgram* pgm, ExceptionSink* xsink);

#endif // _QORE_MODULE_ML_QC_ONNXMODEL_H
