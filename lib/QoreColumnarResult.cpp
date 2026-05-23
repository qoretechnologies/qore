/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreColumnarResult.cpp

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.
*/

#include <qore/QoreColumnarResult.h>

#include "qore/intern/QC_ColumnarResult.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <memory>
#include <strings.h>
#include <utility>

namespace {

enum class ColumnarInferredKind {
    Unknown,
    Bool,
    Int,
    Float,
    Number,
    String,
    Date,
    Binary,
    Auto,
};

struct ColumnarShape {
    QoreColumnarColumnType column_type = QoreColumnarColumnType::Auto;
    QoreBufferElementType buffer_type = QoreBufferElementType::Invalid;
    const QoreTypeInfo* element_type = autoTypeInfo;
    bool nullable = false;
    bool dense = false;
    int32_t decimal_precision = 0;
    int32_t decimal_scale = 0;
};

static bool columnar_parse_decimal_metadata(const std::string& native_type, int32_t& precision, int32_t& scale);

static bool columnar_decimal_metadata_is_supported(int32_t precision, int32_t scale) {
    return precision > 0 && precision <= 38 && scale >= 0 && scale <= precision;
}

static void columnar_shape_set_decimal128(ColumnarShape& shape, int32_t precision, int32_t scale) {
    shape.column_type = QoreColumnarColumnType::Number;
    shape.buffer_type = QoreBufferElementType::Decimal128;
    shape.element_type = numberTypeInfo;
    shape.dense = true;
    shape.decimal_precision = precision > 0 ? precision : 38;
    shape.decimal_scale = scale >= 0 ? scale : 0;
}

static std::string columnar_lower_type(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });

    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    value = value.substr(begin, end - begin);
    size_t paren = value.find('(');
    if (paren != std::string::npos) {
        value.resize(paren);
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
            value.pop_back();
        }
    }
    return value;
}

static bool columnar_type_is_one_of(const std::string& value, std::initializer_list<const char*> names) {
    for (const char* name : names) {
        if (value == name) {
            return true;
        }
    }
    return false;
}

static std::string columnar_get_string_value(QoreValue value) {
    if (value.getType() != NT_STRING) {
        return std::string();
    }

    QoreStringValueHelper str(value);
    return std::string(str->c_str());
}

static std::string columnar_get_desc_string(const QoreHashNode* desc, const char* key) {
    return desc ? columnar_get_string_value(desc->getKeyValue(key)) : std::string();
}

static int64 columnar_get_desc_int(const QoreHashNode* desc, const char* key, int64 def = 0) {
    if (!desc) {
        return def;
    }

    QoreValue value = desc->getKeyValue(key);
    return value.getType() == NT_INT ? value.getAsBigInt() : def;
}

static bool columnar_get_desc_nullable(const QoreHashNode* desc) {
    if (!desc) {
        return false;
    }

    QoreValue value = desc->getKeyValue("nullable");
    return value.getType() == NT_BOOLEAN ? value.getAsBool() : false;
}

static const QoreHashNode* columnar_get_desc_column(const QoreHashNode* desc, const char* key) {
    if (!desc) {
        return nullptr;
    }

    QoreValue value = desc->getKeyValue(key);
    return value.getType() == NT_HASH ? value.get<const QoreHashNode>() : nullptr;
}

static bool columnar_shape_from_native_type(const std::string& native_type, int64 maxsize, ColumnarShape& shape) {
    std::string type = columnar_lower_type(native_type);
    if (type.empty()) {
        return false;
    }

    if (type.size() > 2 && type.compare(type.size() - 2, 2, "[]") == 0) {
        shape = {QoreColumnarColumnType::Auto, QoreBufferElementType::Invalid, autoTypeInfo, true, false};
        return true;
    }

    if (!type.empty() && type[0] == '_') {
        shape = {QoreColumnarColumnType::Auto, QoreBufferElementType::Invalid, autoTypeInfo, true, false};
        return true;
    }

    if (columnar_type_is_one_of(type, {"bool", "boolean", "bit", "sql_bit"})) {
        shape = {QoreColumnarColumnType::Bool, QoreBufferElementType::Bool, boolTypeInfo, false, true};
        return true;
    }

    if (columnar_type_is_one_of(type, {"tinyint", "int1", "sql_tinyint"})) {
        shape = {QoreColumnarColumnType::Int, QoreBufferElementType::Int8, bigIntTypeInfo, false, true};
        return true;
    }

    if (columnar_type_is_one_of(type, {"smallint", "int2", "sql_smallint"})) {
        shape = {QoreColumnarColumnType::Int, QoreBufferElementType::Int16, bigIntTypeInfo, false, true};
        return true;
    }

    if (columnar_type_is_one_of(type, {"integer", "int", "int4", "serial", "mediumint", "sql_integer"})) {
        shape = {QoreColumnarColumnType::Int, QoreBufferElementType::Int32, bigIntTypeInfo, false, true};
        return true;
    }

    if (columnar_type_is_one_of(type, {"bigint", "int8", "bigserial", "longlong", "sql_bigint"})) {
        shape = {QoreColumnarColumnType::Int, QoreBufferElementType::Int64, bigIntTypeInfo, false, true};
        return true;
    }

    if (columnar_type_is_one_of(type, {"real", "float4", "sql_real", "binary_float"})) {
        shape = {QoreColumnarColumnType::Float, QoreBufferElementType::Float32, floatTypeInfo, false, true};
        return true;
    }

    if (columnar_type_is_one_of(type, {"double", "double precision", "float8", "sql_double", "binary_double"})) {
        shape = {QoreColumnarColumnType::Float, QoreBufferElementType::Float64, floatTypeInfo, false, true};
        return true;
    }

    if (type == "float") {
        shape = {QoreColumnarColumnType::Float,
            maxsize == 4 ? QoreBufferElementType::Float32 : QoreBufferElementType::Float64,
            floatTypeInfo, false, true};
        return true;
    }

    if (columnar_type_is_one_of(type, {"numeric", "decimal", "number", "sql_numeric", "sql_decimal"})) {
        int32_t precision = 0;
        int32_t scale = 0;
        if (columnar_parse_decimal_metadata(native_type, precision, scale)
                && columnar_decimal_metadata_is_supported(precision, scale)) {
            shape = {QoreColumnarColumnType::Number, QoreBufferElementType::Decimal128, numberTypeInfo, false, true,
                precision, scale};
            return true;
        }
        shape = {QoreColumnarColumnType::Number, QoreBufferElementType::Invalid, numberTypeInfo, false, false};
        return true;
    }

    if (type.find("char") != std::string::npos || type.find("text") != std::string::npos
            || type.find("clob") != std::string::npos || type == "string" || type == "enum"
            || type == "set" || type == "uuid" || type == "xml") {
        shape = {QoreColumnarColumnType::String, QoreBufferElementType::String, stringTypeInfo, true, true};
        return true;
    }

    if (type.find("date") != std::string::npos || type.find("time") != std::string::npos) {
        shape = {QoreColumnarColumnType::Date, QoreBufferElementType::Invalid, dateTypeInfo, false, false};
        return true;
    }

    if (type.find("binary") != std::string::npos || type.find("blob") != std::string::npos
            || type == "bytea" || type == "raw" || type == "varbinary") {
        shape = {QoreColumnarColumnType::Binary, QoreBufferElementType::Invalid, binaryTypeInfo, false, false};
        return true;
    }

    if (type == "json" || type == "jsonb" || type == "geometry" || type == "geography") {
        shape = {QoreColumnarColumnType::Auto, QoreBufferElementType::Invalid, autoTypeInfo, true, false};
        return true;
    }

    return false;
}

static bool columnar_shape_from_desc(const QoreHashNode* desc, ColumnarShape& shape) {
    if (!desc) {
        return false;
    }

    if (columnar_shape_from_native_type(
            columnar_get_desc_string(desc, "native_type"), columnar_get_desc_int(desc, "maxsize"), shape)) {
        shape.nullable = shape.nullable || columnar_get_desc_nullable(desc);
        int32_t precision = static_cast<int32_t>(columnar_get_desc_int(desc, "precision", shape.decimal_precision));
        int32_t scale = static_cast<int32_t>(columnar_get_desc_int(desc, "scale", shape.decimal_scale));
        if (shape.column_type == QoreColumnarColumnType::Number
                && columnar_decimal_metadata_is_supported(precision, scale)) {
            columnar_shape_set_decimal128(shape, precision, scale);
        }
        return true;
    }

    switch (columnar_get_desc_int(desc, "type", NT_NOTHING)) {
        case NT_BOOLEAN:
            shape = {QoreColumnarColumnType::Bool, QoreBufferElementType::Bool, boolTypeInfo,
                columnar_get_desc_nullable(desc), true};
            return true;
        case NT_INT:
            shape = {QoreColumnarColumnType::Int, QoreBufferElementType::Int64, bigIntTypeInfo,
                columnar_get_desc_nullable(desc), true};
            return true;
        case NT_FLOAT:
            shape = {QoreColumnarColumnType::Float, QoreBufferElementType::Float64, floatTypeInfo,
                columnar_get_desc_nullable(desc), true};
            return true;
        case NT_NUMBER:
            shape = {QoreColumnarColumnType::Number, QoreBufferElementType::Invalid, numberTypeInfo,
                columnar_get_desc_nullable(desc), false};
            return true;
        case NT_STRING:
            shape = {QoreColumnarColumnType::String, QoreBufferElementType::String, stringTypeInfo, true, true};
            return true;
        case NT_DATE:
            shape = {QoreColumnarColumnType::Date, QoreBufferElementType::Invalid, dateTypeInfo,
                columnar_get_desc_nullable(desc), false};
            return true;
        case NT_BINARY:
            shape = {QoreColumnarColumnType::Binary, QoreBufferElementType::Invalid, binaryTypeInfo,
                columnar_get_desc_nullable(desc), false};
            return true;
        default:
            break;
    }

    return false;
}

static ColumnarInferredKind columnar_value_kind(QoreValue value) {
    switch (value.getType()) {
        case NT_BOOLEAN:
            return ColumnarInferredKind::Bool;
        case NT_INT:
            return ColumnarInferredKind::Int;
        case NT_FLOAT:
            return ColumnarInferredKind::Float;
        case NT_NUMBER:
            return ColumnarInferredKind::Number;
        case NT_STRING:
            return ColumnarInferredKind::String;
        case NT_DATE:
            return ColumnarInferredKind::Date;
        case NT_BINARY:
            return ColumnarInferredKind::Binary;
        default:
            return ColumnarInferredKind::Auto;
    }
}

static void columnar_merge_kind(ColumnarInferredKind& current, ColumnarInferredKind value) {
    if (current == ColumnarInferredKind::Unknown) {
        current = value;
        return;
    }
    if (current == value) {
        return;
    }
    if ((current == ColumnarInferredKind::Int && value == ColumnarInferredKind::Float)
            || (current == ColumnarInferredKind::Float && value == ColumnarInferredKind::Int)) {
        current = ColumnarInferredKind::Float;
        return;
    }
    if ((current == ColumnarInferredKind::Int || current == ColumnarInferredKind::Float
            || current == ColumnarInferredKind::Number)
            && (value == ColumnarInferredKind::Int || value == ColumnarInferredKind::Float
                || value == ColumnarInferredKind::Number)) {
        current = ColumnarInferredKind::Number;
        return;
    }
    current = ColumnarInferredKind::Auto;
}

static ColumnarShape columnar_shape_from_kind(ColumnarInferredKind kind, bool nullable) {
    switch (kind) {
        case ColumnarInferredKind::Bool:
            return {QoreColumnarColumnType::Bool, QoreBufferElementType::Bool, boolTypeInfo, nullable, true};
        case ColumnarInferredKind::Int:
            return {QoreColumnarColumnType::Int, QoreBufferElementType::Int64, bigIntTypeInfo, nullable, true};
        case ColumnarInferredKind::Float:
            return {QoreColumnarColumnType::Float, QoreBufferElementType::Float64, floatTypeInfo, nullable, true};
        case ColumnarInferredKind::Number:
            return {QoreColumnarColumnType::Number, QoreBufferElementType::Invalid, numberTypeInfo, nullable, false};
        case ColumnarInferredKind::String:
            return {QoreColumnarColumnType::String, QoreBufferElementType::String, stringTypeInfo, nullable, true};
        case ColumnarInferredKind::Date:
            return {QoreColumnarColumnType::Date, QoreBufferElementType::Invalid, dateTypeInfo, nullable, false};
        case ColumnarInferredKind::Binary:
            return {QoreColumnarColumnType::Binary, QoreBufferElementType::Invalid, binaryTypeInfo, nullable, false};
        default:
            return {QoreColumnarColumnType::Auto, QoreBufferElementType::Invalid, autoTypeInfo, true, false};
    }
}

