/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    Tensor.cpp

    Qore ml module - Tensor implementation

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

#include "QC_Tensor.h"

#include <cstring>
#include <limits>
#include <vector>
#include <memory>
#include <string>

#ifdef HAVE_CUDART
#include <cuda_runtime.h>
#endif

namespace {

//! Owner for a mock device buffer: holds the "device" bytes (really host memory)
//! alive for the lifetime of the wrapping QoreBufferNode.
struct MockDeviceOwner {
    std::vector<uint8_t> bytes;
};

#ifdef HAVE_CUDART
//! Owner for a real CUDA device buffer produced by host->device upload: holds the
//! cudaMalloc'd device pointer and frees it on destruction; records the element byte
//! size so the copy-to-host callback can size the device->host transfer.
struct CudaUploadOwner {
    void* device_ptr = nullptr;
    size_t element_size = 0;

    DLLLOCAL ~CudaUploadOwner() {
        if (device_ptr) {
            // best-effort free at buffer end-of-life; errors here are not actionable
            cudaFree(device_ptr);
        }
    }
};

//! Copy-to-host callback for uploaded CUDA device buffers: cudaMemcpy device->host.
int cudaUploadCopyToHost(void* host_data, uint8_t* /*host_validity*/, size_t length,
        const void* device_data, const uint8_t* /*device_validity*/,
        const QoreBufferDeviceInfo& info, const void* owner, ExceptionSink* xsink) {
    const CudaUploadOwner* o = static_cast<const CudaUploadOwner*>(owner);
    size_t bytes = length * (o ? o->element_size : 0);
    if (!bytes) {
        return 0;
    }
    cudaError_t err = cudaMemcpy(host_data, device_data, bytes, cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        xsink->raiseException("ML-TENSOR-DEVICE-ERROR",
            "failed to copy %zu bytes from %s device %lld to host: %s", bytes,
            qore_buffer_device_kind_name(info.kind), (long long)info.device_id,
            cudaGetErrorString(err));
        return -1;
    }
    return 0;
}
#endif

//! Copy-to-host callback for mock device buffers; the bytes already live in host
//! memory, so materialization is a plain memcpy of the owner-held bytes.
int mockDeviceCopyToHost(void* host_data, uint8_t* /*host_validity*/, size_t /*length*/,
        const void* device_data, const uint8_t* /*device_validity*/,
        const QoreBufferDeviceInfo& /*device_info*/, const void* owner, ExceptionSink* /*xsink*/) {
    const MockDeviceOwner* o = static_cast<const MockDeviceOwner*>(owner);
    if (o && !o->bytes.empty()) {
        memcpy(host_data, device_data, o->bytes.size());
    }
    return 0;
}

//! Maps a lower-case device-kind name to a QoreBufferDeviceKind for the mock helper.
QoreBufferDeviceKind mockDeviceKindFromName(const char* name) {
    std::string n = name ? name : "";
    for (char& c : n) {
        c = static_cast<char>((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
    }
    if (n == "cuda") return QoreBufferDeviceKind::Cuda;
    if (n == "rocm" || n == "hip") return QoreBufferDeviceKind::Rocm;
    if (n == "opencl") return QoreBufferDeviceKind::OpenCL;
    if (n == "vulkan") return QoreBufferDeviceKind::Vulkan;
    if (n == "oneapi") return QoreBufferDeviceKind::OneAPI;
    if (n == "metal") return QoreBufferDeviceKind::Metal;
    if (n == "java") return QoreBufferDeviceKind::Java;
    return QoreBufferDeviceKind::Other;
}

struct inferred_tensor_type_t {
    bool has_int = false;
    bool has_float = false;
    bool has_bool = false;
    bool has_string = false;
};

int inferTensorTypeRec(QoreValue value, inferred_tensor_type_t& info, ExceptionSink* xsink) {
    if (value.getType() == NT_LIST) {
        const QoreListNode* list = value.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "inferring tensor dtype")) {
                return -1;
            }
            if (inferTensorTypeRec(list->retrieveEntry(i), info, xsink)) {
                return -1;
            }
        }
        return 0;
    }

    if (value.isNullOrNothing()) {
        xsink->raiseException("ML-TENSOR-ERROR",
            "tensor values cannot contain NOTHING or NULL because model tensors do not carry per-element nulls");
        return -1;
    }

