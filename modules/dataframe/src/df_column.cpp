/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    df_column.cpp

    DataFrame column utilities implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "df_column.h"

namespace QoreDataFrameNS {

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
            return QoreValue(new QoreStringNode(str_data[i]));
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
    ColumnType type = inferColumnType(values);
    return buildColumnData(values, type, xsink);
}

QoreListNode* columnToQoreList(const ColumnData& col, ExceptionSink* xsink) {
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

} // namespace QoreDataFrameNS
