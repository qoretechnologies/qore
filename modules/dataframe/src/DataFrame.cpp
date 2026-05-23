/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    DataFrame.cpp

    DataFrame core implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_DataFrame.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <unordered_set>

namespace QoreDataFrameNS {

namespace {

struct RecordColumnInference {
    bool has_int = false;
    bool has_float = false;
    bool has_string = false;
    bool has_bool = false;
    bool has_date = false;
    bool has_auto = false;
};

static void mergeRecordColumnType(RecordColumnInference& inf, QoreValue value) {
    if (value.isNullOrNothing()) {
        return;
    }

    switch (value.getType()) {
        case NT_INT:
            inf.has_int = true;
            break;
        case NT_FLOAT:
        case NT_NUMBER:
            inf.has_float = true;
            break;
        case NT_STRING:
            inf.has_string = true;
            break;
        case NT_BOOLEAN:
            inf.has_bool = true;
            break;
        case NT_DATE:
            inf.has_date = true;
            break;
        default:
            inf.has_auto = true;
            break;
    }
}

static ColumnType finishRecordColumnType(const RecordColumnInference& inf) {
    if (inf.has_auto) {
        return ColumnType::AUTO;
    }
    if (inf.has_string) {
        return ColumnType::STRING;
    }
    if (inf.has_float) {
        return ColumnType::FLOAT64;
    }
    if (inf.has_int && !inf.has_bool && !inf.has_date) {
        return ColumnType::INT64;
    }
    if (inf.has_bool && !inf.has_int && !inf.has_float && !inf.has_date) {
        return ColumnType::BOOL;
    }
    if (inf.has_date && !inf.has_int && !inf.has_float && !inf.has_bool) {
        return ColumnType::DATE;
    }
    if (inf.has_int || inf.has_float || inf.has_bool || inf.has_date) {
        return ColumnType::STRING;
    }
    return ColumnType::FLOAT64;
}

static bool getContiguousSelectionRange(const std::vector<int64_t>& indices, int64_t n_rows,
        int64_t& start, ExceptionSink* xsink) {
    if (indices.empty()) {
        start = 0;
        return true;
    }

    start = indices[0];
    if (start < 0 || start >= n_rows || indices.size() > static_cast<size_t>(n_rows - start)) {
        return false;
    }

    for (size_t i = 1; i < indices.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "checking contiguous DataFrame row selection")) {
            return false;
        }
        if (indices[i] != start + static_cast<int64_t>(i)) {
            return false;
        }
    }

    return true;
}

static ColumnType fastRecordColumnType(QoreValue value) {
    if (value.isNullOrNothing()) {
        return ColumnType::AUTO;
    }

    switch (value.getType()) {
        case NT_INT:
            return ColumnType::INT64;
        case NT_FLOAT:
        case NT_NUMBER:
            return ColumnType::FLOAT64;
        case NT_STRING:
            return ColumnType::STRING;
        case NT_BOOLEAN:
            return ColumnType::BOOL;
        case NT_DATE:
            return ColumnType::DATE;
        default:
            return ColumnType::AUTO;
    }
}

static bool fastRecordValueCompatible(ColumnType type, QoreValue value) {
    if (value.isNullOrNothing()) {
        return true;
    }

    switch (type) {
        case ColumnType::FLOAT64:
            return value.getType() == NT_FLOAT || value.getType() == NT_NUMBER || value.getType() == NT_INT;
        case ColumnType::INT64:
            return value.getType() == NT_INT;
        case ColumnType::STRING:
            return true;
        case ColumnType::BOOL:
            return value.getType() == NT_BOOLEAN;
        case ColumnType::DATE:
            return value.getType() == NT_DATE;
        case ColumnType::AUTO:
            return true;
        default:
            return false;
    }
}

static void initializeRecordColumnData(ColumnData& data, ColumnType type, int64_t n) {
    data.type = type;
    data.n_rows = n;
    data.null_mask.resize(n, 0);
    switch (data.type) {
        case ColumnType::FLOAT64:
            data.float_data.resize(n);
            break;
        case ColumnType::INT64:
            data.int_data.resize(n);
            break;
        case ColumnType::STRING:
            data.str_data.resize(n);
            break;
        case ColumnType::BOOL:
            data.bool_data.resize(n);
            break;
        case ColumnType::DATE:
            data.date_data.resize(n);
            break;
        case ColumnType::AUTO:
            data.auto_data.resize(n);
            break;
        default:
            break;
    }
}

static void storeRecordValue(ColumnData& data, int64_t i, QoreValue value) {
    if (value.isNullOrNothing()) {
        data.null_mask[i] = 1;
        if (data.type == ColumnType::FLOAT64) {
            data.float_data(i) = std::numeric_limits<double>::quiet_NaN();
        }
        return;
    }

    switch (data.type) {
        case ColumnType::FLOAT64:
            data.float_data(i) = value.getAsFloat();
            break;
        case ColumnType::INT64:
            data.int_data[i] = value.getAsBigInt();
            break;
        case ColumnType::STRING: {
            QoreStringValueHelper str(value);
            data.str_data[i] = str->c_str();
            break;
        }
        case ColumnType::BOOL:
            data.bool_data[i] = value.getAsBool() ? 1 : 0;
            break;
        case ColumnType::DATE:
            if (value.getType() == NT_DATE) {
                data.date_data[i] = value.get<const DateTimeNode>()->getEpochMicrosecondsUTC();
            } else {
                data.null_mask[i] = 1;
            }
            break;
        case ColumnType::AUTO:
            data.setAutoValue(i, value, nullptr);
            break;
        default:
            break;
    }
}

static QoreColumnarColumnType columnarTypeFromDataFrameType(ColumnType type) {
    switch (type) {
        case ColumnType::FLOAT64:
            return QoreColumnarColumnType::Float;
        case ColumnType::INT64:
            return QoreColumnarColumnType::Int;
        case ColumnType::BOOL:
            return QoreColumnarColumnType::Bool;
        case ColumnType::DATE:
            return QoreColumnarColumnType::Date;
        case ColumnType::STRING:
            return QoreColumnarColumnType::String;
        case ColumnType::AUTO:
        default:
            return QoreColumnarColumnType::Auto;
    }
}

static QoreBufferElementType bufferTypeFromDataFrameType(ColumnType type) {
    switch (type) {
        case ColumnType::FLOAT64:
            return QoreBufferElementType::Float64;
        case ColumnType::INT64:
            return QoreBufferElementType::Int64;
        case ColumnType::BOOL:
            return QoreBufferElementType::Bool;
        case ColumnType::STRING:
            return QoreBufferElementType::String;
        default:
            return QoreBufferElementType::Invalid;
    }
}

static ColumnType dataFrameTypeFromColumnarType(QoreColumnarColumnType type) {
    switch (type) {
        case QoreColumnarColumnType::Bool:
            return ColumnType::BOOL;
        case QoreColumnarColumnType::Int:
            return ColumnType::INT64;
        case QoreColumnarColumnType::Float:
            return ColumnType::FLOAT64;
        case QoreColumnarColumnType::Number:
            return ColumnType::AUTO;
        case QoreColumnarColumnType::Date:
            return ColumnType::DATE;
        case QoreColumnarColumnType::String:
            return ColumnType::STRING;
        case QoreColumnarColumnType::Binary:
            return ColumnType::AUTO;
        case QoreColumnarColumnType::Auto:
        default:
            return ColumnType::AUTO;
    }
}

static ColumnType dataFrameTypeFromColumnarSchema(const QoreColumnarTypeDescriptor& schema,
        QoreColumnarColumnType fallback) {
    switch (schema.kind) {
        case QoreColumnarTypeKind::Bool:
            return ColumnType::BOOL;
        case QoreColumnarTypeKind::Int:
            return ColumnType::INT64;
        case QoreColumnarTypeKind::Float:
            return ColumnType::FLOAT64;
        case QoreColumnarTypeKind::String:
            return ColumnType::STRING;
        case QoreColumnarTypeKind::Date:
        case QoreColumnarTypeKind::Timestamp:
            return ColumnType::DATE;
        case QoreColumnarTypeKind::Number:
        case QoreColumnarTypeKind::Binary:
        case QoreColumnarTypeKind::Duration:
        case QoreColumnarTypeKind::Decimal128:
        case QoreColumnarTypeKind::List:
        case QoreColumnarTypeKind::LargeList:
        case QoreColumnarTypeKind::FixedSizeList:
        case QoreColumnarTypeKind::Struct:
        case QoreColumnarTypeKind::Map:
        case QoreColumnarTypeKind::Dictionary:
        case QoreColumnarTypeKind::Auto:
            return ColumnType::AUTO;
        default:
            return dataFrameTypeFromColumnarType(fallback);
    }
}

static void preserveColumnarSchema(ColumnData& data, const std::string& name,
        const QoreColumnarTypeDescriptor& schema) {
    data.columnar_schema = schema;
    data.columnar_schema.name = name;
    data.has_columnar_schema = true;
}

static std::string dataFrameValueKey(const ColumnData& cd, int64_t row) {
    if (cd.isNull(row)) {
        return std::string("\xffnull", 5);
    }

    std::string key;
    switch (cd.type) {
        case ColumnType::FLOAT64: {
            double value = cd.float_data(row);
            if (value == 0.0) {
                value = 0.0;  // Normalize -0.0 to match numeric equality.
            }
            uint64_t bits;
            std::memcpy(&bits, &value, sizeof(bits));
            key.assign("f", 1);
            key.append(reinterpret_cast<const char*>(&bits), sizeof(bits));
            return key;
        }
        case ColumnType::INT64: {
            int64_t value = cd.int_data[row];
            key.assign("i", 1);
            key.append(reinterpret_cast<const char*>(&value), sizeof(value));
            return key;
        }
        case ColumnType::STRING:
            key.assign("s", 1);
            key.append(cd.str_data[row]);
            return key;
        case ColumnType::BOOL:
            return cd.bool_data[row] ? std::string("b1", 2) : std::string("b0", 2);
        case ColumnType::DATE: {
            int64_t value = cd.date_data[row];
            key.assign("d", 1);
            key.append(reinterpret_cast<const char*>(&value), sizeof(value));
            return key;
        }
        case ColumnType::AUTO: {
            QoreStringValueHelper sh(cd.auto_data[row]);
            key.assign("a", 1);
            key.append(sh->c_str(), sh->size());
            return key;
        }
        default:
            return {};
    }
}

}

