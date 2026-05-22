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
#include <string>
#include <vector>

static constexpr size_t QORE_BUFFER_ALIGNMENT = 64;

static size_t qore_buffer_bitmap_bytes(size_t elements) {
    return ((elements + 63) / 64) * 8;
}

static bool qore_buffer_external_storage_supported(QoreBufferElementType element_type) {
    switch (element_type) {
        case QoreBufferElementType::Int8:
        case QoreBufferElementType::Int16:
        case QoreBufferElementType::Int32:
        case QoreBufferElementType::Int64:
        case QoreBufferElementType::Float32:
        case QoreBufferElementType::Float64:
        case QoreBufferElementType::Bool:
            return true;
        default:
            return false;
    }
}

static int64 qore_buffer_count_nulls_bitmap(const uint8_t* validity, size_t offset, size_t length,
        ExceptionSink* xsink) {
    if (!validity || !length) {
        return 0;
    }
    int64 rv = 0;
    for (size_t i = 0; i < length; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "counting external buffer nulls")) {
            return -1;
        }
        size_t physical = offset + i;
        if (!(validity[physical / 8] & (uint8_t(1) << (physical % 8)))) {
            ++rv;
        }
    }
    return rv;
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
        case QoreBufferElementType::String:
            return "string";
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
    } else if (!strcmp(name, "string")) {
        element_type = QoreBufferElementType::String;
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
        case QoreBufferElementType::String:
            rv = nullable ? stringOrNothingTypeInfo : stringTypeInfo;
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
        case QoreBufferElementType::String:
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
    bool can_be_string = false;
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
    rv.can_be_string = QoreTypeInfo::parseReturns(typeInfo, NT_STRING) != QTI_NOT_EQUAL;
    return rv;
}

static bool qore_buffer_parse_operand_is_numeric_scalar(const qore_buffer_parse_operand_t& operand) {
    return !operand.is_buffer && (operand.can_be_int || operand.can_be_float);
}

static bool qore_buffer_parse_operand_is_bool_scalar(const qore_buffer_parse_operand_t& operand) {
    return !operand.is_buffer && operand.can_be_bool && !operand.can_be_int && !operand.can_be_float;
}

static bool qore_buffer_parse_operand_is_string_scalar(const qore_buffer_parse_operand_t& operand) {
    return !operand.is_buffer && operand.can_be_string && !operand.can_be_int && !operand.can_be_float
        && !operand.can_be_bool;
}

