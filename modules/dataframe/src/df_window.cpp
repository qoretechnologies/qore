/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    df_window.cpp

    DataFrame pivot, melt, withColumn, and window function implementations

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_DataFrame.h"

#include <algorithm>
#include <charconv>
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

static bool parseWindowSpec(const QoreHashNode* spec, WindowSpec& ws,
        const std::unordered_map<std::string, size_t>& col_index,
        ExceptionSink* xsink) {
    if (!spec) {
        return true;
    }
    QoreValue v = spec->getKeyValue("partition_by");
    if (v.getType() == NT_LIST) {
        const QoreListNode* list = v.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            std::string name;
            if (getDataFrameString(list->retrieveEntry(i), name)) {
                if (!col_index.count(name)) {
                    xsink->raiseException("DATAFRAME-COLUMN-ERROR",
                        "window partition_by column '%s' not found", name.c_str());
                    return false;
                }
                ws.partition_by.push_back(name);
            }
        }
    }
    v = spec->getKeyValue("order_by");
    if (v.getType() == NT_LIST) {
        const QoreListNode* list = v.get<const QoreListNode>();
        for (size_t i = 0; i < list->size(); ++i) {
            std::string name;
            if (getDataFrameString(list->retrieveEntry(i), name)) {
                if (!col_index.count(name)) {
                    xsink->raiseException("DATAFRAME-COLUMN-ERROR",
                        "window order_by column '%s' not found", name.c_str());
                    return false;
                }
                ws.order_by.push_back(name);
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
    return true;
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
            (int64)((int64_t)data->size()), (int64)n_rows);
        return nullptr;
    }

    // the new or replacement column
    Column new_col;
    new_col.name = name;
    new_col.data = buildColumnDataAuto(data, xsink);
    if (*xsink) {
        return nullptr;
    }

    auto* df = new QoreDataFrame();
    df->n_rows = n_rows;

    // Copy all existing columns (shared_ptr — COW); a replaced column keeps its position
    bool replaced = false;
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "copying DataFrame columns")) {
            delete df;
            return nullptr;
        }
        df->col_index[columns[i].name] = df->columns.size();
        if (columns[i].name == name) {
            df->columns.push_back(std::move(new_col));
            replaced = true;
        } else {
            df->columns.push_back(columns[i]);
        }
    }

    // a new column is added last
    if (!replaced) {
        df->col_index[name] = df->columns.size();
        df->columns.push_back(std::move(new_col));
    }

    return df;
}

// --- pivot ---

