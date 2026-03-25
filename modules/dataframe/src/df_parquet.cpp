/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    df_parquet.cpp

    Parquet I/O via Apache Arrow

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_DataFrame.h"

#ifdef HAVE_ARROW

#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>

#include <limits>

namespace QoreDataFrameNS {

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
        int64_t idx = 0;
        for (int c = 0; c < arr->num_chunks(); ++c) {
            auto chunk = arr->chunk(c);
            for (int64_t i = 0; i < chunk->length(); ++i) {
                if (chunk->IsNull(i)) {
                    cd->null_mask[idx] = 1;
                    cd->int_data[idx] = 0;
                } else {
                    auto scalar = chunk->GetScalar(i).ValueOrDie();
                    cd->int_data[idx] = std::static_pointer_cast<arrow::Int64Scalar>(
                        scalar->CastTo(arrow::int64()).ValueOrDie())->value;
                }
                ++idx;
            }
        }
    } else if (arrow_type->id() == arrow::Type::DOUBLE
            || arrow_type->id() == arrow::Type::FLOAT) {
        cd->type = ColumnType::FLOAT64;
        cd->float_data.resize(n);
        int64_t idx = 0;
        for (int c = 0; c < arr->num_chunks(); ++c) {
            auto chunk = arr->chunk(c);
            for (int64_t i = 0; i < chunk->length(); ++i) {
                if (chunk->IsNull(i)) {
                    cd->null_mask[idx] = 1;
                    cd->float_data(idx) = std::numeric_limits<double>::quiet_NaN();
                } else {
                    auto scalar = chunk->GetScalar(i).ValueOrDie();
                    cd->float_data(idx) = std::static_pointer_cast<arrow::DoubleScalar>(
                        scalar->CastTo(arrow::float64()).ValueOrDie())->value;
                }
                ++idx;
            }
        }
    } else if (arrow_type->id() == arrow::Type::STRING
            || arrow_type->id() == arrow::Type::LARGE_STRING) {
        cd->type = ColumnType::STRING;
        cd->str_data.resize(n);
        int64_t idx = 0;
        for (int c = 0; c < arr->num_chunks(); ++c) {
            auto chunk = arr->chunk(c);
            for (int64_t i = 0; i < chunk->length(); ++i) {
                if (chunk->IsNull(i)) {
                    cd->null_mask[idx] = 1;
                } else {
                    auto scalar = chunk->GetScalar(i).ValueOrDie();
                    cd->str_data[idx] = scalar->ToString();
                }
                ++idx;
            }
        }
    } else if (arrow_type->id() == arrow::Type::BOOL) {
        cd->type = ColumnType::BOOL;
        cd->bool_data.resize(n);
        int64_t idx = 0;
        for (int c = 0; c < arr->num_chunks(); ++c) {
            auto chunk = arr->chunk(c);
            for (int64_t i = 0; i < chunk->length(); ++i) {
                if (chunk->IsNull(i)) {
                    cd->null_mask[idx] = 1;
                    cd->bool_data[idx] = 0;
                } else {
                    auto scalar = chunk->GetScalar(i).ValueOrDie();
                    cd->bool_data[idx] = std::static_pointer_cast<arrow::BooleanScalar>(
                        scalar)->value ? 1 : 0;
                }
                ++idx;
            }
        }
    } else {
        // Fallback: convert everything to string
        cd->type = ColumnType::STRING;
        cd->str_data.resize(n);
        int64_t idx = 0;
        for (int c = 0; c < arr->num_chunks(); ++c) {
            auto chunk = arr->chunk(c);
            for (int64_t i = 0; i < chunk->length(); ++i) {
                if (chunk->IsNull(i)) {
                    cd->null_mask[idx] = 1;
                } else {
                    auto scalar = chunk->GetScalar(i).ValueOrDie();
                    cd->str_data[idx] = scalar->ToString();
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

    // Open file
    auto maybe_infile = arrow::io::ReadableFile::Open(path);
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

    std::lock_guard<std::mutex> lk(mtx);

    // Build Arrow schema and arrays
    std::vector<std::shared_ptr<arrow::Field>> fields;
    std::vector<std::shared_ptr<arrow::Array>> arrays;

    for (const auto& col : columns) {
        const ColumnData& cd = *col.data;

        switch (cd.type) {
            case ColumnType::INT64: {
                fields.push_back(arrow::field(col.name, arrow::int64()));
                arrow::Int64Builder builder;
                auto st = builder.Reserve(cd.n_rows);
                for (int64_t i = 0; i < cd.n_rows; ++i) {
                    if (cd.isNull(i)) {
                        st = builder.AppendNull();
                    } else {
                        st = builder.Append(cd.int_data[i]);
                    }
                }
                std::shared_ptr<arrow::Array> arr;
                st = builder.Finish(&arr);
                arrays.push_back(arr);
                break;
            }
            case ColumnType::FLOAT64: {
                fields.push_back(arrow::field(col.name, arrow::float64()));
                arrow::DoubleBuilder builder;
                auto st = builder.Reserve(cd.n_rows);
                for (int64_t i = 0; i < cd.n_rows; ++i) {
                    if (cd.isNull(i)) {
                        st = builder.AppendNull();
                    } else {
                        st = builder.Append(cd.float_data(i));
                    }
                }
                std::shared_ptr<arrow::Array> arr;
                st = builder.Finish(&arr);
                arrays.push_back(arr);
                break;
            }
            case ColumnType::STRING: {
                fields.push_back(arrow::field(col.name, arrow::utf8()));
                arrow::StringBuilder builder;
                auto st = builder.Reserve(cd.n_rows);
                for (int64_t i = 0; i < cd.n_rows; ++i) {
                    if (cd.isNull(i)) {
                        st = builder.AppendNull();
                    } else {
                        st = builder.Append(cd.str_data[i]);
                    }
                }
                std::shared_ptr<arrow::Array> arr;
                st = builder.Finish(&arr);
                arrays.push_back(arr);
                break;
            }
            case ColumnType::BOOL: {
                fields.push_back(arrow::field(col.name, arrow::boolean()));
                arrow::BooleanBuilder builder;
                auto st = builder.Reserve(cd.n_rows);
                for (int64_t i = 0; i < cd.n_rows; ++i) {
                    if (cd.isNull(i)) {
                        st = builder.AppendNull();
                    } else {
                        st = builder.Append(cd.bool_data[i] != 0);
                    }
                }
                std::shared_ptr<arrow::Array> arr;
                st = builder.Finish(&arr);
                arrays.push_back(arr);
                break;
            }
            default:
                // Skip unsupported types
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
    auto status = parquet::arrow::WriteTable(*table, arrow::default_memory_pool(),
        outfile, n_rows > 0 ? n_rows : 1024);
    if (!status.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "error writing Parquet file '%s': %s", path.c_str(),
            status.ToString().c_str());
    }
}

} // namespace QoreDataFrameNS

#else // !HAVE_ARROW

namespace QoreDataFrameNS {

QoreDataFrame* QoreDataFrame::readParquet(const std::string& path,
        ExceptionSink* xsink) {
    xsink->raiseException("MISSING-FEATURE-ERROR",
        "Parquet support requires Apache Arrow; install libarrow and libparquet "
        "and rebuild the dataframe module");
    return nullptr;
}

void QoreDataFrame::writeParquet(const std::string& path,
        ExceptionSink* xsink) const {
    xsink->raiseException("MISSING-FEATURE-ERROR",
        "Parquet support requires Apache Arrow; install libarrow and libparquet "
        "and rebuild the dataframe module");
}

} // namespace QoreDataFrameNS

#endif // HAVE_ARROW
