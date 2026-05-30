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

#include "qore/intern/QC_FutureImpl.h"
#include <qore/qore_thread.h>

#include <onnxruntime_session_options_config_keys.h>

#ifdef HAVE_CUDART
#include <cuda_runtime_api.h>
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <numeric>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_set>

// Hashdecl extern declarations

namespace {

std::mutex auto_provider_mutex;
std::unordered_set<std::string> unusable_auto_providers;
using SteadyClock = std::chrono::steady_clock;

bool isAutoProviderDisabled(const std::string& provider) {
    std::lock_guard<std::mutex> lock(auto_provider_mutex);
    return unusable_auto_providers.find(provider) != unusable_auto_providers.end();
}

double elapsedMilliseconds(const SteadyClock::time_point& start) {
    return std::chrono::duration<double, std::milli>(SteadyClock::now() - start).count();
}

std::string makeOutputNamesSignature(const QoreListNode* output_names) {
    if (!output_names) {
        return "all";
    }

    std::string rv = "selected:";
    for (size_t i = 0; i < output_names->size(); ++i) {
        QoreStringValueHelper name(output_names->retrieveEntry(i));
        rv += std::to_string(name->strlen());
        rv += ':';
        rv.append(name->c_str(), name->strlen());
        rv += ';';
    }
    return rv;
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
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING:
            return QoreBufferElementType::String;
        default:
            return QoreBufferElementType::Invalid;
    }
}

bool onnxTypeCanWrapExternalOutput(ONNXTensorElementDataType type) {
    switch (type) {
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16:
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
            return true;
        default:
            return false;
    }
}

const void* getOnnxTensorDataPointer(const Ort::Value& tensor, ONNXTensorElementDataType type) {
    switch (type) {
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
            return tensor.GetTensorData<float>();
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
            return tensor.GetTensorData<double>();
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
            return tensor.GetTensorData<int8_t>();
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16:
            return tensor.GetTensorData<int16_t>();
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
            return tensor.GetTensorData<int32_t>();
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
            return tensor.GetTensorData<int64_t>();
        default:
            return nullptr;
    }
}

// --- device <-> ONNX Runtime memory-info mapping (Phase A4) -----------------
//
// The classic Ort::MemoryInfo(name, allocator_type, device_id, mem_type) ctor and
// the GetDeviceType()/GetDeviceId()/GetAllocatorName() accessors are available in
// every supported ONNX Runtime release (1.20+), so no CreateMemoryInfo_V2 /
// HAVE_ORT_MEMORYINFO_V2 feature guard is needed here.

//! ONNX Runtime allocator/memory-info name for a Qore device kind, or nullptr if
//! ONNX Runtime has no device allocator for that kind.
const char* deviceKindToOrtMemoryName(QoreBufferDeviceKind kind) {
    switch (kind) {
        case QoreBufferDeviceKind::Cuda: return "Cuda";
        case QoreBufferDeviceKind::Rocm: return "Hip";  // ORT ROCm allocator name
        default: return nullptr;
    }
}

//! Maps a device-kind name (e.g. "cuda") to a QoreBufferDeviceKind.
QoreBufferDeviceKind deviceKindFromName(const std::string& name) {
    std::string n = name;
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return std::tolower(c); });
    if (n == "cuda" || n == "cudaexecutionprovider") return QoreBufferDeviceKind::Cuda;
    if (n == "rocm" || n == "hip" || n == "rocmexecutionprovider") return QoreBufferDeviceKind::Rocm;
    if (n == "opencl") return QoreBufferDeviceKind::OpenCL;
    if (n == "vulkan") return QoreBufferDeviceKind::Vulkan;
    if (n == "oneapi") return QoreBufferDeviceKind::OneAPI;
    if (n == "metal") return QoreBufferDeviceKind::Metal;
    if (n == "java") return QoreBufferDeviceKind::Java;
    return QoreBufferDeviceKind::Unknown;
}

//! Maps an ONNX Runtime memory-info device type back to a Qore device kind.
QoreBufferDeviceKind ortDeviceTypeToKind(OrtMemoryInfoDeviceType dt, const std::string& alloc_name) {
    if (dt == OrtMemoryInfoDeviceType_GPU) {
        std::string n = alloc_name;
        std::transform(n.begin(), n.end(), n.begin(),
            [](unsigned char c) { return std::tolower(c); });
        if (n.find("hip") != std::string::npos || n.find("rocm") != std::string::npos) {
            return QoreBufferDeviceKind::Rocm;
        }
        return QoreBufferDeviceKind::Cuda;  // default GPU allocator -> CUDA
    }
    return QoreBufferDeviceKind::Unknown;
}

//! Builds an Ort::MemoryInfo for a device descriptor.  Returns CPU memory info
//! when dev is null or describes host/unknown storage; raises
//! ML-ONNX-DEVICE-MISMATCH for a device kind ONNX Runtime cannot address.
Ort::MemoryInfo makeOrtMemoryInfo(const QoreBufferDeviceInfo* dev, ExceptionSink* xsink) {
    if (!dev || dev->kind == QoreBufferDeviceKind::Unknown) {
        return Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    }
    const char* name = deviceKindToOrtMemoryName(dev->kind);
    if (!name) {
        xsink->raiseException("ML-ONNX-DEVICE-MISMATCH",
            "device buffer is on a '%s' device, which ONNX Runtime cannot bind directly",
            qore_buffer_device_kind_name(dev->kind));
        return Ort::MemoryInfo(nullptr);
    }
    int id = dev->device_id >= 0 ? static_cast<int>(dev->device_id) : 0;
    return Ort::MemoryInfo(name, OrtArenaAllocator, id, OrtMemTypeDefault);
}

//! Creates an ONNX tensor over an external device pointer (no host copy) for the
//! directly-wrappable fixed-width numeric types.  Returns a null value for types
//! that cannot be wrapped zero-copy (caller must check onnxTypeCanWrapExternalOutput).
Ort::Value createDeviceInputTensor(ONNXTensorElementDataType type, const Ort::MemoryInfo& mem,
        const void* device_ptr, size_t count, const std::vector<int64_t>& shape) {
    void* p = const_cast<void*>(device_ptr);
    const int64_t* s = shape.data();
    size_t sn = shape.size();
    switch (type) {
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
            return Ort::Value::CreateTensor<float>(mem, static_cast<float*>(p), count, s, sn);
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
            return Ort::Value::CreateTensor<double>(mem, static_cast<double*>(p), count, s, sn);
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
            return Ort::Value::CreateTensor<int8_t>(mem, static_cast<int8_t*>(p), count, s, sn);
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16:
            return Ort::Value::CreateTensor<int16_t>(mem, static_cast<int16_t*>(p), count, s, sn);
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
            return Ort::Value::CreateTensor<int32_t>(mem, static_cast<int32_t*>(p), count, s, sn);
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
            return Ort::Value::CreateTensor<int64_t>(mem, static_cast<int64_t*>(p), count, s, sn);
        default:
            return Ort::Value(nullptr);
    }
}

//! Inspects an ONNX output tensor's memory info; on non-CPU memory fills out and
//! returns true, otherwise returns false (host/CPU memory).
bool ortValueToDeviceInfo(const Ort::Value& value, QoreBufferDeviceInfo& out) {
    Ort::ConstMemoryInfo mi = value.GetTensorMemoryInfo();
    if (mi.GetDeviceType() == OrtMemoryInfoDeviceType_CPU) {
        return false;
    }
    out.name = mi.GetAllocatorName();
    out.kind = ortDeviceTypeToKind(mi.GetDeviceType(), out.name);
    out.device_id = mi.GetDeviceId();
    return true;
}

const char* onnxElementTypeName(ONNXTensorElementDataType type);  // defined below

//! Owner that keeps an ONNX Runtime device-output allocation alive for the
//! lifetime of the wrapping QoreBufferNode, and records the element byte size so
//! the copy-to-host callback can size the device->host transfer.
struct OnnxDeviceOutputOwner {
    Ort::Value value;
    size_t element_size = 0;
};

#ifdef HAVE_CUDART
//! Copy-to-host callback for CUDA device outputs: cudaMemcpy device->host.
int onnxCudaCopyToHost(void* host_data, uint8_t* /*host_validity*/, size_t length,
        const void* device_data, const uint8_t* /*device_validity*/,
        const QoreBufferDeviceInfo& info, const void* owner, ExceptionSink* xsink) {
    const OnnxDeviceOutputOwner* o = static_cast<const OnnxDeviceOutputOwner*>(owner);
    size_t bytes = length * (o ? o->element_size : 0);
    if (!bytes) {
        return 0;
    }
    cudaError_t err = cudaMemcpy(host_data, device_data, bytes, cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        xsink->raiseException("ML-ONNX-DEVICE-MATERIALIZATION-ERROR",
            "failed to copy %zu bytes from %s device %lld to host: %s", bytes,
            qore_buffer_device_kind_name(info.kind), (long long)info.device_id,
            cudaGetErrorString(err));
        return -1;
    }
    return 0;
}
#endif

//! Wraps an ONNX device-output tensor as a device-backed QoreBufferNode with a
//! copy-to-host callback, keeping the Ort::Value owner alive.  Raises if the
//! element type cannot be wrapped or no device->host copy path is available.
QoreBufferNode* wrapOnnxDeviceOutput(Ort::Value&& tensor, ONNXTensorElementDataType type,
        QoreBufferElementType buffer_type, size_t total_elements,
        const QoreBufferDeviceInfo& dev_info, ExceptionSink* xsink) {
    if (!onnxTypeCanWrapExternalOutput(type)) {
        xsink->raiseException("ML-ONNX-DEVICE-BINDING-ERROR",
            "ONNX output type '%s' cannot be returned as a device-backed buffer; "
            "use a CPU output binding or materialize_outputs", onnxElementTypeName(type));
        return nullptr;
    }
#ifdef HAVE_CUDART
    if (dev_info.kind != QoreBufferDeviceKind::Cuda) {
        xsink->raiseException("ML-ONNX-DEVICE-MATERIALIZATION-ERROR",
            "no host-copy path is available for ONNX outputs on a '%s' device",
            qore_buffer_device_kind_name(dev_info.kind));
        return nullptr;
    }
    auto owner = std::make_shared<OnnxDeviceOutputOwner>();
    owner->element_size = qore_buffer_element_storage_size(buffer_type);
    owner->value = std::move(tensor);
    const void* device_data = total_elements ? getOnnxTensorDataPointer(owner->value, type) : nullptr;
    return QoreBufferNode::wrapExternalDeviceStorage(buffer_type, false, total_elements,
        device_data, nullptr, std::static_pointer_cast<const void>(owner), 0, dev_info,
        onnxCudaCopyToHost, xsink);
#else
    (void)buffer_type;
    (void)total_elements;
    xsink->raiseException("ML-ONNX-DEVICE-MATERIALIZATION-ERROR",
        "this ml build has no CUDA runtime support, so ONNX %s device outputs cannot be "
        "returned; rebuild with CUDA or use a CPU output binding",
        qore_buffer_device_kind_name(dev_info.kind));
    return nullptr;
#endif
}

const char* onnxElementTypeName(ONNXTensorElementDataType type) {
    switch (type) {
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: return "float";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: return "uint8";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8: return "int8";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16: return "uint16";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16: return "int16";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: return "int32";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: return "int64";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING: return "string";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL: return "bool";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16: return "float16";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE: return "double";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32: return "uint32";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64: return "uint64";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16: return "bfloat16";
        default: return "unknown";
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

int inferValueShapeRec(QoreValue value, std::vector<int64_t>& shape, ExceptionSink* xsink) {
    shape.clear();
    if (value.getType() != NT_LIST) {
        return 0;
    }

    const QoreListNode* list = value.get<const QoreListNode>();
    shape.push_back(static_cast<int64_t>(list->size()));
    if (list->empty()) {
        return 0;
    }

    std::vector<int64_t> first_shape;
    if (inferValueShapeRec(list->retrieveEntry(0), first_shape, xsink)) {
        return -1;
    }
    for (size_t i = 1; i < list->size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "inferring ONNX tensor input shape")) {
            return -1;
        }
        std::vector<int64_t> current_shape;
        if (inferValueShapeRec(list->retrieveEntry(i), current_shape, xsink)) {
            return -1;
        }
        if (current_shape != first_shape) {
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "cannot infer ONNX tensor shape from a ragged nested list; element %zu has a different shape", i);
            return -1;
        }
    }
    shape.insert(shape.end(), first_shape.begin(), first_shape.end());
    return 0;
}

QoreValue uint64ToQore(uint64_t value) {
    if (value <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
        return static_cast<int64_t>(value);
    }
    return new QoreNumberNode(std::to_string(value).c_str());
}

int qoreValueToUInt64(QoreValue value, uint64_t max_value, const char* type_name,
        uint64_t& out, ExceptionSink* xsink) {
    if (value.getType() == NT_STRING) {
        QoreStringValueHelper str(value);
        const char* s = str->c_str();
        if (!*s) {
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "%s tensor value is empty; expected an unsigned integer in the range 0..%llu",
                type_name, static_cast<unsigned long long>(max_value));
            return -1;
        }
        for (const char* p = s; *p; ++p) {
            if (*p < '0' || *p > '9') {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "%s tensor value '%s' is not an unsigned decimal integer in the range 0..%llu",
                    type_name, s, static_cast<unsigned long long>(max_value));
                return -1;
            }
        }
        char* end = nullptr;
        errno = 0;
        unsigned long long parsed = std::strtoull(s, &end, 10);
        if (errno || !end || *end || parsed > max_value) {
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "%s tensor value '%s' is outside the supported range 0..%llu",
                type_name, s, static_cast<unsigned long long>(max_value));
            return -1;
        }
        out = static_cast<uint64_t>(parsed);
        return 0;
    }

    int64_t v = value.getAsBigInt();
    if (v < 0 || static_cast<uint64_t>(v) > max_value) {
        xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
            "%s tensor value " QLLD " is outside the supported range 0..%llu",
            type_name, v, static_cast<unsigned long long>(max_value));
        return -1;
    }
    out = static_cast<uint64_t>(v);
    return 0;
}

int bufferToFloat16Vector(const QoreBufferNode& buffer, std::vector<Ort::Float16_t>& out,
        ExceptionSink* xsink) {
    out.reserve(buffer.size());
    for (size_t i = 0; i < buffer.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "converting tensor buffer to ONNX float16 input")) {
            return -1;
        }
        ValueHolder value(buffer.getReferencedEntry(i, xsink), xsink);
        if (*xsink) {
            return -1;
        }
        out.push_back(Ort::Float16_t(static_cast<float>(value->getAsFloat())));
    }
    return 0;
}

int bufferToBFloat16Vector(const QoreBufferNode& buffer, std::vector<Ort::BFloat16_t>& out,
        ExceptionSink* xsink) {
    out.reserve(buffer.size());
    for (size_t i = 0; i < buffer.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "converting tensor buffer to ONNX bfloat16 input")) {
            return -1;
        }
        ValueHolder value(buffer.getReferencedEntry(i, xsink), xsink);
        if (*xsink) {
            return -1;
        }
        out.push_back(Ort::BFloat16_t(static_cast<float>(value->getAsFloat())));
    }
    return 0;
}

template <typename T>
int bufferToUnsignedVector(const QoreBufferNode& buffer, std::vector<T>& out,
        uint64_t max_value, const char* type_name, ExceptionSink* xsink) {
    out.reserve(buffer.size());
    for (size_t i = 0; i < buffer.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "converting tensor buffer to unsigned ONNX input")) {
            return -1;
        }
        ValueHolder value(buffer.getReferencedEntry(i, xsink), xsink);
        if (*xsink) {
            return -1;
        }
        uint64_t parsed;
        if (qoreValueToUInt64(*value, max_value, type_name, parsed, xsink)) {
            return -1;
        }
        out.push_back(static_cast<T>(parsed));
    }
    return 0;
}

template <typename T>
QoreValue reshapeFloatingOutput(const T* data, const std::vector<int64_t>& shape,
        size_t& offset, const char* cancel_msg, ExceptionSink* xsink) {
    if (shape.empty() || (shape.size() == 1 && shape[0] == 1)) {
        return static_cast<double>(data[offset++].ToFloat());
    }
    if (shape.size() == 1) {
        ReferenceHolder<QoreListNode> list(new QoreListNode(floatTypeInfo), nullptr);
        for (int64_t i = 0; i < shape[0]; ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, cancel_msg)) {
                return QoreValue();
            }
            list->push(static_cast<double>(data[offset++].ToFloat()), nullptr);
        }
        return list.release();
    }
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), nullptr);
    std::vector<int64_t> inner_shape(shape.begin() + 1, shape.end());
    for (int64_t i = 0; i < shape[0]; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, cancel_msg)) {
            return QoreValue();
        }
        QoreValue value = reshapeFloatingOutput(data, inner_shape, offset, cancel_msg, xsink);
        if (*xsink) {
            return QoreValue();
        }
        list->push(value, nullptr);
    }
    return list.release();
}

template <typename T>
QoreValue reshapeUnsignedOutput(const T* data, const std::vector<int64_t>& shape,
        size_t& offset, const char* cancel_msg, ExceptionSink* xsink) {
    if (shape.empty() || (shape.size() == 1 && shape[0] == 1)) {
        return uint64ToQore(static_cast<uint64_t>(data[offset++]));
    }
    if (shape.size() == 1) {
        ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), nullptr);
        for (int64_t i = 0; i < shape[0]; ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, cancel_msg)) {
                return QoreValue();
            }
            list->push(uint64ToQore(static_cast<uint64_t>(data[offset++])), nullptr);
        }
        return list.release();
    }
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), nullptr);
    std::vector<int64_t> inner_shape(shape.begin() + 1, shape.end());
    for (int64_t i = 0; i < shape[0]; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, cancel_msg)) {
            return QoreValue();
        }
        QoreValue value = reshapeUnsignedOutput(data, inner_shape, offset, cancel_msg, xsink);
        if (*xsink) {
            return QoreValue();
        }
        list->push(value, nullptr);
    }
    return list.release();
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

struct HeldQoreValue {
    DLLLOCAL explicit HeldQoreValue(QoreValue value) : value(value.refSelf()) {
    }

    DLLLOCAL ~HeldQoreValue() {
        ExceptionSink xsink;
        value.discard(&xsink);
    }

    QoreValue value;
};