QoreDataFrame::QoreDataFrame() {
}

QoreDataFrame* QoreDataFrame::fromRecords(const QoreListNode* records,
        ExceptionSink* xsink) {
    if (qore_check_cancel(xsink, "building DataFrame from records")) {
        return nullptr;
    }
    if (!records || records->empty()) {
        return new QoreDataFrame();
    }

    // First pass: collect column names from the first record
    const QoreHashNode* first = records->retrieveEntry(0).get<const QoreHashNode>();
    if (!first) {
        xsink->raiseException("DATAFRAME-ERROR",
            "expected list of hashes, got non-hash element at index 0");
        return nullptr;
    }

    std::vector<std::string> col_names;
    col_names.reserve(first->size());
    ConstHashIterator hi(first);
    while (hi.next()) {
        col_names.push_back(hi.getKey());
    }

    int64_t n = (int64_t)records->size();
    size_t num_cols = col_names.size();
    std::vector<const char*> col_keys;
    col_keys.reserve(num_cols);
    for (const std::string& name : col_names) {
        col_keys.push_back(name.c_str());
    }

    std::vector<ColumnType> fast_types(num_cols);
    bool can_fast = true;
    for (size_t c = 0; c < num_cols; ++c) {
        fast_types[c] = fastRecordColumnType(first->getKeyValue(col_keys[c]));
        if (fast_types[c] == ColumnType::AUTO) {
            can_fast = false;
            break;
        }
    }

    if (can_fast) {
        auto* df = new QoreDataFrame();
        df->n_rows = n;
        df->columns.reserve(num_cols);
        df->col_index.reserve(num_cols);

        for (size_t c = 0; c < num_cols; ++c) {
            auto data = std::make_shared<ColumnData>();
            initializeRecordColumnData(*data, fast_types[c], n);

            Column col;
            col.name = col_names[c];
            col.data = std::move(data);
            df->col_index[col.name] = df->columns.size();
            df->columns.push_back(std::move(col));
        }

        bool fast_failed = false;
        for (int64_t i = 0; i < n; ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame from records")) {
                delete df;
                return nullptr;
            }
            const QoreHashNode* row = records->retrieveEntry(i).get<const QoreHashNode>();
            if (!row) {
                xsink->raiseException("DATAFRAME-ERROR",
                    "expected hash at index " QLLD ", got non-hash", i);
                delete df;
                return nullptr;
            }
            for (size_t c = 0; c < num_cols; ++c) {
                QoreValue value = row->getKeyValue(col_keys[c]);
                if (!fastRecordValueCompatible(fast_types[c], value)) {
                    fast_failed = true;
                    break;
                }
                storeRecordValue(*df->columns[c].data, i, value);
            }
            if (fast_failed) {
                break;
            }
        }

        if (!fast_failed) {
            return df;
        }
        delete df;
    }

    std::vector<RecordColumnInference> inferred(num_cols);
    std::vector<const QoreHashNode*> rows;
    rows.reserve(n);
    for (int64_t i = 0; i < n; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "inferring DataFrame record columns")) {
            return nullptr;
        }
        const QoreHashNode* row = records->retrieveEntry(i).get<const QoreHashNode>();
        if (!row) {
            xsink->raiseException("DATAFRAME-ERROR",
                "expected hash at index " QLLD ", got non-hash", i);
            return nullptr;
        }
        rows.push_back(row);
        for (size_t c = 0; c < num_cols; ++c) {
            mergeRecordColumnType(inferred[c], row->getKeyValue(col_keys[c]));
        }
    }

    auto* df = new QoreDataFrame();
    df->n_rows = n;
    df->columns.reserve(num_cols);
    df->col_index.reserve(num_cols);
    for (size_t c = 0; c < num_cols; ++c) {
        auto data = std::make_shared<ColumnData>();
        initializeRecordColumnData(*data, finishRecordColumnType(inferred[c]), n);

        Column col;
        col.name = col_names[c];
        col.data = std::move(data);
        df->col_index[col.name] = df->columns.size();
        df->columns.push_back(std::move(col));
    }

    for (int64_t i = 0; i < n; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame from records")) {
            delete df;
            return nullptr;
        }
        const QoreHashNode* row = rows[i];
        for (size_t c = 0; c < num_cols; ++c) {
            storeRecordValue(*df->columns[c].data, i, row->getKeyValue(col_keys[c]));
        }
    }

    return df;
}

QoreDataFrame* QoreDataFrame::fromColumns(const QoreHashNode* columns,
        ExceptionSink* xsink) {
    if (qore_check_cancel(xsink, "building DataFrame from columns")) {
        return nullptr;
    }
    if (!columns || columns->empty()) {
        return new QoreDataFrame();
    }

    auto* df = new QoreDataFrame();
    int64_t expected_rows = -1;

    ConstHashIterator hi(columns);
    while (hi.next()) {
        QoreValue col_value = hi.get();
        const QoreListNode* values = col_value.getType() == NT_LIST
            ? col_value.get<const QoreListNode>() : nullptr;
        const QoreBufferNode* buffer = col_value.getType() == NT_BUFFER
            ? col_value.get<const QoreBufferNode>() : nullptr;
        if (!values && !buffer) {
            xsink->raiseException("DATAFRAME-ERROR",
                "column '%s' value must be a list or buffer", hi.getKey());
            delete df;
            return nullptr;
        }

        int64_t col_rows = values ? (int64_t)values->size() : (int64_t)buffer->size();
        if (expected_rows < 0) {
            expected_rows = col_rows;
        } else if (col_rows != expected_rows) {
            xsink->raiseException("DATAFRAME-ERROR",
                "column '%s' has " QLLD " rows, expected " QLLD,
                hi.getKey(), col_rows, expected_rows);
            delete df;
            return nullptr;
        }

        Column col;
        col.name = hi.getKey();
        col.data = values
            ? buildColumnDataAuto(values, xsink)
            : buildColumnDataFromBuffer(buffer, xsink);
        if (*xsink) {
            delete df;
            return nullptr;
        }
        df->col_index[col.name] = df->columns.size();
        df->columns.push_back(std::move(col));
    }

    df->n_rows = expected_rows >= 0 ? expected_rows : 0;
    return df;
}

QoreDataFrame* QoreDataFrame::fromColumnarResult(const QoreColumnarResult* result,
        ExceptionSink* xsink) {
    if (qore_check_cancel(xsink, "building DataFrame from columnar result")) {
        return nullptr;
    }
    if (!result) {
        return new QoreDataFrame();
    }

    auto* df = new QoreDataFrame();
    df->n_rows = static_cast<int64_t>(result->numRows());

    for (size_t i = 0; i < result->numColumns(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame from columnar result")) {
            delete df;
            return nullptr;
        }

        const QoreColumnarResult::Column* src = result->getColumn(i);
        assert(src);

        Column col;
        col.name = src->name;

        switch (src->data.getType()) {
            case NT_LIST: {
                const QoreListNode* values = src->data.get<const QoreListNode>();
                ColumnType type = dataFrameTypeFromColumnarSchema(src->schema, src->column_type);
                col.data = type == ColumnType::AUTO
                    ? buildColumnData(values, ColumnType::AUTO, xsink) : buildColumnData(values, type, xsink);
                break;
            }
            case NT_BUFFER:
                col.data = buildColumnDataFromBuffer(src->data.get<const QoreBufferNode>(), xsink);
                break;
            default:
                xsink->raiseException("DATAFRAME-ERROR",
                    "ColumnarResult column '%s' has unsupported storage type '%s'; expected list or buffer",
                    src->name.c_str(), src->data.getTypeName());
                delete df;
                return nullptr;
        }

        if (*xsink) {
            delete df;
            return nullptr;
        }
        if (col.data) {
            preserveColumnarSchema(*col.data, col.name, src->schema);
        }
        if (col.data && col.data->n_rows != df->n_rows) {
            xsink->raiseException("DATAFRAME-ERROR",
                "ColumnarResult column '%s' has " QLLD " rows, expected " QLLD,
                src->name.c_str(), col.data->n_rows, df->n_rows);
            delete df;
            return nullptr;
        }

        df->col_index[col.name] = df->columns.size();
        df->columns.push_back(std::move(col));
    }

    return df;
}

int64_t QoreDataFrame::numRows() const {
    std::lock_guard<std::mutex> lk(mtx);
    return n_rows;
}

int64_t QoreDataFrame::numCols() const {
    std::lock_guard<std::mutex> lk(mtx);
    return (int64_t)columns.size();
}

QoreListNode* QoreDataFrame::columnNames(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    ReferenceHolder<QoreListNode> list(new QoreListNode(stringTypeInfo), xsink);
    for (const auto& col : columns) {
        list->push(new QoreStringNode(col.name), xsink);
    }
    return list.release();
}

QoreListNode* QoreDataFrame::dtypes(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    ReferenceHolder<QoreListNode> list(new QoreListNode(stringTypeInfo), xsink);
    for (const auto& col : columns) {
        list->push(new QoreStringNode(columnTypeName(col.data->type)), xsink);
    }
    return list.release();
}

QoreHashNode* QoreDataFrame::shape(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclDataFrameShape, xsink), xsink);
    h->setKeyValue("rows", n_rows, xsink);
    h->setKeyValue("cols", (int64_t)columns.size(), xsink);
    return h.release();
}

int QoreDataFrame::getColIdx(const std::string& name, ExceptionSink* xsink) const {
    auto it = col_index.find(name);
    if (it == col_index.end()) {
        xsink->raiseException("DATAFRAME-COLUMN-ERROR",
            "column '%s' not found", name.c_str());
        return -1;
    }
    return (int)it->second;
}

QoreListNode* QoreDataFrame::getColumn(const std::string& name,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    int idx = getColIdx(name, xsink);
    if (idx < 0) {
        return nullptr;
    }
    return columnToQoreList(*columns[idx].data, xsink);
}

QoreBufferNode* QoreDataFrame::getColumnBuffer(const std::string& name,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    int idx = getColIdx(name, xsink);
    if (idx < 0) {
        return nullptr;
    }
    return columnToQoreBuffer(*columns[idx].data, xsink);
}