    switch (value.getType()) {
        case NT_INT:
            info.has_int = true;
            return 0;
        case NT_FLOAT:
        case NT_NUMBER:
            info.has_float = true;
            return 0;
        case NT_BOOLEAN:
            info.has_bool = true;
            return 0;
        case NT_STRING:
            info.has_string = true;
            return 0;
        default:
            xsink->raiseException("ML-TENSOR-ERROR",
                "cannot infer tensor dtype from value of type '%s'; pass a numeric, bool, string, list, or buffer value",
                value.getFullTypeName());
            return -1;
    }
}

template <typename T>
int packDenseColumns(QoreBufferNode& output, const std::vector<const QoreBufferNode*>& columns,
        size_t rows, ExceptionSink* xsink) {
    T* dst = static_cast<T*>(output.getRawData());
    std::vector<const T*> srcs;
    srcs.reserve(columns.size());
    for (const QoreBufferNode* col : columns) {
        srcs.push_back(static_cast<const T*>(col->getRawData()));
    }

    size_t width = columns.size();
    for (size_t i = 0; i < rows; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "packing tensor columns")) {
            return -1;
        }
        size_t offset = i * width;
        for (size_t j = 0; j < width; ++j) {
            dst[offset + j] = srcs[j][i];
        }
    }
    return 0;
}

class TensorRefList {
public:
    ~TensorRefList() {
        ExceptionSink xsink;
        for (QoreTensor* tensor : refs) {
            tensor->deref(&xsink);
        }
    }

    int add(QoreValue value, size_t index, ExceptionSink* xsink) {
        if (value.getType() != NT_OBJECT) {
            xsink->raiseException("ML-TENSOR-ERROR",
                "concatRows() list element %zu is type '%s'; expected ML::Tensor",
                index, value.getFullTypeName());
            return -1;
        }

        QoreObject* obj = value.get<QoreObject>();
        QoreTensor* tensor = static_cast<QoreTensor*>(obj->getReferencedPrivateData(CID_TENSOR, xsink));
        if (*xsink) {
            return -1;
        }
        if (!tensor) {
            xsink->raiseException("ML-TENSOR-ERROR",
                "concatRows() list element %zu is not an ML::Tensor", index);
            return -1;
        }
        refs.push_back(tensor);
        return 0;
    }

    const std::vector<QoreTensor*>& values() const {
        return refs;
    }

private:
    std::vector<QoreTensor*> refs;
};

int64_t tensorTrailingElementCount(const std::vector<int64_t>& shape, ExceptionSink* xsink) {
    if (shape.empty()) {
        xsink->raiseException("ML-TENSOR-SHAPE-ERROR",
            "row tensor operation requires a tensor with rank >= 1");
        return -1;
    }

    int64_t count = 1;
    for (size_t i = 1; i < shape.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "validating tensor row shape")) {
            return -1;
        }
        int64_t dim = shape[i];
        if (dim && count > std::numeric_limits<int64_t>::max() / dim) {
            xsink->raiseException("ML-TENSOR-SHAPE-ERROR",
                "tensor row width overflows int64");
            return -1;
        }
        count *= dim;
    }
    return count;
}

}

QoreTensor::QoreTensor(QoreBufferNode* n_buffer, std::vector<int64_t> n_shape)
        : buffer(n_buffer), shape(std::move(n_shape)) {
    assert(buffer);
}

QoreTensor::QoreTensor(const QoreBufferNode* n_buffer, std::vector<int64_t> n_shape)
        : buffer(const_cast<QoreBufferNode*>(n_buffer)), shape(std::move(n_shape)) {
    assert(buffer);
    buffer->ref();
}

QoreTensor::~QoreTensor() {
    if (buffer) {
        ExceptionSink xsink;
        buffer->deref(&xsink);
    }
}

int QoreTensor::parseShape(const QoreListNode* shape, std::vector<int64_t>& out, ExceptionSink* xsink) {
    out.clear();
    if (!shape) {
        return 0;
    }

    out.reserve(shape->size());
    for (size_t i = 0; i < shape->size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "parsing tensor shape")) {
            return -1;
        }
        QoreValue v = shape->retrieveEntry(i);
        int64_t dim = v.getAsBigInt();
        if (dim < 0) {
            xsink->raiseException("ML-TENSOR-SHAPE-ERROR",
                "tensor shape dimension %zu is " QLLD "; dimensions must be >= 0", i, dim);
            return -1;
        }
        out.push_back(dim);
    }
    return 0;
}

