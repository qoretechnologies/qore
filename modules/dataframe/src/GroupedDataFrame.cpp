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
    group_map.reserve(static_cast<size_t>(std::min<int64_t>(source_n_rows, 65536)));

    auto get_key_value = [](const ColumnData& cd, int64_t row) -> std::string {
        if (cd.isNull(row)) {
            return "\x00NULL\x00";  // sentinel for null
        }
        switch (cd.type) {
            case ColumnType::FLOAT64:
                return std::to_string(cd.float_data(row));
            case ColumnType::INT64:
                return std::to_string(cd.int_data[row]);
            case ColumnType::STRING:
                return cd.str_data[row];
            case ColumnType::BOOL:
                return cd.bool_data[row] ? "true" : "false";
            case ColumnType::DATE:
                return std::to_string(cd.date_data[row]);
            case ColumnType::AUTO: {
                QoreStringValueHelper sh(cd.auto_data[row]);
                return sh->c_str();
            }
            default:
                return {};
        }
    };

    for (int64_t r = 0; r < source_n_rows; ++r) {
        if (r && !(r % 100) && qore_check_cancel(xsink, "building grouped DataFrame keys")) {
            return;
        }
        // Build key string from group column values
        std::string key;
        for (const auto& gc : group_cols) {
            size_t ci = src_col_index.at(gc);
            const ColumnData& cd = *src_columns[ci].data;
            std::string val = get_key_value(cd, r);
            if (!key.empty()) {
                key += "\x01";  // separator
            }
            key += val;
        }

        auto inserted = group_map.emplace(std::move(key), groups.size());
        if (inserted.second) {
            std::vector<std::string> key_values;
            key_values.reserve(group_cols.size());
            for (const auto& gc : group_cols) {
                size_t ci = src_col_index.at(gc);
                key_values.push_back(get_key_value(*src_columns[ci].data, r));
            }
            groups.push_back({std::move(key_values), {r}});
        } else {
            groups[inserted.first->second].row_indices.push_back(r);
        }
    }
}