static bool qore_buffer_parse_operand_is_null_scalar(const qore_buffer_parse_operand_t& operand) {
    return !operand.is_buffer && !operand.can_be_int && !operand.can_be_float && !operand.can_be_bool
        && !operand.can_be_string && (operand.may_be_nothing || operand.may_be_null);
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
    if (buffer_operand.element_type == QoreBufferElementType::String) {
        return other_operand.is_buffer
            ? (!other_operand.has_specific_buffer_type
                || other_operand.element_type == QoreBufferElementType::String)
            : qore_buffer_parse_operand_is_string_scalar(other_operand);
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
        case NT_STRING:
            return QoreBufferElementType::String;
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

static bool qore_buffer_runtime_operand_is_string(const qore_buffer_runtime_operand_t& operand) {
    return qore_buffer_runtime_operand_element_type(operand) == QoreBufferElementType::String;
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
            if (qore_buffer_runtime_operand_is_numeric(other) || qore_buffer_runtime_operand_is_bool(other)
                    || qore_buffer_runtime_operand_is_string(other)) {
                return 0;
            }
        }
        bool numeric = qore_buffer_runtime_operand_is_numeric(left) && qore_buffer_runtime_operand_is_numeric(right);
        bool boolean = qore_buffer_runtime_operand_is_bool(left) && qore_buffer_runtime_operand_is_bool(right);
        bool string = qore_buffer_runtime_operand_is_string(left) && qore_buffer_runtime_operand_is_string(right);
        if (numeric || boolean || string) {
            return 0;
        }
        xsink->raiseException("BUFFER-OPERATION-ERROR", "cannot apply '%s' to %s and %s; buffer comparisons "
            "support numeric buffers with int/float operands, bool buffers with bool operands, or string buffers "
            "with string operands",
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

    if (left.getType() == NT_STRING && right.getType() == NT_STRING) {
        QoreStringNodeValueHelper lstr(left);
        QoreStringNodeValueHelper rstr(right);
        int cmp = lstr->compare(*rstr);
        switch (op) {
            case QoreBufferBinaryOperation::Equal:
                return cmp == 0;
            case QoreBufferBinaryOperation::NotEqual:
                return cmp != 0;
            case QoreBufferBinaryOperation::LessThan:
                return cmp < 0;
            case QoreBufferBinaryOperation::LessThanOrEqual:
                return cmp <= 0;
            case QoreBufferBinaryOperation::GreaterThan:
                return cmp > 0;
            case QoreBufferBinaryOperation::GreaterThanOrEqual:
                return cmp >= 0;
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
    if (element_type == QoreBufferElementType::String) {
        assignStringList(list, xsink);
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

QoreBufferNode::QoreBufferNode(QoreBufferElementType element_type, bool nullable_elements, size_t length,
        const void* data, const uint8_t* validity, std::shared_ptr<const void> owner, int64_t null_count)
        : AbstractQoreNode(NT_BUFFER, true, false), element_type(element_type),
        nullable_elements(nullable_elements), length(length), null_count(null_count), external_owner(std::move(owner)),
        external_data(static_cast<const uint8_t*>(data)), external_validity(validity) {
    assert(element_type != QoreBufferElementType::Invalid);
    assert(qore_buffer_external_storage_supported(element_type));
    assert(!length || external_data);
    assert(!length || external_owner);
}

QoreBufferNode::~QoreBufferNode() {
    if (view_parent) {
        --view_parent->view_ref_count;
        view_parent->deref(nullptr);
    }
}

QoreBufferNode* QoreBufferNode::wrapExternalStorage(QoreBufferElementType element_type, bool nullable_elements,
        size_t length, const void* data, const uint8_t* validity, std::shared_ptr<const void> owner,
        int64_t null_count, ExceptionSink* xsink) {
    return wrapExternalStorage(element_type, nullable_elements, 0, length, data, validity, std::move(owner),
        null_count, xsink);
}

QoreBufferNode* QoreBufferNode::wrapExternalStorage(QoreBufferElementType element_type, bool nullable_elements,
        size_t offset, size_t length, const void* data, const uint8_t* validity, std::shared_ptr<const void> owner,
        int64_t null_count, ExceptionSink* xsink) {
    if (!xsink) {
        return nullptr;
    }
    if (!qore_buffer_external_storage_supported(element_type)) {
        xsink->raiseException("BUFFER-TYPE-ERROR",
            "cannot wrap external storage for buffer<%s>; only fixed-width numeric and bool buffers are supported",
            qore_buffer_element_type_name(element_type));
        return nullptr;
    }
    if (offset > std::numeric_limits<size_t>::max() - length) {
        xsink->raiseException("BUFFER-EXTERNAL-STORAGE-ERROR",
            "external buffer offset " QSD " and length " QSD " overflow storage size", offset, length);
        return nullptr;
    }
    size_t root_length = offset + length;
    if (root_length && !data) {
        xsink->raiseException("BUFFER-EXTERNAL-STORAGE-ERROR",
            "cannot wrap non-empty external buffer<%s> storage without a data pointer",
            qore_buffer_element_type_name(element_type));
        return nullptr;
    }
    if (root_length && !owner) {
        xsink->raiseException("BUFFER-EXTERNAL-STORAGE-ERROR",
            "cannot wrap non-empty external buffer<%s> storage without an owner",
            qore_buffer_element_type_name(element_type));
        return nullptr;
    }
    if (nullable_elements && !validity && null_count > 0) {
        xsink->raiseException("BUFFER-EXTERNAL-STORAGE-ERROR",
            "external nullable buffer<%s> storage with null_count " QLLD " requires a validity bitmap",
            qore_buffer_element_type_name(element_type), null_count);
        return nullptr;
    }
    if (null_count < -1) {
        xsink->raiseException("BUFFER-EXTERNAL-STORAGE-ERROR",
            "external buffer<%s> null_count must be >= -1; got " QLLD,
            qore_buffer_element_type_name(element_type), null_count);
        return nullptr;
    }
    if (nullable_elements && null_count > static_cast<int64>(length)) {
        xsink->raiseException("BUFFER-EXTERNAL-STORAGE-ERROR",
            "external buffer<%s> null_count " QLLD " exceeds length " QSD,
            qore_buffer_element_type_name(element_type), null_count, length);
        return nullptr;
    }

    int64_t root_null_count = 0;
    if (nullable_elements) {
        if (!validity) {
            root_null_count = 0;
        } else if (!offset && null_count >= 0) {
            root_null_count = null_count;
        } else {
            root_null_count = qore_buffer_count_nulls_bitmap(validity, 0, root_length, xsink);
            if (*xsink) {
                return nullptr;
            }
        }
    }

    ReferenceHolder<QoreBufferNode> root(new QoreBufferNode(element_type, nullable_elements, root_length, data,
        validity, std::move(owner), root_null_count), xsink);
    if (!offset) {
        return root.release();
    }

    QoreBufferNode* rv = new QoreBufferNode(*root, offset, length);
    return rv;
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
    const QoreBufferNode* root = storageRoot();
    return root->external_data ? root->external_data : root->data_buffer.data();
}

uint8_t* QoreBufferNode::validityBytes() {
    return storageRoot()->validity_buffer.data();
}

const uint8_t* QoreBufferNode::validityBytes() const {
    const QoreBufferNode* root = storageRoot();
    return root->external_data ? root->external_validity : root->validity_buffer.data();
}

uint64_t* QoreBufferNode::stringOffsets() {
    assert(element_type == QoreBufferElementType::String);
    return reinterpret_cast<uint64_t*>(storageRoot()->data_buffer.data());
}

const uint64_t* QoreBufferNode::stringOffsets() const {
    assert(element_type == QoreBufferElementType::String);
    return reinterpret_cast<const uint64_t*>(storageRoot()->data_buffer.data());
}

char* QoreBufferNode::stringBytes() {
    assert(element_type == QoreBufferElementType::String);
    QoreBufferNode* root = storageRoot();
    return reinterpret_cast<char*>(root->data_buffer.data() + root->dataByteSize(root->length));
}

const char* QoreBufferNode::stringBytes() const {
    assert(element_type == QoreBufferElementType::String);
    const QoreBufferNode* root = storageRoot();
    return reinterpret_cast<const char*>(root->data_buffer.data() + root->dataByteSize(root->length));
}

size_t QoreBufferNode::dataByteSize(size_t n_length) const {
    if (element_type == QoreBufferElementType::Bool) {
        return qore_buffer_bitmap_bytes(n_length);
    }
    if (element_type == QoreBufferElementType::String) {
        return (n_length + 1) * sizeof(uint64_t);
    }
    return n_length * qore_buffer_element_storage_size(element_type);
}

void QoreBufferNode::resizeStorage(size_t n_length) {
    assert(!isView());
    external_owner.reset();
    external_data = nullptr;
    external_validity = nullptr;
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

int QoreBufferNode::detachExternalStorage(ExceptionSink* xsink) {
    QoreBufferNode* root = storageRoot();
    if (!root->external_data) {
        return 0;
    }
    if (!xsink) {
        return -1;
    }

    size_t data_size = root->dataByteSize(root->length);
    root->data_buffer.resize(data_size, false);
    if (data_size) {
        memcpy(root->data_buffer.data(), root->external_data, data_size);
    }

    if (root->nullable_elements) {
        size_t validity_size = qore_buffer_bitmap_bytes(root->length);
        root->validity_buffer.resize(validity_size, false);
        if (validity_size) {
            if (root->external_validity) {
                memcpy(root->validity_buffer.data(), root->external_validity, validity_size);
            } else {
                memset(root->validity_buffer.data(), 0xff, validity_size);
                root->null_count = 0;
            }
        }
    } else {
        root->validity_buffer.clear();
        root->null_count = 0;
    }

    root->external_data = nullptr;
    root->external_validity = nullptr;
    root->external_owner.reset();
    return 0;
}

int QoreBufferNode::ensureOwnedStorage(ExceptionSink* xsink) {
    return storageRoot()->detachExternalStorage(xsink);
}

bool QoreBufferNode::getValidityBit(size_t index) const {
    assert(nullable_elements);
    assert(index < length);
    size_t physical = physicalIndex(index);
    const uint8_t* bytes = validityBytes();
    if (!bytes) {
        return true;
    }
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

int QoreBufferNode::assignStringStorage(const std::vector<std::string>& values,
        const std::vector<uint8_t>& nulls, ExceptionSink* xsink) {
    assert(!isView());
    assert(element_type == QoreBufferElementType::String);
    assert(values.size() == length);
    assert(nulls.empty() || nulls.size() == length);

    size_t total = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "building string buffer offsets")) {
            return -1;
        }
        if (!nulls.empty() && nulls[i]) {
            continue;
        }
        if (values[i].size() > std::numeric_limits<size_t>::max() - total) {
            xsink->raiseException("BUFFER-RANGE-ERROR", "buffer<string> byte storage size overflow");
            return -1;
        }
        total += values[i].size();
    }

    size_t offsets_size = dataByteSize(length);
    if (total > std::numeric_limits<size_t>::max() - offsets_size) {
        xsink->raiseException("BUFFER-RANGE-ERROR", "buffer<string> storage size overflow");
        return -1;
    }
    data_buffer.resize(offsets_size + total, true);

    uint64_t* offsets = stringOffsets();
    offsets[0] = 0;
    size_t pos = 0;
    char* bytes = stringBytes();
    for (size_t i = 0; i < values.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "building string buffer data")) {
            return -1;
        }
        if (nulls.empty() || !nulls[i]) {
            const std::string& value = values[i];
            if (!value.empty()) {
                memcpy(bytes + pos, value.data(), value.size());
                pos += value.size();
            }
        }
        offsets[i + 1] = static_cast<uint64_t>(pos);
    }

    if (nullable_elements) {
        memset(validity_buffer.data(), 0, validity_buffer.size());
        null_count = static_cast<int64>(length);
        for (size_t i = 0; i < length; ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "building string buffer validity")) {
                return -1;
            }
            if (nulls.empty() || !nulls[i]) {
                setValidityBit(i, true);
            }
        }
    }
    return 0;
}

int QoreBufferNode::assignStringRange(const QoreBufferNode& source, size_t offset, size_t count, bool reverse,
        ExceptionSink* xsink) {
    assert(!isView());
    assert(element_type == QoreBufferElementType::String);
    assert(source.element_type == QoreBufferElementType::String);
    assert(count == length);
    assert(offset < source.length || !count);
    assert(!count || (reverse ? offset + 1 >= count : count <= source.length - offset));

    std::vector<std::string> values(count);
    std::vector<uint8_t> nulls(nullable_elements ? count : 0, 0);
    const uint64_t* offsets = source.stringOffsets();
    const char* bytes = source.stringBytes();
    for (size_t i = 0; i < count; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "copying string buffer data")) {
            return -1;
        }
        size_t source_index = reverse ? offset - i : offset + i;
        if (source.isElementNull(source_index)) {
            if (nullable_elements) {
                nulls[i] = 1;
            }
            continue;
        }
        size_t physical = source.physicalIndex(source_index);
        uint64_t begin = offsets[physical];
        uint64_t end = offsets[physical + 1];
        size_t len = static_cast<size_t>(end - begin);
        if (len) {
            values[i].assign(bytes + begin, len);
        }
    }
    return assignStringStorage(values, nulls, xsink);
}