QoreHashNode* QoreDataFrame::getRow(int64_t index, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (n_rows == 0) {
        xsink->raiseException("DATAFRAME-INDEX-ERROR",
            "row index " QLLD " out of range: DataFrame is empty", index);
        return nullptr;
    }
    if (index < 0 || index >= n_rows) {
        xsink->raiseException("DATAFRAME-INDEX-ERROR",
            "row index " QLLD " out of range (0.." QLLD ")", index, n_rows - 1);
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> row(new QoreHashNode(autoTypeInfo), xsink);
    for (const auto& col : columns) {
        QoreValue v = col.data->getValueAt(index, xsink);
        if (*xsink) {
            return nullptr;
        }
        row->setKeyValue(col.name.c_str(), v, xsink);
    }
    return row.release();
}

QoreDataFrame* QoreDataFrame::sliceRows(int64_t start, int64_t count,
        ExceptionSink* xsink) const {
    // Caller must hold lock
    auto* df = new QoreDataFrame();
    df->n_rows = count;

    for (const auto& src_col : columns) {
        if (df->columns.size() && !(df->columns.size() % 100)
                && qore_check_cancel(xsink, "slicing DataFrame rows")) {
            delete df;
            return nullptr;
        }

        std::shared_ptr<ColumnData> new_data;
        if (src_col.data->hasDenseBuffer()) {
            ReferenceHolder<QoreBufferNode> buffer(src_col.data->dense_buffer->view(
                static_cast<size_t>(start), static_cast<size_t>(count)), xsink);
            new_data = buildColumnDataFromBuffer(*buffer, xsink);
            if (*xsink) {
                delete df;
                return nullptr;
            }
        } else {
            new_data = std::make_shared<ColumnData>();
            new_data->type = src_col.data->type;
            new_data->n_rows = count;
            new_data->null_mask.assign(
                src_col.data->null_mask.begin() + start,
                src_col.data->null_mask.begin() + start + count);

            switch (src_col.data->type) {
                case ColumnType::FLOAT64:
                    new_data->float_data = src_col.data->float_data.segment(start, count);
                    break;
                case ColumnType::INT64:
                    new_data->int_data.assign(
                        src_col.data->int_data.begin() + start,
                        src_col.data->int_data.begin() + start + count);
                    break;
                case ColumnType::STRING:
                    new_data->str_data.assign(
                        src_col.data->str_data.begin() + start,
                        src_col.data->str_data.begin() + start + count);
                    break;
                case ColumnType::BOOL:
                    new_data->bool_data.assign(
                        src_col.data->bool_data.begin() + start,
                        src_col.data->bool_data.begin() + start + count);
                    break;
                case ColumnType::DATE:
                    new_data->date_data.assign(
                        src_col.data->date_data.begin() + start,
                        src_col.data->date_data.begin() + start + count);
                    break;
                case ColumnType::AUTO:
                    new_data->auto_data.resize(count);
                    for (int64_t i = 0; i < count; ++i) {
                        if (i && !(i % 100) && qore_check_cancel(xsink, "slicing auto DataFrame rows")) {
                            delete df;
                            return nullptr;
                        }
                        if (!new_data->null_mask[i]) {
                            new_data->setAutoValue(i, src_col.data->auto_data[start + i], xsink);
                        }
                    }
                    break;
                default:
                    break;
            }
        }
        if (new_data && src_col.data->has_columnar_schema) {
            preserveColumnarSchema(*new_data, src_col.name, src_col.data->columnar_schema);
        }
        if (new_data && src_col.data->external_column_kind != ExternalColumnKind::NONE) {
            sliceExternalColumnRef(*new_data, *src_col.data, start, count);
        }

        Column col;
        col.name = src_col.name;
        col.data = std::move(new_data);
        df->col_index[col.name] = df->columns.size();
        df->columns.push_back(std::move(col));
    }

    return df;
}

QoreDataFrame* QoreDataFrame::head(int64_t n, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (n < 0) {
        n = 0;
    }
    if (n > n_rows) {
        n = n_rows;
    }
    return sliceRows(0, n, xsink);
}

QoreDataFrame* QoreDataFrame::tail(int64_t n, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (n < 0) {
        n = 0;
    }
    if (n > n_rows) {
        n = n_rows;
    }
    return sliceRows(n_rows - n, n, xsink);
}

QoreDataFrame* QoreDataFrame::slice(int64_t start, int64_t end,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (start < 0) {
        start = 0;
    }
    if (end > n_rows) {
        end = n_rows;
    }
    if (start >= end) {
        return new QoreDataFrame();
    }
    return sliceRows(start, end - start, xsink);
}

QoreDataFrame* QoreDataFrame::sliceRange(const QoreValue& start_index, const QoreValue& stop_index,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!n_rows) {
        return new QoreDataFrame();
    }

    bool no_start = start_index.isNothing();
    bool no_stop = stop_index.isNothing();
    int64_t start = no_start ? 0 : start_index.getAsBigInt();
    int64_t stop = no_stop ? n_rows - 1 : stop_index.getAsBigInt();

    if ((no_start && stop < 0) || (no_stop && start >= n_rows)) {
        return new QoreDataFrame();
    }

    if (start < stop) {
        if (start >= n_rows || stop < 0) {
            return new QoreDataFrame();
        }
        if (start < 0) {
            start = 0;
        }
        if (stop >= n_rows) {
            stop = n_rows - 1;
        }
        return sliceRows(start, stop - start + 1, xsink);
    }

    if (stop > n_rows - 1 || start < 0) {
        return new QoreDataFrame();
    }
    if (stop < 0) {
        stop = 0;
    }
    if (start >= n_rows) {
        start = n_rows - 1;
    }

    std::vector<int64_t> indices;
    indices.reserve(start - stop + 1);
    for (int64_t i = start; i >= stop; --i) {
        if (indices.size() && !(indices.size() % 100) && qore_check_cancel(xsink, "slicing DataFrame range")) {
            return nullptr;
        }
        indices.push_back(i);
    }
    return selectRows(indices, xsink);
}

QoreListNode* QoreDataFrame::toRecords(ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "converting DataFrame to records")) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx);
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);

    for (int64_t i = 0; i < n_rows; ++i) {
        ReferenceHolder<QoreHashNode> row(new QoreHashNode(autoTypeInfo), xsink);
        for (const auto& col : columns) {
            QoreValue v = col.data->getValueAt(i, xsink);
            if (*xsink) {
                return nullptr;
            }
            row->setKeyValue(col.name.c_str(), v, xsink);
        }
        list->push(row.release(), xsink);
    }

    return list.release();
}

QoreHashNode* QoreDataFrame::toColumnHash(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);

    for (const auto& col : columns) {
        QoreListNode* values = columnToQoreList(*col.data, xsink);
        if (*xsink) {
            return nullptr;
        }
        h->setKeyValue(col.name.c_str(), values, xsink);
    }

    return h.release();
}

QoreColumnarResult* QoreDataFrame::toColumnarResult(ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "converting DataFrame to columnar result")) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx);
    ReferenceHolder<QoreColumnarResult> result(new QoreColumnarResult, xsink);

    for (const auto& col : columns) {
        if (result->numColumns() && !(result->numColumns() % 100)
                && qore_check_cancel(xsink, "converting DataFrame to columnar result")) {
            return nullptr;
        }

        const ColumnData& cd = *col.data;
        QoreColumnarColumnType columnar_type = columnarTypeFromDataFrameType(cd.type);
        QoreBufferElementType buffer_type = cd.hasDenseBuffer()
            ? cd.dense_buffer_type : bufferTypeFromDataFrameType(cd.type);
        bool nullable = cd.hasDenseBuffer() ? cd.dense_buffer->hasNullableElements() : cd.countNull() > 0;
        QoreColumnarTypeDescriptor schema;
        bool use_schema = cd.has_columnar_schema;
        if (use_schema) {
            schema = cd.columnar_schema;
            schema.name = col.name;
            schema.nullable = schema.nullable || nullable;
            if (schema.buffer_type == QoreBufferElementType::Invalid
                    && buffer_type != QoreBufferElementType::Invalid) {
                schema.buffer_type = buffer_type;
            }
            if (schema.column_type == QoreColumnarColumnType::Auto) {
                schema.column_type = columnar_type;
            }
        }

        if (buffer_type != QoreBufferElementType::Invalid) {
            ReferenceHolder<QoreBufferNode> buffer(columnToQoreBuffer(cd, xsink), xsink);
            if (*xsink) {
                return nullptr;
            }
            int rc = use_schema
                ? result->addColumn(col.name.c_str(), QoreValue(buffer.release()), schema, xsink)
                : result->addColumn(col.name.c_str(), QoreValue(buffer.release()), columnar_type, buffer_type,
                    nullable, "", xsink);
            if (rc) {
                return nullptr;
            }
        } else {
            ReferenceHolder<QoreListNode> values(columnToQoreList(cd, xsink), xsink);
            if (*xsink) {
                return nullptr;
            }
            int rc = use_schema
                ? result->addColumn(col.name.c_str(), QoreValue(values.release()), schema, xsink)
                : result->addColumn(col.name.c_str(), QoreValue(values.release()), columnar_type,
                    QoreBufferElementType::Invalid, nullable, "", xsink);
            if (rc) {
                return nullptr;
            }
        }
    }

    return result.release();
}

QoreListNode* QoreDataFrame::describe(ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "computing DataFrame statistics")) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx);
    ReferenceHolder<QoreListNode> result(new QoreListNode(hashdeclColumnStats->getTypeInfo()), xsink);

    for (const auto& col : columns) {
        ReferenceHolder<QoreHashNode> stats(new QoreHashNode(hashdeclColumnStats, xsink), xsink);
        const ColumnData& cd = *col.data;

        stats->setKeyValue("name", new QoreStringNode(col.name), xsink);
        stats->setKeyValue("dtype", new QoreStringNode(columnTypeName(cd.type)), xsink);
        stats->setKeyValue("count", cd.countNonNull(), xsink);
        stats->setKeyValue("null_count", cd.countNull(), xsink);

        // Numeric statistics for float64 and int64
        if (cd.type == ColumnType::FLOAT64 || cd.type == ColumnType::INT64) {
            // Collect non-null values
            std::vector<double> vals;
            vals.reserve(cd.n_rows);
            for (int64_t i = 0; i < cd.n_rows; ++i) {
                if (!cd.isNull(i)) {
                    double v = (cd.type == ColumnType::FLOAT64)
                        ? cd.float_data(i) : (double)cd.int_data[i];
                    vals.push_back(v);
                }
            }

            if (!vals.empty()) {
                std::sort(vals.begin(), vals.end());
                double sum = 0;
                for (double v : vals) {
                    sum += v;
                }
                double mean = sum / vals.size();

                double var_sum = 0;
                for (double v : vals) {
                    var_sum += (v - mean) * (v - mean);
                }
                double std_val = (vals.size() > 1)
                    ? std::sqrt(var_sum / (vals.size() - 1)) : 0.0;

                auto percentile = [&vals](double p) -> double {
                    double idx = p * (vals.size() - 1);
                    int lo = (int)idx;
                    int hi = lo + 1;
                    if (hi >= (int)vals.size()) {
                        return vals.back();
                    }
                    double frac = idx - lo;
                    return vals[lo] * (1.0 - frac) + vals[hi] * frac;
                };

                stats->setKeyValue("mean", mean, xsink);
                stats->setKeyValue("std", std_val, xsink);
                stats->setKeyValue("min", vals.front(), xsink);
                stats->setKeyValue("q25", percentile(0.25), xsink);
                stats->setKeyValue("q50", percentile(0.50), xsink);
                stats->setKeyValue("q75", percentile(0.75), xsink);
                stats->setKeyValue("max", vals.back(), xsink);
            }
        }

        // Unique count for string columns
        if (cd.type == ColumnType::STRING) {
            std::unordered_set<std::string> uniq;
            for (int64_t i = 0; i < cd.n_rows; ++i) {
                if (!cd.isNull(i)) {
                    uniq.insert(cd.str_data[i]);
                }
            }
            stats->setKeyValue("unique_count", (int64_t)uniq.size(), xsink);
        }

        result->push(stats.release(), xsink);
    }

    return result.release();
}

