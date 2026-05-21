/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    df_column.cpp

    DataFrame column utilities implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "df_column.h"

#include <cassert>
#include <cstring>
#include <limits>
#include <utility>

namespace QoreDataFrameNS {

ColumnData::ColumnData(const ColumnData& old)
        : type(old.type), n_rows(old.n_rows), dense_buffer(old.dense_buffer),
        dense_buffer_type(old.dense_buffer_type), float_data(old.float_data), int_data(old.int_data),
        str_data(old.str_data), bool_data(old.bool_data), date_data(old.date_data), null_mask(old.null_mask) {
    if (dense_buffer) {
        dense_buffer->ref();
    }
}

ColumnData::ColumnData(ColumnData&& old) noexcept
        : type(old.type), n_rows(old.n_rows), dense_buffer(old.dense_buffer),
        dense_buffer_type(old.dense_buffer_type), float_data(std::move(old.float_data)),
        int_data(std::move(old.int_data)), str_data(std::move(old.str_data)),
        bool_data(std::move(old.bool_data)), date_data(std::move(old.date_data)),
        null_mask(std::move(old.null_mask)) {
    old.dense_buffer = nullptr;
    old.dense_buffer_type = QoreBufferElementType::Invalid;
}

ColumnData& ColumnData::operator=(const ColumnData& old) {
    if (this != &old) {
        if (old.dense_buffer) {
            old.dense_buffer->ref();
        }
        if (dense_buffer) {
            ExceptionSink xsink;
            dense_buffer->deref(&xsink);
        }

        type = old.type;
        n_rows = old.n_rows;
        dense_buffer = old.dense_buffer;
        dense_buffer_type = old.dense_buffer_type;
        float_data = old.float_data;
        int_data = old.int_data;
        str_data = old.str_data;
        bool_data = old.bool_data;
        date_data = old.date_data;
        null_mask = old.null_mask;
    }
    return *this;
}

ColumnData& ColumnData::operator=(ColumnData&& old) noexcept {
    if (this != &old) {
        if (dense_buffer) {
            ExceptionSink xsink;
            dense_buffer->deref(&xsink);
        }

        type = old.type;
        n_rows = old.n_rows;
        dense_buffer = old.dense_buffer;
        dense_buffer_type = old.dense_buffer_type;
        float_data = std::move(old.float_data);
        int_data = std::move(old.int_data);
        str_data = std::move(old.str_data);
        bool_data = std::move(old.bool_data);
        date_data = std::move(old.date_data);
        null_mask = std::move(old.null_mask);

        old.dense_buffer = nullptr;
        old.dense_buffer_type = QoreBufferElementType::Invalid;
    }
    return *this;
}

ColumnData::~ColumnData() {
    if (dense_buffer) {
        ExceptionSink xsink;
        dense_buffer->deref(&xsink);
    }
}

void ColumnData::setDenseBufferRef(const QoreBufferNode* buffer) {
    if (dense_buffer) {
        ExceptionSink xsink;
        dense_buffer->deref(&xsink);
        dense_buffer = nullptr;
        dense_buffer_type = QoreBufferElementType::Invalid;
    }
    if (buffer) {
        dense_buffer = const_cast<QoreBufferNode*>(buffer);
        dense_buffer->ref();
        dense_buffer_type = buffer->getElementType();
    }
}

QoreBufferNode* ColumnData::refDenseBuffer() const {
    if (!dense_buffer) {
        return nullptr;
    }
    dense_buffer->ref();
    return dense_buffer;
}

const char* columnTypeName(ColumnType type) {
    switch (type) {
        case ColumnType::FLOAT64: return "float64";
        case ColumnType::INT64:   return "int64";
        case ColumnType::STRING:  return "string";
        case ColumnType::BOOL:    return "bool";
        case ColumnType::DATE:    return "date";
        case ColumnType::AUTO:    return "auto";
        default:                  return "unknown";
    }
}

int64_t ColumnData::countNonNull() const {
    int64_t count = 0;
    for (int64_t i = 0; i < n_rows; ++i) {
        if (!isNull(i)) {
            ++count;
        }
    }
    return count;
}

int64_t ColumnData::countNull() const {
    return n_rows - countNonNull();
}

QoreValue ColumnData::getValueAt(int64_t i, ExceptionSink* xsink) const {
    if (i < 0 || i >= n_rows) {
        xsink->raiseException("DATAFRAME-INDEX-ERROR",
            "row index " QLLD " out of range (0.." QLLD ")", i, n_rows - 1);
        return QoreValue();
    }
    if (isNull(i)) {
        return QoreValue();  // NOTHING
    }

    switch (type) {
        case ColumnType::FLOAT64:
            return QoreValue(float_data(i));
        case ColumnType::INT64:
            return QoreValue(int_data[i]);
        case ColumnType::STRING:
            return QoreValue::makeStringValue(str_data[i]);
        case ColumnType::BOOL:
            return QoreValue((bool)bool_data[i]);
        case ColumnType::DATE:
            return QoreValue(DateTimeNode::makeAbsolute(currentTZ(),
                date_data[i] / 1000000, (int)(date_data[i] % 1000000)));
        case ColumnType::AUTO:
            // AUTO columns are not yet supported; inferColumnType() never
            // returns AUTO — it resolves to a concrete type or STRING
            return QoreValue();
        default:
            return QoreValue();
    }
}

ColumnType inferColumnType(const QoreListNode* values) {
    if (!values || values->empty()) {
        return ColumnType::STRING;  // default for empty
    }

    bool has_int = false;
    bool has_float = false;
    bool has_string = false;
    bool has_bool = false;
    bool has_date = false;

    for (size_t i = 0; i < values->size(); ++i) {
        QoreValue v = values->retrieveEntry(i);
        if (v.isNullOrNothing()) {
            continue;  // nulls don't affect type inference
        }

        qore_type_t t = v.getType();
        switch (t) {
            case NT_INT:
                has_int = true;
                break;
            case NT_FLOAT:
            case NT_NUMBER:
                has_float = true;
                break;
            case NT_STRING:
                has_string = true;
                break;
            case NT_BOOLEAN:
                has_bool = true;
                break;
            case NT_DATE:
                has_date = true;
                break;
            default:
                // Unknown type → string fallback
                has_string = true;
                break;
        }
    }

    // Priority: if mixed numeric, widen to float64
    if (has_string) {
        return ColumnType::STRING;
    }
    if (has_float && has_int) {
        return ColumnType::FLOAT64;
    }
    if (has_float) {
        return ColumnType::FLOAT64;
    }
    if (has_int && !has_bool && !has_date) {
        return ColumnType::INT64;
    }
    if (has_bool && !has_int && !has_float && !has_date) {
        return ColumnType::BOOL;
    }
    if (has_date && !has_int && !has_float && !has_bool) {
        return ColumnType::DATE;
    }
    // Mixed types → string
    if (has_int || has_float || has_bool || has_date) {
        return ColumnType::STRING;
    }
    // All nulls
    return ColumnType::FLOAT64;
}

std::shared_ptr<ColumnData> buildColumnData(const QoreListNode* values,
        ColumnType type, ExceptionSink* xsink) {
    auto col = std::make_shared<ColumnData>();
    col->type = type;
    int64_t n = values ? (int64_t)values->size() : 0;
    col->n_rows = n;
    col->null_mask.resize(n, 0);

    switch (type) {
        case ColumnType::FLOAT64: {
            col->float_data.resize(n);
            for (int64_t i = 0; i < n; ++i) {
                QoreValue v = values->retrieveEntry(i);
                if (v.isNullOrNothing()) {
                    col->null_mask[i] = 1;
                    col->float_data(i) = std::numeric_limits<double>::quiet_NaN();
                } else {
                    col->float_data(i) = v.getAsFloat();
                }
            }
            break;
        }
        case ColumnType::INT64: {
            col->int_data.resize(n);
            for (int64_t i = 0; i < n; ++i) {
                QoreValue v = values->retrieveEntry(i);
                if (v.isNullOrNothing()) {
                    col->null_mask[i] = 1;
                    col->int_data[i] = 0;
                } else {
                    col->int_data[i] = v.getAsBigInt();
                }
            }
            break;
        }
        case ColumnType::STRING: {
            col->str_data.resize(n);
            for (int64_t i = 0; i < n; ++i) {
                QoreValue v = values->retrieveEntry(i);
                if (v.isNullOrNothing()) {
                    col->null_mask[i] = 1;
                } else {
                    QoreStringValueHelper str(v);
                    col->str_data[i] = str->c_str();
                }
            }
            break;
        }
        case ColumnType::BOOL: {
            col->bool_data.resize(n);
            for (int64_t i = 0; i < n; ++i) {
                QoreValue v = values->retrieveEntry(i);
                if (v.isNullOrNothing()) {
                    col->null_mask[i] = 1;
                    col->bool_data[i] = 0;
                } else {
                    col->bool_data[i] = v.getAsBool() ? 1 : 0;
                }
            }
            break;
        }
        case ColumnType::DATE: {
            col->date_data.resize(n);
            for (int64_t i = 0; i < n; ++i) {
                QoreValue v = values->retrieveEntry(i);
                if (v.isNullOrNothing()) {
                    col->null_mask[i] = 1;
                    col->date_data[i] = 0;
                } else if (v.getType() == NT_DATE) {
                    const DateTimeNode* dt = v.get<const DateTimeNode>();
                    col->date_data[i] = dt->getEpochMicrosecondsUTC();
                } else {
                    // Non-date value in a date column → treat as null
                    // (type inference should prevent this; if it happens,
                    // the column would have been inferred as STRING instead)
                    col->null_mask[i] = 1;
                    col->date_data[i] = 0;
                }
            }
            break;
        }
        default:
            break;
    }

    return col;
}

std::shared_ptr<ColumnData> buildColumnDataAuto(const QoreListNode* values,
        ExceptionSink* xsink) {
    int64_t n = values ? (int64_t)values->size() : 0;
    if (!n) {
        return buildColumnData(values, ColumnType::STRING, xsink);
    }

    auto col = std::make_shared<ColumnData>();
    col->type = ColumnType::AUTO;
    col->n_rows = n;
    col->null_mask.resize(n, 0);

    auto set_float_nulls = [&](int64_t count) -> bool {
        for (int64_t j = 0; j < count; ++j) {
            if (j && !(j % 100) && qore_check_cancel(xsink, "building DataFrame column")) {
                return true;
            }
            if (col->null_mask[j]) {
                col->float_data(j) = std::numeric_limits<double>::quiet_NaN();
            }
        }
        return false;
    };

    auto initialize_type = [&](ColumnType type, int64_t initialized_rows) -> bool {
        col->type = type;
        switch (type) {
            case ColumnType::FLOAT64:
                col->float_data.resize(n);
                return set_float_nulls(initialized_rows);
            case ColumnType::INT64:
                col->int_data.resize(n);
                break;
            case ColumnType::STRING:
                col->str_data.resize(n);
                break;
            case ColumnType::BOOL:
                col->bool_data.resize(n);
                break;
            case ColumnType::DATE:
                col->date_data.resize(n);
                break;
            default:
                break;
        }
        return false;
    };

    auto widen_int_to_float = [&](int64_t initialized_rows) -> bool {
        Eigen::VectorXd float_data(n);
        for (int64_t j = 0; j < initialized_rows; ++j) {
            if (j && !(j % 100) && qore_check_cancel(xsink, "building DataFrame column")) {
                return true;
            }
            float_data(j) = col->null_mask[j]
                ? std::numeric_limits<double>::quiet_NaN() : (double)col->int_data[j];
        }
        col->int_data.clear();
        col->int_data.shrink_to_fit();
        col->float_data = std::move(float_data);
        col->type = ColumnType::FLOAT64;
        return false;
    };

    for (int64_t i = 0; i < n; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame column")) {
            return nullptr;
        }

        QoreValue v = values->retrieveEntry(i);
        if (v.isNullOrNothing()) {
            col->null_mask[i] = 1;
            if (col->type == ColumnType::FLOAT64) {
                col->float_data(i) = std::numeric_limits<double>::quiet_NaN();
            }
            continue;
        }

        switch (v.getType()) {
            case NT_INT:
                if (col->type == ColumnType::AUTO) {
                    if (initialize_type(ColumnType::INT64, i)) {
                        return nullptr;
                    }
                } else if (col->type == ColumnType::FLOAT64) {
                    col->float_data(i) = v.getAsFloat();
                    continue;
                } else if (col->type != ColumnType::INT64) {
                    return buildColumnData(values, ColumnType::STRING, xsink);
                }
                col->int_data[i] = v.getAsBigInt();
                break;

            case NT_FLOAT:
            case NT_NUMBER:
                if (col->type == ColumnType::AUTO) {
                    if (initialize_type(ColumnType::FLOAT64, i)) {
                        return nullptr;
                    }
                } else if (col->type == ColumnType::INT64) {
                    if (widen_int_to_float(i)) {
                        return nullptr;
                    }
                } else if (col->type != ColumnType::FLOAT64) {
                    return buildColumnData(values, ColumnType::STRING, xsink);
                }
                col->float_data(i) = v.getAsFloat();
                break;

            case NT_STRING: {
                if (col->type == ColumnType::AUTO) {
                    if (initialize_type(ColumnType::STRING, i)) {
                        return nullptr;
                    }
                } else if (col->type != ColumnType::STRING) {
                    return buildColumnData(values, ColumnType::STRING, xsink);
                }
                QoreStringValueHelper str(v);
                col->str_data[i] = str->c_str();
                break;
            }

            case NT_BOOLEAN:
                if (col->type == ColumnType::AUTO) {
                    if (initialize_type(ColumnType::BOOL, i)) {
                        return nullptr;
                    }
                } else if (col->type != ColumnType::BOOL) {
                    return buildColumnData(values, ColumnType::STRING, xsink);
                }
                col->bool_data[i] = v.getAsBool() ? 1 : 0;
                break;

            case NT_DATE:
                if (col->type == ColumnType::AUTO) {
                    if (initialize_type(ColumnType::DATE, i)) {
                        return nullptr;
                    }
                } else if (col->type != ColumnType::DATE) {
                    return buildColumnData(values, ColumnType::STRING, xsink);
                }
                col->date_data[i] = v.get<const DateTimeNode>()->getEpochMicrosecondsUTC();
                break;

            default:
                return buildColumnData(values, ColumnType::STRING, xsink);
        }
    }

