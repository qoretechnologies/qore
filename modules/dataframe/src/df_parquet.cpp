/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    df_parquet.cpp

    Parquet I/O via Apache Arrow

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_DataFrame.h"

#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>

#include <limits>

namespace QoreDataFrameNS {

static bool checkParquetCancel(int64_t i, const char* action, ExceptionSink* xsink) {
    return i && !(i % 100) && qore_check_cancel(xsink, action);
}

static bool checkArrowColumnStatus(const arrow::Status& status, ExceptionSink* xsink,
        const char* action, const std::string& column_name) {
    if (status.ok()) {
        return false;
    }
    xsink->raiseException("DATAFRAME-IO-ERROR", "%s for column '%s': %s",
        action, column_name.c_str(), status.ToString().c_str());
    return true;
}

template <typename ArrowArray, typename StoreValue, typename StoreNull>
static bool copyArrowChunks(const std::shared_ptr<arrow::ChunkedArray>& arr, ColumnData& cd,
        ExceptionSink* xsink, const char* action, StoreValue store_value, StoreNull store_null) {
    int64_t idx = 0;
    for (int c = 0; c < arr->num_chunks(); ++c) {
        auto chunk = std::static_pointer_cast<ArrowArray>(arr->chunk(c));
        for (int64_t i = 0; i < chunk->length(); ++i) {
            if (checkParquetCancel(idx, action, xsink)) {
                return false;
            }
            if (chunk->IsNull(i)) {
                cd.null_mask[idx] = 1;
                store_null(cd, idx);
            } else {
                store_value(cd, idx, *chunk, i);
            }
            ++idx;
        }
    }
    return true;
}

// Convert an Arrow ChunkedArray to a DataFrame Column
static std::shared_ptr<ColumnData> arrowColumnToDF(
        const std::shared_ptr<arrow::ChunkedArray>& arr, ExceptionSink* xsink) {
    auto cd = std::make_shared<ColumnData>();
    int64_t n = arr->length();
    cd->n_rows = n;
    cd->null_mask.resize(n, 0);

    // Determine type from Arrow type
    auto arrow_type = arr->type();

    if (arrow_type->id() == arrow::Type::INT64 || arrow_type->id() == arrow::Type::INT32
            || arrow_type->id() == arrow::Type::INT16 || arrow_type->id() == arrow::Type::INT8) {
        cd->type = ColumnType::INT64;
        cd->int_data.resize(n);
        auto store_null = [](ColumnData& data, int64_t idx) { data.int_data[idx] = 0; };
        bool ok = true;
        switch (arrow_type->id()) {
            case arrow::Type::INT64:
                ok = copyArrowChunks<arrow::Int64Array>(arr, *cd, xsink, "converting Arrow int64 column",
                    [](ColumnData& data, int64_t idx, const arrow::Int64Array& chunk, int64_t i) {
                        data.int_data[idx] = chunk.Value(i);
                    }, store_null);
                break;
            case arrow::Type::INT32:
                ok = copyArrowChunks<arrow::Int32Array>(arr, *cd, xsink, "converting Arrow int32 column",
                    [](ColumnData& data, int64_t idx, const arrow::Int32Array& chunk, int64_t i) {
                        data.int_data[idx] = chunk.Value(i);
                    }, store_null);
                break;
            case arrow::Type::INT16:
                ok = copyArrowChunks<arrow::Int16Array>(arr, *cd, xsink, "converting Arrow int16 column",
                    [](ColumnData& data, int64_t idx, const arrow::Int16Array& chunk, int64_t i) {
                        data.int_data[idx] = chunk.Value(i);
                    }, store_null);
                break;
            case arrow::Type::INT8:
                ok = copyArrowChunks<arrow::Int8Array>(arr, *cd, xsink, "converting Arrow int8 column",
                    [](ColumnData& data, int64_t idx, const arrow::Int8Array& chunk, int64_t i) {
                        data.int_data[idx] = chunk.Value(i);
                    }, store_null);
                break;
            default:
                break;
        }
        if (!ok) {
            return nullptr;
        }
    } else if (arrow_type->id() == arrow::Type::DOUBLE
            || arrow_type->id() == arrow::Type::FLOAT) {
        cd->type = ColumnType::FLOAT64;
        cd->float_data.resize(n);
        auto store_null = [](ColumnData& data, int64_t idx) {
            data.float_data(idx) = std::numeric_limits<double>::quiet_NaN();
        };
        bool ok = true;
        if (arrow_type->id() == arrow::Type::DOUBLE) {
            ok = copyArrowChunks<arrow::DoubleArray>(arr, *cd, xsink, "converting Arrow double column",
                [](ColumnData& data, int64_t idx, const arrow::DoubleArray& chunk, int64_t i) {
                    data.float_data(idx) = chunk.Value(i);
                }, store_null);
        } else {
            ok = copyArrowChunks<arrow::FloatArray>(arr, *cd, xsink, "converting Arrow float column",
                [](ColumnData& data, int64_t idx, const arrow::FloatArray& chunk, int64_t i) {
                    data.float_data(idx) = chunk.Value(i);
                }, store_null);
        }
        if (!ok) {
            return nullptr;
        }
    } else if (arrow_type->id() == arrow::Type::STRING
            || arrow_type->id() == arrow::Type::LARGE_STRING) {
        cd->type = ColumnType::STRING;
        cd->str_data.resize(n);
        auto store_null = [](ColumnData&, int64_t) {};
        bool ok = true;
        if (arrow_type->id() == arrow::Type::STRING) {
            ok = copyArrowChunks<arrow::StringArray>(arr, *cd, xsink, "converting Arrow string column",
                [](ColumnData& data, int64_t idx, const arrow::StringArray& chunk, int64_t i) {
                    data.str_data[idx] = chunk.GetString(i);
                }, store_null);
        } else {
            ok = copyArrowChunks<arrow::LargeStringArray>(arr, *cd, xsink, "converting Arrow large string column",
                [](ColumnData& data, int64_t idx, const arrow::LargeStringArray& chunk, int64_t i) {
                    data.str_data[idx] = chunk.GetString(i);
                }, store_null);
        }
        if (!ok) {
            return nullptr;
        }
    } else if (arrow_type->id() == arrow::Type::BOOL) {
        cd->type = ColumnType::BOOL;
        cd->bool_data.resize(n);
        if (!copyArrowChunks<arrow::BooleanArray>(arr, *cd, xsink, "converting Arrow boolean column",
                [](ColumnData& data, int64_t idx, const arrow::BooleanArray& chunk, int64_t i) {
                    data.bool_data[idx] = chunk.Value(i) ? 1 : 0;
                }, [](ColumnData& data, int64_t idx) { data.bool_data[idx] = 0; })) {
            return nullptr;
        }
    } else if (arrow_type->id() == arrow::Type::TIMESTAMP) {
        cd->type = ColumnType::DATE;
        cd->date_data.resize(n);
        auto ts_type = std::static_pointer_cast<arrow::TimestampType>(arrow_type);
        if (!copyArrowChunks<arrow::TimestampArray>(arr, *cd, xsink, "converting Arrow timestamp column",
                [ts_type](ColumnData& data, int64_t idx, const arrow::TimestampArray& chunk, int64_t i) {
                    int64_t val = chunk.Value(i);
                    switch (ts_type->unit()) {
                        case arrow::TimeUnit::SECOND: val *= 1000000; break;
                        case arrow::TimeUnit::MILLI:  val *= 1000; break;
                        case arrow::TimeUnit::MICRO:  break;
                        case arrow::TimeUnit::NANO:   val /= 1000; break;
                    }
                    data.date_data[idx] = val;
                }, [](ColumnData& data, int64_t idx) { data.date_data[idx] = 0; })) {
            return nullptr;
        }
    } else {
        // Fallback: convert everything to string
        cd->type = ColumnType::STRING;
        cd->str_data.resize(n);
        int64_t idx = 0;
        for (int c = 0; c < arr->num_chunks(); ++c) {
            auto chunk = arr->chunk(c);
            for (int64_t i = 0; i < chunk->length(); ++i) {
                if (checkParquetCancel(idx, "converting Arrow fallback column", xsink)) {
                    return nullptr;
                }
                if (chunk->IsNull(i)) {
                    cd->null_mask[idx] = 1;
                } else {
                    auto scalar_result = chunk->GetScalar(i);
                    if (!scalar_result.ok()) {
                        xsink->raiseException("DATAFRAME-IO-ERROR",
                            "error reading Arrow scalar: %s", scalar_result.status().ToString().c_str());
                        return nullptr;
                    }
                    cd->str_data[idx] = scalar_result.ValueOrDie()->ToString();
                }
                ++idx;
            }
        }
    }

    return cd;
}

QoreDataFrame* QoreDataFrame::readParquet(const std::string& path,
        ExceptionSink* xsink) {
    if (qore_check_cancel(xsink, "reading Parquet file")) {
        return nullptr;
    }
    arrow::MemoryPool* pool = arrow::system_memory_pool();

    // Open file
    auto maybe_infile = arrow::io::ReadableFile::Open(path, pool);
    if (!maybe_infile.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "cannot open Parquet file '%s': %s", path.c_str(),
            maybe_infile.status().ToString().c_str());
        return nullptr;
    }
    auto infile = maybe_infile.ValueOrDie();

