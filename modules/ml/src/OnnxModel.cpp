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
#include <limits>
#include <mutex>
#include <unordered_set>

// Hashdecl extern declarations
extern const TypedHashDecl* hashdeclOnnxTensorInfo;
extern const TypedHashDecl* hashdeclOnnxModelInfo;
extern const TypedHashDecl* hashdeclOnnxProviderConfig;
extern const TypedHashDecl* hashdeclOnnxSessionConfig;
extern const TypedHashDecl* hashdeclOnnxProviderDiagnostic;

namespace {

std::mutex auto_provider_mutex;
std::unordered_set<std::string> unusable_auto_providers;

bool isAutoProviderDisabled(const std::string& provider) {
    std::lock_guard<std::mutex> lock(auto_provider_mutex);
    return unusable_auto_providers.find(provider) != unusable_auto_providers.end();
}

void disableAutoProvider(const std::string& provider) {
    std::lock_guard<std::mutex> lock(auto_provider_mutex);
    unusable_auto_providers.insert(provider);
}

QoreBufferElementType onnxTypeToBufferElementType(ONNXTensorElementDataType type) {
    switch (type) {
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
            return QoreBufferElementType::Float32;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
            return QoreBufferElementType::Float64;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
            return QoreBufferElementType::Int8;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16:
            return QoreBufferElementType::Int16;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
            return QoreBufferElementType::Int32;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
            return QoreBufferElementType::Int64;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL:
            return QoreBufferElementType::Bool;
        default:
            return QoreBufferElementType::Invalid;
    }
}

int64_t tensorShapeElementCount(const std::vector<int64_t>& shape, ExceptionSink* xsink) {
    int64_t total = 1;
    for (int64_t dim : shape) {
        if (dim < 0) {
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "cannot use unresolved dynamic tensor shape dimension " QLLD " for inference", dim);
            return -1;
        }
        total *= dim;
    }
    return total;
}

bool isTensorObject(QoreValue value) {
    return value.getType() == NT_OBJECT && value.get<const QoreObject>()->getClass()->getID() == CID_TENSOR;
}

std::vector<std::string> getAvailableOnnxProviders() {
    std::vector<std::string> providers = Ort::GetAvailableProviders();
    if (std::find(providers.begin(), providers.end(), "CPUExecutionProvider") == providers.end()) {
        providers.push_back("CPUExecutionProvider");
    }
    return providers;
}

QoreListNode* stringVectorToList(const std::vector<std::string>& values, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(stringTypeInfo), xsink);
    for (const auto& value : values) {
        rv->push(new QoreStringNode(value), xsink);
    }
    return rv.release();
}

QoreListNode* providerAliases(const std::vector<std::string>& aliases, ExceptionSink* xsink) {
    return stringVectorToList(aliases, xsink);
}

void setStringOption(QoreHashNode* h, const char* key, const char* value, ExceptionSink* xsink) {
    h->setKeyValue(key, new QoreStringNode(value), xsink);
}

QoreHashNode* makeProviderOptionInfo(const char* canonical, const std::vector<std::string>& aliases,
        const std::vector<std::pair<const char*, const char*>>& options,
        const std::vector<std::string>& available, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    setStringOption(*rv, "name", canonical, xsink);
    rv->setKeyValue("available",
        std::find(available.begin(), available.end(), canonical) != available.end(), xsink);
    rv->setKeyValue("aliases", providerAliases(aliases, xsink), xsink);

    ReferenceHolder<QoreHashNode> opts(new QoreHashNode(autoTypeInfo), xsink);
    for (const auto& opt : options) {
        setStringOption(*opts, opt.first, opt.second, xsink);
    }
    rv->setKeyValue("known_options", opts.release(), xsink);
    return rv.release();
}

QoreHashNode* getOnnxProviderOptionsMetadata(ExceptionSink* xsink) {
    std::vector<std::string> available = getAvailableOnnxProviders();
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);

    rv->setKeyValue("CPUExecutionProvider", makeProviderOptionInfo("CPUExecutionProvider",
        {"CPU", "CPUExecutionProvider"}, {}, available, xsink), xsink);
    rv->setKeyValue("CUDAExecutionProvider", makeProviderOptionInfo("CUDAExecutionProvider",
        {"CUDA", "CUDAExecutionProvider"}, {
            {"device_id", "CUDA device ordinal to use"},
            {"gpu_mem_limit", "maximum GPU memory in bytes"},
            {"arena_extend_strategy", "CUDA arena extension strategy"},
            {"cudnn_conv_algo_search", "cuDNN convolution algorithm search mode"},
            {"do_copy_in_default_stream", "whether copies run on the default stream"},
            {"enable_cuda_graph", "enable CUDA graph capture when supported"},
        }, available, xsink), xsink);
    rv->setKeyValue("TensorrtExecutionProvider", makeProviderOptionInfo("TensorrtExecutionProvider",
        {"TensorRT", "TensorRTExecutionProvider", "TensorrtExecutionProvider"}, {
            {"device_id", "CUDA device ordinal to use"},
            {"trt_max_workspace_size", "TensorRT workspace size in bytes"},
            {"trt_fp16_enable", "enable FP16 TensorRT kernels"},
            {"trt_int8_enable", "enable INT8 TensorRT kernels"},
            {"trt_engine_cache_enable", "enable TensorRT engine cache"},
            {"trt_engine_cache_path", "TensorRT engine cache directory"},
        }, available, xsink), xsink);
    rv->setKeyValue("CoreMLExecutionProvider", makeProviderOptionInfo("CoreMLExecutionProvider",
        {"CoreML", "CoreMLExecutionProvider"}, {}, available, xsink), xsink);
    rv->setKeyValue("OpenVINOExecutionProvider", makeProviderOptionInfo("OpenVINOExecutionProvider",
        {"OpenVINO", "OpenVINOExecutionProvider"}, {
            {"device_type", "OpenVINO target device such as CPU, GPU, or AUTO"},
            {"precision", "OpenVINO precision policy"},
        }, available, xsink), xsink);
    rv->setKeyValue("DmlExecutionProvider", makeProviderOptionInfo("DmlExecutionProvider",
        {"DML", "DirectML", "DmlExecutionProvider"}, {
            {"device_id", "DirectML device ordinal to use"},
        }, available, xsink), xsink);
    rv->setKeyValue("ROCMExecutionProvider", makeProviderOptionInfo("ROCMExecutionProvider",
        {"ROCM", "ROCm", "ROCMExecutionProvider"}, {
            {"device_id", "ROCm device ordinal to use"},
            {"gpu_mem_limit", "maximum GPU memory in bytes"},
        }, available, xsink), xsink);

    return rv.release();
}

}