int QoreBufferNode::assignStringList(const QoreListNode* list, ExceptionSink* xsink) {
    assert(element_type == QoreBufferElementType::String);
    assert(!isView());
    assert(list);

    std::vector<std::string> values(length);
    std::vector<uint8_t> nulls(nullable_elements ? length : 0, 0);
    ConstListIterator i(list);
    while (i.next()) {
        if (i.index() && !(i.index() % 100) && qore_check_cancel(xsink, "buffer<string> construction")) {
            return -1;
        }

        QoreValue value = i.getValue();
        if (value.isNothing() || value.getType() == NT_NULL) {
            if (!nullable_elements) {
                xsink->raiseException("BUFFER-TYPE-ERROR",
                    "cannot assign NOTHING or NULL to non-nullable buffer<string> element %d", (int)i.index());
                return -1;
            }
            nulls[i.index()] = 1;
            continue;
        }
        if (value.getType() != NT_STRING) {
            xsink->raiseException("BUFFER-TYPE-ERROR", "cannot assign type '%s' to buffer<string> element %d; "
                "expected string", value.getFullTypeName(), (int)i.index());
            return -1;
        }

        QoreStringValueHelper str(value, QCS_UTF8, xsink);
        if (*xsink) {
            return -1;
        }
        values[i.index()].assign(str->c_str(), str->size());
    }
    return assignStringStorage(values, nulls, xsink);
}

