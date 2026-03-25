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
#include <sstream>
#include <unordered_set>

namespace QoreDataFrameNS {

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
    ConstHashIterator hi(first);
    while (hi.next()) {
        col_names.push_back(hi.getKey());
    }

    // Build per-column value lists
    int64_t n = (int64_t)records->size();
    size_t num_cols = col_names.size();
    std::vector<QoreListNode*> col_values(num_cols);
    for (size_t c = 0; c < num_cols; ++c) {
        col_values[c] = new QoreListNode(autoTypeInfo);
    }

    for (int64_t i = 0; i < n; ++i) {
        const QoreHashNode* row = records->retrieveEntry(i).get<const QoreHashNode>();
        if (!row) {
            for (size_t c = 0; c < num_cols; ++c) {
                col_values[c]->deref(xsink);
            }
            xsink->raiseException("DATAFRAME-ERROR",
                "expected hash at index " QLLD ", got non-hash", i);
            return nullptr;
        }
        for (size_t c = 0; c < num_cols; ++c) {
            QoreValue v = row->getKeyValue(col_names[c].c_str());
            col_values[c]->push(v.refSelf(), xsink);
        }
    }

    // Build DataFrame
    auto* df = new QoreDataFrame();
    df->n_rows = n;
    for (size_t c = 0; c < num_cols; ++c) {
        Column col;
        col.name = col_names[c];
        col.data = buildColumnDataAuto(col_values[c], xsink);
        col_values[c]->deref(xsink);
        if (*xsink) {
            // Clean up remaining lists
            for (size_t j = c + 1; j < num_cols; ++j) {
                col_values[j]->deref(xsink);
            }
            delete df;
            return nullptr;
        }
        df->col_index[col.name] = df->columns.size();
        df->columns.push_back(std::move(col));
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
        const QoreListNode* values = hi.get().get<const QoreListNode>();
        if (!values) {
            xsink->raiseException("DATAFRAME-ERROR",
                "column '%s' value must be a list", hi.getKey());
            delete df;
            return nullptr;
        }

        int64_t col_rows = (int64_t)values->size();
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
        col.data = buildColumnDataAuto(values, xsink);
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
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
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
        auto new_data = std::make_shared<ColumnData>();
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
            default:
                break;
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

QoreListNode* QoreDataFrame::describe(ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "computing DataFrame statistics")) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx);
    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);

    for (const auto& col : columns) {
        ReferenceHolder<QoreHashNode> stats(new QoreHashNode(autoTypeInfo), xsink);
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

// --- Query Operations ---

QoreDataFrame* QoreDataFrame::selectRows(const std::vector<int64_t>& indices,
        ExceptionSink* xsink) const {
    // Caller must hold lock
    auto* df = new QoreDataFrame();
    int64_t count = (int64_t)indices.size();
    df->n_rows = count;

    for (const auto& src_col : columns) {
        auto new_data = std::make_shared<ColumnData>();
        new_data->type = src_col.data->type;
        new_data->n_rows = count;
        new_data->null_mask.resize(count);

        switch (src_col.data->type) {
            case ColumnType::FLOAT64: {
                new_data->float_data.resize(count);
                for (int64_t i = 0; i < count; ++i) {
                    new_data->null_mask[i] = src_col.data->null_mask[indices[i]];
                    new_data->float_data(i) = src_col.data->float_data(indices[i]);
                }
                break;
            }
            case ColumnType::INT64: {
                new_data->int_data.resize(count);
                for (int64_t i = 0; i < count; ++i) {
                    new_data->null_mask[i] = src_col.data->null_mask[indices[i]];
                    new_data->int_data[i] = src_col.data->int_data[indices[i]];
                }
                break;
            }
            case ColumnType::STRING: {
                new_data->str_data.resize(count);
                for (int64_t i = 0; i < count; ++i) {
                    new_data->null_mask[i] = src_col.data->null_mask[indices[i]];
                    new_data->str_data[i] = src_col.data->str_data[indices[i]];
                }
                break;
            }
            case ColumnType::BOOL: {
                new_data->bool_data.resize(count);
                for (int64_t i = 0; i < count; ++i) {
                    new_data->null_mask[i] = src_col.data->null_mask[indices[i]];
                    new_data->bool_data[i] = src_col.data->bool_data[indices[i]];
                }
                break;
            }
            case ColumnType::DATE: {
                new_data->date_data.resize(count);
                for (int64_t i = 0; i < count; ++i) {
                    new_data->null_mask[i] = src_col.data->null_mask[indices[i]];
                    new_data->date_data[i] = src_col.data->date_data[indices[i]];
                }
                break;
            }
            default:
                break;
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
    std::lock_guard<std::mutex> lk(mtx);

    auto* df = new QoreDataFrame();
    df->n_rows = n_rows;

    for (size_t i = 0; i < col_list->size(); ++i) {
        const QoreStringNode* name_node = col_list->retrieveEntry(i).get<const QoreStringNode>();
        if (!name_node) {
            xsink->raiseException("DATAFRAME-ERROR",
                "column list element %zu is not a string", i);
            delete df;
            return nullptr;
        }
        std::string name = name_node->c_str();
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

QoreDataFrame* QoreDataFrame::filter(const std::string& column, const std::string& op,
        QoreValue value, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    int col_idx = getColIdx(column, xsink);
    if (col_idx < 0) {
        return nullptr;
    }

    const ColumnData& cd = *columns[col_idx].data;
    std::vector<int64_t> matching_rows;

    for (int64_t i = 0; i < n_rows; ++i) {
        if (cd.isNull(i)) {
            // null comparison: only is_null/not_null match
            if (op == "is_null") {
                matching_rows.push_back(i);
            }
            continue;
        }

        if (op == "not_null") {
            matching_rows.push_back(i);
            continue;
        }
        if (op == "is_null") {
            continue;
        }

        bool match = false;

        if (cd.type == ColumnType::FLOAT64) {
            double cell = cd.float_data(i);
            double cmp = value.getAsFloat();
            if (op == "==") { match = cell == cmp; }
            else if (op == "!=") { match = cell != cmp; }
            else if (op == "<") { match = cell < cmp; }
            else if (op == "<=") { match = cell <= cmp; }
            else if (op == ">") { match = cell > cmp; }
            else if (op == ">=") { match = cell >= cmp; }
        } else if (cd.type == ColumnType::INT64) {
            int64_t cell = cd.int_data[i];
            int64_t cmp = value.getAsBigInt();
            if (op == "==") { match = cell == cmp; }
            else if (op == "!=") { match = cell != cmp; }
            else if (op == "<") { match = cell < cmp; }
            else if (op == "<=") { match = cell <= cmp; }
            else if (op == ">") { match = cell > cmp; }
            else if (op == ">=") { match = cell >= cmp; }
        } else if (cd.type == ColumnType::STRING) {
            const std::string& cell = cd.str_data[i];
            std::string cmp;
            if (value.getType() == NT_STRING) {
                cmp = value.get<const QoreStringNode>()->c_str();
            } else {
                QoreStringValueHelper sh(value);
                cmp = sh->c_str();
            }
            if (op == "==") { match = cell == cmp; }
            else if (op == "!=") { match = cell != cmp; }
            else if (op == "<") { match = cell < cmp; }
            else if (op == "<=") { match = cell <= cmp; }
            else if (op == ">") { match = cell > cmp; }
            else if (op == ">=") { match = cell >= cmp; }
            else if (op == "contains") { match = cell.find(cmp) != std::string::npos; }
            else if (op == "startswith") { match = cell.compare(0, cmp.size(), cmp) == 0; }
            else if (op == "endswith") {
                match = cell.size() >= cmp.size()
                    && cell.compare(cell.size() - cmp.size(), cmp.size(), cmp) == 0;
            }
        } else if (cd.type == ColumnType::BOOL) {
            bool cell = cd.bool_data[i];
            bool cmp = value.getAsBool();
            if (op == "==") { match = cell == cmp; }
            else if (op == "!=") { match = cell != cmp; }
        }

        if (match) {
            matching_rows.push_back(i);
        }
    }

    return selectRows(matching_rows, xsink);
}

QoreDataFrame* QoreDataFrame::sortBy(const QoreListNode* col_list,
        const QoreListNode* asc_list, ExceptionSink* xsink) const {
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
        const QoreStringNode* name = col_list->retrieveEntry(i).get<const QoreStringNode>();
        if (!name) {
            xsink->raiseException("DATAFRAME-ERROR",
                "sort column list element %zu is not a string", i);
            return nullptr;
        }
        int idx = getColIdx(name->c_str(), xsink);
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
        for (const auto& c : first_df->columns) {
            col_names.push_back(c.name);
            col_types.push_back(c.data->type);
        }

        // Collect total rows and build column value lists
        int64_t total_rows = 0;
        std::vector<QoreListNode*> col_values(num_cols);
        for (size_t c = 0; c < num_cols; ++c) {
            col_values[c] = new QoreListNode(autoTypeInfo);
        }

        for (size_t d = 0; d < dataframes->size(); ++d) {
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
                for (size_t c = 0; c < num_cols; ++c) {
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

    // axis == 1: horizontal concat (not yet implemented)
    xsink->raiseException("DATAFRAME-ERROR", "horizontal concat (axis=1) not yet implemented");
    return nullptr;
}

QoreDataFrame* QoreDataFrame::fillna(QoreValue value, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    auto* df = new QoreDataFrame();
    df->n_rows = n_rows;

    for (const auto& src_col : columns) {
        auto new_data = std::make_shared<ColumnData>();
        *new_data = *src_col.data;  // copy all data
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