QoreListNode* qore_ml_get_onnx_providers(ExceptionSink* xsink) {
    return stringVectorToList(getAvailableOnnxProviders(), xsink);
}

QoreHashNode* qore_ml_get_onnx_provider_options(ExceptionSink* xsink) {
    return getOnnxProviderOptionsMetadata(xsink);
}

QoreOnnxModel::QoreOnnxModel(const char* model_path, ExceptionSink* xsink)
    : active_provider("CPUExecutionProvider") {
    try {
        available_providers = getAvailableOnnxProviders();
        env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "qore-ml");
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        autoDetectProvider(session_options);
        createSessionFromPath(model_path, session_options, nullptr, xsink);
        if (*xsink) {
            return;
        }
        loadMetadata(xsink);
    } catch (const Ort::Exception& e) {
        xsink->raiseException("ML-ONNX-ERROR", "failed to load ONNX model '%s': %s",
            model_path, e.what());
    }
}

QoreOnnxModel::QoreOnnxModel(const char* model_path, const QoreHashNode* config,
    ExceptionSink* xsink) : active_provider("CPUExecutionProvider") {
    try {
        available_providers = getAvailableOnnxProviders();
        Ort::SessionOptions session_options;
        if (!initEnvAndOptions(session_options, config, xsink)) {
            return;
        }
        createSessionFromPath(model_path, session_options, config, xsink);
        if (*xsink) {
            return;
        }
        loadMetadata(xsink);
    } catch (const Ort::Exception& e) {
        xsink->raiseException("ML-ONNX-ERROR", "failed to load ONNX model '%s': %s",
            model_path, e.what());
    }
}

QoreOnnxModel::QoreOnnxModel(const void* model_data, size_t model_data_len,
    ExceptionSink* xsink) : active_provider("CPUExecutionProvider") {
    try {
        available_providers = getAvailableOnnxProviders();
        env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "qore-ml");
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        autoDetectProvider(session_options);
        // Ort::Session copies the buffer, so the caller's memory lifetime doesn't matter
        createSessionFromMemory(model_data, model_data_len, session_options, nullptr, xsink);
        if (*xsink) {
            return;
        }
        loadMetadata(xsink);
    } catch (const Ort::Exception& e) {
        xsink->raiseException("ML-ONNX-ERROR",
            "failed to load ONNX model from memory (%zu bytes): %s", model_data_len, e.what());
    }
}

QoreOnnxModel::QoreOnnxModel(const void* model_data, size_t model_data_len,
    const QoreHashNode* config, ExceptionSink* xsink)
    : active_provider("CPUExecutionProvider") {
    try {
        available_providers = getAvailableOnnxProviders();
        Ort::SessionOptions session_options;
        if (!initEnvAndOptions(session_options, config, xsink)) {
            return;
        }
        createSessionFromMemory(model_data, model_data_len, session_options, config, xsink);
        if (*xsink) {
            return;
        }
        loadMetadata(xsink);
    } catch (const Ort::Exception& e) {
        xsink->raiseException("ML-ONNX-ERROR",
            "failed to load ONNX model from memory (%zu bytes): %s", model_data_len, e.what());
    }
}

bool QoreOnnxModel::initEnvAndOptions(Ort::SessionOptions& session_options,
    const QoreHashNode* config, ExceptionSink* xsink) {
    // Read log severity from config (default: WARNING = 2)
    OrtLoggingLevel log_level = ORT_LOGGING_LEVEL_WARNING;
    QoreValue log_sev_val = config->getKeyValue("log_severity");
    if (!log_sev_val.isNullOrNothing()) {
        int64_t log_sev = log_sev_val.getAsBigInt();
        if (log_sev < 0 || log_sev > 4) {
            xsink->raiseException("ML-ONNX-ERROR",
                "invalid log_severity %lld; must be 0-4 (0=verbose, 1=info, 2=warning, "
                "3=error, 4=fatal)", (long long)log_sev);
            return false;
        }
        log_level = static_cast<OrtLoggingLevel>(log_sev);
    }

    env = std::make_unique<Ort::Env>(log_level, "qore-ml");

    configureProviderPolicy(config, xsink);
    if (*xsink) {
        return false;
    }

    configureSession(session_options, config, xsink);
    if (*xsink) {
        return false;
    }
    return true;
}

void QoreOnnxModel::configureBaseSessionOptions(Ort::SessionOptions& opts,
    const QoreHashNode* config, ExceptionSink* xsink) {
    if (!config) {
        opts.SetIntraOpNumThreads(1);
        return;
    }

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
}

std::string QoreOnnxModel::normalizeProviderName(const std::string& name) {
    if (name == "CPU" || name == "CPUExecutionProvider") {
        return "CPUExecutionProvider";
    }
    if (name == "CUDA" || name == "CUDAExecutionProvider") {
        return "CUDAExecutionProvider";
    }
    if (name == "TensorRT" || name == "TensorRTExecutionProvider"
            || name == "TensorrtExecutionProvider") {
        return "TensorrtExecutionProvider";
    }
    if (name == "CoreML" || name == "CoreMLExecutionProvider") {
        return "CoreMLExecutionProvider";
    }
    if (name == "OpenVINO" || name == "OpenVINOExecutionProvider") {
        return "OpenVINOExecutionProvider";
    }
    if (name == "DML" || name == "DirectML" || name == "DmlExecutionProvider") {
        return "DmlExecutionProvider";
    }
    if (name == "ROCM" || name == "ROCm" || name == "ROCMExecutionProvider") {
        return "ROCMExecutionProvider";
    }
    return name;
}