int64_t QoreTensor::shapeElementCount(const std::vector<int64_t>& shape, ExceptionSink* xsink) {
    if (shape.empty()) {
        return 1;
    }

    int64_t count = 1;
    for (size_t i = 0; i < shape.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "validating tensor shape")) {
            return -1;
        }
        int64_t dim = shape[i];
        if (dim && count > std::numeric_limits<int64_t>::max() / dim) {
            xsink->raiseException("ML-TENSOR-SHAPE-ERROR",
                "tensor shape is too large; element count overflows int64");
            return -1;
        }
        count *= dim;
    }
    return count;
}

int QoreTensor::inferListShape(QoreValue value, std::vector<int64_t>& shape, ExceptionSink* xsink) {
    shape.clear();
    if (value.getType() != NT_LIST) {
        shape.push_back(1);
        return 0;
    }

    const QoreListNode* list = value.get<const QoreListNode>();
    shape.push_back(static_cast<int64_t>(list->size()));
    if (list->empty()) {
        return 0;
    }

    std::vector<int64_t> first_shape;
    if (inferListShape(list->retrieveEntry(0), first_shape, xsink)) {
        return -1;
    }
    if (first_shape.size() == 1 && first_shape[0] == 1
            && list->retrieveEntry(0).getType() != NT_LIST) {
        first_shape.clear();
    }

    for (size_t i = 1; i < list->size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "inferring tensor shape")) {
            return -1;
        }
        std::vector<int64_t> current_shape;
        if (inferListShape(list->retrieveEntry(i), current_shape, xsink)) {
            return -1;
        }
        if (current_shape.size() == 1 && current_shape[0] == 1
                && list->retrieveEntry(i).getType() != NT_LIST) {
            current_shape.clear();
        }
        if (current_shape != first_shape) {
            xsink->raiseException("ML-TENSOR-SHAPE-ERROR",
                "cannot infer tensor shape from a ragged nested list; element %zu has a different shape", i);
            return -1;
        }
    }

    shape.insert(shape.end(), first_shape.begin(), first_shape.end());
    return 0;
}

int QoreTensor::flattenToList(QoreValue value, QoreListNode& out, ExceptionSink* xsink) {
    if (value.getType() == NT_LIST) {
        const QoreListNode* list = value.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "flattening tensor data")) {
                return -1;
            }
            if (flattenToList(list->retrieveEntry(i), out, xsink)) {
                return -1;
            }
        }
        return 0;
    }

    if (value.isNullOrNothing()) {
        xsink->raiseException("ML-TENSOR-ERROR",
            "tensor values cannot contain NOTHING or NULL because model tensors do not carry per-element nulls");
        return -1;
    }

    out.push(value.refSelf(), xsink);
    return *xsink ? -1 : 0;
}

QoreBufferElementType QoreTensor::inferElementType(QoreValue value, ExceptionSink* xsink) {
    inferred_tensor_type_t info;
    if (inferTensorTypeRec(value, info, xsink)) {
        return QoreBufferElementType::Invalid;
    }
    if (info.has_float) {
        if (info.has_string) {
            xsink->raiseException("ML-TENSOR-DTYPE-ERROR",
                "cannot infer tensor dtype from mixed string and numeric values; pass a homogeneous tensor");
            return QoreBufferElementType::Invalid;
        }
        return QoreBufferElementType::Float64;
    }
    if (info.has_int) {
        if (info.has_string) {
            xsink->raiseException("ML-TENSOR-DTYPE-ERROR",
                "cannot infer tensor dtype from mixed string and numeric values; pass a homogeneous tensor");
            return QoreBufferElementType::Invalid;
        }
        return QoreBufferElementType::Int64;
    }
    if (info.has_bool) {
        if (info.has_string) {
            xsink->raiseException("ML-TENSOR-DTYPE-ERROR",
                "cannot infer tensor dtype from mixed string and bool values; pass a homogeneous tensor");
            return QoreBufferElementType::Invalid;
        }
        return QoreBufferElementType::Bool;
    }
    if (info.has_string) {
        return QoreBufferElementType::String;
    }
    xsink->raiseException("ML-TENSOR-ERROR",
        "cannot infer tensor dtype from an empty list; pass dtype explicitly");
    return QoreBufferElementType::Invalid;
}

