/*
    QoreBufferNode.cpp

    Qore Programming Language

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#include <qore/Qore.h>

#ifdef _Q_WINDOWS
#include <malloc.h>
#endif

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

static constexpr size_t QORE_BUFFER_ALIGNMENT = 64;

static size_t qore_buffer_bitmap_bytes(size_t elements) {
    return ((elements + 63) / 64) * 8;
}

static void* qore_buffer_aligned_alloc(size_t size) {
    if (!size) {
        return nullptr;
    }
#ifdef _Q_WINDOWS
    void* ptr = _aligned_malloc(size, QORE_BUFFER_ALIGNMENT);
    if (!ptr) {
        throw std::bad_alloc();
    }
    return ptr;
#else
    void* ptr = nullptr;
    int rc = posix_memalign(&ptr, QORE_BUFFER_ALIGNMENT, size);
    if (rc || !ptr) {
        throw std::bad_alloc();
    }
    return ptr;
#endif
}

static void qore_buffer_aligned_free(void* ptr) {
#ifdef _Q_WINDOWS
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

QoreBufferNode::AlignedByteBuffer::AlignedByteBuffer(const AlignedByteBuffer& old) {
    resize(old.len);
    if (len) {
        memcpy(ptr, old.ptr, len);
    }
}

QoreBufferNode::AlignedByteBuffer& QoreBufferNode::AlignedByteBuffer::operator=(const AlignedByteBuffer& old) {
    if (this == &old) {
        return *this;
    }
    resize(old.len);
    if (len) {
        memcpy(ptr, old.ptr, len);
    }
    return *this;
}

QoreBufferNode::AlignedByteBuffer::~AlignedByteBuffer() {
    clear();
}

void QoreBufferNode::AlignedByteBuffer::resize(size_t n_size, bool clear_data) {
    if (n_size == len) {
        if (clear_data && ptr) {
            memset(ptr, 0, len);
        }
        return;
    }

    uint8_t* old_ptr = ptr;
    size_t old_len = len;
    ptr = static_cast<uint8_t*>(qore_buffer_aligned_alloc(n_size));
    len = n_size;
    if (ptr) {
        if (clear_data) {
            memset(ptr, 0, len);
        } else if (old_ptr) {
            size_t copy_len = old_len < len ? old_len : len;
            memcpy(ptr, old_ptr, copy_len);
            if (copy_len < len) {
                memset(ptr + copy_len, 0, len - copy_len);
            }
        } else {
            memset(ptr, 0, len);
        }
    }
    qore_buffer_aligned_free(old_ptr);
}

void QoreBufferNode::AlignedByteBuffer::clear() {
    if (ptr) {
        qore_buffer_aligned_free(ptr);
        ptr = nullptr;
    }
    len = 0;
}

const char* qore_buffer_element_type_name(QoreBufferElementType element_type) {
    switch (element_type) {
        case QoreBufferElementType::Int8:
            return "int8";
        case QoreBufferElementType::Int16:
            return "int16";
        case QoreBufferElementType::Int32:
            return "int32";
        case QoreBufferElementType::Int64:
            return "int64";
        case QoreBufferElementType::Float32:
            return "float32";
        case QoreBufferElementType::Float64:
            return "float64";
        case QoreBufferElementType::Bool:
            return "bool";
        default:
            return "invalid";
    }
}

bool qore_buffer_element_type_from_name(const char* name, QoreBufferElementType& element_type) {
    if (!strcmp(name, "int8")) {
        element_type = QoreBufferElementType::Int8;
    } else if (!strcmp(name, "int16")) {
        element_type = QoreBufferElementType::Int16;
    } else if (!strcmp(name, "int32")) {
        element_type = QoreBufferElementType::Int32;
    } else if (!strcmp(name, "int64")) {
        element_type = QoreBufferElementType::Int64;
    } else if (!strcmp(name, "float32")) {
        element_type = QoreBufferElementType::Float32;
    } else if (!strcmp(name, "float64")) {
        element_type = QoreBufferElementType::Float64;
    } else if (!strcmp(name, "bool")) {
        element_type = QoreBufferElementType::Bool;
    } else {
        element_type = QoreBufferElementType::Invalid;
        return false;
    }
    return true;
}

const QoreTypeInfo* qore_buffer_element_scalar_type_info(QoreBufferElementType element_type, bool nullable) {
    const QoreTypeInfo* rv = nullptr;
    switch (element_type) {
        case QoreBufferElementType::Int8:
        case QoreBufferElementType::Int16:
        case QoreBufferElementType::Int32:
        case QoreBufferElementType::Int64:
            rv = nullable ? bigIntOrNothingTypeInfo : bigIntTypeInfo;
            break;
        case QoreBufferElementType::Float32:
        case QoreBufferElementType::Float64:
            rv = nullable ? floatOrNothingTypeInfo : floatTypeInfo;
            break;
        case QoreBufferElementType::Bool:
            rv = nullable ? boolOrNothingTypeInfo : boolTypeInfo;
            break;
        default:
            rv = nullable ? anyTypeInfo : autoTypeInfo;
            break;
    }
    return rv;
}

size_t qore_buffer_element_storage_size(QoreBufferElementType element_type) {
    switch (element_type) {
        case QoreBufferElementType::Int8:
            return sizeof(int8_t);
        case QoreBufferElementType::Int16:
            return sizeof(int16_t);
        case QoreBufferElementType::Int32:
        case QoreBufferElementType::Float32:
            return sizeof(int32_t);
        case QoreBufferElementType::Int64:
        case QoreBufferElementType::Float64:
            return sizeof(int64_t);
        case QoreBufferElementType::Bool:
            return 0;
        default:
            return 0;
    }
}

bool qore_buffer_element_type_is_integer(QoreBufferElementType element_type) {
    return element_type == QoreBufferElementType::Int8
        || element_type == QoreBufferElementType::Int16
        || element_type == QoreBufferElementType::Int32
        || element_type == QoreBufferElementType::Int64;
}

bool qore_buffer_element_type_is_float(QoreBufferElementType element_type) {
    return element_type == QoreBufferElementType::Float32 || element_type == QoreBufferElementType::Float64;
}

static bool qore_buffer_element_type_is_numeric(QoreBufferElementType element_type) {
    return qore_buffer_element_type_is_integer(element_type) || qore_buffer_element_type_is_float(element_type);
}

static bool qore_buffer_op_is_comparison(QoreBufferBinaryOperation op) {
    switch (op) {
        case QoreBufferBinaryOperation::Equal:
        case QoreBufferBinaryOperation::NotEqual:
        case QoreBufferBinaryOperation::LessThan:
        case QoreBufferBinaryOperation::LessThanOrEqual:
        case QoreBufferBinaryOperation::GreaterThan:
        case QoreBufferBinaryOperation::GreaterThanOrEqual:
            return true;
        default:
            return false;
    }
}

static const char* qore_buffer_op_symbol(QoreBufferBinaryOperation op) {
    switch (op) {
        case QoreBufferBinaryOperation::Add:
            return "+";
        case QoreBufferBinaryOperation::Subtract:
            return "-";
        case QoreBufferBinaryOperation::Multiply:
            return "*";
        case QoreBufferBinaryOperation::Divide:
            return "/";
        case QoreBufferBinaryOperation::Equal:
            return "==";
        case QoreBufferBinaryOperation::NotEqual:
            return "!=";
        case QoreBufferBinaryOperation::LessThan:
            return "<";
        case QoreBufferBinaryOperation::LessThanOrEqual:
            return "<=";
        case QoreBufferBinaryOperation::GreaterThan:
            return ">";
        case QoreBufferBinaryOperation::GreaterThanOrEqual:
            return ">=";
        default:
            return "?";
    }
}

bool qore_buffer_binary_op_applies(const QoreValue& left, const QoreValue& right) {
    return left.getType() == NT_BUFFER || right.getType() == NT_BUFFER;
}

struct qore_buffer_parse_operand_t {
    bool is_buffer = false;
    bool has_specific_buffer_type = false;
    QoreBufferElementType element_type = QoreBufferElementType::Invalid;
    bool nullable_elements = false;
    bool may_be_nothing = false;
    bool may_be_null = false;
    bool can_be_int = false;
    bool can_be_float = false;
    bool can_be_bool = false;
    bool known = false;
};

static qore_buffer_parse_operand_t qore_buffer_parse_operand(const QoreTypeInfo* typeInfo) {
    qore_buffer_parse_operand_t rv;
    if (!typeInfo) {
        return rv;
    }
    rv.known = QoreTypeInfo::hasType(typeInfo);
    rv.may_be_nothing = QoreTypeInfo::parseReturns(typeInfo, NT_NOTHING) != QTI_NOT_EQUAL;
    rv.may_be_null = QoreTypeInfo::parseReturns(typeInfo, NT_NULL) != QTI_NOT_EQUAL;

    const QoreTypeInfo* buffer_type = QoreTypeInfo::getReturnComplexBufferOrNothing(typeInfo);
    if (buffer_type) {
        rv.is_buffer = true;
        if (const QoreComplexBufferTypeInfo* bti = QoreTypeInfo::getComplexBufferType(buffer_type)) {
            rv.has_specific_buffer_type = true;
            rv.element_type = bti->getBufferElementType();
            rv.nullable_elements = bti->hasNullableElements();
        }
        return rv;
    }

    if (QoreTypeInfo::isType(typeInfo, NT_BUFFER)) {
        rv.is_buffer = true;
        return rv;
    }

    rv.can_be_int = QoreTypeInfo::parseReturns(typeInfo, NT_INT) != QTI_NOT_EQUAL;
    rv.can_be_float = QoreTypeInfo::parseReturns(typeInfo, NT_FLOAT) != QTI_NOT_EQUAL;
    rv.can_be_bool = QoreTypeInfo::parseReturns(typeInfo, NT_BOOLEAN) != QTI_NOT_EQUAL;
    return rv;
}

static bool qore_buffer_parse_operand_is_numeric_scalar(const qore_buffer_parse_operand_t& operand) {
    return !operand.is_buffer && (operand.can_be_int || operand.can_be_float);
}

static bool qore_buffer_parse_operand_is_bool_scalar(const qore_buffer_parse_operand_t& operand) {
    return !operand.is_buffer && operand.can_be_bool && !operand.can_be_int && !operand.can_be_float;
}

static bool qore_buffer_parse_operand_is_null_scalar(const qore_buffer_parse_operand_t& operand) {
    return !operand.is_buffer && !operand.can_be_int && !operand.can_be_float && !operand.can_be_bool
        && (operand.may_be_nothing || operand.may_be_null);
}

static bool qore_buffer_parse_operand_is_supported_for_comparison(const qore_buffer_parse_operand_t& buffer_operand,
        const qore_buffer_parse_operand_t& other_operand) {
    assert(buffer_operand.is_buffer);
    if (!buffer_operand.has_specific_buffer_type) {
        return true;
    }
    if (qore_buffer_parse_operand_is_null_scalar(other_operand)) {
        return true;
    }
    if (qore_buffer_element_type_is_numeric(buffer_operand.element_type)) {
        return other_operand.is_buffer
            ? (!other_operand.has_specific_buffer_type
                || qore_buffer_element_type_is_numeric(other_operand.element_type))
            : qore_buffer_parse_operand_is_numeric_scalar(other_operand);
    }
    if (buffer_operand.element_type == QoreBufferElementType::Bool) {
        return other_operand.is_buffer
            ? (!other_operand.has_specific_buffer_type
                || other_operand.element_type == QoreBufferElementType::Bool)
            : qore_buffer_parse_operand_is_bool_scalar(other_operand);
    }
    return false;
}

static bool qore_buffer_parse_operand_is_supported_for_arithmetic(const qore_buffer_parse_operand_t& buffer_operand,
        const qore_buffer_parse_operand_t& other_operand) {
    assert(buffer_operand.is_buffer);
    if (!buffer_operand.has_specific_buffer_type) {
        return true;
    }
    if (!qore_buffer_element_type_is_numeric(buffer_operand.element_type)) {
        return false;
    }
    if (qore_buffer_parse_operand_is_null_scalar(other_operand)) {
        return true;
    }
    return other_operand.is_buffer
        ? (!other_operand.has_specific_buffer_type || qore_buffer_element_type_is_numeric(other_operand.element_type))
        : qore_buffer_parse_operand_is_numeric_scalar(other_operand);
}

const QoreTypeInfo* qore_buffer_binary_op_type(const QoreTypeInfo* leftTypeInfo, const QoreTypeInfo* rightTypeInfo,
        QoreBufferBinaryOperation op) {
    qore_buffer_parse_operand_t left = qore_buffer_parse_operand(leftTypeInfo);
    qore_buffer_parse_operand_t right = qore_buffer_parse_operand(rightTypeInfo);

    if (!left.is_buffer && !right.is_buffer) {
        return nullptr;
    }

    const qore_buffer_parse_operand_t& buffer_operand = left.is_buffer ? left : right;
    const qore_buffer_parse_operand_t& other_operand = left.is_buffer ? right : left;
    bool comparison = qore_buffer_op_is_comparison(op);
    bool supported = comparison
        ? qore_buffer_parse_operand_is_supported_for_comparison(buffer_operand, other_operand)
        : qore_buffer_parse_operand_is_supported_for_arithmetic(buffer_operand, other_operand);
    if (!supported) {
        return nothingTypeInfo;
    }

    if (comparison) {
        bool nullable = (left.is_buffer && left.nullable_elements) || (right.is_buffer && right.nullable_elements)
            || left.may_be_nothing || right.may_be_nothing || left.may_be_null || right.may_be_null;
        return qore_get_complex_buffer_type(QoreBufferElementType::Bool, nullable);
    }

    if (!buffer_operand.has_specific_buffer_type || (other_operand.is_buffer && !other_operand.has_specific_buffer_type)) {
        return bufferTypeInfo;
    }

    bool nullable = (left.is_buffer && left.nullable_elements) || (right.is_buffer && right.nullable_elements)
        || left.may_be_nothing || right.may_be_nothing || left.may_be_null || right.may_be_null;
    bool floating = qore_buffer_element_type_is_float(buffer_operand.element_type)
        || (other_operand.is_buffer
            ? qore_buffer_element_type_is_float(other_operand.element_type)
            : other_operand.can_be_float);
    return qore_get_complex_buffer_type(floating ? QoreBufferElementType::Float64 : QoreBufferElementType::Int64,
        nullable);
}

struct qore_buffer_runtime_operand_t {
    const QoreBufferNode* buffer = nullptr;
    QoreValue scalar;
};

static QoreBufferElementType qore_buffer_runtime_operand_element_type(const qore_buffer_runtime_operand_t& operand) {
    if (operand.buffer) {
        return operand.buffer->getElementType();
    }
    switch (operand.scalar.getType()) {
        case NT_INT:
            return QoreBufferElementType::Int64;
        case NT_FLOAT:
            return QoreBufferElementType::Float64;
        case NT_BOOLEAN:
            return QoreBufferElementType::Bool;
        default:
            return QoreBufferElementType::Invalid;
    }
}

static bool qore_buffer_runtime_operand_is_null_scalar(const qore_buffer_runtime_operand_t& operand) {
    return !operand.buffer && (operand.scalar.isNothing() || operand.scalar.getType() == NT_NULL);
}

static bool qore_buffer_runtime_operand_nullable(const qore_buffer_runtime_operand_t& operand) {
    return operand.buffer ? operand.buffer->hasNullableElements() : qore_buffer_runtime_operand_is_null_scalar(operand);
}

static QoreValue qore_buffer_runtime_operand_value(const qore_buffer_runtime_operand_t& operand, size_t index,
        bool& is_null) {
    if (operand.buffer) {
        is_null = operand.buffer->isElementNull(index);
        return is_null ? QoreValue() : operand.buffer->getReferencedEntry(index);
    }
    is_null = qore_buffer_runtime_operand_is_null_scalar(operand);
    return is_null ? QoreValue() : operand.scalar;
}

static bool qore_buffer_runtime_operand_is_numeric(const qore_buffer_runtime_operand_t& operand) {
    QoreBufferElementType element_type = qore_buffer_runtime_operand_element_type(operand);
    return qore_buffer_element_type_is_numeric(element_type);
}

static bool qore_buffer_runtime_operand_is_bool(const qore_buffer_runtime_operand_t& operand) {
    return qore_buffer_runtime_operand_element_type(operand) == QoreBufferElementType::Bool;
}

static int qore_buffer_validate_runtime_operands(const qore_buffer_runtime_operand_t& left,
        const qore_buffer_runtime_operand_t& right, QoreBufferBinaryOperation op, ExceptionSink* xsink) {
    assert(left.buffer || right.buffer);

    if (left.buffer && right.buffer && left.buffer->size() != right.buffer->size()) {
        xsink->raiseException("BUFFER-SIZE-ERROR", "cannot apply '%s' to buffer<%s> with length " QSD
            " and buffer<%s> with length " QSD "; buffer operands must have the same length",
            qore_buffer_op_symbol(op), qore_buffer_element_type_name(left.buffer->getElementType()),
            left.buffer->size(), qore_buffer_element_type_name(right.buffer->getElementType()), right.buffer->size());
        return -1;
    }

    bool comparison = qore_buffer_op_is_comparison(op);
    bool left_null_scalar = qore_buffer_runtime_operand_is_null_scalar(left);
    bool right_null_scalar = qore_buffer_runtime_operand_is_null_scalar(right);
    if (comparison) {
        if (left_null_scalar || right_null_scalar) {
            const qore_buffer_runtime_operand_t& other = left_null_scalar ? right : left;
            if (qore_buffer_runtime_operand_is_numeric(other) || qore_buffer_runtime_operand_is_bool(other)) {
                return 0;
            }
        }
        bool numeric = qore_buffer_runtime_operand_is_numeric(left) && qore_buffer_runtime_operand_is_numeric(right);
        bool boolean = qore_buffer_runtime_operand_is_bool(left) && qore_buffer_runtime_operand_is_bool(right);
        if (numeric || boolean) {
            return 0;
        }
        xsink->raiseException("BUFFER-OPERATION-ERROR", "cannot apply '%s' to %s and %s; buffer comparisons "
            "support numeric buffers with int/float operands or bool buffers with bool operands",
            qore_buffer_op_symbol(op), left.buffer ? QoreTypeInfo::getName(left.buffer->getTypeInfo())
                : left.scalar.getFullTypeName(),
            right.buffer ? QoreTypeInfo::getName(right.buffer->getTypeInfo()) : right.scalar.getFullTypeName());
        return -1;
    }

    if (left_null_scalar || right_null_scalar) {
        const qore_buffer_runtime_operand_t& other = left_null_scalar ? right : left;
        if (qore_buffer_runtime_operand_is_numeric(other)) {
            return 0;
        }
    }

    if (qore_buffer_runtime_operand_is_numeric(left) && qore_buffer_runtime_operand_is_numeric(right)) {
        return 0;
    }

    xsink->raiseException("BUFFER-OPERATION-ERROR", "cannot apply '%s' to %s and %s; buffer arithmetic supports "
        "int/float buffers with int/float operands",
        qore_buffer_op_symbol(op), left.buffer ? QoreTypeInfo::getName(left.buffer->getTypeInfo())
            : left.scalar.getFullTypeName(),
        right.buffer ? QoreTypeInfo::getName(right.buffer->getTypeInfo()) : right.scalar.getFullTypeName());
    return -1;
}

static QoreBufferElementType qore_buffer_arithmetic_result_element_type(const qore_buffer_runtime_operand_t& left,
        const qore_buffer_runtime_operand_t& right) {
    return qore_buffer_element_type_is_float(qore_buffer_runtime_operand_element_type(left))
        || qore_buffer_element_type_is_float(qore_buffer_runtime_operand_element_type(right))
        ? QoreBufferElementType::Float64
        : QoreBufferElementType::Int64;
}

static QoreValue qore_buffer_compute_arithmetic_value(QoreValue left, QoreValue right,
        QoreBufferBinaryOperation op, QoreBufferElementType result_element_type, ExceptionSink* xsink) {
    if (result_element_type == QoreBufferElementType::Float64) {
        double l = left.getAsFloat();
        double r = right.getAsFloat();
        switch (op) {
            case QoreBufferBinaryOperation::Add:
                return l + r;
            case QoreBufferBinaryOperation::Subtract:
                return l - r;
            case QoreBufferBinaryOperation::Multiply:
                return l * r;
            case QoreBufferBinaryOperation::Divide:
                if (!r) {
                    xsink->raiseException("DIVISION-BY-ZERO", "division by zero found in buffer floating-point "
                        "expression");
                    return QoreValue();
                }
                return l / r;
            default:
                assert(false);
                return QoreValue();
        }
    }

    int64 l = left.getAsBigInt();
    int64 r = right.getAsBigInt();
    switch (op) {
        case QoreBufferBinaryOperation::Add:
            return l + r;
        case QoreBufferBinaryOperation::Subtract:
            return l - r;
        case QoreBufferBinaryOperation::Multiply:
            return l * r;
        case QoreBufferBinaryOperation::Divide:
            if (!r) {
                xsink->raiseException("DIVISION-BY-ZERO", "division by zero found in buffer integer expression");
                return QoreValue();
            }
            return l / r;
        default:
            assert(false);
            return QoreValue();
    }
}

static bool qore_buffer_compute_comparison_value(QoreValue left, QoreValue right, QoreBufferBinaryOperation op) {
    if (left.getType() == NT_BOOLEAN && right.getType() == NT_BOOLEAN) {
        bool l = left.getAsBool();
        bool r = right.getAsBool();
        switch (op) {
            case QoreBufferBinaryOperation::Equal:
                return l == r;
            case QoreBufferBinaryOperation::NotEqual:
                return l != r;
            case QoreBufferBinaryOperation::LessThan:
                return !l && r;
            case QoreBufferBinaryOperation::LessThanOrEqual:
                return !l || r;
            case QoreBufferBinaryOperation::GreaterThan:
                return l && !r;
            case QoreBufferBinaryOperation::GreaterThanOrEqual:
                return l || !r;
            default:
                assert(false);
                return false;
        }
    }

    if (left.getType() == NT_FLOAT || right.getType() == NT_FLOAT) {
        double l = left.getAsFloat();
        double r = right.getAsFloat();
        switch (op) {
            case QoreBufferBinaryOperation::Equal:
                return l == r;
            case QoreBufferBinaryOperation::NotEqual:
                return l != r;
            case QoreBufferBinaryOperation::LessThan:
                return l < r;
            case QoreBufferBinaryOperation::LessThanOrEqual:
                return l <= r;
            case QoreBufferBinaryOperation::GreaterThan:
                return l > r;
            case QoreBufferBinaryOperation::GreaterThanOrEqual:
                return l >= r;
            default:
                assert(false);
                return false;
        }
    }

    int64 l = left.getAsBigInt();
    int64 r = right.getAsBigInt();
    switch (op) {
        case QoreBufferBinaryOperation::Equal:
            return l == r;
        case QoreBufferBinaryOperation::NotEqual:
            return l != r;
        case QoreBufferBinaryOperation::LessThan:
            return l < r;
        case QoreBufferBinaryOperation::LessThanOrEqual:
            return l <= r;
        case QoreBufferBinaryOperation::GreaterThan:
            return l > r;
        case QoreBufferBinaryOperation::GreaterThanOrEqual:
            return l >= r;
        default:
            assert(false);
            return false;
    }
}

QoreValue qore_buffer_binary_op(const QoreValue& left, const QoreValue& right, QoreBufferBinaryOperation op,
        ExceptionSink* xsink) {
    if (!qore_buffer_binary_op_applies(left, right)) {
        return QoreValue();
    }

    qore_buffer_runtime_operand_t l{left.getType() == NT_BUFFER ? left.get<const QoreBufferNode>() : nullptr, left};
    qore_buffer_runtime_operand_t r{right.getType() == NT_BUFFER ? right.get<const QoreBufferNode>() : nullptr, right};
    if (qore_buffer_validate_runtime_operands(l, r, op, xsink)) {
        return QoreValue();
    }

    bool comparison = qore_buffer_op_is_comparison(op);
    size_t length = l.buffer ? l.buffer->size() : r.buffer->size();
    bool nullable = qore_buffer_runtime_operand_nullable(l) || qore_buffer_runtime_operand_nullable(r);
    QoreBufferElementType result_element_type = comparison
        ? QoreBufferElementType::Bool
        : qore_buffer_arithmetic_result_element_type(l, r);

    ReferenceHolder<QoreBufferNode> rv(new QoreBufferNode(result_element_type, nullable, length), xsink);
    for (size_t i = 0; i < length; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "buffer elementwise operation")) {
            return QoreValue();
        }

        bool left_null = false;
        bool right_null = false;
        QoreValue lv = qore_buffer_runtime_operand_value(l, i, left_null);
        QoreValue rv_value = qore_buffer_runtime_operand_value(r, i, right_null);
        if (left_null || right_null) {
            if ((*rv)->setEntry(i, QoreValue(), xsink)) {
                return QoreValue();
            }
            continue;
        }

        QoreValue value = comparison
            ? QoreValue(qore_buffer_compute_comparison_value(lv, rv_value, op))
            : qore_buffer_compute_arithmetic_value(lv, rv_value, op, result_element_type, xsink);
        if (*xsink || (*rv)->setEntry(i, value, xsink)) {
            return QoreValue();
        }
    }

    return rv.release();
}

QoreBufferNode::QoreBufferNode(QoreBufferElementType element_type, bool nullable_elements)
        : AbstractQoreNode(NT_BUFFER, true, false), element_type(element_type),
        nullable_elements(nullable_elements) {
    assert(element_type != QoreBufferElementType::Invalid);
}

QoreBufferNode::QoreBufferNode(QoreBufferElementType element_type, bool nullable_elements, size_t length)
        : QoreBufferNode(element_type, nullable_elements) {
    resizeStorage(length);
    if (nullable_elements && length) {
        memset(validity_buffer.data(), 0xff, validity_buffer.size());
        null_count = 0;
    }
}

QoreBufferNode::QoreBufferNode(QoreBufferElementType element_type, bool nullable_elements, const QoreListNode* list,
        ExceptionSink* xsink) : QoreBufferNode(element_type, nullable_elements, list ? list->size() : 0) {
    if (!list) {
        return;
    }

    ConstListIterator i(list);
    while (i.next()) {
        if (i.index() && !(i.index() % 100) && qore_check_cancel(xsink, "buffer construction")) {
            return;
        }
        if (setEntry(i.index(), i.getValue(), xsink)) {
            return;
        }
    }
}

QoreBufferNode::QoreBufferNode(QoreBufferNode* parent, size_t offset, size_t length)
        : AbstractQoreNode(NT_BUFFER, true, false), element_type(parent->element_type),
        nullable_elements(parent->nullable_elements), length(length), view_parent(parent), view_offset(offset) {
    assert(parent);
    assert(!parent->isView());
    assert(offset <= parent->length);
    assert(length <= parent->length - offset);
    view_parent->ref();
    ++view_parent->view_ref_count;
}

QoreBufferNode::~QoreBufferNode() {
    if (view_parent) {
        --view_parent->view_ref_count;
        view_parent->deref(nullptr);
    }
}

QoreBufferNode* QoreBufferNode::storageRoot() {
    return view_parent ? view_parent : this;
}

const QoreBufferNode* QoreBufferNode::storageRoot() const {
    return view_parent ? view_parent : this;
}

size_t QoreBufferNode::physicalIndex(size_t index) const {
    assert(index < length);
    return view_offset + index;
}

uint8_t* QoreBufferNode::dataBytes() {
    return storageRoot()->data_buffer.data();
}

const uint8_t* QoreBufferNode::dataBytes() const {
    return storageRoot()->data_buffer.data();
}

uint8_t* QoreBufferNode::validityBytes() {
    return storageRoot()->validity_buffer.data();
}

const uint8_t* QoreBufferNode::validityBytes() const {
    return storageRoot()->validity_buffer.data();
}

size_t QoreBufferNode::dataByteSize(size_t n_length) const {
    return element_type == QoreBufferElementType::Bool
        ? qore_buffer_bitmap_bytes(n_length)
        : n_length * qore_buffer_element_storage_size(element_type);
}

void QoreBufferNode::resizeStorage(size_t n_length) {
    assert(!isView());
    length = n_length;
    data_buffer.resize(dataByteSize(length), true);
    if (nullable_elements) {
        validity_buffer.resize(qore_buffer_bitmap_bytes(length), true);
        null_count = length ? static_cast<int64>(length) : 0;
    } else {
        validity_buffer.clear();
        null_count = 0;
    }
}

bool QoreBufferNode::getValidityBit(size_t index) const {
    assert(nullable_elements);
    assert(index < length);
    size_t physical = physicalIndex(index);
    const uint8_t* bytes = validityBytes();
    return bytes[physical / 8] & (uint8_t(1) << (physical % 8));
}

void QoreBufferNode::setValidityBit(size_t index, bool valid) {
    assert(nullable_elements);
    assert(index < length);
    QoreBufferNode* root = storageRoot();
    size_t physical = physicalIndex(index);
    uint8_t* bytes = root->validity_buffer.data();
    uint8_t mask = uint8_t(1) << (physical % 8);
    bool was_valid = bytes[physical / 8] & mask;
    if (valid) {
        bytes[physical / 8] |= mask;
        if (!was_valid && root->null_count > 0) {
            --root->null_count;
        }
    } else {
        bytes[physical / 8] &= ~mask;
        if (was_valid) {
            ++root->null_count;
        }
    }
}

bool QoreBufferNode::getBoolBit(size_t index) const {
    assert(element_type == QoreBufferElementType::Bool);
    assert(index < length);
    size_t physical = physicalIndex(index);
    const uint8_t* bytes = dataBytes();
    return bytes[physical / 8] & (uint8_t(1) << (physical % 8));
}

void QoreBufferNode::setBoolBit(size_t index, bool value) {
    assert(element_type == QoreBufferElementType::Bool);
    assert(index < length);
    size_t physical = physicalIndex(index);
    uint8_t* bytes = dataBytes();
    uint8_t mask = uint8_t(1) << (physical % 8);
    if (value) {
        bytes[physical / 8] |= mask;
    } else {
        bytes[physical / 8] &= ~mask;
    }
}

void QoreBufferNode::setNull(size_t index) {
    assert(nullable_elements);
    setValidityBit(index, false);
}

static bool qore_buffer_integer_fits(QoreBufferElementType element_type, int64 value) {
    switch (element_type) {
        case QoreBufferElementType::Int8:
            return value >= std::numeric_limits<int8_t>::min() && value <= std::numeric_limits<int8_t>::max();
        case QoreBufferElementType::Int16:
            return value >= std::numeric_limits<int16_t>::min() && value <= std::numeric_limits<int16_t>::max();
        case QoreBufferElementType::Int32:
            return value >= std::numeric_limits<int32_t>::min() && value <= std::numeric_limits<int32_t>::max();
        case QoreBufferElementType::Int64:
            return true;
        default:
            return false;
    }
}

int QoreBufferNode::setValue(size_t index, QoreValue value, ExceptionSink* xsink) {
    assert(index < length);
    if (!xsink) {
        return -1;
    }
    switch (element_type) {
        case QoreBufferElementType::Int8:
        case QoreBufferElementType::Int16:
        case QoreBufferElementType::Int32:
        case QoreBufferElementType::Int64: {
            if (value.getType() != NT_INT) {
                xsink->raiseException("BUFFER-TYPE-ERROR", "cannot assign type '%s' to buffer<%s> element %d; "
                    "expected int", value.getFullTypeName(), qore_buffer_element_type_name(element_type),
                    (int)index);
                return -1;
            }
            int64 i = value.getAsBigInt();
            if (!qore_buffer_integer_fits(element_type, i)) {
                xsink->raiseException("BUFFER-RANGE-ERROR", "integer value " QLLD " is outside the range of "
                    "buffer<%s> element %d", i, qore_buffer_element_type_name(element_type), (int)index);
                return -1;
            }
            size_t physical = physicalIndex(index);
            uint8_t* ptr = dataBytes() + (physical * qore_buffer_element_storage_size(element_type));
            switch (element_type) {
                case QoreBufferElementType::Int8: {
                    int8_t v = static_cast<int8_t>(i);
                    memcpy(ptr, &v, sizeof(v));
                    break;
                }
                case QoreBufferElementType::Int16: {
                    int16_t v = static_cast<int16_t>(i);
                    memcpy(ptr, &v, sizeof(v));
                    break;
                }
                case QoreBufferElementType::Int32: {
                    int32_t v = static_cast<int32_t>(i);
                    memcpy(ptr, &v, sizeof(v));
                    break;
                }
                case QoreBufferElementType::Int64: {
                    int64_t v = static_cast<int64_t>(i);
                    memcpy(ptr, &v, sizeof(v));
                    break;
                }
                default:
                    assert(false);
            }
            break;
        }
        case QoreBufferElementType::Float32:
        case QoreBufferElementType::Float64: {
            qore_type_t t = value.getType();
            if (t != NT_FLOAT && t != NT_INT) {
                xsink->raiseException("BUFFER-TYPE-ERROR", "cannot assign type '%s' to buffer<%s> element %d; "
                    "expected float or int", value.getFullTypeName(), qore_buffer_element_type_name(element_type),
                    (int)index);
                return -1;
            }
            double d = value.getAsFloat();
            if (element_type == QoreBufferElementType::Float32) {
                if (!std::isfinite(d) || d < -std::numeric_limits<float>::max()
                        || d > std::numeric_limits<float>::max()) {
                    xsink->raiseException("BUFFER-RANGE-ERROR", "floating-point value %f is outside the range of "
                        "buffer<float32> element %d", d, (int)index);
                    return -1;
                }
                float v = static_cast<float>(d);
                memcpy(dataBytes() + (physicalIndex(index) * sizeof(v)), &v, sizeof(v));
            } else {
                memcpy(dataBytes() + (physicalIndex(index) * sizeof(d)), &d, sizeof(d));
            }
            break;
        }
        case QoreBufferElementType::Bool: {
            if (value.getType() != NT_BOOLEAN) {
                xsink->raiseException("BUFFER-TYPE-ERROR", "cannot assign type '%s' to buffer<bool> element %d; "
                    "expected bool", value.getFullTypeName(), (int)index);
                return -1;
            }
            setBoolBit(index, value.getAsBool());
            break;
        }
        default:
            assert(false);
    }
    if (nullable_elements) {
        setValidityBit(index, true);
    }
    return 0;
}

bool QoreBufferNode::getAsBoolImpl() const {
    if (runtime_check_parse_option(PO_STRICT_BOOLEAN_EVAL)) {
        return false;
    }
    return !empty();
}

int QoreBufferNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    str.sprintf("%sbuffer<%s> object %p (" QSD " element%s)",
        nullable_elements ? "nullable " : "", qore_buffer_element_type_name(element_type), this, size(),
        size() == 1 ? "" : "s");
    return 0;
}

QoreString* QoreBufferNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = true;
    QoreString* rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

AbstractQoreNode* QoreBufferNode::realCopy() const {
    return copy();
}

bool QoreBufferNode::is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    return is_equal_hard(v, xsink);
}

bool QoreBufferNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    const QoreBufferNode* b = dynamic_cast<const QoreBufferNode*>(v);
    if (!b || element_type != b->element_type || nullable_elements != b->nullable_elements || length != b->length) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        if (xsink && i && !(i % 100) && qore_check_cancel(xsink, "buffer comparison")) {
            return false;
        }
        if (isElementNull(i) != b->isElementNull(i)) {
            return false;
        }
        if (!isElementNull(i)) {
            switch (element_type) {
                case QoreBufferElementType::Int8:
                case QoreBufferElementType::Int16:
                case QoreBufferElementType::Int32:
                case QoreBufferElementType::Int64:
                    if (getReferencedEntry(i).getAsBigInt() != b->getReferencedEntry(i).getAsBigInt()) {
                        return false;
                    }
                    break;
                case QoreBufferElementType::Float32:
                case QoreBufferElementType::Float64:
                    if (getReferencedEntry(i).getAsFloat() != b->getReferencedEntry(i).getAsFloat()) {
                        return false;
                    }
                    break;
                case QoreBufferElementType::Bool:
                    if (getBoolBit(i) != b->getBoolBit(i)) {
                        return false;
                    }
                    break;
                default:
                    assert(false);
            }
        }
    }
    return true;
}

const char* QoreBufferNode::getTypeName() const {
    return getStaticTypeName();
}

QoreValue QoreBufferNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    needs_deref = false;
    return const_cast<QoreBufferNode*>(this);
}

QoreBufferNode* QoreBufferNode::copy(ExceptionSink* xsink) const {
    return copy(nullable_elements, xsink);
}

QoreBufferNode* QoreBufferNode::copy(bool n_nullable_elements, ExceptionSink* xsink) const {
    assert(n_nullable_elements || !nullable_elements);

    QoreBufferNode* rv = new QoreBufferNode(element_type, n_nullable_elements, length);
    if (element_type == QoreBufferElementType::Bool) {
        for (size_t i = 0; i < length; ++i) {
            if (xsink && i && !(i % 100) && qore_check_cancel(xsink, "buffer copy")) {
                rv->deref(nullptr);
                return nullptr;
            }
            rv->setBoolBit(i, getBoolBit(i));
        }
    } else if (length) {
        size_t element_size = qore_buffer_element_storage_size(element_type);
        memcpy(rv->data_buffer.data(), static_cast<const uint8_t*>(getRawData()), length * element_size);
    }

    if (n_nullable_elements) {
        if (nullable_elements) {
            for (size_t i = 0; i < length; ++i) {
                if (xsink && i && !(i % 100) && qore_check_cancel(xsink, "buffer copy validity")) {
                    rv->deref(nullptr);
                    return nullptr;
                }
                rv->setValidityBit(i, !isElementNull(i));
            }
        } else {
            if (length) {
                memset(rv->validity_buffer.data(), 0xff, rv->validity_buffer.size());
            }
            rv->null_count = 0;
        }
    }
    return rv;
}

const QoreTypeInfo* QoreBufferNode::getTypeInfo() const {
    return qore_get_complex_buffer_type(element_type, nullable_elements);
}

const QoreTypeInfo* QoreBufferNode::getElementTypeInfo() const {
    return qore_buffer_element_scalar_type_info(element_type, nullable_elements);
}

void* QoreBufferNode::getRawData() {
    if (!length) {
        return nullptr;
    }
    size_t physical = physicalIndex(0);
    if (element_type == QoreBufferElementType::Bool) {
        return dataBytes() + (physical / 8);
    }
    return dataBytes() + (physical * qore_buffer_element_storage_size(element_type));
}

const void* QoreBufferNode::getRawData() const {
    if (!length) {
        return nullptr;
    }
    size_t physical = physicalIndex(0);
    if (element_type == QoreBufferElementType::Bool) {
        return dataBytes() + (physical / 8);
    }
    return dataBytes() + (physical * qore_buffer_element_storage_size(element_type));
}

bool QoreBufferNode::isElementNull(size_t index) const {
    if (index >= length || !nullable_elements) {
        return false;
    }
    return !getValidityBit(index);
}

QoreValue QoreBufferNode::getReferencedEntry(size_t index) const {
    if (index >= length || isElementNull(index)) {
        return QoreValue();
    }

    size_t physical = physicalIndex(index);
    switch (element_type) {
        case QoreBufferElementType::Int8: {
            int8_t v;
            memcpy(&v, dataBytes() + physical, sizeof(v));
            return static_cast<int64>(v);
        }
        case QoreBufferElementType::Int16: {
            int16_t v;
            memcpy(&v, dataBytes() + (physical * sizeof(v)), sizeof(v));
            return static_cast<int64>(v);
        }
        case QoreBufferElementType::Int32: {
            int32_t v;
            memcpy(&v, dataBytes() + (physical * sizeof(v)), sizeof(v));
            return static_cast<int64>(v);
        }
        case QoreBufferElementType::Int64: {
            int64_t v;
            memcpy(&v, dataBytes() + (physical * sizeof(v)), sizeof(v));
            return static_cast<int64>(v);
        }
        case QoreBufferElementType::Float32: {
            float v;
            memcpy(&v, dataBytes() + (physical * sizeof(v)), sizeof(v));
            return static_cast<double>(v);
        }
        case QoreBufferElementType::Float64: {
            double v;
            memcpy(&v, dataBytes() + (physical * sizeof(v)), sizeof(v));
            return v;
        }
        case QoreBufferElementType::Bool:
            return getBoolBit(index);
        default:
            assert(false);
            return QoreValue();
    }
}

int QoreBufferNode::setEntry(size_t index, QoreValue value, ExceptionSink* xsink) {
    if (!xsink) {
        return -1;
    }
    if (index >= length) {
        xsink->raiseException("BUFFER-INDEX-ERROR", "buffer<%s> index %d is out of range for buffer length " QSD,
            qore_buffer_element_type_name(element_type), (int)index, length);
        return -1;
    }
    if (value.isNothing() || value.getType() == NT_NULL) {
        if (!nullable_elements) {
            xsink->raiseException("BUFFER-TYPE-ERROR", "cannot assign NOTHING or NULL to non-nullable buffer<%s> "
                "element %d", qore_buffer_element_type_name(element_type), (int)index);
            return -1;
        }
        setNull(index);
        return 0;
    }
    return setValue(index, value, xsink);
}

QoreListNode* QoreBufferNode::toList(ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(getElementTypeInfo()), xsink);
    for (size_t i = 0; i < length; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "buffer toList")) {
            return nullptr;
        }
        rv->push(getReferencedEntry(i), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    return rv.release();
}

size_t QoreBufferNode::countValid(ExceptionSink* xsink) const {
    if (!nullable_elements) {
        return length;
    }
    if (!isView()) {
        return length - static_cast<size_t>(null_count);
    }
    size_t rv = 0;
    for (size_t i = 0; i < length; ++i) {
        if (xsink && i && !(i % 100) && qore_check_cancel(xsink, "buffer count")) {
            return 0;
        }
        if (!isElementNull(i)) {
            ++rv;
        }
    }
    return rv;
}

QoreValue QoreBufferNode::sum(ExceptionSink* xsink) const {
    if (qore_buffer_element_type_is_float(element_type)) {
        double rv = 0.0;
        for (size_t i = 0; i < length; ++i) {
            if (xsink && i && !(i % 100) && qore_check_cancel(xsink, "buffer sum")) {
                return QoreValue();
            }
            if (isElementNull(i)) {
                continue;
            }
            rv += getReferencedEntry(i).getAsFloat();
        }
        return rv;
    }

    int64 rv = 0;
    for (size_t i = 0; i < length; ++i) {
        if (xsink && i && !(i % 100) && qore_check_cancel(xsink, "buffer sum")) {
            return QoreValue();
        }
        if (isElementNull(i)) {
            continue;
        }
        if (element_type == QoreBufferElementType::Bool) {
            rv += getBoolBit(i) ? 1 : 0;
        } else {
            rv += getReferencedEntry(i).getAsBigInt();
        }
    }
    return rv;
}

QoreValue QoreBufferNode::mean(ExceptionSink* xsink) const {
    size_t count = 0;
    double rv = 0.0;
    for (size_t i = 0; i < length; ++i) {
        if (xsink && i && !(i % 100) && qore_check_cancel(xsink, "buffer mean")) {
            return QoreValue();
        }
        if (isElementNull(i)) {
            continue;
        }
        ++count;
        if (element_type == QoreBufferElementType::Bool) {
            rv += getBoolBit(i) ? 1.0 : 0.0;
        } else {
            rv += getReferencedEntry(i).getAsFloat();
        }
    }
    return count ? QoreValue(rv / count) : QoreValue();
}

QoreValue QoreBufferNode::min(ExceptionSink* xsink) const {
    bool found = false;
    int64 int_rv = 0;
    double float_rv = 0.0;
    bool bool_rv = true;
    for (size_t i = 0; i < length; ++i) {
        if (xsink && i && !(i % 100) && qore_check_cancel(xsink, "buffer min")) {
            return QoreValue();
        }
        if (isElementNull(i)) {
            continue;
        }
        switch (element_type) {
            case QoreBufferElementType::Int8:
            case QoreBufferElementType::Int16:
            case QoreBufferElementType::Int32:
            case QoreBufferElementType::Int64: {
                int64 n = getReferencedEntry(i).getAsBigInt();
                if (!found || n < int_rv) {
                    int_rv = n;
                }
                break;
            }
            case QoreBufferElementType::Float32:
            case QoreBufferElementType::Float64: {
                double n = getReferencedEntry(i).getAsFloat();
                if (!found || n < float_rv) {
                    float_rv = n;
                }
                break;
            }
            case QoreBufferElementType::Bool: {
                bool n = getBoolBit(i);
                if (!n) {
                    bool_rv = false;
                }
                break;
            }
            default:
                assert(false);
        }
        found = true;
    }

    if (!found) {
        return QoreValue();
    }
    if (qore_buffer_element_type_is_float(element_type)) {
        return float_rv;
    }
    if (element_type == QoreBufferElementType::Bool) {
        return bool_rv;
    }
    return int_rv;
}

QoreValue QoreBufferNode::max(ExceptionSink* xsink) const {
    bool found = false;
    int64 int_rv = 0;
    double float_rv = 0.0;
    bool bool_rv = false;
    for (size_t i = 0; i < length; ++i) {
        if (xsink && i && !(i % 100) && qore_check_cancel(xsink, "buffer max")) {
            return QoreValue();
        }
        if (isElementNull(i)) {
            continue;
        }
        switch (element_type) {
            case QoreBufferElementType::Int8:
            case QoreBufferElementType::Int16:
            case QoreBufferElementType::Int32:
            case QoreBufferElementType::Int64: {
                int64 n = getReferencedEntry(i).getAsBigInt();
                if (!found || n > int_rv) {
                    int_rv = n;
                }
                break;
            }
            case QoreBufferElementType::Float32:
            case QoreBufferElementType::Float64: {
                double n = getReferencedEntry(i).getAsFloat();
                if (!found || n > float_rv) {
                    float_rv = n;
                }
                break;
            }
            case QoreBufferElementType::Bool: {
                bool n = getBoolBit(i);
                if (n) {
                    bool_rv = true;
                }
                break;
            }
            default:
                assert(false);
        }
        found = true;
    }

    if (!found) {
        return QoreValue();
    }
    if (qore_buffer_element_type_is_float(element_type)) {
        return float_rv;
    }
    if (element_type == QoreBufferElementType::Bool) {
        return bool_rv;
    }
    return int_rv;
}

QoreBufferNode* QoreBufferNode::slice(size_t offset, size_t count, ExceptionSink* xsink) const {
    assert(offset <= length);
    assert(count <= length - offset);
    ReferenceHolder<QoreBufferNode> rv(new QoreBufferNode(element_type, nullable_elements, count), xsink);

    if (element_type == QoreBufferElementType::Bool) {
        for (size_t i = 0; i < count; ++i) {
            if (xsink && i && !(i % 100) && qore_check_cancel(xsink, "buffer slice")) {
                return nullptr;
            }
            rv->setBoolBit(i, getBoolBit(offset + i));
        }
    } else if (count) {
        size_t element_size = qore_buffer_element_storage_size(element_type);
        const uint8_t* source = static_cast<const uint8_t*>(getRawData()) + (offset * element_size);
        memcpy((*rv)->data_buffer.data(), source, count * element_size);
    }

    if (nullable_elements) {
        for (size_t i = 0; i < count; ++i) {
            if (xsink && i && !(i % 100) && qore_check_cancel(xsink, "buffer slice validity")) {
                return nullptr;
            }
            rv->setValidityBit(i, getValidityBit(offset + i));
        }
    }
    return rv.release();
}

QoreBufferNode* QoreBufferNode::view(size_t offset, size_t count) const {
    assert(offset <= length);
    assert(count <= length - offset);
    QoreBufferNode* root = const_cast<QoreBufferNode*>(storageRoot());
    return new QoreBufferNode(root, view_offset + offset, count);
}

bool QoreBufferNode::isUniqueForMutation() const {
    if (isView()) {
        return true;
    }

    return reference_count() == (view_ref_count.load(std::memory_order_acquire) + 1);
}

QoreBufferNode* QoreBufferNode::sliceRange(size_t start, size_t stop, ExceptionSink* xsink) const {
    assert(start < length);
    assert(stop < length);

    if (start <= stop) {
        return slice(start, stop - start + 1, xsink);
    }

    size_t count = start - stop + 1;
    ReferenceHolder<QoreBufferNode> rv(new QoreBufferNode(element_type, nullable_elements, count), xsink);
    for (size_t i = 0; i < count; ++i) {
        if (xsink && i && !(i % 100) && qore_check_cancel(xsink, "buffer reverse slice")) {
            return nullptr;
        }
        size_t source_index = start - i;
        if (isElementNull(source_index)) {
            rv->setNull(i);
        } else if (rv->setEntry(i, getReferencedEntry(source_index), xsink)) {
            return nullptr;
        }
    }
    return rv.release();
}