struct OnnxBoundOrtValue {
    DLLLOCAL OnnxBoundOrtValue() : value(nullptr) {
    }

    DLLLOCAL ~OnnxBoundOrtValue() {
        if (tensor_ref) {
            ExceptionSink xsink;
            tensor_ref->deref(&xsink);
        }
    }

    DLLLOCAL void holdValue(QoreValue value) {
        value_refs.push_back(std::make_unique<HeldQoreValue>(value));
    }

    DLLLOCAL void holdTensor(QoreTensor* tensor) {
        tensor_ref = tensor;
    }

    Ort::Value value;
    std::vector<int64_t> shape;
    std::vector<std::unique_ptr<HeldQoreValue>> value_refs;
    QoreTensor* tensor_ref = nullptr;

    std::unique_ptr<std::vector<float>> floats;
    std::unique_ptr<std::vector<double>> doubles;
    std::unique_ptr<std::vector<Ort::Float16_t>> float16s;
    std::unique_ptr<std::vector<Ort::BFloat16_t>> bfloat16s;
    std::unique_ptr<std::vector<int8_t>> int8s;
    std::unique_ptr<std::vector<int16_t>> int16s;
    std::unique_ptr<std::vector<int32_t>> int32s;
    std::unique_ptr<std::vector<int64_t>> int64s;
    std::unique_ptr<std::vector<uint8_t>> uint8s;
    std::unique_ptr<std::vector<uint16_t>> uint16s;
    std::unique_ptr<std::vector<uint32_t>> uint32s;
    std::unique_ptr<std::vector<uint64_t>> uint64s;
    std::unique_ptr<std::vector<uint8_t>> bool_bytes;
    std::unique_ptr<std::vector<std::string>> strings;
    std::vector<const char*> string_ptrs;
};

QoreListNode* qore_ml_get_onnx_providers(ExceptionSink* xsink) {
    return stringVectorToList(getAvailableOnnxProviders(), xsink);
}

QoreHashNode* qore_ml_get_onnx_provider_options(ExceptionSink* xsink) {
    return getOnnxProviderOptionsMetadata(xsink);
}

QoreObject* qore_ml_onnx_run_options_to_object(QoreOnnxRunOptions* options,
        QoreProgram* pgm, ExceptionSink* xsink) {
    ReferenceHolder<QoreOnnxRunOptions> holder(options, xsink);
    if (!options) {
        return nullptr;
    }
    QoreObject* obj = new QoreObject(QC_ONNXRUNOPTIONS, pgm);
    obj->setPrivate(CID_ONNXRUNOPTIONS, holder.release());
    return obj;
}

QoreObject* qore_ml_onnx_io_binding_to_object(QoreOnnxIoBinding* binding,
        QoreProgram* pgm, ExceptionSink* xsink) {
    ReferenceHolder<QoreOnnxIoBinding> holder(binding, xsink);
    if (!binding) {
        return nullptr;
    }
    QoreObject* obj = new QoreObject(QC_ONNXIOBINDING, pgm);
    obj->setPrivate(CID_ONNXIOBINDING, holder.release());
    return obj;
}

QoreObject* makeOnnxFutureObject(QorePromise* promise, const QoreTypeInfo* future_type,
        QoreProgram* pgm, ExceptionSink* xsink) {
    ReferenceHolder<QoreFuture> future_holder(promise->getFuture(xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    type_vec_t type_args;
    type_args.push_back(future_type ? future_type : autoTypeInfo);
    QoreObject* rv = new QoreObject(QC_FUTUREIMPL, pgm, future_holder.release());
    rv->setInstantiatedTypeInfo(QC_FUTUREIMPL->getTypeInfo(type_args));
    return rv;
}

static std::unordered_map<std::string, std::string> hashToStringMap(const QoreHashNode* hash,
        const char* context, ExceptionSink* xsink) {
    std::unordered_map<std::string, std::string> rv;
    if (!hash) {
        return rv;
    }
    ConstHashIterator hi(hash);
    size_t count = 0;
    while (hi.next()) {
        if (count && !(count % 100) && qore_check_cancel(xsink, "processing ONNX run config entries")) {
            return {};
        }
        ++count;
        QoreValue value = hi.get();
        if (value.isNullOrNothing()) {
            xsink->raiseException("ML-ONNX-RUN-OPTIONS-ERROR",
                "%s config entry '%s' is NOTHING; expected a string-compatible value",
                context, hi.getKey());
            return {};
        }
        QoreStringValueHelper str(value);
        rv[hi.getKey()] = str->c_str();
    }
    return rv;
}

static bool hasOrtSuffix(const char* path) {
    if (!path) {
        return false;
    }
    std::string p(path);
    return p.size() >= 4 && p.compare(p.size() - 4, 4, ".ort") == 0;
}

static std::string getConfigStringValue(const QoreHashNode* config, const char* key) {
    if (!config) {
        return {};
    }
    QoreValue val = config->getKeyValue(key);
    if (val.isNullOrNothing()) {
        return {};
    }
    QoreStringValueHelper str(val);
    return str->c_str();
}

static bool configValueIsOrt(const QoreHashNode* config, const char* key) {
    std::string value = getConfigStringValue(config, key);
    return value == "ORT" || value == "ort";
}

static void addSessionConfigEntry(Ort::SessionOptions& opts, const char* key,
        const std::string& value, ExceptionSink* xsink) {
    try {
        opts.AddConfigEntry(key, value.c_str());
    } catch (const Ort::Exception& e) {
        xsink->raiseException("ML-ONNX-ERROR",
            "failed to set ONNX Runtime session config entry '%s': %s", key, e.what());
    }
}

static QoreHashNode* buildOptimizationConfig(const QoreHashNode* config, const char* output_path,
        bool ort_format, const char* source_load_model_format, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> rv(config ? config->copy() : new QoreHashNode(hashdeclOnnxSessionConfig, xsink),
        xsink);
    rv->setKeyValue("optimized_model_filepath", new QoreStringNode(output_path), xsink);
    if (*xsink) {
        return nullptr;
    }

    if (ort_format || hasOrtSuffix(output_path)) {
        rv->setKeyValue("save_model_format", new QoreStringNode("ORT"), xsink);
        if (*xsink) {
            return nullptr;
        }
    }

    if (source_load_model_format && *source_load_model_format
            && rv->getKeyValue("load_model_format").isNullOrNothing()) {
        rv->setKeyValue("load_model_format", new QoreStringNode(source_load_model_format), xsink);
        if (*xsink) {
            return nullptr;
        }
    }

    return rv.release();
}

static QoreHashNode* makeOptimizationInfo(const char* input_path, const char* output_path,
        bool ort_format, const QoreOnnxModel& optimizer, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    rv->setKeyValue("input_path", input_path && *input_path
        ? QoreValue(new QoreStringNode(input_path)) : QoreValue(), xsink);
    rv->setKeyValue("output_path", new QoreStringNode(output_path), xsink);
    rv->setKeyValue("output_format", new QoreStringNode(ort_format ? "ORT" : "ONNX"), xsink);
    rv->setKeyValue("provider_report", optimizer.getEffectiveProviderReport(xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    return rv.release();
}

QoreOnnxRunOptions::QoreOnnxRunOptions(const QoreHashNode* opts, ExceptionSink* xsink) {
    if (!opts) {
        return;
    }

    QoreValue tag_val = opts->getKeyValue("run_tag");
    if (!tag_val.isNullOrNothing()) {
        QoreStringValueHelper tag(tag_val);
        run_tag = tag->c_str();
        try {
            options.SetRunTag(run_tag.c_str());
        } catch (const Ort::Exception& e) {
            xsink->raiseException("ML-ONNX-RUN-OPTIONS-ERROR",
                "failed to set ONNX Runtime run tag: %s", e.what());
            return;
        }
    }

    QoreValue severity_val = opts->getKeyValue("log_severity_level");
    if (!severity_val.isNullOrNothing()) {
        log_severity_level = static_cast<int>(severity_val.getAsBigInt());
        try {
            options.SetRunLogSeverityLevel(log_severity_level);
        } catch (const Ort::Exception& e) {
            xsink->raiseException("ML-ONNX-RUN-OPTIONS-ERROR",
                "failed to set ONNX Runtime run log severity level: %s", e.what());
            return;
        }
    }

    QoreValue verbosity_val = opts->getKeyValue("log_verbosity_level");
    if (!verbosity_val.isNullOrNothing()) {
        log_verbosity_level = static_cast<int>(verbosity_val.getAsBigInt());
        try {
            options.SetRunLogVerbosityLevel(log_verbosity_level);
        } catch (const Ort::Exception& e) {
            xsink->raiseException("ML-ONNX-RUN-OPTIONS-ERROR",
                "failed to set ONNX Runtime run log verbosity level: %s", e.what());
            return;
        }
    }

    QoreValue config_val = opts->getKeyValue("config");
    if (!config_val.isNullOrNothing()) {
        if (config_val.getType() != NT_HASH) {
            xsink->raiseException("ML-ONNX-RUN-OPTIONS-ERROR",
                "run options config must be a hash of string-compatible values");
            return;
        }
        config_entries = hashToStringMap(config_val.get<const QoreHashNode>(), "run options", xsink);
        if (*xsink) {
            return;
        }
        try {
            size_t count = 0;
            for (const auto& kv : config_entries) {
                if (count && !(count % 100) && qore_check_cancel(xsink, "adding ONNX run config entries")) {
                    return;
                }
                ++count;
                options.AddConfigEntry(kv.first.c_str(), kv.second.c_str());
            }
        } catch (const Ort::Exception& e) {
            xsink->raiseException("ML-ONNX-RUN-OPTIONS-ERROR",
                "failed to add ONNX Runtime run config entry: %s", e.what());
            return;
        }
    }

    QoreValue terminate_val = opts->getKeyValue("terminate");
    if (!terminate_val.isNullOrNothing() && terminate_val.getAsBool()) {
        setTerminate(xsink);
    }
}

void QoreOnnxRunOptions::setTerminate(ExceptionSink* xsink) {
    try {
        options.SetTerminate();
        terminated = true;
    } catch (const Ort::Exception& e) {
        xsink->raiseException("ML-ONNX-RUN-OPTIONS-ERROR",
            "failed to terminate ONNX Runtime run options: %s", e.what());
    }
}

void QoreOnnxRunOptions::unsetTerminate(ExceptionSink* xsink) {
    try {
        options.UnsetTerminate();
        terminated = false;
    } catch (const Ort::Exception& e) {
        xsink->raiseException("ML-ONNX-RUN-OPTIONS-ERROR",
            "failed to clear ONNX Runtime run termination flag: %s", e.what());
    }
}

QoreHashNode* QoreOnnxRunOptions::getInfo(ExceptionSink* xsink) const {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    rv->setKeyValue("run_tag", run_tag.empty() ? QoreValue() : QoreValue(new QoreStringNode(run_tag)), xsink);
    rv->setKeyValue("log_severity_level", log_severity_level, xsink);
    rv->setKeyValue("log_verbosity_level", log_verbosity_level, xsink);
    rv->setKeyValue("terminated", terminated, xsink);

    ReferenceHolder<QoreHashNode> config(new QoreHashNode(autoTypeInfo), xsink);
    size_t count = 0;
    for (const auto& kv : config_entries) {
        if (count && !(count % 100) && qore_check_cancel(xsink, "building ONNX run options info")) {
            return nullptr;
        }
        ++count;
        config->setKeyValue(kv.first.c_str(), new QoreStringNode(kv.second), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    rv->setKeyValue("config", config.release(), xsink);
    return rv.release();
}

QoreOnnxModel::QoreOnnxModel(const char* model_path, ExceptionSink* xsink)
    : active_provider("CPUExecutionProvider") {
    source_model_path = model_path;
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
    source_model_path = model_path;
    source_load_model_format = getConfigStringValue(config, "load_model_format");
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
    const char* data = static_cast<const char*>(model_data);
    source_model_data.assign(data, data + model_data_len);
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
    const char* data = static_cast<const char*>(model_data);
    source_model_data.assign(data, data + model_data_len);
    source_load_model_format = getConfigStringValue(config, "load_model_format");
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

    QoreValue profile_prefix_val = config->getKeyValue("profile_file_prefix");
    QoreValue enable_profile_val = config->getKeyValue("enable_profiling");
    bool enable_profile = (!enable_profile_val.isNullOrNothing() && enable_profile_val.getAsBool())
        || !profile_prefix_val.isNullOrNothing();
    if (enable_profile) {
        std::string prefix = "qore-onnx-profile";
        if (!profile_prefix_val.isNullOrNothing()) {
            QoreStringValueHelper profile_prefix(profile_prefix_val);
            if (!profile_prefix->strlen()) {
                xsink->raiseException("ML-ONNX-ERROR",
                    "invalid profile_file_prefix; expected a non-empty path prefix");
                return;
            }
            prefix = profile_prefix->c_str();
        }
        try {
            opts.EnableProfiling(prefix.c_str());
            {
                std::lock_guard<std::mutex> lock(stats_mutex);
                profiling_enabled = true;
                profiling_file_prefix = prefix;
                last_profile_file.clear();
            }
        } catch (const Ort::Exception& e) {
            xsink->raiseException("ML-ONNX-ERROR",
                "failed to enable ONNX Runtime profiling: %s", e.what());
            return;
        }
    }

    QoreValue optimized_path_val = config->getKeyValue("optimized_model_filepath");
    if (!optimized_path_val.isNullOrNothing()) {
        QoreStringValueHelper optimized_path(optimized_path_val);
        if (!optimized_path->strlen()) {
            xsink->raiseException("ML-ONNX-ERROR",
                "invalid optimized_model_filepath; expected a non-empty output path");
            return;
        }
        opts.SetOptimizedModelFilePath(optimized_path->c_str());
    }

    std::unordered_map<std::string, std::string> session_config_entries;
    QoreValue session_config_val = config->getKeyValue("config");
    if (!session_config_val.isNullOrNothing()) {
        if (session_config_val.getType() != NT_HASH) {
            xsink->raiseException("ML-ONNX-ERROR",
                "invalid ONNX session config value; expected a hash of string-compatible values");
            return;
        }
        session_config_entries = hashToStringMap(session_config_val.get<const QoreHashNode>(),
            "ONNX session options", xsink);
        if (*xsink) {
            return;
        }
    }

    QoreValue load_format_val = config->getKeyValue("load_model_format");
    if (!load_format_val.isNullOrNothing()) {
        QoreStringValueHelper load_format(load_format_val);
        if (!strcmp(load_format->c_str(), "ORT") || !strcmp(load_format->c_str(), "ort")) {
            session_config_entries[kOrtSessionOptionsConfigLoadModelFormat] = "ORT";
        } else if (strcmp(load_format->c_str(), "ONNX") && strcmp(load_format->c_str(), "onnx")) {
            xsink->raiseException("ML-ONNX-ERROR",
                "invalid load_model_format '%s'; expected 'ONNX' or 'ORT'", load_format->c_str());
            return;
        }
    }

    QoreValue save_format_val = config->getKeyValue("save_model_format");
    if (!save_format_val.isNullOrNothing()) {
        QoreStringValueHelper save_format(save_format_val);
        if (!strcmp(save_format->c_str(), "ORT") || !strcmp(save_format->c_str(), "ort")) {
            session_config_entries[kOrtSessionOptionsConfigSaveModelFormat] = "ORT";
        } else if (strcmp(save_format->c_str(), "ONNX") && strcmp(save_format->c_str(), "onnx")) {
            xsink->raiseException("ML-ONNX-ERROR",
                "invalid save_model_format '%s'; expected 'ONNX' or 'ORT'", save_format->c_str());
            return;
        }
    }

    size_t count = 0;
    for (const auto& entry : session_config_entries) {
        if (count && !(count % 100) && qore_check_cancel(xsink, "adding ONNX session config entries")) {
            return;
        }
        ++count;
        addSessionConfigEntry(opts, entry.first.c_str(), entry.second, xsink);
        if (*xsink) {
            return;
        }
    }
}

std::string QoreOnnxModel::normalizeProviderName(const std::string& name) {
    // match aliases case-insensitively; users may write e.g. "cuda", "CUDA", or
    // the canonical "CUDAExecutionProvider"
    std::string lname = name;
    std::transform(lname.begin(), lname.end(), lname.begin(),
        [](unsigned char c) { return std::tolower(c); });

    if (lname == "cpu" || lname == "cpuexecutionprovider") {
        return "CPUExecutionProvider";
    }
    if (lname == "cuda" || lname == "cudaexecutionprovider") {
        return "CUDAExecutionProvider";
    }
    if (lname == "tensorrt" || lname == "tensorrtexecutionprovider") {
        return "TensorrtExecutionProvider";
    }
    if (lname == "coreml" || lname == "coremlexecutionprovider") {
        return "CoreMLExecutionProvider";
    }
    if (lname == "openvino" || lname == "openvinoexecutionprovider") {
        return "OpenVINOExecutionProvider";
    }
    if (lname == "dml" || lname == "directml" || lname == "dmlexecutionprovider") {
        return "DmlExecutionProvider";
    }
    if (lname == "rocm" || lname == "rocmexecutionprovider") {
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

    configureDeviceBinding(config, xsink);
}

void QoreOnnxModel::configureDeviceBinding(const QoreHashNode* config, ExceptionSink* xsink) {
    // reset to defaults; an absent device_binding leaves device binding disabled
    device_binding = OnnxDeviceBindingPolicy();

    if (!config) {
        return;
    }

    QoreValue db_val = config->getKeyValue("device_binding");
    if (db_val.isNullOrNothing()) {
        return;
    }
    if (db_val.getType() != NT_HASH) {
        xsink->raiseException("ML-ONNX-DEVICE-BINDING-ERROR",
            "invalid device_binding value; expected a hash<OnnxDeviceBindingConfig>");
        return;
    }
    const QoreHashNode* db = db_val.get<const QoreHashNode>();

    QoreValue enabled_val = db->getKeyValue("enabled");
    if (!enabled_val.isNullOrNothing()) {
        device_binding.enabled = enabled_val.getAsBool();
    }

    QoreValue host_fallback_val = db->getKeyValue("allow_host_fallback");
    if (!host_fallback_val.isNullOrNothing()) {
        device_binding.allow_host_fallback = host_fallback_val.getAsBool();
    }

    QoreValue materialize_val = db->getKeyValue("materialize_outputs");
    if (!materialize_val.isNullOrNothing()) {
        device_binding.materialize_outputs = materialize_val.getAsBool();
    }

    QoreValue zc_in_val = db->getKeyValue("require_zero_copy_inputs");
    if (!zc_in_val.isNullOrNothing()) {
        device_binding.require_zero_copy_inputs = zc_in_val.getAsBool();
    }

    QoreValue zc_out_val = db->getKeyValue("require_zero_copy_outputs");
    if (!zc_out_val.isNullOrNothing()) {
        device_binding.require_zero_copy_outputs = zc_out_val.getAsBool();
    }

    QoreValue out_dev_val = db->getKeyValue("default_output_device");
    if (!out_dev_val.isNullOrNothing()) {
        QoreStringValueHelper out_dev(out_dev_val);
        std::string spec = out_dev->c_str();
        // optional ":<device_id>" suffix on an explicit provider/device name
        std::string name = spec;
        std::string id_part;
        size_t colon = spec.find(':');
        if (colon != std::string::npos) {
            name = spec.substr(0, colon);
            id_part = spec.substr(colon + 1);
        }

        std::string lname = name;
        std::transform(lname.begin(), lname.end(), lname.begin(),
            [](unsigned char c) { return std::tolower(c); });

        if (lname == "cpu") {
            device_binding.default_output_device = OnnxOutputDevice::Cpu;
        } else if (lname == "provider") {
            device_binding.default_output_device = OnnxOutputDevice::Provider;
        } else {
            device_binding.default_output_device = OnnxOutputDevice::Explicit;
            device_binding.device_name = normalizeProviderName(name);
        }

        if (!id_part.empty()) {
            try {
                device_binding.device_id = std::stoll(id_part);
            } catch (const std::exception&) {
                xsink->raiseException("ML-ONNX-DEVICE-BINDING-ERROR",
                    "invalid device_binding.default_output_device '%s'; the ':<device_id>' "
                    "suffix must be an integer", spec.c_str());
                return;
            }
            if (device_binding.device_id < 0) {
                xsink->raiseException("ML-ONNX-DEVICE-BINDING-ERROR",
                    "invalid device_binding.default_output_device '%s'; device id must be >= 0",
                    spec.c_str());
                return;
            }
        }
    }

    // a require_zero_copy_* or materialize policy is meaningless without enabling binding
    if (!device_binding.enabled
        && (device_binding.require_zero_copy_inputs || device_binding.require_zero_copy_outputs
            || device_binding.materialize_outputs)) {
        xsink->raiseException("ML-ONNX-DEVICE-BINDING-ERROR",
            "device_binding.require_zero_copy_inputs/require_zero_copy_outputs/materialize_outputs "
            "require device_binding.enabled = True");
        return;
    }
}

bool QoreOnnxModel::inputDeviceMatchesProvider(const QoreBufferDeviceInfo& dinfo) const {
    if (dinfo.kind == QoreBufferDeviceKind::Cuda) {
        return active_provider == "CUDAExecutionProvider"
            || active_provider == "TensorrtExecutionProvider";
    }
    if (dinfo.kind == QoreBufferDeviceKind::Rocm) {
        return active_provider == "ROCMExecutionProvider";
    }
    return false;
}

bool QoreOnnxModel::resolveOutputDeviceInfo(const QoreHashNode* device,
        QoreBufferDeviceInfo& out, ExceptionSink* xsink) const {
    // explicit {kind, device_id} hash overrides policy
    if (device) {
        QoreValue kind_val = device->getKeyValue("kind");
        if (kind_val.isNullOrNothing()) {
            xsink->raiseException("ML-ONNX-DEVICE-BINDING-ERROR",
                "device hash requires a 'kind' key (e.g. \"cuda\")");
            return false;
        }
        QoreStringValueHelper kind(kind_val);
        out.kind = deviceKindFromName(kind->c_str());
        if (out.kind == QoreBufferDeviceKind::Unknown) {
            xsink->raiseException("ML-ONNX-DEVICE-BINDING-ERROR",
                "unknown device kind '%s'", kind->c_str());
            return false;
        }
        QoreValue id_val = device->getKeyValue("device_id");
        out.device_id = id_val.isNullOrNothing() ? 0 : id_val.getAsBigInt();
        out.name = kind->c_str();
        return true;
    }

    // policy-driven resolution
    if (device_binding.default_output_device == OnnxOutputDevice::Cpu) {
        return false;
    }
    int64_t policy_id = device_binding.device_id >= 0 ? device_binding.device_id : 0;
    if (device_binding.default_output_device == OnnxOutputDevice::Explicit) {
        out.kind = deviceKindFromName(device_binding.device_name);
        out.device_id = policy_id;
        out.name = device_binding.device_name;
        if (out.kind == QoreBufferDeviceKind::Unknown) {
            xsink->raiseException("ML-ONNX-DEVICE-BINDING-ERROR",
                "device_binding.default_output_device '%s' is not a known device",
                device_binding.device_name.c_str());
            return false;
        }
        return true;
    }

    // "provider": follow the active execution provider
    QoreBufferDeviceKind active_kind = QoreBufferDeviceKind::Unknown;
    if (active_provider == "CUDAExecutionProvider" || active_provider == "TensorrtExecutionProvider") {
        active_kind = QoreBufferDeviceKind::Cuda;
    } else if (active_provider == "ROCMExecutionProvider") {
        active_kind = QoreBufferDeviceKind::Rocm;
    }
    if (active_kind != QoreBufferDeviceKind::Unknown) {
        out.kind = active_kind;
        out.device_id = policy_id;
        out.name = active_provider;
        return true;
    }

    // active provider is CPU-only
    if (device_binding.allow_host_fallback) {
        return false;
    }
    xsink->raiseException("ML-ONNX-DEVICE-UNAVAILABLE",
        "a device output was requested but the active provider is '%s', which has no device "
        "memory; use a non-CPU provider or set device_binding.allow_host_fallback = True",
        active_provider.empty() ? "CPUExecutionProvider" : active_provider.c_str());
    return false;
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
        input_meta[i].symbolic_shape.clear();
        // ONNX Runtime's C++ API has no zero-arg vector-returning overload for
        // symbolic dimensions (unlike GetShape()); the symbolic dim count matches
        // the tensor rank, so size the out-param array to the shape's length.
        std::vector<const char*> symbolic_dims(input_meta[i].shape.size(), nullptr);
        if (!symbolic_dims.empty()) {
            tensor_info.GetSymbolicDimensions(symbolic_dims.data(), symbolic_dims.size());
        }
        input_meta[i].symbolic_shape.reserve(symbolic_dims.size());
        for (const char* dim : symbolic_dims) {
            input_meta[i].symbolic_shape.push_back(dim ? dim : "");
        }
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
        output_meta[i].symbolic_shape.clear();
        // ONNX Runtime's C++ API has no zero-arg vector-returning overload for
        // symbolic dimensions (unlike GetShape()); the symbolic dim count matches
        // the tensor rank, so size the out-param array to the shape's length.
        std::vector<const char*> symbolic_dims(output_meta[i].shape.size(), nullptr);
        if (!symbolic_dims.empty()) {
            tensor_info.GetSymbolicDimensions(symbolic_dims.data(), symbolic_dims.size());
        }
        output_meta[i].symbolic_shape.reserve(symbolic_dims.size());
        for (const char* dim : symbolic_dims) {
            output_meta[i].symbolic_shape.push_back(dim ? dim : "");
        }
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
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16: return "bfloat16";
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

    ReferenceHolder<QoreListNode> symbolic_shape(new QoreListNode(stringTypeInfo), xsink);
    for (const auto& dim : meta.symbolic_shape) {
        symbolic_shape->push(new QoreStringNode(dim), xsink);
    }
    rv->setKeyValue("symbolic_shape", symbolic_shape.release(), xsink);
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

    ReferenceHolder<QoreHashNode> db(new QoreHashNode(autoTypeInfo), xsink);
    db->setKeyValue("enabled", device_binding.enabled, xsink);
    const char* out_dev = "provider";
    switch (device_binding.default_output_device) {
        case OnnxOutputDevice::Cpu: out_dev = "cpu"; break;
        case OnnxOutputDevice::Provider: out_dev = "provider"; break;
        case OnnxOutputDevice::Explicit: out_dev = "explicit"; break;
    }
    db->setKeyValue("default_output_device", new QoreStringNode(out_dev), xsink);
    if (!device_binding.device_name.empty()) {
        db->setKeyValue("device_name", new QoreStringNode(device_binding.device_name), xsink);
    }
    db->setKeyValue("device_id", device_binding.device_id, xsink);
    db->setKeyValue("allow_host_fallback", device_binding.allow_host_fallback, xsink);
    db->setKeyValue("materialize_outputs", device_binding.materialize_outputs, xsink);
    db->setKeyValue("require_zero_copy_inputs", device_binding.require_zero_copy_inputs, xsink);
    db->setKeyValue("require_zero_copy_outputs", device_binding.require_zero_copy_outputs, xsink);
    rv->setKeyValue("device_binding", db.release(), xsink);

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

void QoreOnnxModel::flattenToFloat16(const QoreValue& val, std::vector<Ort::Float16_t>& out,
        ExceptionSink* xsink) {
    if (val.getType() == NT_LIST) {
        const QoreListNode* list = val.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "flattening ONNX float16 tensor input")) {
                return;
            }
            flattenToFloat16(list->retrieveEntry(i), out, xsink);
            if (*xsink) {
                return;
            }
        }
    } else {
        out.push_back(Ort::Float16_t(static_cast<float>(val.getAsFloat())));
    }
}

void QoreOnnxModel::flattenToBFloat16(const QoreValue& val, std::vector<Ort::BFloat16_t>& out,
        ExceptionSink* xsink) {
    if (val.getType() == NT_LIST) {
        const QoreListNode* list = val.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "flattening ONNX bfloat16 tensor input")) {
                return;
            }
            flattenToBFloat16(list->retrieveEntry(i), out, xsink);
            if (*xsink) {
                return;
            }
        }
    } else {
        out.push_back(Ort::BFloat16_t(static_cast<float>(val.getAsFloat())));
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

void QoreOnnxModel::flattenToUInt32(const QoreValue& val, std::vector<uint32_t>& out,
        ExceptionSink* xsink) {
    if (val.getType() == NT_LIST) {
        const QoreListNode* list = val.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "flattening ONNX uint32 tensor input")) {
                return;
            }
            flattenToUInt32(list->retrieveEntry(i), out, xsink);
            if (*xsink) {
                return;
            }
        }
    } else {
        uint64_t v;
        if (qoreValueToUInt64(val, std::numeric_limits<uint32_t>::max(), "uint32", v, xsink)) {
            return;
        }
        out.push_back(static_cast<uint32_t>(v));
    }
}

void QoreOnnxModel::flattenToUInt16(const QoreValue& val, std::vector<uint16_t>& out,
        ExceptionSink* xsink) {
    if (val.getType() == NT_LIST) {
        const QoreListNode* list = val.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "flattening ONNX uint16 tensor input")) {
                return;
            }
            flattenToUInt16(list->retrieveEntry(i), out, xsink);
            if (*xsink) {
                return;
            }
        }
    } else {
        uint64_t v;
        if (qoreValueToUInt64(val, std::numeric_limits<uint16_t>::max(), "uint16", v, xsink)) {
            return;
        }
        out.push_back(static_cast<uint16_t>(v));
    }
}