int QoreBufferNode::fillString(QoreValue value, ExceptionSink* xsink) {
    assert(element_type == QoreBufferElementType::String);
    assert(!isView());

    std::vector<std::string> values(length);
    std::vector<uint8_t> nulls(nullable_elements ? length : 0, 0);
    if (value.isNothing() || value.getType() == NT_NULL) {
        if (!nullable_elements) {
            xsink->raiseException("BUFFER-TYPE-ERROR",
                "cannot assign NOTHING or NULL to non-nullable buffer<string> elements");
            return -1;
        }
        for (size_t i = 0; i < length; ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "filling nullable string buffer")) {
                return -1;
            }
            nulls[i] = 1;
        }
        return assignStringStorage(values, nulls, xsink);
    }
    if (value.getType() != NT_STRING) {
        xsink->raiseException("BUFFER-TYPE-ERROR",
            "cannot assign type '%s' to buffer<string> elements; expected string", value.getFullTypeName());
        return -1;
    }

    QoreStringValueHelper str(value, QCS_UTF8, xsink);
    if (*xsink) {
        return -1;
    }
    for (size_t i = 0; i < length; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "filling string buffer")) {
            return -1;
        }
        values[i].assign(str->c_str(), str->size());
    }
    return assignStringStorage(values, nulls, xsink);
}