QoreStringNode* QoreDataFrame::toString(int64_t max_rows, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (max_rows <= 0) {
        max_rows = 10;
    }

    std::ostringstream oss;
    oss << "DataFrame(" << n_rows << " rows x " << columns.size() << " cols)\n";

    // Column headers
    for (size_t c = 0; c < columns.size(); ++c) {
        if (c > 0) {
            oss << "\t";
        }
        oss << columns[c].name;
    }
    oss << "\n";

    // Rows
    int64_t show = std::min(max_rows, n_rows);
    for (int64_t i = 0; i < show; ++i) {
        for (size_t c = 0; c < columns.size(); ++c) {
            if (c > 0) {
                oss << "\t";
            }
            const ColumnData& cd = *columns[c].data;
            if (cd.isNull(i)) {
                oss << "NULL";
            } else {
                switch (cd.type) {
                    case ColumnType::FLOAT64:
                        oss << cd.float_data(i);
                        break;
                    case ColumnType::INT64:
                        oss << cd.int_data[i];
                        break;
                    case ColumnType::STRING:
                        oss << cd.str_data[i];
                        break;
                    case ColumnType::BOOL:
                        oss << (cd.bool_data[i] ? "True" : "False");
                        break;
                    case ColumnType::DATE:
                        oss << cd.date_data[i];
                        break;
                    case ColumnType::AUTO: {
                        QoreStringValueHelper sh(cd.auto_data[i]);
                        oss << sh->c_str();
                        break;
                    }
                    default:
                        oss << "?";
                        break;
                }
            }
        }
        oss << "\n";
    }

    if (n_rows > show) {
        oss << "... (" << (n_rows - show) << " more rows)\n";
    }

    return new QoreStringNode(oss.str());
}

// --- ML Integration ---

QoreListNode* QoreDataFrame::toMatrix(const QoreListNode* col_list,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    // Resolve columns — if null, use all numeric columns
    std::vector<size_t> col_indices;
    if (col_list && col_list->size() > 0) {
        for (size_t i = 0; i < col_list->size(); ++i) {
            std::string name;
            if (!getDataFrameString(col_list->retrieveEntry(i), name)) {
                xsink->raiseException("DATAFRAME-ERROR",
                    "toMatrix column list element %zu is not a string", i);
                return nullptr;
            }
            int idx = getColIdx(name, xsink);
            if (idx < 0) {
                return nullptr;
            }
            ColumnType t = columns[idx].data->type;
            if (t != ColumnType::FLOAT64 && t != ColumnType::INT64) {
                xsink->raiseException("DATAFRAME-ERROR",
                    "toMatrix: column '%s' is %s, not numeric",
                    name.c_str(), columnTypeName(t));
                return nullptr;
            }
            col_indices.push_back(idx);
        }
    } else {
        // All numeric columns
        for (size_t i = 0; i < columns.size(); ++i) {
            ColumnType t = columns[i].data->type;
            if (t == ColumnType::FLOAT64 || t == ColumnType::INT64) {
                col_indices.push_back(i);
            }
        }
    }

    if (col_indices.empty()) {
        return new QoreListNode(autoTypeInfo);
    }

    // Build matrix: list of row lists
    ReferenceHolder<QoreListNode> matrix(new QoreListNode(autoTypeInfo), xsink);
    for (int64_t r = 0; r < n_rows; ++r) {
        ReferenceHolder<QoreListNode> row(new QoreListNode(autoTypeInfo), xsink);
        for (size_t ci : col_indices) {
            const ColumnData& cd = *columns[ci].data;
            if (cd.isNull(r)) {
                row->push(std::numeric_limits<double>::quiet_NaN(), xsink);
            } else if (cd.type == ColumnType::FLOAT64) {
                row->push(cd.float_data(r), xsink);
            } else {
                row->push((double)cd.int_data[r], xsink);
            }
        }
        matrix->push(row.release(), xsink);
    }

    return matrix.release();
}

QoreListNode* QoreDataFrame::toVector(const std::string& column,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    int idx = getColIdx(column, xsink);
    if (idx < 0) {
        return nullptr;
    }

    const ColumnData& cd = *columns[idx].data;
    if (cd.type != ColumnType::FLOAT64 && cd.type != ColumnType::INT64) {
        xsink->raiseException("DATAFRAME-ERROR",
            "toVector: column '%s' is %s, not numeric",
            column.c_str(), columnTypeName(cd.type));
        return nullptr;
    }

    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);
    for (int64_t i = 0; i < n_rows; ++i) {
        if (cd.isNull(i)) {
            list->push(std::numeric_limits<double>::quiet_NaN(), xsink);
        } else if (cd.type == ColumnType::FLOAT64) {
            list->push(cd.float_data(i), xsink);
        } else {
            list->push((double)cd.int_data[i], xsink);
        }
    }

    return list.release();
}

QoreDataFrame* QoreDataFrame::fromMatrix(const QoreListNode* matrix,
        const QoreListNode* column_names, ExceptionSink* xsink) {
    if (!matrix || matrix->empty()) {
        return new QoreDataFrame();
    }

    // Determine dimensions from first row
    const QoreListNode* first_row = matrix->retrieveEntry(0).get<const QoreListNode>();
    if (!first_row) {
        xsink->raiseException("DATAFRAME-ERROR",
            "fromMatrix: first element is not a list");
        return nullptr;
    }
    size_t num_cols = first_row->size();
    int64_t num_rows = (int64_t)matrix->size();

    // Generate column names if not provided
    std::vector<std::string> names;
    if (column_names && column_names->size() > 0) {
        for (size_t i = 0; i < column_names->size() && i < num_cols; ++i) {
            std::string name;
            names.push_back(getDataFrameString(column_names->retrieveEntry(i), name)
                ? name : ("col_" + std::to_string(i)));
        }
    }
    while (names.size() < num_cols) {
        names.push_back("col_" + std::to_string(names.size()));
    }

    // Build float64 columns
    auto* df = new QoreDataFrame();
    df->n_rows = num_rows;

    for (size_t c = 0; c < num_cols; ++c) {
        auto cd = std::make_shared<ColumnData>();
        cd->type = ColumnType::FLOAT64;
        cd->n_rows = num_rows;
        cd->null_mask.resize(num_rows, 0);
        cd->float_data.resize(num_rows);

        for (int64_t r = 0; r < num_rows; ++r) {
            const QoreListNode* row = matrix->retrieveEntry(r).get<const QoreListNode>();
            if (!row || c >= row->size()) {
                cd->null_mask[r] = 1;
                cd->float_data(r) = std::numeric_limits<double>::quiet_NaN();
            } else {
                QoreValue v = row->retrieveEntry(c);
                if (v.isNullOrNothing()) {
                    cd->null_mask[r] = 1;
                    cd->float_data(r) = std::numeric_limits<double>::quiet_NaN();
                } else {
                    cd->float_data(r) = v.getAsFloat();
                }
            }
        }

        Column col;
        col.name = names[c];
        col.data = std::move(cd);
        df->col_index[col.name] = df->columns.size();
        df->columns.push_back(std::move(col));
    }

    return df;
}

// --- Query Operations ---

