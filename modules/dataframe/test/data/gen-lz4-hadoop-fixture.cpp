/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    gen-lz4-hadoop-fixture.cpp Copyright 2026 Qore Technologies, s.r.o.
    MIT License

    Generates lz4_hadoop.parquet, which parquet-compression.qtest reads: a file whose pages use the Parquet codec
    LZ4 (the deprecated codec with the Hadoop framing, which Arrow calls LZ4_HADOOP) that other Parquet writers
    still produce; DataFrame::writeParquet() writes the codec LZ4_RAW for "lz4" (qore#6209)

    Its columns are "id" (int64: 0 to 99), "name" (string: "name-<id % 10>") and "v" (double: id * 0.5), in one
    row group.

    build: c++ -std=c++20 -o gen-lz4-hadoop-fixture gen-lz4-hadoop-fixture.cpp $(pkg-config --cflags --libs parquet arrow)
    usage: ./gen-lz4-hadoop-fixture <output-dir>
*/

#include <arrow/api.h>
#include <arrow/io/file.h>
#include <parquet/arrow/writer.h>

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <output-dir>" << std::endl;
        return 1;
    }
    arrow::Int64Builder ids;
    arrow::StringBuilder names;
    arrow::DoubleBuilder values;
    for (int64_t i = 0; i < 100; ++i) {
        if (!ids.Append(i).ok() || !names.Append("name-" + std::to_string(i % 10)).ok()
            || !values.Append(i * 0.5).ok()) {
            return 1;
        }
    }
    std::shared_ptr<arrow::Array> id_array, name_array, value_array;
    if (!ids.Finish(&id_array).ok() || !names.Finish(&name_array).ok() || !values.Finish(&value_array).ok()) {
        return 1;
    }
    auto schema = arrow::schema({arrow::field("id", arrow::int64()), arrow::field("name", arrow::utf8()),
        arrow::field("v", arrow::float64())});
    auto table = arrow::Table::Make(schema, {id_array, name_array, value_array});

    std::string path = std::string(argv[1]) + "/lz4_hadoop.parquet";
    auto out = arrow::io::FileOutputStream::Open(path);
    if (!out.ok()) {
        std::cerr << out.status().ToString() << std::endl;
        return 1;
    }
    auto props = parquet::WriterProperties::Builder().compression(arrow::Compression::LZ4_HADOOP)->build();
    arrow::Status st = parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), *out, 100, props);
    if (st.ok()) {
        st = (*out)->Close();
    }
    if (!st.ok()) {
        std::cerr << path << ": " << st.ToString() << std::endl;
        return 1;
    }
    return 0;
}