QoreTensor* QoreTensor::fromValue(QoreValue data, const QoreListNode* shape_arg,
        const QoreStringNode* dtype, bool zero_copy, ExceptionSink* xsink) {
    std::vector<int64_t> shape;
    if (parseShape(shape_arg, shape, xsink)) {
        return nullptr;
    }

    QoreBufferElementType element_type = QoreBufferElementType::Invalid;
    if (dtype) {
        if (!qore_buffer_element_type_from_name(dtype->c_str(), element_type)) {
            xsink->raiseException("ML-TENSOR-DTYPE-ERROR",
                "unsupported tensor dtype '%s'; expected int8, int16, int32, int64, float32, float64, bool, or string",
                dtype->c_str());
            return nullptr;
        }
        if (element_type == QoreBufferElementType::Decimal128) {
            xsink->raiseException("ML-TENSOR-DTYPE-ERROR",
                "tensor dtype '%s' is not supported for ML tensors yet; use numeric, bool, or string tensor data",
                dtype->c_str());
            return nullptr;
        }
    }

    ReferenceHolder<QoreBufferNode> buffer_holder(xsink);
    if (data.getType() == NT_BUFFER) {
        const QoreBufferNode* source = data.get<const QoreBufferNode>();
        QoreBufferElementType source_type = source->getElementType();
        if (source_type == QoreBufferElementType::Decimal128 || source_type == QoreBufferElementType::Invalid) {
            xsink->raiseException("ML-TENSOR-DTYPE-ERROR",
                "tensor dtype '%s' is not supported for ML tensors yet; use numeric, bool, or string tensor data",
                qore_buffer_element_type_name(source_type));
            return nullptr;
        }
        if (source->hasNullableElements()) {
            xsink->raiseException("ML-TENSOR-ERROR",
                "cannot create a tensor from '%s' because nullable buffer elements cannot be represented in model "
                "tensors; remove or impute nulls first", data.getFullTypeName());
            return nullptr;
        }
        if (element_type != QoreBufferElementType::Invalid && source_type != element_type) {
            xsink->raiseException("ML-TENSOR-DTYPE-ERROR",
                "tensor dtype '%s' does not match source buffer type '%s'",
                qore_buffer_element_type_name(element_type),
                qore_buffer_element_type_name(source_type));
            return nullptr;
        }
        if (shape.empty()) {
            shape.push_back(static_cast<int64_t>(source->size()));
        }
        int64_t count = shapeElementCount(shape, xsink);
        if (*xsink) {
            return nullptr;
        }
        if (count != static_cast<int64_t>(source->size())) {
            xsink->raiseException("ML-TENSOR-SHAPE-ERROR",
                "tensor shape expects " QLLD " elements, but source buffer contains %zu",
                count, source->size());
            return nullptr;
        }
        if (zero_copy) {
            return new QoreTensor(source, std::move(shape));
        }
        buffer_holder = source->copy(xsink);
    } else {
        if (shape.empty() && inferListShape(data, shape, xsink)) {
            return nullptr;
        }
        int64_t count = shapeElementCount(shape, xsink);
        if (*xsink) {
            return nullptr;
        }
        if (element_type == QoreBufferElementType::Invalid) {
            element_type = inferElementType(data, xsink);
            if (*xsink) {
                return nullptr;
            }
        }

        ReferenceHolder<QoreListNode> flat(new QoreListNode(autoTypeInfo), xsink);
        if (flattenToList(data, **flat, xsink)) {
            return nullptr;
        }
        if (count != static_cast<int64_t>((*flat)->size())) {
            xsink->raiseException("ML-TENSOR-SHAPE-ERROR",
                "tensor shape expects " QLLD " elements, but source data contains %zu",
                count, (*flat)->size());
            return nullptr;
        }
        buffer_holder = new QoreBufferNode(element_type, false, *flat, xsink);
    }

    if (*xsink) {
        return nullptr;
    }
    return new QoreTensor(buffer_holder.release(), std::move(shape));
}

