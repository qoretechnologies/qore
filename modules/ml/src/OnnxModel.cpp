/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    OnnxModel.cpp

    Qore ml module - OnnxModel implementation

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

#ifdef HAVE_ONNXRUNTIME

#include "QC_OnnxModel.h"

#include <algorithm>
#include <numeric>
#include <cstring>

// Hashdecl extern declarations
extern const TypedHashDecl* hashdeclOnnxTensorInfo;
extern const TypedHashDecl* hashdeclOnnxModelInfo;
extern const TypedHashDecl* hashdeclOnnxProviderConfig;
extern const TypedHashDecl* hashdeclOnnxSessionConfig;

QoreOnnxModel::QoreOnnxModel(const char* model_path, ExceptionSink* xsink)
    : active_provider("CPUExecutionProvider") {
    try {
        env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "qore-ml");
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        autoDetectProvider(session_options);
        session = std::make_unique<Ort::Session>(*env, model_path, session_options);
        loadMetadata(xsink);
    } catch (const Ort::Exception& e) {
        xsink->raiseException("ML-ONNX-ERROR", "failed to load ONNX model '%s': %s",
            model_path, e.what());
    }
}

QoreOnnxModel::QoreOnnxModel(const char* model_path, const QoreHashNode* config,
    ExceptionSink* xsink) : active_provider("CPUExecutionProvider") {
    try {
        // Read log severity from config (default: WARNING = 2)
        OrtLoggingLevel log_level = ORT_LOGGING_LEVEL_WARNING;
        QoreValue log_sev_val = config->getKeyValue("log_severity");
        if (!log_sev_val.isNullOrNothing()) {
            int64_t log_sev = log_sev_val.getAsBigInt();
            if (log_sev < 0 || log_sev > 4) {
                xsink->raiseException("ML-ONNX-ERROR",
                    "invalid log_severity %lld; must be 0-4 (0=verbose, 1=info, 2=warning, "
                    "3=error, 4=fatal)", (long long)log_sev);
                return;
            }
            log_level = static_cast<OrtLoggingLevel>(log_sev);
        }

        env = std::make_unique<Ort::Env>(log_level, "qore-ml");
        Ort::SessionOptions session_options;

        configureSession(session_options, config, xsink);
        if (*xsink) {
            return;
        }

        session = std::make_unique<Ort::Session>(*env, model_path, session_options);
        loadMetadata(xsink);
    } catch (const Ort::Exception& e) {
        xsink->raiseException("ML-ONNX-ERROR", "failed to load ONNX model '%s': %s",
            model_path, e.what());
    }
}