bool QoreOnnxModel::isProviderAvailable(const std::string& name) const {
    std::string normalized = normalizeProviderName(name);
    return normalized == "CPUExecutionProvider"
        || std::find(available_providers.begin(), available_providers.end(), normalized)
            != available_providers.end();
}

std::string QoreOnnxModel::availableProvidersString() const {
    std::string rv;
    for (size_t i = 0; i < available_providers.size(); ++i) {
        if (i) {
            rv += ", ";
        }
        rv += available_providers[i];
    }
    return rv;
}

OnnxProviderDiagnostic& QoreOnnxModel::providerDiagnostic(const std::string& name) {
    std::string normalized = normalizeProviderName(name);
    for (auto& diag : provider_diagnostics) {
        if (diag.name == normalized) {
            return diag;
        }
    }
    provider_diagnostics.push_back(OnnxProviderDiagnostic());
    OnnxProviderDiagnostic& diag = provider_diagnostics.back();
    diag.name = normalized;
    diag.available = isProviderAvailable(normalized);
    return diag;
}

void QoreOnnxModel::markProviderAppended(const std::string& name, bool auto_selected) {
    std::string normalized = normalizeProviderName(name);
    OnnxProviderDiagnostic& diag = providerDiagnostic(normalized);
    diag.appended = true;
    diag.auto_selected = auto_selected;
    diag.error.clear();
    for (auto& d : provider_diagnostics) {
        d.active = d.name == active_provider;
    }
}

void QoreOnnxModel::markProviderError(const std::string& name, const std::string& error) {
    OnnxProviderDiagnostic& diag = providerDiagnostic(name);
    diag.error = error;
}

void QoreOnnxModel::configureProviderPolicy(const QoreHashNode* config, ExceptionSink* xsink) {
    allow_cpu_fallback = true;
    fail_on_provider_fallback = false;
    required_providers.clear();

    if (!config) {
        return;
    }

    QoreValue allow_fallback_val = config->getKeyValue("allow_cpu_fallback");
    if (!allow_fallback_val.isNullOrNothing()) {
        allow_cpu_fallback = allow_fallback_val.getAsBool();
    }

    QoreValue fail_fallback_val = config->getKeyValue("fail_on_provider_fallback");
    if (!fail_fallback_val.isNullOrNothing()) {
        fail_on_provider_fallback = fail_fallback_val.getAsBool();
    }

    QoreValue required_val = config->getKeyValue("required_providers");
    if (!required_val.isNullOrNothing()) {
        if (required_val.getType() != NT_LIST) {
            xsink->raiseException("ML-ONNX-PROVIDER-ERROR",
                "invalid required_providers value; expected a list of provider names");
            return;
        }

        const QoreListNode* required = required_val.get<const QoreListNode>();
        for (size_t i = 0; i < required->size(); ++i) {
            QoreStringValueHelper name(required->retrieveEntry(i));
            std::string normalized = normalizeProviderName(name->c_str());
            required_providers.push_back(normalized);
            OnnxProviderDiagnostic& diag = providerDiagnostic(normalized);
            diag.required = true;
            if (!diag.available) {
                xsink->raiseException("ML-ONNX-PROVIDER-ERROR",
                    "required ONNX Runtime execution provider '%s' is not available; "
                    "available providers: %s", normalized.c_str(), availableProvidersString().c_str());
                return;
            }
        }
    }
}

void QoreOnnxModel::validateRequiredProviders(ExceptionSink* xsink) const {
    std::string requested;
    for (size_t i = 0; i < requested_providers.size(); ++i) {
        if (i) {
            requested += ", ";
        }
        requested += requested_providers[i];
    }
    if (requested.empty()) {
        requested = "<none>";
    }

    for (const auto& required : required_providers) {
        bool appended = false;
        for (const auto& diag : provider_diagnostics) {
            if (diag.name == required && diag.appended) {
                appended = true;
                break;
            }
        }
        if (!appended) {
            xsink->raiseException("ML-ONNX-PROVIDER-ERROR",
                "required ONNX Runtime execution provider '%s' was available but was not "
                "configured for this session; requested providers: %s; available providers: %s",
                required.c_str(), requested.c_str(), availableProvidersString().c_str());
            return;
        }
    }
}

void QoreOnnxModel::createSessionFromPath(const char* model_path, Ort::SessionOptions& opts,
    const QoreHashNode* config, ExceptionSink* xsink) {
    try {
        session = std::make_unique<Ort::Session>(*env, model_path, opts);
    } catch (const Ort::Exception& e) {
        if (explicit_provider_config && active_provider != "CPUExecutionProvider") {
            markProviderError(active_provider, e.what());
            xsink->raiseException("ML-ONNX-PROVIDER-ERROR",
                "failed to load ONNX model '%s' with requested execution provider '%s': %s; "
                "available providers reported by ONNX Runtime: %s. Use providers: () for "
                "explicit CPU-only execution, or set a provider that is fully installed on "
                "this host.", model_path, active_provider.c_str(), e.what(),
                availableProvidersString().c_str());
            return;
        }
        if (!auto_provider_selected || active_provider == "CPUExecutionProvider") {
            throw;
        }

        std::string provider = active_provider;
        disableAutoProvider(provider);
        markProviderError(provider, e.what());
        if (!allow_cpu_fallback || fail_on_provider_fallback) {
            xsink->raiseException("ML-ONNX-PROVIDER-ERROR",
                "failed to load ONNX model '%s' with auto-selected provider '%s': %s; "
                "CPU fallback is disabled by session configuration", model_path, provider.c_str(),
                e.what());
            return;
        }
        Ort::SessionOptions cpu_opts;
        configureBaseSessionOptions(cpu_opts, config, xsink);
        if (*xsink) {
            return;
        }
        active_provider = "CPUExecutionProvider";
        auto_provider_selected = false;
        cpu_fallback_used = true;
        markProviderAppended("CPUExecutionProvider", false);
        OnnxProviderDiagnostic& cpu_diag = providerDiagnostic("CPUExecutionProvider");
        cpu_diag.cpu_fallback = true;

        try {
            session = std::make_unique<Ort::Session>(*env, model_path, cpu_opts);
        } catch (const Ort::Exception& cpu_e) {
            xsink->raiseException("ML-ONNX-ERROR",
                "failed to load ONNX model '%s' with auto-selected provider '%s': %s; "
                "CPU fallback also failed: %s", model_path, provider.c_str(), e.what(),
                cpu_e.what());
        }
    }
}

