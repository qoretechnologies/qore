/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    df_window.cpp

    DataFrame pivot, melt, withColumn, and window function implementations

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_DataFrame.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace QoreDataFrameNS {

// --- Helper: parse WindowSpec from hash ---
struct WindowSpec {
    std::vector<std::string> partition_by;
    std::vector<std::string> order_by;
    std::vector<bool> ascending;
};

static WindowSpec parseWindowSpec(const QoreHashNode* spec) {
    WindowSpec ws;
    if (!spec) {
        return ws;
    }
    QoreValue v = spec->getKeyValue("partition_by");
    if (v.getType() == NT_LIST) {
        const QoreListNode* list = v.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            const QoreStringNode* s = list->retrieveEntry(i).get<const QoreStringNode>();
            if (s) {
                ws.partition_by.push_back(s->c_str());
            }
        }
    }
    v = spec->getKeyValue("order_by");
    if (v.getType() == NT_LIST) {
        const QoreListNode* list = v.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            const QoreStringNode* s = list->retrieveEntry(i).get<const QoreStringNode>();
            if (s) {
                ws.order_by.push_back(s->c_str());
            }
        }
    }
    v = spec->getKeyValue("ascending");
    if (v.getType() == NT_LIST) {
        const QoreListNode* list = v.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            ws.ascending.push_back(list->retrieveEntry(i).getAsBool());
        }
    }
    return ws;
}

// Helper: partition rows by the partition_by columns, then sort within each partition
// Returns a vector of row indices in window order (all partitions concatenated)
// and a mapping from original row index to output position
static std::vector<int64_t> computeWindowOrder(
        const std::vector<Column>& columns,
        const std::unordered_map<std::string, size_t>& col_index,
        int64_t n_rows,
        const WindowSpec& ws,
        std::vector<int64_t>& original_to_output) {
    // Start with all row indices
    std::vector<int64_t> indices(n_rows);
    for (int64_t i = 0; i < n_rows; ++i) {
        indices[i] = i;
    }

    // Partition: group by partition_by columns
    // For simplicity, sort by (partition_key, order_key) so partitions are contiguous
    std::vector<size_t> part_col_idx;
    for (const auto& pc : ws.partition_by) {
        auto it = col_index.find(pc);
        if (it != col_index.end()) {
            part_col_idx.push_back(it->second);
        }
    }
    std::vector<size_t> order_col_idx;
    for (const auto& oc : ws.order_by) {
        auto it = col_index.find(oc);
        if (it != col_index.end()) {
            order_col_idx.push_back(it->second);
        }
    }

    // Sort indices by partition columns then order columns
    std::stable_sort(indices.begin(), indices.end(), [&](int64_t a, int64_t b) -> bool {
        // Compare partition columns first
        for (size_t ci : part_col_idx) {
            const ColumnData& cd = *columns[ci].data;
            int cmp = 0;
            switch (cd.type) {
                case ColumnType::INT64:
                    cmp = (cd.int_data[a] < cd.int_data[b]) ? -1
                        : (cd.int_data[a] > cd.int_data[b]) ? 1 : 0;
                    break;
                case ColumnType::STRING:
                    cmp = cd.str_data[a].compare(cd.str_data[b]);
                    break;
                case ColumnType::FLOAT64:
                    cmp = (cd.float_data(a) < cd.float_data(b)) ? -1
                        : (cd.float_data(a) > cd.float_data(b)) ? 1 : 0;
                    break;
                default:
                    break;
            }
            if (cmp != 0) {
                return cmp < 0;
            }
        }
        // Then order columns
        for (size_t oi = 0; oi < order_col_idx.size(); ++oi) {
            size_t ci = order_col_idx[oi];
            const ColumnData& cd = *columns[ci].data;
            bool asc = (oi < ws.ascending.size()) ? ws.ascending[oi] : true;
            int cmp = 0;
            switch (cd.type) {
                case ColumnType::INT64:
                    cmp = (cd.int_data[a] < cd.int_data[b]) ? -1
                        : (cd.int_data[a] > cd.int_data[b]) ? 1 : 0;
                    break;
                case ColumnType::FLOAT64:
                    cmp = (cd.float_data(a) < cd.float_data(b)) ? -1
                        : (cd.float_data(a) > cd.float_data(b)) ? 1 : 0;
                    break;
                case ColumnType::STRING:
                    cmp = cd.str_data[a].compare(cd.str_data[b]);
                    break;
                default:
                    break;
            }
            if (cmp != 0) {
                return asc ? (cmp < 0) : (cmp > 0);
            }
        }
        return false;
    });

    // Build reverse mapping: original row → position in sorted order
    original_to_output.resize(n_rows);
    for (int64_t i = 0; i < n_rows; ++i) {
        original_to_output[indices[i]] = i;
    }

    return indices;
}