    if (col->type == ColumnType::AUTO) {
        if (initialize_type(ColumnType::FLOAT64, n)) {
            return nullptr;
        }
    }

    return col;
}

std::shared_ptr<ColumnData> buildColumnDataFromBuffer(const QoreBufferNode* values,
        ExceptionSink* xsink) {
    if (!values) {
        return std::make_shared<ColumnData>();
    }

    auto col = std::make_shared<ColumnData>();
    int64_t n = values ? (int64_t)values->size() : 0;
    col->n_rows = n;
    col->null_mask.resize(n, 0);
    col->setDenseBufferRef(values);

    switch (values->getElementType()) {
        case QoreBufferElementType::Int8:
        case QoreBufferElementType::Int16:
        case QoreBufferElementType::Int32:
        case QoreBufferElementType::Int64: {
            col->type = ColumnType::INT64;
            col->int_data.resize(n);
            if (!values->hasNullableElements()) {
                switch (values->getElementType()) {
                    case QoreBufferElementType::Int8: {
                        const int8_t* src = static_cast<const int8_t*>(values->getRawData());
                        for (int64_t i = 0; i < n; ++i) {
                            if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame column from buffer")) {
                                return nullptr;
                            }
                            col->int_data[i] = src[i];
                        }
                        break;
                    }
                    case QoreBufferElementType::Int16: {
                        const int16_t* src = static_cast<const int16_t*>(values->getRawData());
                        for (int64_t i = 0; i < n; ++i) {
                            if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame column from buffer")) {
                                return nullptr;
                            }
                            col->int_data[i] = src[i];
                        }
                        break;
                    }
                    case QoreBufferElementType::Int32: {
                        const int32_t* src = static_cast<const int32_t*>(values->getRawData());
                        for (int64_t i = 0; i < n; ++i) {
                            if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame column from buffer")) {
                                return nullptr;
                            }
                            col->int_data[i] = src[i];
                        }
                        break;
                    }
                    case QoreBufferElementType::Int64: {
                        const int64_t* src = static_cast<const int64_t*>(values->getRawData());
                        if (n) {
                            std::memcpy(col->int_data.data(), src, n * sizeof(int64_t));
                        }
                        break;
                    }
                    default:
                        break;
                }
            } else {
                for (int64_t i = 0; i < n; ++i) {
                    if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame column from buffer")) {
                        return nullptr;
                    }
                    if (values->isElementNull(i)) {
                        col->null_mask[i] = 1;
                        col->int_data[i] = 0;
                    } else {
                        col->int_data[i] = values->getReferencedEntry(i).getAsBigInt();
                    }
                }
            }
            break;
        }
        case QoreBufferElementType::Float32:
        case QoreBufferElementType::Float64: {
            col->type = ColumnType::FLOAT64;
            col->float_data.resize(n);
            if (!values->hasNullableElements()) {
                if (values->getElementType() == QoreBufferElementType::Float64) {
                    const double* src = static_cast<const double*>(values->getRawData());
                    if (n) {
                        std::memcpy(col->float_data.data(), src, n * sizeof(double));
                    }
                } else {
                    const float* src = static_cast<const float*>(values->getRawData());
                    for (int64_t i = 0; i < n; ++i) {
                        if (i && !(i % 100)
                                && qore_check_cancel(xsink, "building DataFrame column from buffer")) {
                            return nullptr;
                        }
                        col->float_data(i) = src[i];
                    }
                }
            } else {
                for (int64_t i = 0; i < n; ++i) {
                    if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame column from buffer")) {
                        return nullptr;
                    }
                    if (values->isElementNull(i)) {
                        col->null_mask[i] = 1;
                        col->float_data(i) = std::numeric_limits<double>::quiet_NaN();
                    } else {
                        col->float_data(i) = values->getReferencedEntry(i).getAsFloat();
                    }
                }
            }
            break;
        }
        case QoreBufferElementType::Bool: {
            col->type = ColumnType::BOOL;
            col->bool_data.resize(n);
            for (int64_t i = 0; i < n; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame column from buffer")) {
                    return nullptr;
                }
                if (values->isElementNull(i)) {
                    col->null_mask[i] = 1;
                    col->bool_data[i] = 0;
                } else {
                    col->bool_data[i] = values->getReferencedEntry(i).getAsBool() ? 1 : 0;
                }
            }
            break;
        }
        case QoreBufferElementType::String: {
            col->type = ColumnType::STRING;
            col->str_data.resize(n);
            for (int64_t i = 0; i < n; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame column from buffer")) {
                    return nullptr;
                }
                if (values->isElementNull(i)) {
                    col->null_mask[i] = 1;
                } else if (!getDataFrameString(values->getReferencedEntry(i), col->str_data[i])) {
                    xsink->raiseException("DATAFRAME-ERROR",
                        "buffer<string> element " QLLD " could not be converted to a DataFrame string", i);
                    return nullptr;
                }
            }
            break;
        }
        default:
            xsink->raiseException("DATAFRAME-ERROR",
                "unsupported buffer element type '%s' for DataFrame column",
                qore_buffer_element_type_name(values->getElementType()));
            return nullptr;
    }

    return col;
}

