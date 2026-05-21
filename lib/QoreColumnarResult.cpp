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
#include <cstring>
#include <initializer_list>
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
};

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

    ExceptionSink xsink;
    QoreString str;
    value.getAsString(str, 0, &xsink);
    return xsink ? std::string() : std::string(str.c_str());
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
        shape = {QoreColumnarColumnType::Number, QoreBufferElementType::Invalid, numberTypeInfo, false, false};
        return true;
    }

    if (type.find("char") != std::string::npos || type.find("text") != std::string::npos
            || type.find("clob") != std::string::npos || type == "string" || type == "enum"
            || type == "set" || type == "uuid" || type == "xml") {
        shape = {QoreColumnarColumnType::String, QoreBufferElementType::Invalid, stringTypeInfo, true, false};
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
            shape = {QoreColumnarColumnType::String, QoreBufferElementType::Invalid, stringTypeInfo, true, false};
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
            return {QoreColumnarColumnType::String, QoreBufferElementType::Invalid, stringTypeInfo, nullable, false};
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

static QoreValue columnar_value_at(QoreValue value, size_t index) {
    switch (value.getType()) {
        case NT_LIST:
            return value.get<const QoreListNode>()->retrieveEntry(index).refSelf();
        case NT_BUFFER:
            return value.get<const QoreBufferNode>()->getReferencedEntry(index);
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

static bool columnar_mask_at(QoreValue mask, size_t index) {
    switch (mask.getType()) {
        case NT_LIST:
            return mask.get<const QoreListNode>()->retrieveEntry(index).getAsBool();
        case NT_BUFFER: {
            QoreValue value = mask.get<const QoreBufferNode>()->getReferencedEntry(index);
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
        if (columnar_mask_at(mask, i)) {
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
            if (columnar_mask_at(mask, i)) {
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
        ReferenceHolder<QoreBufferNode> rv(new QoreBufferNode(source->getElementType(),
            source->hasNullableElements(), selected), xsink);
        size_t out = 0;
        for (size_t i = 0; i < source->size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "filtering columnar buffer column")) {
                return QoreValue();
            }
            if (columnar_mask_at(mask, i)) {
                if (rv->setEntry(out++, source->getReferencedEntry(i), xsink)) {
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

static bool columnar_value_is_null(QoreValue value, size_t index) {
    switch (value.getType()) {
        case NT_LIST:
            return value.get<const QoreListNode>()->retrieveEntry(index).isNullOrNothing();
        case NT_BUFFER:
            return value.get<const QoreBufferNode>()->getReferencedEntry(index).isNullOrNothing();
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
    return QoreColumnarColumnType::Auto;
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

QoreColumnarResult::Column::Column(std::string n_name, QoreColumnarColumnType n_column_type,
        QoreBufferElementType n_buffer_type, bool n_nullable, std::string n_native_type, QoreValue n_data)
        : name(std::move(n_name)), column_type(n_column_type), buffer_type(n_buffer_type), nullable(n_nullable),
        native_type(std::move(n_native_type)), data(n_data) {
}

QoreColumnarResult::Column::Column(Column&& old) noexcept
        : name(std::move(old.name)), column_type(old.column_type), buffer_type(old.buffer_type),
        nullable(old.nullable), native_type(std::move(old.native_type)), data(old.data) {
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

    columns.emplace_back(name, column_type, buffer_type, nullable, native_type ? native_type : "",
        data_holder.release());
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
            if (rv->addColumn(i.getKey(), value.refSelf(), columnar_type_from_buffer(buffer->getElementType()),
                    buffer->getElementType(), buffer->hasNullableElements(), native_type.c_str(), xsink)) {
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

        ValueHolder column_value(columnar_make_column_value(source, shape, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        if (rv->addColumn(i.getKey(), column_value.release(), shape.column_type, shape.buffer_type, shape.nullable,
                native_type.c_str(), xsink)) {
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
        rv->push(h.release(), xsink);
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
            row->setKeyValue(column.name.c_str(), columnar_value_at(column.data, r), xsink);
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
        if (rv->addColumn(column.name.c_str(), data.release(), column.column_type, column.buffer_type,
                column.nullable, column.native_type.c_str(), xsink)) {
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
        if (rv->addColumn(column.name.c_str(), data.release(), column.column_type, column.buffer_type,
                column.nullable, column.native_type.c_str(), xsink)) {
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
            ? (columnar_mask_at(lhs, i) && columnar_mask_at(rhs, i))
            : (columnar_mask_at(lhs, i) || columnar_mask_at(rhs, i));
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
        if (rv->setEntry(i, QoreValue(!columnar_mask_at(mask, i)), xsink)) {
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
        bool selected = columnar_value_is_null(column->data, i);
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