void QoreOnnxModel::createSessionFromMemory(const void* model_data, size_t model_data_len,
    Ort::SessionOptions& opts, const QoreHashNode* config, ExceptionSink* xsink) {
    try {
        session = std::make_unique<Ort::Session>(*env, model_data, model_data_len, opts);
    } catch (const Ort::Exception& e) {
        if (explicit_provider_config && active_provider != "CPUExecutionProvider") {
            markProviderError(active_provider, e.what());
            xsink->raiseException("ML-ONNX-PROVIDER-ERROR",
                "failed to load ONNX model from memory (%zu bytes) with requested execution "
                "provider '%s': %s; available providers reported by ONNX Runtime: %s. Use "
                "providers: () for explicit CPU-only execution, or set a provider that is "
                "fully installed on this host.", model_data_len, active_provider.c_str(),
                e.what(), availableProvidersString().c_str());
            return;
        }
        if (!auto_provider_selected || active_provider == "CPUExecutionProvider") {
            throw;
        }

        std::string provider = active_provider;
        disableAutoProvider(provider);
        markProviderError(provider, e.what());
        if (!allow_cpu_fallback || fail_on_provider_fallback) {
            xsink->raiseException("ML-ONNX-PROVIDER-ERROR",
                "failed to load ONNX model from memory (%zu bytes) with auto-selected provider "
                "'%s': %s; CPU fallback is disabled by session configuration", model_data_len,
                provider.c_str(), e.what());
            return;
        }
        Ort::SessionOptions cpu_opts;
        configureBaseSessionOptions(cpu_opts, config, xsink);
        if (*xsink) {
            return;
        }
        active_provider = "CPUExecutionProvider";
        auto_provider_selected = false;
        cpu_fallback_used = true;
        markProviderAppended("CPUExecutionProvider", false);
        OnnxProviderDiagnostic& cpu_diag = providerDiagnostic("CPUExecutionProvider");
        cpu_diag.cpu_fallback = true;

        try {
            session = std::make_unique<Ort::Session>(*env, model_data, model_data_len,
                cpu_opts);
        } catch (const Ort::Exception& cpu_e) {
            xsink->raiseException("ML-ONNX-ERROR",
                "failed to load ONNX model from memory (%zu bytes) with auto-selected "
                "provider '%s': %s; CPU fallback also failed: %s", model_data_len,
                provider.c_str(), e.what(), cpu_e.what());
        }
    }
}

void QoreOnnxModel::configureSession(Ort::SessionOptions& opts, const QoreHashNode* config,
    ExceptionSink* xsink) {
    configureBaseSessionOptions(opts, config, xsink);
    if (*xsink) {
        return;
    }

    // providers (list of OnnxProviderConfig)
    QoreValue providers_val = config->getKeyValue("providers");
    if (!providers_val.isNullOrNothing() && providers_val.getType() != NT_LIST) {
        xsink->raiseException("ML-ONNX-PROVIDER-ERROR",
            "invalid providers value; expected a list of OnnxProviderConfig hashes");
        return;
    }

    if (!providers_val.isNullOrNothing()) {
        explicit_provider_config = true;
        const QoreListNode* providers = providers_val.get<const QoreListNode>();
        if (!providers->size()) {
            markProviderAppended("CPUExecutionProvider", false);
            validateRequiredProviders(xsink);
            return;
        }

        for (size_t i = 0; i < providers->size(); ++i) {
            QoreValue pval = providers->retrieveEntry(i);
            if (pval.getType() != NT_HASH) {
                xsink->raiseException("ML-ONNX-PROVIDER-ERROR",
                    "invalid providers entry at index %zu; expected an OnnxProviderConfig hash", i);
                return;
            }
            const QoreHashNode* pconfig = pval.get<const QoreHashNode>();
            QoreValue name_val = pconfig->getKeyValue("name");
            if (name_val.isNullOrNothing()) {
                xsink->raiseException("ML-ONNX-PROVIDER-ERROR",
                    "invalid providers entry at index %zu; missing provider name", i);
                return;
            }
            QoreStringValueHelper name_str(name_val);
            std::string provider_name = normalizeProviderName(name_str->c_str());
            requested_providers.push_back(provider_name);
            OnnxProviderDiagnostic& diag = providerDiagnostic(provider_name);
            diag.requested = true;

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
            diag.options = provider_opts;

            if (!isProviderAvailable(provider_name)) {
                xsink->raiseException("ML-ONNX-PROVIDER-ERROR",
                    "requested ONNX Runtime execution provider '%s' is not available; "
                    "available providers: %s; call ML::ml_get_onnx_providers() to inspect "
                    "this runtime, or use providers: () for explicit CPU-only execution",
                    provider_name.c_str(), availableProvidersString().c_str());
                return;
            }

            appendProvider(opts, provider_name, provider_opts, xsink);
            if (*xsink) {
                if (diag.error.empty()) {
                    diag.error = "provider append failed";
                }
                return;
            }
            markProviderAppended(provider_name, false);
        }
        validateRequiredProviders(xsink);
    } else {
        // No explicit providers configured — auto-detect best available GPU provider
        autoDetectProvider(opts);
        validateRequiredProviders(xsink);
    }
}