void QoreOnnxModel::flattenToUInt8(const QoreValue& val, std::vector<uint8_t>& out,
        ExceptionSink* xsink) {
    if (val.getType() == NT_LIST) {
        const QoreListNode* list = val.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "flattening ONNX uint8 tensor input")) {
                return;
            }
            flattenToUInt8(list->retrieveEntry(i), out, xsink);
            if (*xsink) {
                return;
            }
        }
    } else {
        uint64_t v;
        if (qoreValueToUInt64(val, std::numeric_limits<uint8_t>::max(), "uint8", v, xsink)) {
            return;
        }
        out.push_back(static_cast<uint8_t>(v));
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

void QoreOnnxModel::flattenToUInt64(const QoreValue& val, std::vector<uint64_t>& out,
        ExceptionSink* xsink) {
    if (val.getType() == NT_LIST) {
        const QoreListNode* list = val.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "flattening ONNX uint64 tensor input")) {
                return;
            }
            flattenToUInt64(list->retrieveEntry(i), out, xsink);
            if (*xsink) {
                return;
            }
        }
    } else {
        uint64_t v;
        if (qoreValueToUInt64(val, std::numeric_limits<uint64_t>::max(), "uint64", v, xsink)) {
            return;
        }
        out.push_back(v);
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

void QoreOnnxModel::flattenToStrings(const QoreValue& val, std::vector<std::string>& out,
        ExceptionSink* xsink) {
    if (val.getType() == NT_LIST) {
        const QoreListNode* list = val.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "flattening ONNX string tensor input")) {
                return;
            }
            flattenToStrings(list->retrieveEntry(i), out, xsink);
            if (*xsink) {
                return;
            }
        }
    } else if (val.getType() == NT_STRING) {
        QoreStringValueHelper str(val);
        out.push_back(str->c_str());
    } else {
        xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
            "string tensor value has type '%s'; expected string", val.getFullTypeName());
    }
}

std::vector<int64_t> QoreOnnxModel::inferShape(const QoreValue& val, const TensorMeta& meta,
        ExceptionSink* xsink) {
    std::vector<int64_t> shape = meta.shape;
    std::vector<int64_t> supplied_shape;
    if (inferValueShapeRec(val, supplied_shape, xsink)) {
        return {};
    }

    if (shape.empty()) {
        return supplied_shape;
    }

    if (supplied_shape.empty()) {
        bool scalar_ok = true;
        for (int64_t& dim : shape) {
            if (dim == -1) {
                dim = 1;
            } else if (dim != 1) {
                scalar_ok = false;
            }
        }
        if (scalar_ok) {
            return shape;
        }
        xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
            "input tensor '%s': model expects rank %zu with shape metadata, but call supplies a scalar",
            meta.name.c_str(), shape.size());
        return {};
    }

    if (shape.size() != supplied_shape.size()) {
        xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
            "input tensor '%s': model expects rank %zu, but call supplies rank %zu",
            meta.name.c_str(), shape.size(), supplied_shape.size());
        return {};
    }

    for (size_t i = 0; i < shape.size(); ++i) {
        if (shape[i] == -1) {
            shape[i] = supplied_shape[i];
        } else if (shape[i] > 0 && shape[i] != supplied_shape[i]) {
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "input tensor '%s': model expects dimension %zu to be " QLLD ", but call supplies " QLLD,
                meta.name.c_str(), i, shape[i], supplied_shape[i]);
            return {};
        } else if (shape[i] < -1) {
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "input tensor '%s': unsupported negative dimension " QLLD " at index %zu",
                meta.name.c_str(), shape[i], i);
            return {};
        }
    }

    return shape;
}

const TensorMeta* QoreOnnxModel::findInputMeta(const char* name) const {
    for (const auto& meta : input_meta) {
        if (meta.name == name) {
            return &meta;
        }
    }
    return nullptr;
}

const TensorMeta* QoreOnnxModel::findOutputMeta(const char* name) const {
    for (const auto& meta : output_meta) {
        if (meta.name == name) {
            return &meta;
        }
    }
    return nullptr;
}

std::vector<const TensorMeta*> QoreOnnxModel::selectOutputMeta(const QoreListNode* output_names,
        ExceptionSink* xsink) const {
    std::vector<const TensorMeta*> selected;
    if (!output_names) {
        selected.reserve(output_meta.size());
        for (const auto& meta : output_meta) {
            selected.push_back(&meta);
        }
        return selected;
    }

    if (!output_names->size()) {
        xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
            "output_names must contain at least one ONNX output name; omit output_names to request all outputs");
        return {};
    }

    std::unordered_set<std::string> seen;
    selected.reserve(output_names->size());
    for (size_t i = 0; i < output_names->size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "checking ONNX output names")) {
            return {};
        }
        QoreStringValueHelper name(output_names->retrieveEntry(i));
        const TensorMeta* meta = findOutputMeta(name->c_str());
        if (!meta) {
            std::string available;
            for (size_t j = 0; j < output_meta.size(); ++j) {
                if (j) {
                    available += ", ";
                }
                available += output_meta[j].name;
            }
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "unknown ONNX output name '%s' at output_names[%zu]; available outputs: %s",
                name->c_str(), i, available.c_str());
            return {};
        }
        if (!seen.insert(meta->name).second) {
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "duplicate ONNX output name '%s' at output_names[%zu]; output names must be unique because "
                "results are returned in a hash keyed by output name",
                meta->name.c_str(), i);
            return {};
        }
        selected.push_back(meta);
    }
    return selected;
}

static int resolveFlatBufferShape(const TensorMeta& meta, size_t element_count,
        std::vector<int64_t>& shape, ExceptionSink* xsink) {
    shape = meta.shape;
    if (shape.empty()) {
        if (element_count != 1) {
            shape.push_back(static_cast<int64_t>(element_count));
        }
        return 0;
    }

    int dynamic_idx = -1;
    int64_t known = 1;
    for (size_t i = 0; i < shape.size(); ++i) {
        if (shape[i] == -1) {
            if (dynamic_idx >= 0) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': cannot infer multiple dynamic dimensions from a flat buffer; "
                    "wrap the buffer in ML::Tensor with an explicit shape",
                    meta.name.c_str());
                return -1;
            }
            dynamic_idx = static_cast<int>(i);
        } else if (shape[i] < -1) {
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "input tensor '%s': unsupported negative dimension " QLLD,
                meta.name.c_str(), shape[i]);
            return -1;
        } else if (shape[i] && known > std::numeric_limits<int64_t>::max() / shape[i]) {
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "input tensor '%s': shape product overflows int64", meta.name.c_str());
            return -1;
        } else {
            known *= shape[i];
        }
    }

    if (dynamic_idx >= 0) {
        if (!known || element_count % known) {
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "input tensor '%s': cannot infer dynamic dimension from %zu buffer elements and known "
                "shape product " QLLD,
                meta.name.c_str(), element_count, known);
            return -1;
        }
        shape[dynamic_idx] = static_cast<int64_t>(element_count / known);
    }
    return 0;
}