QoreTensor* QoreTensor::fromColumns(const QoreListNode* columns_arg, const QoreStringNode* dtype,
        ExceptionSink* xsink) {
    if (!columns_arg || columns_arg->empty()) {
        xsink->raiseException("ML-TENSOR-ERROR", "fromColumns() requires at least one dense buffer column");
        return nullptr;
    }

    QoreBufferElementType element_type = QoreBufferElementType::Invalid;
    if (dtype) {
        if (!qore_buffer_element_type_from_name(dtype->c_str(), element_type)) {
            xsink->raiseException("ML-TENSOR-DTYPE-ERROR",
                "unsupported tensor dtype '%s'; expected int8, int16, int32, int64, float32, float64, or bool",
                dtype->c_str());
            return nullptr;
        }
    }

    std::vector<const QoreBufferNode*> columns;
    columns.reserve(columns_arg->size());
    size_t rows = 0;
    for (size_t i = 0; i < columns_arg->size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "validating tensor columns")) {
            return nullptr;
        }

        QoreValue value = columns_arg->retrieveEntry(i);
        if (value.getType() != NT_BUFFER) {
            xsink->raiseException("ML-TENSOR-ERROR",
                "fromColumns() column %zu is type '%s'; expected a dense buffer", i, value.getFullTypeName());
            return nullptr;
        }

        const QoreBufferNode* column = value.get<const QoreBufferNode>();
        if (column->hasNullableElements()) {
            xsink->raiseException("ML-TENSOR-ERROR",
                "fromColumns() column %zu is '%s'; nullable buffer elements cannot be represented in model tensors",
                i, value.getFullTypeName());
            return nullptr;
        }
        if (column->ensureHostStorage(xsink)) {
            return nullptr;
        }

        if (i == 0) {
            if (element_type == QoreBufferElementType::Invalid) {
                element_type = column->getElementType();
            }
            rows = column->size();
        } else if (column->size() != rows) {
            xsink->raiseException("ML-TENSOR-SHAPE-ERROR",
                "fromColumns() column %zu has %zu rows; expected %zu", i, column->size(), rows);
            return nullptr;
        }

        if (column->getElementType() != element_type) {
            xsink->raiseException("ML-TENSOR-DTYPE-ERROR",
                "fromColumns() column %zu has type '%s'; expected '%s'", i,
                qore_buffer_element_type_name(column->getElementType()),
                qore_buffer_element_type_name(element_type));
            return nullptr;
        }
        columns.push_back(column);
    }

    if (element_type == QoreBufferElementType::String || element_type == QoreBufferElementType::Decimal128
            || element_type == QoreBufferElementType::Invalid) {
        xsink->raiseException("ML-TENSOR-DTYPE-ERROR",
            "fromColumns() does not support tensor dtype '%s'; use numeric or bool columns",
            qore_buffer_element_type_name(element_type));
        return nullptr;
    }

    size_t width = columns.size();
    if (rows && width > std::numeric_limits<size_t>::max() / rows) {
        xsink->raiseException("ML-TENSOR-SHAPE-ERROR", "fromColumns() tensor element count overflows size_t");
        return nullptr;
    }
    constexpr size_t max_int64 = static_cast<size_t>(std::numeric_limits<int64_t>::max());
    if (rows > max_int64 || width > max_int64) {
        xsink->raiseException("ML-TENSOR-SHAPE-ERROR", "fromColumns() tensor dimensions exceed int64 limits");
        return nullptr;
    }

    ReferenceHolder<QoreBufferNode> buffer(new QoreBufferNode(element_type, false, rows * width), xsink);
    switch (element_type) {
        case QoreBufferElementType::Float32:
            if (packDenseColumns<float>(**buffer, columns, rows, xsink)) {
                return nullptr;
            }
            break;
        case QoreBufferElementType::Float64:
            if (packDenseColumns<double>(**buffer, columns, rows, xsink)) {
                return nullptr;
            }
            break;
        case QoreBufferElementType::Int8:
            if (packDenseColumns<int8_t>(**buffer, columns, rows, xsink)) {
                return nullptr;
            }
            break;
        case QoreBufferElementType::Int16:
            if (packDenseColumns<int16_t>(**buffer, columns, rows, xsink)) {
                return nullptr;
            }
            break;
        case QoreBufferElementType::Int32:
            if (packDenseColumns<int32_t>(**buffer, columns, rows, xsink)) {
                return nullptr;
            }
            break;
        case QoreBufferElementType::Int64:
            if (packDenseColumns<int64_t>(**buffer, columns, rows, xsink)) {
                return nullptr;
            }
            break;
        case QoreBufferElementType::Bool:
            for (size_t i = 0; i < rows; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "packing bool tensor columns")) {
                    return nullptr;
                }
                size_t offset = i * width;
                for (size_t j = 0; j < width; ++j) {
                    if ((*buffer)->setEntry(offset + j, columns[j]->getReferencedEntry(i, xsink), xsink)) {
                        return nullptr;
                    }
                }
            }
            break;
        default:
            assert(false);
    }

    return new QoreTensor(buffer.release(), {
        static_cast<int64_t>(rows),
        static_cast<int64_t>(width),
    });
}