// Check if two rows are in the same partition
static bool samePartition(const std::vector<Column>& columns,
        const std::vector<size_t>& part_col_idx, int64_t a, int64_t b) {
    for (size_t ci : part_col_idx) {
        const ColumnData& cd = *columns[ci].data;
        switch (cd.type) {
            case ColumnType::INT64:
                if (cd.int_data[a] != cd.int_data[b]) { return false; }
                break;
            case ColumnType::STRING:
                if (cd.str_data[a] != cd.str_data[b]) { return false; }
                break;
            case ColumnType::FLOAT64:
                if (cd.float_data(a) != cd.float_data(b)) { return false; }
                break;
            default:
                break;
        }
    }
    return true;
}

// --- withColumn ---

QoreDataFrame* QoreDataFrame::withColumn(const std::string& name,
        const QoreListNode* data, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (data && (int64_t)data->size() != n_rows) {
        xsink->raiseException("DATAFRAME-COLUMN-ERROR",
            "withColumn data has " QLLD " elements, expected " QLLD,
            (int64_t)data->size(), n_rows);
        return nullptr;
    }

    auto* df = new QoreDataFrame();
    df->n_rows = n_rows;

    // Copy all existing columns (shared_ptr — COW)
    for (const auto& col : columns) {
        if (col.name == name) {
            continue;  // skip if replacing
        }
        df->col_index[col.name] = df->columns.size();
        df->columns.push_back(col);
    }

    // Add the new/replacement column
    Column new_col;
    new_col.name = name;
    new_col.data = buildColumnDataAuto(data, xsink);
    if (*xsink) {
        delete df;
        return nullptr;
    }
    df->col_index[name] = df->columns.size();
    df->columns.push_back(std::move(new_col));

    return df;
}

// --- pivot ---