static int validateDirectTensorShape(const TensorMeta& meta, const std::vector<int64_t>& shape,
        const char* source, ExceptionSink* xsink) {
    if (meta.shape.size() != shape.size()) {
        xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
            "input tensor '%s': model expects rank %zu, %s has rank %zu",
            meta.name.c_str(), meta.shape.size(), source, shape.size());
        return -1;
    }
    for (size_t i = 0; i < meta.shape.size(); ++i) {
        if (meta.shape[i] > 0 && meta.shape[i] != shape[i]) {
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "input tensor '%s': model expects dimension %zu to be " QLLD ", %s has " QLLD,
                meta.name.c_str(), i, meta.shape[i], source, shape[i]);
            return -1;
        }
    }
    return 0;
}

static int validateDirectBuffer(const TensorMeta& meta, const QoreBufferNode* buffer,
        const std::vector<int64_t>& shape, ExceptionSink* xsink, bool require_host_storage = true) {
    if (buffer->hasNullableElements()) {
        xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
            "input tensor '%s': nullable buffer elements cannot be passed to ONNX Runtime; impute or filter "
            "missing values first",
            meta.name.c_str());
        return -1;
    }
    QoreBufferElementType expected = onnxTypeToBufferElementType(meta.element_type);
    if (expected == QoreBufferElementType::Invalid) {
        switch (meta.element_type) {
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16:
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16:
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16:
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32:
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64:
                break;
            default:
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': model expects unsupported ONNX type '%s'",
                    meta.name.c_str(), onnxElementTypeName(meta.element_type));
                return -1;
        }
    } else if (buffer->getElementType() != expected) {
        xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
            "input tensor '%s': model expects ONNX type '%s', but input buffer has type '%s'",
            meta.name.c_str(), onnxElementTypeName(meta.element_type),
            qore_buffer_element_type_name(buffer->getElementType()));
        return -1;
    }
    // Materialize device storage to host for the host binding path; the zero-copy
    // device path passes require_host_storage=false to avoid the host copy.
    if (require_host_storage && buffer->ensureHostStorage(xsink)) {
        return -1;
    }

    int64_t total_elements = tensorShapeElementCount(shape, xsink);
    if (*xsink) {
        return -1;
    }
    if (total_elements != static_cast<int64_t>(buffer->size())) {
        xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
            "input tensor '%s': shape expects " QLLD " elements, got %zu",
            meta.name.c_str(), total_elements, buffer->size());
        return -1;
    }
    return 0;
}

std::unique_ptr<OnnxBoundOrtValue> QoreOnnxModel::prepareInputValue(const TensorMeta& meta,
        QoreValue val, ExceptionSink* xsink) {
    auto bound = std::make_unique<OnnxBoundOrtValue>();
    const QoreBufferNode* direct_buffer = nullptr;
    std::vector<int64_t> direct_shape;

    if (isTensorObject(val)) {
        QoreObject* obj = val.get<QoreObject>();
        QoreTensor* tensor = static_cast<QoreTensor*>(obj->getReferencedPrivateData(CID_TENSOR, xsink));
        if (*xsink) {
            return nullptr;
        }
        bound->holdTensor(tensor);
        direct_buffer = tensor->getBuffer();
        direct_shape = tensor->getShape();
        if (validateDirectTensorShape(meta, direct_shape, "ML::Tensor", xsink)) {
            return nullptr;
        }
    } else if (val.getType() == NT_BUFFER) {
        direct_buffer = val.get<const QoreBufferNode>();
        bound->holdValue(val);
        if (resolveFlatBufferShape(meta, direct_buffer->size(), direct_shape, xsink)) {
            return nullptr;
        }
    }

    // Device-resident input: bind the ONNX tensor directly over the provider
    // device pointer (no host copy) when the device matches the active provider
    // and the dtype is directly wrappable; otherwise fall back to materializing
    // the buffer to host (unless require_zero_copy_inputs forbids it).
    if (direct_buffer && direct_buffer->hasExternalDeviceStorage()) {
        const QoreBufferDeviceInfo* dinfo = direct_buffer->getDeviceInfo();
        bool can_zero_copy = dinfo && inputDeviceMatchesProvider(*dinfo)
            && onnxTypeCanWrapExternalOutput(meta.element_type);
        if (can_zero_copy) {
            if (validateDirectBuffer(meta, direct_buffer, direct_shape, xsink, false)) {
                return nullptr;
            }
            bound->shape = direct_shape;
            Ort::MemoryInfo dev_mem = makeOrtMemoryInfo(dinfo, xsink);
            if (*xsink) {
                return nullptr;
            }
            bound->value = createDeviceInputTensor(meta.element_type, dev_mem,
                direct_buffer->getDeviceData(), direct_buffer->size(), bound->shape);
            db_zero_copy_inputs.fetch_add(1, std::memory_order_relaxed);
            return bound;
        }
        if (device_binding.require_zero_copy_inputs) {
            xsink->raiseException("ML-ONNX-DEVICE-BINDING-ERROR",
                "input tensor '%s' on a '%s' device cannot be bound zero-copy to the active "
                "provider '%s' (dtype or device mismatch); require_zero_copy_inputs is set",
                meta.name.c_str(), dinfo ? qore_buffer_device_kind_name(dinfo->kind) : "unknown",
                active_provider.empty() ? "CPUExecutionProvider" : active_provider.c_str());
            return nullptr;
        }
        // host fallback: the host path below materializes the device buffer to host
        // (a device->host transfer) so the existing CPU binding path can be used
        db_host_fallback_inputs.fetch_add(1, std::memory_order_relaxed);
        db_device_to_host_transfers.fetch_add(1, std::memory_order_relaxed);
    }

    Ort::MemoryInfo mem_info = makeOrtMemoryInfo(nullptr, xsink);
    if (*xsink) {
        return nullptr;
    }
    if (direct_buffer) {
        if (validateDirectBuffer(meta, direct_buffer, direct_shape, xsink)) {
            return nullptr;
        }
        bound->shape = direct_shape;

        switch (meta.element_type) {
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16:
                bound->float16s = std::make_unique<std::vector<Ort::Float16_t>>();
                if (bufferToFloat16Vector(*direct_buffer, *bound->float16s, xsink)) {
                    return nullptr;
                }
                bound->value = Ort::Value::CreateTensor<Ort::Float16_t>(mem_info,
                    bound->float16s->data(), bound->float16s->size(), bound->shape.data(), bound->shape.size());
                return bound;
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16:
                bound->bfloat16s = std::make_unique<std::vector<Ort::BFloat16_t>>();
                if (bufferToBFloat16Vector(*direct_buffer, *bound->bfloat16s, xsink)) {
                    return nullptr;
                }
                bound->value = Ort::Value::CreateTensor<Ort::BFloat16_t>(mem_info,
                    bound->bfloat16s->data(), bound->bfloat16s->size(), bound->shape.data(), bound->shape.size());
                return bound;
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
                bound->value = Ort::Value::CreateTensor<float>(mem_info,
                    const_cast<float*>(static_cast<const float*>(direct_buffer->getRawData())),
                    direct_buffer->size(), bound->shape.data(), bound->shape.size());
                return bound;
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
                bound->value = Ort::Value::CreateTensor<double>(mem_info,
                    const_cast<double*>(static_cast<const double*>(direct_buffer->getRawData())),
                    direct_buffer->size(), bound->shape.data(), bound->shape.size());
                return bound;
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
                bound->value = Ort::Value::CreateTensor<int8_t>(mem_info,
                    const_cast<int8_t*>(static_cast<const int8_t*>(direct_buffer->getRawData())),
                    direct_buffer->size(), bound->shape.data(), bound->shape.size());
                return bound;
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16:
                bound->value = Ort::Value::CreateTensor<int16_t>(mem_info,
                    const_cast<int16_t*>(static_cast<const int16_t*>(direct_buffer->getRawData())),
                    direct_buffer->size(), bound->shape.data(), bound->shape.size());
                return bound;
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
                bound->value = Ort::Value::CreateTensor<int32_t>(mem_info,
                    const_cast<int32_t*>(static_cast<const int32_t*>(direct_buffer->getRawData())),
                    direct_buffer->size(), bound->shape.data(), bound->shape.size());
                return bound;
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
                bound->value = Ort::Value::CreateTensor<int64_t>(mem_info,
                    const_cast<int64_t*>(static_cast<const int64_t*>(direct_buffer->getRawData())),
                    direct_buffer->size(), bound->shape.data(), bound->shape.size());
                return bound;
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:
                bound->uint8s = std::make_unique<std::vector<uint8_t>>();
                if (bufferToUnsignedVector(*direct_buffer, *bound->uint8s, std::numeric_limits<uint8_t>::max(),
                        "uint8", xsink)) {
                    return nullptr;
                }
                bound->value = Ort::Value::CreateTensor<uint8_t>(mem_info,
                    bound->uint8s->data(), bound->uint8s->size(), bound->shape.data(), bound->shape.size());
                return bound;
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16:
                bound->uint16s = std::make_unique<std::vector<uint16_t>>();
                if (bufferToUnsignedVector(*direct_buffer, *bound->uint16s, std::numeric_limits<uint16_t>::max(),
                        "uint16", xsink)) {
                    return nullptr;
                }
                bound->value = Ort::Value::CreateTensor<uint16_t>(mem_info,
                    bound->uint16s->data(), bound->uint16s->size(), bound->shape.data(), bound->shape.size());
                return bound;
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32:
                bound->uint32s = std::make_unique<std::vector<uint32_t>>();
                if (bufferToUnsignedVector(*direct_buffer, *bound->uint32s, std::numeric_limits<uint32_t>::max(),
                        "uint32", xsink)) {
                    return nullptr;
                }
                bound->value = Ort::Value::CreateTensor<uint32_t>(mem_info,
                    bound->uint32s->data(), bound->uint32s->size(), bound->shape.data(), bound->shape.size());
                return bound;
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64:
                bound->uint64s = std::make_unique<std::vector<uint64_t>>();
                if (bufferToUnsignedVector(*direct_buffer, *bound->uint64s, std::numeric_limits<uint64_t>::max(),
                        "uint64", xsink)) {
                    return nullptr;
                }
                bound->value = Ort::Value::CreateTensor<uint64_t>(mem_info,
                    bound->uint64s->data(), bound->uint64s->size(), bound->shape.data(), bound->shape.size());
                return bound;
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL: {
                bound->bool_bytes = std::make_unique<std::vector<uint8_t>>();
                bound->bool_bytes->reserve(direct_buffer->size());
                for (size_t i = 0; i < direct_buffer->size(); ++i) {
                    if (i && !(i % 100) && qore_check_cancel(xsink, "converting bool tensor input")) {
                        return nullptr;
                    }
                    bound->bool_bytes->push_back(direct_buffer->getReferencedEntry(i, xsink).getAsBool() ? 1 : 0);
                    if (*xsink) {
                        return nullptr;
                    }
                }
                bound->value = Ort::Value::CreateTensor<bool>(mem_info,
                    reinterpret_cast<bool*>(bound->bool_bytes->data()), bound->bool_bytes->size(),
                    bound->shape.data(), bound->shape.size());
                return bound;
            }
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING: {
                bound->strings = std::make_unique<std::vector<std::string>>();
                bound->strings->reserve(direct_buffer->size());
                bound->string_ptrs.reserve(direct_buffer->size());
                for (size_t i = 0; i < direct_buffer->size(); ++i) {
                    if (i && !(i % 100) && qore_check_cancel(xsink, "converting string tensor input")) {
                        return nullptr;
                    }
                    ValueHolder entry(direct_buffer->getReferencedEntry(i, xsink), xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (entry->getType() != NT_STRING) {
                        xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                            "input tensor '%s': string buffer element %zu has type '%s'; expected string",
                            meta.name.c_str(), i, entry->getFullTypeName());
                        return nullptr;
                    }
                    QoreStringValueHelper str(*entry);
                    bound->strings->push_back(str->c_str());
                }
                for (size_t i = 0; i < bound->strings->size(); ++i) {
                    if (i && !(i % 100) && qore_check_cancel(xsink, "preparing string tensor input")) {
                        return nullptr;
                    }
                    bound->string_ptrs.push_back((*bound->strings)[i].c_str());
                }
                bound->value = Ort::Value::CreateTensor(allocator, bound->shape.data(),
                    bound->shape.size(), ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING);
                bound->value.FillStringTensor(bound->string_ptrs.data(), bound->string_ptrs.size());
                return bound;
            }
            default:
                assert(false);
        }
    }

    bound->shape = inferShape(val, meta, xsink);
    if (*xsink) {
        return nullptr;
    }
    int64_t total_elements = tensorShapeElementCount(bound->shape, xsink);
    if (*xsink) {
        return nullptr;
    }

    switch (meta.element_type) {
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
            bound->floats = std::make_unique<std::vector<float>>();
            flattenToFloats(val, *bound->floats, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (static_cast<int64_t>(bound->floats->size()) != total_elements) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': expected " QLLD " elements, got %zu",
                    meta.name.c_str(), total_elements, bound->floats->size());
                return nullptr;
            }
            bound->value = Ort::Value::CreateTensor<float>(mem_info, bound->floats->data(), bound->floats->size(),
                bound->shape.data(), bound->shape.size());
            return bound;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
            bound->doubles = std::make_unique<std::vector<double>>();
            flattenToDoubles(val, *bound->doubles, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (static_cast<int64_t>(bound->doubles->size()) != total_elements) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': expected " QLLD " elements, got %zu",
                    meta.name.c_str(), total_elements, bound->doubles->size());
                return nullptr;
            }
            bound->value = Ort::Value::CreateTensor<double>(mem_info, bound->doubles->data(), bound->doubles->size(),
                bound->shape.data(), bound->shape.size());
            return bound;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16:
            bound->float16s = std::make_unique<std::vector<Ort::Float16_t>>();
            flattenToFloat16(val, *bound->float16s, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (static_cast<int64_t>(bound->float16s->size()) != total_elements) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': expected " QLLD " elements, got %zu",
                    meta.name.c_str(), total_elements, bound->float16s->size());
                return nullptr;
            }
            bound->value = Ort::Value::CreateTensor<Ort::Float16_t>(mem_info, bound->float16s->data(),
                bound->float16s->size(), bound->shape.data(), bound->shape.size());
            return bound;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16:
            bound->bfloat16s = std::make_unique<std::vector<Ort::BFloat16_t>>();
            flattenToBFloat16(val, *bound->bfloat16s, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (static_cast<int64_t>(bound->bfloat16s->size()) != total_elements) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': expected " QLLD " elements, got %zu",
                    meta.name.c_str(), total_elements, bound->bfloat16s->size());
                return nullptr;
            }
            bound->value = Ort::Value::CreateTensor<Ort::BFloat16_t>(mem_info, bound->bfloat16s->data(),
                bound->bfloat16s->size(), bound->shape.data(), bound->shape.size());
            return bound;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
            bound->int8s = std::make_unique<std::vector<int8_t>>();
            flattenToInt8(val, *bound->int8s, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (static_cast<int64_t>(bound->int8s->size()) != total_elements) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': expected " QLLD " elements, got %zu",
                    meta.name.c_str(), total_elements, bound->int8s->size());
                return nullptr;
            }
            bound->value = Ort::Value::CreateTensor<int8_t>(mem_info, bound->int8s->data(), bound->int8s->size(),
                bound->shape.data(), bound->shape.size());
            return bound;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:
            bound->uint8s = std::make_unique<std::vector<uint8_t>>();
            flattenToUInt8(val, *bound->uint8s, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (static_cast<int64_t>(bound->uint8s->size()) != total_elements) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': expected " QLLD " elements, got %zu",
                    meta.name.c_str(), total_elements, bound->uint8s->size());
                return nullptr;
            }
            bound->value = Ort::Value::CreateTensor<uint8_t>(mem_info, bound->uint8s->data(), bound->uint8s->size(),
                bound->shape.data(), bound->shape.size());
            return bound;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16:
            bound->int16s = std::make_unique<std::vector<int16_t>>();
            flattenToInt16(val, *bound->int16s, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (static_cast<int64_t>(bound->int16s->size()) != total_elements) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': expected " QLLD " elements, got %zu",
                    meta.name.c_str(), total_elements, bound->int16s->size());
                return nullptr;
            }
            bound->value = Ort::Value::CreateTensor<int16_t>(mem_info, bound->int16s->data(), bound->int16s->size(),
                bound->shape.data(), bound->shape.size());
            return bound;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16:
            bound->uint16s = std::make_unique<std::vector<uint16_t>>();
            flattenToUInt16(val, *bound->uint16s, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (static_cast<int64_t>(bound->uint16s->size()) != total_elements) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': expected " QLLD " elements, got %zu",
                    meta.name.c_str(), total_elements, bound->uint16s->size());
                return nullptr;
            }
            bound->value = Ort::Value::CreateTensor<uint16_t>(mem_info, bound->uint16s->data(),
                bound->uint16s->size(), bound->shape.data(), bound->shape.size());
            return bound;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
            bound->int32s = std::make_unique<std::vector<int32_t>>();
            flattenToInt32(val, *bound->int32s, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (static_cast<int64_t>(bound->int32s->size()) != total_elements) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': expected " QLLD " elements, got %zu",
                    meta.name.c_str(), total_elements, bound->int32s->size());
                return nullptr;
            }
            bound->value = Ort::Value::CreateTensor<int32_t>(mem_info, bound->int32s->data(), bound->int32s->size(),
                bound->shape.data(), bound->shape.size());
            return bound;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32:
            bound->uint32s = std::make_unique<std::vector<uint32_t>>();
            flattenToUInt32(val, *bound->uint32s, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (static_cast<int64_t>(bound->uint32s->size()) != total_elements) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': expected " QLLD " elements, got %zu",
                    meta.name.c_str(), total_elements, bound->uint32s->size());
                return nullptr;
            }
            bound->value = Ort::Value::CreateTensor<uint32_t>(mem_info, bound->uint32s->data(),
                bound->uint32s->size(), bound->shape.data(), bound->shape.size());
            return bound;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
            bound->int64s = std::make_unique<std::vector<int64_t>>();
            flattenToInt64(val, *bound->int64s, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (static_cast<int64_t>(bound->int64s->size()) != total_elements) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': expected " QLLD " elements, got %zu",
                    meta.name.c_str(), total_elements, bound->int64s->size());
                return nullptr;
            }
            bound->value = Ort::Value::CreateTensor<int64_t>(mem_info, bound->int64s->data(), bound->int64s->size(),
                bound->shape.data(), bound->shape.size());
            return bound;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64:
            bound->uint64s = std::make_unique<std::vector<uint64_t>>();
            flattenToUInt64(val, *bound->uint64s, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (static_cast<int64_t>(bound->uint64s->size()) != total_elements) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': expected " QLLD " elements, got %zu",
                    meta.name.c_str(), total_elements, bound->uint64s->size());
                return nullptr;
            }
            bound->value = Ort::Value::CreateTensor<uint64_t>(mem_info, bound->uint64s->data(),
                bound->uint64s->size(), bound->shape.data(), bound->shape.size());
            return bound;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL:
            bound->bool_bytes = std::make_unique<std::vector<uint8_t>>();
            flattenToBools(val, *bound->bool_bytes, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (static_cast<int64_t>(bound->bool_bytes->size()) != total_elements) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': expected " QLLD " elements, got %zu",
                    meta.name.c_str(), total_elements, bound->bool_bytes->size());
                return nullptr;
            }
            bound->value = Ort::Value::CreateTensor<bool>(mem_info,
                reinterpret_cast<bool*>(bound->bool_bytes->data()), bound->bool_bytes->size(),
                bound->shape.data(), bound->shape.size());
            return bound;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING:
            bound->strings = std::make_unique<std::vector<std::string>>();
            flattenToStrings(val, *bound->strings, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (static_cast<int64_t>(bound->strings->size()) != total_elements) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': expected " QLLD " elements, got %zu",
                    meta.name.c_str(), total_elements, bound->strings->size());
                return nullptr;
            }
            bound->string_ptrs.reserve(bound->strings->size());
            for (size_t i = 0; i < bound->strings->size(); ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "preparing string tensor input")) {
                    return nullptr;
                }
                bound->string_ptrs.push_back((*bound->strings)[i].c_str());
            }
            bound->value = Ort::Value::CreateTensor(allocator, bound->shape.data(),
                bound->shape.size(), ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING);
            bound->value.FillStringTensor(bound->string_ptrs.data(), bound->string_ptrs.size());
            return bound;
        default:
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "unsupported input tensor type '%s' for tensor '%s'",
                elementTypeToString(meta.element_type), meta.name.c_str());
            return nullptr;
    }
}