void QoreOnnxModel::configureSession(Ort::SessionOptions& opts, const QoreHashNode* config,
    ExceptionSink* xsink) {
    // intra_op_threads (default: 1)
    QoreValue intra_val = config->getKeyValue("intra_op_threads");
    if (!intra_val.isNullOrNothing()) {
        int64_t intra = intra_val.getAsBigInt();
        if (intra < 1) {
            xsink->raiseException("ML-ONNX-ERROR",
                "invalid intra_op_threads %lld; must be >= 1", (long long)intra);
            return;
        }
        opts.SetIntraOpNumThreads((int)intra);
    } else {
        opts.SetIntraOpNumThreads(1);
    }

    // inter_op_threads (default: 0 = auto)
    QoreValue inter_val = config->getKeyValue("inter_op_threads");
    if (!inter_val.isNullOrNothing()) {
        int64_t inter = inter_val.getAsBigInt();
        if (inter < 0) {
            xsink->raiseException("ML-ONNX-ERROR",
                "invalid inter_op_threads %lld; must be >= 0", (long long)inter);
            return;
        }
        opts.SetInterOpNumThreads((int)inter);
    }

    // graph_optimization_level (default: 99 = all)
    QoreValue opt_val = config->getKeyValue("graph_optimization_level");
    if (!opt_val.isNullOrNothing()) {
        int64_t opt_level = opt_val.getAsBigInt();
        GraphOptimizationLevel ort_level;
        switch (opt_level) {
            case 0:
                ort_level = ORT_DISABLE_ALL;
                break;
            case 1:
                ort_level = ORT_ENABLE_BASIC;
                break;
            case 2:
                ort_level = ORT_ENABLE_EXTENDED;
                break;
            case 99:
                ort_level = ORT_ENABLE_ALL;
                break;
            default:
                xsink->raiseException("ML-ONNX-ERROR",
                    "invalid graph_optimization_level %lld; must be 0 (disabled), 1 (basic), "
                    "2 (extended), or 99 (all)", (long long)opt_level);
                return;
        }
        opts.SetGraphOptimizationLevel(ort_level);
    } else {
        opts.SetGraphOptimizationLevel(ORT_ENABLE_ALL);
    }

    // log_severity on session options
    QoreValue log_sev_val = config->getKeyValue("log_severity");
    if (!log_sev_val.isNullOrNothing()) {
        opts.SetLogSeverityLevel((int)log_sev_val.getAsBigInt());
    }

    // providers (list of OnnxProviderConfig)
    QoreValue providers_val = config->getKeyValue("providers");
    if (!providers_val.isNullOrNothing() && providers_val.getType() == NT_LIST) {
        const QoreListNode* providers = providers_val.get<const QoreListNode>();
        for (size_t i = 0; i < providers->size(); ++i) {
            QoreValue pval = providers->retrieveEntry(i);
            if (pval.getType() != NT_HASH) {
                continue;
            }
            const QoreHashNode* pconfig = pval.get<const QoreHashNode>();
            QoreValue name_val = pconfig->getKeyValue("name");
            if (name_val.isNullOrNothing()) {
                continue;
            }
            QoreStringValueHelper name_str(name_val);
            std::string provider_name(name_str->c_str());

            // Parse options hash
            std::unordered_map<std::string, std::string> provider_opts;
            QoreValue opts_val = pconfig->getKeyValue("options");
            if (!opts_val.isNullOrNothing() && opts_val.getType() == NT_HASH) {
                const QoreHashNode* opts_hash = opts_val.get<const QoreHashNode>();
                ConstHashIterator hi(opts_hash);
                while (hi.next()) {
                    QoreStringValueHelper key_str(hi.get());
                    provider_opts[hi.getKey()] = key_str->c_str();
                }
            }

            appendProvider(opts, provider_name, provider_opts, xsink);
            if (*xsink) {
                return;
            }
        }
    } else {
        // No explicit providers configured — auto-detect best available GPU provider
        autoDetectProvider(opts);
    }
}

void QoreOnnxModel::autoDetectProvider(Ort::SessionOptions& opts) {
    std::vector<std::string> available = Ort::GetAvailableProviders();

    // Priority order: CUDA (Linux), CoreML (macOS), then CPU fallback
    static const std::vector<std::string> preferred = {
        "CUDAExecutionProvider",
        "CoreMLExecutionProvider",
    };

    for (const auto& pref : preferred) {
        if (std::find(available.begin(), available.end(), pref) != available.end()) {
            try {
                std::unordered_map<std::string, std::string> empty_opts;
                ExceptionSink xsink;
                appendProvider(opts, pref, empty_opts, &xsink);
                if (!xsink) {
                    return;
                }
                // Provider failed to initialize — try the next one
                active_provider = "CPUExecutionProvider";
            } catch (...) {
                active_provider = "CPUExecutionProvider";
            }
        }
    }
    // Fall through to CPU (always available, no action needed)
}