QoreDataFrame* QoreDataFrame::pivot(const std::string& index_col,
        const std::string& columns_col, const std::string& values_col,
        const std::string& agg_func, ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "pivoting DataFrame")) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx);

    int idx_ci = getColIdx(index_col, xsink);
    if (idx_ci < 0) { return nullptr; }
    int col_ci = getColIdx(columns_col, xsink);
    if (col_ci < 0) { return nullptr; }
    int val_ci = getColIdx(values_col, xsink);
    if (val_ci < 0) { return nullptr; }

    const ColumnData& idx_cd = *columns[idx_ci].data;
    const ColumnData& col_cd = *columns[col_ci].data;
    const ColumnData& val_cd = *columns[val_ci].data;

    // Collect unique index values and column values
    std::vector<std::string> unique_indices;
    std::unordered_map<std::string, size_t> index_map;
    std::vector<std::string> unique_cols;
    std::unordered_map<std::string, size_t> col_map;

    for (int64_t i = 0; i < n_rows; ++i) {
        std::string idx_val;
        if (!idx_cd.isNull(i)) {
            switch (idx_cd.type) {
                case ColumnType::STRING: idx_val = idx_cd.str_data[i]; break;
                case ColumnType::INT64: idx_val = std::to_string(idx_cd.int_data[i]); break;
                default: idx_val = std::to_string(i); break;
            }
        }
        if (!index_map.count(idx_val)) {
            index_map[idx_val] = unique_indices.size();
            unique_indices.push_back(idx_val);
        }

        std::string col_val;
        if (!col_cd.isNull(i)) {
            switch (col_cd.type) {
                case ColumnType::STRING: col_val = col_cd.str_data[i]; break;
                case ColumnType::INT64: col_val = std::to_string(col_cd.int_data[i]); break;
                default: col_val = std::to_string(i); break;
            }
        }
        if (!col_map.count(col_val)) {
            col_map[col_val] = unique_cols.size();
            unique_cols.push_back(col_val);
        }
    }

    // Build result: one row per unique index, one column per unique column value
    int64_t out_rows = (int64_t)unique_indices.size();
    int64_t out_cols = (int64_t)unique_cols.size();

    // Initialize accumulation grid
    std::vector<std::vector<double>> sums(out_rows, std::vector<double>(out_cols, 0.0));
    std::vector<std::vector<int>> counts(out_rows, std::vector<int>(out_cols, 0));

    for (int64_t i = 0; i < n_rows; ++i) {
        std::string idx_val;
        if (!idx_cd.isNull(i)) {
            switch (idx_cd.type) {
                case ColumnType::STRING: idx_val = idx_cd.str_data[i]; break;
                case ColumnType::INT64: idx_val = std::to_string(idx_cd.int_data[i]); break;
                default: break;
            }
        }
        std::string col_val;
        if (!col_cd.isNull(i)) {
            switch (col_cd.type) {
                case ColumnType::STRING: col_val = col_cd.str_data[i]; break;
                case ColumnType::INT64: col_val = std::to_string(col_cd.int_data[i]); break;
                default: break;
            }
        }

        size_t ri = index_map[idx_val];
        size_t ci = col_map[col_val];

        double val = 0;
        if (!val_cd.isNull(i)) {
            val = (val_cd.type == ColumnType::FLOAT64) ? val_cd.float_data(i)
                : (double)val_cd.int_data[i];
        }
        sums[ri][ci] += val;
        counts[ri][ci]++;
    }

    // Build output DataFrame
    auto* df = new QoreDataFrame();
    df->n_rows = out_rows;

    // Index column
    auto idx_out = std::make_shared<ColumnData>();
    idx_out->type = ColumnType::STRING;
    idx_out->n_rows = out_rows;
    idx_out->null_mask.resize(out_rows, 0);
    idx_out->str_data = unique_indices;
    Column idx_col_out;
    idx_col_out.name = index_col;
    idx_col_out.data = std::move(idx_out);
    df->col_index[idx_col_out.name] = df->columns.size();
    df->columns.push_back(std::move(idx_col_out));

    // Value columns
    for (size_t c = 0; c < unique_cols.size(); ++c) {
        auto cd = std::make_shared<ColumnData>();
        cd->type = ColumnType::FLOAT64;
        cd->n_rows = out_rows;
        cd->null_mask.resize(out_rows, 0);
        cd->float_data.resize(out_rows);
        for (int64_t r = 0; r < out_rows; ++r) {
            if (counts[r][c] == 0) {
                cd->null_mask[r] = 1;
                cd->float_data(r) = std::numeric_limits<double>::quiet_NaN();
            } else if (agg_func == "mean") {
                cd->float_data(r) = sums[r][c] / counts[r][c];
            } else {
                // default: sum
                cd->float_data(r) = sums[r][c];
            }
        }
        Column col;
        col.name = unique_cols[c];
        col.data = std::move(cd);
        df->col_index[col.name] = df->columns.size();
        df->columns.push_back(std::move(col));
    }

    return df;
}

// --- melt ---