std::unique_ptr<OnnxBoundOrtValue> QoreOnnxModel::prepareOutputTensorValue(const TensorMeta& meta,
        const QoreObject* tensor_obj, ExceptionSink* xsink) {
    QoreTensor* tensor = static_cast<QoreTensor*>(
        tensor_obj->getReferencedPrivateData(CID_TENSOR, xsink));
    if (*xsink) {
        return nullptr;
    }

    auto bound = std::make_unique<OnnxBoundOrtValue>();
    bound->holdTensor(tensor);
    QoreBufferNode* buffer = tensor->getMutableBuffer();
    std::vector<int64_t> shape = tensor->getShape();

    if (meta.shape.size() != shape.size()) {
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "output tensor '%s': model expects rank %zu, preallocated Tensor has rank %zu",
            meta.name.c_str(), meta.shape.size(), shape.size());
        return nullptr;
    }
    for (size_t i = 0; i < meta.shape.size(); ++i) {
        if (meta.shape[i] > 0 && meta.shape[i] != shape[i]) {
            xsink->raiseException("ML-ONNX-BINDING-ERROR",
                "output tensor '%s': model expects dimension %zu to be " QLLD ", preallocated Tensor has " QLLD,
                meta.name.c_str(), i, meta.shape[i], shape[i]);
            return nullptr;
        }
    }
    QoreBufferElementType expected = onnxTypeToBufferElementType(meta.element_type);
    if (expected == QoreBufferElementType::Invalid || buffer->getElementType() != expected) {
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "output tensor '%s': model output type is '%s', but preallocated Tensor buffer type is '%s'",
            meta.name.c_str(), elementTypeToString(meta.element_type),
            qore_buffer_element_type_name(buffer->getElementType()));
        return nullptr;
    }
    if (buffer->hasNullableElements()) {
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "output tensor '%s': preallocated Tensor buffer must not be nullable",
            meta.name.c_str());
        return nullptr;
    }
    if (meta.element_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL
            || meta.element_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING) {
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "output tensor '%s': preallocated output binding does not support %s tensors; use bindOutput()",
            meta.name.c_str(), elementTypeToString(meta.element_type));
        return nullptr;
    }

    if (buffer->hasExternalStorage() || buffer->hasExternalDeviceStorage()) {
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "output tensor '%s': preallocated output binding requires a mutable Qore-owned Tensor buffer; "
            "the supplied Tensor buffer wraps immutable external storage",
            meta.name.c_str());
        return nullptr;
    }
    if (buffer->ensureHostStorage(xsink)) {
        return nullptr;
    }
    int64_t total_elements = tensorShapeElementCount(shape, xsink);
    if (*xsink) {
        return nullptr;
    }
    if (total_elements != static_cast<int64_t>(buffer->size())) {
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "output tensor '%s': shape expects " QLLD " elements, got %zu",
            meta.name.c_str(), total_elements, buffer->size());
        return nullptr;
    }

    bound->shape = std::move(shape);
    Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    size_t bytes = qore_buffer_element_storage_size(buffer->getElementType()) * buffer->size();
    bound->value = Ort::Value::CreateTensor(mem_info, buffer->getRawData(), bytes,
        bound->shape.data(), bound->shape.size(), meta.element_type);
    return bound;
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

QoreValue QoreOnnxModel::reshapeOutputFloat16(const Ort::Float16_t* data,
        const std::vector<int64_t>& shape, size_t& offset, ExceptionSink* xsink) {
    return reshapeFloatingOutput(data, shape, offset, "reshaping ONNX float16 tensor output", xsink);
}

QoreValue QoreOnnxModel::reshapeOutputBFloat16(const Ort::BFloat16_t* data,
        const std::vector<int64_t>& shape, size_t& offset, ExceptionSink* xsink) {
    return reshapeFloatingOutput(data, shape, offset, "reshaping ONNX bfloat16 tensor output", xsink);
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

QoreValue QoreOnnxModel::reshapeOutputUInt32(const uint32_t* data,
        const std::vector<int64_t>& shape, size_t& offset, ExceptionSink* xsink) {
    return reshapeUnsignedOutput(data, shape, offset, "reshaping ONNX uint32 tensor output", xsink);
}

QoreValue QoreOnnxModel::reshapeOutputUInt16(const uint16_t* data,
        const std::vector<int64_t>& shape, size_t& offset, ExceptionSink* xsink) {
    return reshapeUnsignedOutput(data, shape, offset, "reshaping ONNX uint16 tensor output", xsink);
}

QoreValue QoreOnnxModel::reshapeOutputUInt8(const uint8_t* data,
        const std::vector<int64_t>& shape, size_t& offset, ExceptionSink* xsink) {
    return reshapeUnsignedOutput(data, shape, offset, "reshaping ONNX uint8 tensor output", xsink);
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

QoreValue QoreOnnxModel::reshapeOutputUInt64(const uint64_t* data,
        const std::vector<int64_t>& shape, size_t& offset, ExceptionSink* xsink) {
    return reshapeUnsignedOutput(data, shape, offset, "reshaping ONNX uint64 tensor output", xsink);
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

QoreValue QoreOnnxModel::reshapeOutputString(Ort::Value& tensor,
        const std::vector<int64_t>& shape, size_t& offset, ExceptionSink* xsink) {
    if (shape.empty() || (shape.size() == 1 && shape[0] == 1)) {
        return new QoreStringNode(tensor.GetStringTensorElement(offset++));
    }
    if (shape.size() == 1) {
        ReferenceHolder<QoreListNode> list(new QoreListNode(stringTypeInfo), xsink);
        for (int64_t i = 0; i < shape[0]; ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "reshaping ONNX string tensor output")) {
                return QoreValue();
            }
            list->push(new QoreStringNode(tensor.GetStringTensorElement(offset++)), xsink);
        }
        return list.release();
    }
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);
    std::vector<int64_t> inner_shape(shape.begin() + 1, shape.end());
    for (int64_t i = 0; i < shape[0]; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "reshaping ONNX string tensor output")) {
            return QoreValue();
        }
        QoreValue value = reshapeOutputString(tensor, inner_shape, offset, xsink);
        if (*xsink) {
            return QoreValue();
        }
        list->push(value, xsink);
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
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16: {
            const Ort::Float16_t* data = tensor.GetTensorData<Ort::Float16_t>();
            return reshapeOutputFloat16(data, shape, offset, xsink);
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16: {
            const Ort::BFloat16_t* data = tensor.GetTensorData<Ort::BFloat16_t>();
            return reshapeOutputBFloat16(data, shape, offset, xsink);
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: {
            const int64_t* data = tensor.GetTensorData<int64_t>();
            return reshapeOutputInt64(data, shape, offset);
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64: {
            const uint64_t* data = tensor.GetTensorData<uint64_t>();
            return reshapeOutputUInt64(data, shape, offset, xsink);
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: {
            const int32_t* data = tensor.GetTensorData<int32_t>();
            return reshapeOutputInt32(data, shape, offset);
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32: {
            const uint32_t* data = tensor.GetTensorData<uint32_t>();
            return reshapeOutputUInt32(data, shape, offset, xsink);
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16: {
            const int16_t* data = tensor.GetTensorData<int16_t>();
            return reshapeOutputInt16(data, shape, offset, xsink);
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16: {
            const uint16_t* data = tensor.GetTensorData<uint16_t>();
            return reshapeOutputUInt16(data, shape, offset, xsink);
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8: {
            const int8_t* data = tensor.GetTensorData<int8_t>();
            return reshapeOutputInt8(data, shape, offset, xsink);
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: {
            const uint8_t* data = tensor.GetTensorData<uint8_t>();
            return reshapeOutputUInt8(data, shape, offset, xsink);
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL: {
            const bool* data = tensor.GetTensorData<bool>();
            return reshapeOutputBool(data, shape, offset, xsink);
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING:
            return reshapeOutputString(tensor, shape, offset, xsink);
        default:
            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                "unsupported output tensor type '%s'", elementTypeToString(type));
            return QoreValue();
    }
}

QoreValue QoreOnnxModel::convertOutputTensorToTensor(Ort::Value&& tensor, ExceptionSink* xsink) {
    if (!tensor.IsTensor()) {
        xsink->raiseException("ML-ONNX-INFERENCE-ERROR", "output is not a tensor");
        return QoreValue();
    }

    auto tensor_info = tensor.GetTensorTypeAndShapeInfo();
    ONNXTensorElementDataType type = tensor_info.GetElementType();
    std::vector<int64_t> shape = tensor_info.GetShape();
    QoreBufferElementType buffer_type = onnxTypeToBufferElementType(type);
    if (buffer_type == QoreBufferElementType::Invalid) {
        switch (type) {
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16:
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16:
                buffer_type = QoreBufferElementType::Float32;
                break;
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16:
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32:
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64:
                buffer_type = QoreBufferElementType::Int64;
                break;
            default:
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "unsupported output tensor type '%s'", elementTypeToString(type));
                return QoreValue();
        }
    }

    int64_t total_elements = tensorShapeElementCount(shape, xsink);
    if (*xsink) {
        return QoreValue();
    }
    if (static_cast<uint64_t>(total_elements) > std::numeric_limits<size_t>::max()) {
        xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
            "output tensor element count " QLLD " exceeds this platform's addressable buffer size",
            total_elements);
        return QoreValue();
    }

    // Provider-managed device output: wrap the device allocation as a device-backed
    // buffer rather than copying through host memory (Phase C).
    QoreBufferDeviceInfo dev_info;
    if (ortValueToDeviceInfo(tensor, dev_info)) {
        ReferenceHolder<QoreBufferNode> buffer(
            wrapOnnxDeviceOutput(std::move(tensor), type, buffer_type,
                static_cast<size_t>(total_elements), dev_info, xsink), xsink);
        if (*xsink) {
            return QoreValue();
        }
        db_device_outputs.fetch_add(1, std::memory_order_relaxed);
        if (device_binding.materialize_outputs) {
            if (buffer->ensureHostStorage(xsink)) {
                return QoreValue();
            }
            db_output_materializations.fetch_add(1, std::memory_order_relaxed);
            db_device_to_host_transfers.fetch_add(1, std::memory_order_relaxed);
        }
        ReferenceHolder<QoreTensor> qore_tensor(new QoreTensor(buffer.release(), std::move(shape)), xsink);
        return qore_ml_tensor_to_object(qore_tensor.release(), getProgram(), xsink);
    }

    // any tensor output that is not provider device memory is returned as host memory
    db_host_outputs.fetch_add(1, std::memory_order_relaxed);

    if (onnxTypeCanWrapExternalOutput(type)) {
        std::shared_ptr<Ort::Value> owner = std::make_shared<Ort::Value>(std::move(tensor));
        const void* data = total_elements ? getOnnxTensorDataPointer(*owner, type) : nullptr;
        ReferenceHolder<QoreBufferNode> buffer(
            QoreBufferNode::wrapExternalStorage(buffer_type, false, static_cast<size_t>(total_elements),
                data, nullptr, owner, 0, xsink), xsink);
        if (*xsink) {
            return QoreValue();
        }
        ReferenceHolder<QoreTensor> qore_tensor(new QoreTensor(buffer.release(), std::move(shape)), xsink);
        return qore_ml_tensor_to_object(qore_tensor.release(), getProgram(), xsink);
    }

    if (type == ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING) {
        ReferenceHolder<QoreListNode> flat(new QoreListNode(stringTypeInfo), xsink);
        for (int64_t i = 0; i < total_elements; ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "converting ONNX string tensor output")) {
                return QoreValue();
            }
            flat->push(new QoreStringNode(tensor.GetStringTensorElement(static_cast<size_t>(i))), xsink);
        }
        ReferenceHolder<QoreBufferNode> string_buffer(
            new QoreBufferNode(QoreBufferElementType::String, false, *flat, xsink), xsink);
        if (*xsink) {
            return QoreValue();
        }
        ReferenceHolder<QoreTensor> qore_tensor(
            new QoreTensor(string_buffer.release(), std::move(shape)), xsink);
        return qore_ml_tensor_to_object(qore_tensor.release(), getProgram(), xsink);
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
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16: {
            const Ort::Float16_t* data = tensor.GetTensorData<Ort::Float16_t>();
            float* dst = static_cast<float*>((*buffer)->getRawData());
            for (int64_t i = 0; i < total_elements; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "converting ONNX float16 tensor output")) {
                    return QoreValue();
                }
                dst[i] = data[i].ToFloat();
            }
            break;
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16: {
            const Ort::BFloat16_t* data = tensor.GetTensorData<Ort::BFloat16_t>();
            float* dst = static_cast<float*>((*buffer)->getRawData());
            for (int64_t i = 0; i < total_elements; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "converting ONNX bfloat16 tensor output")) {
                    return QoreValue();
                }
                dst[i] = data[i].ToFloat();
            }
            break;
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: {
            const int32_t* data = tensor.GetTensorData<int32_t>();
            std::memcpy((*buffer)->getRawData(), data, sizeof(int32_t) * static_cast<size_t>(total_elements));
            break;
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32: {
            const uint32_t* data = tensor.GetTensorData<uint32_t>();
            int64_t* dst = static_cast<int64_t*>((*buffer)->getRawData());
            for (int64_t i = 0; i < total_elements; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "converting ONNX uint32 tensor output")) {
                    return QoreValue();
                }
                dst[i] = static_cast<int64_t>(data[i]);
            }
            break;
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16: {
            const int16_t* data = tensor.GetTensorData<int16_t>();
            std::memcpy((*buffer)->getRawData(), data, sizeof(int16_t) * static_cast<size_t>(total_elements));
            break;
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16: {
            const uint16_t* data = tensor.GetTensorData<uint16_t>();
            int64_t* dst = static_cast<int64_t*>((*buffer)->getRawData());
            for (int64_t i = 0; i < total_elements; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "converting ONNX uint16 tensor output")) {
                    return QoreValue();
                }
                dst[i] = static_cast<int64_t>(data[i]);
            }
            break;
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8: {
            const int8_t* data = tensor.GetTensorData<int8_t>();
            std::memcpy((*buffer)->getRawData(), data, sizeof(int8_t) * static_cast<size_t>(total_elements));
            break;
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: {
            const uint8_t* data = tensor.GetTensorData<uint8_t>();
            int64_t* dst = static_cast<int64_t*>((*buffer)->getRawData());
            for (int64_t i = 0; i < total_elements; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "converting ONNX uint8 tensor output")) {
                    return QoreValue();
                }
                dst[i] = static_cast<int64_t>(data[i]);
            }
            break;
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: {
            const int64_t* data = tensor.GetTensorData<int64_t>();
            std::memcpy((*buffer)->getRawData(), data, sizeof(int64_t) * static_cast<size_t>(total_elements));
            break;
        }
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64: {
            const uint64_t* data = tensor.GetTensorData<uint64_t>();
            int64_t* dst = static_cast<int64_t*>((*buffer)->getRawData());
            for (int64_t i = 0; i < total_elements; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "converting ONNX uint64 tensor output")) {
                    return QoreValue();
                }
                if (data[i] > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                    xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                        "uint64 output tensor value %llu exceeds Qore ML::Tensor int64 range",
                        static_cast<unsigned long long>(data[i]));
                    return QoreValue();
                }
                dst[i] = static_cast<int64_t>(data[i]);
            }
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
    return run(inputs, nullptr, xsink);
}

QoreHashNode* QoreOnnxModel::run(const QoreHashNode* inputs, const QoreListNode* output_names,
        ExceptionSink* xsink) {
    return runImpl(inputs, false, output_names, xsink);
}

QoreHashNode* QoreOnnxModel::runTensors(const QoreHashNode* inputs, ExceptionSink* xsink) {
    return runTensors(inputs, nullptr, xsink);
}

QoreHashNode* QoreOnnxModel::runTensors(const QoreHashNode* inputs, const QoreListNode* output_names,
        ExceptionSink* xsink) {
    return runImpl(inputs, true, output_names, xsink);
}

QoreObject* QoreOnnxModel::createBinding(QoreProgram* pgm, ExceptionSink* xsink) {
    if (!session) {
        xsink->raiseException("ML-ONNX-ERROR", "model is not loaded");
        return nullptr;
    }
    ref();
    ReferenceHolder<QoreOnnxIoBinding> holder(new QoreOnnxIoBinding(this, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    return qore_ml_onnx_io_binding_to_object(holder.release(), pgm, xsink);
}

QoreHashNode* QoreOnnxModel::runBound(const QoreObject* binding_obj, const QoreObject* options_obj,
        bool return_tensors, ExceptionSink* xsink) {
    if (!session) {
        xsink->raiseException("ML-ONNX-ERROR", "model is not loaded");
        return nullptr;
    }
    QoreOnnxIoBinding* binding = static_cast<QoreOnnxIoBinding*>(
        binding_obj->getReferencedPrivateData(CID_ONNXIOBINDING, xsink));
    if (*xsink) {
        return nullptr;
    }
    ReferenceHolder<QoreOnnxIoBinding> binding_holder(binding, xsink);
    return binding->run(options_obj, return_tensors, xsink);
}

QoreStringNode* QoreOnnxModel::endProfiling(ExceptionSink* xsink) {
    if (!session) {
        xsink->raiseException("ML-ONNX-ERROR", "model is not loaded");
        return nullptr;
    }
    try {
        Ort::AllocatedStringPtr profile_file = session->EndProfilingAllocated(allocator);
        if (!profile_file || !profile_file.get() || !*profile_file.get()) {
            std::lock_guard<std::mutex> lock(stats_mutex);
            profiling_enabled = false;
            return nullptr;
        }
        std::string profile_path = profile_file.get();
        {
            std::lock_guard<std::mutex> lock(stats_mutex);
            last_profile_file = profile_path;
            profiling_enabled = false;
        }
        return new QoreStringNode(profile_path);
    } catch (const Ort::Exception& e) {
        xsink->raiseException("ML-ONNX-PROFILING-ERROR",
            "failed to end ONNX Runtime profiling: %s", e.what());
        return nullptr;
    }
}

void QoreOnnxModel::recordInference(double elapsed_ms, uint64_t batch_items) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    ++inference_run_count;
    inference_batch_items += batch_items;
    total_inference_ms += elapsed_ms;
    last_inference_ms = elapsed_ms;
    if (elapsed_ms > max_inference_ms) {
        max_inference_ms = elapsed_ms;
    }
}

QoreHashNode* QoreOnnxModel::getInferenceStats(ExceptionSink* xsink) const {
    uint64_t run_count;
    uint64_t batch_items;
    double total_ms;
    double last_ms;
    double max_ms;
    bool profiling_active;
    std::string profile_prefix;
    std::string profile_file;
    {
        std::lock_guard<std::mutex> lock(stats_mutex);
        run_count = inference_run_count;
        batch_items = inference_batch_items;
        total_ms = total_inference_ms;
        last_ms = last_inference_ms;
        max_ms = max_inference_ms;
        profiling_active = profiling_enabled;
        profile_prefix = profiling_file_prefix;
        profile_file = last_profile_file;
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    rv->setKeyValue("run_count", static_cast<int64_t>(run_count), xsink);
    rv->setKeyValue("batch_items", static_cast<int64_t>(batch_items), xsink);
    rv->setKeyValue("total_ms", total_ms, xsink);
    rv->setKeyValue("last_ms", last_ms, xsink);
    rv->setKeyValue("max_ms", max_ms, xsink);
    rv->setKeyValue("avg_ms", run_count ? total_ms / run_count : 0.0, xsink);
    rv->setKeyValue("profiling_enabled", profiling_active, xsink);
    rv->setKeyValue("profile_file_prefix", profile_prefix.empty()
        ? QoreValue() : QoreValue(new QoreStringNode(profile_prefix)), xsink);
    rv->setKeyValue("last_profile_file", profile_file.empty()
        ? QoreValue() : QoreValue(new QoreStringNode(profile_file)), xsink);

    // device-binding transfer counters: make host/device placement and materialization observable
    ReferenceHolder<QoreHashNode> db(new QoreHashNode(autoTypeInfo), xsink);
    db->setKeyValue("zero_copy_inputs",
        static_cast<int64_t>(db_zero_copy_inputs.load(std::memory_order_relaxed)), xsink);
    db->setKeyValue("host_fallback_inputs",
        static_cast<int64_t>(db_host_fallback_inputs.load(std::memory_order_relaxed)), xsink);
    db->setKeyValue("device_outputs",
        static_cast<int64_t>(db_device_outputs.load(std::memory_order_relaxed)), xsink);
    db->setKeyValue("host_outputs",
        static_cast<int64_t>(db_host_outputs.load(std::memory_order_relaxed)), xsink);
    db->setKeyValue("output_materializations",
        static_cast<int64_t>(db_output_materializations.load(std::memory_order_relaxed)), xsink);
    db->setKeyValue("host_to_device_transfers",
        static_cast<int64_t>(db_host_to_device_transfers.load(std::memory_order_relaxed)), xsink);
    db->setKeyValue("device_to_host_transfers",
        static_cast<int64_t>(db_device_to_host_transfers.load(std::memory_order_relaxed)), xsink);
    rv->setKeyValue("device_binding", db.release(), xsink);

    if (*xsink) {
        return nullptr;
    }
    return rv.release();
}

void QoreOnnxModel::resetInferenceStats() {
    std::lock_guard<std::mutex> lock(stats_mutex);
    inference_run_count = 0;
    inference_batch_items = 0;
    total_inference_ms = 0.0;
    last_inference_ms = 0.0;
    max_inference_ms = 0.0;
    db_zero_copy_inputs.store(0, std::memory_order_relaxed);
    db_host_fallback_inputs.store(0, std::memory_order_relaxed);
    db_device_outputs.store(0, std::memory_order_relaxed);
    db_host_outputs.store(0, std::memory_order_relaxed);
    db_output_materializations.store(0, std::memory_order_relaxed);
    db_host_to_device_transfers.store(0, std::memory_order_relaxed);
    db_device_to_host_transfers.store(0, std::memory_order_relaxed);
}

QoreHashNode* QoreOnnxModel::saveOptimized(const char* output_path, const QoreHashNode* config,
        bool ort_format, ExceptionSink* xsink) const {
    if (!output_path || !*output_path) {
        xsink->raiseException("ML-ONNX-ERROR",
            "optimized model output path must be a non-empty string");
        return nullptr;
    }

    bool effective_ort_format = ort_format || hasOrtSuffix(output_path)
        || configValueIsOrt(config, "save_model_format");
    ReferenceHolder<QoreHashNode> opt_config(buildOptimizationConfig(config, output_path,
        effective_ort_format, source_load_model_format.c_str(), xsink), xsink);
    if (*xsink) {
        return nullptr;
    }

    if (!source_model_path.empty()) {
        QoreOnnxModel optimizer(source_model_path.c_str(), *opt_config, xsink);
        if (*xsink) {
            return nullptr;
        }
        return makeOptimizationInfo(source_model_path.c_str(), output_path, effective_ort_format,
            optimizer, xsink);
    }

    if (!source_model_data.empty()) {
        QoreOnnxModel optimizer(source_model_data.data(), source_model_data.size(), *opt_config, xsink);
        if (*xsink) {
            return nullptr;
        }
        return makeOptimizationInfo(nullptr, output_path, effective_ort_format, optimizer, xsink);
    }

    xsink->raiseException("ML-ONNX-ERROR",
        "cannot save an optimized ONNX model because the original model source is not available");
    return nullptr;
}

QoreHashNode* qore_ml_onnx_optimize_model(const char* input_path, const char* output_path,
        const QoreHashNode* config, bool ort_format, ExceptionSink* xsink) {
    if (!input_path || !*input_path) {
        xsink->raiseException("ML-ONNX-ERROR",
            "input model path must be a non-empty string");
        return nullptr;
    }
    if (!output_path || !*output_path) {
        xsink->raiseException("ML-ONNX-ERROR",
            "optimized model output path must be a non-empty string");
        return nullptr;
    }

    bool effective_ort_format = ort_format || hasOrtSuffix(output_path)
        || configValueIsOrt(config, "save_model_format");
    ReferenceHolder<QoreHashNode> opt_config(buildOptimizationConfig(config, output_path,
        effective_ort_format, nullptr, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }

    QoreOnnxModel optimizer(input_path, *opt_config, xsink);
    if (*xsink) {
        return nullptr;
    }

    return makeOptimizationInfo(input_path, output_path, effective_ort_format, optimizer, xsink);
}

QoreHashNode* QoreOnnxModel::collectBoundOutputs(Ort::IoBinding& binding, bool return_tensors,
        ExceptionSink* xsink) {
    std::vector<std::string> names;
    std::vector<Ort::Value> values;
    try {
        names = binding.GetOutputNames(allocator);
        values = binding.GetOutputValues(allocator);
    } catch (const Ort::Exception& e) {
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "failed to collect bound ONNX outputs: %s", e.what());
        return nullptr;
    }

    if (names.size() != values.size()) {
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "ONNX Runtime returned %zu bound output names but %zu values",
            names.size(), values.size());
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    for (size_t i = 0; i < values.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "converting bound ONNX outputs")) {
            return nullptr;
        }
        QoreValue out_val;
        if (return_tensors) {
            out_val = convertOutputTensorToTensor(std::move(values[i]), xsink);
        } else {
            out_val = convertOutputTensor(values[i], xsink);
        }
        if (*xsink) {
            return nullptr;
        }
        rv->setKeyValue(names[i].c_str(), out_val, xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    return rv.release();
}

QoreOnnxIoBinding::QoreOnnxIoBinding(QoreOnnxModel* n_model, ExceptionSink* xsink)
        : model(n_model) {
    try {
        binding = std::make_unique<Ort::IoBinding>(*model->session);
    } catch (const Ort::Exception& e) {
        model->deref(xsink);
        model = nullptr;
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "failed to create ONNX Runtime I/O binding: %s", e.what());
    }
}

QoreOnnxIoBinding::~QoreOnnxIoBinding() {
    if (model) {
        ExceptionSink xsink;
        model->deref(&xsink);
    }
}

QoreOnnxRunOptions* QoreOnnxIoBinding::getRunOptions(const QoreObject* options_obj,
        ExceptionSink* xsink) const {
    if (!options_obj) {
        return nullptr;
    }
    return static_cast<QoreOnnxRunOptions*>(
        options_obj->getReferencedPrivateData(CID_ONNXRUNOPTIONS, xsink));
}

void QoreOnnxIoBinding::bindInput(const char* name, QoreValue value, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lock(mutex);
    bindInputUnlocked(name, value, xsink);
}

void QoreOnnxIoBinding::bindInputUnlocked(const char* name, QoreValue value, ExceptionSink* xsink) {
    const TensorMeta* meta = model->findInputMeta(name);
    if (!meta) {
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "unknown ONNX input tensor '%s'", name);
        return;
    }

    std::unique_ptr<OnnxBoundOrtValue> bound = model->prepareInputValue(*meta, value, xsink);
    if (*xsink) {
        return;
    }
    try {
        binding->BindInput(name, bound->value);
    } catch (const Ort::Exception& e) {
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "failed to bind ONNX input tensor '%s': %s", name, e.what());
        return;
    }

    bound_input_names.push_back(name);
    bound_inputs.push_back(std::move(bound));
}