//! Gives the key and the label of an index or columns cell of pivot() that is not null (qore#6099)
/** The key tells every two different values apart: the bits of a float (0.0 and -0.0 being the same), the
    microseconds of a date, the type and value of a cell of a column that holds any value; the label is the string
    form of the value in UTF-8 (a date with its microseconds and UTC offset).  A cell of a column that holds any
    value that is not a scalar (a list, a hash, a binary value, an object) has no key and raises DATAFRAME-ERROR.  The
    cells of a column other than a column that holds any value all have one type, so their keys have no type tag; a
    string column needs no key, as its strings are their own keys.

    @param label the label to set, or nullptr if only the key is needed

    @return 0 for success, -1 if an exception was raised
*/
static int pivotKey(const ColumnData& cd, int64_t i, std::string& key, std::string* label, const std::string& name,
        ExceptionSink* xsink) {
    switch (cd.type) {
        case ColumnType::STRING:
            key = cd.str_data[i];
            if (label) {
                *label = key;
            }
            return 0;
        case ColumnType::INT64:
        case ColumnType::FLOAT64:
        case ColumnType::BOOL:
        case ColumnType::DATE:
            key = dataFrameValueKey(cd, i);
            if (!label) {
                return 0;
            }
            break;
        default:
            break;
    }
    ValueHolder value(cd.getValueAt(i, xsink), xsink);
    if (*xsink) {
        return -1;
    }
    QoreValue v = value->isEnum() ? value->getEnumMember()->getValue() : *value;
    qore_type_t t = v.getType();
    if (t == NT_DATE) {
        const DateTimeNode* dt = v.get<const DateTimeNode>();
        if (cd.type != ColumnType::DATE) {
            int64 micros;
            if (!dt->getEpochMicrosecondsUTCChecked(micros)) {
                return dataFrameDateOutOfRange(dt, xsink);
            }
            key.assign(dt->isRelative() ? "r" : "d", 1);
            key.append(reinterpret_cast<const char*>(&micros), sizeof(micros));
        }
        if (!label) {
            return 0;
        }
        if (dt->isRelative()) {
            // the length of the duration in seconds with its microseconds, as an ISO-8601 duration ("PT1.5S"), which
            // tells every two keys apart
            int64 micros;
            if (!dt->getEpochMicrosecondsUTCChecked(micros)) {
                return dataFrameDateOutOfRange(dt, xsink);
            }
            uint64_t abs_micros = micros < 0 ? -static_cast<uint64_t>(micros) : static_cast<uint64_t>(micros);
            char buf[64];
            int len = snprintf(buf, sizeof(buf), "%sPT%llu", micros < 0 ? "-" : "",
                static_cast<unsigned long long>(abs_micros / 1000000));
            unsigned frac = static_cast<unsigned>(abs_micros % 1000000);
            if (frac) {
                len += snprintf(buf + len, sizeof(buf) - len, ".%06u", frac);
                while (buf[len - 1] == '0') {
                    --len;
                }
            }
            buf[len++] = 'S';
            label->assign(buf, len);
        } else {
            QoreString str;
            dt->format(str, "YYYY-MM-DDTHH:mm:SS.usZ");
            label->assign(str.c_str(), str.size());
        }
        return 0;
    }
    if (cd.type == ColumnType::AUTO) {
        switch (t) {
            case NT_INT: {
                int64 iv = v.getAsBigInt();
                key.assign("i", 1);
                key.append(reinterpret_cast<const char*>(&iv), sizeof(iv));
                break;
            }
            case NT_FLOAT: {
                double fv = v.getAsFloat();
                if (fv == 0.0) {
                    fv = 0.0;
                }
                key.assign("f", 1);
                key.append(reinterpret_cast<const char*>(&fv), sizeof(fv));
                break;
            }
            case NT_NUMBER:
            case NT_STRING:
            case NT_BOOLEAN:
            case NT_CHAR:
                key.assign(t == NT_NUMBER ? "n" : (t == NT_STRING ? "s" : (t == NT_BOOLEAN ? "b" : "c")), 1);
                key.append(dataFrameUtf8Key(v));
                break;
            default:
                xsink->raiseException("DATAFRAME-ERROR", "pivot column '%s' has a value of type '%s', which cannot "
                    "be a key: the index and columns values must be scalars", name.c_str(), v.getTypeName());
                return -1;
        }
    }
    if (!label) {
        return 0;
    }
    if (t == NT_FLOAT) {
        // the shortest form that gives the float back, so that two different floats have two different labels
        char buf[64];
        auto rc = std::to_chars(buf, buf + sizeof(buf), v.getAsFloat());
        label->assign(buf, rc.ptr);
    } else if (t == NT_BOOLEAN) {
        *label = v.getAsBool() ? "True" : "False";
    } else {
        *label = dataFrameUtf8Key(v);
    }
    return 0;
}