QoreDataFrame* QoreDataFrame::melt(const QoreListNode* id_vars,
        const QoreListNode* value_vars, const std::string& var_name,
        const std::string& value_name, ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "melting DataFrame")) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx);

    // Resolve id columns
    std::vector<size_t> id_indices;
    std::unordered_set<std::string> id_names;
    if (id_vars) {
        for (size_t i = 0; i < id_vars->size(); ++i) {
            const QoreStringNode* s = id_vars->retrieveEntry(i).get<const QoreStringNode>();
            if (s) {
                int idx = getColIdx(s->c_str(), xsink);
                if (idx < 0) { return nullptr; }
                id_indices.push_back(idx);
                id_names.insert(s->c_str());
            }
        }
    }

    // Resolve value columns (default: all non-id columns)
    std::vector<size_t> val_indices;
    std::vector<std::string> val_names;
    if (value_vars && value_vars->size() > 0) {
        for (size_t i = 0; i < value_vars->size(); ++i) {
            const QoreStringNode* s = value_vars->retrieveEntry(i).get<const QoreStringNode>();
            if (s) {
                int idx = getColIdx(s->c_str(), xsink);
                if (idx < 0) { return nullptr; }
                val_indices.push_back(idx);
                val_names.push_back(s->c_str());
            }
        }
    } else {
        for (size_t i = 0; i < columns.size(); ++i) {
            if (!id_names.count(columns[i].name)) {
                val_indices.push_back(i);
                val_names.push_back(columns[i].name);
            }
        }
    }

    int64_t out_rows = n_rows * (int64_t)val_indices.size();
    auto* df = new QoreDataFrame();
    df->n_rows = out_rows;

    // ID columns: each value repeated len(val_indices) times
    for (size_t ii = 0; ii < id_indices.size(); ++ii) {
        const ColumnData& src = *columns[id_indices[ii]].data;
        auto cd = std::make_shared<ColumnData>();
        cd->type = src.type;
        cd->n_rows = out_rows;
        cd->null_mask.resize(out_rows, 0);

        switch (src.type) {
            case ColumnType::STRING:
                cd->str_data.reserve(out_rows);
                for (int64_t r = 0; r < n_rows; ++r) {
                    for (size_t v = 0; v < val_indices.size(); ++v) {
                        cd->null_mask[r * val_indices.size() + v] = src.null_mask[r];
                        cd->str_data.push_back(src.str_data[r]);
                    }
                }
                break;
            case ColumnType::INT64:
                cd->int_data.reserve(out_rows);
                for (int64_t r = 0; r < n_rows; ++r) {
                    for (size_t v = 0; v < val_indices.size(); ++v) {
                        cd->null_mask[r * val_indices.size() + v] = src.null_mask[r];
                        cd->int_data.push_back(src.int_data[r]);
                    }
                }
                break;
            case ColumnType::FLOAT64:
                cd->float_data.resize(out_rows);
                for (int64_t r = 0; r < n_rows; ++r) {
                    for (size_t v = 0; v < val_indices.size(); ++v) {
                        int64_t oi = r * val_indices.size() + v;
                        cd->null_mask[oi] = src.null_mask[r];
                        cd->float_data(oi) = src.float_data(r);
                    }
                }
                break;
            default:
                break;
        }

        Column col;
        col.name = columns[id_indices[ii]].name;
        col.data = std::move(cd);
        df->col_index[col.name] = df->columns.size();
        df->columns.push_back(std::move(col));
    }

    // Variable name column
    auto var_cd = std::make_shared<ColumnData>();
    var_cd->type = ColumnType::STRING;
    var_cd->n_rows = out_rows;
    var_cd->null_mask.resize(out_rows, 0);
    var_cd->str_data.reserve(out_rows);
    for (int64_t r = 0; r < n_rows; ++r) {
        for (const auto& vn : val_names) {
            var_cd->str_data.push_back(vn);
        }
    }
    Column var_col;
    var_col.name = var_name;
    var_col.data = std::move(var_cd);
    df->col_index[var_col.name] = df->columns.size();
    df->columns.push_back(std::move(var_col));

    // Value column (all values as float64)
    auto val_cd = std::make_shared<ColumnData>();
    val_cd->type = ColumnType::FLOAT64;
    val_cd->n_rows = out_rows;
    val_cd->null_mask.resize(out_rows, 0);
    val_cd->float_data.resize(out_rows);
    for (int64_t r = 0; r < n_rows; ++r) {
        for (size_t vi = 0; vi < val_indices.size(); ++vi) {
            int64_t oi = r * val_indices.size() + vi;
            const ColumnData& src = *columns[val_indices[vi]].data;
            if (src.isNull(r)) {
                val_cd->null_mask[oi] = 1;
                val_cd->float_data(oi) = std::numeric_limits<double>::quiet_NaN();
            } else {
                val_cd->float_data(oi) = (src.type == ColumnType::FLOAT64)
                    ? src.float_data(r) : (double)src.int_data[r];
            }
        }
    }
    Column val_col;
    val_col.name = value_name;
    val_col.data = std::move(val_cd);
    df->col_index[val_col.name] = df->columns.size();
    df->columns.push_back(std::move(val_col));

    return df;
}