QoreTensor* QoreTensor::concatRows(const QoreListNode* tensors_arg, ExceptionSink* xsink) {
    if (!tensors_arg || tensors_arg->empty()) {
        xsink->raiseException("ML-TENSOR-ERROR", "concatRows() requires at least one ML::Tensor");
        return nullptr;
    }

    TensorRefList tensor_refs;
    for (size_t i = 0; i < tensors_arg->size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "validating tensor rows")) {
            return nullptr;
        }
        if (tensor_refs.add(tensors_arg->retrieveEntry(i), i, xsink)) {
            return nullptr;
        }
    }

    const std::vector<QoreTensor*>& tensors = tensor_refs.values();
    const std::vector<int64_t>& first_shape = tensors.front()->getShape();
    int64_t row_width = tensorTrailingElementCount(first_shape, xsink);
    if (*xsink) {
        return nullptr;
    }
    QoreBufferElementType element_type = tensors.front()->getElementType();
    int64_t total_rows = 0;
    for (size_t i = 0; i < tensors.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "validating tensor row compatibility")) {
            return nullptr;
        }
        const QoreTensor* tensor = tensors[i];
        const std::vector<int64_t>& shape = tensor->getShape();
        if (shape.size() != first_shape.size()) {
            xsink->raiseException("ML-TENSOR-SHAPE-ERROR",
                "concatRows() tensor %zu has rank %zu; expected rank %zu",
                i, shape.size(), first_shape.size());
            return nullptr;
        }
        if (tensor->getElementType() != element_type) {
            xsink->raiseException("ML-TENSOR-DTYPE-ERROR",
                "concatRows() tensor %zu has dtype '%s'; expected '%s'", i,
                qore_buffer_element_type_name(tensor->getElementType()),
                qore_buffer_element_type_name(element_type));
            return nullptr;
        }
        for (size_t dim = 1; dim < shape.size(); ++dim) {
            if (dim && !(dim % 100) && qore_check_cancel(xsink, "validating tensor trailing dimensions")) {
                return nullptr;
            }
            if (shape[dim] != first_shape[dim]) {
                xsink->raiseException("ML-TENSOR-SHAPE-ERROR",
                    "concatRows() tensor %zu dimension %zu is " QLLD "; expected " QLLD,
                    i, dim, shape[dim], first_shape[dim]);
                return nullptr;
            }
        }
        if (shape[0] && total_rows > std::numeric_limits<int64_t>::max() - shape[0]) {
            xsink->raiseException("ML-TENSOR-SHAPE-ERROR", "concatRows() row count overflows int64");
            return nullptr;
        }
        total_rows += shape[0];
    }

    if (row_width && total_rows > std::numeric_limits<int64_t>::max() / row_width) {
        xsink->raiseException("ML-TENSOR-SHAPE-ERROR", "concatRows() element count overflows int64");
        return nullptr;
    }
    int64_t total_elements_i64 = total_rows * row_width;
    if (static_cast<uint64_t>(total_elements_i64) > std::numeric_limits<size_t>::max()) {
        xsink->raiseException("ML-TENSOR-SHAPE-ERROR", "concatRows() tensor element count overflows size_t");
        return nullptr;
    }
    size_t total_elements = static_cast<size_t>(total_elements_i64);

    ReferenceHolder<QoreBufferNode> buffer(new QoreBufferNode(element_type, false, total_elements), xsink);
    size_t output_offset = 0;
    for (size_t i = 0; i < tensors.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "concatenating tensor rows")) {
            return nullptr;
        }
        const QoreBufferNode* input = tensors[i]->getBuffer();
        if (input->hasNullableElements()) {
            xsink->raiseException("ML-TENSOR-ERROR",
                "concatRows() tensor %zu contains nullable elements; model tensors do not support nulls", i);
            return nullptr;
        }
        if (input->ensureHostStorage(xsink)) {
            return nullptr;
        }
        size_t input_size = input->size();
        if (element_type == QoreBufferElementType::Bool) {
            for (size_t j = 0; j < input_size; ++j) {
                if (j && !(j % 100) && qore_check_cancel(xsink, "concatenating bool tensor rows")) {
                    return nullptr;
                }
                if ((*buffer)->setEntry(output_offset + j, input->getReferencedEntry(j, xsink), xsink)) {
                    return nullptr;
                }
            }
        } else if (input_size) {
            size_t element_size = qore_buffer_element_storage_size(element_type);
            std::memcpy(static_cast<uint8_t*>((*buffer)->getRawData()) + (output_offset * element_size),
                input->getRawData(), input_size * element_size);
        }
        output_offset += input_size;
    }

    std::vector<int64_t> shape = first_shape;
    shape[0] = total_rows;
    return new QoreTensor(buffer.release(), std::move(shape));
}

