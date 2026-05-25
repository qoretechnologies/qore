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

#include <limits>
#include <vector>

namespace {

struct inferred_tensor_type_t {
    bool has_int = false;
    bool has_float = false;
    bool has_bool = false;
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
        default:
            xsink->raiseException("ML-TENSOR-ERROR",
                "cannot infer tensor dtype from value of type '%s'; pass a numeric, bool, list, or buffer value",
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
        return QoreBufferElementType::Float64;
    }
    if (info.has_int) {
        return QoreBufferElementType::Int64;
    }
    if (info.has_bool) {
        return QoreBufferElementType::Bool;
    }
    xsink->raiseException("ML-TENSOR-ERROR",
        "cannot infer tensor dtype from an empty list; pass dtype explicitly");
    return QoreBufferElementType::Invalid;
}

QoreTensor* QoreTensor::fromValue(QoreValue data, const QoreListNode* shape_arg,
        const QoreStringNode* dtype, ExceptionSink* xsink) {
    std::vector<int64_t> shape;
    if (parseShape(shape_arg, shape, xsink)) {
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
        if (element_type == QoreBufferElementType::String || element_type == QoreBufferElementType::Decimal128) {
            xsink->raiseException("ML-TENSOR-DTYPE-ERROR",
                "tensor dtype '%s' is not supported for ML tensors yet; use numeric or bool tensor data",
                dtype->c_str());
            return nullptr;
        }
    }

    ReferenceHolder<QoreBufferNode> buffer_holder(xsink);
    if (data.getType() == NT_BUFFER) {
        const QoreBufferNode* source = data.get<const QoreBufferNode>();
        if (source->hasNullableElements()) {
            xsink->raiseException("ML-TENSOR-ERROR",
                "cannot create a tensor from '%s' because nullable buffer elements cannot be represented in model "
                "tensors; remove or impute nulls first", data.getFullTypeName());
            return nullptr;
        }
        if (element_type != QoreBufferElementType::Invalid && source->getElementType() != element_type) {
            xsink->raiseException("ML-TENSOR-DTYPE-ERROR",
                "tensor dtype '%s' does not match source buffer type '%s'",
                qore_buffer_element_type_name(element_type),
                qore_buffer_element_type_name(source->getElementType()));
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
        QoreValue value = buffer->getReferencedEntry(offset++, xsink);
        return value.refSelf();
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
