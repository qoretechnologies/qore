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

size_t QoreBufferNode::dataByteSize(size_t n_length) const {
    return element_type == QoreBufferElementType::Bool
        ? qore_buffer_bitmap_bytes(n_length)
        : n_length * qore_buffer_element_storage_size(element_type);
}

void QoreBufferNode::resizeStorage(size_t n_length) {
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
    const uint8_t* bytes = validity_buffer.data();
    return bytes[index / 8] & (uint8_t(1) << (index % 8));
}

void QoreBufferNode::setValidityBit(size_t index, bool valid) {
    assert(nullable_elements);
    assert(index < length);
    uint8_t* bytes = validity_buffer.data();
    uint8_t mask = uint8_t(1) << (index % 8);
    bool was_valid = bytes[index / 8] & mask;
    if (valid) {
        bytes[index / 8] |= mask;
        if (!was_valid && null_count > 0) {
            --null_count;
        }
    } else {
        bytes[index / 8] &= ~mask;
        if (was_valid) {
            ++null_count;
        }
    }
}

bool QoreBufferNode::getBoolBit(size_t index) const {
    assert(element_type == QoreBufferElementType::Bool);
    assert(index < length);
    const uint8_t* bytes = data_buffer.data();
    return bytes[index / 8] & (uint8_t(1) << (index % 8));
}

void QoreBufferNode::setBoolBit(size_t index, bool value) {
    assert(element_type == QoreBufferElementType::Bool);
    assert(index < length);
    uint8_t* bytes = data_buffer.data();
    uint8_t mask = uint8_t(1) << (index % 8);
    if (value) {
        bytes[index / 8] |= mask;
    } else {
        bytes[index / 8] &= ~mask;
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
            uint8_t* ptr = data_buffer.data() + (index * qore_buffer_element_storage_size(element_type));
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
                memcpy(data_buffer.data() + (index * sizeof(v)), &v, sizeof(v));
            } else {
                memcpy(data_buffer.data() + (index * sizeof(d)), &d, sizeof(d));
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
    if (data_buffer.size() != b->data_buffer.size()
            || memcmp(data_buffer.data(), b->data_buffer.data(), data_buffer.size())) {
        return false;
    }
    if (validity_buffer.size() != b->validity_buffer.size()) {
        return false;
    }
    return !validity_buffer.size()
        || !memcmp(validity_buffer.data(), b->validity_buffer.data(), validity_buffer.size());
}

const char* QoreBufferNode::getTypeName() const {
    return getStaticTypeName();
}

QoreValue QoreBufferNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    needs_deref = false;
    return const_cast<QoreBufferNode*>(this);
}

QoreBufferNode* QoreBufferNode::copy() const {
    QoreBufferNode* rv = new QoreBufferNode(element_type, nullable_elements);
    rv->length = length;
    rv->null_count = null_count;
    rv->data_buffer = data_buffer;
    rv->validity_buffer = validity_buffer;
    return rv;
}

const QoreTypeInfo* QoreBufferNode::getTypeInfo() const {
    return qore_get_complex_buffer_type(element_type, nullable_elements);
}

const QoreTypeInfo* QoreBufferNode::getElementTypeInfo() const {
    return qore_buffer_element_scalar_type_info(element_type, nullable_elements);
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

    switch (element_type) {
        case QoreBufferElementType::Int8: {
            int8_t v;
            memcpy(&v, data_buffer.data() + index, sizeof(v));
            return static_cast<int64>(v);
        }
        case QoreBufferElementType::Int16: {
            int16_t v;
            memcpy(&v, data_buffer.data() + (index * sizeof(v)), sizeof(v));
            return static_cast<int64>(v);
        }
        case QoreBufferElementType::Int32: {
            int32_t v;
            memcpy(&v, data_buffer.data() + (index * sizeof(v)), sizeof(v));
            return static_cast<int64>(v);
        }
        case QoreBufferElementType::Int64: {
            int64_t v;
            memcpy(&v, data_buffer.data() + (index * sizeof(v)), sizeof(v));
            return static_cast<int64>(v);
        }
        case QoreBufferElementType::Float32: {
            float v;
            memcpy(&v, data_buffer.data() + (index * sizeof(v)), sizeof(v));
            return static_cast<double>(v);
        }
        case QoreBufferElementType::Float64: {
            double v;
            memcpy(&v, data_buffer.data() + (index * sizeof(v)), sizeof(v));
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
    if (value.isNothing()) {
        if (!nullable_elements) {
            xsink->raiseException("BUFFER-TYPE-ERROR", "cannot assign NOTHING to non-nullable buffer<%s> "
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

size_t QoreBufferNode::countValid() const {
    return nullable_elements ? length - static_cast<size_t>(null_count) : length;
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
        memcpy((*rv)->data_buffer.data(), data_buffer.data() + (offset * element_size), count * element_size);
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