    // Open Parquet reader via FileReaderBuilder
    std::unique_ptr<parquet::arrow::FileReader> reader;
    auto st_builder = parquet::arrow::FileReaderBuilder();
    st_builder.memory_pool(pool);
    auto status = st_builder.Open(infile);
    if (!status.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "cannot open Parquet reader for '%s': %s", path.c_str(),
            status.ToString().c_str());
        return nullptr;
    }
    status = st_builder.Build(&reader);
    if (!status.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "cannot build Parquet reader for '%s': %s", path.c_str(),
            status.ToString().c_str());
        return nullptr;
    }

    // Read entire file into Arrow Table
    std::shared_ptr<arrow::Table> table;
    status = reader->ReadTable(&table);
    if (!status.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "error reading Parquet table from '%s': %s", path.c_str(),
            status.ToString().c_str());
        return nullptr;
    }

    // Convert Arrow Table to DataFrame
    auto* df = new QoreDataFrame();
    df->n_rows = table->num_rows();

    for (int c = 0; c < table->num_columns(); ++c) {
        auto arrow_col = table->column(c);
        std::string col_name = table->schema()->field(c)->name();

        auto cd = arrowColumnToDF(arrow_col, xsink);
        if (*xsink) {
            delete df;
            return nullptr;
        }

        Column col;
        col.name = col_name;
        col.data = std::move(cd);
        df->col_index[col.name] = df->columns.size();
        df->columns.push_back(std::move(col));
    }

    return df;
}