QoreListNode* columnToQoreList(const ColumnData& col, ExceptionSink* xsink) {
    if (col.hasDenseBuffer()) {
        return col.dense_buffer->toList(xsink);
    }

    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);

    for (int64_t i = 0; i < col.n_rows; ++i) {
        if (col.isNull(i)) {
            list->push(QoreValue(), xsink);
        } else {
            switch (col.type) {
                case ColumnType::FLOAT64:
                    list->push(col.float_data(i), xsink);
                    break;
                case ColumnType::INT64:
                    list->push(col.int_data[i], xsink);
                    break;
                case ColumnType::STRING:
                    list->push(new QoreStringNode(col.str_data[i]), xsink);
                    break;
                case ColumnType::BOOL:
                    list->push((bool)col.bool_data[i], xsink);
                    break;
                case ColumnType::DATE:
                    list->push(DateTimeNode::makeAbsolute(currentTZ(),
                        col.date_data[i] / 1000000,
                        (int)(col.date_data[i] % 1000000)), xsink);
                    break;
                default:
                    list->push(QoreValue(), xsink);
                    break;
            }
        }
    }

    return list.release();
}

QoreListNode* columnToQoreListRange(const ColumnData& col, int64_t start,
        int64_t count, ExceptionSink* xsink) {
    if (start < 0 || count < 0 || start + count > col.n_rows) {
        xsink->raiseException("DATAFRAME-INDEX-ERROR",
            "column slice start " QLLD " count " QLLD " out of range for " QLLD " rows",
            start, count, col.n_rows);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);

    if (col.hasDenseBuffer()) {
        ReferenceHolder<QoreBufferNode> buffer(col.dense_buffer->slice(
            static_cast<size_t>(start), static_cast<size_t>(count), xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        return buffer->toList(xsink);
    }

    for (int64_t i = start; i < start + count; ++i) {
        if (i != start && !((i - start) % 100)
                && qore_check_cancel(xsink, "converting DataFrame column range to list")) {
            return nullptr;
        }
        if (col.isNull(i)) {
            list->push(QoreValue(), xsink);
        } else {
            switch (col.type) {
                case ColumnType::FLOAT64:
                    list->push(col.float_data(i), xsink);
                    break;
                case ColumnType::INT64:
                    list->push(col.int_data[i], xsink);
                    break;
                case ColumnType::STRING:
                    list->push(new QoreStringNode(col.str_data[i]), xsink);
                    break;
                case ColumnType::BOOL:
                    list->push((bool)col.bool_data[i], xsink);
                    break;
                case ColumnType::DATE:
                    list->push(DateTimeNode::makeAbsolute(currentTZ(),
                        col.date_data[i] / 1000000,
                        (int)(col.date_data[i] % 1000000)), xsink);
                    break;
                default:
                    list->push(QoreValue(), xsink);
                    break;
            }
        }
    }

    return list.release();
}

QoreBufferNode* columnToQoreBuffer(const ColumnData& col, ExceptionSink* xsink) {
    if (col.hasDenseBuffer()) {
        return col.refDenseBuffer();
    }

    QoreBufferElementType element_type = QoreBufferElementType::Invalid;
    switch (col.type) {
        case ColumnType::FLOAT64:
            element_type = QoreBufferElementType::Float64;
            break;
        case ColumnType::INT64:
            element_type = QoreBufferElementType::Int64;
            break;
        case ColumnType::BOOL:
            element_type = QoreBufferElementType::Bool;
            break;
        case ColumnType::STRING:
            element_type = QoreBufferElementType::String;
            break;
        default:
            xsink->raiseException("DATAFRAME-COLUMN-ERROR",
                "column type '%s' cannot be represented as a dense buffer",
                columnTypeName(col.type));
            return nullptr;
    }

    if (element_type == QoreBufferElementType::String) {
        ReferenceHolder<QoreListNode> values(columnToQoreList(col, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        return new QoreBufferNode(element_type, col.countNull() > 0, *values, xsink);
    }

    ReferenceHolder<QoreBufferNode> buffer(
        new QoreBufferNode(element_type, col.countNull() > 0, static_cast<size_t>(col.n_rows)), xsink);

    for (int64_t i = 0; i < col.n_rows; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "building dense DataFrame column buffer")) {
            return nullptr;
        }
        if (col.isNull(i)) {
            if (buffer->setEntry(static_cast<size_t>(i), QoreValue(), xsink)) {
                return nullptr;
            }
            continue;
        }

        QoreValue value;
        switch (col.type) {
            case ColumnType::FLOAT64:
                value = QoreValue(col.float_data(i));
                break;
            case ColumnType::INT64:
                value = QoreValue(col.int_data[i]);
                break;
            case ColumnType::BOOL:
                value = QoreValue(static_cast<bool>(col.bool_data[i]));
                break;
            case ColumnType::STRING:
                value = QoreValue::makeStringValue(col.str_data[i]);
                break;
            default:
                assert(false);
                break;
        }
        if (buffer->setEntry(static_cast<size_t>(i), value, xsink)) {
            return nullptr;
        }
    }

    return buffer.release();
}

} // namespace QoreDataFrameNS