void QoreOnnxIoBinding::bindInputs(const QoreHashNode* inputs, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lock(mutex);
    clearInputsUnlocked();
    for (const auto& meta : model->input_meta) {
        QoreValue val = inputs->getKeyValue(meta.name.c_str());
        if (val.isNullOrNothing()) {
            xsink->raiseException("ML-ONNX-BINDING-ERROR",
                "missing required input tensor '%s'", meta.name.c_str());
            return;
        }
        bindInputUnlocked(meta.name.c_str(), val, xsink);
        if (*xsink) {
            return;
        }
    }
}

void QoreOnnxIoBinding::bindOutput(const char* name, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lock(mutex);
    bindOutputUnlocked(name, xsink);
}

void QoreOnnxIoBinding::bindOutputUnlocked(const char* name, ExceptionSink* xsink) {
    const TensorMeta* meta = model->findOutputMeta(name);
    if (!meta) {
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "unknown ONNX output tensor '%s'", name);
        return;
    }
    Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    try {
        binding->BindOutput(name, mem_info);
    } catch (const Ort::Exception& e) {
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "failed to bind ONNX output tensor '%s': %s", name, e.what());
        return;
    }
    bound_output_names.push_back(name);
}

void QoreOnnxIoBinding::bindOutputs(ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lock(mutex);
    clearOutputsUnlocked();
    for (const auto& meta : model->output_meta) {
        bindOutputUnlocked(meta.name.c_str(), xsink);
        if (*xsink) {
            return;
        }
    }
}

void QoreOnnxIoBinding::bindOutputDevice(const char* name, const QoreHashNode* device,
        ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lock(mutex);
    bindOutputDeviceUnlocked(name, device, xsink);
}

void QoreOnnxIoBinding::bindOutputDeviceUnlocked(const char* name, const QoreHashNode* device,
        ExceptionSink* xsink) {
    const TensorMeta* meta = model->findOutputMeta(name);
    if (!meta) {
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "unknown ONNX output tensor '%s'", name);
        return;
    }

    QoreBufferDeviceInfo dev;
    bool use_device = model->resolveOutputDeviceInfo(device, dev, xsink);
    if (*xsink) {
        return;
    }
    if (!use_device) {
        // policy/host-fallback selected CPU output memory
        bindOutputUnlocked(name, xsink);
        return;
    }

    Ort::MemoryInfo mem_info = makeOrtMemoryInfo(&dev, xsink);
    if (*xsink) {
        return;
    }
    try {
        binding->BindOutput(name, mem_info);
    } catch (const Ort::Exception& e) {
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "failed to bind ONNX output tensor '%s' to %s device %lld: %s", name,
            qore_buffer_device_kind_name(dev.kind), (long long)dev.device_id, e.what());
        return;
    }
    bound_output_names.push_back(name);
}

void QoreOnnxIoBinding::bindOutputsDevice(const QoreHashNode* device, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lock(mutex);
    clearOutputsUnlocked();
    for (const auto& meta : model->output_meta) {
        bindOutputDeviceUnlocked(meta.name.c_str(), device, xsink);
        if (*xsink) {
            return;
        }
    }
}

void QoreOnnxIoBinding::bindOutputTensor(const char* name, const QoreObject* tensor_obj,
        ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lock(mutex);
    bindOutputTensorUnlocked(name, tensor_obj, xsink);
}

void QoreOnnxIoBinding::bindOutputTensorUnlocked(const char* name, const QoreObject* tensor_obj,
        ExceptionSink* xsink) {
    const TensorMeta* meta = model->findOutputMeta(name);
    if (!meta) {
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "unknown ONNX output tensor '%s'", name);
        return;
    }

    std::unique_ptr<OnnxBoundOrtValue> bound = model->prepareOutputTensorValue(*meta, tensor_obj, xsink);
    if (*xsink) {
        return;
    }
    try {
        binding->BindOutput(name, bound->value);
    } catch (const Ort::Exception& e) {
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "failed to bind ONNX output tensor '%s': %s", name, e.what());
        return;
    }

    bound_output_names.push_back(name);
    bound_outputs.push_back(std::move(bound));
}

void QoreOnnxIoBinding::clearInputs() {
    std::lock_guard<std::mutex> lock(mutex);
    clearInputsUnlocked();
}

void QoreOnnxIoBinding::clearInputsUnlocked() {
    if (binding) {
        binding->ClearBoundInputs();
    }
    bound_inputs.clear();
    bound_input_names.clear();
}

void QoreOnnxIoBinding::clearOutputs() {
    std::lock_guard<std::mutex> lock(mutex);
    clearOutputsUnlocked();
}

void QoreOnnxIoBinding::clearOutputsUnlocked() {
    if (binding) {
        binding->ClearBoundOutputs();
    }
    bound_outputs.clear();
    bound_output_names.clear();
}

QoreHashNode* QoreOnnxIoBinding::run(const QoreObject* options_obj, bool return_tensors,
        ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!model || !model->session) {
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "the ONNX model for this binding is not loaded");
        return nullptr;
    }
    if (bound_input_names.empty()) {
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "cannot run ONNX binding without bound inputs");
        return nullptr;
    }
    if (bound_output_names.empty()) {
        xsink->raiseException("ML-ONNX-BINDING-ERROR",
            "cannot run ONNX binding without bound outputs");
        return nullptr;
    }

    ReferenceHolder<QoreOnnxRunOptions> options_holder(xsink);
    QoreOnnxRunOptions* qore_options = getRunOptions(options_obj, xsink);
    if (*xsink) {
        return nullptr;
    }
    if (qore_options) {
        options_holder = qore_options;
    }

    auto start = SteadyClock::now();
    try {
        if (qore_options) {
            model->session->Run(qore_options->options, *binding);
        } else {
            model->session->Run(Ort::RunOptions{nullptr}, *binding);
        }
    } catch (const Ort::Exception& e) {
        xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
            "bound inference failed: %s", e.what());
        return nullptr;
    }

    QoreHashNode* rv = model->collectBoundOutputs(*binding, return_tensors, xsink);
    if (!*xsink) {
        model->recordInference(elapsedMilliseconds(start), 1);
    }
    return rv;
}

QoreListNode* QoreOnnxIoBinding::getBoundInputNames(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lock(mutex);
    return stringVectorToList(bound_input_names, xsink);
}

QoreListNode* QoreOnnxIoBinding::getBoundOutputNames(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lock(mutex);
    return stringVectorToList(bound_output_names, xsink);
}

