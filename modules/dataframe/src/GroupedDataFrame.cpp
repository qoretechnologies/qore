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

QoreGroupedDataFrame::~QoreGroupedDataFrame() {
    // the object's destructor detaches the GroupedDataFrame; private data released without it still releases its
    // claims
    if (attached) {
        releaseScanColumns(source_columns);
    }
}

void QoreGroupedDataFrame::attach(QoreObject& self) {
    // the columns replaced by copies are released after the locks
    std::vector<std::shared_ptr<ColumnData>> replaced;
    std::lock_guard<std::mutex> lk(mtx);
    std::lock_guard<std::mutex> slk(scan_mtx);
    assert(!attached);
    scan_columns = claimScanColumns(source_columns, replaced);
    attached = true;
    if (scan_columns) {
        // the object's count follows the GroupedDataFrame's under its lock (see AbstractScannablePrivateData)
        self.addPrivateDataScan(CID_GROUPEDDATAFRAME);
        markScanColumnValuesStored(self, source_columns);
    }
}

void QoreGroupedDataFrame::destructor(QoreObject& self, ExceptionSink* xsink) {
    // the columns are released after the locks: releasing a value can run a destructor that uses this object
    std::vector<Column> old_columns;
    std::lock_guard<std::mutex> lk(mtx);
    std::lock_guard<std::mutex> slk(scan_mtx);
    if (attached) {
        if (scan_columns) {
            // the values are given up; the object's recursive set counted their references as internal
            self.privateDataValuesRemoved();
            self.removePrivateDataScan(CID_GROUPEDDATAFRAME);
            scan_columns = 0;
        }
        releaseScanColumns(source_columns);
        attached = false;
    }
    old_columns.swap(source_columns);
    source_col_index.clear();
    groups.clear();
    source_n_rows = 0;
}

bool QoreGroupedDataFrame::scanValues(QorePrivateDataScanner& scanner) {
    // the scan never blocks (see AbstractScannablePrivateData::scanValues())
    std::unique_lock<std::mutex> slk(scan_mtx, std::try_to_lock);
    if (!slk.owns_lock() || !scan_columns) {
        return false;
    }
    return scanColumnValues(source_columns, scanner);
}