void QoreOnnxModel::autoDetectProvider(Ort::SessionOptions& opts) {
    auto_provider_selected = false;
    if (available_providers.empty()) {
        available_providers = getAvailableOnnxProviders();
    }

    // Priority order: CUDA (Linux), CoreML (macOS), then CPU fallback
    static const std::vector<std::string> preferred = {
        "CUDAExecutionProvider",
        "CoreMLExecutionProvider",
    };

    for (const auto& pref : preferred) {
        OnnxProviderDiagnostic& diag = providerDiagnostic(pref);
        if (std::find(available_providers.begin(), available_providers.end(), pref) != available_providers.end()) {
            if (isAutoProviderDisabled(pref)) {
                diag.error = "provider was disabled after a previous session creation failure";
                continue;
            }

            try {
                std::unordered_map<std::string, std::string> empty_opts;
                ExceptionSink xsink;
                appendProvider(opts, pref, empty_opts, &xsink);
                if (!xsink) {
                    auto_provider_selected = active_provider != "CPUExecutionProvider";
                    markProviderAppended(pref, auto_provider_selected);
                    return;
                }
                // Provider failed to initialize — try the next one
                diag.error = "provider append failed";
                xsink.clear();
                disableAutoProvider(pref);
                active_provider = "CPUExecutionProvider";
                auto_provider_selected = false;
            } catch (...) {
                diag.error = "provider append raised an unknown C++ exception";
                disableAutoProvider(pref);
                active_provider = "CPUExecutionProvider";
                auto_provider_selected = false;
            }
        }
    }
    // Fall through to CPU (always available, no action needed)
    markProviderAppended("CPUExecutionProvider", false);
}

void QoreOnnxModel::appendProvider(Ort::SessionOptions& opts, const std::string& name,
    const std::unordered_map<std::string, std::string>& provider_opts,
    ExceptionSink* xsink) {
    std::string normalized = normalizeProviderName(name);

    // CPU is always a fallback; no action needed
    if (normalized == "CPUExecutionProvider") {
        if (active_provider.empty()) {
            active_provider = "CPUExecutionProvider";
        }
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
                    markProviderError(normalized, msg);
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
                markProviderError(normalized, msg);
                xsink->raiseException("ML-ONNX-PROVIDER-ERROR",
                    "failed to append CUDA provider: %s", msg.c_str());
                return;
            }
            if (active_provider.empty() || active_provider == "CPUExecutionProvider") {
                active_provider = "CUDAExecutionProvider";
            }
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
                    markProviderError(normalized, msg);
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
                markProviderError(normalized, msg);
                xsink->raiseException("ML-ONNX-PROVIDER-ERROR",
                    "failed to append TensorRT provider: %s", msg.c_str());
                return;
            }
            if (active_provider.empty() || active_provider == "CPUExecutionProvider") {
                active_provider = "TensorrtExecutionProvider";
            }
        } else {
            // Generic API for all other providers
            opts.AppendExecutionProvider(normalized, provider_opts);
            if (active_provider.empty() || active_provider == "CPUExecutionProvider") {
                active_provider = normalized;
            }
        }
    } catch (const Ort::Exception& e) {
        markProviderError(normalized, e.what());
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

QoreListNode* QoreOnnxModel::getProviders(ExceptionSink* xsink) const {
    return stringVectorToList(available_providers, xsink);
}

QoreHashNode* QoreOnnxModel::getProviderOptions(ExceptionSink* xsink) const {
    return getOnnxProviderOptionsMetadata(xsink);
}

QoreListNode* QoreOnnxModel::getRequestedProviders(ExceptionSink* xsink) const {
    return stringVectorToList(requested_providers, xsink);
}

static QoreHashNode* providerDiagnosticToHash(const OnnxProviderDiagnostic& diag, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclOnnxProviderDiagnostic, xsink), xsink);
    rv->setKeyValue("name", new QoreStringNode(diag.name), xsink);
    rv->setKeyValue("requested", diag.requested, xsink);
    rv->setKeyValue("required", diag.required, xsink);
    rv->setKeyValue("available", diag.available, xsink);
    rv->setKeyValue("appended", diag.appended, xsink);
    rv->setKeyValue("auto_selected", diag.auto_selected, xsink);
    rv->setKeyValue("active", diag.active, xsink);
    rv->setKeyValue("cpu_fallback", diag.cpu_fallback, xsink);

    if (!diag.error.empty()) {
        rv->setKeyValue("error", new QoreStringNode(diag.error), xsink);
    }

    if (!diag.options.empty()) {
        ReferenceHolder<QoreHashNode> options(new QoreHashNode(stringTypeInfo), xsink);
        for (const auto& opt : diag.options) {
            options->setKeyValue(opt.first.c_str(), new QoreStringNode(opt.second), xsink);
        }
        rv->setKeyValue("options", options.release(), xsink);
    }

    return rv.release();
}

QoreListNode* QoreOnnxModel::getProviderDiagnostics(ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(hashdeclOnnxProviderDiagnostic->getTypeInfo()),
        xsink);
    for (const auto& diag : provider_diagnostics) {
        rv->push(providerDiagnosticToHash(diag, xsink), xsink);
    }
    return rv.release();
}

