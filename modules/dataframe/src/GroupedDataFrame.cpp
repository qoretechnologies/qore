/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    GroupedDataFrame.cpp

    GroupedDataFrame implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_GroupedDataFrame.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace QoreDataFrameNS {

QoreGroupedDataFrame::QoreGroupedDataFrame(const QoreDataFrame* source,
        const std::vector<std::string>& group_cols, ExceptionSink* xsink) {
    group_col_names = group_cols;

    // Lock the source DataFrame to read its columns
    std::lock_guard<std::mutex> lk(source->getMutex());

    const auto& src_columns = source->getColumns();
    const auto& src_col_index = source->getColIndex();
    source_n_rows = source->getNumRows();

    // Copy column references (shared_ptr — no deep copy)
    for (const auto& col : src_columns) {
        source_col_index[col.name] = source_columns.size();
        source_columns.push_back(col);
    }

    // Validate group columns exist
    for (const auto& gc : group_cols) {
        if (src_col_index.find(gc) == src_col_index.end()) {
            xsink->raiseException("DATAFRAME-COLUMN-ERROR",
                "group column '%s' not found", gc.c_str());
            return;
        }
    }

    // Build groups: hash group key values → row indices
    std::unordered_map<std::string, size_t> group_map;  // key_str → group index

    for (int64_t r = 0; r < source_n_rows; ++r) {
        // Build key string from group column values
        std::string key;
        std::vector<std::string> key_values;
        for (const auto& gc : group_cols) {
            size_t ci = src_col_index.at(gc);
            const ColumnData& cd = *src_columns[ci].data;
            std::string val;
            if (cd.isNull(r)) {
                val = "\x00NULL\x00";  // sentinel for null
            } else {
                switch (cd.type) {
                    case ColumnType::FLOAT64:
                        val = std::to_string(cd.float_data(r));
                        break;
                    case ColumnType::INT64:
                        val = std::to_string(cd.int_data[r]);
                        break;
                    case ColumnType::STRING:
                        val = cd.str_data[r];
                        break;
                    case ColumnType::BOOL:
                        val = cd.bool_data[r] ? "true" : "false";
                        break;
                    case ColumnType::DATE:
                        val = std::to_string(cd.date_data[r]);
                        break;
                    default:
                        break;
                }
            }
            if (!key.empty()) {
                key += "\x01";  // separator
            }
            key += val;
            key_values.push_back(val);
        }

        auto it = group_map.find(key);
        if (it == group_map.end()) {
            size_t gi = groups.size();
            group_map[key] = gi;
            groups.push_back({std::move(key_values), {r}});
        } else {
            groups[it->second].row_indices.push_back(r);
        }
    }
}

double QoreGroupedDataFrame::aggregateNumeric(const ColumnData& cd,
        const std::vector<int64_t>& indices, const std::string& func) const {
    std::vector<double> vals;
    vals.reserve(indices.size());
    for (int64_t i : indices) {
        if (!cd.isNull(i)) {
            double v = (cd.type == ColumnType::FLOAT64) ? cd.float_data(i)
                : (double)cd.int_data[i];
            vals.push_back(v);
        }
    }

    if (vals.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    if (func == "sum") {
        double s = 0;
        for (double v : vals) { s += v; }
        return s;
    }
    if (func == "mean") {
        double s = 0;
        for (double v : vals) { s += v; }
        return s / vals.size();
    }
    if (func == "min") {
        return *std::min_element(vals.begin(), vals.end());
    }
    if (func == "max") {
        return *std::max_element(vals.begin(), vals.end());
    }
    if (func == "std") {
        if (vals.size() < 2) { return 0.0; }
        double s = 0;
        for (double v : vals) { s += v; }
        double mean = s / vals.size();
        double var_sum = 0;
        for (double v : vals) { var_sum += (v - mean) * (v - mean); }
        return std::sqrt(var_sum / (vals.size() - 1));
    }
    if (func == "median") {
        std::sort(vals.begin(), vals.end());
        size_t n = vals.size();
        if (n % 2 == 0) {
            return (vals[n / 2 - 1] + vals[n / 2]) / 2.0;
        }
        return vals[n / 2];
    }
    if (func == "count") {
        return (double)vals.size();
    }
    if (func == "first") {
        return vals.front();
    }
    if (func == "last") {
        return vals.back();
    }
    return std::numeric_limits<double>::quiet_NaN();
}

QoreDataFrame* QoreGroupedDataFrame::agg(const QoreHashNode* agg_spec,
        ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "aggregating grouped DataFrame")) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx);

    int64_t n_groups = (int64_t)groups.size();

    // Build group key columns
    auto* df = new QoreDataFrame();
    for (const auto& gc : group_col_names) {
        auto it = source_col_index.find(gc);
        if (it == source_col_index.end()) {
            delete df;
            xsink->raiseException("DATAFRAME-COLUMN-ERROR",
                "group column '%s' not found", gc.c_str());
            return nullptr;
        }
        const ColumnData& src_cd = *source_columns[it->second].data;
        auto cd = std::make_shared<ColumnData>();
        cd->type = src_cd.type;
        cd->n_rows = n_groups;
        cd->null_mask.resize(n_groups, 0);

        switch (src_cd.type) {
            case ColumnType::INT64:
                cd->int_data.resize(n_groups);
                for (int64_t g = 0; g < n_groups; ++g) {
                    int64_t r = groups[g].row_indices[0];
                    cd->null_mask[g] = src_cd.null_mask[r];
                    cd->int_data[g] = src_cd.int_data[r];
                }
                break;
            case ColumnType::FLOAT64:
                cd->float_data.resize(n_groups);
                for (int64_t g = 0; g < n_groups; ++g) {
                    int64_t r = groups[g].row_indices[0];
                    cd->null_mask[g] = src_cd.null_mask[r];
                    cd->float_data(g) = src_cd.float_data(r);
                }
                break;
            case ColumnType::STRING:
                cd->str_data.resize(n_groups);
                for (int64_t g = 0; g < n_groups; ++g) {
                    int64_t r = groups[g].row_indices[0];
                    cd->null_mask[g] = src_cd.null_mask[r];
                    cd->str_data[g] = src_cd.str_data[r];
                }
                break;
            case ColumnType::BOOL:
                cd->bool_data.resize(n_groups);
                for (int64_t g = 0; g < n_groups; ++g) {
                    int64_t r = groups[g].row_indices[0];
                    cd->null_mask[g] = src_cd.null_mask[r];
                    cd->bool_data[g] = src_cd.bool_data[r];
                }
                break;
            default:
                break;
        }

        Column col;
        col.name = gc;
        col.data = std::move(cd);
        df->col_index[col.name] = df->columns.size();
        df->columns.push_back(std::move(col));
    }

    // Apply aggregation functions per column
    ConstHashIterator hi(agg_spec);
    while (hi.next()) {
        std::string col_name = hi.getKey();
        auto col_it = source_col_index.find(col_name);
        if (col_it == source_col_index.end()) {
            delete df;
            xsink->raiseException("DATAFRAME-COLUMN-ERROR",
                "aggregation column '%s' not found", col_name.c_str());
            return nullptr;
        }
        const ColumnData& src_cd = *source_columns[col_it->second].data;

        // Get function name(s)
        QoreValue func_val = hi.get();
        std::vector<std::string> funcs;
        if (func_val.getType() == NT_STRING) {
            funcs.push_back(func_val.get<const QoreStringNode>()->c_str());
        } else if (func_val.getType() == NT_LIST) {
            const QoreListNode* func_list = func_val.get<const QoreListNode>();
            for (size_t i = 0; i < func_list->size(); ++i) {
                const QoreStringNode* fs = func_list->retrieveEntry(i)
                    .get<const QoreStringNode>();
                if (fs) {
                    funcs.push_back(fs->c_str());
                }
            }
        }

        for (const auto& func : funcs) {
            // Numeric aggregations → float64 result column
            if (src_cd.type == ColumnType::FLOAT64 || src_cd.type == ColumnType::INT64) {
                auto cd = std::make_shared<ColumnData>();
                cd->type = ColumnType::FLOAT64;
                cd->n_rows = n_groups;
                cd->null_mask.resize(n_groups, 0);
                cd->float_data.resize(n_groups);

                for (int64_t g = 0; g < n_groups; ++g) {
                    double val = aggregateNumeric(src_cd, groups[g].row_indices, func);
                    if (std::isnan(val)) {
                        cd->null_mask[g] = 1;
                    }
                    cd->float_data(g) = val;
                }

                std::string result_name = (funcs.size() > 1)
                    ? col_name + "_" + func : col_name;
                Column col;
                col.name = result_name;
                col.data = std::move(cd);
                df->col_index[col.name] = df->columns.size();
                df->columns.push_back(std::move(col));
            } else if (func == "count") {
                // count works on any type
                auto cd = std::make_shared<ColumnData>();
                cd->type = ColumnType::INT64;
                cd->n_rows = n_groups;
                cd->null_mask.resize(n_groups, 0);
                cd->int_data.resize(n_groups);

                for (int64_t g = 0; g < n_groups; ++g) {
                    int64_t cnt = 0;
                    for (int64_t r : groups[g].row_indices) {
                        if (!src_cd.isNull(r)) {
                            ++cnt;
                        }
                    }
                    cd->int_data[g] = cnt;
                }

                std::string result_name = (funcs.size() > 1)
                    ? col_name + "_" + func : col_name;
                Column col;
                col.name = result_name;
                col.data = std::move(cd);
                df->col_index[col.name] = df->columns.size();
                df->columns.push_back(std::move(col));
            } else if (func == "nunique") {
                // unique count for string columns
                auto cd = std::make_shared<ColumnData>();
                cd->type = ColumnType::INT64;
                cd->n_rows = n_groups;
                cd->null_mask.resize(n_groups, 0);
                cd->int_data.resize(n_groups);

                for (int64_t g = 0; g < n_groups; ++g) {
                    std::unordered_set<std::string> uniq;
                    for (int64_t r : groups[g].row_indices) {
                        if (!src_cd.isNull(r) && src_cd.type == ColumnType::STRING) {
                            uniq.insert(src_cd.str_data[r]);
                        }
                    }
                    cd->int_data[g] = (int64_t)uniq.size();
                }

                std::string result_name = (funcs.size() > 1)
                    ? col_name + "_" + func : col_name;
                Column col;
                col.name = result_name;
                col.data = std::move(cd);
                df->col_index[col.name] = df->columns.size();
                df->columns.push_back(std::move(col));
            }
        }
    }

    df->n_rows = n_groups;
    return df;
}