void QoreOnnxModel::appendProvider(Ort::SessionOptions& opts, const std::string& name,
    const std::unordered_map<std::string, std::string>& provider_opts,
    ExceptionSink* xsink) {
    // Normalize name: strip "ExecutionProvider" suffix if present for matching
    std::string normalized = name;

    // CPU is always a fallback; no action needed
    if (normalized == "CPU" || normalized == "CPUExecutionProvider") {
        active_provider = "CPUExecutionProvider";
        return;
    }

    try {
        if (normalized == "CUDA" || normalized == "CUDAExecutionProvider") {
            // Use V2 API for CUDA
            OrtCUDAProviderOptionsV2* cuda_opts = nullptr;
            Ort::ThrowOnError(Ort::GetApi().CreateCUDAProviderOptions(&cuda_opts));
            // RAII guard for cleanup
            auto cuda_deleter = [](OrtCUDAProviderOptionsV2* p) {
                Ort::GetApi().ReleaseCUDAProviderOptions(p);
            };
            std::unique_ptr<OrtCUDAProviderOptionsV2, decltype(cuda_deleter)>
                cuda_guard(cuda_opts, cuda_deleter);

            if (!provider_opts.empty()) {
                std::vector<const char*> keys;
                std::vector<const char*> values;
                for (const auto& kv : provider_opts) {
                    keys.push_back(kv.first.c_str());
                    values.push_back(kv.second.c_str());
                }
                OrtStatus* status = Ort::GetApi().UpdateCUDAProviderOptions(
                    cuda_opts, keys.data(), values.data(), keys.size());
                if (status) {
                    std::string msg = Ort::GetApi().GetErrorMessage(status);
                    Ort::GetApi().ReleaseStatus(status);
                    xsink->raiseException("ML-ONNX-PROVIDER-ERROR",
                        "failed to configure CUDA provider: %s", msg.c_str());
                    return;
                }
            }

            OrtStatus* status = Ort::GetApi().SessionOptionsAppendExecutionProvider_CUDA_V2(
                opts, cuda_opts);
            if (status) {
                std::string msg = Ort::GetApi().GetErrorMessage(status);
                Ort::GetApi().ReleaseStatus(status);
                xsink->raiseException("ML-ONNX-PROVIDER-ERROR",
                    "failed to append CUDA provider: %s", msg.c_str());
                return;
            }
            active_provider = "CUDAExecutionProvider";
        } else if (normalized == "TensorRT" || normalized == "TensorrtExecutionProvider"
                || normalized == "TensorRTExecutionProvider") {
            // Use V2 API for TensorRT
            OrtTensorRTProviderOptionsV2* trt_opts = nullptr;
            Ort::ThrowOnError(Ort::GetApi().CreateTensorRTProviderOptions(&trt_opts));
            auto trt_deleter = [](OrtTensorRTProviderOptionsV2* p) {
                Ort::GetApi().ReleaseTensorRTProviderOptions(p);
            };
            std::unique_ptr<OrtTensorRTProviderOptionsV2, decltype(trt_deleter)>
                trt_guard(trt_opts, trt_deleter);

            if (!provider_opts.empty()) {
                std::vector<const char*> keys;
                std::vector<const char*> values;
                for (const auto& kv : provider_opts) {
                    keys.push_back(kv.first.c_str());
                    values.push_back(kv.second.c_str());
                }
                OrtStatus* status = Ort::GetApi().UpdateTensorRTProviderOptions(
                    trt_opts, keys.data(), values.data(), keys.size());
                if (status) {
                    std::string msg = Ort::GetApi().GetErrorMessage(status);
                    Ort::GetApi().ReleaseStatus(status);
                    xsink->raiseException("ML-ONNX-PROVIDER-ERROR",
                        "failed to configure TensorRT provider: %s", msg.c_str());
                    return;
                }
            }

            OrtStatus* status = Ort::GetApi().SessionOptionsAppendExecutionProvider_TensorRT_V2(
                opts, trt_opts);
            if (status) {
                std::string msg = Ort::GetApi().GetErrorMessage(status);
                Ort::GetApi().ReleaseStatus(status);
                xsink->raiseException("ML-ONNX-PROVIDER-ERROR",
                    "failed to append TensorRT provider: %s", msg.c_str());
                return;
            }
            active_provider = "TensorrtExecutionProvider";
        } else {
            // Generic API for all other providers
            opts.AppendExecutionProvider(normalized, provider_opts);
            active_provider = normalized;
        }
    } catch (const Ort::Exception& e) {
        xsink->raiseException("ML-ONNX-PROVIDER-ERROR",
            "failed to append execution provider '%s': %s", name.c_str(), e.what());
    }
}