QoreDataFrame* QoreDataFrame::selectRows(const std::vector<int64_t>& indices,
        ExceptionSink* xsink) const {
    // Caller must hold lock
    auto* df = new QoreDataFrame();
    int64_t count = (int64_t)indices.size();
    df->n_rows = count;

    bool identity_selection = count == n_rows;
    if (identity_selection) {
        for (int64_t i = 0; i < count; ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "checking DataFrame row selection")) {
                delete df;
                return nullptr;
            }
            if (indices[i] != i) {
                identity_selection = false;
                break;
            }
        }
    }
    if (!identity_selection) {
        int64_t contiguous_start = 0;
        if (getContiguousSelectionRange(indices, n_rows, contiguous_start, xsink)) {
            delete df;
            return sliceRows(contiguous_start, count, xsink);
        }
        if (*xsink) {
            delete df;
            return nullptr;
        }
    }

    for (const auto& src_col : columns) {
        if (df->columns.size() && !(df->columns.size() % 100)
                && qore_check_cancel(xsink, "selecting DataFrame rows")) {
            delete df;
            return nullptr;
        }

        std::shared_ptr<ColumnData> new_data;
        if (identity_selection) {
            new_data = src_col.data;
        } else if (src_col.data->hasDenseBuffer()) {
            ReferenceHolder<QoreBufferNode> buffer(new QoreBufferNode(src_col.data->dense_buffer_type,
                src_col.data->dense_buffer->hasNullableElements(), static_cast<size_t>(count)), xsink);
            for (int64_t i = 0; i < count; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "selecting dense DataFrame rows")) {
                    delete df;
                    return nullptr;
                }
                ValueHolder value(src_col.data->dense_buffer->getReferencedEntry(static_cast<size_t>(indices[i]),
                    xsink), xsink);
                if (buffer->setEntry(static_cast<size_t>(i), *value, xsink)) {
                    delete df;
                    return nullptr;
                }
            }

            new_data = buildColumnDataFromBuffer(*buffer, xsink);
            if (*xsink) {
                delete df;
                return nullptr;
            }
        } else {
            new_data = std::make_shared<ColumnData>();
            new_data->type = src_col.data->type;
            new_data->n_rows = count;
            new_data->null_mask.resize(count);

            switch (src_col.data->type) {
                case ColumnType::FLOAT64: {
                    new_data->float_data.resize(count);
                    for (int64_t i = 0; i < count; ++i) {
                        if (i && !(i % 100) && qore_check_cancel(xsink, "selecting float DataFrame rows")) {
                            delete df;
                            return nullptr;
                        }
                        new_data->null_mask[i] = src_col.data->null_mask[indices[i]];
                        new_data->float_data(i) = src_col.data->float_data(indices[i]);
                    }
                    break;
                }
                case ColumnType::INT64: {
                    new_data->int_data.resize(count);
                    for (int64_t i = 0; i < count; ++i) {
                        if (i && !(i % 100) && qore_check_cancel(xsink, "selecting int DataFrame rows")) {
                            delete df;
                            return nullptr;
                        }
                        new_data->null_mask[i] = src_col.data->null_mask[indices[i]];
                        new_data->int_data[i] = src_col.data->int_data[indices[i]];
                    }
                    break;
                }
                case ColumnType::STRING: {
                    new_data->str_data.resize(count);
                    for (int64_t i = 0; i < count; ++i) {
                        if (i && !(i % 100) && qore_check_cancel(xsink, "selecting string DataFrame rows")) {
                            delete df;
                            return nullptr;
                        }
                        new_data->null_mask[i] = src_col.data->null_mask[indices[i]];
                        new_data->str_data[i] = src_col.data->str_data[indices[i]];
                    }
                    break;
                }
                case ColumnType::BOOL: {
                    new_data->bool_data.resize(count);
                    for (int64_t i = 0; i < count; ++i) {
                        if (i && !(i % 100) && qore_check_cancel(xsink, "selecting bool DataFrame rows")) {
                            delete df;
                            return nullptr;
                        }
                        new_data->null_mask[i] = src_col.data->null_mask[indices[i]];
                        new_data->bool_data[i] = src_col.data->bool_data[indices[i]];
                    }
                    break;
                }
                case ColumnType::DATE: {
                    new_data->date_data.resize(count);
                    for (int64_t i = 0; i < count; ++i) {
                        if (i && !(i % 100) && qore_check_cancel(xsink, "selecting date DataFrame rows")) {
                            delete df;
                            return nullptr;
                        }
                        new_data->null_mask[i] = src_col.data->null_mask[indices[i]];
                        new_data->date_data[i] = src_col.data->date_data[indices[i]];
                    }
                    break;
                }
                case ColumnType::AUTO: {
                    new_data->auto_data.resize(count);
                    for (int64_t i = 0; i < count; ++i) {
                        if (i && !(i % 100) && qore_check_cancel(xsink, "selecting auto DataFrame rows")) {
                            delete df;
                            return nullptr;
                        }
                        int64_t src_index = indices[i];
                        new_data->null_mask[i] = src_col.data->null_mask[src_index];
                        if (!new_data->null_mask[i]) {
                            new_data->setAutoValue(i, src_col.data->auto_data[src_index], xsink);
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
        if (new_data && src_col.data->has_columnar_schema) {
            preserveColumnarSchema(*new_data, src_col.name, src_col.data->columnar_schema);
        }

        Column col;
        col.name = src_col.name;
        col.data = std::move(new_data);
        df->col_index[col.name] = df->columns.size();
        df->columns.push_back(std::move(col));
    }

    return df;
}

QoreDataFrame* QoreDataFrame::select(const QoreListNode* col_list,
        ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "selecting DataFrame columns")) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx);

    auto* df = new QoreDataFrame();
    df->n_rows = n_rows;

    for (size_t i = 0; i < col_list->size(); ++i) {
        std::string name;
        if (!getDataFrameString(col_list->retrieveEntry(i), name)) {
            xsink->raiseException("DATAFRAME-ERROR",
                "column list element %zu is not a string", i);
            delete df;
            return nullptr;
        }
        int idx = getColIdx(name, xsink);
        if (idx < 0) {
            delete df;
            return nullptr;
        }
        // Share the column data (COW — no copy until mutation)
        Column col;
        col.name = name;
        col.data = columns[idx].data;
        df->col_index[col.name] = df->columns.size();
        df->columns.push_back(std::move(col));
    }

    return df;
}

std::vector<uint8_t> QoreDataFrame::compareColumnMask(const std::string& column, const std::string& op,
        QoreValue value, ExceptionSink* xsink) const {
    // Validate operator
    static const std::unordered_set<std::string> valid_ops = {
        "==", "!=", "<", "<=", ">", ">=",
        "contains", "startswith", "endswith",
        "is_null", "not_null",
    };
    if (!valid_ops.count(op)) {
        xsink->raiseException("DATAFRAME-FILTER-ERROR",
            "unknown filter operator '%s'; valid operators: ==, !=, <, <=, >, >=, "
            "contains, startswith, endswith, is_null, not_null", op.c_str());
        return {};
    }

    if (qore_check_cancel(xsink, "building DataFrame row mask")) {
        return {};
    }
    std::lock_guard<std::mutex> lk(mtx);

    int col_idx = getColIdx(column, xsink);
    if (col_idx < 0) {
        return {};
    }

    const ColumnData& cd = *columns[col_idx].data;
    std::vector<uint8_t> mask(n_rows, 0);

    const bool op_eq = op == "==";
    const bool op_ne = op == "!=";
    const bool op_lt = op == "<";
    const bool op_le = op == "<=";
    const bool op_gt = op == ">";
    const bool op_ge = op == ">=";
    const bool op_contains = op == "contains";
    const bool op_startswith = op == "startswith";
    const bool op_endswith = op == "endswith";
    const bool op_is_null = op == "is_null";
    const bool op_not_null = op == "not_null";
    const bool cmp_is_null = value.isNullOrNothing();
    double cmp_float = 0.0;
    int64_t cmp_int = 0;
    bool cmp_bool = false;
    int64_t cmp_date = 0;
    std::string cmp_string;
    if (!cmp_is_null) {
        switch (cd.type) {
            case ColumnType::FLOAT64:
                cmp_float = value.getAsFloat();
                break;
            case ColumnType::INT64:
                cmp_int = value.getAsBigInt();
                break;
            case ColumnType::STRING: {
                QoreStringValueHelper sh(value);
                cmp_string = sh->c_str();
                break;
            }
            case ColumnType::BOOL:
                cmp_bool = value.getAsBool();
                break;
            case ColumnType::DATE:
                if (value.getType() == NT_DATE) {
                    cmp_date = value.get<const DateTimeNode>()->getEpochMicrosecondsUTC();
                }
                break;
            default:
                break;
        }
    }

    for (int64_t i = 0; i < n_rows; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame row mask")) {
            return {};
        }

        if (cd.isNull(i)) {
            mask[i] = (op_is_null || (op_eq && cmp_is_null)) ? 1 : 0;
            continue;
        }

        if (op_not_null) {
            mask[i] = 1;
            continue;
        }
        if (op_is_null || cmp_is_null) {
            mask[i] = (op_ne && cmp_is_null) ? 1 : 0;
            continue;
        }

        bool match = false;

        if (cd.type == ColumnType::FLOAT64) {
            double cell = cd.float_data(i);
            if (op_eq) { match = cell == cmp_float; }
            else if (op_ne) { match = cell != cmp_float; }
            else if (op_lt) { match = cell < cmp_float; }
            else if (op_le) { match = cell <= cmp_float; }
            else if (op_gt) { match = cell > cmp_float; }
            else if (op_ge) { match = cell >= cmp_float; }
        } else if (cd.type == ColumnType::INT64) {
            int64_t cell = cd.int_data[i];
            if (op_eq) { match = cell == cmp_int; }
            else if (op_ne) { match = cell != cmp_int; }
            else if (op_lt) { match = cell < cmp_int; }
            else if (op_le) { match = cell <= cmp_int; }
            else if (op_gt) { match = cell > cmp_int; }
            else if (op_ge) { match = cell >= cmp_int; }
        } else if (cd.type == ColumnType::STRING) {
            const std::string& cell = cd.str_data[i];
            if (op_eq) { match = cell == cmp_string; }
            else if (op_ne) { match = cell != cmp_string; }
            else if (op_lt) { match = cell < cmp_string; }
            else if (op_le) { match = cell <= cmp_string; }
            else if (op_gt) { match = cell > cmp_string; }
            else if (op_ge) { match = cell >= cmp_string; }
            else if (op_contains) { match = cell.find(cmp_string) != std::string::npos; }
            else if (op_startswith) { match = cell.compare(0, cmp_string.size(), cmp_string) == 0; }
            else if (op_endswith) {
                match = cell.size() >= cmp_string.size()
                    && cell.compare(cell.size() - cmp_string.size(), cmp_string.size(), cmp_string) == 0;
            }
        } else if (cd.type == ColumnType::BOOL) {
            bool cell = cd.bool_data[i];
            if (op_eq) { match = cell == cmp_bool; }
            else if (op_ne) { match = cell != cmp_bool; }
        } else if (cd.type == ColumnType::DATE && value.getType() == NT_DATE) {
                int64_t cell = cd.date_data[i];
                if (op_eq) { match = cell == cmp_date; }
                else if (op_ne) { match = cell != cmp_date; }
                else if (op_lt) { match = cell < cmp_date; }
                else if (op_le) { match = cell <= cmp_date; }
                else if (op_gt) { match = cell > cmp_date; }
                else if (op_ge) { match = cell >= cmp_date; }
        }

        if (match) {
            mask[i] = 1;
        }
    }

    return mask;
}

QoreDataFrame* QoreDataFrame::filterMask(const std::vector<uint8_t>& mask, ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "filtering DataFrame with row mask")) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx);

    if (static_cast<int64_t>(mask.size()) != n_rows) {
        xsink->raiseException("DATAFRAME-FILTER-ERROR",
            "row mask length " QLLD " does not match DataFrame row count " QLLD,
            static_cast<int64_t>(mask.size()), n_rows);
        return nullptr;
    }

    std::vector<int64_t> matching_rows;
    matching_rows.reserve(mask.size());
    for (int64_t i = 0; i < n_rows; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "filtering DataFrame with row mask")) {
            return nullptr;
        }
        if (mask[i]) {
            matching_rows.push_back(i);
        }
    }

    return selectRows(matching_rows, xsink);
}