QoreDataFrame* QoreGroupedDataFrame::count(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    int64_t n_groups = (int64_t)groups.size();
    auto* df = new QoreDataFrame();

    // Group key columns (same as in agg)
    for (const auto& gc : group_col_names) {
        auto it = source_col_index.find(gc);
        const ColumnData& src_cd = *source_columns[it->second].data;
        auto cd = std::make_shared<ColumnData>();
        cd->type = src_cd.type;
        cd->n_rows = n_groups;
        cd->null_mask.resize(n_groups, 0);

        switch (src_cd.type) {
            case ColumnType::STRING:
                cd->str_data.resize(n_groups);
                for (int64_t g = 0; g < n_groups; ++g) {
                    int64_t r = groups[g].row_indices[0];
                    cd->null_mask[g] = src_cd.null_mask[r];
                    cd->str_data[g] = src_cd.str_data[r];
                }
                break;
            case ColumnType::INT64:
                cd->int_data.resize(n_groups);
                for (int64_t g = 0; g < n_groups; ++g) {
                    int64_t r = groups[g].row_indices[0];
                    cd->null_mask[g] = src_cd.null_mask[r];
                    cd->int_data[g] = src_cd.int_data[r];
                }
                break;
            default:
                break;
        }

        Column col;
        col.name = gc;
        col.data = std::move(cd);
        df->col_index[col.name] = df->columns.size();
        df->columns.push_back(std::move(col));
    }

    // Count column
    auto cd = std::make_shared<ColumnData>();
    cd->type = ColumnType::INT64;
    cd->n_rows = n_groups;
    cd->null_mask.resize(n_groups, 0);
    cd->int_data.resize(n_groups);
    for (int64_t g = 0; g < n_groups; ++g) {
        cd->int_data[g] = (int64_t)groups[g].row_indices.size();
    }

    Column col;
    col.name = "count";
    col.data = std::move(cd);
    df->col_index[col.name] = df->columns.size();
    df->columns.push_back(std::move(col));

    df->n_rows = n_groups;
    return df;
}