// --- Window functions ---

QoreListNode* QoreDataFrame::rowNumber(const QoreHashNode* spec,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    WindowSpec ws = parseWindowSpec(spec);
    std::vector<int64_t> orig_to_out;
    auto indices = computeWindowOrder(columns, col_index, n_rows, ws, orig_to_out);

    // Resolve partition columns
    std::vector<size_t> part_col_idx;
    for (const auto& pc : ws.partition_by) {
        auto it = col_index.find(pc);
        if (it != col_index.end()) {
            part_col_idx.push_back(it->second);
        }
    }

    // Assign row numbers within each partition
    std::vector<int64_t> result(n_rows);
    int64_t rank = 1;
    for (int64_t i = 0; i < n_rows; ++i) {
        if (i == 0 || !samePartition(columns, part_col_idx, indices[i], indices[i - 1])) {
            rank = 1;
        }
        result[indices[i]] = rank++;
    }

    ReferenceHolder<QoreListNode> list(new QoreListNode(bigIntTypeInfo), xsink);
    for (int64_t i = 0; i < n_rows; ++i) {
        list->push(result[i], xsink);
    }
    return list.release();
}

QoreListNode* QoreDataFrame::lag(const std::string& column, int64_t offset,
        const QoreHashNode* spec, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    int col_idx = getColIdx(column, xsink);
    if (col_idx < 0) { return nullptr; }

    WindowSpec ws = parseWindowSpec(spec);
    std::vector<int64_t> orig_to_out;
    auto indices = computeWindowOrder(columns, col_index, n_rows, ws, orig_to_out);

    std::vector<size_t> part_col_idx;
    for (const auto& pc : ws.partition_by) {
        auto it = col_index.find(pc);
        if (it != col_index.end()) {
            part_col_idx.push_back(it->second);
        }
    }

    const ColumnData& cd = *columns[col_idx].data;
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);

    // For each row in original order, find its position in window order,
    // look back `offset` positions (within same partition)
    for (int64_t orig = 0; orig < n_rows; ++orig) {
        int64_t pos = orig_to_out[orig];
        int64_t src_pos = pos - offset;
        if (src_pos < 0 || !samePartition(columns, part_col_idx,
                indices[pos], indices[src_pos])) {
            list->push(QoreValue(), xsink);  // NOTHING — out of partition
        } else {
            QoreValue v = cd.getValueAt(indices[src_pos], xsink);
            if (*xsink) { return nullptr; }
            list->push(v, xsink);
        }
    }
    return list.release();
}