static bool columnar_list_has_nulls(const QoreListNode* list, ExceptionSink* xsink) {
    ConstListIterator i(list);
    while (i.next()) {
        if (i.index() && !(i.index() % 100) && qore_check_cancel(xsink, "columnar DBI result null scan")) {
            return true;
        }
        qore_type_t value_type = i.getValue().getType();
        if (value_type == NT_NOTHING || value_type == NT_NULL) {
            return true;
        }
    }
    return false;
}

static ColumnarShape columnar_infer_list_shape(const QoreListNode* list, ExceptionSink* xsink) {
    ColumnarInferredKind kind = ColumnarInferredKind::Unknown;
    bool nullable = false;

    ConstListIterator i(list);
    while (i.next()) {
        if (i.index() && !(i.index() % 100) && qore_check_cancel(xsink, "columnar DBI result inference")) {
            return columnar_shape_from_kind(ColumnarInferredKind::Auto, true);
        }

        QoreValue value = i.getValue();
        if (value.getType() == NT_NOTHING || value.getType() == NT_NULL) {
            nullable = true;
            continue;
        }
        columnar_merge_kind(kind, columnar_value_kind(value));
    }

    return columnar_shape_from_kind(kind, nullable);
}

static QoreListNode* columnar_make_typed_list(const QoreListNode* source, const QoreTypeInfo* element_type,
        bool nullable, ExceptionSink* xsink) {
    const QoreTypeInfo* ti = nullable ? qore_get_or_nothing_type(element_type) : element_type;
    if (ti == autoTypeInfo) {
        return source->listRefSelf();
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(ti), xsink);
    ConstListIterator i(source);
    while (i.next()) {
        if (i.index() && !(i.index() % 100) && qore_check_cancel(xsink, "columnar DBI list conversion")) {
            return nullptr;
        }
        rv->push(i.getReferencedValue(), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    return rv.release();
}

static QoreValue columnar_make_column_value(const QoreListNode* source, const ColumnarShape& shape,
        ExceptionSink* xsink) {
    if (shape.dense) {
        if (shape.buffer_type == QoreBufferElementType::Decimal128) {
            return new QoreBufferNode(shape.buffer_type, shape.nullable, source, xsink,
                shape.decimal_precision > 0 ? shape.decimal_precision : 38, shape.decimal_scale);
        }
        return new QoreBufferNode(shape.buffer_type, shape.nullable, source, xsink);
    }
    return columnar_make_typed_list(source, shape.element_type, shape.nullable, xsink);
}

static size_t columnar_value_size(QoreValue value) {
    switch (value.getType()) {
        case NT_LIST:
            return value.get<const QoreListNode>()->size();
        case NT_BUFFER:
            return value.get<const QoreBufferNode>()->size();
        default:
            return 0;
    }
}

static QoreValue columnar_value_at(QoreValue value, size_t index, ExceptionSink* xsink) {
    switch (value.getType()) {
        case NT_LIST:
            return value.get<const QoreListNode>()->retrieveEntry(index).refSelf();
        case NT_BUFFER:
            return value.get<const QoreBufferNode>()->getReferencedEntry(index, xsink);
        default:
            return QoreValue();
    }
}

static size_t columnar_mask_size(QoreValue mask, ExceptionSink* xsink) {
    switch (mask.getType()) {
        case NT_LIST:
            return mask.get<const QoreListNode>()->size();
        case NT_BUFFER: {
            const QoreBufferNode* buffer = mask.get<const QoreBufferNode>();
            if (buffer->getElementType() != QoreBufferElementType::Bool) {
                xsink->raiseException("COLUMNAR-RESULT-ERROR",
                    "row mask buffer has element type '%s'; expected buffer<bool>",
                    qore_buffer_element_type_name(buffer->getElementType()));
                return 0;
            }
            return mask.get<const QoreBufferNode>()->size();
        }
        default:
            xsink->raiseException("COLUMNAR-RESULT-ERROR",
                "row mask has type '%s'; expected list<bool> or buffer<bool>", mask.getTypeName());
            return 0;
    }
}

static bool columnar_mask_at(QoreValue mask, size_t index, ExceptionSink* xsink) {
    switch (mask.getType()) {
        case NT_LIST:
            return mask.get<const QoreListNode>()->retrieveEntry(index).getAsBool();
        case NT_BUFFER: {
            QoreValue value = mask.get<const QoreBufferNode>()->getReferencedEntry(index, xsink);
            return !value.isNullOrNothing() && value.getAsBool();
        }
        default:
            return false;
    }
}

static size_t columnar_mask_count(QoreValue mask, ExceptionSink* xsink) {
    size_t count = 0;
    size_t size = columnar_mask_size(mask, xsink);
    if (*xsink) {
        return 0;
    }
    for (size_t i = 0; i < size; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "counting columnar row mask")) {
            return 0;
        }
        if (columnar_mask_at(mask, i, xsink)) {
            ++count;
        }
    }
    return count;
}

static QoreValue columnar_filter_value(QoreValue value, QoreValue mask, size_t selected, ExceptionSink* xsink) {
    if (value.getType() == NT_LIST) {
        const QoreListNode* source = value.get<const QoreListNode>();
        ReferenceHolder<QoreListNode> rv(new QoreListNode(source->getValueTypeInfo()), xsink);
        for (size_t i = 0; i < source->size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "filtering columnar list column")) {
                return QoreValue();
            }
            if (columnar_mask_at(mask, i, xsink)) {
                rv->push(source->retrieveEntry(i).refSelf(), xsink);
                if (*xsink) {
                    return QoreValue();
                }
            }
        }
        return rv.release();
    }

    if (value.getType() == NT_BUFFER) {
        const QoreBufferNode* source = value.get<const QoreBufferNode>();
        if (source->getElementType() == QoreBufferElementType::String) {
            ReferenceHolder<QoreListNode> values(new QoreListNode(source->getElementTypeInfo()), xsink);
            for (size_t i = 0; i < source->size(); ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "filtering columnar string buffer column")) {
                    return QoreValue();
                }
                if (columnar_mask_at(mask, i, xsink)) {
                    values->push(source->getReferencedEntry(i, xsink), xsink);
                    if (*xsink) {
                        return QoreValue();
                    }
                }
            }
            return new QoreBufferNode(source->getElementType(), source->hasNullableElements(), *values, xsink);
        }

        ReferenceHolder<QoreBufferNode> rv(new QoreBufferNode(source->getElementType(),
            source->hasNullableElements(), selected), xsink);
        size_t out = 0;
        for (size_t i = 0; i < source->size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "filtering columnar buffer column")) {
                return QoreValue();
            }
            if (columnar_mask_at(mask, i, xsink)) {
                if (rv->setEntry(out++, source->getReferencedEntry(i, xsink), xsink)) {
                    return QoreValue();
                }
            }
        }
        return rv.release();
    }

    assert(false);
    return QoreValue();
}

static QoreValue columnar_slice_value(QoreValue value, size_t offset, size_t count, ExceptionSink* xsink) {
    if (value.getType() == NT_LIST) {
        const QoreListNode* source = value.get<const QoreListNode>();
        ReferenceHolder<QoreListNode> rv(new QoreListNode(source->getValueTypeInfo()), xsink);
        size_t end = offset + count;
        for (size_t i = offset; i < end; ++i) {
            if (i != offset && !((i - offset) % 100) && qore_check_cancel(xsink, "slicing columnar list column")) {
                return QoreValue();
            }
            rv->push(source->retrieveEntry(i).refSelf(), xsink);
            if (*xsink) {
                return QoreValue();
            }
        }
        return rv.release();
    }

    if (value.getType() == NT_BUFFER) {
        return value.get<const QoreBufferNode>()->view(offset, count);
    }

    assert(false);
    return QoreValue();
}

static bool columnar_value_is_null(QoreValue value, size_t index, ExceptionSink* xsink) {
    switch (value.getType()) {
        case NT_LIST:
            return value.get<const QoreListNode>()->retrieveEntry(index).isNullOrNothing();
        case NT_BUFFER:
            return value.get<const QoreBufferNode>()->getReferencedEntry(index, xsink).isNullOrNothing();
        default:
            assert(false);
            return true;
    }
}

static bool columnar_mask_op_is_and(const char* op, ExceptionSink* xsink) {
    if (!op) {
        xsink->raiseException("COLUMNAR-RESULT-ERROR",
            "unsupported row-mask operator '<null>'; expected '&&' or '||'");
        return false;
    }
    if (!strcmp(op, "&&") || !strcasecmp(op, "AND")) {
        return true;
    }
    if (!strcmp(op, "||") || !strcasecmp(op, "OR")) {
        return false;
    }
    xsink->raiseException("COLUMNAR-RESULT-ERROR",
        "unsupported row-mask operator '%s'; expected '&&' or '||'", op);
    return false;
}

static QoreColumnarColumnType columnar_type_from_buffer(QoreBufferElementType buffer_type) {
    if (qore_buffer_element_type_is_integer(buffer_type)) {
        return QoreColumnarColumnType::Int;
    }
    if (qore_buffer_element_type_is_float(buffer_type)) {
        return QoreColumnarColumnType::Float;
    }
    if (buffer_type == QoreBufferElementType::Bool) {
        return QoreColumnarColumnType::Bool;
    }
    if (buffer_type == QoreBufferElementType::String) {
        return QoreColumnarColumnType::String;
    }
    if (buffer_type == QoreBufferElementType::Decimal128) {
        return QoreColumnarColumnType::Number;
    }
    return QoreColumnarColumnType::Auto;
}

static QoreColumnarTypeKind columnar_kind_from_flat_type(QoreColumnarColumnType type) {
    switch (type) {
        case QoreColumnarColumnType::Bool:
            return QoreColumnarTypeKind::Bool;
        case QoreColumnarColumnType::Int:
            return QoreColumnarTypeKind::Int;
        case QoreColumnarColumnType::Float:
            return QoreColumnarTypeKind::Float;
        case QoreColumnarColumnType::Number:
            return QoreColumnarTypeKind::Number;
        case QoreColumnarColumnType::String:
            return QoreColumnarTypeKind::String;
        case QoreColumnarColumnType::Date:
            return QoreColumnarTypeKind::Date;
        case QoreColumnarColumnType::Binary:
            return QoreColumnarTypeKind::Binary;
        case QoreColumnarColumnType::Auto:
        default:
            return QoreColumnarTypeKind::Auto;
    }
}

static QoreColumnarColumnType columnar_flat_type_from_kind(QoreColumnarTypeKind kind) {
    switch (kind) {
        case QoreColumnarTypeKind::Bool:
            return QoreColumnarColumnType::Bool;
        case QoreColumnarTypeKind::Int:
            return QoreColumnarColumnType::Int;
        case QoreColumnarTypeKind::Float:
            return QoreColumnarColumnType::Float;
        case QoreColumnarTypeKind::Number:
        case QoreColumnarTypeKind::Decimal128:
            return QoreColumnarColumnType::Number;
        case QoreColumnarTypeKind::String:
            return QoreColumnarColumnType::String;
        case QoreColumnarTypeKind::Date:
        case QoreColumnarTypeKind::Timestamp:
        case QoreColumnarTypeKind::Duration:
            return QoreColumnarColumnType::Date;
        case QoreColumnarTypeKind::Binary:
            return QoreColumnarColumnType::Binary;
        case QoreColumnarTypeKind::Auto:
        case QoreColumnarTypeKind::List:
        case QoreColumnarTypeKind::LargeList:
        case QoreColumnarTypeKind::FixedSizeList:
        case QoreColumnarTypeKind::Struct:
        case QoreColumnarTypeKind::Map:
        case QoreColumnarTypeKind::Dictionary:
        default:
            return QoreColumnarColumnType::Auto;
    }
}