QoreTensor* QoreTensor::reshape(const QoreListNode* shape_arg, ExceptionSink* xsink) const {
    std::vector<int64_t> n_shape;
    if (parseShape(shape_arg, n_shape, xsink)) {
        return nullptr;
    }
    int64_t new_count = shapeElementCount(n_shape, xsink);
    if (*xsink) {
        return nullptr;
    }
    if (new_count != static_cast<int64_t>(buffer->size())) {
        xsink->raiseException("ML-TENSOR-SHAPE-ERROR",
            "reshape() shape expects " QLLD " elements, but tensor has %zu",
            new_count, buffer->size());
        return nullptr;
    }
    return new QoreTensor(static_cast<const QoreBufferNode*>(buffer), std::move(n_shape));
}

QoreTensor* QoreTensor::sliceRows(int64_t offset, int64_t count, bool zero_copy, ExceptionSink* xsink) const {
    const std::vector<int64_t>& old_shape = getShape();
    int64_t row_width = tensorTrailingElementCount(old_shape, xsink);
    if (*xsink) {
        return nullptr;
    }
    int64_t rows = old_shape[0];
    if (offset < 0 || count < 0 || offset > rows || count > rows - offset) {
        xsink->raiseException("ML-TENSOR-SHAPE-ERROR",
            "sliceRows() requested offset " QLLD " and count " QLLD " for tensor with " QLLD " rows",
            offset, count, rows);
        return nullptr;
    }

    int64_t element_offset_i64 = offset * row_width;
    int64_t element_count_i64 = count * row_width;
    assert(element_offset_i64 >= 0 && element_count_i64 >= 0);
    size_t element_offset = static_cast<size_t>(element_offset_i64);
    size_t element_count = static_cast<size_t>(element_count_i64);

    ReferenceHolder<QoreBufferNode> n_buffer(
        zero_copy ? buffer->view(element_offset, element_count) : buffer->slice(element_offset, element_count, xsink),
        xsink);
    if (*xsink) {
        return nullptr;
    }

    std::vector<int64_t> n_shape = old_shape;
    n_shape[0] = count;
    return new QoreTensor(n_buffer.release(), std::move(n_shape));
}

QoreBufferNode* QoreTensor::refBuffer() const {
    buffer->ref();
    return buffer;
}

QoreListNode* QoreTensor::getShapeList(ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(bigIntTypeInfo), xsink);
    for (size_t i = 0; i < shape.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "building tensor shape list")) {
            return nullptr;
        }
        int64_t dim = shape[i];
        rv->push(dim, xsink);
    }
    return rv.release();
}

QoreValue QoreTensor::toListImpl(size_t depth, size_t& offset, ExceptionSink* xsink) const {
    if (depth >= shape.size()) {
        // getReferencedEntry() already returns an owned reference; transfer it
        // directly. Calling refSelf() here would take a second reference and
        // leak the first.
        return buffer->getReferencedEntry(offset++, xsink);
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    int64_t dim = shape[depth];
    for (int64_t i = 0; i < dim; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "converting tensor to nested lists")) {
            return QoreValue();
        }
        rv->push(toListImpl(depth + 1, offset, xsink), xsink);
        if (*xsink) {
            return QoreValue();
        }
    }
    return rv.release();
}

QoreValue QoreTensor::toList(ExceptionSink* xsink) const {
    size_t offset = 0;
    return toListImpl(0, offset, xsink);
}

QoreObject* qore_ml_tensor_to_object(QoreTensor* tensor, QoreProgram* pgm, ExceptionSink* xsink) {
    ReferenceHolder<QoreTensor> tensor_holder(tensor, xsink);
    ReferenceHolder<QoreObject> obj(new QoreObject(QC_TENSOR, pgm), xsink);
    (*obj)->setPrivate(CID_TENSOR, tensor_holder.release());
    return obj.release();
}

QoreTensor* qore_ml_make_mock_device_tensor(const QoreTensor* host, const char* device_kind,
        int64_t device_id, ExceptionSink* xsink) {
    const QoreBufferNode* host_buf = host->getBuffer();
    QoreBufferElementType et = host_buf->getElementType();
    if (et == QoreBufferElementType::Bool) {
        xsink->raiseException("ML-TENSOR-DEVICE-ERROR",
            "mock device tensors do not support bit-packed bool buffers");
        return nullptr;
    }

    size_t n = host_buf->size();
    size_t byte_size = n * qore_buffer_element_storage_size(et);

    // copy the host bytes into the owner; these stand in for device memory
    auto owner = std::make_shared<MockDeviceOwner>();
    owner->bytes.resize(byte_size);
    if (byte_size) {
        memcpy(owner->bytes.data(), host_buf->getRawData(), byte_size);
    }

    QoreBufferDeviceInfo info;
    info.kind = mockDeviceKindFromName(device_kind);
    info.device_id = device_id;
    info.name = std::string(qore_buffer_device_kind_name(info.kind)) + ":" + std::to_string(device_id);

    const void* device_data = owner->bytes.data();
    ReferenceHolder<QoreBufferNode> dev_buf(
        QoreBufferNode::wrapExternalDeviceStorage(et, false, n, device_data, nullptr,
            std::static_pointer_cast<const void>(owner), 0, info, mockDeviceCopyToHost, xsink),
        xsink);
    if (*xsink) {
        return nullptr;
    }
    return new QoreTensor(dev_buf.release(), host->getShape());
}