QoreDataFrame* QoreDataFrame::filter(const std::string& column, const std::string& op,
        QoreValue value, ExceptionSink* xsink) const {
    std::vector<uint8_t> mask = compareColumnMask(column, op, value, xsink);
    if (*xsink) {
        return nullptr;
    }
    return filterMask(mask, xsink);
}

QoreDataFrame* QoreDataFrame::sortBy(const QoreListNode* col_list,
        const QoreListNode* asc_list, ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "sorting DataFrame")) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx);

    if (n_rows == 0) {
        return new QoreDataFrame();
    }

    // Resolve column indices and ascending flags
    struct SortKey {
        int col_idx;
        bool ascending;
    };
    std::vector<SortKey> keys;

    for (size_t i = 0; i < col_list->size(); ++i) {
        std::string name;
        if (!getDataFrameString(col_list->retrieveEntry(i), name)) {
            xsink->raiseException("DATAFRAME-ERROR",
                "sort column list element %zu is not a string", i);
            return nullptr;
        }
        int idx = getColIdx(name, xsink);
        if (idx < 0) {
            return nullptr;
        }
        bool asc = true;
        if (asc_list && i < asc_list->size()) {
            asc = asc_list->retrieveEntry(i).getAsBool();
        }
        keys.push_back({idx, asc});
    }

    // Build index permutation
    std::vector<int64_t> perm(n_rows);
    for (int64_t i = 0; i < n_rows; ++i) {
        perm[i] = i;
    }

    std::stable_sort(perm.begin(), perm.end(), [&](int64_t a, int64_t b) -> bool {
        for (const auto& key : keys) {
            const ColumnData& cd = *columns[key.col_idx].data;
            bool a_null = cd.isNull(a);
            bool b_null = cd.isNull(b);
            // Nulls sort last
            if (a_null && b_null) { continue; }
            if (a_null) { return false; }
            if (b_null) { return true; }

            int cmp = 0;
            switch (cd.type) {
                case ColumnType::FLOAT64: {
                    double va = cd.float_data(a), vb = cd.float_data(b);
                    cmp = (va < vb) ? -1 : (va > vb) ? 1 : 0;
                    break;
                }
                case ColumnType::INT64: {
                    int64_t va = cd.int_data[a], vb = cd.int_data[b];
                    cmp = (va < vb) ? -1 : (va > vb) ? 1 : 0;
                    break;
                }
                case ColumnType::STRING:
                    cmp = cd.str_data[a].compare(cd.str_data[b]);
                    break;
                case ColumnType::BOOL: {
                    int va = cd.bool_data[a], vb = cd.bool_data[b];
                    cmp = (va < vb) ? -1 : (va > vb) ? 1 : 0;
                    break;
                }
                case ColumnType::DATE: {
                    int64_t va = cd.date_data[a], vb = cd.date_data[b];
                    cmp = (va < vb) ? -1 : (va > vb) ? 1 : 0;
                    break;
                }
                default:
                    break;
            }
            if (cmp != 0) {
                return key.ascending ? (cmp < 0) : (cmp > 0);
            }
        }
        return false;  // equal
    });

    return selectRows(perm, xsink);
}

QoreDataFrame* QoreDataFrame::concat(const QoreListNode* dataframes, int axis,
        ExceptionSink* xsink) {
    if (!dataframes || dataframes->empty()) {
        return new QoreDataFrame();
    }
    if (axis != 0 && axis != 1) {
        xsink->raiseException("DATAFRAME-ERROR",
            "concat axis must be 0 (vertical) or 1 (horizontal), got %d", axis);
        return nullptr;
    }

    if (axis == 0) {
        // Vertical concat: stack rows, columns must match
        // Get first DataFrame's column structure
        QoreObject* first_obj = dataframes->retrieveEntry(0).get<QoreObject>();
        if (!first_obj) {
            xsink->raiseException("DATAFRAME-ERROR", "concat list element 0 is not a DataFrame");
            return nullptr;
        }
        QoreDataFrame* first_df = static_cast<QoreDataFrame*>(
            first_obj->getReferencedPrivateData(CID_DATAFRAME, xsink));
        if (!first_df) {
            xsink->raiseException("DATAFRAME-ERROR", "concat list element 0 is not a DataFrame");
            return nullptr;
        }
        // Lock first df and get column info
        std::lock_guard<std::mutex> lk(first_df->mtx);
        size_t num_cols = first_df->columns.size();
        std::vector<std::string> col_names;
        std::vector<ColumnType> col_types;
        for (size_t c = 0; c < first_df->columns.size(); ++c) {
            if (c && !(c % 100) && qore_check_cancel(xsink, "preparing DataFrame concat columns")) {
                first_df->deref(xsink);
                return nullptr;
            }
            col_names.push_back(first_df->columns[c].name);
            col_types.push_back(first_df->columns[c].data->type);
        }

        // Collect total rows and build column value lists
        int64_t total_rows = 0;
        std::vector<QoreListNode*> col_values(num_cols);
        for (size_t c = 0; c < num_cols; ++c) {
            if (c && !(c % 100) && qore_check_cancel(xsink, "initializing DataFrame concat columns")) {
                for (size_t j = 0; j < c; ++j) { col_values[j]->deref(xsink); }
                first_df->deref(xsink);
                return nullptr;
            }
            col_values[c] = new QoreListNode(autoTypeInfo);
        }

        for (size_t d = 0; d < dataframes->size(); ++d) {
            if (d && !(d % 100) && qore_check_cancel(xsink, "vertically concatenating DataFrames")) {
                for (size_t c = 0; c < num_cols; ++c) { col_values[c]->deref(xsink); }
                first_df->deref(xsink);
                return nullptr;
            }
            QoreObject* df_obj = dataframes->retrieveEntry(d).get<QoreObject>();
            if (!df_obj) {
                for (size_t c = 0; c < num_cols; ++c) { col_values[c]->deref(xsink); }
                first_df->deref(xsink);
                xsink->raiseException("DATAFRAME-ERROR",
                    "concat list element %zu is not a DataFrame", d);
                return nullptr;
            }
            QoreDataFrame* cur_df;
            if (d == 0) {
                cur_df = first_df;
                // already locked above
            } else {
                cur_df = static_cast<QoreDataFrame*>(
                    df_obj->getReferencedPrivateData(CID_DATAFRAME, xsink));
                if (!cur_df) {
                    for (size_t c = 0; c < num_cols; ++c) { col_values[c]->deref(xsink); }
                    first_df->deref(xsink);
                    xsink->raiseException("DATAFRAME-ERROR",
                        "concat list element %zu is not a DataFrame", d);
                    return nullptr;
                }
            }

            // For non-first DataFrames, we need to lock them too
            std::unique_lock<std::mutex> cur_lk(cur_df->mtx, std::defer_lock);
            if (d != 0) {
                cur_lk.lock();
            }

            for (int64_t r = 0; r < cur_df->n_rows; ++r) {
                if (r && !(r % 100) && qore_check_cancel(xsink, "vertically concatenating DataFrame rows")) {
                    for (size_t j = 0; j < num_cols; ++j) { col_values[j]->deref(xsink); }
                    if (d != 0) { cur_df->deref(xsink); }
                    first_df->deref(xsink);
                    return nullptr;
                }
                for (size_t c = 0; c < num_cols; ++c) {
                    if (c && !(c % 100) && qore_check_cancel(xsink, "vertically concatenating DataFrame columns")) {
                        for (size_t j = 0; j < num_cols; ++j) { col_values[j]->deref(xsink); }
                        if (d != 0) { cur_df->deref(xsink); }
                        first_df->deref(xsink);
                        return nullptr;
                    }
                    auto it = cur_df->col_index.find(col_names[c]);
                    if (it != cur_df->col_index.end()) {
                        QoreValue v = cur_df->columns[it->second].data->getValueAt(r, xsink);
                        if (*xsink) {
                            for (size_t j = 0; j < num_cols; ++j) { col_values[j]->deref(xsink); }
                            if (d != 0) { cur_df->deref(xsink); }
                            first_df->deref(xsink);
                            return nullptr;
                        }
                        col_values[c]->push(v, xsink);
                    } else {
                        col_values[c]->push(QoreValue(), xsink);
                    }
                }
            }
            total_rows += cur_df->n_rows;

            if (d != 0) {
                cur_df->deref(xsink);
            }
        }
        first_df->deref(xsink);

        // Build result
        auto* result = new QoreDataFrame();
        result->n_rows = total_rows;
        for (size_t c = 0; c < num_cols; ++c) {
            if (c && !(c % 100) && qore_check_cancel(xsink, "building vertically concatenated DataFrame")) {
                for (size_t j = c; j < num_cols; ++j) { col_values[j]->deref(xsink); }
                delete result;
                return nullptr;
            }
            Column col;
            col.name = col_names[c];
            col.data = buildColumnData(col_values[c], col_types[c], xsink);
            col_values[c]->deref(xsink);
            if (*xsink) {
                for (size_t j = c + 1; j < num_cols; ++j) { col_values[j]->deref(xsink); }
                delete result;
                return nullptr;
            }
            result->col_index[col.name] = result->columns.size();
            result->columns.push_back(std::move(col));
        }
        return result;
    }

    auto* result = new QoreDataFrame();
    int64_t expected_rows = -1;

    for (size_t d = 0; d < dataframes->size(); ++d) {
        if (d && !(d % 100) && qore_check_cancel(xsink, "horizontally concatenating DataFrames")) {
            delete result;
            return nullptr;
        }

        QoreObject* df_obj = dataframes->retrieveEntry(d).get<QoreObject>();
        if (!df_obj) {
            delete result;
            xsink->raiseException("DATAFRAME-ERROR",
                "concat list element %zu is not a DataFrame", d);
            return nullptr;
        }

        ReferenceHolder<QoreDataFrame> cur_df(static_cast<QoreDataFrame*>(
            df_obj->getReferencedPrivateData(CID_DATAFRAME, xsink)), xsink);
        if (!cur_df) {
            delete result;
            xsink->raiseException("DATAFRAME-ERROR",
                "concat list element %zu is not a DataFrame", d);
            return nullptr;
        }

        std::lock_guard<std::mutex> lk(cur_df->mtx);
        if (expected_rows < 0) {
            expected_rows = cur_df->n_rows;
            result->n_rows = expected_rows;
        } else if (cur_df->n_rows != expected_rows) {
            delete result;
            xsink->raiseException("DATAFRAME-ERROR",
                "concat list element %zu has " QLLD " rows, expected " QLLD,
                d, cur_df->n_rows, expected_rows);
            return nullptr;
        }

        size_t local_col_count = 0;
        for (const Column& src_col : cur_df->columns) {
            if (local_col_count && !(local_col_count % 100)
                && qore_check_cancel(xsink, "horizontally concatenating DataFrame columns")) {
                delete result;
                return nullptr;
            }

            if (result->col_index.count(src_col.name)) {
                delete result;
                xsink->raiseException("DATAFRAME-COLUMN-ERROR",
                    "horizontal concat would create duplicate column '%s'; rename or select columns before "
                    "concatenating", src_col.name.c_str());
                return nullptr;
            }

            Column col;
            col.name = src_col.name;
            col.data = src_col.data;
            result->col_index[col.name] = result->columns.size();
            result->columns.push_back(std::move(col));
            ++local_col_count;
        }
    }

    if (expected_rows < 0) {
        result->n_rows = 0;
    }
    return result;
}