void setGroupedDataFramePrivate(QoreObject& obj, QoreGroupedDataFrame* gdf) {
    assert(gdf);
    obj.setPrivate(CID_GROUPEDDATAFRAME, gdf);
    gdf->attach(obj);
}

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

    // returns the value of a group column as a string; null values are distinguished from strings by the key
    // components (see appendDataFrameKeyNull())
    auto get_key_value = [](const ColumnData& cd, int64_t row) -> std::string {
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
                return dataFrameStdString(*sh);
            }
            default:
                return {};
        }
    };

    for (int64_t r = 0; r < source_n_rows; ++r) {
        if (r && !(r % 100) && qore_check_cancel(xsink, "building grouped DataFrame keys")) {
            return;
        }
        // Build the key from the group column values; each value is a tagged component with its length, so that
        // different value tuples always make different keys
        std::string key;
        for (const auto& gc : group_cols) {
            size_t ci = src_col_index.at(gc);
            const ColumnData& cd = *src_columns[ci].data;
            if (cd.isNull(r)) {
                appendDataFrameKeyNull(key);
            } else {
                appendDataFrameKeyValue(key, get_key_value(cd, r));
            }
        }

        auto inserted = group_map.emplace(std::move(key), groups.size());
        if (inserted.second) {
            std::vector<std::string> key_values;
            key_values.reserve(group_cols.size());
            for (const auto& gc : group_cols) {
                size_t ci = src_col_index.at(gc);
                const ColumnData& cd = *src_columns[ci].data;
                key_values.push_back(cd.isNull(r) ? std::string() : get_key_value(cd, r));
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

namespace {
// raises the exception for an aggregation that cannot be applied to a column; the column name is kept whole
void raiseAggregationError(ExceptionSink* xsink, const char* err, const std::string& func, const std::string& column,
        const char* reason) {
    QoreStringNodeHolder desc(new QoreStringNode("aggregation '"));
    desc->concat(func.data(), func.size());
    desc->concat("' of column '");
    desc->concat(column.data(), column.size());
    desc->sprintf("' %s", reason);
    xsink->raiseException(err, desc.release());
}

// a column with one null value for each group, of the type and schema of the source column
std::shared_ptr<ColumnData> makeGroupValueColumn(const ColumnData& src, int64_t n_groups,
        const std::string& result_name) {
    auto cd = std::make_shared<ColumnData>();
    cd->type = src.type;
    cd->n_rows = n_groups;
    cd->null_mask.assign(static_cast<size_t>(n_groups), 1);
    if (src.has_columnar_schema) {
        cd->columnar_schema = src.columnar_schema;
        cd->columnar_schema.name = result_name;
        cd->has_columnar_schema = true;
    }
    switch (src.type) {
        case ColumnType::FLOAT64:
            cd->float_data = Eigen::VectorXd::Zero(n_groups);
            break;
        case ColumnType::INT64:
            cd->int_data.resize(n_groups);
            break;
        case ColumnType::STRING:
            cd->str_data.resize(n_groups);
            break;
        case ColumnType::BOOL:
            cd->bool_data.resize(n_groups);
            break;
        case ColumnType::DATE:
            cd->date_data.resize(n_groups);
            break;
        case ColumnType::AUTO:
            cd->auto_data.resize(n_groups);
            break;
    }
    return cd;
}

// sets the value of a group to the value of a row of the source column, which is not null
void setGroupValue(ColumnData& dest, int64_t g, const ColumnData& src, int64_t r, ExceptionSink* xsink) {
    assert(!src.isNull(r));
    dest.null_mask[g] = 0;
    switch (src.type) {
        case ColumnType::FLOAT64:
            dest.float_data(g) = src.float_data(r);
            break;
        case ColumnType::INT64:
            dest.int_data[g] = src.int_data[r];
            break;
        case ColumnType::STRING:
            dest.str_data[g] = src.str_data[r];
            break;
        case ColumnType::BOOL:
            dest.bool_data[g] = src.bool_data[r];
            break;
        case ColumnType::DATE:
            dest.date_data[g] = src.date_data[r];
            break;
        case ColumnType::AUTO:
            dest.setAutoValue(g, src.auto_data[r], xsink);
            break;
    }
}

// returns true if the value of row a is less than that of row b in a string, date or bool column
bool groupValueLess(const ColumnData& cd, int64_t a, int64_t b) {
    switch (cd.type) {
        case ColumnType::STRING:
            return cd.str_data[a] < cd.str_data[b];
        case ColumnType::DATE:
            return cd.date_data[a] < cd.date_data[b];
        case ColumnType::BOOL:
            return cd.bool_data[a] < cd.bool_data[b];
        default:
            assert(false);
            return false;
    }
}
}

std::shared_ptr<ColumnData> QoreGroupedDataFrame::aggregateColumn(const ColumnData& src_cd,
        const std::string& col_name, const std::string& func, const std::string& result_name,
        ExceptionSink* xsink) const {
    int64_t n_groups = static_cast<int64_t>(groups.size());
    bool numeric = src_cd.type == ColumnType::FLOAT64 || src_cd.type == ColumnType::INT64;
    // the rows processed, for the cancellation checks
    int64_t work = 0;
    auto cancelled = [&work, xsink]() -> bool {
        return !(++work % 100) && qore_check_cancel(xsink, "aggregating grouped DataFrame columns");
    };

    // the values of any type that are counted
    if (func == "count" || func == "nunique") {
        auto cd = std::make_shared<ColumnData>();
        cd->type = ColumnType::INT64;
        cd->n_rows = n_groups;
        cd->null_mask.resize(n_groups, 0);
        cd->int_data.resize(n_groups);
        for (int64_t g = 0; g < n_groups; ++g) {
            int64_t count = 0;
            std::unordered_set<std::string> seen;
            for (int64_t r : groups[g].row_indices) {
                if (cancelled()) {
                    return nullptr;
                }
                if (src_cd.isNull(r)) {
                    continue;
                }
                if (func == "count") {
                    ++count;
                } else {
                    seen.insert(dataFrameValueKey(src_cd, r));
                }
            }
            cd->int_data[g] = func == "count" ? count : static_cast<int64_t>(seen.size());
        }
        return cd;
    }

    // a value of each group, of any type
    if (func == "first" || func == "last") {
        std::shared_ptr<ColumnData> cd = makeGroupValueColumn(src_cd, n_groups, result_name);
        for (int64_t g = 0; g < n_groups; ++g) {
            int64_t found = -1;
            for (int64_t r : groups[g].row_indices) {
                if (cancelled()) {
                    return nullptr;
                }
                if (!src_cd.isNull(r)) {
                    found = r;
                    if (func == "first") {
                        break;
                    }
                }
            }
            if (found >= 0) {
                setGroupValue(*cd, g, src_cd, found, xsink);
                if (*xsink) {
                    return nullptr;
                }
            }
        }
        return cd;
    }

    // the smallest or largest value of a string, date or bool column, of the type of the column; numeric columns are
    // aggregated as floating point numbers below
    if ((func == "min" || func == "max") && (src_cd.type == ColumnType::STRING || src_cd.type == ColumnType::DATE
            || src_cd.type == ColumnType::BOOL)) {
        bool is_min = func == "min";
        std::shared_ptr<ColumnData> cd = makeGroupValueColumn(src_cd, n_groups, result_name);
        for (int64_t g = 0; g < n_groups; ++g) {
            int64_t best = -1;
            for (int64_t r : groups[g].row_indices) {
                if (cancelled()) {
                    return nullptr;
                }
                if (src_cd.isNull(r)) {
                    continue;
                }
                if (best < 0 || (is_min ? groupValueLess(src_cd, r, best) : groupValueLess(src_cd, best, r))) {
                    best = r;
                }
            }
            if (best >= 0) {
                setGroupValue(*cd, g, src_cd, best, xsink);
            }
        }
        return cd;
    }

    if (func != "sum" && func != "mean" && func != "min" && func != "max" && func != "std" && func != "median") {
        raiseAggregationError(xsink, "DATAFRAME-ERROR", func, col_name, "is unknown; expecting one of: \"sum\", "
            "\"mean\", \"min\", \"max\", \"std\", \"median\", \"count\", \"nunique\", \"first\", \"last\"");
        return nullptr;
    }
    if (!numeric) {
        raiseAggregationError(xsink, "DATAFRAME-COLUMN-ERROR", func, col_name,
            (std::string("cannot be applied to a column of type ") + columnTypeName(src_cd.type)).c_str());
        return nullptr;
    }

    // numeric aggregations: a float64 column
    auto cd = std::make_shared<ColumnData>();
    cd->type = ColumnType::FLOAT64;
    cd->n_rows = n_groups;
    cd->null_mask.resize(n_groups, 0);
    cd->float_data.resize(n_groups);
    for (int64_t g = 0; g < n_groups; ++g) {
        if (g && !(g % 100) && qore_check_cancel(xsink, "aggregating grouped DataFrame columns")) {
            return nullptr;
        }
        double val = aggregateNumeric(src_cd, groups[g].row_indices, func, xsink);
        if (*xsink) {
            return nullptr;
        }
        if (std::isnan(val)) {
            cd->null_mask[g] = 1;
        }
        cd->float_data(g) = val;
    }
    return cd;
}

QoreDataFrame* QoreGroupedDataFrame::agg(const QoreHashNode* agg_spec,
        ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "aggregating grouped DataFrame")) {
        return nullptr;
    }

    std::vector<AggSpec> specs;
    specs.reserve(agg_spec->size());
    ConstHashIterator hi(agg_spec);
    while (hi.next()) {
        if (specs.size() && !(specs.size() % 100) && qore_check_cancel(xsink, "reading DataFrame aggregations")) {
            return nullptr;
        }
        AggSpec spec;
        spec.column = hi.getKey();

        // Get function name(s); a value that is not a function name is an error, never ignored
        QoreValue func_val = hi.get();
        std::string func;
        if (getDataFrameString(func_val, func)) {
            spec.funcs.push_back(func);
        } else if (func_val.getType() == NT_LIST) {
            const QoreListNode* func_list = func_val.get<const QoreListNode>();
            for (size_t i = 0; i < func_list->size(); ++i) {
                QoreValue v = func_list->retrieveEntry(i);
                if (!getDataFrameString(v, func)) {
                    xsink->raiseException("DATAFRAME-ERROR", "the aggregations of column '%s' must be strings; "
                        "element %d has type '%s'", hi.getKey(), static_cast<int>(i), v.getTypeName());
                    return nullptr;
                }
                spec.funcs.push_back(func);
            }
        } else {
            xsink->raiseException("DATAFRAME-ERROR", "the aggregation of column '%s' must be a string or a list of "
                "strings; got type '%s'", hi.getKey(), func_val.getTypeName());
            return nullptr;
        }
        specs.push_back(std::move(spec));
    }

    return aggSpecs(specs, xsink);
}

QoreDataFrame* QoreGroupedDataFrame::aggSpecs(const std::vector<AggSpec>& specs, ExceptionSink* xsink) const {
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
    for (size_t si = 0; si < specs.size(); ++si) {
        if (si && !(si % 100) && qore_check_cancel(xsink, "aggregating grouped DataFrame columns")) {
            delete df;
            return nullptr;
        }
        const std::string& col_name = specs[si].column;
        const std::vector<std::string>& funcs = specs[si].funcs;
        auto col_it = source_col_index.find(col_name);
        if (col_it == source_col_index.end()) {
            delete df;
            QoreStringNodeHolder desc(new QoreStringNode("aggregation column '"));
            desc->concat(col_name.data(), col_name.size());
            desc->concat("' not found");
            xsink->raiseException("DATAFRAME-COLUMN-ERROR", desc.release());
            return nullptr;
        }
        const ColumnData& src_cd = *source_columns[col_it->second].data;

        for (const auto& func : funcs) {
            std::string result_name = (funcs.size() > 1) ? col_name + "_" + func : col_name;
            std::shared_ptr<ColumnData> cd = aggregateColumn(src_cd, col_name, func, result_name, xsink);
            if (!cd) {
                delete df;
                return nullptr;
            }
            Column col;
            col.name = std::move(result_name);
            col.data = std::move(cd);
            df->col_index[col.name] = df->columns.size();
            df->columns.push_back(std::move(col));
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

std::vector<QoreGroupedDataFrame::AggSpec> QoreGroupedDataFrame::numericSpecs(const char* func,
        ExceptionSink* xsink) const {
    // the specs are not a hash, because a column name may contain NUL characters, which a hash key cannot
    std::vector<AggSpec> specs;
    for (size_t i = 0; i < source_columns.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "selecting numeric DataFrame columns")) {
            return {};
        }
        const Column& col = source_columns[i];
        // Skip group columns
        bool is_group = false;
        for (const auto& gc : group_col_names) {
            if (col.name == gc) {
                is_group = true;
                break;
            }
        }
        if (is_group) {
            continue;
        }
        if (col.data->type == ColumnType::FLOAT64 || col.data->type == ColumnType::INT64) {
            specs.push_back({col.name, {func}});
        }
    }
    return specs;
}

QoreDataFrame* QoreGroupedDataFrame::sum(ExceptionSink* xsink) const {
    // all numeric columns → "sum"
    std::vector<AggSpec> specs = numericSpecs("sum", xsink);
    if (*xsink) {
        return nullptr;
    }
    return aggSpecs(specs, xsink);
}

QoreDataFrame* QoreGroupedDataFrame::mean(ExceptionSink* xsink) const {
    // all numeric columns → "mean"
    std::vector<AggSpec> specs = numericSpecs("mean", xsink);
    if (*xsink) {
        return nullptr;
    }
    return aggSpecs(specs, xsink);
}

int64_t QoreGroupedDataFrame::numGroups() const {
    std::lock_guard<std::mutex> lk(mtx);
    return (int64_t)groups.size();
}

} // namespace QoreDataFrameNS