QoreTensor* qore_ml_make_device_tensor(const QoreTensor* host, const char* device_kind,
        int64_t device_id, ExceptionSink* xsink) {
    QoreBufferDeviceKind kind = mockDeviceKindFromName(device_kind);
    if (device_id < 0) {
        xsink->raiseException("ML-TENSOR-DEVICE-ERROR",
            "device_id must be >= 0; got " QLLD, (long long)device_id);
        return nullptr;
    }
#ifdef HAVE_CUDART
    // v1 uploads target CUDA device memory; other device families (rocm, etc.) are
    // planned but require their own runtime allocator/copy path
    if (kind != QoreBufferDeviceKind::Cuda) {
        xsink->raiseException("ML-TENSOR-DEVICE-ERROR",
            "host->device upload is only implemented for the 'cuda' device kind in this build; "
            "got '%s'", device_kind ? device_kind : "");
        return nullptr;
    }

    const QoreBufferNode* host_buf = host->getBuffer();
    QoreBufferElementType et = host_buf->getElementType();
    if (et == QoreBufferElementType::Bool) {
        xsink->raiseException("ML-TENSOR-DEVICE-ERROR",
            "device upload does not support bit-packed bool buffers");
        return nullptr;
    }
    // a device-resident source must be materialized to host before re-uploading
    if (host_buf->hasExternalDeviceStorage() && host_buf->ensureHostStorage(xsink)) {
        return nullptr;
    }

    size_t n = host_buf->size();
    size_t elem_size = qore_buffer_element_storage_size(et);
    size_t byte_size = n * elem_size;

    cudaError_t err = cudaSetDevice(static_cast<int>(device_id));
    if (err != cudaSuccess) {
        xsink->raiseException("ML-TENSOR-DEVICE-ERROR",
            "cannot select CUDA device " QLLD ": %s", (long long)device_id, cudaGetErrorString(err));
        return nullptr;
    }

    auto owner = std::make_shared<CudaUploadOwner>();
    owner->element_size = elem_size;
    if (byte_size) {
        err = cudaMalloc(&owner->device_ptr, byte_size);
        if (err != cudaSuccess) {
            xsink->raiseException("ML-TENSOR-DEVICE-ERROR",
                "failed to allocate %zu bytes on CUDA device " QLLD ": %s", byte_size,
                (long long)device_id, cudaGetErrorString(err));
            return nullptr;
        }
        err = cudaMemcpy(owner->device_ptr, host_buf->getRawData(), byte_size,
            cudaMemcpyHostToDevice);
        if (err != cudaSuccess) {
            // owner destructor frees the allocation
            xsink->raiseException("ML-TENSOR-DEVICE-ERROR",
                "failed to copy %zu bytes to CUDA device " QLLD ": %s", byte_size,
                (long long)device_id, cudaGetErrorString(err));
            return nullptr;
        }
    }

    QoreBufferDeviceInfo info;
    info.kind = QoreBufferDeviceKind::Cuda;
    info.device_id = device_id;
    info.name = std::string(qore_buffer_device_kind_name(info.kind)) + ":" + std::to_string(device_id);

    const void* device_data = owner->device_ptr;
    ReferenceHolder<QoreBufferNode> dev_buf(
        QoreBufferNode::wrapExternalDeviceStorage(et, false, n, device_data, nullptr,
            std::static_pointer_cast<const void>(owner), 0, info, cudaUploadCopyToHost, xsink),
        xsink);
    if (*xsink) {
        return nullptr;
    }
    return new QoreTensor(dev_buf.release(), host->getShape());
#else
    (void)host;
    (void)kind;
    xsink->raiseException("ML-TENSOR-DEVICE-ERROR",
        "this ml build has no CUDA runtime support, so tensors cannot be uploaded to a "
        "device; rebuild with CUDA");
    return nullptr;
#endif
}