QoreDataFrame* QoreDataFrame::fillna(QoreValue value, ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "filling DataFrame nulls")) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx);

    auto* df = new QoreDataFrame();
    df->n_rows = n_rows;

    for (const auto& src_col : columns) {
        // If column has no nulls, share data (COW)
        if (src_col.data->countNull() == 0) {
            Column col;
            col.name = src_col.name;
            col.data = src_col.data;
            df->col_index[col.name] = df->columns.size();
            df->columns.push_back(std::move(col));
            continue;
        }

        auto new_data = std::make_shared<ColumnData>();
        *new_data = *src_col.data;  // copy column with nulls
        // Clear null_mask and fill with the replacement value
        for (int64_t i = 0; i < n_rows; ++i) {
            if (new_data->null_mask[i]) {
                new_data->null_mask[i] = 0;
                switch (new_data->type) {
                    case ColumnType::FLOAT64:
                        new_data->float_data(i) = value.getAsFloat();
                        break;
                    case ColumnType::INT64:
                        new_data->int_data[i] = value.getAsBigInt();
                        break;
                    case ColumnType::STRING: {
                        QoreStringValueHelper sh(value);
                        new_data->str_data[i] = sh->c_str();
                        break;
                    }
                    case ColumnType::BOOL:
                        new_data->bool_data[i] = value.getAsBool() ? 1 : 0;
                        break;
                    case ColumnType::AUTO:
                        new_data->setAutoValue(i, value, xsink);
                        break;
                    default:
                        break;
                }
            }
        }

        Column col;
        col.name = src_col.name;
        col.data = std::move(new_data);
        df->col_index[col.name] = df->columns.size();
        df->columns.push_back(std::move(col));
    }

    return df;
}

QoreDataFrame* QoreDataFrame::dropna(ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "dropping DataFrame null rows")) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx);

    std::vector<int64_t> keep_rows;
    for (int64_t i = 0; i < n_rows; ++i) {
        bool has_null = false;
        for (const auto& col : columns) {
            if (col.data->isNull(i)) {
                has_null = true;
                break;
            }
        }
        if (!has_null) {
            keep_rows.push_back(i);
        }
    }

    return selectRows(keep_rows, xsink);
}

QoreDataFrame* QoreDataFrame::valueCounts(const std::string& column, bool dropna,
        bool sort_desc, const std::string& count_name, ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "counting DataFrame column values")) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx);

    int idx = getColIdx(column, xsink);
    if (idx < 0) {
        return nullptr;
    }

    const ColumnData& src = *columns[idx].data;
    const std::string out_count_name = count_name == column ? "value_count" : count_name;

    if (src.type == ColumnType::INT64) {
        struct IntCountEntry {
            int64_t value;
            bool is_null;
            int64_t count;
        };
        std::unordered_map<int64_t, size_t> value_map;
        value_map.reserve(static_cast<size_t>(std::min<int64_t>(n_rows, 65536)));
        std::vector<IntCountEntry> entries;
        int64_t null_entry = -1;
        for (int64_t row = 0; row < n_rows; ++row) {
            if (row && !(row % 100) && qore_check_cancel(xsink, "counting integer DataFrame values")) {
                return nullptr;
            }
            if (src.isNull(row)) {
                if (dropna) {
                    continue;
                }
                if (null_entry < 0) {
                    null_entry = static_cast<int64_t>(entries.size());
                    entries.push_back({0, true, 1});
                } else {
                    ++entries[null_entry].count;
                }
                continue;
            }
            int64_t value = src.int_data[row];
            auto it = value_map.find(value);
            if (it == value_map.end()) {
                value_map.emplace(value, entries.size());
                entries.push_back({value, false, 1});
            } else {
                ++entries[it->second].count;
            }
        }

        std::vector<size_t> order(entries.size());
        for (size_t i = 0; i < entries.size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "ordering integer DataFrame value counts")) {
                return nullptr;
            }
            order[i] = i;
        }
        if (sort_desc) {
            std::stable_sort(order.begin(), order.end(), [&entries](size_t a, size_t b) {
                return entries[a].count > entries[b].count;
            });
        }

        auto* df = new QoreDataFrame();
        df->n_rows = static_cast<int64_t>(entries.size());
        auto value_cd = std::make_shared<ColumnData>();
        value_cd->type = ColumnType::INT64;
        value_cd->n_rows = df->n_rows;
        value_cd->null_mask.resize(df->n_rows, 0);
        value_cd->int_data.resize(df->n_rows);
        auto count_cd = std::make_shared<ColumnData>();
        count_cd->type = ColumnType::INT64;
        count_cd->n_rows = df->n_rows;
        count_cd->null_mask.resize(df->n_rows, 0);
        count_cd->int_data.resize(df->n_rows);
        for (int64_t out = 0; out < df->n_rows; ++out) {
            if (out && !(out % 100) && qore_check_cancel(xsink, "building integer DataFrame value counts")) {
                delete df;
                return nullptr;
            }
            const IntCountEntry& entry = entries[order[out]];
            value_cd->null_mask[out] = entry.is_null ? 1 : 0;
            value_cd->int_data[out] = entry.value;
            count_cd->int_data[out] = entry.count;
        }
        Column value_col;
        value_col.name = column;
        value_col.data = std::move(value_cd);
        df->col_index[value_col.name] = df->columns.size();
        df->columns.push_back(std::move(value_col));
        Column count_col;
        count_col.name = out_count_name;
        count_col.data = std::move(count_cd);
        df->col_index[count_col.name] = df->columns.size();
        df->columns.push_back(std::move(count_col));
        return df;
    }

    if (src.type == ColumnType::STRING) {
        struct StringCountEntry {
            std::string value;
            bool is_null;
            int64_t count;
        };
        std::unordered_map<std::string, size_t> value_map;
        value_map.reserve(static_cast<size_t>(std::min<int64_t>(n_rows, 65536)));
        std::vector<StringCountEntry> entries;
        int64_t null_entry = -1;
        for (int64_t row = 0; row < n_rows; ++row) {
            if (row && !(row % 100) && qore_check_cancel(xsink, "counting string DataFrame values")) {
                return nullptr;
            }
            if (src.isNull(row)) {
                if (dropna) {
                    continue;
                }
                if (null_entry < 0) {
                    null_entry = static_cast<int64_t>(entries.size());
                    entries.push_back({"", true, 1});
                } else {
                    ++entries[null_entry].count;
                }
                continue;
            }
            const std::string& value = src.str_data[row];
            auto it = value_map.find(value);
            if (it == value_map.end()) {
                value_map.emplace(value, entries.size());
                entries.push_back({value, false, 1});
            } else {
                ++entries[it->second].count;
            }
        }

        std::vector<size_t> order(entries.size());
        for (size_t i = 0; i < entries.size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "ordering string DataFrame value counts")) {
                return nullptr;
            }
            order[i] = i;
        }
        if (sort_desc) {
            std::stable_sort(order.begin(), order.end(), [&entries](size_t a, size_t b) {
                return entries[a].count > entries[b].count;
            });
        }

        auto* df = new QoreDataFrame();
        df->n_rows = static_cast<int64_t>(entries.size());
        auto value_cd = std::make_shared<ColumnData>();
        value_cd->type = ColumnType::STRING;
        value_cd->n_rows = df->n_rows;
        value_cd->null_mask.resize(df->n_rows, 0);
        value_cd->str_data.resize(df->n_rows);
        auto count_cd = std::make_shared<ColumnData>();
        count_cd->type = ColumnType::INT64;
        count_cd->n_rows = df->n_rows;
        count_cd->null_mask.resize(df->n_rows, 0);
        count_cd->int_data.resize(df->n_rows);
        for (int64_t out = 0; out < df->n_rows; ++out) {
            if (out && !(out % 100) && qore_check_cancel(xsink, "building string DataFrame value counts")) {
                delete df;
                return nullptr;
            }
            const StringCountEntry& entry = entries[order[out]];
            value_cd->null_mask[out] = entry.is_null ? 1 : 0;
            value_cd->str_data[out] = entry.value;
            count_cd->int_data[out] = entry.count;
        }
        Column value_col;
        value_col.name = column;
        value_col.data = std::move(value_cd);
        df->col_index[value_col.name] = df->columns.size();
        df->columns.push_back(std::move(value_col));
        Column count_col;
        count_col.name = out_count_name;
        count_col.data = std::move(count_cd);
        df->col_index[count_col.name] = df->columns.size();
        df->columns.push_back(std::move(count_col));
        return df;
    }

    if (src.type == ColumnType::BOOL) {
        struct BoolCountEntry {
            bool value;
            bool is_null;
            int64_t count;
        };
        std::vector<BoolCountEntry> entries;
        int64_t false_entry = -1;
        int64_t true_entry = -1;
        int64_t null_entry = -1;
        for (int64_t row = 0; row < n_rows; ++row) {
            if (row && !(row % 100) && qore_check_cancel(xsink, "counting bool DataFrame values")) {
                return nullptr;
            }
            if (src.isNull(row)) {
                if (dropna) {
                    continue;
                }
                if (null_entry < 0) {
                    null_entry = static_cast<int64_t>(entries.size());
                    entries.push_back({false, true, 1});
                } else {
                    ++entries[null_entry].count;
                }
            } else if (src.bool_data[row]) {
                if (true_entry < 0) {
                    true_entry = static_cast<int64_t>(entries.size());
                    entries.push_back({true, false, 1});
                } else {
                    ++entries[true_entry].count;
                }
            } else {
                if (false_entry < 0) {
                    false_entry = static_cast<int64_t>(entries.size());
                    entries.push_back({false, false, 1});
                } else {
                    ++entries[false_entry].count;
                }
            }
        }

        if (sort_desc) {
            std::stable_sort(entries.begin(), entries.end(), [](const BoolCountEntry& a, const BoolCountEntry& b) {
                return a.count > b.count;
            });
        }

        auto* df = new QoreDataFrame();
        df->n_rows = static_cast<int64_t>(entries.size());
        auto value_cd = std::make_shared<ColumnData>();
        value_cd->type = ColumnType::BOOL;
        value_cd->n_rows = df->n_rows;
        value_cd->null_mask.resize(df->n_rows, 0);
        value_cd->bool_data.resize(df->n_rows);
        auto count_cd = std::make_shared<ColumnData>();
        count_cd->type = ColumnType::INT64;
        count_cd->n_rows = df->n_rows;
        count_cd->null_mask.resize(df->n_rows, 0);
        count_cd->int_data.resize(df->n_rows);
        for (int64_t out = 0; out < df->n_rows; ++out) {
            const BoolCountEntry& entry = entries[out];
            value_cd->null_mask[out] = entry.is_null ? 1 : 0;
            value_cd->bool_data[out] = entry.value ? 1 : 0;
            count_cd->int_data[out] = entry.count;
        }
        Column value_col;
        value_col.name = column;
        value_col.data = std::move(value_cd);
        df->col_index[value_col.name] = df->columns.size();
        df->columns.push_back(std::move(value_col));
        Column count_col;
        count_col.name = out_count_name;
        count_col.data = std::move(count_cd);
        df->col_index[count_col.name] = df->columns.size();
        df->columns.push_back(std::move(count_col));
        return df;
    }

    struct CountEntry {
        int64_t first_row;
        int64_t count;
    };
    std::unordered_map<std::string, size_t> entry_map;
    entry_map.reserve(static_cast<size_t>(std::min<int64_t>(n_rows, 65536)));
    std::vector<CountEntry> entries;

    for (int64_t row = 0; row < n_rows; ++row) {
        if (row && !(row % 100) && qore_check_cancel(xsink, "counting DataFrame column values")) {
            return nullptr;
        }
        if (dropna && src.isNull(row)) {
            continue;
        }

        std::string key = dataFrameValueKey(src, row);
        auto it = entry_map.find(key);
        if (it == entry_map.end()) {
            entry_map.emplace(std::move(key), entries.size());
            entries.push_back({row, 1});
        } else {
            ++entries[it->second].count;
        }
    }

    std::vector<size_t> order(entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "ordering DataFrame value counts")) {
            return nullptr;
        }
        order[i] = i;
    }
    if (sort_desc) {
        std::stable_sort(order.begin(), order.end(), [&entries](size_t a, size_t b) {
            return entries[a].count > entries[b].count;
        });
    }

    auto* df = new QoreDataFrame();
    df->n_rows = static_cast<int64_t>(entries.size());

    auto value_cd = std::make_shared<ColumnData>();
    value_cd->type = src.type;
    value_cd->n_rows = df->n_rows;
    value_cd->null_mask.resize(df->n_rows, 0);
    switch (src.type) {
        case ColumnType::FLOAT64:
            value_cd->float_data.resize(df->n_rows);
            break;
        case ColumnType::INT64:
            value_cd->int_data.resize(df->n_rows);
            break;
        case ColumnType::STRING:
            value_cd->str_data.resize(df->n_rows);
            break;
        case ColumnType::BOOL:
            value_cd->bool_data.resize(df->n_rows);
            break;
        case ColumnType::DATE:
            value_cd->date_data.resize(df->n_rows);
            break;
        case ColumnType::AUTO:
            value_cd->auto_data.resize(df->n_rows);
            break;
        default:
            break;
    }

    auto count_cd = std::make_shared<ColumnData>();
    count_cd->type = ColumnType::INT64;
    count_cd->n_rows = df->n_rows;
    count_cd->null_mask.resize(df->n_rows, 0);
    count_cd->int_data.resize(df->n_rows);

    for (int64_t out = 0; out < df->n_rows; ++out) {
        if (out && !(out % 100) && qore_check_cancel(xsink, "building DataFrame value counts")) {
            delete df;
            return nullptr;
        }
        const CountEntry& entry = entries[order[out]];
        int64_t row = entry.first_row;
        value_cd->null_mask[out] = src.null_mask[row];
        if (!value_cd->null_mask[out]) {
            switch (src.type) {
                case ColumnType::FLOAT64:
                    value_cd->float_data(out) = src.float_data(row);
                    break;
                case ColumnType::INT64:
                    value_cd->int_data[out] = src.int_data[row];
                    break;
                case ColumnType::STRING:
                    value_cd->str_data[out] = src.str_data[row];
                    break;
                case ColumnType::BOOL:
                    value_cd->bool_data[out] = src.bool_data[row];
                    break;
                case ColumnType::DATE:
                    value_cd->date_data[out] = src.date_data[row];
                    break;
                case ColumnType::AUTO:
                    value_cd->setAutoValue(out, src.auto_data[row], xsink);
                    if (*xsink) {
                        delete df;
                        return nullptr;
                    }
                    break;
                default:
                    break;
            }
        }
        count_cd->int_data[out] = entry.count;
    }

    Column value_col;
    value_col.name = column;
    value_col.data = std::move(value_cd);
    df->col_index[value_col.name] = df->columns.size();
    df->columns.push_back(std::move(value_col));

    Column count_col;
    count_col.name = out_count_name;
    count_col.data = std::move(count_cd);
    df->col_index[count_col.name] = df->columns.size();
    df->columns.push_back(std::move(count_col));

    return df;
}