double QoreGroupedDataFrame::aggregateNumeric(const ColumnData& cd,
        const std::vector<int64_t>& indices, const std::string& func, ExceptionSink* xsink) const {
    auto get_value = [&cd](int64_t i) -> double {
        return cd.type == ColumnType::FLOAT64 ? cd.float_data(i) : static_cast<double>(cd.int_data[i]);
    };

    if (func == "sum" || func == "mean") {
        double s = 0;
        int64_t count = 0;
        for (size_t i = 0; i < indices.size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "aggregating numeric DataFrame group")) {
                return std::numeric_limits<double>::quiet_NaN();
            }
            int64_t row = indices[i];
            if (!cd.isNull(row)) {
                s += get_value(row);
                ++count;
            }
        }
        if (!count) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        return func == "mean" ? s / static_cast<double>(count) : s;
    }

    if (func == "min" || func == "max") {
        double result = 0.0;
        bool have_value = false;
        for (size_t i = 0; i < indices.size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "aggregating numeric DataFrame group")) {
                return std::numeric_limits<double>::quiet_NaN();
            }
            int64_t row = indices[i];
            if (cd.isNull(row)) {
                continue;
            }
            double value = get_value(row);
            if (!have_value) {
                result = value;
                have_value = true;
            } else if (func == "min") {
                result = std::min(result, value);
            } else {
                result = std::max(result, value);
            }
        }
        return have_value ? result : std::numeric_limits<double>::quiet_NaN();
    }

    if (func == "count") {
        int64_t count = 0;
        for (size_t i = 0; i < indices.size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "counting numeric DataFrame group")) {
                return std::numeric_limits<double>::quiet_NaN();
            }
            if (!cd.isNull(indices[i])) {
                ++count;
            }
        }
        return static_cast<double>(count);
    }

    if (func == "first") {
        for (size_t i = 0; i < indices.size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "finding first numeric DataFrame group value")) {
                return std::numeric_limits<double>::quiet_NaN();
            }
            int64_t row = indices[i];
            if (!cd.isNull(row)) {
                return get_value(row);
            }
        }
        return std::numeric_limits<double>::quiet_NaN();
    }

    if (func == "last") {
        double result = 0.0;
        bool have_value = false;
        for (size_t i = 0; i < indices.size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "finding last numeric DataFrame group value")) {
                return std::numeric_limits<double>::quiet_NaN();
            }
            int64_t row = indices[i];
            if (!cd.isNull(row)) {
                result = get_value(row);
                have_value = true;
            }
        }
        return have_value ? result : std::numeric_limits<double>::quiet_NaN();
    }

    if (func == "std") {
        double s = 0;
        int64_t count = 0;
        for (size_t i = 0; i < indices.size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "aggregating numeric DataFrame group")) {
                return std::numeric_limits<double>::quiet_NaN();
            }
            int64_t row = indices[i];
            if (!cd.isNull(row)) {
                s += get_value(row);
                ++count;
            }
        }
        if (count < 2) {
            return count ? 0.0 : std::numeric_limits<double>::quiet_NaN();
        }
        double mean = s / static_cast<double>(count);
        double var_sum = 0;
        for (size_t i = 0; i < indices.size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "aggregating numeric DataFrame group variance")) {
                return std::numeric_limits<double>::quiet_NaN();
            }
            int64_t row = indices[i];
            if (!cd.isNull(row)) {
                double delta = get_value(row) - mean;
                var_sum += delta * delta;
            }
        }
        return std::sqrt(var_sum / static_cast<double>(count - 1));
    }

    if (func == "median") {
        std::vector<double> vals;
        vals.reserve(indices.size());
        for (size_t i = 0; i < indices.size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "collecting median DataFrame group values")) {
                return std::numeric_limits<double>::quiet_NaN();
            }
            int64_t row = indices[i];
            if (!cd.isNull(row)) {
                vals.push_back(get_value(row));
            }
        }
        if (vals.empty()) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        std::sort(vals.begin(), vals.end());
        size_t n = vals.size();
        if (n % 2 == 0) {
            return (vals[n / 2 - 1] + vals[n / 2]) / 2.0;
        }
        return vals[n / 2];
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
            case ColumnType::DATE:
                cd->date_data.resize(n_groups);
                for (int64_t g = 0; g < n_groups; ++g) {
                    if (g && !(g % 100) && qore_check_cancel(xsink, "aggregating grouped DataFrame keys")) {
                        delete df;
                        return nullptr;
                    }
                    int64_t r = groups[g].row_indices[0];
                    cd->null_mask[g] = src_cd.null_mask[r];
                    cd->date_data[g] = src_cd.date_data[r];
                }
                break;
            case ColumnType::AUTO:
                cd->auto_data.resize(n_groups);
                for (int64_t g = 0; g < n_groups; ++g) {
                    if (g && !(g % 100) && qore_check_cancel(xsink, "aggregating grouped DataFrame keys")) {
                        delete df;
                        return nullptr;
                    }
                    int64_t r = groups[g].row_indices[0];
                    cd->null_mask[g] = src_cd.null_mask[r];
                    if (!cd->null_mask[g]) {
                        cd->setAutoValue(g, src_cd.auto_data[r], xsink);
                    }
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
        std::string func;
        if (getDataFrameString(func_val, func)) {
            funcs.push_back(func);
        } else if (func_val.getType() == NT_LIST) {
            const QoreListNode* func_list = func_val.get<const QoreListNode>();
            for (size_t i = 0; i < func_list->size(); ++i) {
                if (getDataFrameString(func_list->retrieveEntry(i), func)) {
                    funcs.push_back(func);
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
                    if (g && !(g % 100) && qore_check_cancel(xsink, "aggregating grouped DataFrame columns")) {
                        delete df;
                        return nullptr;
                    }
                    double val = aggregateNumeric(src_cd, groups[g].row_indices, func, xsink);
                    if (*xsink) {
                        delete df;
                        return nullptr;
                    }
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
            case ColumnType::BOOL:
                cd->bool_data.resize(n_groups);
                for (int64_t g = 0; g < n_groups; ++g) {
                    if (g && !(g % 100) && qore_check_cancel(xsink, "counting grouped DataFrame keys")) {
                        delete df;
                        return nullptr;
                    }
                    int64_t r = groups[g].row_indices[0];
                    cd->null_mask[g] = src_cd.null_mask[r];
                    cd->bool_data[g] = src_cd.bool_data[r];
                }
                break;
            case ColumnType::DATE:
                cd->date_data.resize(n_groups);
                for (int64_t g = 0; g < n_groups; ++g) {
                    if (g && !(g % 100) && qore_check_cancel(xsink, "counting grouped DataFrame keys")) {
                        delete df;
                        return nullptr;
                    }
                    int64_t r = groups[g].row_indices[0];
                    cd->null_mask[g] = src_cd.null_mask[r];
                    cd->date_data[g] = src_cd.date_data[r];
                }
                break;
            case ColumnType::AUTO:
                cd->auto_data.resize(n_groups);
                for (int64_t g = 0; g < n_groups; ++g) {
                    if (g && !(g % 100) && qore_check_cancel(xsink, "counting grouped DataFrame keys")) {
                        delete df;
                        return nullptr;
                    }
                    int64_t r = groups[g].row_indices[0];
                    cd->null_mask[g] = src_cd.null_mask[r];
                    if (!cd->null_mask[g]) {
                        cd->setAutoValue(g, src_cd.auto_data[r], xsink);
                    }
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