QoreDataFrame* QoreDataFrame::pivot(const std::string& index_col,
        const std::string& columns_col, const std::string& values_col,
        const std::string& agg_func, ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "pivoting DataFrame")) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx);

    if (agg_func != "sum" && agg_func != "mean") {
        xsink->raiseException("DATAFRAME-ERROR",
            "pivot agg_func must be 'sum' or 'mean', got '%s'", agg_func.c_str());
        return nullptr;
    }

    int idx_ci = getColIdx(index_col, xsink);
    if (idx_ci < 0) { return nullptr; }
    int col_ci = getColIdx(columns_col, xsink);
    if (col_ci < 0) { return nullptr; }
    int val_ci = getColIdx(values_col, xsink);
    if (val_ci < 0) { return nullptr; }

    const ColumnData& idx_cd = *columns[idx_ci].data;
    const ColumnData& col_cd = *columns[col_ci].data;
    const ColumnData& val_cd = *columns[val_ci].data;

    // the values are aggregated as numbers: an int or float column, or a column that holds any value whose cells are
    // numbers (a decimal column, for example); any other column has no sum or mean, unless all its cells are null
    // (qore#6099)
    if (val_cd.type != ColumnType::FLOAT64 && val_cd.type != ColumnType::INT64) {
        bool numbers = true;
        for (int64_t i = 0; numbers && i < n_rows; ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "checking DataFrame pivot values")) {
                return nullptr;
            }
            if (!val_cd.isNull(i)) {
                if (val_cd.type != ColumnType::AUTO) {
                    numbers = false;
                } else {
                    qore_type_t t = val_cd.auto_data[i].getType();
                    numbers = t == NT_INT || t == NT_FLOAT || t == NT_NUMBER;
                }
            }
        }
        if (!numbers) {
            xsink->raiseException("DATAFRAME-ERROR", "pivot values column '%s' must hold numbers to be aggregated "
                "with '%s'; its type is '%s'", values_col.c_str(), agg_func.c_str(), columnTypeName(val_cd.type));
            return nullptr;
        }
    }

    // a column that is not an int or float column has only numbers or null cells here; its cells are read as floats
    if (idx_cd.type == ColumnType::INT64 && col_cd.type == ColumnType::STRING) {
        std::vector<int64_t> unique_indices;
        std::unordered_map<int64_t, size_t> index_map;
        std::vector<std::string> unique_cols;
        std::unordered_map<std::string, size_t> col_map;
        index_map.reserve(static_cast<size_t>(std::min<int64_t>(n_rows, 65536)));
        col_map.reserve(static_cast<size_t>(std::min<int64_t>(n_rows, 65536)));

        for (int64_t i = 0; i < n_rows; ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "pivoting integer/string DataFrame keys")) {
                return nullptr;
            }
            // a row with a null index or columns cell is left out, as in the general path below (qore#6099)
            if (idx_cd.isNull(i) || col_cd.isNull(i)) {
                continue;
            }
            int64_t idx_val = idx_cd.int_data[i];
            auto inserted = index_map.emplace(idx_val, unique_indices.size());
            if (inserted.second) {
                unique_indices.push_back(idx_val);
            }
            const std::string& col_val = col_cd.str_data[i];
            auto col_inserted = col_map.emplace(col_val, unique_cols.size());
            if (col_inserted.second) {
                unique_cols.push_back(col_val);
            }
        }

        int64_t out_rows = static_cast<int64_t>(unique_indices.size());
        size_t out_cols = unique_cols.size();
        std::vector<double> sums(static_cast<size_t>(out_rows) * out_cols, 0.0);
        std::vector<int64_t> counts(static_cast<size_t>(out_rows) * out_cols, 0);

        for (int64_t i = 0; i < n_rows; ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "pivoting integer/string DataFrame values")) {
                return nullptr;
            }
            if (idx_cd.isNull(i) || col_cd.isNull(i)) {
                continue;
            }
            auto ri_it = index_map.find(idx_cd.int_data[i]);
            auto ci_it = col_map.find(col_cd.str_data[i]);
            if (ri_it == index_map.end() || ci_it == col_map.end()) {
                continue;
            }
            size_t pos = ri_it->second * out_cols + ci_it->second;
            // a null value is not aggregated: it counts neither for the sum nor for the mean (qore#6099)
            if (!val_cd.isNull(i)) {
                sums[pos] += val_cd.type == ColumnType::FLOAT64 ? val_cd.float_data(i)
                    : (val_cd.type == ColumnType::INT64 ? static_cast<double>(val_cd.int_data[i])
                        : val_cd.auto_data[i].getAsFloat());
                ++counts[pos];
            }
        }

        auto* df = new QoreDataFrame();
        df->n_rows = out_rows;

        auto idx_out = std::make_shared<ColumnData>();
        idx_out->type = ColumnType::INT64;
        idx_out->n_rows = out_rows;
        idx_out->null_mask.resize(out_rows, 0);
        idx_out->int_data = std::move(unique_indices);
        Column idx_col_out;
        idx_col_out.name = index_col;
        idx_col_out.data = std::move(idx_out);
        df->col_index[idx_col_out.name] = df->columns.size();
        df->columns.push_back(std::move(idx_col_out));

        for (size_t c = 0; c < unique_cols.size(); ++c) {
            if (c && !(c % 100) && qore_check_cancel(xsink, "building pivoted DataFrame column set")) {
                delete df;
                return nullptr;
            }
            auto cd = std::make_shared<ColumnData>();
            cd->type = ColumnType::FLOAT64;
            cd->n_rows = out_rows;
            cd->null_mask.resize(out_rows, 0);
            cd->float_data.resize(out_rows);
            for (int64_t r = 0; r < out_rows; ++r) {
                if (r && !(r % 100) && qore_check_cancel(xsink, "building pivoted DataFrame columns")) {
                    delete df;
                    return nullptr;
                }
                size_t pos = static_cast<size_t>(r) * out_cols + c;
                if (!counts[pos]) {
                    cd->null_mask[r] = 1;
                    cd->float_data(r) = std::numeric_limits<double>::quiet_NaN();
                } else if (agg_func == "mean") {
                    cd->float_data(r) = sums[pos] / static_cast<double>(counts[pos]);
                } else {
                    cd->float_data(r) = sums[pos];
                }
            }
            Column col;
            // a column name is in the default encoding, and a string cell in UTF-8
            if (dataFrameDefaultString(unique_cols[c], col.name, xsink)) {
                delete df;
                return nullptr;
            }
            if (df->col_index.count(col.name)) {
                xsink->raiseException("DATAFRAME-ERROR", "pivot columns column '%s' has two values whose column "
                    "name is '%s', or a value whose column name is the name of the index column",
                    columns_col.c_str(), col.name.c_str());
                delete df;
                return nullptr;
            }
            col.data = std::move(cd);
            df->col_index[col.name] = df->columns.size();
            df->columns.push_back(std::move(col));
        }

        return df;
    }

    // the rows are grouped by the exact values of the index and columns cells (a key that tells every two different
    // values apart: the bits of a float, the microseconds of a date), and labelled with their string forms; a row
    // with a null index or columns cell is left out, as in the int/string path above (qore#6099)
    std::vector<std::string> unique_indices;
    std::unordered_map<std::string, size_t> index_map;
    std::vector<std::string> unique_cols;
    std::unordered_map<std::string, size_t> col_map;
    // the column names made so far, to raise an error for two values that make the same name
    std::unordered_map<std::string, std::string> col_names;
    col_names[index_col] = std::string();
    // the labels of the index values of a column that holds any value, to raise an error for two that are the same
    std::unordered_set<std::string> index_labels;
    // the result row and column of each row, or no_slot for a row that is left out, so that the keys are made once
    constexpr size_t no_slot = std::numeric_limits<size_t>::max();
    std::vector<size_t> row_ri(n_rows, no_slot);
    std::vector<size_t> row_ci(n_rows, no_slot);

    // gives the slot of the key of a cell, adding the key and the label of its value if it is new; a cell of a string
    // column is its own key and is not copied
    std::string key_buf;
    std::string label;
    auto slot = [&] (const ColumnData& cd, int64_t i, std::unordered_map<std::string, size_t>& map, size_t& rv,
            bool& added, const std::string& name) -> int {
        const std::string* key = &key_buf;
        if (cd.type == ColumnType::STRING) {
            key = &cd.str_data[i];
        } else if (pivotKey(cd, i, key_buf, nullptr, name, xsink)) {
            return -1;
        }
        auto it = map.find(*key);
        if (it != map.end()) {
            rv = it->second;
            added = false;
            return 0;
        }
        rv = map.size();
        map.emplace(*key, rv);
        added = true;
        std::string new_key;
        return pivotKey(cd, i, new_key, &label, name, xsink);
    };

    for (int64_t i = 0; i < n_rows; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "pivoting DataFrame keys")) {
            return nullptr;
        }
        if (idx_cd.isNull(i) || col_cd.isNull(i)) {
            continue;
        }
        bool added;
        if (slot(idx_cd, i, index_map, row_ri[i], added, index_col)) {
            return nullptr;
        }
        if (added) {
            // values of different types in a column that holds any value can have the same label (1, 1.0 and "1")
            if (idx_cd.type == ColumnType::AUTO && !index_labels.insert(label).second) {
                xsink->raiseException("DATAFRAME-ERROR", "pivot index column '%s' has two values of different types "
                    "whose string form is '%s'", index_col.c_str(), label.c_str());
                return nullptr;
            }
            unique_indices.push_back(std::move(label));
        }

        if (slot(col_cd, i, col_map, row_ci[i], added, columns_col)) {
            return nullptr;
        }
        if (added) {
            // a column name is in the default encoding, and the label in UTF-8
            std::string name;
            if (dataFrameDefaultString(label, name, xsink)) {
                return nullptr;
            }
            auto inserted = col_names.emplace(name, label);
            if (!inserted.second) {
                xsink->raiseException("DATAFRAME-ERROR", "pivot columns column '%s' has two values whose column "
                    "name is '%s', or a value whose column name is the name of the index column",
                    columns_col.c_str(), name.c_str());
                return nullptr;
            }
            unique_cols.push_back(std::move(name));
        }
    }

    // Build result: one row per unique index, one column per unique column value
    int64_t out_rows = (int64_t)unique_indices.size();
    size_t out_cols = unique_cols.size();

    // the accumulation grid, row by row
    std::vector<double> sums(static_cast<size_t>(out_rows) * out_cols, 0.0);
    std::vector<int64_t> counts(static_cast<size_t>(out_rows) * out_cols, 0);

    for (int64_t i = 0; i < n_rows; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "pivoting DataFrame values")) {
            return nullptr;
        }
        // a null value is not aggregated: it counts neither for the sum nor for the mean
        if (row_ri[i] == no_slot || val_cd.isNull(i)) {
            continue;
        }
        size_t pos = row_ri[i] * out_cols + row_ci[i];

        double val;
        switch (val_cd.type) {
            case ColumnType::FLOAT64:
                val = val_cd.float_data(i);
                break;
            case ColumnType::INT64:
                val = (double)val_cd.int_data[i];
                break;
            default:
                val = val_cd.auto_data[i].getAsFloat();
                break;
        }
        sums[pos] += val;
        ++counts[pos];
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
        if (c && !(c % 100) && qore_check_cancel(xsink, "building pivoted DataFrame columns")) {
            delete df;
            return nullptr;
        }
        auto cd = std::make_shared<ColumnData>();
        cd->type = ColumnType::FLOAT64;
        cd->n_rows = out_rows;
        cd->null_mask.resize(out_rows, 0);
        cd->float_data.resize(out_rows);
        for (int64_t r = 0; r < out_rows; ++r) {
            size_t pos = static_cast<size_t>(r) * out_cols + c;
            if (counts[pos] == 0) {
                cd->null_mask[r] = 1;
                cd->float_data(r) = std::numeric_limits<double>::quiet_NaN();
            } else if (agg_func == "mean") {
                cd->float_data(r) = sums[pos] / static_cast<double>(counts[pos]);
            } else {
                // default: sum
                cd->float_data(r) = sums[pos];
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
            std::string name;
            if (getDataFrameString(id_vars->retrieveEntry(i), name)) {
                int idx = getColIdx(name, xsink);
                if (idx < 0) { return nullptr; }
                id_indices.push_back(idx);
                id_names.insert(name);
            }
        }
    }

    // Resolve value columns (default: all non-id columns)
    std::vector<size_t> val_indices;
    std::vector<std::string> val_names;
    if (value_vars && value_vars->size() > 0) {
        for (size_t i = 0; i < value_vars->size(); ++i) {
            std::string name;
            if (getDataFrameString(value_vars->retrieveEntry(i), name)) {
                int idx = getColIdx(name, xsink);
                if (idx < 0) { return nullptr; }
                val_indices.push_back(idx);
                val_names.push_back(name);
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
    // the names are in the default encoding, and the string cells in UTF-8
    std::vector<std::string> val_cells(val_names.size());
    for (size_t v = 0; v < val_names.size(); ++v) {
        if (dataFrameUtf8Bytes(val_names[v].data(), val_names[v].size(), QCS_DEFAULT, val_cells[v], xsink)) {
            delete df;
            return nullptr;
        }
    }
    for (int64_t r = 0; r < n_rows; ++r) {
        for (const auto& vn : val_cells) {
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
    if (qore_check_cancel(xsink, "computing row numbers")) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx);

    WindowSpec ws;
    if (!parseWindowSpec(spec, ws, col_index, xsink)) {
        return nullptr;
    }
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
    if (qore_check_cancel(xsink, "computing lag")) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx);

    int col_idx = getColIdx(column, xsink);
    if (col_idx < 0) { return nullptr; }

    WindowSpec ws;
    if (!parseWindowSpec(spec, ws, col_index, xsink)) {
        return nullptr;
    }
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
        // a negative offset looks ahead: the source row can be on either side
        if (src_pos < 0 || src_pos >= n_rows || !samePartition(columns, part_col_idx,
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
    if (qore_check_cancel(xsink, "computing lead")) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx);

    int col_idx = getColIdx(column, xsink);
    if (col_idx < 0) { return nullptr; }

    WindowSpec ws;
    if (!parseWindowSpec(spec, ws, col_index, xsink)) {
        return nullptr;
    }
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
        // a negative offset looks back: the source row can be on either side
        if (src_pos < 0 || src_pos >= n_rows || !samePartition(columns, part_col_idx,
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
    if (qore_check_cancel(xsink, "computing cumulative sum")) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx);

    int col_idx = getColIdx(column, xsink);
    if (col_idx < 0) { return nullptr; }

    WindowSpec ws;
    if (!parseWindowSpec(spec, ws, col_index, xsink)) {
        return nullptr;
    }
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
    if (qore_check_cancel(xsink, "computing rolling mean")) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx);

    int col_idx = getColIdx(column, xsink);
    if (col_idx < 0) { return nullptr; }

    WindowSpec ws;
    if (!parseWindowSpec(spec, ws, col_index, xsink)) {
        return nullptr;
    }
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