QoreHashNode* QoreOnnxModel::getEffectiveProviderReport(ExceptionSink* xsink) const {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    rv->setKeyValue("active_provider", new QoreStringNode(active_provider), xsink);
    rv->setKeyValue("available_providers", getProviders(xsink), xsink);
    rv->setKeyValue("requested_providers", getRequestedProviders(xsink), xsink);
    rv->setKeyValue("provider_diagnostics", getProviderDiagnostics(xsink), xsink);
    rv->setKeyValue("explicit_provider_config", explicit_provider_config, xsink);
    rv->setKeyValue("allow_cpu_fallback", allow_cpu_fallback, xsink);
    rv->setKeyValue("fail_on_provider_fallback", fail_on_provider_fallback, xsink);
    rv->setKeyValue("cpu_fallback_used", cpu_fallback_used, xsink);
    rv->setKeyValue("auto_provider_selected", auto_provider_selected, xsink);
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
    rv->setKeyValue("available_providers", getProviders(xsink), xsink);
    rv->setKeyValue("requested_providers", getRequestedProviders(xsink), xsink);
    rv->setKeyValue("provider_diagnostics", getProviderDiagnostics(xsink), xsink);
    rv->setKeyValue("cpu_fallback_used", cpu_fallback_used, xsink);

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

void QoreOnnxModel::flattenToInt16(const QoreValue& val, std::vector<int16_t>& out,
        ExceptionSink* xsink) {
    if (val.getType() == NT_LIST) {
        const QoreListNode* list = val.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "flattening ONNX int16 tensor input")) {
                return;
            }
            flattenToInt16(list->retrieveEntry(i), out, xsink);
            if (*xsink) {
                return;
            }
        }
    } else {
        int64_t v = val.getAsBigInt();
        if (v < std::numeric_limits<int16_t>::min() || v > std::numeric_limits<int16_t>::max()) {
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "int16 tensor value " QLLD " is outside the supported range", v);
            return;
        }
        out.push_back((int16_t)v);
    }
}

void QoreOnnxModel::flattenToInt8(const QoreValue& val, std::vector<int8_t>& out,
        ExceptionSink* xsink) {
    if (val.getType() == NT_LIST) {
        const QoreListNode* list = val.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "flattening ONNX int8 tensor input")) {
                return;
            }
            flattenToInt8(list->retrieveEntry(i), out, xsink);
            if (*xsink) {
                return;
            }
        }
    } else {
        int64_t v = val.getAsBigInt();
        if (v < std::numeric_limits<int8_t>::min() || v > std::numeric_limits<int8_t>::max()) {
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "int8 tensor value " QLLD " is outside the supported range", v);
            return;
        }
        out.push_back((int8_t)v);
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

void QoreOnnxModel::flattenToBools(const QoreValue& val, std::vector<uint8_t>& out,
        ExceptionSink* xsink) {
    if (val.getType() == NT_LIST) {
        const QoreListNode* list = val.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "flattening ONNX bool tensor input")) {
                return;
            }
            flattenToBools(list->retrieveEntry(i), out, xsink);
            if (*xsink) {
                return;
            }
        }
    } else {
        out.push_back(val.getAsBool() ? 1 : 0);
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

QoreValue QoreOnnxModel::reshapeOutputInt16(const int16_t* data,
        const std::vector<int64_t>& shape, size_t& offset, ExceptionSink* xsink) {
    if (shape.empty() || (shape.size() == 1 && shape[0] == 1)) {
        return (int64_t)data[offset++];
    }
    if (shape.size() == 1) {
        ReferenceHolder<QoreListNode> list(new QoreListNode(bigIntTypeInfo), nullptr);
        for (int64_t i = 0; i < shape[0]; ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "reshaping ONNX int16 tensor output")) {
                return QoreValue();
            }
            list->push((int64_t)data[offset++], nullptr);
        }
        return list.release();
    }
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), nullptr);
    std::vector<int64_t> inner_shape(shape.begin() + 1, shape.end());
    for (int64_t i = 0; i < shape[0]; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "reshaping ONNX int16 tensor output")) {
            return QoreValue();
        }
        QoreValue value = reshapeOutputInt16(data, inner_shape, offset, xsink);
        if (*xsink) {
            return QoreValue();
        }
        list->push(value, nullptr);
    }
    return list.release();
}

QoreValue QoreOnnxModel::reshapeOutputInt8(const int8_t* data,
        const std::vector<int64_t>& shape, size_t& offset, ExceptionSink* xsink) {
    if (shape.empty() || (shape.size() == 1 && shape[0] == 1)) {
        return (int64_t)data[offset++];
    }
    if (shape.size() == 1) {
        ReferenceHolder<QoreListNode> list(new QoreListNode(bigIntTypeInfo), nullptr);
        for (int64_t i = 0; i < shape[0]; ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "reshaping ONNX int8 tensor output")) {
                return QoreValue();
            }
            list->push((int64_t)data[offset++], nullptr);
        }
        return list.release();
    }
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), nullptr);
    std::vector<int64_t> inner_shape(shape.begin() + 1, shape.end());
    for (int64_t i = 0; i < shape[0]; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "reshaping ONNX int8 tensor output")) {
            return QoreValue();
        }
        QoreValue value = reshapeOutputInt8(data, inner_shape, offset, xsink);
        if (*xsink) {
            return QoreValue();
        }
        list->push(value, nullptr);
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

QoreValue QoreOnnxModel::reshapeOutputBool(const bool* data,
        const std::vector<int64_t>& shape, size_t& offset, ExceptionSink* xsink) {
    if (shape.empty() || (shape.size() == 1 && shape[0] == 1)) {
        return data[offset++];
    }
    if (shape.size() == 1) {
        ReferenceHolder<QoreListNode> list(new QoreListNode(boolTypeInfo), nullptr);
        for (int64_t i = 0; i < shape[0]; ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "reshaping ONNX bool tensor output")) {
                return QoreValue();
            }
            list->push(data[offset++], nullptr);
        }
        return list.release();
    }
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), nullptr);
    std::vector<int64_t> inner_shape(shape.begin() + 1, shape.end());
    for (int64_t i = 0; i < shape[0]; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "reshaping ONNX bool tensor output")) {
            return QoreValue();
        }
        QoreValue value = reshapeOutputBool(data, inner_shape, offset, xsink);
        if (*xsink) {
            return QoreValue();
        }
        list->push(value, nullptr);
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
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16: {
            const int16_t* data = tensor.GetTensorData<int16_t>();
            return reshapeOutputInt16(data, shape, offset, xsink);
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8: {
            const int8_t* data = tensor.GetTensorData<int8_t>();
            return reshapeOutputInt8(data, shape, offset, xsink);
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL: {
            const bool* data = tensor.GetTensorData<bool>();
            return reshapeOutputBool(data, shape, offset, xsink);
        }
        default:
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "unsupported output tensor type '%s'", elementTypeToString(type));
            return QoreValue();
    }
}