void QoreOnnxModel::loadMetadata(ExceptionSink* xsink) {
    // Load input metadata
    size_t num_inputs = session->GetInputCount();
    input_meta.resize(num_inputs);
    for (size_t i = 0; i < num_inputs; ++i) {
        Ort::AllocatedStringPtr name_ptr = session->GetInputNameAllocated(i, allocator);
        input_meta[i].name = name_ptr.get();

        Ort::TypeInfo type_info = session->GetInputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        input_meta[i].element_type = tensor_info.GetElementType();
        input_meta[i].shape = tensor_info.GetShape();
    }

    // Load output metadata
    size_t num_outputs = session->GetOutputCount();
    output_meta.resize(num_outputs);
    for (size_t i = 0; i < num_outputs; ++i) {
        Ort::AllocatedStringPtr name_ptr = session->GetOutputNameAllocated(i, allocator);
        output_meta[i].name = name_ptr.get();

        Ort::TypeInfo type_info = session->GetOutputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        output_meta[i].element_type = tensor_info.GetElementType();
        output_meta[i].shape = tensor_info.GetShape();
    }
}

const char* QoreOnnxModel::elementTypeToString(ONNXTensorElementDataType type) {
    switch (type) {
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: return "float";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE: return "double";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8: return "int8";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16: return "int16";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: return "int32";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: return "int64";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: return "uint8";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16: return "uint16";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32: return "uint32";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64: return "uint64";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL: return "bool";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING: return "string";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16: return "float16";
        default: return "unknown";
    }
}

QoreHashNode* QoreOnnxModel::tensorMetaToHash(const TensorMeta& meta,
        ExceptionSink* xsink) const {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclOnnxTensorInfo, xsink), xsink);
    rv->setKeyValue("name", new QoreStringNode(meta.name), xsink);
    rv->setKeyValue("type", new QoreStringNode(elementTypeToString(meta.element_type)), xsink);

    ReferenceHolder<QoreListNode> shape_list(new QoreListNode(bigIntTypeInfo), xsink);
    for (int64_t dim : meta.shape) {
        shape_list->push(dim, xsink);
    }
    rv->setKeyValue("shape", shape_list.release(), xsink);
    return rv.release();
}

QoreListNode* QoreOnnxModel::getInputInfo(ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(hashdeclOnnxTensorInfo->getTypeInfo()),
        xsink);
    for (const auto& meta : input_meta) {
        rv->push(tensorMetaToHash(meta, xsink), xsink);
    }
    return rv.release();
}

QoreListNode* QoreOnnxModel::getOutputInfo(ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(hashdeclOnnxTensorInfo->getTypeInfo()),
        xsink);
    for (const auto& meta : output_meta) {
        rv->push(tensorMetaToHash(meta, xsink), xsink);
    }
    return rv.release();
}

QoreHashNode* QoreOnnxModel::getModelInfo(ExceptionSink* xsink) const {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclOnnxModelInfo, xsink), xsink);

    Ort::ModelMetadata metadata = session->GetModelMetadata();

    Ort::AllocatedStringPtr producer = metadata.GetProducerNameAllocated(allocator);
    rv->setKeyValue("producer_name", new QoreStringNode(producer.get()), xsink);

    Ort::AllocatedStringPtr desc = metadata.GetDescriptionAllocated(allocator);
    const char* desc_str = desc.get();
    if (desc_str && desc_str[0]) {
        rv->setKeyValue("description", new QoreStringNode(desc_str), xsink);
    }

    rv->setKeyValue("version", (int64_t)metadata.GetVersion(), xsink);

    // Input info
    rv->setKeyValue("inputs", getInputInfo(xsink), xsink);
    // Output info
    rv->setKeyValue("outputs", getOutputInfo(xsink), xsink);

    // Active execution provider
    if (!active_provider.empty()) {
        rv->setKeyValue("active_provider", new QoreStringNode(active_provider), xsink);
    }

    return rv.release();
}

void QoreOnnxModel::flattenToFloats(const QoreValue& val, std::vector<float>& out,
        ExceptionSink* xsink) {
    if (val.getType() == NT_LIST) {
        const QoreListNode* list = val.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            flattenToFloats(list->retrieveEntry(i), out, xsink);
            if (*xsink) {
                return;
            }
        }
    } else {
        out.push_back((float)val.getAsFloat());
    }
}