static bool columnar_type_kind_from_name(const std::string& value, QoreColumnarTypeKind& kind) {
    std::string type = columnar_lower_type(value);
    if (type == "bool" || type == "boolean") {
        kind = QoreColumnarTypeKind::Bool;
    } else if (type == "int" || type == "integer") {
        kind = QoreColumnarTypeKind::Int;
    } else if (type == "float" || type == "double") {
        kind = QoreColumnarTypeKind::Float;
    } else if (type == "number" || type == "numeric" || type == "decimal") {
        kind = QoreColumnarTypeKind::Number;
    } else if (type == "string" || type == "utf8" || type == "large_utf8") {
        kind = QoreColumnarTypeKind::String;
    } else if (type == "date") {
        kind = QoreColumnarTypeKind::Date;
    } else if (type == "binary" || type == "large_binary" || type == "fixed_size_binary") {
        kind = QoreColumnarTypeKind::Binary;
    } else if (type == "timestamp") {
        kind = QoreColumnarTypeKind::Timestamp;
    } else if (type == "duration") {
        kind = QoreColumnarTypeKind::Duration;
    } else if (type == "decimal128") {
        kind = QoreColumnarTypeKind::Decimal128;
    } else if (type == "list") {
        kind = QoreColumnarTypeKind::List;
    } else if (type == "large_list") {
        kind = QoreColumnarTypeKind::LargeList;
    } else if (type == "fixed_size_list") {
        kind = QoreColumnarTypeKind::FixedSizeList;
    } else if (type == "struct") {
        kind = QoreColumnarTypeKind::Struct;
    } else if (type == "map") {
        kind = QoreColumnarTypeKind::Map;
    } else if (type == "dictionary") {
        kind = QoreColumnarTypeKind::Dictionary;
    } else if (type == "auto") {
        kind = QoreColumnarTypeKind::Auto;
    } else {
        return false;
    }
    return true;
}

static QoreColumnarTypeDescriptor columnar_descriptor_from_flat(QoreColumnarColumnType column_type,
        QoreBufferElementType buffer_type, bool nullable, const char* native_type) {
    QoreColumnarTypeDescriptor schema;
    schema.kind = columnar_kind_from_flat_type(column_type);
    schema.column_type = column_type;
    schema.buffer_type = buffer_type;
    schema.nullable = nullable;
    schema.native_type = native_type ? native_type : "";
    if (buffer_type == QoreBufferElementType::Decimal128) {
        schema.kind = QoreColumnarTypeKind::Decimal128;
        schema.column_type = QoreColumnarColumnType::Number;
        schema.precision = 38;
        schema.scale = 0;
    }
    return schema;
}

static QoreColumnarTypeDescriptor columnar_descriptor_from_shape(const ColumnarShape& shape,
        const char* native_type) {
    QoreColumnarTypeDescriptor schema =
        columnar_descriptor_from_flat(shape.column_type, shape.buffer_type, shape.nullable, native_type);
    if (shape.buffer_type == QoreBufferElementType::Decimal128) {
        schema.kind = QoreColumnarTypeKind::Decimal128;
        schema.column_type = QoreColumnarColumnType::Number;
        schema.precision = shape.decimal_precision > 0 ? shape.decimal_precision : 38;
        schema.scale = shape.decimal_scale;
    }
    return schema;
}

static bool columnar_parse_decimal_metadata(const std::string& native_type, int32_t& precision, int32_t& scale) {
    std::string type = columnar_lower_type(native_type);
    if (type.find("decimal") == std::string::npos && type.find("numeric") == std::string::npos
            && type.find("number") == std::string::npos) {
        return false;
    }

    size_t open = native_type.find('(');
    size_t comma = native_type.find(',', open == std::string::npos ? 0 : open + 1);
    size_t close = native_type.find(')', comma == std::string::npos ? 0 : comma + 1);
    if (open == std::string::npos || comma == std::string::npos || close == std::string::npos) {
        return false;
    }

    precision = static_cast<int32_t>(std::strtol(native_type.substr(open + 1, comma - open - 1).c_str(), nullptr, 10));
    scale = static_cast<int32_t>(std::strtol(native_type.substr(comma + 1, close - comma - 1).c_str(), nullptr, 10));
    return precision > 0 && scale >= 0;
}

static QoreColumnarTypeDescriptor columnar_descriptor_from_desc(const QoreHashNode* desc,
        const QoreColumnarTypeDescriptor& fallback, ExceptionSink* xsink);