QoreHashNode* QoreOnnxModel::runImpl(const QoreHashNode* inputs, bool return_tensors,
        const QoreListNode* requested_output_names, ExceptionSink* xsink) {
    if (!session) {
        xsink->raiseException("ML-ONNX-ERROR", "model is not loaded");
        return nullptr;
    }
    auto start = SteadyClock::now();

    // Build input tensor names and values
    std::vector<const char*> input_names;
    std::vector<Ort::Value> input_tensors;

    // We need to keep the data buffers alive until after Run()
    std::vector<std::unique_ptr<std::vector<float>>> float_buffers;
    std::vector<std::unique_ptr<std::vector<double>>> double_buffers;
    std::vector<std::unique_ptr<std::vector<Ort::Float16_t>>> float16_buffers;
    std::vector<std::unique_ptr<std::vector<Ort::BFloat16_t>>> bfloat16_buffers;
    std::vector<std::unique_ptr<std::vector<int8_t>>> int8_buffers;
    std::vector<std::unique_ptr<std::vector<int16_t>>> int16_buffers;
    std::vector<std::unique_ptr<std::vector<int32_t>>> int32_buffers;
    std::vector<std::unique_ptr<std::vector<int64_t>>> int64_buffers;
    std::vector<std::unique_ptr<std::vector<uint8_t>>> uint8_buffers;
    std::vector<std::unique_ptr<std::vector<uint16_t>>> uint16_buffers;
    std::vector<std::unique_ptr<std::vector<uint32_t>>> uint32_buffers;
    std::vector<std::unique_ptr<std::vector<uint64_t>>> uint64_buffers;
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
                if (direct_buffer->size() != 1) {
                    direct_shape.push_back(static_cast<int64_t>(direct_buffer->size()));
                }
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
            if (expected == QoreBufferElementType::Invalid) {
                switch (meta.element_type) {
                    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16: {
                        auto buf = std::make_unique<std::vector<Ort::Float16_t>>();
                        if (bufferToFloat16Vector(*direct_buffer, *buf, xsink)) {
                            return nullptr;
                        }
                        input_tensors.push_back(Ort::Value::CreateTensor<Ort::Float16_t>(mem_info,
                            buf->data(), buf->size(), direct_shape.data(), direct_shape.size()));
                        float16_buffers.push_back(std::move(buf));
                        break;
                    }
                    case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16: {
                        auto buf = std::make_unique<std::vector<Ort::BFloat16_t>>();
                        if (bufferToBFloat16Vector(*direct_buffer, *buf, xsink)) {
                            return nullptr;
                        }
                        input_tensors.push_back(Ort::Value::CreateTensor<Ort::BFloat16_t>(mem_info,
                            buf->data(), buf->size(), direct_shape.data(), direct_shape.size()));
                        bfloat16_buffers.push_back(std::move(buf));
                        break;
                    }
                    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: {
                        auto buf = std::make_unique<std::vector<uint8_t>>();
                        if (bufferToUnsignedVector(*direct_buffer, *buf, std::numeric_limits<uint8_t>::max(),
                                "uint8", xsink)) {
                            return nullptr;
                        }
                        input_tensors.push_back(Ort::Value::CreateTensor<uint8_t>(mem_info,
                            buf->data(), buf->size(), direct_shape.data(), direct_shape.size()));
                        uint8_buffers.push_back(std::move(buf));
                        break;
                    }
                    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16: {
                        auto buf = std::make_unique<std::vector<uint16_t>>();
                        if (bufferToUnsignedVector(*direct_buffer, *buf, std::numeric_limits<uint16_t>::max(),
                                "uint16", xsink)) {
                            return nullptr;
                        }
                        input_tensors.push_back(Ort::Value::CreateTensor<uint16_t>(mem_info,
                            buf->data(), buf->size(), direct_shape.data(), direct_shape.size()));
                        uint16_buffers.push_back(std::move(buf));
                        break;
                    }
                    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32: {
                        auto buf = std::make_unique<std::vector<uint32_t>>();
                        if (bufferToUnsignedVector(*direct_buffer, *buf, std::numeric_limits<uint32_t>::max(),
                                "uint32", xsink)) {
                            return nullptr;
                        }
                        input_tensors.push_back(Ort::Value::CreateTensor<uint32_t>(mem_info,
                            buf->data(), buf->size(), direct_shape.data(), direct_shape.size()));
                        uint32_buffers.push_back(std::move(buf));
                        break;
                    }
                    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64: {
                        auto buf = std::make_unique<std::vector<uint64_t>>();
                        if (bufferToUnsignedVector(*direct_buffer, *buf, std::numeric_limits<uint64_t>::max(),
                                "uint64", xsink)) {
                            return nullptr;
                        }
                        input_tensors.push_back(Ort::Value::CreateTensor<uint64_t>(mem_info,
                            buf->data(), buf->size(), direct_shape.data(), direct_shape.size()));
                        uint64_buffers.push_back(std::move(buf));
                        break;
                    }
                    default:
                        xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                            "input tensor '%s': model expects unsupported ONNX type '%s'",
                            meta.name.c_str(), elementTypeToString(meta.element_type));
                        return nullptr;
                }
                continue;
            }
            if (direct_buffer->getElementType() != expected) {
                xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                    "input tensor '%s': model expects ONNX type '%s', but input buffer has type '%s'",
                    meta.name.c_str(), elementTypeToString(meta.element_type),
                    qore_buffer_element_type_name(direct_buffer->getElementType()));
                return nullptr;
            }
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
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING: {
                    std::vector<std::string> strings;
                    strings.reserve(direct_buffer->size());
                    std::vector<const char*> raw;
                    raw.reserve(direct_buffer->size());
                    for (size_t i = 0; i < direct_buffer->size(); ++i) {
                        if (i && !(i % 100) && qore_check_cancel(xsink, "converting string tensor input")) {
                            return nullptr;
                        }
                        ValueHolder entry(direct_buffer->getReferencedEntry(i, xsink), xsink);
                        if (*xsink) {
                            return nullptr;
                        }
                        if (entry->getType() != NT_STRING) {
                            xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                                "input tensor '%s': string buffer element %zu has type '%s'; expected string",
                                meta.name.c_str(), i, entry->getFullTypeName());
                            return nullptr;
                        }
                        QoreStringValueHelper str(*entry);
                        strings.push_back(str->c_str());
                    }
                    for (const auto& str : strings) {
                        raw.push_back(str.c_str());
                    }
                    Ort::Value string_tensor = Ort::Value::CreateTensor(allocator, direct_shape.data(),
                        direct_shape.size(), ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING);
                    string_tensor.FillStringTensor(raw.data(), raw.size());
                    input_tensors.push_back(std::move(string_tensor));
                    break;
                }
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

        int64_t total_elements = tensorShapeElementCount(shape, xsink);
        if (*xsink) {
            return nullptr;
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
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16: {
                auto buf = std::make_unique<std::vector<Ort::Float16_t>>();
                flattenToFloat16(val, *buf, xsink);
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
                    Ort::Value::CreateTensor<Ort::Float16_t>(mem_info, buf->data(), buf->size(),
                        shape.data(), shape.size()));
                float16_buffers.push_back(std::move(buf));
                break;
            }
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16: {
                auto buf = std::make_unique<std::vector<Ort::BFloat16_t>>();
                flattenToBFloat16(val, *buf, xsink);
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
                    Ort::Value::CreateTensor<Ort::BFloat16_t>(mem_info, buf->data(), buf->size(),
                        shape.data(), shape.size()));
                bfloat16_buffers.push_back(std::move(buf));
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
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: {
                auto buf = std::make_unique<std::vector<uint8_t>>();
                flattenToUInt8(val, *buf, xsink);
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
                    Ort::Value::CreateTensor<uint8_t>(mem_info, buf->data(), buf->size(),
                        shape.data(), shape.size()));
                uint8_buffers.push_back(std::move(buf));
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
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16: {
                auto buf = std::make_unique<std::vector<uint16_t>>();
                flattenToUInt16(val, *buf, xsink);
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
                    Ort::Value::CreateTensor<uint16_t>(mem_info, buf->data(), buf->size(),
                        shape.data(), shape.size()));
                uint16_buffers.push_back(std::move(buf));
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
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32: {
                auto buf = std::make_unique<std::vector<uint32_t>>();
                flattenToUInt32(val, *buf, xsink);
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
                    Ort::Value::CreateTensor<uint32_t>(mem_info, buf->data(), buf->size(),
                        shape.data(), shape.size()));
                uint32_buffers.push_back(std::move(buf));
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
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64: {
                auto buf = std::make_unique<std::vector<uint64_t>>();
                flattenToUInt64(val, *buf, xsink);
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
                    Ort::Value::CreateTensor<uint64_t>(mem_info, buf->data(), buf->size(),
                        shape.data(), shape.size()));
                uint64_buffers.push_back(std::move(buf));
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
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING: {
                std::vector<std::string> strings;
                flattenToStrings(val, strings, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if ((int64_t)strings.size() != total_elements) {
                    xsink->raiseException("ML-ONNX-INFERENCE-ERROR",
                        "input tensor '%s': expected %lld elements, got %zu",
                        meta.name.c_str(), (long long)total_elements, strings.size());
                    return nullptr;
                }
                std::vector<const char*> raw;
                raw.reserve(strings.size());
                for (const auto& str : strings) {
                    raw.push_back(str.c_str());
                }
                Ort::Value string_tensor = Ort::Value::CreateTensor(allocator, shape.data(),
                    shape.size(), ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING);
                string_tensor.FillStringTensor(raw.data(), raw.size());
                input_tensors.push_back(std::move(string_tensor));
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
    std::vector<const TensorMeta*> selected_outputs = selectOutputMeta(requested_output_names, xsink);
    if (*xsink) {
        return nullptr;
    }
    std::vector<const char*> output_names;
    output_names.reserve(selected_outputs.size());
    for (const TensorMeta* meta : selected_outputs) {
        output_names.push_back(meta->name.c_str());
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
        QoreValue out_val;
        if (return_tensors) {
            out_val = convertOutputTensorToTensor(std::move(output_tensors[i]), xsink);
        } else {
            out_val = convertOutputTensor(output_tensors[i], xsink);
        }
        if (*xsink) {
            return nullptr;
        }
        rv->setKeyValue(selected_outputs[i]->name.c_str(), out_val, xsink);
        if (*xsink) {
            return nullptr;
        }
    }

    recordInference(elapsedMilliseconds(start), 1);
    return rv.release();
}

QoreListNode* QoreOnnxModel::runBatch(const QoreListNode* batch, ExceptionSink* xsink) {
    return runBatch(batch, nullptr, xsink);
}

QoreListNode* QoreOnnxModel::runBatch(const QoreListNode* batch, const QoreListNode* output_names,
        ExceptionSink* xsink) {
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
        QoreHashNode* result = run(entry.get<const QoreHashNode>(), output_names, xsink);
        if (*xsink) {
            return nullptr;
        }
        rv->push(result, xsink);
    }
    return rv.release();
}

QoreOnnxSessionPool::AsyncRequest::AsyncRequest(const QoreHashNode* inputs,
        const QoreListNode* output_names, QorePromise* promise)
        : payload(inputs->hashRefSelf()),
        output_names(output_names ? QoreValue(output_names->listRefSelf()) : QoreValue()),
        output_signature(makeOutputNamesSignature(output_names)),
        promise(promise) {
    promise->ref();
}

QoreOnnxSessionPool::AsyncRequest::~AsyncRequest() {
    ExceptionSink xsink;
    payload.discard(&xsink);
    output_names.discard(&xsink);
    promise->deref(&xsink);
}

QoreOnnxSessionPool::SingleAsyncParams::SingleAsyncParams(QoreOnnxSessionPool* pool,
        QoreValue payload, QoreValue output_names, QorePromise* promise, AsyncOp op)
        : pool(pool), payload(payload), output_names(output_names), promise(promise), op(op) {
    pool->ref();
    promise->ref();
}

QoreOnnxSessionPool::SingleAsyncParams::~SingleAsyncParams() {
    ExceptionSink xsink;
    payload.discard(&xsink);
    output_names.discard(&xsink);
    promise->deref(&xsink);
    pool->deref(&xsink);
}

QoreOnnxSessionPool::Lease::~Lease() {
    pool.release(index);
}

QoreOnnxSessionPool::~QoreOnnxSessionPool() {
    std::deque<std::unique_ptr<AsyncRequest>> requests;
    {
        std::lock_guard<std::mutex> lock(async_mutex);
        requests.swap(async_requests);
    }

    for (auto& request : requests) {
        ExceptionSink psink;
        request->promise->setError("ML-ONNX-POOL-ERROR",
            "OnnxSessionPool was destroyed before the async request could run", QoreValue(), &psink);
        if (psink) {
            psink.clear();
        }
    }
}

QoreOnnxSessionPool::QoreOnnxSessionPool(const char* model_path,
        const QoreHashNode* session_config, const QoreHashNode* pool_options,
        ExceptionSink* xsink) {
    parsePoolOptions(pool_options, xsink);
    if (*xsink) {
        return;
    }

    size_t session_count = static_cast<size_t>(max_sessions);
    sessions.reserve(session_count);
    in_use.resize(session_count, false);
    for (size_t i = 0; i < session_count; ++i) {
        if (i && (i % 100) == 0 && qore_check_cancel(xsink, "OnnxSessionPool constructor")) {
            return;
        }
        std::unique_ptr<QoreOnnxModel> session(
            session_config
                ? new QoreOnnxModel(model_path, session_config, xsink)
                : new QoreOnnxModel(model_path, xsink));
        if (*xsink) {
            return;
        }
        sessions.push_back(std::move(session));
    }
}

QoreOnnxSessionPool::QoreOnnxSessionPool(const void* model_data, size_t model_data_len,
        const QoreHashNode* session_config, const QoreHashNode* pool_options,
        ExceptionSink* xsink) {
    parsePoolOptions(pool_options, xsink);
    if (*xsink) {
        return;
    }

    size_t session_count = static_cast<size_t>(max_sessions);
    sessions.reserve(session_count);
    in_use.resize(session_count, false);
    for (size_t i = 0; i < session_count; ++i) {
        if (i && (i % 100) == 0 && qore_check_cancel(xsink, "OnnxSessionPool constructor")) {
            return;
        }
        std::unique_ptr<QoreOnnxModel> session(
            session_config
                ? new QoreOnnxModel(model_data, model_data_len, session_config, xsink)
                : new QoreOnnxModel(model_data, model_data_len, xsink));
        if (*xsink) {
            return;
        }
        sessions.push_back(std::move(session));
    }
}

void QoreOnnxSessionPool::parsePoolOptions(const QoreHashNode* pool_options,
        ExceptionSink* xsink) {
    max_sessions = getPositiveOption(pool_options, "max_sessions", max_sessions, false, xsink);
    if (*xsink) {
        return;
    }
    max_concurrent_runs = getPositiveOption(pool_options, "max_concurrent_runs",
        max_sessions, false, xsink);
    if (*xsink) {
        return;
    }
    queue_depth = getPositiveOption(pool_options, "queue_depth", queue_depth, true, xsink);
    if (*xsink) {
        return;
    }
    timeout_ms = getPositiveOption(pool_options, "timeout_ms", timeout_ms, true, xsink);
    if (*xsink) {
        return;
    }
    dynamic_batch_size = getPositiveOption(pool_options, "dynamic_batch_size",
        dynamic_batch_size, true, xsink);
    if (*xsink) {
        return;
    }
    dynamic_batch_wait_ms = getPositiveOption(pool_options, "dynamic_batch_wait_ms",
        dynamic_batch_wait_ms, true, xsink);
    if (*xsink) {
        return;
    }

    if (max_concurrent_runs > max_sessions) {
        max_concurrent_runs = max_sessions;
    }
}

int64_t QoreOnnxSessionPool::getPositiveOption(const QoreHashNode* options,
        const char* name, int64_t current, bool allow_zero, ExceptionSink* xsink) {
    if (!options) {
        return current;
    }

    QoreValue value = options->getKeyValue(name);
    if (value.isNullOrNothing()) {
        return current;
    }

    int64_t parsed = value.getAsBigInt();
    if (parsed < 0 || (!allow_zero && parsed == 0)) {
        xsink->raiseException("ML-ONNX-POOL-ERROR",
            "invalid %s value %lld; expected %s integer", name, (long long)parsed,
            allow_zero ? "a non-negative" : "a positive");
        return current;
    }
    return parsed;
}

void QoreOnnxSessionPool::validateReady(ExceptionSink* xsink) const {
    if (sessions.empty()) {
        xsink->raiseException("ML-ONNX-POOL-ERROR", "OnnxSessionPool has no loaded sessions");
    }
}

size_t QoreOnnxSessionPool::findAvailableUnlocked() const {
    for (size_t i = 0; i < in_use.size(); ++i) {
        if (!in_use[i]) {
            return i;
        }
    }
    return in_use.size();
}

std::unique_ptr<QoreOnnxSessionPool::Lease> QoreOnnxSessionPool::acquire(ExceptionSink* xsink) {
    validateReady(xsink);
    if (*xsink) {
        return nullptr;
    }

    std::unique_lock<std::mutex> lock(mutex);
    size_t index = active_runs < static_cast<uint64_t>(max_concurrent_runs)
        ? findAvailableUnlocked()
        : in_use.size();
    if (index < in_use.size()) {
        in_use[index] = true;
        ++active_runs;
        if (active_runs > max_active_runs_observed) {
            max_active_runs_observed = active_runs;
        }
        return std::make_unique<Lease>(*this, index);
    }

    if (queue_depth == 0 || waiting_callers >= static_cast<uint64_t>(queue_depth)) {
        ++total_rejections;
        xsink->raiseException("ML-ONNX-POOL-BACKPRESSURE",
            "OnnxSessionPool queue is full; active_runs=%llu waiting_callers=%llu queue_depth=%lld",
            (unsigned long long)active_runs, (unsigned long long)waiting_callers,
            (long long)queue_depth);
        return nullptr;
    }

    ++waiting_callers;
    ++total_waits;
    if (waiting_callers > max_waiting_callers_observed) {
        max_waiting_callers_observed = waiting_callers;
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (true) {
        if (qore_check_cancel(xsink, "OnnxSessionPool acquire")) {
            --waiting_callers;
            return nullptr;
        }

        if (timeout_ms > 0) {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                --waiting_callers;
                ++total_timeouts;
                xsink->raiseException("ML-ONNX-POOL-TIMEOUT",
                    "timed out after %lld ms waiting for an ONNX session", (long long)timeout_ms);
                return nullptr;
            }
            cv.wait_until(lock, std::min(deadline, now + std::chrono::milliseconds(100)));
        } else {
            cv.wait_for(lock, std::chrono::milliseconds(100));
        }

        index = findAvailableUnlocked();
        if (active_runs < static_cast<uint64_t>(max_concurrent_runs)
                && index < in_use.size()) {
            --waiting_callers;
            in_use[index] = true;
            ++active_runs;
            if (active_runs > max_active_runs_observed) {
                max_active_runs_observed = active_runs;
            }
            return std::make_unique<Lease>(*this, index);
        }
    }
}

void QoreOnnxSessionPool::release(size_t index) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (index < in_use.size() && in_use[index]) {
            in_use[index] = false;
        }
        if (active_runs > 0) {
            --active_runs;
        }
    }
    cv.notify_one();
}

