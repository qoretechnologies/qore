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

//! Input/output tensor metadata
struct TensorMeta {
    std::string name;
    ONNXTensorElementDataType element_type;
    std::vector<int64_t> shape;
};

//! ONNX Model wrapper class
class QoreOnnxModel : public AbstractPrivateData {
public:
    //! Constructor — loads the ONNX model from the given file path
    DLLLOCAL QoreOnnxModel(const char* model_path, ExceptionSink* xsink);

    //! Constructor — loads the ONNX model with session configuration
    DLLLOCAL QoreOnnxModel(const char* model_path, const QoreHashNode* config, ExceptionSink* xsink);

    //! Returns the active execution provider name
    DLLLOCAL const std::string& getActiveProvider() const { return active_provider; }

    //! Run inference with named input tensors
    /** @param inputs hash where keys are tensor names, values are data (scalars, lists, or nested lists)
        @param xsink exception sink
        @return hash of output tensors (keys = output names, values = data)
    */
    DLLLOCAL QoreHashNode* run(const QoreHashNode* inputs, ExceptionSink* xsink);

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

    std::string active_provider;

    //! Configure session options from a config hash
    DLLLOCAL void configureSession(Ort::SessionOptions& opts, const QoreHashNode* config,
        ExceptionSink* xsink);

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

    //! Flatten a nested Qore list into a flat vector of int64_t
    DLLLOCAL void flattenToInt64(const QoreValue& val, std::vector<int64_t>& out,
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

    //! Reshape flat int64 data
    DLLLOCAL QoreValue reshapeOutputInt64(const int64_t* data, const std::vector<int64_t>& shape,
        size_t& offset);

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
    DLLLOCAL QoreListNode* runBatch(const QoreListNode*, ExceptionSink*) { return nullptr; }
    DLLLOCAL QoreHashNode* getModelInfo(ExceptionSink*) const { return nullptr; }
    DLLLOCAL QoreListNode* getInputInfo(ExceptionSink*) const { return nullptr; }
    DLLLOCAL QoreListNode* getOutputInfo(ExceptionSink*) const { return nullptr; }
    DLLLOCAL bool isLoaded() const { return false; }
    DLLLOCAL const std::string& getActiveProvider() const {
        static std::string empty;
        return empty;
    }
};

#endif // HAVE_ONNXRUNTIME

#endif // _QORE_MODULE_ML_QC_ONNXMODEL_H