static QoreHashNode* columnar_descriptor_to_hash(const QoreColumnarTypeDescriptor& schema, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
    if (!schema.name.empty()) {
        h->setKeyValue("name", new QoreStringNode(schema.name), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    h->setKeyValue("kind", new QoreStringNode(qore_columnar_type_kind_name(schema.kind)), xsink);
    if (*xsink) {
        return nullptr;
    }
    h->setKeyValue("type", new QoreStringNode(qore_columnar_column_type_name(schema.column_type)), xsink);
    if (*xsink) {
        return nullptr;
    }
    h->setKeyValue("nullable", schema.nullable, xsink);
    if (*xsink) {
        return nullptr;
    }
    if (schema.buffer_type != QoreBufferElementType::Invalid) {
        h->setKeyValue("buffer_type", new QoreStringNode(qore_buffer_element_type_name(schema.buffer_type)), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    if (!schema.native_type.empty()) {
        h->setKeyValue("native_type", new QoreStringNode(schema.native_type), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    if (schema.precision || schema.kind == QoreColumnarTypeKind::Decimal128) {
        h->setKeyValue("precision", static_cast<int64>(schema.precision), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    if (schema.scale || schema.kind == QoreColumnarTypeKind::Decimal128) {
        h->setKeyValue("scale", static_cast<int64>(schema.scale), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    if (schema.fixed_size) {
        h->setKeyValue("fixed_size", static_cast<int64>(schema.fixed_size), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    if (!schema.time_unit.empty()) {
        h->setKeyValue("time_unit", new QoreStringNode(schema.time_unit), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    if (!schema.timezone.empty()) {
        h->setKeyValue("timezone", new QoreStringNode(schema.timezone), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    if (!schema.dictionary_index_type.empty()) {
        h->setKeyValue("dictionary_index_type", new QoreStringNode(schema.dictionary_index_type), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    if (!schema.children.empty()) {
        ReferenceHolder<QoreListNode> children(new QoreListNode(autoTypeInfo), xsink);
        for (size_t i = 0; i < schema.children.size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "building columnar schema children")) {
                return nullptr;
            }
            children->push(columnar_descriptor_to_hash(schema.children[i], xsink), xsink);
            if (*xsink) {
                return nullptr;
            }
        }
        h->setKeyValue("children", children.release(), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    return h.release();
}

static QoreColumnarTypeDescriptor columnar_descriptor_from_desc_hash(const QoreHashNode* desc,
        const QoreColumnarTypeDescriptor& fallback, ExceptionSink* xsink) {
    QoreColumnarTypeDescriptor schema = fallback;
    if (!desc) {
        return schema;
    }

    std::string name = columnar_get_desc_string(desc, "name");
    if (!name.empty()) {
        schema.name = std::move(name);
    }

    std::string kind_name = columnar_get_desc_string(desc, "kind");
    if (kind_name.empty()) {
        kind_name = columnar_get_desc_string(desc, "logical_type");
    }
    if (!kind_name.empty()) {
        QoreColumnarTypeKind kind;
        if (columnar_type_kind_from_name(kind_name, kind)) {
            schema.kind = kind;
            schema.column_type = columnar_flat_type_from_kind(kind);
        }
    }

    QoreValue nullable = desc->getKeyValue("nullable");
    if (nullable.getType() == NT_BOOLEAN) {
        schema.nullable = nullable.getAsBool();
    }

    std::string native_type = columnar_get_desc_string(desc, "native_type");
    if (!native_type.empty()) {
        schema.native_type = native_type;
    }

    std::string buffer_type = columnar_get_desc_string(desc, "buffer_type");
    if (!buffer_type.empty()) {
        QoreBufferElementType parsed = QoreBufferElementType::Invalid;
        if (qore_buffer_element_type_from_name(buffer_type.c_str(), parsed)) {
            schema.buffer_type = parsed;
            schema.column_type = columnar_type_from_buffer(parsed);
            schema.kind = columnar_kind_from_flat_type(schema.column_type);
            if (parsed == QoreBufferElementType::Decimal128) {
                schema.kind = QoreColumnarTypeKind::Decimal128;
                schema.column_type = QoreColumnarColumnType::Number;
            }
        }
    }

    schema.precision = static_cast<int32_t>(columnar_get_desc_int(desc, "precision", schema.precision));
    schema.scale = static_cast<int32_t>(columnar_get_desc_int(desc, "scale", schema.scale));
    schema.fixed_size = static_cast<int32_t>(columnar_get_desc_int(desc, "fixed_size", schema.fixed_size));

    std::string time_unit = columnar_get_desc_string(desc, "time_unit");
    if (!time_unit.empty()) {
        schema.time_unit = std::move(time_unit);
    }
    std::string timezone = columnar_get_desc_string(desc, "timezone");
    if (!timezone.empty()) {
        schema.timezone = std::move(timezone);
    }
    std::string dictionary_index_type = columnar_get_desc_string(desc, "dictionary_index_type");
    if (!dictionary_index_type.empty()) {
        schema.dictionary_index_type = std::move(dictionary_index_type);
    }

    QoreValue children_value = desc->getKeyValue("children");
    if (children_value.getType() == NT_LIST) {
        const QoreListNode* children = children_value.get<const QoreListNode>();
        schema.children.clear();
        schema.children.reserve(children->size());
        for (size_t i = 0; i < children->size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "parsing columnar schema children")) {
                return schema;
            }
            QoreValue child_value = children->retrieveEntry(i);
            if (child_value.getType() != NT_HASH) {
                continue;
            }
            QoreColumnarTypeDescriptor child_fallback;
            schema.children.push_back(
                columnar_descriptor_from_desc_hash(child_value.get<const QoreHashNode>(), child_fallback, xsink));
            if (*xsink) {
                return schema;
            }
        }
    }

    if (schema.kind == QoreColumnarTypeKind::Number && columnar_parse_decimal_metadata(schema.native_type,
            schema.precision, schema.scale) && schema.precision <= 38) {
        schema.kind = QoreColumnarTypeKind::Decimal128;
        schema.column_type = QoreColumnarColumnType::Number;
    }
    if (schema.kind == QoreColumnarTypeKind::Decimal128) {
        schema.column_type = QoreColumnarColumnType::Number;
        schema.buffer_type = QoreBufferElementType::Decimal128;
        if (schema.precision <= 0) {
            schema.precision = 38;
        }
        if (schema.scale < 0) {
            schema.scale = 0;
        }
        if (!columnar_decimal_metadata_is_supported(schema.precision, schema.scale)) {
            xsink->raiseException("COLUMNAR-SCHEMA-ERROR",
                "decimal128 column schema requires precision 1..38 and scale 0..precision; got precision %d, "
                "scale %d", schema.precision, schema.scale);
        }
    }

    return schema;
}

static QoreColumnarTypeDescriptor columnar_descriptor_from_desc(const QoreHashNode* desc,
        const QoreColumnarTypeDescriptor& fallback, ExceptionSink* xsink) {
    if (!desc) {
        return fallback;
    }

    QoreValue schema_value = desc->getKeyValue("schema");
    if (schema_value.getType() == NT_HASH) {
        return columnar_descriptor_from_desc_hash(schema_value.get<const QoreHashNode>(), fallback, xsink);
    }

    return columnar_descriptor_from_desc_hash(desc, fallback, xsink);
}

static void columnar_shape_apply_schema(ColumnarShape& shape, const QoreColumnarTypeDescriptor& schema) {
    if (schema.kind == QoreColumnarTypeKind::Decimal128
            || schema.buffer_type == QoreBufferElementType::Decimal128) {
        columnar_shape_set_decimal128(shape, schema.precision > 0 ? schema.precision : 38,
            schema.scale >= 0 ? schema.scale : 0);
    }
}

struct QoreArrowSchemaPrivate {
    std::string format;
    std::string name;
    std::string metadata;
    std::vector<ArrowSchema*> children;
};

struct QoreArrowArrayPrivate {
    std::vector<ArrowArray*> children;
    std::vector<const void*> buffers;
    std::vector<std::vector<uint8_t>> owned_buffers;
    std::vector<QoreValue> refs;

    ~QoreArrowArrayPrivate() {
        ExceptionSink xsink;
        for (QoreValue& ref : refs) {
            ref.discard(&xsink);
        }
    }
};

struct QoreArrowImportedOwner {
    ArrowSchema schema = {};
    ArrowArray array = {};

    ~QoreArrowImportedOwner() {
        if (array.release) {
            array.release(&array);
        }
        if (schema.release) {
            schema.release(&schema);
        }
    }
};

struct QoreArrowValueVector {
    std::vector<QoreValue> values;

    ~QoreArrowValueVector() {
        ExceptionSink xsink;
        for (QoreValue& value : values) {
            value.discard(&xsink);
        }
    }
};

class QoreArrowValueSource {
public:
    virtual ~QoreArrowValueSource() = default;
    virtual size_t size() const = 0;
    virtual QoreValue get(size_t index, ExceptionSink* xsink) const = 0;
};

class QoreArrowListValueSource : public QoreArrowValueSource {
public:
    QoreArrowListValueSource(const QoreListNode* n_list) : list(n_list) {
    }

    size_t size() const override {
        return list ? list->size() : 0;
    }

    QoreValue get(size_t index, ExceptionSink*) const override {
        return list->retrieveEntry(index).refSelf();
    }

private:
    const QoreListNode* list;
};

class QoreArrowVectorValueSource : public QoreArrowValueSource {
public:
    QoreArrowVectorValueSource(const std::vector<QoreValue>& n_values) : values(n_values) {
    }

    size_t size() const override {
        return values.size();
    }

    QoreValue get(size_t index, ExceptionSink*) const override {
        return values[index].refSelf();
    }

private:
    const std::vector<QoreValue>& values;
};

static void qore_arrow_schema_release_callback(ArrowSchema* schema) {
    if (!schema || !schema->release) {
        return;
    }

    QoreArrowSchemaPrivate* priv = static_cast<QoreArrowSchemaPrivate*>(schema->private_data);
    if (priv) {
        for (ArrowSchema* child : priv->children) {
            if (child) {
                if (child->release) {
                    child->release(child);
                }
                delete child;
            }
        }
        delete priv;
    }
    if (schema->dictionary) {
        if (schema->dictionary->release) {
            schema->dictionary->release(schema->dictionary);
        }
        delete schema->dictionary;
    }
    std::memset(schema, 0, sizeof(*schema));
}

static void qore_arrow_array_release_callback(ArrowArray* array) {
    if (!array || !array->release) {
        return;
    }

    QoreArrowArrayPrivate* priv = static_cast<QoreArrowArrayPrivate*>(array->private_data);
    if (priv) {
        for (ArrowArray* child : priv->children) {
            if (child) {
                if (child->release) {
                    child->release(child);
                }
                delete child;
            }
        }
        delete priv;
    }
    if (array->dictionary) {
        if (array->dictionary->release) {
            array->dictionary->release(array->dictionary);
        }
        delete array->dictionary;
    }
    std::memset(array, 0, sizeof(*array));
}

static QoreArrowSchemaPrivate* qore_arrow_init_schema(ArrowSchema* schema, const char* format, const char* name,
        bool nullable, int64_t children) {
    std::memset(schema, 0, sizeof(*schema));
    QoreArrowSchemaPrivate* priv = new QoreArrowSchemaPrivate;
    priv->format = format ? format : "";
    priv->name = name ? name : "";
    priv->children.resize(children, nullptr);
    schema->format = priv->format.c_str();
    schema->name = priv->name.empty() ? nullptr : priv->name.c_str();
    schema->flags = nullable ? ARROW_FLAG_NULLABLE : 0;
    schema->n_children = children;
    schema->children = children ? priv->children.data() : nullptr;
    schema->release = qore_arrow_schema_release_callback;
    schema->private_data = priv;
    return priv;
}

static QoreArrowArrayPrivate* qore_arrow_init_array(ArrowArray* array, int64_t length, int64_t null_count,
        int64_t offset, int64_t buffers, int64_t children) {
    std::memset(array, 0, sizeof(*array));
    QoreArrowArrayPrivate* priv = new QoreArrowArrayPrivate;
    priv->buffers.resize(buffers, nullptr);
    priv->children.resize(children, nullptr);
    array->length = length;
    array->null_count = null_count;
    array->offset = offset;
    array->n_buffers = buffers;
    array->buffers = buffers ? priv->buffers.data() : nullptr;
    array->n_children = children;
    array->children = children ? priv->children.data() : nullptr;
    array->release = qore_arrow_array_release_callback;
    array->private_data = priv;
    return priv;
}

static size_t qore_arrow_bitmap_bytes(size_t length) {
    return (length + 7) / 8;
}

static bool qore_arrow_get_bit(const uint8_t* bitmap, size_t bit) {
    return bitmap && (bitmap[bit / 8] & (1u << (bit % 8)));
}

static void qore_arrow_set_bit(uint8_t* bitmap, size_t bit, bool value) {
    if (value) {
        bitmap[bit / 8] |= static_cast<uint8_t>(1u << (bit % 8));
    } else {
        bitmap[bit / 8] &= static_cast<uint8_t>(~(1u << (bit % 8)));
    }
}

static const void* qore_arrow_add_owned_buffer(QoreArrowArrayPrivate* priv, size_t index,
        std::vector<uint8_t>&& buffer) {
    priv->owned_buffers.push_back(std::move(buffer));
    const void* ptr = priv->owned_buffers.back().empty() ? nullptr : priv->owned_buffers.back().data();
    priv->buffers[index] = ptr;
    return ptr;
}

static std::vector<uint8_t> qore_arrow_copy_bitmap_range(const uint8_t* src, size_t bit_offset, size_t length,
        ExceptionSink* xsink, const char* context) {
    std::vector<uint8_t> dst(qore_arrow_bitmap_bytes(length), 0);
    if (!src) {
        return dst;
    }
    for (size_t i = 0; i < length; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, context)) {
            return dst;
        }
        if (qore_arrow_get_bit(src, bit_offset + i)) {
            qore_arrow_set_bit(dst.data(), i, true);
        }
    }
    return dst;
}

static bool qore_arrow_parse_decimal_format(const char* format, int32_t& precision, int32_t& scale) {
    if (!format || format[0] != 'd' || format[1] != ':') {
        return false;
    }
    char* end = nullptr;
    long p = std::strtol(format + 2, &end, 10);
    if (!end || *end != ',') {
        return false;
    }
    long s = std::strtol(end + 1, &end, 10);
    long width = 128;
    if (end && *end == ',') {
        width = std::strtol(end + 1, &end, 10);
    }
    if ((end && *end) || width != 128 || p <= 0 || p > 38 || s < 0 || s > p) {
        return false;
    }
    precision = static_cast<int32_t>(p);
    scale = static_cast<int32_t>(s);
    return true;
}

static std::string qore_arrow_decimal_format(int32_t precision, int32_t scale) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "d:%d,%d,128", precision > 0 ? precision : 38, scale >= 0 ? scale : 0);
    return buffer;
}

static bool qore_arrow_format_to_buffer_type(const char* format, QoreBufferElementType& element_type,
        int32_t& precision, int32_t& scale) {
    precision = 0;
    scale = 0;
    if (!format) {
        return false;
    }
    if (!std::strcmp(format, "c")) {
        element_type = QoreBufferElementType::Int8;
    } else if (!std::strcmp(format, "s")) {
        element_type = QoreBufferElementType::Int16;
    } else if (!std::strcmp(format, "i")) {
        element_type = QoreBufferElementType::Int32;
    } else if (!std::strcmp(format, "l")) {
        element_type = QoreBufferElementType::Int64;
    } else if (!std::strcmp(format, "f")) {
        element_type = QoreBufferElementType::Float32;
    } else if (!std::strcmp(format, "g")) {
        element_type = QoreBufferElementType::Float64;
    } else if (!std::strcmp(format, "b")) {
        element_type = QoreBufferElementType::Bool;
    } else if (qore_arrow_parse_decimal_format(format, precision, scale)) {
        element_type = QoreBufferElementType::Decimal128;
    } else {
        element_type = QoreBufferElementType::Invalid;
        return false;
    }
    return true;
}

static const char* qore_arrow_format_from_buffer_type(QoreBufferElementType element_type) {
    switch (element_type) {
        case QoreBufferElementType::Int8:
            return "c";
        case QoreBufferElementType::Int16:
            return "s";
        case QoreBufferElementType::Int32:
            return "i";
        case QoreBufferElementType::Int64:
            return "l";
        case QoreBufferElementType::Float32:
            return "f";
        case QoreBufferElementType::Float64:
            return "g";
        case QoreBufferElementType::Bool:
            return "b";
        case QoreBufferElementType::String:
            return "U";
        default:
            return nullptr;
    }
}

static QoreBufferElementType qore_arrow_buffer_type_from_schema(const QoreColumnarTypeDescriptor& schema) {
    if (schema.buffer_type != QoreBufferElementType::Invalid) {
        return schema.buffer_type;
    }
    switch (schema.kind) {
        case QoreColumnarTypeKind::Bool:
            return QoreBufferElementType::Bool;
        case QoreColumnarTypeKind::Int:
            return QoreBufferElementType::Int64;
        case QoreColumnarTypeKind::Float:
            return QoreBufferElementType::Float64;
        case QoreColumnarTypeKind::String:
            return QoreBufferElementType::String;
        case QoreColumnarTypeKind::Decimal128:
            return QoreBufferElementType::Decimal128;
        default:
            return QoreBufferElementType::Invalid;
    }
}

static int64_t qore_arrow_int64_length(size_t length, ExceptionSink* xsink, const char* context) {
    if (length > static_cast<size_t>(std::numeric_limits<int64_t>::max())) {
        xsink->raiseException("ARROW-C-DATA-ERROR", "%s length " QSD " exceeds Arrow int64 range", context, length);
        return -1;
    }
    return static_cast<int64_t>(length);
}

static int qore_arrow_export_buffer_array(const QoreBufferNode* buffer, const char* name, ArrowSchema* schema,
        ArrowArray* array, ExceptionSink* xsink, QoreValue keep_alive = QoreValue()) {
    if (buffer->ensureHostStorage(xsink)) {
        keep_alive.discard(xsink);
        return -1;
    }

    size_t length = buffer->size();
    int64_t arrow_length = qore_arrow_int64_length(length, xsink, "buffer export");
    if (*xsink) {
        keep_alive.discard(xsink);
        return -1;
    }

    int64_t null_count = buffer->hasNullableElements()
        ? static_cast<int64_t>(length - buffer->countValid(xsink))
        : 0;
    if (*xsink) {
        keep_alive.discard(xsink);
        return -1;
    }

    std::string decimal_format;
    const char* format = nullptr;
    if (buffer->getElementType() == QoreBufferElementType::Decimal128) {
        decimal_format = qore_arrow_decimal_format(buffer->getDecimalPrecision(), buffer->getDecimalScale());
        format = decimal_format.c_str();
    } else {
        format = qore_arrow_format_from_buffer_type(buffer->getElementType());
    }
    if (!format) {
        xsink->raiseException("ARROW-C-DATA-ERROR", "cannot export buffer<%s> through Arrow C Data",
            qore_buffer_element_type_name(buffer->getElementType()));
        keep_alive.discard(xsink);
        return -1;
    }

    QoreArrowSchemaPrivate* schema_priv = qore_arrow_init_schema(schema, format, name,
        buffer->hasNullableElements(), 0);
    if (!decimal_format.empty()) {
        schema_priv->format = decimal_format;
        schema->format = schema_priv->format.c_str();
    }

    int64_t n_buffers = buffer->getElementType() == QoreBufferElementType::String ? 3 : 2;
    QoreArrowArrayPrivate* array_priv = qore_arrow_init_array(array, arrow_length, null_count, 0, n_buffers, 0);
    if (!keep_alive.isNothing()) {
        array_priv->refs.push_back(keep_alive);
        keep_alive = QoreValue();
    }

    if (buffer->getElementType() == QoreBufferElementType::String) {
        std::vector<uint8_t> validity(buffer->hasNullableElements() ? qore_arrow_bitmap_bytes(length) : 0, 0);
        std::vector<int64_t> offsets64(length + 1, 0);
        std::vector<uint8_t> bytes;
        int64_t current = 0;
        for (size_t i = 0; i < length; ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "exporting Arrow string buffer")) {
                return -1;
            }
            offsets64[i] = current;
            if (buffer->isElementNull(i)) {
                continue;
            }
            if (buffer->hasNullableElements()) {
                qore_arrow_set_bit(validity.data(), i, true);
            }
            ValueHolder value(buffer->getReferencedEntry(i, xsink), xsink);
            if (*xsink) {
                return -1;
            }
            QoreStringValueHelper str(*value, QCS_UTF8, xsink);
            if (*xsink) {
                return -1;
            }
            size_t len = str->strlen();
            if (len > static_cast<size_t>(std::numeric_limits<int64_t>::max() - current)) {
                xsink->raiseException("ARROW-C-DATA-ERROR",
                    "Arrow string buffer for column '%s' exceeds int64 offset range", name ? name : "");
                return -1;
            }
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(str->c_str());
            bytes.insert(bytes.end(), ptr, ptr + len);
            current += static_cast<int64_t>(len);
        }
        offsets64[length] = current;
        if (current <= std::numeric_limits<int32_t>::max()) {
            std::vector<uint8_t> offsets((length + 1) * sizeof(int32_t), 0);
            int32_t* offset_ptr = reinterpret_cast<int32_t*>(offsets.data());
            for (size_t i = 0; i <= length; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "exporting Arrow string offsets")) {
                    return -1;
                }
                offset_ptr[i] = static_cast<int32_t>(offsets64[i]);
            }
            schema_priv->format = "u";
            schema->format = schema_priv->format.c_str();
            if (buffer->hasNullableElements()) {
                qore_arrow_add_owned_buffer(array_priv, 0, std::move(validity));
            }
            qore_arrow_add_owned_buffer(array_priv, 1, std::move(offsets));
            qore_arrow_add_owned_buffer(array_priv, 2, std::move(bytes));
            return 0;
        }

        std::vector<uint8_t> offsets((length + 1) * sizeof(int64_t), 0);
        std::memcpy(offsets.data(), offsets64.data(), offsets.size());
        if (buffer->hasNullableElements()) {
            qore_arrow_add_owned_buffer(array_priv, 0, std::move(validity));
        }
        qore_arrow_add_owned_buffer(array_priv, 1, std::move(offsets));
        qore_arrow_add_owned_buffer(array_priv, 2, std::move(bytes));
        return 0;
    }

    const uint8_t* validity = buffer->getRawValidityData();
    size_t validity_bit_offset = buffer->getRawValidityBitOffset();
    if (validity && validity_bit_offset) {
        qore_arrow_add_owned_buffer(array_priv, 0, qore_arrow_copy_bitmap_range(validity, validity_bit_offset,
            length, xsink, "exporting Arrow validity bitmap"));
        if (*xsink) {
            return -1;
        }
    } else {
        array_priv->buffers[0] = validity;
    }

    const uint8_t* data = static_cast<const uint8_t*>(buffer->getRawData());
    if (length && !data) {
        xsink->raiseException("ARROW-C-DATA-ERROR",
            "cannot export buffer<%s> through Arrow C Data without host storage",
            qore_buffer_element_type_name(buffer->getElementType()));
        return -1;
    }
    if (buffer->getElementType() == QoreBufferElementType::Bool && buffer->getRawDataBitOffset()) {
        qore_arrow_add_owned_buffer(array_priv, 1, qore_arrow_copy_bitmap_range(data, buffer->getRawDataBitOffset(),
            length, xsink, "exporting Arrow bool bitmap"));
        if (*xsink) {
            return -1;
        }
    } else {
        array_priv->buffers[1] = data;
    }
    return 0;
}

static int qore_arrow_export_values_array(const QoreArrowValueSource& source,
        const QoreColumnarTypeDescriptor& schema_desc, const char* name, ArrowSchema* schema, ArrowArray* array,
        ExceptionSink* xsink);

static int qore_arrow_export_values_as_buffer(const QoreArrowValueSource& source,
        const QoreColumnarTypeDescriptor& schema_desc, const char* name, ArrowSchema* schema, ArrowArray* array,
        ExceptionSink* xsink) {
    QoreBufferElementType element_type = qore_arrow_buffer_type_from_schema(schema_desc);
    if (element_type == QoreBufferElementType::Invalid) {
        xsink->raiseException("ARROW-C-DATA-ERROR",
            "column '%s' schema kind '%s' is not supported by Arrow C Data export without nested metadata",
            name ? name : "", qore_columnar_type_kind_name(schema_desc.kind));
        return -1;
    }

    ReferenceHolder<QoreListNode> values(new QoreListNode(autoTypeInfo), xsink);
    for (size_t i = 0; i < source.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "collecting Arrow export values")) {
            return -1;
        }
        values->push(source.get(i, xsink), xsink);
        if (*xsink) {
            return -1;
        }
    }

    ValueHolder buffer_value(xsink);
    if (element_type == QoreBufferElementType::Decimal128) {
        buffer_value = new QoreBufferNode(element_type, schema_desc.nullable, *values, xsink,
            schema_desc.precision > 0 ? schema_desc.precision : 38, schema_desc.scale);
    } else {
        buffer_value = new QoreBufferNode(element_type, schema_desc.nullable, *values, xsink);
    }
    if (*xsink) {
        return -1;
    }
    const QoreBufferNode* buffer = buffer_value->get<const QoreBufferNode>();
    return qore_arrow_export_buffer_array(buffer, name, schema, array, xsink, buffer_value.release());
}

static int qore_arrow_export_list_array(const QoreArrowValueSource& source,
        const QoreColumnarTypeDescriptor& schema_desc, const char* name, ArrowSchema* schema, ArrowArray* array,
        ExceptionSink* xsink) {
    if (schema_desc.children.empty()) {
        xsink->raiseException("ARROW-C-DATA-ERROR",
            "cannot export list column '%s' through Arrow C Data without child schema metadata", name ? name : "");
        return -1;
    }

    size_t length = source.size();
    int64_t arrow_length = qore_arrow_int64_length(length, xsink, "list export");
    if (*xsink) {
        return -1;
    }

    bool large = schema_desc.kind == QoreColumnarTypeKind::LargeList;
    std::vector<uint8_t> validity(schema_desc.nullable ? qore_arrow_bitmap_bytes(length) : 0, 0);
    std::vector<uint8_t> offsets((length + 1) * (large ? sizeof(int64_t) : sizeof(int32_t)), 0);
    QoreArrowValueVector child_values;
    int64_t current = 0;
    int64_t null_count = 0;

    for (size_t i = 0; i < length; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "exporting Arrow list offsets")) {
            return -1;
        }
        if (large) {
            reinterpret_cast<int64_t*>(offsets.data())[i] = current;
        } else {
            if (current > std::numeric_limits<int32_t>::max()) {
                xsink->raiseException("ARROW-C-DATA-ERROR",
                    "list column '%s' exceeds Arrow 32-bit list offset range; use large_list schema metadata",
                    name ? name : "");
                return -1;
            }
            reinterpret_cast<int32_t*>(offsets.data())[i] = static_cast<int32_t>(current);
        }

        ValueHolder row(source.get(i, xsink), xsink);
        if (*xsink) {
            return -1;
        }
        if (row->isNullOrNothing()) {
            ++null_count;
            continue;
        }
        if (row->getType() != NT_LIST) {
            xsink->raiseException("ARROW-C-DATA-ERROR",
                "list column '%s' row " QSD " has type '%s'; expected list or NOTHING",
                name ? name : "", i, row->getTypeName());
            return -1;
        }
        if (schema_desc.nullable) {
            qore_arrow_set_bit(validity.data(), i, true);
        }
        const QoreListNode* row_list = row->get<const QoreListNode>();
        if (row_list->size() > static_cast<size_t>(std::numeric_limits<int64_t>::max() - current)) {
            xsink->raiseException("ARROW-C-DATA-ERROR",
                "list column '%s' child length exceeds Arrow int64 range", name ? name : "");
            return -1;
        }
        for (size_t j = 0; j < row_list->size(); ++j) {
            if (j && !(j % 100) && qore_check_cancel(xsink, "collecting Arrow list child values")) {
                return -1;
            }
            child_values.values.push_back(row_list->retrieveEntry(j).refSelf());
        }
        current += static_cast<int64_t>(row_list->size());
    }

    if (large) {
        reinterpret_cast<int64_t*>(offsets.data())[length] = current;
    } else {
        if (current > std::numeric_limits<int32_t>::max()) {
            xsink->raiseException("ARROW-C-DATA-ERROR",
                "list column '%s' exceeds Arrow 32-bit list offset range; use large_list schema metadata",
                name ? name : "");
            return -1;
        }
        reinterpret_cast<int32_t*>(offsets.data())[length] = static_cast<int32_t>(current);
    }

    qore_arrow_init_schema(schema, large ? "+L" : "+l", name, schema_desc.nullable, 1);
    QoreArrowArrayPrivate* array_priv = qore_arrow_init_array(array, arrow_length, null_count, 0, 2, 1);
    if (schema_desc.nullable) {
        qore_arrow_add_owned_buffer(array_priv, 0, std::move(validity));
    }
    qore_arrow_add_owned_buffer(array_priv, 1, std::move(offsets));

    schema->children[0] = new ArrowSchema{};
    array->children[0] = new ArrowArray{};
    QoreArrowVectorValueSource child_source(child_values.values);
    const std::string& child_name = schema_desc.children[0].name;
    return qore_arrow_export_values_array(child_source, schema_desc.children[0], child_name.c_str(),
        schema->children[0], array->children[0], xsink);
}

static int qore_arrow_export_struct_array(const QoreArrowValueSource& source,
        const QoreColumnarTypeDescriptor& schema_desc, const char* name, ArrowSchema* schema, ArrowArray* array,
        ExceptionSink* xsink) {
    size_t length = source.size();
    int64_t arrow_length = qore_arrow_int64_length(length, xsink, "struct export");
    if (*xsink) {
        return -1;
    }

    qore_arrow_init_schema(schema, "+s", name, schema_desc.nullable, schema_desc.children.size());
    QoreArrowArrayPrivate* array_priv = qore_arrow_init_array(array, arrow_length, 0, 0, 1,
        schema_desc.children.size());

    std::vector<uint8_t> validity(schema_desc.nullable ? qore_arrow_bitmap_bytes(length) : 0, 0);
    std::vector<QoreArrowValueVector> child_values(schema_desc.children.size());
    int64_t null_count = 0;

    for (size_t i = 0; i < length; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "exporting Arrow struct values")) {
            return -1;
        }
        ValueHolder row(source.get(i, xsink), xsink);
        if (*xsink) {
            return -1;
        }
        if (row->isNullOrNothing()) {
            ++null_count;
            for (QoreArrowValueVector& child : child_values) {
                child.values.push_back(QoreValue());
            }
            continue;
        }
        if (row->getType() != NT_HASH) {
            xsink->raiseException("ARROW-C-DATA-ERROR",
                "struct column '%s' row " QSD " has type '%s'; expected hash or NOTHING",
                name ? name : "", i, row->getTypeName());
            return -1;
        }
        if (schema_desc.nullable) {
            qore_arrow_set_bit(validity.data(), i, true);
        }
        const QoreHashNode* row_hash = row->get<const QoreHashNode>();
        for (size_t c = 0; c < schema_desc.children.size(); ++c) {
            if (c && !(c % 100) && qore_check_cancel(xsink, "collecting Arrow struct child values")) {
                return -1;
            }
            const std::string& child_name = schema_desc.children[c].name;
            child_values[c].values.push_back(row_hash->getKeyValue(child_name.c_str()).refSelf());
        }
    }

    array->null_count = null_count;
    if (schema_desc.nullable) {
        qore_arrow_add_owned_buffer(array_priv, 0, std::move(validity));
    }

    for (size_t c = 0; c < schema_desc.children.size(); ++c) {
        schema->children[c] = new ArrowSchema{};
        array->children[c] = new ArrowArray{};
        QoreArrowVectorValueSource child_source(child_values[c].values);
        const std::string& child_name = schema_desc.children[c].name;
        if (qore_arrow_export_values_array(child_source, schema_desc.children[c], child_name.c_str(),
                schema->children[c], array->children[c], xsink)) {
            return -1;
        }
    }
    return 0;
}

static int qore_arrow_export_values_array(const QoreArrowValueSource& source,
        const QoreColumnarTypeDescriptor& schema_desc, const char* name, ArrowSchema* schema, ArrowArray* array,
        ExceptionSink* xsink) {
    switch (schema_desc.kind) {
        case QoreColumnarTypeKind::List:
        case QoreColumnarTypeKind::LargeList:
            return qore_arrow_export_list_array(source, schema_desc, name, schema, array, xsink);
        case QoreColumnarTypeKind::Struct:
            return qore_arrow_export_struct_array(source, schema_desc, name, schema, array, xsink);
        case QoreColumnarTypeKind::Bool:
        case QoreColumnarTypeKind::Int:
        case QoreColumnarTypeKind::Float:
        case QoreColumnarTypeKind::String:
        case QoreColumnarTypeKind::Decimal128:
            return qore_arrow_export_values_as_buffer(source, schema_desc, name, schema, array, xsink);
        default:
            if (schema_desc.buffer_type != QoreBufferElementType::Invalid) {
                return qore_arrow_export_values_as_buffer(source, schema_desc, name, schema, array, xsink);
            }
            xsink->raiseException("ARROW-C-DATA-ERROR",
                "column '%s' schema kind '%s' is not supported by Arrow C Data export",
                name ? name : "", qore_columnar_type_kind_name(schema_desc.kind));
            return -1;
    }
}

static bool qore_arrow_array_is_null(const ArrowArray* array, int64_t index) {
    const uint8_t* validity = array && array->n_buffers > 0
        ? static_cast<const uint8_t*>(array->buffers[0])
        : nullptr;
    return validity && !qore_arrow_get_bit(validity, static_cast<size_t>(array->offset + index));
}

static std::string qore_arrow_decimal_to_string(const uint8_t* data, int64_t index, int32_t scale) {
    uint64_t low = 0;
    int64_t high = 0;
    std::memcpy(&low, data + (index * 16), sizeof(low));
    std::memcpy(&high, data + (index * 16) + sizeof(low), sizeof(high));
    unsigned __int128 raw = (static_cast<unsigned __int128>(static_cast<uint64_t>(high)) << 64) | low;
    bool negative = high < 0;
    unsigned __int128 magnitude = negative ? (~raw + 1) : raw;

    std::string digits;
    do {
        digits.push_back(static_cast<char>('0' + (magnitude % 10)));
        magnitude /= 10;
    } while (magnitude);
    std::reverse(digits.begin(), digits.end());

    if (scale > 0) {
        if (digits.size() <= static_cast<size_t>(scale)) {
            digits.insert(digits.begin(), static_cast<size_t>(scale) - digits.size() + 1, '0');
        }
        digits.insert(digits.end() - scale, '.');
    }
    if (negative) {
        digits.insert(digits.begin(), '-');
    }
    return digits;
}

static QoreColumnarTypeDescriptor qore_arrow_schema_to_descriptor(const ArrowSchema* schema, ExceptionSink* xsink) {
    QoreColumnarTypeDescriptor desc;
    desc.name = schema && schema->name ? schema->name : "";
    desc.nullable = schema && (schema->flags & ARROW_FLAG_NULLABLE);
    if (!schema || !schema->format) {
        return desc;
    }

    int32_t precision = 0;
    int32_t scale = 0;
    QoreBufferElementType element_type = QoreBufferElementType::Invalid;
    if (qore_arrow_format_to_buffer_type(schema->format, element_type, precision, scale)) {
        desc.buffer_type = element_type;
        desc.column_type = columnar_type_from_buffer(element_type);
        desc.kind = element_type == QoreBufferElementType::Decimal128
            ? QoreColumnarTypeKind::Decimal128
            : columnar_kind_from_flat_type(desc.column_type);
        desc.precision = precision;
        desc.scale = scale;
        return desc;
    }

    if (!std::strcmp(schema->format, "u") || !std::strcmp(schema->format, "U")) {
        desc.kind = QoreColumnarTypeKind::String;
        desc.column_type = QoreColumnarColumnType::String;
        desc.buffer_type = QoreBufferElementType::String;
    } else if (!std::strcmp(schema->format, "+l")) {
        desc.kind = QoreColumnarTypeKind::List;
        desc.column_type = QoreColumnarColumnType::Auto;
    } else if (!std::strcmp(schema->format, "+L")) {
        desc.kind = QoreColumnarTypeKind::LargeList;
        desc.column_type = QoreColumnarColumnType::Auto;
    } else if (!std::strcmp(schema->format, "+s")) {
        desc.kind = QoreColumnarTypeKind::Struct;
        desc.column_type = QoreColumnarColumnType::Auto;
    } else {
        desc.kind = QoreColumnarTypeKind::Auto;
        desc.column_type = QoreColumnarColumnType::Auto;
    }

    for (int64_t i = 0; i < schema->n_children; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "importing Arrow schema children")) {
            return desc;
        }
        desc.children.push_back(qore_arrow_schema_to_descriptor(schema->children[i], xsink));
        if (*xsink) {
            return desc;
        }
    }
    return desc;
}

