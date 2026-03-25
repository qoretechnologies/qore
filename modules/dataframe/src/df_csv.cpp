/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    df_csv.cpp

    CSV reader/writer implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "df_csv.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace QoreDataFrameNS {

//! Parse a single CSV line into fields, handling quoted fields
static std::vector<std::string> parseCsvLine(const std::string& line, char sep,
        char quote) {
    std::vector<std::string> fields;
    std::string field;
    bool in_quote = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (in_quote) {
            if (c == quote) {
                // Check for escaped quote (doubled)
                if (i + 1 < line.size() && line[i + 1] == quote) {
                    field += quote;
                    ++i;
                } else {
                    in_quote = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == quote) {
                in_quote = true;
            } else if (c == sep) {
                fields.push_back(field);
                field.clear();
            } else {
                field += c;
            }
        }
    }
    fields.push_back(field);

    return fields;
}

//! Try to parse a string as int64; returns true on success
static bool tryParseInt(const std::string& s, int64_t& out) {
    if (s.empty()) {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    long long val = strtoll(s.c_str(), &end, 10);
    if (errno != 0 || end != s.c_str() + s.size() || end == s.c_str()) {
        return false;
    }
    out = (int64_t)val;
    return true;
}

//! Try to parse a string as double; returns true on success
static bool tryParseFloat(const std::string& s, double& out) {
    if (s.empty()) {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    double val = strtod(s.c_str(), &end);
    if (errno != 0 || end != s.c_str() + s.size() || end == s.c_str()) {
        return false;
    }
    out = val;
    return true;
}

//! Check if a string represents a null/missing value
static bool isNullString(const std::string& s, const std::string& null_string) {
    if (s.empty()) {
        return true;
    }
    if (!null_string.empty() && s == null_string) {
        return true;
    }
    if (s == "NA" || s == "N/A" || s == "null" || s == "NULL" || s == "None") {
        return true;
    }
    return false;
}

//! Infer column type from a sample of string values
static ColumnType inferCsvColumnType(const std::vector<std::string>& values,
        const std::string& null_string) {
    bool all_int = true;
    bool all_float = true;
    bool has_non_null = false;

    for (const auto& v : values) {
        if (isNullString(v, null_string)) {
            continue;
        }
        has_non_null = true;

        int64_t ival;
        if (!tryParseInt(v, ival)) {
            all_int = false;
        }
        double fval;
        if (!tryParseFloat(v, fval)) {
            all_float = false;
        }

        if (!all_int && !all_float) {
            return ColumnType::STRING;
        }
    }

    if (!has_non_null) {
        return ColumnType::FLOAT64;  // all null → default to float
    }
    if (all_int) {
        return ColumnType::INT64;
    }
    if (all_float) {
        return ColumnType::FLOAT64;
    }
    return ColumnType::STRING;
}

// Ensure Eigen VectorXd has capacity for at least n elements (doubling strategy)
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

// Helper: append a parsed field value to column storage
static void appendFieldToColumn(ColumnData& cd, const std::string& field,
        const std::string& null_string) {
    int64_t idx = cd.n_rows;
    ++cd.n_rows;
    cd.null_mask.push_back(0);

    if (isNullString(field, null_string)) {
        cd.null_mask[idx] = 1;
        switch (cd.type) {
            case ColumnType::INT64:
                cd.int_data.push_back(0);
                break;
            case ColumnType::FLOAT64:
                ensureFloatCapacity(cd.float_data, cd.n_rows);
                cd.float_data(idx) = std::numeric_limits<double>::quiet_NaN();
                break;
            case ColumnType::STRING:
                cd.str_data.emplace_back();
                break;
            case ColumnType::BOOL:
                cd.bool_data.push_back(0);
                break;
            default:
                break;
        }
        return;
    }

    switch (cd.type) {
        case ColumnType::INT64: {
            int64_t val = 0;
            tryParseInt(field, val);
            cd.int_data.push_back(val);
            break;
        }
        case ColumnType::FLOAT64: {
            double val = 0.0;
            tryParseFloat(field, val);
            ensureFloatCapacity(cd.float_data, cd.n_rows);
            cd.float_data(idx) = val;
            break;
        }
        case ColumnType::STRING:
            cd.str_data.push_back(field);
            break;
        case ColumnType::BOOL:
            cd.bool_data.push_back(
                (field == "true" || field == "True" || field == "1") ? 1 : 0);
            break;
        default:
            break;
    }
}

QoreDataFrame* QoreDataFrame::readCSV(const std::string& path, const QoreHashNode* options,
        ExceptionSink* xsink) {
    if (qore_check_cancel(xsink, "reading CSV file")) {
        return nullptr;
    }

    // Parse options
    char sep = ',';
    char quote_char = '"';
    bool has_header = true;
    std::string null_string;
    int skip_rows = 0;
    int64_t max_rows = -1;

    if (options) {
        QoreValue v = options->getKeyValue("separator");
        if (v.getType() == NT_STRING) {
            const char* s = v.get<const QoreStringNode>()->c_str();
            if (s[0]) {
                sep = s[0];
            }
        }
        v = options->getKeyValue("quote_char");
        if (v.getType() == NT_STRING) {
            const char* s = v.get<const QoreStringNode>()->c_str();
            if (s[0]) {
                quote_char = s[0];
            }
        }
        v = options->getKeyValue("header");
        if (!v.isNullOrNothing()) {
            has_header = v.getAsBool();
        }
        v = options->getKeyValue("null_string");
        if (v.getType() == NT_STRING) {
            null_string = v.get<const QoreStringNode>()->c_str();
        }
        v = options->getKeyValue("skip_rows");
        if (!v.isNullOrNothing()) {
            skip_rows = (int)v.getAsBigInt();
        }
        v = options->getKeyValue("max_rows");
        if (!v.isNullOrNothing()) {
            max_rows = v.getAsBigInt();
        }
    }

    // Open file
    std::ifstream file(path);
    if (!file.is_open()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "cannot open file '%s': %s", path.c_str(), strerror(errno));
        return nullptr;
    }

    // Skip rows
    std::string line;
    for (int i = 0; i < skip_rows; ++i) {
        if (!std::getline(file, line)) {
            break;
        }
    }

    // Read header
    std::vector<std::string> col_names;
    if (has_header) {
        if (!std::getline(file, line)) {
            return new QoreDataFrame();  // empty file
        }
        // Strip trailing \r if present (Windows line endings)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        col_names = parseCsvLine(line, sep, quote_char);
    }

    // --- Streaming two-phase approach ---
    // Phase 1: Read a sample (first 100 rows) for type inference
    static const int SAMPLE_SIZE = 100;
    std::vector<std::vector<std::string>> sample_rows;
    int64_t rows_read = 0;

    while (std::getline(file, line) && (int64_t)sample_rows.size() < SAMPLE_SIZE) {
        if (max_rows >= 0 && rows_read >= max_rows) {
            break;
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        sample_rows.push_back(parseCsvLine(line, sep, quote_char));
        ++rows_read;
    }

    if (sample_rows.empty()) {
        return new QoreDataFrame();
    }

    // Determine column count and names
    size_t num_cols = sample_rows[0].size();
    if (col_names.empty()) {
        for (size_t c = 0; c < num_cols; ++c) {
            col_names.push_back("col_" + std::to_string(c));
        }
    }
    if (num_cols > col_names.size()) {
        num_cols = col_names.size();
    }

    // Infer types from sample (transposed per-column view)
    std::vector<ColumnType> types(num_cols);
    for (size_t c = 0; c < num_cols; ++c) {
        std::vector<std::string> sample_col;
        sample_col.reserve(sample_rows.size());
        for (const auto& row : sample_rows) {
            sample_col.push_back(c < row.size() ? row[c] : "");
        }
        types[c] = inferCsvColumnType(sample_col, null_string);
    }

    // Phase 2: Create column storage and populate from sample + stream remaining
    auto* df = new QoreDataFrame();
    for (size_t c = 0; c < num_cols; ++c) {
        auto cd = std::make_shared<ColumnData>();
        cd->type = types[c];
        cd->n_rows = 0;
        // Reserve for sample rows; columns grow dynamically after
        switch (types[c]) {
            case ColumnType::INT64:
                cd->int_data.reserve(sample_rows.size());
                break;
            case ColumnType::STRING:
                cd->str_data.reserve(sample_rows.size());
                break;
            case ColumnType::BOOL:
                cd->bool_data.reserve(sample_rows.size());
                break;
            default:
                break;
        }
        Column col;
        col.name = col_names[c];
        col.data = std::move(cd);
        df->col_index[col.name] = df->columns.size();
        df->columns.push_back(std::move(col));
    }

    // Populate from sample rows
    for (const auto& row : sample_rows) {
        for (size_t c = 0; c < num_cols; ++c) {
            const std::string& field = (c < row.size()) ? row[c] : "";
            appendFieldToColumn(*df->columns[c].data, field, null_string);
        }
    }
    // Free sample memory
    sample_rows.clear();
    sample_rows.shrink_to_fit();

    // Stream remaining rows directly into columns
    while (std::getline(file, line)) {
        if (max_rows >= 0 && rows_read >= max_rows) {
            break;
        }
        // Periodic cancellation check every 10K rows
        if ((rows_read % 10000) == 0 && rows_read > 0) {
            if (qore_check_cancel(xsink, "reading CSV file")) {
                delete df;
                return nullptr;
            }
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        auto fields = parseCsvLine(line, sep, quote_char);
        for (size_t c = 0; c < num_cols; ++c) {
            const std::string& field = (c < fields.size()) ? fields[c] : "";
            appendFieldToColumn(*df->columns[c].data, field, null_string);
        }
        ++rows_read;
    }
    file.close();

    // Trim over-allocated Eigen float vectors to actual size
    for (auto& col : df->columns) {
        if (col.data->type == ColumnType::FLOAT64
                && col.data->float_data.size() > col.data->n_rows) {
            col.data->float_data.conservativeResize(col.data->n_rows);
        }
    }

    // Set final row count from first column
    df->n_rows = df->columns.empty() ? 0 : df->columns[0].data->n_rows;
    return df;
}

void QoreDataFrame::writeCSV(const std::string& path,
        const QoreHashNode* options, ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "writing CSV file")) {
        return;
    }

    // Parse options
    char sep = ',';
    if (options) {
        QoreValue v = options->getKeyValue("separator");
        if (v.getType() == NT_STRING) {
            const char* s = v.get<const QoreStringNode>()->c_str();
            if (s[0]) {
                sep = s[0];
            }
        }
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "cannot open file '%s' for writing: %s", path.c_str(), strerror(errno));
        return;
    }

    std::lock_guard<std::mutex> lk(mtx);

    // Write header
    for (size_t c = 0; c < columns.size(); ++c) {
        if (c > 0) {
            file << sep;
        }
        file << columns[c].name;
    }
    file << "\n";

    // Write rows
    for (int64_t r = 0; r < n_rows; ++r) {
        for (size_t c = 0; c < columns.size(); ++c) {
            if (c > 0) {
                file << sep;
            }
            const ColumnData& cd = *columns[c].data;
            if (cd.isNull(r)) {
                // empty for null
                continue;
            }
            switch (cd.type) {
                case ColumnType::INT64:
                    file << cd.int_data[r];
                    break;
                case ColumnType::FLOAT64:
                    file << cd.float_data(r);
                    break;
                case ColumnType::STRING: {
                    const std::string& s = cd.str_data[r];
                    if (s.find(sep) != std::string::npos
                            || s.find('"') != std::string::npos
                            || s.find('\n') != std::string::npos) {
                        file << '"';
                        for (char ch : s) {
                            if (ch == '"') {
                                file << "\"\"";
                            } else {
                                file << ch;
                            }
                        }
                        file << '"';
                    } else {
                        file << s;
                    }
                    break;
                }
                case ColumnType::BOOL:
                    file << (cd.bool_data[r] ? "true" : "false");
                    break;
                case ColumnType::DATE:
                    file << cd.date_data[r];
                    break;
                default:
                    break;
            }
        }
        file << "\n";
    }

    file.close();
}

} // namespace QoreDataFrameNS