void QoreOnnxModel::flattenToDoubles(const QoreValue& val, std::vector<double>& out,
        ExceptionSink* xsink) {
    if (val.getType() == NT_LIST) {
        const QoreListNode* list = val.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            flattenToDoubles(list->retrieveEntry(i), out, xsink);
            if (*xsink) {
                return;
            }
        }
    } else {
        out.push_back(val.getAsFloat());
    }
}

void QoreOnnxModel::flattenToInt32(const QoreValue& val, std::vector<int32_t>& out,
        ExceptionSink* xsink) {
    if (val.getType() == NT_LIST) {
        const QoreListNode* list = val.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            flattenToInt32(list->retrieveEntry(i), out, xsink);
            if (*xsink) {
                return;
            }
        }
    } else {
        out.push_back((int32_t)val.getAsBigInt());
    }
}

void QoreOnnxModel::flattenToInt64(const QoreValue& val, std::vector<int64_t>& out,
        ExceptionSink* xsink) {
    if (val.getType() == NT_LIST) {
        const QoreListNode* list = val.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            flattenToInt64(list->retrieveEntry(i), out, xsink);
            if (*xsink) {
                return;
            }
        }
    } else {
        out.push_back(val.getAsBigInt());
    }
}

std::vector<int64_t> QoreOnnxModel::inferShape(const QoreValue& val, const TensorMeta& meta,
        ExceptionSink* xsink) {
    std::vector<int64_t> shape = meta.shape;

    // Resolve dynamic dimensions (-1) by inspecting the data
    if (val.getType() == NT_LIST) {
        const QoreListNode* list = val.get<const QoreListNode>();
        // First dimension is the list size
        if (!shape.empty() && shape[0] == -1) {
            shape[0] = (int64_t)list->size();
        }
        // For 2D+ tensors, check first element for second dimension
        if (shape.size() >= 2 && shape[1] == -1 && list->size() > 0) {
            QoreValue first = list->retrieveEntry(0);
            if (first.getType() == NT_LIST) {
                shape[1] = (int64_t)first.get<const QoreListNode>()->size();
            }
        }
    } else {
        // Scalar input: shape should be empty or {1}
        if (shape.empty()) {
            shape.push_back(1);
        }
    }

    return shape;
}

QoreValue QoreOnnxModel::reshapeOutput(const float* data, const std::vector<int64_t>& shape,
        size_t& offset) {
    if (shape.empty() || (shape.size() == 1 && shape[0] == 1)) {
        return (double)data[offset++];
    }
    if (shape.size() == 1) {
        ReferenceHolder<QoreListNode> list(new QoreListNode(floatTypeInfo), nullptr);
        for (int64_t i = 0; i < shape[0]; ++i) {
            list->push((double)data[offset++], nullptr);
        }
        return list.release();
    }
    // Multi-dimensional
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), nullptr);
    std::vector<int64_t> inner_shape(shape.begin() + 1, shape.end());
    for (int64_t i = 0; i < shape[0]; ++i) {
        list->push(reshapeOutput(data, inner_shape, offset), nullptr);
    }
    return list.release();
}

QoreValue QoreOnnxModel::reshapeOutput(const double* data, const std::vector<int64_t>& shape,
        size_t& offset) {
    if (shape.empty() || (shape.size() == 1 && shape[0] == 1)) {
        return data[offset++];
    }
    if (shape.size() == 1) {
        ReferenceHolder<QoreListNode> list(new QoreListNode(floatTypeInfo), nullptr);
        for (int64_t i = 0; i < shape[0]; ++i) {
            list->push(data[offset++], nullptr);
        }
        return list.release();
    }
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), nullptr);
    std::vector<int64_t> inner_shape(shape.begin() + 1, shape.end());
    for (int64_t i = 0; i < shape[0]; ++i) {
        list->push(reshapeOutput(data, inner_shape, offset), nullptr);
    }
    return list.release();
}

