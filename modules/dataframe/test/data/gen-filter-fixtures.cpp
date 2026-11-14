/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    gen-filter-fixtures.cpp Copyright 2026 Qore Technologies, s.r.o.
    MIT License

    Generates the Parquet file with a timestamp column in nanoseconds that dataframe-filter-core-semantics.qtest
    reads.  A DataFrame cannot write it itself: Qore dates have a resolution of a microsecond, and the cells of the
    file are not whole microseconds, which the reader truncates to microseconds (qore#6061).

    build: c++ -std=c++20 -o gen-filter-fixtures gen-filter-fixtures.cpp $(pkg-config --cflags --libs parquet arrow)
    usage: ./gen-filter-fixtures <output-dir>
*/

#include <arrow/api.h>
#include <arrow/io/file.h>
#include <parquet/arrow/writer.h>

#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <output-dir>" << std::endl;
        return 1;
    }

    // the cells: 1.5 microseconds, 5 milliseconds and -1.5 microseconds after the epoch, and a null value
    arrow::TimestampBuilder values(arrow::timestamp(arrow::TimeUnit::NANO, "UTC"), arrow::default_memory_pool());
    arrow::Int64Builder ids;
    const int64_t cells[] = {1500, 5000000, -1500};
    for (int64_t i = 0; i < 3; ++i) {
        if (!values.Append(cells[i]).ok() || !ids.Append(i).ok()) {
            return 1;
        }
    }
    if (!values.AppendNull().ok() || !ids.Append(3).ok()) {
        return 1;
    }
    std::shared_ptr<arrow::Array> id_array;
    std::shared_ptr<arrow::Array> value_array;
    if (!ids.Finish(&id_array).ok() || !values.Finish(&value_array).ok()) {
        return 1;
    }

    auto schema = arrow::schema({arrow::field("id", arrow::int64()), arrow::field("v", value_array->type())});
    auto table = arrow::Table::Make(schema, {id_array, value_array});
    auto out = arrow::io::FileOutputStream::Open(std::string(argv[1]) + "/filter_timestamp_ns.parquet");
    if (!out.ok()) {
        std::cerr << out.status().ToString() << std::endl;
        return 1;
    }
    parquet::WriterProperties::Builder props;
    props.disable_dictionary()->compression(parquet::Compression::UNCOMPRESSED);
    // one row per row group, so that a filter can skip row groups by their statistics
    arrow::Status status = parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), *out, 1, props.build(),
        parquet::ArrowWriterProperties::Builder().store_schema()->build());
    if (!status.ok()) {
        std::cerr << status.ToString() << std::endl;
        return 1;
    }
    return 0;
}