int64_t QoreDataFrame::nunique(const std::string& column, bool dropna,
        ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "counting unique DataFrame values")) {
        return 0;
    }
    std::lock_guard<std::mutex> lk(mtx);

    int idx = getColIdx(column, xsink);
    if (idx < 0) {
        return 0;
    }

    const ColumnData& src = *columns[idx].data;
    if (src.type == ColumnType::INT64) {
        std::unordered_set<int64_t> seen;
        seen.reserve(static_cast<size_t>(std::min<int64_t>(n_rows, 65536)));
        bool seen_null = false;
        for (int64_t row = 0; row < n_rows; ++row) {
            if (row && !(row % 100) && qore_check_cancel(xsink, "counting unique integer DataFrame values")) {
                return 0;
            }
            if (src.isNull(row)) {
                if (!dropna) {
                    seen_null = true;
                }
            } else {
                seen.insert(src.int_data[row]);
            }
        }
        return static_cast<int64_t>(seen.size()) + (seen_null ? 1 : 0);
    }
    if (src.type == ColumnType::STRING) {
        std::unordered_set<std::string> seen;
        seen.reserve(static_cast<size_t>(std::min<int64_t>(n_rows, 65536)));
        bool seen_null = false;
        for (int64_t row = 0; row < n_rows; ++row) {
            if (row && !(row % 100) && qore_check_cancel(xsink, "counting unique string DataFrame values")) {
                return 0;
            }
            if (src.isNull(row)) {
                if (!dropna) {
                    seen_null = true;
                }
            } else {
                seen.insert(src.str_data[row]);
            }
        }
        return static_cast<int64_t>(seen.size()) + (seen_null ? 1 : 0);
    }
    if (src.type == ColumnType::BOOL) {
        bool seen_false = false;
        bool seen_true = false;
        bool seen_null = false;
        for (int64_t row = 0; row < n_rows; ++row) {
            if (row && !(row % 100) && qore_check_cancel(xsink, "counting unique bool DataFrame values")) {
                return 0;
            }
            if (src.isNull(row)) {
                if (!dropna) {
                    seen_null = true;
                }
            } else if (src.bool_data[row]) {
                seen_true = true;
            } else {
                seen_false = true;
            }
        }
        return (seen_false ? 1 : 0) + (seen_true ? 1 : 0) + (seen_null ? 1 : 0);
    }

    std::unordered_set<std::string> seen;
    seen.reserve(static_cast<size_t>(std::min<int64_t>(n_rows, 65536)));
    for (int64_t row = 0; row < n_rows; ++row) {
        if (row && !(row % 100) && qore_check_cancel(xsink, "counting unique DataFrame values")) {
            return 0;
        }
        if (dropna && src.isNull(row)) {
            continue;
        }
        seen.insert(dataFrameValueKey(src, row));
    }
    return static_cast<int64_t>(seen.size());
}

// --- Mutation ---

void QoreDataFrame::addColumn(const std::string& name, const QoreListNode* data,
        ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (col_index.count(name)) {
        xsink->raiseException("DATAFRAME-COLUMN-ERROR",
            "column '%s' already exists", name.c_str());
        return;
    }

    int64_t col_rows = data ? (int64_t)data->size() : 0;
    if (n_rows > 0 && col_rows != n_rows) {
        xsink->raiseException("DATAFRAME-COLUMN-ERROR",
            "column '%s' has " QLLD " rows, expected " QLLD,
            name.c_str(), col_rows, n_rows);
        return;
    }

    Column col;
    col.name = name;
    col.data = buildColumnDataAuto(data, xsink);
    if (*xsink) {
        return;
    }

    if (n_rows == 0) {
        n_rows = col_rows;
    }

    col_index[name] = columns.size();
    columns.push_back(std::move(col));
}

void QoreDataFrame::dropColumn(const std::string& name, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    int idx = getColIdx(name, xsink);
    if (idx < 0) {
        return;
    }

    columns.erase(columns.begin() + idx);
    rebuildIndex();
}

void QoreDataFrame::renameColumn(const std::string& old_name,
        const std::string& new_name, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    int idx = getColIdx(old_name, xsink);
    if (idx < 0) {
        return;
    }

    if (col_index.count(new_name)) {
        xsink->raiseException("DATAFRAME-COLUMN-ERROR",
            "column '%s' already exists", new_name.c_str());
        return;
    }

    columns[idx].name = new_name;
    rebuildIndex();
}

void QoreDataFrame::rebuildIndex() {
    col_index.clear();
    for (size_t i = 0; i < columns.size(); ++i) {
        col_index[columns[i].name] = i;
    }
}

} // namespace QoreDataFrameNS