void QoreOnnxSessionPool::recordRun(size_t batch_items) {
    std::lock_guard<std::mutex> lock(mutex);
    ++total_runs;
    total_batch_items += batch_items;
}

QoreHashNode* QoreOnnxSessionPool::run(const QoreHashNode* inputs, ExceptionSink* xsink) {
    return run(inputs, nullptr, xsink);
}

QoreHashNode* QoreOnnxSessionPool::run(const QoreHashNode* inputs, const QoreListNode* output_names,
        ExceptionSink* xsink) {
    std::unique_ptr<Lease> lease = acquire(xsink);
    if (*xsink) {
        return nullptr;
    }

    QoreHashNode* rv = sessions[lease->index]->run(inputs, output_names, xsink);
    if (!*xsink) {
        recordRun(1);
    }
    return rv;
}

QoreHashNode* QoreOnnxSessionPool::runTensors(const QoreHashNode* inputs, ExceptionSink* xsink) {
    return runTensors(inputs, nullptr, xsink);
}

QoreHashNode* QoreOnnxSessionPool::runTensors(const QoreHashNode* inputs, const QoreListNode* output_names,
        ExceptionSink* xsink) {
    std::unique_ptr<Lease> lease = acquire(xsink);
    if (*xsink) {
        return nullptr;
    }

    QoreHashNode* rv = sessions[lease->index]->runTensors(inputs, output_names, xsink);
    if (!*xsink) {
        recordRun(1);
    }
    return rv;
}

QoreListNode* QoreOnnxSessionPool::runBatch(const QoreListNode* batch, ExceptionSink* xsink) {
    return runBatch(batch, nullptr, xsink);
}

QoreListNode* QoreOnnxSessionPool::runBatch(const QoreListNode* batch, const QoreListNode* output_names,
        ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    size_t total = batch->size();
    if (!total) {
        return rv.release();
    }

    size_t chunk_size = dynamic_batch_size > 0
        ? static_cast<size_t>(dynamic_batch_size)
        : total;
    if (!chunk_size || chunk_size > total) {
        chunk_size = total;
    }

    for (size_t start = 0; start < total; start += chunk_size) {
        if (start && (start % 100) == 0 && qore_check_cancel(xsink, "OnnxSessionPool runBatch")) {
            return nullptr;
        }
        size_t end = std::min(start + chunk_size, total);
        ReferenceHolder<QoreListNode> chunk(new QoreListNode(autoTypeInfo), xsink);
        for (size_t i = start; i < end; ++i) {
            if ((i - start) && ((i - start) % 100) == 0
                    && qore_check_cancel(xsink, "OnnxSessionPool runBatch chunk")) {
                return nullptr;
            }
            chunk->push(batch->getReferencedEntry(i), xsink);
            if (*xsink) {
                return nullptr;
            }
        }

        std::unique_ptr<Lease> lease = acquire(xsink);
        if (*xsink) {
            return nullptr;
        }
        ReferenceHolder<QoreListNode> chunk_result(
            sessions[lease->index]->runBatch(*chunk, output_names, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }

        for (size_t i = 0; i < chunk_result->size(); ++i) {
            if (i && (i % 100) == 0 && qore_check_cancel(xsink, "OnnxSessionPool runBatch results")) {
                return nullptr;
            }
            rv->push(chunk_result->getReferencedEntry(i), xsink);
            if (*xsink) {
                return nullptr;
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex);
        ++total_batches;
    }
    recordRun(total);
    return rv.release();
}

void QoreOnnxSessionPool::resolveAsyncRequest(AsyncRequest& request, QoreValue value,
        ExceptionSink* xsink) {
    (void)xsink;
    ExceptionSink psink;
    request.promise->set(value, &psink);
    if (psink) {
        psink.clear();
        ++async_errors;
        return;
    }
    ++async_completed;
}

void QoreOnnxSessionPool::rejectAsyncBatch(std::vector<std::unique_ptr<AsyncRequest>>& batch,
        ExceptionSink& err) {
    std::string code = "ML-ONNX-ASYNC-ERROR";
    std::string desc = "async ONNX inference failed";
    QoreValue arg;

    QoreHashNode* ex = err.getExceptionInfo();
    if (ex) {
        QoreValue errv = ex->getKeyValue("err");
        if (errv.getType() == NT_STRING) {
            QoreStringValueHelper errstr(errv);
            code = errstr->c_str();
        }
        QoreValue descv = ex->getKeyValue("desc");
        if (descv.getType() == NT_STRING) {
            QoreStringValueHelper descstr(descv);
            desc = descstr->c_str();
        }
        arg = ex->getKeyValue("arg").refSelf();
        ex->deref(nullptr);
    }

    for (auto& request : batch) {
        ExceptionSink psink;
        request->promise->setError(code.c_str(), desc.c_str(), arg.refSelf(), &psink);
        if (psink) {
            psink.clear();
        }
        ++async_errors;
    }

    ExceptionSink xsink;
    arg.discard(&xsink);
    err.clear();
}

void QoreOnnxSessionPool::singleAsyncThread(ExceptionSink* xsink, void* arg) {
    (void)xsink;
    std::unique_ptr<SingleAsyncParams> params(static_cast<SingleAsyncParams*>(arg));

    ExceptionSink run_xsink;
    QoreValue rv;
    const QoreListNode* output_names = params->output_names.get<const QoreListNode>();
    switch (params->op) {
        case AsyncOp::Run:
            rv = params->pool->run(params->payload.get<const QoreHashNode>(), output_names, &run_xsink);
            break;
        case AsyncOp::RunTensors:
            rv = params->pool->runTensors(params->payload.get<const QoreHashNode>(), output_names, &run_xsink);
            break;
        case AsyncOp::RunBatch:
            rv = params->pool->runBatch(params->payload.get<const QoreListNode>(), output_names, &run_xsink);
            break;
    }

    if (run_xsink) {
        params->pool->async_errors++;
        params->promise->setException(run_xsink);
        return;
    }

    ExceptionSink psink;
    params->promise->set(rv, &psink);
    if (psink) {
        psink.clear();
        params->pool->async_errors++;
        return;
    }
    params->pool->async_completed++;
}

void QoreOnnxSessionPool::dynamicRunAsyncThread(ExceptionSink* xsink, void* arg) {
    QoreOnnxSessionPool* pool = static_cast<QoreOnnxSessionPool*>(arg);
    pool->processDynamicRunQueue(xsink);
    pool->deref(xsink);
}

void QoreOnnxSessionPool::processDynamicRunQueue(ExceptionSink* xsink) {
    while (true) {
        std::vector<std::unique_ptr<AsyncRequest>> batch;
        {
            std::unique_lock<std::mutex> lock(async_mutex);
            if (async_requests.empty()) {
                return;
            }

            size_t target_size = dynamic_batch_size > 0
                ? static_cast<size_t>(dynamic_batch_size)
                : async_requests.size();
            if (target_size < 1) {
                target_size = 1;
            }

            const std::string output_signature = async_requests.front()->output_signature;
            auto matching_count = [&]() {
                size_t count = 0;
                for (const auto& request : async_requests) {
                    if (request->output_signature == output_signature) {
                        ++count;
                    }
                }
                return count;
            };

            if (dynamic_batch_wait_ms > 0 && matching_count() < target_size) {
                auto deadline = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(dynamic_batch_wait_ms);
                while (matching_count() < target_size) {
                    if (async_cv.wait_until(lock, deadline) == std::cv_status::timeout) {
                        break;
                    }
                }
            }

            size_t count = std::min(target_size, matching_count());
            batch.reserve(count);
            for (auto i = async_requests.begin(); i != async_requests.end() && batch.size() < count;) {
                if ((*i)->output_signature == output_signature) {
                    batch.push_back(std::move(*i));
                    i = async_requests.erase(i);
                } else {
                    ++i;
                }
            }
        }

        if (batch.empty()) {
            continue;
        }

        if (qore_check_cancel(xsink, "OnnxSessionPool dynamic async batching")) {
            ExceptionSink err;
            err.assimilate(*xsink);
            rejectAsyncBatch(batch, err);
            return;
        }

        ReferenceHolder<QoreListNode> inputs(new QoreListNode(autoHashTypeInfo), xsink);
        for (size_t i = 0; i < batch.size(); ++i) {
            if (i && (i % 100) == 0
                    && qore_check_cancel(xsink, "OnnxSessionPool dynamic async batch input assembly")) {
                ExceptionSink err;
                err.assimilate(*xsink);
                rejectAsyncBatch(batch, err);
                return;
            }
            inputs->push(batch[i]->payload.refSelf(), xsink);
            if (*xsink) {
                ExceptionSink err;
                err.assimilate(*xsink);
                rejectAsyncBatch(batch, err);
                return;
            }
        }

        const QoreListNode* output_names = batch.front()->output_names.get<const QoreListNode>();
        ReferenceHolder<QoreListNode> results(runBatch(*inputs, output_names, xsink), xsink);
        if (*xsink) {
            ExceptionSink err;
            err.assimilate(*xsink);
            rejectAsyncBatch(batch, err);
            continue;
        }

        if (results->size() != batch.size()) {
            ExceptionSink err;
            err.raiseException("ML-ONNX-ASYNC-ERROR",
                "dynamic ONNX batch returned %zu results for %zu requests",
                results->size(), batch.size());
            rejectAsyncBatch(batch, err);
            continue;
        }

        ++async_batches;
        async_batch_items += batch.size();

        for (size_t i = 0; i < batch.size(); ++i) {
            if (i && (i % 100) == 0
                    && qore_check_cancel(xsink, "OnnxSessionPool dynamic async batch result delivery")) {
                ExceptionSink err;
                err.assimilate(*xsink);
                std::vector<std::unique_ptr<AsyncRequest>> remaining;
                remaining.reserve(batch.size() - i);
                for (size_t j = i; j < batch.size(); ++j) {
                    remaining.push_back(std::move(batch[j]));
                }
                rejectAsyncBatch(remaining, err);
                return;
            }
            QoreValue value = results->getReferencedEntry(i);
            resolveAsyncRequest(*batch[i], value, xsink);
        }
    }
}

QoreObject* QoreOnnxSessionPool::submitSingleAsync(QoreValue payload, QoreValue output_names, AsyncOp op,
        const QoreTypeInfo* future_type, QoreProgram* pgm, ExceptionSink* xsink) {
    ReferenceHolder<QorePromise> promise_holder(new QorePromise(), xsink);
    QorePromise* promise = *promise_holder;
    ReferenceHolder<QoreObject> future_obj(makeOnnxFutureObject(promise, future_type, pgm, xsink),
        xsink);
    if (*xsink) {
        payload.discard(xsink);
        output_names.discard(xsink);
        return nullptr;
    }

    SingleAsyncParams* params = new SingleAsyncParams(this, payload, output_names, promise, op);
    int tid = q_start_thread(xsink, singleAsyncThread, params);
    if (tid == -1) {
        delete params;
        return nullptr;
    }

    ++async_submitted;
    return future_obj.release();
}

QoreObject* QoreOnnxSessionPool::submitDynamicRunAsync(const QoreHashNode* inputs,
        const QoreListNode* output_names, QoreProgram* pgm, ExceptionSink* xsink) {
    ReferenceHolder<QorePromise> promise_holder(new QorePromise(), xsink);
    QorePromise* promise = *promise_holder;
    ReferenceHolder<QoreObject> future_obj(makeOnnxFutureObject(promise, autoHashTypeInfo, pgm, xsink),
        xsink);
    if (*xsink) {
        return nullptr;
    }

    std::unique_ptr<AsyncRequest> request(new AsyncRequest(inputs, output_names, promise));
    bool start_leader = false;
    {
        std::lock_guard<std::mutex> lock(async_mutex);
        start_leader = async_requests.empty();
        if (!start_leader && (queue_depth == 0
                || async_requests.size() >= static_cast<size_t>(queue_depth))) {
            {
                std::lock_guard<std::mutex> stats_lock(mutex);
                ++total_rejections;
            }
            xsink->raiseException("ML-ONNX-POOL-BACKPRESSURE",
                "OnnxSessionPool async queue is full; queued_requests=%zu queue_depth=%lld",
                async_requests.size(), (long long)queue_depth);
            return nullptr;
        }
        async_requests.push_back(std::move(request));
        ++async_submitted;

        if (start_leader) {
            ref();
            int tid = q_start_thread(xsink, dynamicRunAsyncThread, this);
            if (tid == -1) {
                async_requests.pop_back();
                --async_submitted;
                deref(xsink);
                return nullptr;
            }
        } else {
            async_cv.notify_one();
        }
    }

    return future_obj.release();
}

QoreObject* QoreOnnxSessionPool::runAsync(const QoreHashNode* inputs, QoreProgram* pgm,
        ExceptionSink* xsink) {
    return runAsync(inputs, nullptr, pgm, xsink);
}

QoreObject* QoreOnnxSessionPool::runAsync(const QoreHashNode* inputs, const QoreListNode* output_names,
        QoreProgram* pgm, ExceptionSink* xsink) {
    if (dynamic_batch_size > 1 && dynamic_batch_wait_ms > 0) {
        return submitDynamicRunAsync(inputs, output_names, pgm, xsink);
    }
    return submitSingleAsync(QoreValue(inputs->hashRefSelf()),
        output_names ? QoreValue(output_names->listRefSelf()) : QoreValue(), AsyncOp::Run,
        autoHashTypeInfo, pgm, xsink);
}

QoreObject* QoreOnnxSessionPool::runTensorsAsync(const QoreHashNode* inputs, QoreProgram* pgm,
        ExceptionSink* xsink) {
    return runTensorsAsync(inputs, nullptr, pgm, xsink);
}

QoreObject* QoreOnnxSessionPool::runTensorsAsync(const QoreHashNode* inputs,
        const QoreListNode* output_names, QoreProgram* pgm, ExceptionSink* xsink) {
    return submitSingleAsync(QoreValue(inputs->hashRefSelf()),
        output_names ? QoreValue(output_names->listRefSelf()) : QoreValue(), AsyncOp::RunTensors,
        autoHashTypeInfo, pgm, xsink);
}

QoreObject* QoreOnnxSessionPool::runBatchAsync(const QoreListNode* batch, QoreProgram* pgm,
        ExceptionSink* xsink) {
    return runBatchAsync(batch, nullptr, pgm, xsink);
}

QoreObject* QoreOnnxSessionPool::runBatchAsync(const QoreListNode* batch,
        const QoreListNode* output_names, QoreProgram* pgm, ExceptionSink* xsink) {
    return submitSingleAsync(QoreValue(batch->listRefSelf()),
        output_names ? QoreValue(output_names->listRefSelf()) : QoreValue(), AsyncOp::RunBatch,
        qore_get_complex_list_type(autoHashTypeInfo), pgm, xsink);
}

QoreHashNode* QoreOnnxSessionPool::getPoolStats(ExceptionSink* xsink) const {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    std::lock_guard<std::mutex> lock(mutex);
    rv->setKeyValue("max_sessions", max_sessions, xsink);
    rv->setKeyValue("actual_sessions", static_cast<int64_t>(sessions.size()), xsink);
    rv->setKeyValue("max_concurrent_runs", max_concurrent_runs, xsink);
    rv->setKeyValue("queue_depth", queue_depth, xsink);
    rv->setKeyValue("timeout_ms", timeout_ms, xsink);
    rv->setKeyValue("dynamic_batch_size", dynamic_batch_size, xsink);
    rv->setKeyValue("dynamic_batch_wait_ms", dynamic_batch_wait_ms, xsink);
    rv->setKeyValue("active_runs", static_cast<int64_t>(active_runs), xsink);
    rv->setKeyValue("waiting_callers", static_cast<int64_t>(waiting_callers), xsink);
    rv->setKeyValue("total_runs", static_cast<int64_t>(total_runs), xsink);
    rv->setKeyValue("total_batches", static_cast<int64_t>(total_batches), xsink);
    rv->setKeyValue("total_batch_items", static_cast<int64_t>(total_batch_items), xsink);
    rv->setKeyValue("total_waits", static_cast<int64_t>(total_waits), xsink);
    rv->setKeyValue("total_timeouts", static_cast<int64_t>(total_timeouts), xsink);
    rv->setKeyValue("total_rejections", static_cast<int64_t>(total_rejections), xsink);
    rv->setKeyValue("max_active_runs_observed", static_cast<int64_t>(max_active_runs_observed), xsink);
    rv->setKeyValue("max_waiting_callers_observed", static_cast<int64_t>(max_waiting_callers_observed), xsink);
    rv->setKeyValue("async_submitted", static_cast<int64_t>(async_submitted.load()), xsink);
    rv->setKeyValue("async_completed", static_cast<int64_t>(async_completed.load()), xsink);
    rv->setKeyValue("async_errors", static_cast<int64_t>(async_errors.load()), xsink);
    rv->setKeyValue("async_batches", static_cast<int64_t>(async_batches.load()), xsink);
    rv->setKeyValue("async_batch_items", static_cast<int64_t>(async_batch_items.load()), xsink);
    return rv.release();
}

void QoreOnnxSessionPool::resetPoolStats() {
    std::lock_guard<std::mutex> lock(mutex);
    total_runs = 0;
    total_batches = 0;
    total_batch_items = 0;
    total_waits = 0;
    total_timeouts = 0;
    total_rejections = 0;
    max_active_runs_observed = active_runs;
    max_waiting_callers_observed = waiting_callers;
    async_submitted = 0;
    async_completed = 0;
    async_errors = 0;
    async_batches = 0;
    async_batch_items = 0;
}

QoreHashNode* QoreOnnxSessionPool::getModelInfo(ExceptionSink* xsink) {
    std::unique_ptr<Lease> lease = acquire(xsink);
    if (*xsink) {
        return nullptr;
    }
    return sessions[lease->index]->getModelInfo(xsink);
}

QoreListNode* QoreOnnxSessionPool::getInputInfo(ExceptionSink* xsink) {
    std::unique_ptr<Lease> lease = acquire(xsink);
    if (*xsink) {
        return nullptr;
    }
    return sessions[lease->index]->getInputInfo(xsink);
}

QoreListNode* QoreOnnxSessionPool::getOutputInfo(ExceptionSink* xsink) {
    std::unique_ptr<Lease> lease = acquire(xsink);
    if (*xsink) {
        return nullptr;
    }
    return sessions[lease->index]->getOutputInfo(xsink);
}

#endif // HAVE_ONNXRUNTIME