QoreListNode* QoreDataFrame::lead(const std::string& column, int64_t offset,
        const QoreHashNode* spec, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    int col_idx = getColIdx(column, xsink);
    if (col_idx < 0) { return nullptr; }

    WindowSpec ws = parseWindowSpec(spec);
    std::vector<int64_t> orig_to_out;
    auto indices = computeWindowOrder(columns, col_index, n_rows, ws, orig_to_out);

    std::vector<size_t> part_col_idx;
    for (const auto& pc : ws.partition_by) {
        auto it = col_index.find(pc);
        if (it != col_index.end()) {
            part_col_idx.push_back(it->second);
        }
    }

    const ColumnData& cd = *columns[col_idx].data;
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);

    for (int64_t orig = 0; orig < n_rows; ++orig) {
        int64_t pos = orig_to_out[orig];
        int64_t src_pos = pos + offset;
        if (src_pos >= n_rows || !samePartition(columns, part_col_idx,
                indices[pos], indices[src_pos])) {
            list->push(QoreValue(), xsink);
        } else {
            QoreValue v = cd.getValueAt(indices[src_pos], xsink);
            if (*xsink) { return nullptr; }
            list->push(v, xsink);
        }
    }
    return list.release();
}

QoreListNode* QoreDataFrame::cumSum(const std::string& column,
        const QoreHashNode* spec, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    int col_idx = getColIdx(column, xsink);
    if (col_idx < 0) { return nullptr; }

    WindowSpec ws = parseWindowSpec(spec);
    std::vector<int64_t> orig_to_out;
    auto indices = computeWindowOrder(columns, col_index, n_rows, ws, orig_to_out);

    std::vector<size_t> part_col_idx;
    for (const auto& pc : ws.partition_by) {
        auto it = col_index.find(pc);
        if (it != col_index.end()) {
            part_col_idx.push_back(it->second);
        }
    }

    const ColumnData& cd = *columns[col_idx].data;
    std::vector<double> result(n_rows);
    double running = 0;

    for (int64_t i = 0; i < n_rows; ++i) {
        if (i == 0 || !samePartition(columns, part_col_idx, indices[i], indices[i - 1])) {
            running = 0;
        }
        int64_t row = indices[i];
        if (!cd.isNull(row)) {
            running += (cd.type == ColumnType::FLOAT64) ? cd.float_data(row)
                : (double)cd.int_data[row];
        }
        result[row] = running;
    }

    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);
    for (int64_t i = 0; i < n_rows; ++i) {
        list->push(result[i], xsink);
    }
    return list.release();
}

QoreListNode* QoreDataFrame::rollingMean(const std::string& column,
        int64_t window_size, const QoreHashNode* spec, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    int col_idx = getColIdx(column, xsink);
    if (col_idx < 0) { return nullptr; }

    WindowSpec ws = parseWindowSpec(spec);
    std::vector<int64_t> orig_to_out;
    auto indices = computeWindowOrder(columns, col_index, n_rows, ws, orig_to_out);

    std::vector<size_t> part_col_idx;
    for (const auto& pc : ws.partition_by) {
        auto it = col_index.find(pc);
        if (it != col_index.end()) {
            part_col_idx.push_back(it->second);
        }
    }

    const ColumnData& cd = *columns[col_idx].data;
    std::vector<double> result(n_rows, std::numeric_limits<double>::quiet_NaN());
    std::vector<uint8_t> result_null(n_rows, 0);

    int64_t part_start = 0;
    for (int64_t i = 0; i < n_rows; ++i) {
        if (i == 0 || !samePartition(columns, part_col_idx, indices[i], indices[i - 1])) {
            part_start = i;
        }
        int64_t win_start = std::max(part_start, i - window_size + 1);
        int64_t count = 0;
        double sum = 0;
        for (int64_t j = win_start; j <= i; ++j) {
            int64_t row = indices[j];
            if (!cd.isNull(row)) {
                sum += (cd.type == ColumnType::FLOAT64) ? cd.float_data(row)
                    : (double)cd.int_data[row];
                ++count;
            }
        }
        int64_t orig_row = indices[i];
        if (count > 0) {
            result[orig_row] = sum / count;
        } else {
            result_null[orig_row] = 1;
        }
    }

    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);
    for (int64_t i = 0; i < n_rows; ++i) {
        if (result_null[i]) {
            list->push(QoreValue(), xsink);
        } else {
            list->push(result[i], xsink);
        }
    }
    return list.release();
}

} // namespace QoreDataFrameNS
