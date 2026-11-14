/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    gen-filter-fixtures.cpp Copyright 2026 Qore Technologies, s.r.o.
    MIT License

    Generates the Parquet files with values that a DataFrame cannot write itself:
    - filter_timestamp_ns.parquet, which dataframe-filter-core-semantics.qtest reads: a timestamp column in
      nanoseconds whose cells are not whole microseconds, which the reader truncates to microseconds (qore#6061);
      Qore dates have a resolution of a microsecond
    - dense_float32.parquet, which dataframe-dense-rows.qtest reads: a float32 column with NaN and infinite values
      (qore#6092); a DataFrame writes float64 columns

    Each file has the columns "id" (int64) and "v", with one row per row group, so that a filter can skip row
    groups by their statistics.

    build: c++ -std=c++20 -o gen-filter-fixtures gen-filter-fixtures.cpp $(pkg-config --cflags --libs parquet arrow)
    usage: ./gen-filter-fixtures <output-dir>
*/

#include <arrow/api.h>
#include <arrow/io/file.h>
#include <parquet/arrow/writer.h>

#include <cmath>
#include <iostream>
#include <limits>

//! writes the given values with their row numbers in the column "id"
static int write(const std::string& path, const std::shared_ptr<arrow::Array>& value_array) {
    arrow::Int64Builder ids;
    for (int64_t i = 0; i < value_array->length(); ++i) {
        if (!ids.Append(i).ok()) {
            return 1;
        }
    }
    std::shared_ptr<arrow::Array> id_array;
    if (!ids.Finish(&id_array).ok()) {
        return 1;
    }

    auto schema = arrow::schema({arrow::field("id", arrow::int64()), arrow::field("v", value_array->type())});
    auto table = arrow::Table::Make(schema, {id_array, value_array});
    auto out = arrow::io::FileOutputStream::Open(path);
    if (!out.ok()) {
        std::cerr << out.status().ToString() << std::endl;
        return 1;
    }
    parquet::WriterProperties::Builder props;
    props.disable_dictionary()->compression(parquet::Compression::UNCOMPRESSED);
    arrow::Status status = parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), *out, 1, props.build(),
        parquet::ArrowWriterProperties::Builder().store_schema()->build());
    if (!status.ok()) {
        std::cerr << status.ToString() << std::endl;
        return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <output-dir>" << std::endl;
        return 1;
    }
    std::string dir = argv[1];

    {
        // the cells: 1.5 microseconds, 5 milliseconds and -1.5 microseconds after the epoch, and a null value
        arrow::TimestampBuilder values(arrow::timestamp(arrow::TimeUnit::NANO, "UTC"),
            arrow::default_memory_pool());
        const int64_t cells[] = {1500, 5000000, -1500};
        for (int64_t cell : cells) {
            if (!values.Append(cell).ok()) {
                return 1;
            }
        }
        std::shared_ptr<arrow::Array> value_array;
        if (!values.AppendNull().ok() || !values.Finish(&value_array).ok()
                || write(dir + "/filter_timestamp_ns.parquet", value_array)) {
            return 1;
        }
    }

    {
        // the cells: NaN, 1.5, infinity, a null value, -infinity and -2.25
        arrow::FloatBuilder values;
        if (!values.Append(std::numeric_limits<float>::quiet_NaN()).ok() || !values.Append(1.5f).ok()
                || !values.Append(std::numeric_limits<float>::infinity()).ok() || !values.AppendNull().ok()
                || !values.Append(-std::numeric_limits<float>::infinity()).ok() || !values.Append(-2.25f).ok()) {
            return 1;
        }
        std::shared_ptr<arrow::Array> value_array;
        if (!values.Finish(&value_array).ok() || write(dir + "/dense_float32.parquet", value_array)) {
            return 1;
        }
    }
    return 0;
}