int QoreBufferNode::setStringValue(size_t index, QoreValue value, ExceptionSink* xsink) {
    assert(index < length);
    assert(element_type == QoreBufferElementType::String);
    if (value.getType() != NT_STRING) {
        xsink->raiseException("BUFFER-TYPE-ERROR", "cannot assign type '%s' to buffer<string> element %d; "
            "expected string", value.getFullTypeName(), (int)index);
        return -1;
    }

    QoreStringValueHelper str(value, QCS_UTF8, xsink);
    if (*xsink) {
        return -1;
    }

    QoreBufferNode* root = storageRoot();
    std::vector<std::string> values(root->length);
    std::vector<uint8_t> nulls(root->nullable_elements ? root->length : 0, 0);
    const uint64_t* offsets = root->stringOffsets();
    const char* bytes = root->stringBytes();
    size_t target = physicalIndex(index);
    for (size_t i = 0; i < root->length; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "updating string buffer")) {
            return -1;
        }
        if (i == target) {
            values[i].assign(str->c_str(), str->size());
            continue;
        }
        if (root->isElementNull(i)) {
            if (root->nullable_elements) {
                nulls[i] = 1;
            }
            continue;
        }
        uint64_t begin = offsets[i];
        uint64_t end = offsets[i + 1];
        size_t len = static_cast<size_t>(end - begin);
        if (len) {
            values[i].assign(bytes + begin, len);
        }
    }
    return root->assignStringStorage(values, nulls, xsink);
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
            if (ensureOwnedStorage(xsink)) {
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
                if (ensureOwnedStorage(xsink)) {
                    return -1;
                }
                float v = static_cast<float>(d);
                memcpy(dataBytes() + (physicalIndex(index) * sizeof(v)), &v, sizeof(v));
            } else {
                if (ensureOwnedStorage(xsink)) {
                    return -1;
                }
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
            if (ensureOwnedStorage(xsink)) {
                return -1;
            }
            setBoolBit(index, value.getAsBool());
            break;
        }
        case QoreBufferElementType::String:
            return setStringValue(index, value, xsink);
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
                case QoreBufferElementType::String:
                    {
                        size_t left_physical = physicalIndex(i);
                        size_t right_physical = b->physicalIndex(i);
                        const uint64_t* left_offsets = stringOffsets();
                        const uint64_t* right_offsets = b->stringOffsets();
                        uint64_t left_begin = left_offsets[left_physical];
                        uint64_t left_end = left_offsets[left_physical + 1];
                        uint64_t right_begin = right_offsets[right_physical];
                        uint64_t right_end = right_offsets[right_physical + 1];
                        size_t len = static_cast<size_t>(left_end - left_begin);
                        if (len != right_end - right_begin
                                || (len && memcmp(stringBytes() + left_begin, b->stringBytes() + right_begin,
                                    len))) {
                            return false;
                        }
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
    if (element_type == QoreBufferElementType::String) {
        if (rv->assignStringRange(*this, 0, length, false, xsink)) {
            rv->deref(nullptr);
            return nullptr;
        }
    } else if (element_type == QoreBufferElementType::Bool) {
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

    if (n_nullable_elements && element_type != QoreBufferElementType::String) {
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
    if (!length || hasExternalStorage()) {
        return nullptr;
    }
    if (element_type == QoreBufferElementType::String) {
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
    if (element_type == QoreBufferElementType::String) {
        return nullptr;
    }
    size_t physical = physicalIndex(0);
    if (element_type == QoreBufferElementType::Bool) {
        return dataBytes() + (physical / 8);
    }
    return dataBytes() + (physical * qore_buffer_element_storage_size(element_type));
}

const uint8_t* QoreBufferNode::getRawValidityData() const {
    if (!nullable_elements || !length) {
        return nullptr;
    }
    const uint8_t* bytes = validityBytes();
    if (!bytes) {
        return nullptr;
    }
    size_t physical = physicalIndex(0);
    return bytes + (physical / 8);
}

size_t QoreBufferNode::getRawDataBitOffset() const {
    if (!length || element_type != QoreBufferElementType::Bool) {
        return 0;
    }
    return physicalIndex(0) % 8;
}

size_t QoreBufferNode::getRawValidityBitOffset() const {
    if (!length || !nullable_elements) {
        return 0;
    }
    return physicalIndex(0) % 8;
}

bool QoreBufferNode::hasExternalStorage() const {
    return storageRoot()->external_data;
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
        case QoreBufferElementType::String: {
            const uint64_t* offsets = stringOffsets();
            const char* bytes = stringBytes();
            uint64_t begin = offsets[physical];
            uint64_t end = offsets[physical + 1];
            size_t len = static_cast<size_t>(end - begin);
            return QoreValue::makeStringValue(len ? bytes + begin : "", len, QCS_UTF8);
        }
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
        if (ensureOwnedStorage(xsink)) {
            return -1;
        }
        setNull(index);
        return 0;
    }
    return setValue(index, value, xsink);
}

int QoreBufferNode::fill(QoreValue value, ExceptionSink* xsink) {
    if (!xsink) {
        return -1;
    }
    if (element_type == QoreBufferElementType::String) {
        return fillString(value, xsink);
    }
    for (size_t i = 0; i < length; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "buffer filled construction")) {
            return -1;
        }
        if (setEntry(i, value, xsink)) {
            return -1;
        }
    }
    return 0;
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
    if (element_type == QoreBufferElementType::String) {
        xsink->raiseException("BUFFER-OPERATION-ERROR", "buffer<string>.sum() is not supported");
        return QoreValue();
    }
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
    if (element_type == QoreBufferElementType::String) {
        xsink->raiseException("BUFFER-OPERATION-ERROR", "buffer<string>.mean() is not supported");
        return QoreValue();
    }
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
    if (element_type == QoreBufferElementType::String) {
        xsink->raiseException("BUFFER-OPERATION-ERROR", "buffer<string>.min() is not supported");
        return QoreValue();
    }
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
    if (element_type == QoreBufferElementType::String) {
        xsink->raiseException("BUFFER-OPERATION-ERROR", "buffer<string>.max() is not supported");
        return QoreValue();
    }
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

    if (element_type == QoreBufferElementType::String) {
        if (rv->assignStringRange(*this, offset, count, false, xsink)) {
            return nullptr;
        }
        return rv.release();
    }

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
    if (hasExternalStorage()) {
        return false;
    }
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
    if (element_type == QoreBufferElementType::String) {
        if (rv->assignStringRange(*this, start, count, true, xsink)) {
            return nullptr;
        }
        return rv.release();
    }

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
