/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    df_join.cpp

    DataFrame join operations implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_DataFrame.h"

#include <unordered_map>
#include <unordered_set>

// Ensure Eigen VectorXd has capacity (doubling strategy to avoid O(N²) resize)
static void ensureFloatCapacity(Eigen::VectorXd& vec, int64_t needed) {
    if (needed <= vec.size()) {
        return;
    }
    int64_t new_cap = vec.size() * 2;
    if (new_cap < needed) {
        new_cap = needed;
    }
    if (new_cap < 64) {
        new_cap = 64;
    }
    vec.conservativeResize(new_cap);
}

// Reserve append capacity for a column while keeping n_rows as the logical size.
static void reserveAppendCapacity(QoreDataFrameNS::ColumnData& col, int64_t rows) {
    if (rows <= 0) {
        return;
    }
    col.null_mask.reserve(rows);
    switch (col.type) {
        case QoreDataFrameNS::ColumnType::INT64:
            col.int_data.reserve(rows);
            break;
        case QoreDataFrameNS::ColumnType::FLOAT64:
            ensureFloatCapacity(col.float_data, rows);
            break;
        case QoreDataFrameNS::ColumnType::STRING:
            col.str_data.reserve(rows);
            break;
        case QoreDataFrameNS::ColumnType::BOOL:
            col.bool_data.reserve(rows);
            break;
        case QoreDataFrameNS::ColumnType::DATE:
            col.date_data.reserve(rows);
            break;
        case QoreDataFrameNS::ColumnType::AUTO:
            col.auto_data.reserve(rows);
            break;
        default:
            break;
    }
}