QoreValue QoreOnnxModel::reshapeOutputInt32(const int32_t* data,
        const std::vector<int64_t>& shape, size_t& offset) {
    if (shape.empty() || (shape.size() == 1 && shape[0] == 1)) {
        return (int64_t)data[offset++];
    }
    if (shape.size() == 1) {
        ReferenceHolder<QoreListNode> list(new QoreListNode(bigIntTypeInfo), nullptr);
        for (int64_t i = 0; i < shape[0]; ++i) {
            list->push((int64_t)data[offset++], nullptr);
        }
        return list.release();
    }
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), nullptr);
    std::vector<int64_t> inner_shape(shape.begin() + 1, shape.end());
    for (int64_t i = 0; i < shape[0]; ++i) {
        list->push(reshapeOutputInt32(data, inner_shape, offset), nullptr);
    }
    return list.release();
}

QoreValue QoreOnnxModel::reshapeOutputInt64(const int64_t* data,
        const std::vector<int64_t>& shape, size_t& offset) {
    if (shape.empty() || (shape.size() == 1 && shape[0] == 1)) {
        return data[offset++];
    }
    if (shape.size() == 1) {
        ReferenceHolder<QoreListNode> list(new QoreListNode(bigIntTypeInfo), nullptr);
        for (int64_t i = 0; i < shape[0]; ++i) {
            list->push(data[offset++], nullptr);
        }
        return list.release();
    }
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), nullptr);
    std::vector<int64_t> inner_shape(shape.begin() + 1, shape.end());
    for (int64_t i = 0; i < shape[0]; ++i) {
        list->push(reshapeOutputInt64(data, inner_shape, offset), nullptr);
    }
    return list.release();
}