static QoreValue qore_arrow_value_at(const ArrowSchema* schema, const ArrowArray* array, int64_t index,
        ExceptionSink* xsink);

static QoreListNode* qore_arrow_list_to_qore(const ArrowSchema* schema, const ArrowArray* array,
        ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (int64_t i = 0; i < array->length; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "importing Arrow values")) {
            return nullptr;
        }
        rv->push(qore_arrow_value_at(schema, array, i, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    return rv.release();
}

static QoreValue qore_arrow_value_at(const ArrowSchema* schema, const ArrowArray* array, int64_t index,
        ExceptionSink* xsink) {
    if (qore_arrow_array_is_null(array, index)) {
        return QoreValue();
    }

    int64_t physical = array->offset + index;
    int32_t precision = 0;
    int32_t scale = 0;
    QoreBufferElementType element_type = QoreBufferElementType::Invalid;
    if (qore_arrow_format_to_buffer_type(schema->format, element_type, precision, scale)) {
        if (array->n_buffers < 2 || (!array->buffers[1] && array->length)) {
            xsink->raiseException("ARROW-C-DATA-ERROR",
                "Arrow array '%s' has invalid fixed-width buffers", schema->name ? schema->name : "");
            return QoreValue();
        }
        const uint8_t* data = static_cast<const uint8_t*>(array->buffers[1]);
        switch (element_type) {
            case QoreBufferElementType::Int8:
                return static_cast<int64_t>(reinterpret_cast<const int8_t*>(data)[physical]);
            case QoreBufferElementType::Int16:
                return static_cast<int64_t>(reinterpret_cast<const int16_t*>(data)[physical]);
            case QoreBufferElementType::Int32:
                return static_cast<int64_t>(reinterpret_cast<const int32_t*>(data)[physical]);
            case QoreBufferElementType::Int64:
                return reinterpret_cast<const int64_t*>(data)[physical];
            case QoreBufferElementType::Float32:
                return static_cast<double>(reinterpret_cast<const float*>(data)[physical]);
            case QoreBufferElementType::Float64:
                return reinterpret_cast<const double*>(data)[physical];
            case QoreBufferElementType::Bool:
                return qore_arrow_get_bit(data, static_cast<size_t>(physical));
            case QoreBufferElementType::Decimal128: {
                std::string value = qore_arrow_decimal_to_string(data, physical, scale);
                return new QoreNumberNode(value.c_str());
            }
            default:
                break;
        }
    }

    if (!std::strcmp(schema->format, "u") || !std::strcmp(schema->format, "U")) {
        if (array->n_buffers < 3 || !array->buffers[1] || (!array->buffers[2] && array->length)) {
            xsink->raiseException("ARROW-C-DATA-ERROR",
                "Arrow string array '%s' has invalid buffers", schema->name ? schema->name : "");
            return QoreValue();
        }
        int64_t begin;
        int64_t end;
        if (!std::strcmp(schema->format, "u")) {
            const int32_t* offsets = static_cast<const int32_t*>(array->buffers[1]);
            begin = offsets[physical];
            end = offsets[physical + 1];
        } else {
            const int64_t* offsets = static_cast<const int64_t*>(array->buffers[1]);
            begin = offsets[physical];
            end = offsets[physical + 1];
        }
        if (begin < 0 || end < begin) {
            xsink->raiseException("ARROW-C-DATA-ERROR",
                "Arrow string array '%s' has invalid offsets", schema->name ? schema->name : "");
            return QoreValue();
        }
        const char* bytes = static_cast<const char*>(array->buffers[2]);
        return QoreValue::makeStringValue(bytes + begin, static_cast<size_t>(end - begin), QCS_UTF8);
    }

    if (!std::strcmp(schema->format, "+l") || !std::strcmp(schema->format, "+L")) {
        if (schema->n_children != 1 || array->n_children != 1 || array->n_buffers < 2 || !array->buffers[1]) {
            xsink->raiseException("ARROW-C-DATA-ERROR",
                "Arrow list array '%s' has invalid children or buffers", schema->name ? schema->name : "");
            return QoreValue();
        }
        int64_t begin;
        int64_t end;
        if (!std::strcmp(schema->format, "+l")) {
            const int32_t* offsets = static_cast<const int32_t*>(array->buffers[1]);
            begin = offsets[physical];
            end = offsets[physical + 1];
        } else {
            const int64_t* offsets = static_cast<const int64_t*>(array->buffers[1]);
            begin = offsets[physical];
            end = offsets[physical + 1];
        }
        if (begin < 0 || end < begin) {
            xsink->raiseException("ARROW-C-DATA-ERROR",
                "Arrow list array '%s' has invalid offsets", schema->name ? schema->name : "");
            return QoreValue();
        }
        ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);
        for (int64_t i = begin; i < end; ++i) {
            if (i != begin && !((i - begin) % 100) && qore_check_cancel(xsink, "importing Arrow list value")) {
                return QoreValue();
            }
            list->push(qore_arrow_value_at(schema->children[0], array->children[0], i, xsink), xsink);
            if (*xsink) {
                return QoreValue();
            }
        }
        return list.release();
    }

    if (!std::strcmp(schema->format, "+s")) {
        if (schema->n_children != array->n_children) {
            xsink->raiseException("ARROW-C-DATA-ERROR",
                "Arrow struct array '%s' has " QLLD " schema children and " QLLD " array children",
                schema->name ? schema->name : "", schema->n_children, array->n_children);
            return QoreValue();
        }
        ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), xsink);
        for (int64_t i = 0; i < schema->n_children; ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "importing Arrow struct value")) {
                return QoreValue();
            }
            hash->setKeyValue(schema->children[i]->name ? schema->children[i]->name : "",
                qore_arrow_value_at(schema->children[i], array->children[i], physical, xsink), xsink);
            if (*xsink) {
                return QoreValue();
            }
        }
        return hash.release();
    }

    xsink->raiseException("ARROW-C-DATA-ERROR", "unsupported Arrow C Data format '%s' for column '%s'",
        schema->format ? schema->format : "<null>", schema->name ? schema->name : "");
    return QoreValue();
}

