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

#include "QC_Tensor.h"

#include <vector>
#include <string>
#include <memory>

#ifdef HAVE_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#include <unordered_map>
#endif

DLLEXPORT extern qore_classid_t CID_ONNXMODEL;
DLLLOCAL extern QoreClass* QC_ONNXMODEL;

DLLLOCAL void preinitOnnxModelClass();
DLLLOCAL QoreClass* initOnnxModelClass(QoreNamespace& ns);

#ifdef HAVE_ONNXRUNTIME

//! Returns available ONNX Runtime providers as a Qore list
DLLLOCAL QoreListNode* qore_ml_get_onnx_providers(ExceptionSink* xsink);

//! Returns known ONNX Runtime provider option metadata
DLLLOCAL QoreHashNode* qore_ml_get_onnx_provider_options(ExceptionSink* xsink);

//! Input/output tensor metadata
struct TensorMeta {
    std::string name;
    ONNXTensorElementDataType element_type;
    std::vector<int64_t> shape;
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

    //! Run inference and return outputs as ML::Tensor objects
    DLLLOCAL QoreHashNode* runTensors(const QoreHashNode* inputs, ExceptionSink* xsink);

    //! Run batch inference
    /** @param batch list of input hashes
        @param xsink exception sink
        @return list of output hashes
    */
    DLLLOCAL QoreListNode* runBatch(const QoreListNode* batch, ExceptionSink* xsink);

    //! Get model info
    DLLLOCAL QoreHashNode* getModelInfo(ExceptionSink* xsink) const;

    //! Get input tensor info
    DLLLOCAL QoreListNode* getInputInfo(ExceptionSink* xsink) const;

    //! Get output tensor info
    DLLLOCAL QoreListNode* getOutputInfo(ExceptionSink* xsink) const;

    //! Whether the model is loaded
    DLLLOCAL bool isLoaded() const { return session != nullptr; }

private:
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::Session> session;
    Ort::AllocatorWithDefaultOptions allocator;

    std::vector<TensorMeta> input_meta;
    std::vector<TensorMeta> output_meta;

    std::vector<std::string> available_providers;
    std::vector<std::string> requested_providers;
    std::vector<std::string> required_providers;
    std::vector<OnnxProviderDiagnostic> provider_diagnostics;

    std::string active_provider;
    bool auto_provider_selected = false;
    bool explicit_provider_config = false;
    bool allow_cpu_fallback = true;
    bool fail_on_provider_fallback = false;
    bool cpu_fallback_used = false;

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
    DLLLOCAL QoreValue convertOutputTensorToTensor(Ort::Value& tensor, ExceptionSink* xsink);

    //! Shared inference implementation
    DLLLOCAL QoreHashNode* runImpl(const QoreHashNode* inputs, bool return_tensors, ExceptionSink* xsink);

    //! Build a QoreHashNode with OnnxTensorInfo from a TensorMeta
    DLLLOCAL QoreHashNode* tensorMetaToHash(const TensorMeta& meta, ExceptionSink* xsink) const;

    //! Flatten a nested Qore list into a flat vector of floats
    DLLLOCAL void flattenToFloats(const QoreValue& val, std::vector<float>& out,
        ExceptionSink* xsink);

    //! Flatten a nested Qore list into a flat vector of doubles
    DLLLOCAL void flattenToDoubles(const QoreValue& val, std::vector<double>& out,
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

    //! Flatten a nested Qore list into a flat vector of int64_t
    DLLLOCAL void flattenToInt64(const QoreValue& val, std::vector<int64_t>& out,
        ExceptionSink* xsink);

    //! Flatten a nested Qore list into a flat vector of bool bytes
    DLLLOCAL void flattenToBools(const QoreValue& val, std::vector<uint8_t>& out,
        ExceptionSink* xsink);

    //! Reshape a flat float vector into a nested Qore list based on shape
    DLLLOCAL QoreValue reshapeOutput(const float* data, const std::vector<int64_t>& shape,
        size_t& offset);

    //! Reshape a flat double vector into a nested Qore list
    DLLLOCAL QoreValue reshapeOutput(const double* data, const std::vector<int64_t>& shape,
        size_t& offset);

    //! Reshape flat int32 data (converted to int64 for Qore)
    DLLLOCAL QoreValue reshapeOutputInt32(const int32_t* data, const std::vector<int64_t>& shape,
        size_t& offset);

    //! Reshape flat int16 data (converted to int64 for Qore)
    DLLLOCAL QoreValue reshapeOutputInt16(const int16_t* data, const std::vector<int64_t>& shape,
        size_t& offset, ExceptionSink* xsink);

    //! Reshape flat int8 data (converted to int64 for Qore)
    DLLLOCAL QoreValue reshapeOutputInt8(const int8_t* data, const std::vector<int64_t>& shape,
        size_t& offset, ExceptionSink* xsink);

    //! Reshape flat int64 data
    DLLLOCAL QoreValue reshapeOutputInt64(const int64_t* data, const std::vector<int64_t>& shape,
        size_t& offset);

    //! Reshape flat bool data
    DLLLOCAL QoreValue reshapeOutputBool(const bool* data, const std::vector<int64_t>& shape,
        size_t& offset, ExceptionSink* xsink);

    //! Infer shape from Qore data (for dynamic shapes)
    DLLLOCAL std::vector<int64_t> inferShape(const QoreValue& val, const TensorMeta& meta,
        ExceptionSink* xsink);
};

#else // !HAVE_ONNXRUNTIME

//! Stub class — always defined so the OnnxModel Qore class compiles without ONNX Runtime.
//! Constructors throw MISSING-FEATURE-ERROR at the Qore level; these methods are never called.
class QoreOnnxModel : public AbstractPrivateData {
public:
    DLLLOCAL QoreHashNode* run(const QoreHashNode*, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreHashNode* runTensors(const QoreHashNode*, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreListNode* runBatch(const QoreListNode*, ExceptionSink*) { return nullptr; }
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

#endif // HAVE_ONNXRUNTIME

#endif // _QORE_MODULE_ML_QC_ONNXMODEL_H