QoreValue QoreOnnxModel::convertOutputTensor(Ort::Value& tensor, ExceptionSink* xsink) {
    if (!tensor.IsTensor()) {
        xsink->raiseException("ML-ONNX-INFERENCE-ERROR", "output is not a tensor");
        return QoreValue();
    }

    auto tensor_info = tensor.GetTensorTypeAndShapeInfo();
    ONNXTensorElementDataType type = tensor_info.GetElementType();
    std::vector<int64_t> shape = tensor_info.GetShape();

    size_t offset = 0;
    switch (type) {
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: {
            const float* data = tensor.GetTensorData<float>();
            return reshapeOutput(data, shape, offset);
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE: {
            const double* data = tensor.GetTensorData<double>();
            return reshapeOutput(data, shape, offset);
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: {
            const int64_t* data = tensor.GetTensorData<int64_t>();
            return reshapeOutputInt64(data, shape, offset);
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: {
            const int32_t* data = tensor.GetTensorData<int32_t>();
            return reshapeOutputInt32(data, shape, offset);
        }
        default:
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "unsupported output tensor type '%s'", elementTypeToString(type));
            return QoreValue();
    }
}

QoreHashNode* QoreOnnxModel::run(const QoreHashNode* inputs, ExceptionSink* xsink) {
    if (!session) {
        xsink->raiseException("ML-ONNX-ERROR", "model is not loaded");
        return nullptr;
    }

    // Build input tensor names and values
    std::vector<const char*> input_names;
    std::vector<Ort::Value> input_tensors;

    // We need to keep the data buffers alive until after Run()
    std::vector<std::unique_ptr<std::vector<float>>> float_buffers;
    std::vector<std::unique_ptr<std::vector<double>>> double_buffers;
    std::vector<std::unique_ptr<std::vector<int32_t>>> int32_buffers;
    std::vector<std::unique_ptr<std::vector<int64_t>>> int64_buffers;

    for (const auto& meta : input_meta) {
        input_names.push_back(meta.name.c_str());

        // Find the input value in the hash
        QoreValue val = inputs->getKeyValue(meta.name.c_str());
        if (val.isNullOrNothing()) {
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "missing required input tensor '%s'", meta.name.c_str());
            return nullptr;
        }

        // Infer shape
        std::vector<int64_t> shape = inferShape(val, meta, xsink);
        if (*xsink) {
            return nullptr;
        }

        int64_t total_elements = 1;
        for (int64_t dim : shape) {
            total_elements *= dim;
        }

        Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator,
            OrtMemTypeDefault);

        switch (meta.element_type) {
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: {
                auto buf = std::make_unique<std::vector<float>>();
                flattenToFloats(val, *buf, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if ((int64_t)buf->size() != total_elements) {
                    xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                        "input tensor '%s': expected %lld elements, got %zu",
                        meta.name.c_str(), (long long)total_elements, buf->size());
                    return nullptr;
                }
                input_tensors.push_back(
                    Ort::Value::CreateTensor<float>(mem_info, buf->data(), buf->size(),
                        shape.data(), shape.size()));
                float_buffers.push_back(std::move(buf));
                break;
            }
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE: {
                auto buf = std::make_unique<std::vector<double>>();
                flattenToDoubles(val, *buf, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if ((int64_t)buf->size() != total_elements) {
                    xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                        "input tensor '%s': expected %lld elements, got %zu",
                        meta.name.c_str(), (long long)total_elements, buf->size());
                    return nullptr;
                }
                input_tensors.push_back(
                    Ort::Value::CreateTensor<double>(mem_info, buf->data(), buf->size(),
                        shape.data(), shape.size()));
                double_buffers.push_back(std::move(buf));
                break;
            }
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: {
                auto buf = std::make_unique<std::vector<int32_t>>();
                flattenToInt32(val, *buf, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if ((int64_t)buf->size() != total_elements) {
                    xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                        "input tensor '%s': expected %lld elements, got %zu",
                        meta.name.c_str(), (long long)total_elements, buf->size());
                    return nullptr;
                }
                input_tensors.push_back(
                    Ort::Value::CreateTensor<int32_t>(mem_info, buf->data(), buf->size(),
                        shape.data(), shape.size()));
                int32_buffers.push_back(std::move(buf));
                break;
            }
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: {
                auto buf = std::make_unique<std::vector<int64_t>>();
                flattenToInt64(val, *buf, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if ((int64_t)buf->size() != total_elements) {
                    xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                        "input tensor '%s': expected %lld elements, got %zu",
                        meta.name.c_str(), (long long)total_elements, buf->size());
                    return nullptr;
                }
                input_tensors.push_back(
                    Ort::Value::CreateTensor<int64_t>(mem_info, buf->data(), buf->size(),
                        shape.data(), shape.size()));
                int64_buffers.push_back(std::move(buf));
                break;
            }
            default:
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "unsupported input tensor type '%s' for tensor '%s'",
                    elementTypeToString(meta.element_type), meta.name.c_str());
                return nullptr;
        }
    }

    // Build output names
    std::vector<const char*> output_names;
    for (const auto& meta : output_meta) {
        output_names.push_back(meta.name.c_str());
    }

    // Run inference
    std::vector<Ort::Value> output_tensors;
    try {
        output_tensors = session->Run(Ort::RunOptions{nullptr},
            input_names.data(), input_tensors.data(), input_names.size(),
            output_names.data(), output_names.size());
    } catch (const Ort::Exception& e) {
        xsink->raiseException("ML-ONNX-INFERENCE-ERROR", "inference failed: %s", e.what());
        return nullptr;
    }

    // Convert outputs to Qore hash (values can be any type: scalars, lists, etc.)
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    for (size_t i = 0; i < output_tensors.size(); ++i) {
        QoreValue out_val = convertOutputTensor(output_tensors[i], xsink);
        if (*xsink) {
            return nullptr;
        }
        rv->setKeyValue(output_meta[i].name.c_str(), out_val, xsink);
    }

    return rv.release();
}

QoreListNode* QoreOnnxModel::runBatch(const QoreListNode* batch, ExceptionSink* xsink) {
    if (!session) {
        xsink->raiseException("ML-ONNX-ERROR", "model is not loaded");
        return nullptr;
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (size_t i = 0; i < batch->size(); ++i) {
        QoreValue entry = batch->retrieveEntry(i);
        if (entry.getType() != NT_HASH) {
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "batch element %zu is not a hash", i);
            return nullptr;
        }
        QoreHashNode* result = run(entry.get<const QoreHashNode>(), xsink);
        if (*xsink) {
            return nullptr;
        }
        rv->push(result, xsink);
    }
    return rv.release();
}

#endif // HAVE_ONNXRUNTIME