static QoreValue qore_arrow_import_top_level_column(const ArrowSchema* schema, const ArrowArray* array,
        std::shared_ptr<const void> owner, const QoreColumnarTypeDescriptor& desc, ExceptionSink* xsink) {
    int32_t precision = 0;
    int32_t scale = 0;
    QoreBufferElementType element_type = QoreBufferElementType::Invalid;
    if (qore_arrow_format_to_buffer_type(schema->format, element_type, precision, scale)) {
        if (array->n_buffers < 2 || (!array->buffers[1] && array->length)) {
            xsink->raiseException("ARROW-C-DATA-ERROR",
                "Arrow fixed-width array '%s' has invalid buffers", schema->name ? schema->name : "");
            return QoreValue();
        }
        bool nullable = desc.nullable || array->null_count > 0 || (array->n_buffers > 0 && array->buffers[0]);
        if (element_type == QoreBufferElementType::Decimal128) {
            return QoreBufferNode::wrapExternalStorage(element_type, nullable, static_cast<size_t>(array->offset),
                static_cast<size_t>(array->length), array->buffers[1],
                array->n_buffers > 0 ? static_cast<const uint8_t*>(array->buffers[0]) : nullptr,
                owner, array->null_count, precision, scale, xsink);
        }
        return QoreBufferNode::wrapExternalStorage(element_type, nullable, static_cast<size_t>(array->offset),
            static_cast<size_t>(array->length), array->buffers[1],
            array->n_buffers > 0 ? static_cast<const uint8_t*>(array->buffers[0]) : nullptr,
            owner, array->null_count, xsink);
    }

    if (!std::strcmp(schema->format, "u") || !std::strcmp(schema->format, "U")) {
        ReferenceHolder<QoreListNode> values(qore_arrow_list_to_qore(schema, array, xsink), xsink);
        if (*xsink) {
            return QoreValue();
        }
        return new QoreBufferNode(QoreBufferElementType::String, desc.nullable || array->null_count > 0,
            *values, xsink);
    }

    ReferenceHolder<QoreListNode> values(qore_arrow_list_to_qore(schema, array, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    return values.release();
}

}

const char* qore_columnar_column_type_name(QoreColumnarColumnType type) {
    switch (type) {
        case QoreColumnarColumnType::Bool:
            return "bool";
        case QoreColumnarColumnType::Int:
            return "int";
        case QoreColumnarColumnType::Float:
            return "float";
        case QoreColumnarColumnType::Number:
            return "number";
        case QoreColumnarColumnType::String:
            return "string";
        case QoreColumnarColumnType::Date:
            return "date";
        case QoreColumnarColumnType::Binary:
            return "binary";
        case QoreColumnarColumnType::Auto:
        default:
            return "auto";
    }
}

const char* qore_columnar_type_kind_name(QoreColumnarTypeKind kind) {
    switch (kind) {
        case QoreColumnarTypeKind::Bool:
            return "bool";
        case QoreColumnarTypeKind::Int:
            return "int";
        case QoreColumnarTypeKind::Float:
            return "float";
        case QoreColumnarTypeKind::Number:
            return "number";
        case QoreColumnarTypeKind::String:
            return "string";
        case QoreColumnarTypeKind::Date:
            return "date";
        case QoreColumnarTypeKind::Binary:
            return "binary";
        case QoreColumnarTypeKind::Timestamp:
            return "timestamp";
        case QoreColumnarTypeKind::Duration:
            return "duration";
        case QoreColumnarTypeKind::Decimal128:
            return "decimal128";
        case QoreColumnarTypeKind::List:
            return "list";
        case QoreColumnarTypeKind::LargeList:
            return "large_list";
        case QoreColumnarTypeKind::FixedSizeList:
            return "fixed_size_list";
        case QoreColumnarTypeKind::Struct:
            return "struct";
        case QoreColumnarTypeKind::Map:
            return "map";
        case QoreColumnarTypeKind::Dictionary:
            return "dictionary";
        case QoreColumnarTypeKind::Auto:
        default:
            return "auto";
    }
}

QoreColumnarResult::Column::Column(std::string n_name, QoreColumnarColumnType n_column_type,
        QoreBufferElementType n_buffer_type, bool n_nullable, std::string n_native_type, QoreValue n_data)
        : name(std::move(n_name)), column_type(n_column_type), buffer_type(n_buffer_type), nullable(n_nullable),
        native_type(std::move(n_native_type)), data(n_data) {
    schema = columnar_descriptor_from_flat(column_type, buffer_type, nullable, native_type.c_str());
    schema.name = name;
}

QoreColumnarResult::Column::Column(std::string n_name, const QoreColumnarTypeDescriptor& n_schema,
        QoreValue n_data) : name(std::move(n_name)), column_type(n_schema.column_type),
        buffer_type(n_schema.buffer_type), nullable(n_schema.nullable), native_type(n_schema.native_type),
        schema(n_schema), data(n_data) {
    schema.name = name;
}

QoreColumnarResult::Column::Column(Column&& old) noexcept
        : name(std::move(old.name)), column_type(old.column_type), buffer_type(old.buffer_type),
        nullable(old.nullable), native_type(std::move(old.native_type)), schema(std::move(old.schema)),
        data(old.data) {
    old.data = QoreValue();
}

QoreColumnarResult::Column& QoreColumnarResult::Column::operator=(Column&& old) noexcept {
    if (this != &old) {
        ExceptionSink xsink;
        data.discard(&xsink);
        name = std::move(old.name);
        column_type = old.column_type;
        buffer_type = old.buffer_type;
        nullable = old.nullable;
        native_type = std::move(old.native_type);
        schema = std::move(old.schema);
        data = old.data;
        old.data = QoreValue();
    }
    return *this;
}

QoreColumnarResult::Column::~Column() {
    ExceptionSink xsink;
    data.discard(&xsink);
}

QoreColumnarResult::QoreColumnarResult() {
}

QoreColumnarResult::~QoreColumnarResult() {
}

int QoreColumnarResult::setRowCount(size_t n_rows, ExceptionSink* xsink) {
    if (!columns.empty() && row_count != n_rows) {
        xsink->raiseException("COLUMNAR-RESULT-ERROR",
            "column has " QLLD " rows, expected " QLLD, (int64)n_rows, (int64)row_count);
        return -1;
    }
    row_count = n_rows;
    return 0;
}

int QoreColumnarResult::addColumn(const char* name, QoreValue data, QoreColumnarColumnType column_type,
        QoreBufferElementType buffer_type, bool nullable, const char* native_type, ExceptionSink* xsink) {
    QoreColumnarTypeDescriptor schema = columnar_descriptor_from_flat(column_type, buffer_type, nullable,
        native_type);
    return addColumn(name, data, schema, xsink);
}

int QoreColumnarResult::addColumn(const char* name, QoreValue data, const QoreColumnarTypeDescriptor& schema,
        ExceptionSink* xsink) {
    ValueHolder data_holder(data, xsink);
    size_t size = columnar_value_size(data);
    if (data.getType() != NT_LIST && data.getType() != NT_BUFFER) {
        xsink->raiseException("COLUMNAR-RESULT-ERROR",
            "column '%s' has type '%s'; expected list or buffer", name, data.getTypeName());
        return -1;
    }
    if (setRowCount(size, xsink)) {
        return -1;
    }

    columns.emplace_back(name, schema, data_holder.release());
    return 0;
}

QoreColumnarResult* QoreColumnarResult::fromColumnHash(const QoreHashNode* columns, const QoreHashNode* desc,
        ExceptionSink* xsink) {
    assert(xsink);
    if (!columns) {
        xsink->raiseException("COLUMNAR-RESULT-ERROR", "columnar result creation requires a column hash");
        return nullptr;
    }

    ReferenceHolder<QoreColumnarResult> rv(new QoreColumnarResult, xsink);
    ConstHashIterator i(columns);
    while (i.next()) {
        if (rv->numColumns() && !(rv->numColumns() % 100)
                && qore_check_cancel(xsink, "columnar DBI result conversion")) {
            return nullptr;
        }

        QoreValue value = i.get();
        const QoreHashNode* column_desc = columnar_get_desc_column(desc, i.getKey());
        std::string native_type = columnar_get_desc_string(column_desc, "native_type");

        if (value.getType() == NT_BUFFER) {
            const QoreBufferNode* buffer = value.get<const QoreBufferNode>();
            QoreColumnarTypeDescriptor schema = columnar_descriptor_from_flat(
                columnar_type_from_buffer(buffer->getElementType()), buffer->getElementType(),
                buffer->hasNullableElements(), native_type.c_str());
            if (buffer->getElementType() == QoreBufferElementType::Decimal128) {
                schema.kind = QoreColumnarTypeKind::Decimal128;
                schema.column_type = QoreColumnarColumnType::Number;
                schema.precision = buffer->getDecimalPrecision();
                schema.scale = buffer->getDecimalScale();
            }
            schema = columnar_descriptor_from_desc(column_desc, schema, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (buffer->getElementType() == QoreBufferElementType::Decimal128) {
                schema.kind = QoreColumnarTypeKind::Decimal128;
                schema.column_type = QoreColumnarColumnType::Number;
                schema.buffer_type = QoreBufferElementType::Decimal128;
                schema.precision = buffer->getDecimalPrecision();
                schema.scale = buffer->getDecimalScale();
            }
            if (rv->addColumn(i.getKey(), value.refSelf(), schema, xsink)) {
                return nullptr;
            }
            continue;
        }

        if (value.getType() != NT_LIST) {
            xsink->raiseException("COLUMNAR-RESULT-ERROR",
                "column '%s' has type '%s'; expected list or buffer", i.getKey(), value.getTypeName());
            return nullptr;
        }

        const QoreListNode* source = value.get<const QoreListNode>();
        ColumnarShape shape;
        if (columnar_shape_from_desc(column_desc, shape)) {
            shape.nullable = shape.nullable || columnar_list_has_nulls(source, xsink);
        } else {
            shape = columnar_infer_list_shape(source, xsink);
        }
        if (*xsink) {
            return nullptr;
        }

        QoreColumnarTypeDescriptor schema = columnar_descriptor_from_shape(shape, native_type.c_str());
        schema = columnar_descriptor_from_desc(column_desc, schema, xsink);
        if (*xsink) {
            return nullptr;
        }
        columnar_shape_apply_schema(shape, schema);

        ValueHolder column_value(columnar_make_column_value(source, shape, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        if (rv->addColumn(i.getKey(), column_value.release(), schema, xsink)) {
            return nullptr;
        }
    }

    return rv.release();
}

QoreColumnarResult* QoreColumnarResult::fromRows(const QoreListNode* rows, const QoreHashNode* desc,
        ExceptionSink* xsink) {
    assert(xsink);
    if (!rows) {
        xsink->raiseException("COLUMNAR-RESULT-ERROR", "columnar row result creation requires a row list");
        return nullptr;
    }

    std::vector<std::string> names;
    if (desc && !desc->empty()) {
        ConstHashIterator hi(desc);
        while (hi.next()) {
            if (names.size() && !(names.size() % 100)
                    && qore_check_cancel(xsink, "columnar DBI row column collection")) {
                return nullptr;
            }
            names.push_back(hi.getKey());
        }
    } else if (!rows->empty()) {
        QoreValue first = rows->retrieveEntry(0);
        if (first.getType() != NT_HASH) {
            xsink->raiseException("COLUMNAR-RESULT-ERROR",
                "row result entry has type '%s'; expected hash", first.getTypeName());
            return nullptr;
        }
        ConstHashIterator hi(first.get<const QoreHashNode>());
        while (hi.next()) {
            if (names.size() && !(names.size() % 100)
                    && qore_check_cancel(xsink, "columnar DBI row column collection")) {
                return nullptr;
            }
            names.push_back(hi.getKey());
        }
    }

    ReferenceHolder<QoreHashNode> columns(new QoreHashNode(autoTypeInfo), xsink);
    std::vector<QoreListNode*> lists;
    lists.reserve(names.size());
    for (const std::string& name : names) {
        ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);
        lists.push_back(*list);
        columns->setKeyValue(name.c_str(), list.release(), xsink);
        if (*xsink) {
            return nullptr;
        }
    }

    ConstListIterator li(rows);
    while (li.next()) {
        if (li.index() && !(li.index() % 100) && qore_check_cancel(xsink, "columnar DBI row conversion")) {
            return nullptr;
        }

        QoreValue row_value = li.getValue();
        if (row_value.getType() != NT_HASH) {
            xsink->raiseException("COLUMNAR-RESULT-ERROR",
                "row result entry " QLLD " has type '%s'; expected hash", (int64)li.index(),
                row_value.getTypeName());
            return nullptr;
        }

        const QoreHashNode* row = row_value.get<const QoreHashNode>();
        for (size_t n = 0; n < names.size(); ++n) {
            lists[n]->push(row->getKeyValue(names[n].c_str()).refSelf(), xsink);
            if (*xsink) {
                return nullptr;
            }
        }
    }

    return fromColumnHash(*columns, desc, xsink);
}

const QoreColumnarResult::Column* QoreColumnarResult::getColumn(size_t index) const {
    return index < columns.size() ? &columns[index] : nullptr;
}

const QoreColumnarResult::Column* QoreColumnarResult::findColumn(const char* name) const {
    for (const Column& column : columns) {
        if (!std::strcmp(column.name.c_str(), name)) {
            return &column;
        }
    }
    return nullptr;
}

QoreListNode* QoreColumnarResult::getColumnNames(ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(stringTypeInfo), xsink);
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "columnar result column name list")) {
            return nullptr;
        }
        rv->push(new QoreStringNode(columns[i].name), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    return rv.release();
}

QoreListNode* QoreColumnarResult::getSchema(ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "columnar result schema list")) {
            return nullptr;
        }
        const Column& column = columns[i];
        ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
        h->setKeyValue("name", new QoreStringNode(column.name), xsink);
        if (*xsink) {
            return nullptr;
        }
        h->setKeyValue("type", new QoreStringNode(qore_columnar_column_type_name(column.column_type)), xsink);
        if (*xsink) {
            return nullptr;
        }
        h->setKeyValue("nullable", column.nullable, xsink);
        if (*xsink) {
            return nullptr;
        }
        if (column.buffer_type != QoreBufferElementType::Invalid) {
            h->setKeyValue("buffer_type", new QoreStringNode(qore_buffer_element_type_name(column.buffer_type)), xsink);
            if (*xsink) {
                return nullptr;
            }
        }
        if (!column.native_type.empty()) {
            h->setKeyValue("native_type", new QoreStringNode(column.native_type), xsink);
            if (*xsink) {
                return nullptr;
            }
        }
        h->setKeyValue("schema", columnar_descriptor_to_hash(column.schema, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        rv->push(h.release(), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    return rv.release();
}

QoreListNode* QoreColumnarResult::getSchemaV2(ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "columnar result v2 schema list")) {
            return nullptr;
        }
        rv->push(columnar_descriptor_to_hash(columns[i].schema, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    return rv.release();
}

QoreHashNode* QoreColumnarResult::toColumnHash(ExceptionSink* xsink) const {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "columnar result column hash")) {
            return nullptr;
        }
        rv->setKeyValue(columns[i].name.c_str(), columns[i].data.refSelf(), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    return rv.release();
}

QoreListNode* QoreColumnarResult::toRows(ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (size_t r = 0; r < row_count; ++r) {
        if (r && !(r % 100) && qore_check_cancel(xsink, "columnar result row materialization")) {
            return nullptr;
        }
        ReferenceHolder<QoreHashNode> row(new QoreHashNode(autoTypeInfo), xsink);
        for (const Column& column : columns) {
            row->setKeyValue(column.name.c_str(), columnar_value_at(column.data, r, xsink), xsink);
            if (*xsink) {
                return nullptr;
            }
        }
        rv->push(row.release(), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    return rv.release();
}

QoreColumnarResult* QoreColumnarResult::filter(QoreValue mask, ExceptionSink* xsink) const {
    assert(xsink);
    size_t mask_size = columnar_mask_size(mask, xsink);
    if (*xsink) {
        return nullptr;
    }
    if (mask_size != row_count) {
        xsink->raiseException("COLUMNAR-RESULT-ERROR",
            "row mask length " QLLD " does not match columnar result row count " QLLD,
            static_cast<int64>(mask_size), static_cast<int64>(row_count));
        return nullptr;
    }

    size_t selected = columnar_mask_count(mask, xsink);
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreColumnarResult> rv(new QoreColumnarResult, xsink);
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "filtering columnar result")) {
            return nullptr;
        }
        const Column& column = columns[i];
        ValueHolder data(columnar_filter_value(column.data, mask, selected, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        if (rv->addColumn(column.name.c_str(), data.release(), column.schema, xsink)) {
            return nullptr;
        }
    }
    return rv.release();
}

QoreColumnarResult* QoreColumnarResult::slice(size_t offset, size_t count, ExceptionSink* xsink) const {
    assert(xsink);
    if (offset > row_count || count > row_count - offset) {
        xsink->raiseException("COLUMNAR-RESULT-ERROR",
            "slice offset " QLLD " count " QLLD " out of range for " QLLD " rows",
            static_cast<int64>(offset), static_cast<int64>(count), static_cast<int64>(row_count));
        return nullptr;
    }

    ReferenceHolder<QoreColumnarResult> rv(new QoreColumnarResult, xsink);
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "slicing columnar result")) {
            return nullptr;
        }
        const Column& column = columns[i];
        ValueHolder data(columnar_slice_value(column.data, offset, count, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        if (rv->addColumn(column.name.c_str(), data.release(), column.schema, xsink)) {
            return nullptr;
        }
    }
    return rv.release();
}

QoreValue QoreColumnarResult::combineMasks(QoreValue lhs, QoreValue rhs, const char* op, ExceptionSink* xsink) {
    assert(xsink);
    bool is_and = columnar_mask_op_is_and(op, xsink);
    if (*xsink) {
        return QoreValue();
    }

    size_t lhs_size = columnar_mask_size(lhs, xsink);
    if (*xsink) {
        return QoreValue();
    }
    size_t rhs_size = columnar_mask_size(rhs, xsink);
    if (*xsink) {
        return QoreValue();
    }
    if (lhs_size != rhs_size) {
        xsink->raiseException("COLUMNAR-RESULT-ERROR",
            "cannot combine row masks of different lengths: " QLLD " and " QLLD,
            static_cast<int64>(lhs_size), static_cast<int64>(rhs_size));
        return QoreValue();
    }

    ReferenceHolder<QoreBufferNode> rv(new QoreBufferNode(QoreBufferElementType::Bool, false, lhs_size), xsink);
    for (size_t i = 0; i < lhs_size; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "combining columnar row masks")) {
            return QoreValue();
        }
        bool selected = is_and
            ? (columnar_mask_at(lhs, i, xsink) && columnar_mask_at(rhs, i, xsink))
            : (columnar_mask_at(lhs, i, xsink) || columnar_mask_at(rhs, i, xsink));
        if (*xsink) {
            return QoreValue();
        }
        if (rv->setEntry(i, QoreValue(selected), xsink)) {
            return QoreValue();
        }
    }
    return rv.release();
}