namespace QoreDataFrameNS {

// Build a string key from column values at a given row index
static std::string buildJoinKey(const std::vector<size_t>& key_col_indices,
        const std::vector<Column>& columns, int64_t row) {
    std::string key;
    for (size_t ki = 0; ki < key_col_indices.size(); ++ki) {
        const ColumnData& cd = *columns[key_col_indices[ki]].data;
        if (ki > 0) {
            key += '\x01';
        }
        if (cd.isNull(row)) {
            key += "\x00NULL\x00";
        } else {
            switch (cd.type) {
                case ColumnType::INT64:
                    key += std::to_string(cd.int_data[row]);
                    break;
                case ColumnType::FLOAT64:
                    key += std::to_string(cd.float_data(row));
                    break;
                case ColumnType::STRING:
                    key += cd.str_data[row];
                    break;
                case ColumnType::BOOL:
                    key += cd.bool_data[row] ? "1" : "0";
                    break;
                case ColumnType::DATE:
                    key += std::to_string(cd.date_data[row]);
                    break;
                case ColumnType::AUTO: {
                    QoreStringValueHelper sh(cd.auto_data[row]);
                    key += sh->c_str();
                    break;
                }
                default:
                    break;
            }
        }
    }
    return key;
}

// Append a row from src columns to dest columns (or null if row_idx < 0)
static void appendRow(std::vector<std::shared_ptr<ColumnData>>& dest_cols,
        const std::vector<Column>& src_cols,
        const std::vector<size_t>& col_indices, int64_t row_idx) {
    for (size_t i = 0; i < col_indices.size(); ++i) {
        ColumnData& dest = *dest_cols[i];
        const ColumnData& src = *src_cols[col_indices[i]].data;
        int64_t idx = dest.n_rows;
        ++dest.n_rows;

        if (row_idx < 0 || src.isNull(row_idx)) {
            dest.null_mask.push_back(1);
            switch (dest.type) {
                case ColumnType::INT64:
                    dest.int_data.push_back(0);
                    break;
                case ColumnType::FLOAT64:
                    ensureFloatCapacity(dest.float_data, dest.n_rows);
                    dest.float_data(idx) = std::numeric_limits<double>::quiet_NaN();
                    break;
                case ColumnType::STRING:
                    dest.str_data.emplace_back();
                    break;
                case ColumnType::BOOL:
                    dest.bool_data.push_back(0);
                    break;
                case ColumnType::DATE:
                    dest.date_data.push_back(0);
                    break;
                case ColumnType::AUTO:
                    dest.appendAutoValue(QoreValue());
                    break;
                default:
                    break;
            }
        } else {
            dest.null_mask.push_back(0);
            switch (dest.type) {
                case ColumnType::INT64:
                    dest.int_data.push_back(src.int_data[row_idx]);
                    break;
                case ColumnType::FLOAT64:
                    ensureFloatCapacity(dest.float_data, dest.n_rows);
                    dest.float_data(idx) = src.float_data(row_idx);
                    break;
                case ColumnType::STRING:
                    dest.str_data.push_back(src.str_data[row_idx]);
                    break;
                case ColumnType::BOOL:
                    dest.bool_data.push_back(src.bool_data[row_idx]);
                    break;
                case ColumnType::DATE:
                    dest.date_data.push_back(src.date_data[row_idx]);
                    break;
                case ColumnType::AUTO:
                    dest.appendAutoValue(src.auto_data[row_idx]);
                    break;
                default:
                    break;
            }
        }
    }
}

QoreDataFrame* QoreDataFrame::join(const QoreDataFrame* other,
        const QoreListNode* on_columns, const std::string& how,
        ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "joining DataFrames")) {
        return nullptr;
    }

    // Validate join type
    if (how != "inner" && how != "left" && how != "right" && how != "outer"
            && how != "cross") {
        xsink->raiseException("DATAFRAME-JOIN-ERROR",
            "unknown join type '%s'; valid types: inner, left, right, outer, cross",
            how.c_str());
        return nullptr;
    }

    // Lock both DataFrames (always lock in consistent address order to avoid deadlock)
    std::unique_lock<std::mutex> lk1(mtx, std::defer_lock);
    std::unique_lock<std::mutex> lk2(other->mtx, std::defer_lock);
    if (this < other) {
        lk1.lock();
        lk2.lock();
    } else if (this > other) {
        lk2.lock();
        lk1.lock();
    } else {
        // Self-join
        lk1.lock();
    }

    // --- Cross join: no key columns needed ---
    if (how == "cross") {
        auto* df = new QoreDataFrame();
        int64_t result_rows = n_rows * other->n_rows;

        // Left columns
        for (const auto& col : columns) {
            auto cd = std::make_shared<ColumnData>();
            cd->type = col.data->type;
            cd->n_rows = 0;
            Column out_col;
            out_col.name = col.name;
            out_col.data = cd;
            df->col_index[out_col.name] = df->columns.size();
            df->columns.push_back(std::move(out_col));
        }
        // Right columns (suffix if name conflicts)
        std::vector<size_t> right_col_indices;
        for (const auto& col : other->columns) {
            auto cd = std::make_shared<ColumnData>();
            cd->type = col.data->type;
            cd->n_rows = 0;
            std::string name = col.name;
            if (df->col_index.count(name)) {
                name += "_right";
            }
            Column out_col;
            out_col.name = name;
            out_col.data = cd;
            right_col_indices.push_back(df->columns.size());
            df->col_index[out_col.name] = df->columns.size();
            df->columns.push_back(std::move(out_col));
        }

        // Build cross product
        std::vector<std::shared_ptr<ColumnData>> left_dest, right_dest;
        std::vector<size_t> left_src_indices, right_src_indices;
        for (size_t i = 0; i < columns.size(); ++i) {
            left_dest.push_back(df->columns[i].data);
            left_src_indices.push_back(i);
        }
        for (size_t i = 0; i < other->columns.size(); ++i) {
            right_dest.push_back(df->columns[columns.size() + i].data);
            right_src_indices.push_back(i);
        }

        for (int64_t l = 0; l < n_rows; ++l) {
            for (int64_t r = 0; r < other->n_rows; ++r) {
                appendRow(left_dest, columns, left_src_indices, l);
                appendRow(right_dest, other->columns, right_src_indices, r);
            }
        }

        df->n_rows = result_rows;
        return df;
    }

    // --- Keyed joins: inner, left, right, outer ---

    // Resolve key column indices in both DataFrames
    std::vector<size_t> left_key_indices, right_key_indices;
    std::unordered_set<std::string> key_names;
    for (size_t i = 0; i < on_columns->size(); ++i) {
        std::string name;
        if (!getDataFrameString(on_columns->retrieveEntry(i), name)) {
            xsink->raiseException("DATAFRAME-JOIN-ERROR",
                "join key list element %zu is not a string", i);
            return nullptr;
        }
        key_names.insert(name);

        auto lit = col_index.find(name);
        if (lit == col_index.end()) {
            xsink->raiseException("DATAFRAME-JOIN-ERROR",
                "join key column '%s' not found in left DataFrame", name.c_str());
            return nullptr;
        }
        left_key_indices.push_back(lit->second);

        auto rit = other->col_index.find(name);
        if (rit == other->col_index.end()) {
            xsink->raiseException("DATAFRAME-JOIN-ERROR",
                "join key column '%s' not found in right DataFrame", name.c_str());
            return nullptr;
        }
        right_key_indices.push_back(rit->second);
    }

    bool single_int64_key = left_key_indices.size() == 1
        && columns[left_key_indices[0]].data->type == ColumnType::INT64
        && other->columns[right_key_indices[0]].data->type == ColumnType::INT64;

    // Build hash table from the right DataFrame: key → list of row indices
    std::unordered_map<std::string, std::vector<int64_t>> right_hash;
    std::unordered_map<int64_t, std::vector<int64_t>> right_int64_hash;
    std::unordered_map<int64_t, int64_t> right_int64_unique_hash;
    std::vector<int64_t> right_null_rows;
    bool unique_right_keys = false;
    if (single_int64_key && how == "left") {
        unique_right_keys = true;
        right_int64_unique_hash.reserve(other->n_rows);
        const ColumnData& rkey = *other->columns[right_key_indices[0]].data;
        for (int64_t r = 0; r < other->n_rows; ++r) {
            if (r && !(r % 100) && qore_check_cancel(xsink, "joining DataFrames")) {
                return nullptr;
            }
            if (rkey.isNull(r)) {
                if (!right_null_rows.empty()) {
                    unique_right_keys = false;
                    break;
                }
                right_null_rows.push_back(r);
            } else {
                auto inserted = right_int64_unique_hash.emplace(rkey.int_data[r], r);
                if (!inserted.second) {
                    unique_right_keys = false;
                    break;
                }
            }
        }

        if (!unique_right_keys) {
            right_null_rows.clear();
            right_int64_unique_hash.clear();
        }
    }

    if (single_int64_key && !unique_right_keys) {
        right_int64_hash.reserve(other->n_rows);
        const ColumnData& rkey = *other->columns[right_key_indices[0]].data;
        for (int64_t r = 0; r < other->n_rows; ++r) {
            if (r && !(r % 100) && qore_check_cancel(xsink, "joining DataFrames")) {
                return nullptr;
            }
            if (rkey.isNull(r)) {
                right_null_rows.push_back(r);
            } else {
                right_int64_hash[rkey.int_data[r]].push_back(r);
            }
        }
    } else if (!single_int64_key) {
        right_hash.reserve(other->n_rows);
        for (int64_t r = 0; r < other->n_rows; ++r) {
            if (r && !(r % 100) && qore_check_cancel(xsink, "joining DataFrames")) {
                return nullptr;
            }
            std::string key = buildJoinKey(right_key_indices, other->columns, r);
            right_hash[key].push_back(r);
        }
    }

    if (single_int64_key && how == "left") {
        if (unique_right_keys) {
            const ColumnData& lkey = *columns[left_key_indices[0]].data;
            std::vector<int64_t> right_row_for_left(n_rows, -1);
            for (int64_t l = 0; l < n_rows; ++l) {
                if (l && !(l % 100) && qore_check_cancel(xsink, "joining DataFrames")) {
                    return nullptr;
                }
                if (lkey.isNull(l)) {
                    if (!right_null_rows.empty()) {
                        right_row_for_left[l] = right_null_rows[0];
                    }
                } else {
                    auto i = right_int64_unique_hash.find(lkey.int_data[l]);
                    if (i != right_int64_unique_hash.end()) {
                        right_row_for_left[l] = i->second;
                    }
                }
            }

            auto* fast_df = new QoreDataFrame();
            fast_df->n_rows = n_rows;

            for (size_t i = 0; i < left_key_indices.size(); ++i) {
                const Column& src = columns[left_key_indices[i]];
                Column out_col;
                out_col.name = src.name;
                out_col.data = src.data;
                fast_df->col_index[out_col.name] = fast_df->columns.size();
                fast_df->columns.push_back(std::move(out_col));
            }

            for (size_t i = 0; i < columns.size(); ++i) {
                if (key_names.count(columns[i].name)) {
                    continue;
                }
                Column out_col;
                out_col.name = columns[i].name;
                out_col.data = columns[i].data;
                fast_df->col_index[out_col.name] = fast_df->columns.size();
                fast_df->columns.push_back(std::move(out_col));
            }

            for (size_t i = 0; i < other->columns.size(); ++i) {
                if (key_names.count(other->columns[i].name)) {
                    continue;
                }

                const Column& src_col = other->columns[i];
                const ColumnData& src = *src_col.data;
                auto cd = std::make_shared<ColumnData>();
                cd->type = src.type;
                cd->n_rows = n_rows;
                cd->null_mask.resize(n_rows, 0);

                switch (src.type) {
                    case ColumnType::INT64:
                        cd->int_data.resize(n_rows);
                        for (int64_t l = 0; l < n_rows; ++l) {
                            if (l && !(l % 100) && qore_check_cancel(xsink, "joining DataFrames")) {
                                delete fast_df;
                                return nullptr;
                            }
                            int64_t r = right_row_for_left[l];
                            if (r < 0 || src.isNull(r)) {
                                cd->null_mask[l] = 1;
                                cd->int_data[l] = 0;
                            } else {
                                cd->int_data[l] = src.int_data[r];
                            }
                        }
                        break;
                    case ColumnType::FLOAT64:
                        cd->float_data.resize(n_rows);
                        for (int64_t l = 0; l < n_rows; ++l) {
                            if (l && !(l % 100) && qore_check_cancel(xsink, "joining DataFrames")) {
                                delete fast_df;
                                return nullptr;
                            }
                            int64_t r = right_row_for_left[l];
                            if (r < 0 || src.isNull(r)) {
                                cd->null_mask[l] = 1;
                                cd->float_data(l) = std::numeric_limits<double>::quiet_NaN();
                            } else {
                                cd->float_data(l) = src.float_data(r);
                            }
                        }
                        break;
                    case ColumnType::STRING:
                        cd->str_data.resize(n_rows);
                        for (int64_t l = 0; l < n_rows; ++l) {
                            if (l && !(l % 100) && qore_check_cancel(xsink, "joining DataFrames")) {
                                delete fast_df;
                                return nullptr;
                            }
                            int64_t r = right_row_for_left[l];
                            if (r < 0 || src.isNull(r)) {
                                cd->null_mask[l] = 1;
                            } else {
                                cd->str_data[l] = src.str_data[r];
                            }
                        }
                        break;
                    case ColumnType::BOOL:
                        cd->bool_data.resize(n_rows);
                        for (int64_t l = 0; l < n_rows; ++l) {
                            if (l && !(l % 100) && qore_check_cancel(xsink, "joining DataFrames")) {
                                delete fast_df;
                                return nullptr;
                            }
                            int64_t r = right_row_for_left[l];
                            if (r < 0 || src.isNull(r)) {
                                cd->null_mask[l] = 1;
                                cd->bool_data[l] = 0;
                            } else {
                                cd->bool_data[l] = src.bool_data[r];
                            }
                        }
                        break;
                    case ColumnType::DATE:
                        cd->date_data.resize(n_rows);
                        for (int64_t l = 0; l < n_rows; ++l) {
                            if (l && !(l % 100) && qore_check_cancel(xsink, "joining DataFrames")) {
                                delete fast_df;
                                return nullptr;
                            }
                            int64_t r = right_row_for_left[l];
                            if (r < 0 || src.isNull(r)) {
                                cd->null_mask[l] = 1;
                                cd->date_data[l] = 0;
                            } else {
                                cd->date_data[l] = src.date_data[r];
                            }
                        }
                        break;
                    case ColumnType::AUTO:
                        cd->auto_data.resize(n_rows);
                        for (int64_t l = 0; l < n_rows; ++l) {
                            if (l && !(l % 100) && qore_check_cancel(xsink, "joining DataFrames")) {
                                delete fast_df;
                                return nullptr;
                            }
                            int64_t r = right_row_for_left[l];
                            if (r < 0 || src.isNull(r)) {
                                cd->null_mask[l] = 1;
                            } else {
                                cd->setAutoValue(l, src.auto_data[r], xsink);
                            }
                        }
                        break;
                    default:
                        break;
                }

                std::string name = src_col.name;
                if (fast_df->col_index.count(name)) {
                    name += "_right";
                }
                Column out_col;
                out_col.name = name;
                out_col.data = std::move(cd);
                fast_df->col_index[out_col.name] = fast_df->columns.size();
                fast_df->columns.push_back(std::move(out_col));
            }

            return fast_df;
        }
    }

    // Set up output columns: key columns + left non-key + right non-key
    auto* df = new QoreDataFrame();

    // Key columns (from left)
    std::vector<size_t> out_key_src;  // left column index for each key
    for (size_t i = 0; i < left_key_indices.size(); ++i) {
        const Column& lc = columns[left_key_indices[i]];
        auto cd = std::make_shared<ColumnData>();
        cd->type = lc.data->type;
        cd->n_rows = 0;
        Column out_col;
        out_col.name = lc.name;
        out_col.data = cd;
        out_key_src.push_back(left_key_indices[i]);
        df->col_index[out_col.name] = df->columns.size();
        df->columns.push_back(std::move(out_col));
    }

    // Left non-key columns
    std::vector<size_t> left_nonkey_src;
    size_t left_nonkey_start = df->columns.size();
    for (size_t i = 0; i < columns.size(); ++i) {
        if (key_names.count(columns[i].name)) {
            continue;
        }
        auto cd = std::make_shared<ColumnData>();
        cd->type = columns[i].data->type;
        cd->n_rows = 0;
        Column out_col;
        out_col.name = columns[i].name;
        out_col.data = cd;
        left_nonkey_src.push_back(i);
        df->col_index[out_col.name] = df->columns.size();
        df->columns.push_back(std::move(out_col));
    }

    // Right non-key columns
    std::vector<size_t> right_nonkey_src;
    size_t right_nonkey_start = df->columns.size();
    for (size_t i = 0; i < other->columns.size(); ++i) {
        if (key_names.count(other->columns[i].name)) {
            continue;
        }
        auto cd = std::make_shared<ColumnData>();
        cd->type = other->columns[i].data->type;
        cd->n_rows = 0;
        std::string name = other->columns[i].name;
        if (df->col_index.count(name)) {
            name += "_right";
        }
        Column out_col;
        out_col.name = name;
        out_col.data = cd;
        right_nonkey_src.push_back(i);
        df->col_index[out_col.name] = df->columns.size();
        df->columns.push_back(std::move(out_col));
    }

    // Build dest column vectors for appendRow
    std::vector<std::shared_ptr<ColumnData>> key_dest, left_dest, right_dest;
    for (size_t i = 0; i < out_key_src.size(); ++i) {
        key_dest.push_back(df->columns[i].data);
    }
    for (size_t i = 0; i < left_nonkey_src.size(); ++i) {
        left_dest.push_back(df->columns[left_nonkey_start + i].data);
    }
    for (size_t i = 0; i < right_nonkey_src.size(); ++i) {
        right_dest.push_back(df->columns[right_nonkey_start + i].data);
    }

    int64_t reserve_rows = (how == "right") ? other->n_rows : n_rows;
    if (how == "outer") {
        reserve_rows += other->n_rows;
    }
    for (auto& col : df->columns) {
        reserveAppendCapacity(*col.data, reserve_rows);
    }

    // Track which right rows were matched (for right/outer joins)
    std::vector<bool> right_matched(other->n_rows, false);

    // Probe: iterate left rows
    int64_t result_rows = 0;
    const ColumnData* left_int64_key = single_int64_key ? columns[left_key_indices[0]].data.get() : nullptr;
    for (int64_t l = 0; l < n_rows; ++l) {
        const std::vector<int64_t>* matched_rows = nullptr;
        std::unordered_map<std::string, std::vector<int64_t>>::const_iterator string_it;
        std::unordered_map<int64_t, std::vector<int64_t>>::const_iterator int_it;

        if (single_int64_key) {
            if (left_int64_key->isNull(l)) {
                if (!right_null_rows.empty()) {
                    matched_rows = &right_null_rows;
                }
            } else {
                int_it = right_int64_hash.find(left_int64_key->int_data[l]);
                if (int_it != right_int64_hash.end()) {
                    matched_rows = &int_it->second;
                }
            }
        } else {
            std::string key = buildJoinKey(left_key_indices, columns, l);
            string_it = right_hash.find(key);
            if (string_it != right_hash.end()) {
                matched_rows = &string_it->second;
            }
        }

        if (matched_rows) {
            // Matched: emit one output row per right match
            for (int64_t r : *matched_rows) {
                appendRow(key_dest, columns, out_key_src, l);
                appendRow(left_dest, columns, left_nonkey_src, l);
                appendRow(right_dest, other->columns, right_nonkey_src, r);
                right_matched[r] = true;
                ++result_rows;
            }
        } else if (how == "left" || how == "outer") {
            // Left/outer: emit left row with null right columns
            appendRow(key_dest, columns, out_key_src, l);
            appendRow(left_dest, columns, left_nonkey_src, l);
            appendRow(right_dest, other->columns, right_nonkey_src, -1);
            ++result_rows;
        }
        // inner: skip unmatched left rows
    }

    // For right/outer: emit unmatched right rows
    if (how == "right" || how == "outer") {
        for (int64_t r = 0; r < other->n_rows; ++r) {
            if (!right_matched[r]) {
                // Key from right side
                appendRow(key_dest, other->columns, right_key_indices, r);
                appendRow(left_dest, columns, left_nonkey_src, -1);
                appendRow(right_dest, other->columns, right_nonkey_src, r);
                ++result_rows;
            }
        }
    }

    // Trim over-allocated Eigen float vectors
    for (auto& col : df->columns) {
        if (col.data->type == ColumnType::FLOAT64
                && col.data->float_data.size() > col.data->n_rows) {
            col.data->float_data.conservativeResize(col.data->n_rows);
        }
    }

    df->n_rows = result_rows;
    return df;
}

} // namespace QoreDataFrameNS