QoreDataFrame* QoreGroupedDataFrame::sum(ExceptionSink* xsink) const {
    // Build agg_spec: all numeric columns → "sum"
    ReferenceHolder<QoreHashNode> spec(new QoreHashNode(autoTypeInfo), xsink);
    for (const auto& col : source_columns) {
        // Skip group columns
        bool is_group = false;
        for (const auto& gc : group_col_names) {
            if (col.name == gc) { is_group = true; break; }
        }
        if (is_group) { continue; }
        if (col.data->type == ColumnType::FLOAT64 || col.data->type == ColumnType::INT64) {
            spec->setKeyValue(col.name.c_str(), new QoreStringNode("sum"), xsink);
        }
    }
    return agg(*spec, xsink);
}

QoreDataFrame* QoreGroupedDataFrame::mean(ExceptionSink* xsink) const {
    ReferenceHolder<QoreHashNode> spec(new QoreHashNode(autoTypeInfo), xsink);
    for (const auto& col : source_columns) {
        bool is_group = false;
        for (const auto& gc : group_col_names) {
            if (col.name == gc) { is_group = true; break; }
        }
        if (is_group) { continue; }
        if (col.data->type == ColumnType::FLOAT64 || col.data->type == ColumnType::INT64) {
            spec->setKeyValue(col.name.c_str(), new QoreStringNode("mean"), xsink);
        }
    }
    return agg(*spec, xsink);
}

int64_t QoreGroupedDataFrame::numGroups() const {
    std::lock_guard<std::mutex> lk(mtx);
    return (int64_t)groups.size();
}

} // namespace QoreDataFrameNS