QoreValue QoreColumnarResult::invertMask(QoreValue mask, ExceptionSink* xsink) {
    assert(xsink);
    size_t mask_size = columnar_mask_size(mask, xsink);
    if (*xsink) {
        return QoreValue();
    }

    ReferenceHolder<QoreBufferNode> rv(new QoreBufferNode(QoreBufferElementType::Bool, false, mask_size), xsink);
    for (size_t i = 0; i < mask_size; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "inverting columnar row mask")) {
            return QoreValue();
        }
        bool selected = !columnar_mask_at(mask, i, xsink);
        if (*xsink || rv->setEntry(i, QoreValue(selected), xsink)) {
            return QoreValue();
        }
    }
    return rv.release();
}

QoreValue QoreColumnarResult::nullMask(const char* name, bool invert, ExceptionSink* xsink) const {
    assert(xsink);
    const Column* column = findColumn(name);
    if (!column) {
        xsink->raiseException("COLUMNAR-RESULT-ERROR", "column '%s' does not exist", name);
        return QoreValue();
    }

    ReferenceHolder<QoreBufferNode> rv(new QoreBufferNode(QoreBufferElementType::Bool, false, row_count), xsink);
    for (size_t i = 0; i < row_count; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "building columnar null mask")) {
            return QoreValue();
        }
        bool selected = columnar_value_is_null(column->data, i, xsink);
        if (*xsink) {
            return QoreValue();
        }
        if (invert) {
            selected = !selected;
        }
        if (rv->setEntry(i, QoreValue(selected), xsink)) {
            return QoreValue();
        }
    }
    return rv.release();
}