void QoreDataFrame::writeParquet(const std::string& path,
        ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "writing Parquet file")) {
        return;
    }
    arrow::MemoryPool* pool = arrow::system_memory_pool();

    std::lock_guard<std::mutex> lk(mtx);

    // Build Arrow schema and arrays
    std::vector<std::shared_ptr<arrow::Field>> fields;
    std::vector<std::shared_ptr<arrow::Array>> arrays;

    for (const auto& col : columns) {
        const ColumnData& cd = *col.data;

        switch (cd.type) {
            case ColumnType::INT64: {
                fields.push_back(arrow::field(col.name, arrow::int64()));
                arrow::Int64Builder builder(pool);
                if (checkArrowColumnStatus(builder.Reserve(cd.n_rows), xsink,
                        "error reserving Arrow int64 array", col.name)) {
                    return;
                }
                for (int64_t i = 0; i < cd.n_rows; ++i) {
                    if (checkParquetCancel(i, "building Arrow int64 array", xsink)) {
                        return;
                    }
                    auto st = cd.isNull(i) ? builder.AppendNull() : builder.Append(cd.int_data[i]);
                    if (checkArrowColumnStatus(st, xsink, "error appending Arrow int64 value", col.name)) {
                        return;
                    }
                }
                std::shared_ptr<arrow::Array> arr;
                auto st = builder.Finish(&arr);
                if (!st.ok()) {
                    xsink->raiseException("DATAFRAME-IO-ERROR",
                        "error building Arrow int64 array for column '%s': %s",
                        col.name.c_str(), st.ToString().c_str());
                    return;
                }
                arrays.push_back(arr);
                break;
            }
            case ColumnType::FLOAT64: {
                fields.push_back(arrow::field(col.name, arrow::float64()));
                arrow::DoubleBuilder builder(pool);
                if (checkArrowColumnStatus(builder.Reserve(cd.n_rows), xsink,
                        "error reserving Arrow float64 array", col.name)) {
                    return;
                }
                for (int64_t i = 0; i < cd.n_rows; ++i) {
                    if (checkParquetCancel(i, "building Arrow float64 array", xsink)) {
                        return;
                    }
                    auto st = cd.isNull(i) ? builder.AppendNull() : builder.Append(cd.float_data(i));
                    if (checkArrowColumnStatus(st, xsink, "error appending Arrow float64 value", col.name)) {
                        return;
                    }
                }
                std::shared_ptr<arrow::Array> arr;
                auto st = builder.Finish(&arr);
                if (!st.ok()) {
                    xsink->raiseException("DATAFRAME-IO-ERROR",
                        "error building Arrow float64 array for column '%s': %s",
                        col.name.c_str(), st.ToString().c_str());
                    return;
                }
                arrays.push_back(arr);
                break;
            }
            case ColumnType::STRING: {
                fields.push_back(arrow::field(col.name, arrow::utf8()));
                arrow::StringBuilder builder(pool);
                if (checkArrowColumnStatus(builder.Reserve(cd.n_rows), xsink,
                        "error reserving Arrow string array", col.name)) {
                    return;
                }
                for (int64_t i = 0; i < cd.n_rows; ++i) {
                    if (checkParquetCancel(i, "building Arrow string array", xsink)) {
                        return;
                    }
                    auto st = cd.isNull(i) ? builder.AppendNull() : builder.Append(cd.str_data[i]);
                    if (checkArrowColumnStatus(st, xsink, "error appending Arrow string value", col.name)) {
                        return;
                    }
                }
                std::shared_ptr<arrow::Array> arr;
                auto st = builder.Finish(&arr);
                if (!st.ok()) {
                    xsink->raiseException("DATAFRAME-IO-ERROR",
                        "error building Arrow string array for column '%s': %s",
                        col.name.c_str(), st.ToString().c_str());
                    return;
                }
                arrays.push_back(arr);
                break;
            }
            case ColumnType::BOOL: {
                fields.push_back(arrow::field(col.name, arrow::boolean()));
                arrow::BooleanBuilder builder(pool);
                if (checkArrowColumnStatus(builder.Reserve(cd.n_rows), xsink,
                        "error reserving Arrow boolean array", col.name)) {
                    return;
                }
                for (int64_t i = 0; i < cd.n_rows; ++i) {
                    if (checkParquetCancel(i, "building Arrow boolean array", xsink)) {
                        return;
                    }
                    auto st = cd.isNull(i) ? builder.AppendNull() : builder.Append(cd.bool_data[i] != 0);
                    if (checkArrowColumnStatus(st, xsink, "error appending Arrow boolean value", col.name)) {
                        return;
                    }
                }
                std::shared_ptr<arrow::Array> arr;
                auto st = builder.Finish(&arr);
                if (!st.ok()) {
                    xsink->raiseException("DATAFRAME-IO-ERROR",
                        "error building Arrow boolean array for column '%s': %s",
                        col.name.c_str(), st.ToString().c_str());
                    return;
                }
                arrays.push_back(arr);
                break;
            }
            case ColumnType::DATE: {
                // Store dates as int64 microseconds (Arrow TIMESTAMP)
                auto ts_type = arrow::timestamp(arrow::TimeUnit::MICRO, "UTC");
                fields.push_back(arrow::field(col.name, ts_type));
                arrow::TimestampBuilder builder(ts_type, pool);
                if (checkArrowColumnStatus(builder.Reserve(cd.n_rows), xsink,
                        "error reserving Arrow timestamp array", col.name)) {
                    return;
                }
                for (int64_t i = 0; i < cd.n_rows; ++i) {
                    if (checkParquetCancel(i, "building Arrow timestamp array", xsink)) {
                        return;
                    }
                    auto st = cd.isNull(i) ? builder.AppendNull() : builder.Append(cd.date_data[i]);
                    if (checkArrowColumnStatus(st, xsink, "error appending Arrow timestamp value", col.name)) {
                        return;
                    }
                }
                std::shared_ptr<arrow::Array> arr;
                auto st = builder.Finish(&arr);
                if (!st.ok()) {
                    xsink->raiseException("DATAFRAME-IO-ERROR",
                        "error building Arrow timestamp array for column '%s': %s",
                        col.name.c_str(), st.ToString().c_str());
                    return;
                }
                arrays.push_back(arr);
                break;
            }
            default:
                // Skip AUTO columns (unsupported in Parquet)
                break;
        }
    }

    auto schema = arrow::schema(fields);
    auto table = arrow::Table::Make(schema, arrays);

    // Open output file
    auto maybe_outfile = arrow::io::FileOutputStream::Open(path);
    if (!maybe_outfile.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "cannot open Parquet file for writing '%s': %s", path.c_str(),
            maybe_outfile.status().ToString().c_str());
        return;
    }
    auto outfile = maybe_outfile.ValueOrDie();

    // Write table
    auto status = parquet::arrow::WriteTable(*table, pool, outfile, n_rows > 0 ? n_rows : 1024);
    if (!status.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "error writing Parquet file '%s': %s", path.c_str(),
            status.ToString().c_str());
    }
}

} // namespace QoreDataFrameNS