QoreValue QoreOnnxModel::convertOutputTensorToTensor(Ort::Value& tensor, ExceptionSink* xsink) {
    if (!tensor.IsTensor()) {
        xsink->raiseException("ML-ONNX-INFERENCE-ERROR", "output is not a tensor");
        return QoreValue();
    }

    auto tensor_info = tensor.GetTensorTypeAndShapeInfo();
    ONNXTensorElementDataType type = tensor_info.GetElementType();
    std::vector<int64_t> shape = tensor_info.GetShape();
    QoreBufferElementType buffer_type = onnxTypeToBufferElementType(type);
    if (buffer_type == QoreBufferElementType::Invalid) {
        xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
            "unsupported output tensor type '%s'", elementTypeToString(type));
        return QoreValue();
    }

    int64_t total_elements = tensorShapeElementCount(shape, xsink);
    if (*xsink) {
        return QoreValue();
    }

    ReferenceHolder<QoreBufferNode> buffer(
        new QoreBufferNode(buffer_type, false, static_cast<size_t>(total_elements)), xsink);
    switch (type) {
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: {
            const float* data = tensor.GetTensorData<float>();
            std::memcpy((*buffer)->getRawData(), data, sizeof(float) * static_cast<size_t>(total_elements));
            break;
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE: {
            const double* data = tensor.GetTensorData<double>();
            std::memcpy((*buffer)->getRawData(), data, sizeof(double) * static_cast<size_t>(total_elements));
            break;
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: {
            const int32_t* data = tensor.GetTensorData<int32_t>();
            std::memcpy((*buffer)->getRawData(), data, sizeof(int32_t) * static_cast<size_t>(total_elements));
            break;
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16: {
            const int16_t* data = tensor.GetTensorData<int16_t>();
            std::memcpy((*buffer)->getRawData(), data, sizeof(int16_t) * static_cast<size_t>(total_elements));
            break;
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8: {
            const int8_t* data = tensor.GetTensorData<int8_t>();
            std::memcpy((*buffer)->getRawData(), data, sizeof(int8_t) * static_cast<size_t>(total_elements));
            break;
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: {
            const int64_t* data = tensor.GetTensorData<int64_t>();
            std::memcpy((*buffer)->getRawData(), data, sizeof(int64_t) * static_cast<size_t>(total_elements));
            break;
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL: {
            const bool* data = tensor.GetTensorData<bool>();
            for (int64_t i = 0; i < total_elements; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "converting ONNX bool tensor output")) {
                    return QoreValue();
                }
                (*buffer)->setEntry(static_cast<size_t>(i), data[i], xsink);
                if (*xsink) {
                    return QoreValue();
                }
            }
            break;
        }
        default:
            assert(false);
    }

    ReferenceHolder<QoreTensor> qore_tensor(
        new QoreTensor(buffer.release(), std::move(shape)), xsink);
    return qore_ml_tensor_to_object(qore_tensor.release(), getProgram(), xsink);
}

QoreHashNode* QoreOnnxModel::run(const QoreHashNode* inputs, ExceptionSink* xsink) {
    return runImpl(inputs, false, xsink);
}

QoreHashNode* QoreOnnxModel::runTensors(const QoreHashNode* inputs, ExceptionSink* xsink) {
    return runImpl(inputs, true, xsink);
}

QoreHashNode* QoreOnnxModel::runImpl(const QoreHashNode* inputs, bool return_tensors, ExceptionSink* xsink) {
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
    std::vector<std::unique_ptr<std::vector<int8_t>>> int8_buffers;
    std::vector<std::unique_ptr<std::vector<int16_t>>> int16_buffers;
    std::vector<std::unique_ptr<std::vector<int32_t>>> int32_buffers;
    std::vector<std::unique_ptr<std::vector<int64_t>>> int64_buffers;
    std::vector<std::unique_ptr<std::vector<uint8_t>>> bool_buffers;

    for (const auto& meta : input_meta) {
        input_names.push_back(meta.name.c_str());

        // Find the input value in the hash
        QoreValue val = inputs->getKeyValue(meta.name.c_str());
        if (val.isNullOrNothing()) {
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "missing required input tensor '%s'", meta.name.c_str());
            return nullptr;
        }

        const QoreBufferNode* direct_buffer = nullptr;
        std::vector<int64_t> direct_shape;
        ReferenceHolder<QoreTensor> tensor_holder(xsink);

        if (isTensorObject(val)) {
            QoreObject* obj = val.get<QoreObject>();
            tensor_holder = static_cast<QoreTensor*>(obj->getReferencedPrivateData(CID_TENSOR, xsink));
            if (*xsink) {
                return nullptr;
            }
            direct_buffer = (*tensor_holder)->getBuffer();
            direct_shape = (*tensor_holder)->getShape();

            if (meta.shape.size() != direct_shape.size()) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': model expects rank %zu, ML::Tensor has rank %zu",
                    meta.name.c_str(), meta.shape.size(), direct_shape.size());
                return nullptr;
            }
            for (size_t i = 0; i < meta.shape.size(); ++i) {
                if (meta.shape[i] > 0 && meta.shape[i] != direct_shape[i]) {
                    xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                        "input tensor '%s': model expects dimension %zu to be " QLLD ", ML::Tensor has " QLLD,
                        meta.name.c_str(), i, meta.shape[i], direct_shape[i]);
                    return nullptr;
                }
            }
        } else if (val.getType() == NT_BUFFER) {
            direct_buffer = val.get<const QoreBufferNode>();
            direct_shape = meta.shape;
            if (direct_shape.empty()) {
                direct_shape.push_back(static_cast<int64_t>(direct_buffer->size()));
            } else {
                int dynamic_idx = -1;
                int64_t known = 1;
                for (size_t i = 0; i < direct_shape.size(); ++i) {
                    if (direct_shape[i] == -1) {
                        if (dynamic_idx >= 0) {
                            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                                "input tensor '%s': cannot infer multiple dynamic dimensions from a flat buffer; "
                                "wrap the buffer in ML::Tensor with an explicit shape",
                                meta.name.c_str());
                            return nullptr;
                        }
                        dynamic_idx = static_cast<int>(i);
                    } else if (direct_shape[i] < -1) {
                        xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                            "input tensor '%s': unsupported negative dimension " QLLD,
                            meta.name.c_str(), direct_shape[i]);
                        return nullptr;
                    } else {
                        known *= direct_shape[i];
                    }
                }
                if (dynamic_idx >= 0) {
                    if (!known || direct_buffer->size() % known) {
                        xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                            "input tensor '%s': cannot infer dynamic dimension from %zu buffer elements and known "
                            "shape product " QLLD,
                            meta.name.c_str(), direct_buffer->size(), known);
                        return nullptr;
                    }
                    direct_shape[dynamic_idx] = static_cast<int64_t>(direct_buffer->size() / known);
                }
            }
        }

        if (direct_buffer) {
            if (direct_buffer->hasNullableElements()) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': nullable buffer elements cannot be passed to ONNX Runtime; impute or filter "
                    "missing values first",
                    meta.name.c_str());
                return nullptr;
            }
            QoreBufferElementType expected = onnxTypeToBufferElementType(meta.element_type);
            if (expected == QoreBufferElementType::Invalid || direct_buffer->getElementType() != expected) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': model expects ONNX type '%s', but input buffer has type '%s'",
                    meta.name.c_str(), elementTypeToString(meta.element_type),
                    qore_buffer_element_type_name(direct_buffer->getElementType()));
                return nullptr;
            }
            if (direct_buffer->ensureHostStorage(xsink)) {
                return nullptr;
            }

            int64_t total_elements = tensorShapeElementCount(direct_shape, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (total_elements != static_cast<int64_t>(direct_buffer->size())) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': shape expects " QLLD " elements, got %zu",
                    meta.name.c_str(), total_elements, direct_buffer->size());
                return nullptr;
            }

            Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
            switch (meta.element_type) {
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
                    input_tensors.push_back(Ort::Value::CreateTensor<float>(mem_info,
                        const_cast<float*>(static_cast<const float*>(direct_buffer->getRawData())),
                        direct_buffer->size(), direct_shape.data(), direct_shape.size()));
                    break;
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
                    input_tensors.push_back(Ort::Value::CreateTensor<double>(mem_info,
                        const_cast<double*>(static_cast<const double*>(direct_buffer->getRawData())),
                        direct_buffer->size(), direct_shape.data(), direct_shape.size()));
                    break;
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
                    input_tensors.push_back(Ort::Value::CreateTensor<int8_t>(mem_info,
                        const_cast<int8_t*>(static_cast<const int8_t*>(direct_buffer->getRawData())),
                        direct_buffer->size(), direct_shape.data(), direct_shape.size()));
                    break;
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16:
                    input_tensors.push_back(Ort::Value::CreateTensor<int16_t>(mem_info,
                        const_cast<int16_t*>(static_cast<const int16_t*>(direct_buffer->getRawData())),
                        direct_buffer->size(), direct_shape.data(), direct_shape.size()));
                    break;
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
                    input_tensors.push_back(Ort::Value::CreateTensor<int32_t>(mem_info,
                        const_cast<int32_t*>(static_cast<const int32_t*>(direct_buffer->getRawData())),
                        direct_buffer->size(), direct_shape.data(), direct_shape.size()));
                    break;
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
                    input_tensors.push_back(Ort::Value::CreateTensor<int64_t>(mem_info,
                        const_cast<int64_t*>(static_cast<const int64_t*>(direct_buffer->getRawData())),
                        direct_buffer->size(), direct_shape.data(), direct_shape.size()));
                    break;
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL: {
                    auto buf = std::make_unique<std::vector<uint8_t>>();
                    buf->reserve(direct_buffer->size());
                    for (size_t i = 0; i < direct_buffer->size(); ++i) {
                        if (i && !(i % 100) && qore_check_cancel(xsink, "converting bool tensor input")) {
                            return nullptr;
                        }
                        buf->push_back(direct_buffer->getReferencedEntry(i, xsink).getAsBool() ? 1 : 0);
                        if (*xsink) {
                            return nullptr;
                        }
                    }
                    input_tensors.push_back(Ort::Value::CreateTensor<bool>(mem_info,
                        reinterpret_cast<bool*>(buf->data()), buf->size(), direct_shape.data(), direct_shape.size()));
                    bool_buffers.push_back(std::move(buf));
                    break;
                }
                default:
                    assert(false);
            }
            continue;
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
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8: {
                auto buf = std::make_unique<std::vector<int8_t>>();
                flattenToInt8(val, *buf, xsink);
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
                    Ort::Value::CreateTensor<int8_t>(mem_info, buf->data(), buf->size(),
                        shape.data(), shape.size()));
                int8_buffers.push_back(std::move(buf));
                break;
            }
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16: {
                auto buf = std::make_unique<std::vector<int16_t>>();
                flattenToInt16(val, *buf, xsink);
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
                    Ort::Value::CreateTensor<int16_t>(mem_info, buf->data(), buf->size(),
                        shape.data(), shape.size()));
                int16_buffers.push_back(std::move(buf));
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
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL: {
                auto buf = std::make_unique<std::vector<uint8_t>>();
                flattenToBools(val, *buf, xsink);
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
                    Ort::Value::CreateTensor<bool>(mem_info, reinterpret_cast<bool*>(buf->data()), buf->size(),
                        shape.data(), shape.size()));
                bool_buffers.push_back(std::move(buf));
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
        QoreValue out_val = return_tensors
            ? convertOutputTensorToTensor(output_tensors[i], xsink)
            : convertOutputTensor(output_tensors[i], xsink);
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