QoreValue QoreColumnarResult::getColumnValue(const char* name, ExceptionSink* xsink) const {
    const Column* column = findColumn(name);
    if (!column) {
        xsink->raiseException("COLUMNAR-RESULT-ERROR", "column '%s' does not exist", name);
        return QoreValue();
    }
    return column->data.refSelf();
}

QoreObject* qore_columnar_result_to_object(QoreColumnarResult* result, ExceptionSink* xsink) {
    if (!result) {
        return nullptr;
    }
    ReferenceHolder<QoreColumnarResult> result_holder(result, xsink);
    ReferenceHolder<QoreObject> obj(new QoreObject(QC_COLUMNARRESULT, nullptr), xsink);
    obj->setPrivate(CID_COLUMNARRESULT, result_holder.release());
    return obj.release();
}

QoreColumnarResult* qore_columnar_result_try_from_object(const QoreObject* obj, ExceptionSink* xsink) {
    return obj ? static_cast<QoreColumnarResult*>(obj->tryGetReferencedPrivateData(CID_COLUMNARRESULT, xsink))
        : nullptr;
}

QoreColumnarResult* qore_columnar_result_from_value(const QoreValue& value, const QoreHashNode* desc,
        const char* context, ExceptionSink* xsink) {
    assert(xsink);
    switch (value.getType()) {
        case NT_HASH:
            return QoreColumnarResult::fromColumnHash(value.get<const QoreHashNode>(), desc, xsink);
        case NT_LIST:
            return QoreColumnarResult::fromRows(value.get<const QoreListNode>(), desc, xsink);
        case NT_NOTHING:
        case NT_NULL:
            return new QoreColumnarResult;
        default:
            xsink->raiseException("COLUMNAR-RESULT-ERROR",
                "%s returned type '%s'; expected a hash of column containers, a list of row hashes, NOTHING, or NULL",
                context ? context : "columnar conversion input", value.getTypeName());
            return nullptr;
    }
}

int qore_columnar_result_export_arrow_c_data(const QoreColumnarResult* result, ArrowSchema* schema,
        ArrowArray* array, ExceptionSink* xsink) {
    assert(xsink);
    if (!result || !schema || !array) {
        xsink->raiseException("ARROW-C-DATA-ERROR",
            "Arrow C Data export requires non-null ColumnarResult, ArrowSchema, and ArrowArray pointers");
        return -1;
    }

    std::memset(schema, 0, sizeof(*schema));
    std::memset(array, 0, sizeof(*array));

    int64_t row_count = qore_arrow_int64_length(result->numRows(), xsink, "ColumnarResult export");
    if (*xsink) {
        return -1;
    }
    int64_t column_count = qore_arrow_int64_length(result->numColumns(), xsink, "ColumnarResult export");
    if (*xsink) {
        return -1;
    }

    qore_arrow_init_schema(schema, "+s", nullptr, false, column_count);
    qore_arrow_init_array(array, row_count, 0, 0, 1, column_count);

    for (size_t i = 0; i < result->numColumns(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "exporting ColumnarResult to Arrow C Data")) {
            qore_arrow_schema_release(schema);
            qore_arrow_array_release(array);
            return -1;
        }

        const QoreColumnarResult::Column* column = result->getColumn(i);
        assert(column);
        schema->children[i] = new ArrowSchema{};
        array->children[i] = new ArrowArray{};

        int rc;
        if (column->data.getType() == NT_BUFFER) {
            const QoreBufferNode* buffer = column->data.get<const QoreBufferNode>();
            rc = qore_arrow_export_buffer_array(buffer, column->name.c_str(), schema->children[i],
                array->children[i], xsink, column->data.refSelf());
        } else if (column->data.getType() == NT_LIST) {
            QoreArrowListValueSource source(column->data.get<const QoreListNode>());
            rc = qore_arrow_export_values_array(source, column->schema, column->name.c_str(),
                schema->children[i], array->children[i], xsink);
        } else {
            xsink->raiseException("ARROW-C-DATA-ERROR",
                "column '%s' has type '%s'; expected list or buffer for Arrow C Data export",
                column->name.c_str(), column->data.getTypeName());
            rc = -1;
        }

        if (rc || *xsink) {
            qore_arrow_schema_release(schema);
            qore_arrow_array_release(array);
            return -1;
        }
    }

    return 0;
}

QoreColumnarResult* qore_columnar_result_import_arrow_c_data(ArrowSchema* schema, ArrowArray* array,
        ExceptionSink* xsink) {
    assert(xsink);
    if (!schema || !array || !schema->release || !array->release) {
        xsink->raiseException("ARROW-C-DATA-ERROR",
            "Arrow C Data import requires non-null ArrowSchema and ArrowArray pointers with release callbacks");
        return nullptr;
    }
    if (!schema->format || std::strcmp(schema->format, "+s")) {
        xsink->raiseException("ARROW-C-DATA-ERROR",
            "Arrow C Data import expects a top-level struct schema ('+s'); got '%s'",
            schema->format ? schema->format : "<null>");
        return nullptr;
    }
    if (schema->n_children != array->n_children) {
        xsink->raiseException("ARROW-C-DATA-ERROR",
            "Arrow C Data import got " QLLD " schema children and " QLLD " array children",
            schema->n_children, array->n_children);
        return nullptr;
    }
    if (array->length < 0 || array->offset < 0) {
        xsink->raiseException("ARROW-C-DATA-ERROR",
            "Arrow C Data import requires non-negative top-level length and offset; got length " QLLD
            ", offset " QLLD, array->length, array->offset);
        return nullptr;
    }
    if (array->offset) {
        xsink->raiseException("ARROW-C-DATA-ERROR",
            "Arrow C Data import does not support a non-zero top-level struct offset; got offset " QLLD,
            array->offset);
        return nullptr;
    }

    std::shared_ptr<QoreArrowImportedOwner> import_owner = std::make_shared<QoreArrowImportedOwner>();
    import_owner->schema = *schema;
    import_owner->array = *array;
    std::memset(schema, 0, sizeof(*schema));
    std::memset(array, 0, sizeof(*array));
    std::shared_ptr<const void> storage_owner(import_owner, static_cast<const void*>(import_owner.get()));

    const ArrowSchema* root_schema = &import_owner->schema;
    const ArrowArray* root_array = &import_owner->array;
    ReferenceHolder<QoreColumnarResult> rv(new QoreColumnarResult, xsink);
    for (int64_t i = 0; i < root_schema->n_children; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "importing Arrow C Data ColumnarResult")) {
            return nullptr;
        }
        const ArrowSchema* child_schema = root_schema->children[i];
        const ArrowArray* child_array = root_array->children[i];
        if (!child_schema || !child_array) {
            xsink->raiseException("ARROW-C-DATA-ERROR",
                "Arrow C Data import child " QLLD " is missing schema or array", i);
            return nullptr;
        }
        if (child_array->length != root_array->length) {
            xsink->raiseException("ARROW-C-DATA-ERROR",
                "Arrow C Data import child '%s' has length " QLLD ", expected top-level length " QLLD,
                child_schema->name ? child_schema->name : "", child_array->length, root_array->length);
            return nullptr;
        }
        if (child_array->length < 0 || child_array->offset < 0) {
            xsink->raiseException("ARROW-C-DATA-ERROR",
                "Arrow C Data import child '%s' requires non-negative length and offset; got length " QLLD
                ", offset " QLLD, child_schema->name ? child_schema->name : "", child_array->length,
                child_array->offset);
            return nullptr;
        }

        QoreColumnarTypeDescriptor desc = qore_arrow_schema_to_descriptor(child_schema, xsink);
        if (*xsink) {
            return nullptr;
        }
        ValueHolder column(qore_arrow_import_top_level_column(child_schema, child_array, storage_owner, desc, xsink),
            xsink);
        if (*xsink) {
            return nullptr;
        }
        if (rv->addColumn(desc.name.empty() ? "" : desc.name.c_str(), column.release(), desc, xsink)) {
            return nullptr;
        }
    }

    return rv.release();
}

void qore_arrow_schema_release(ArrowSchema* schema) {
    if (schema && schema->release) {
        schema->release(schema);
    }
}

void qore_arrow_array_release(ArrowArray* array) {
    if (array && array->release) {
        array->release(array);
    }
}
