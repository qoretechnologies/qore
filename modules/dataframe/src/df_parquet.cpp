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
#include <arrow/ipc/api.h>
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

static QoreBufferElementType arrowTypeToBufferElementType(const std::shared_ptr<arrow::DataType>& type) {
    switch (type->id()) {
        case arrow::Type::INT8:
            return QoreBufferElementType::Int8;
        case arrow::Type::INT16:
            return QoreBufferElementType::Int16;
        case arrow::Type::INT32:
            return QoreBufferElementType::Int32;
        case arrow::Type::INT64:
            return QoreBufferElementType::Int64;
        case arrow::Type::FLOAT:
            return QoreBufferElementType::Float32;
        case arrow::Type::DOUBLE:
            return QoreBufferElementType::Float64;
        case arrow::Type::BOOL:
            return QoreBufferElementType::Bool;
        default:
            return QoreBufferElementType::Invalid;
    }
}

static QoreBufferNode* tryArrowFixedWidthColumnToDenseBuffer(
        const std::shared_ptr<arrow::ChunkedArray>& arr,
        const std::shared_ptr<arrow::Field>& field,
        ExceptionSink* xsink) {
    if (arr->num_chunks() != 1) {
        return nullptr;
    }

    QoreBufferElementType element_type = arrowTypeToBufferElementType(arr->type());
    if (element_type == QoreBufferElementType::Invalid) {
        return nullptr;
    }

    std::shared_ptr<arrow::Array> chunk = arr->chunk(0);
    std::shared_ptr<arrow::ArrayData> data = chunk->data();
    if (!data || data->buffers.size() < 2 || !data->buffers[1]) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "Arrow column '%s' has no fixed-width data buffer", field->name().c_str());
        return nullptr;
    }

    const uint8_t* validity = data->buffers[0] ? data->buffers[0]->data() : nullptr;
    const uint8_t* values = data->buffers[1]->data();
    bool nullable = field->nullable() || chunk->null_count() > 0 || validity;
    std::shared_ptr<const void> owner(chunk, static_cast<const void*>(chunk.get()));
    return QoreBufferNode::wrapExternalStorage(element_type, nullable, static_cast<size_t>(chunk->offset()),
        static_cast<size_t>(chunk->length()), values, validity, std::move(owner), chunk->null_count(), xsink);
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
        const std::shared_ptr<arrow::ChunkedArray>& arr,
        const std::shared_ptr<arrow::Field>& field,
        ExceptionSink* xsink) {
    ReferenceHolder<QoreBufferNode> dense_buffer(tryArrowFixedWidthColumnToDenseBuffer(arr, field, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    if (dense_buffer) {
        return buildColumnDataFromBuffer(*dense_buffer, xsink);
    }

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

static std::shared_ptr<arrow::Table> dataFrameToArrowTable(const std::vector<Column>& columns,
        int64_t n_rows, arrow::MemoryPool* pool, ExceptionSink* xsink) {
    std::vector<std::shared_ptr<arrow::Field>> fields;
    std::vector<std::shared_ptr<arrow::Array>> arrays;

    size_t column_index = 0;
    for (const auto& col : columns) {
        if (column_index && !(column_index % 100)
                && qore_check_cancel(xsink, "building Arrow table from DataFrame columns")) {
            return nullptr;
        }
        ++column_index;

        const ColumnData& cd = *col.data;

        switch (cd.type) {
            case ColumnType::INT64: {
                fields.push_back(arrow::field(col.name, arrow::int64()));
                arrow::Int64Builder builder(pool);
                if (checkArrowColumnStatus(builder.Reserve(cd.n_rows), xsink,
                        "error reserving Arrow int64 array", col.name)) {
                    return nullptr;
                }
                for (int64_t i = 0; i < cd.n_rows; ++i) {
                    if (checkParquetCancel(i, "building Arrow int64 array", xsink)) {
                        return nullptr;
                    }
                    auto st = cd.isNull(i) ? builder.AppendNull() : builder.Append(cd.int_data[i]);
                    if (checkArrowColumnStatus(st, xsink, "error appending Arrow int64 value", col.name)) {
                        return nullptr;
                    }
                }
                std::shared_ptr<arrow::Array> arr;
                auto st = builder.Finish(&arr);
                if (!st.ok()) {
                    xsink->raiseException("DATAFRAME-IO-ERROR",
                        "error building Arrow int64 array for column '%s': %s",
                        col.name.c_str(), st.ToString().c_str());
                    return nullptr;
                }
                arrays.push_back(arr);
                break;
            }
            case ColumnType::FLOAT64: {
                fields.push_back(arrow::field(col.name, arrow::float64()));
                arrow::DoubleBuilder builder(pool);
                if (checkArrowColumnStatus(builder.Reserve(cd.n_rows), xsink,
                        "error reserving Arrow float64 array", col.name)) {
                    return nullptr;
                }
                for (int64_t i = 0; i < cd.n_rows; ++i) {
                    if (checkParquetCancel(i, "building Arrow float64 array", xsink)) {
                        return nullptr;
                    }
                    auto st = cd.isNull(i) ? builder.AppendNull() : builder.Append(cd.float_data(i));
                    if (checkArrowColumnStatus(st, xsink, "error appending Arrow float64 value", col.name)) {
                        return nullptr;
                    }
                }
                std::shared_ptr<arrow::Array> arr;
                auto st = builder.Finish(&arr);
                if (!st.ok()) {
                    xsink->raiseException("DATAFRAME-IO-ERROR",
                        "error building Arrow float64 array for column '%s': %s",
                        col.name.c_str(), st.ToString().c_str());
                    return nullptr;
                }
                arrays.push_back(arr);
                break;
            }
            case ColumnType::STRING: {
                fields.push_back(arrow::field(col.name, arrow::utf8()));
                arrow::StringBuilder builder(pool);
                if (checkArrowColumnStatus(builder.Reserve(cd.n_rows), xsink,
                        "error reserving Arrow string array", col.name)) {
                    return nullptr;
                }
                for (int64_t i = 0; i < cd.n_rows; ++i) {
                    if (checkParquetCancel(i, "building Arrow string array", xsink)) {
                        return nullptr;
                    }
                    auto st = cd.isNull(i) ? builder.AppendNull() : builder.Append(cd.str_data[i]);
                    if (checkArrowColumnStatus(st, xsink, "error appending Arrow string value", col.name)) {
                        return nullptr;
                    }
                }
                std::shared_ptr<arrow::Array> arr;
                auto st = builder.Finish(&arr);
                if (!st.ok()) {
                    xsink->raiseException("DATAFRAME-IO-ERROR",
                        "error building Arrow string array for column '%s': %s",
                        col.name.c_str(), st.ToString().c_str());
                    return nullptr;
                }
                arrays.push_back(arr);
                break;
            }
            case ColumnType::BOOL: {
                fields.push_back(arrow::field(col.name, arrow::boolean()));
                arrow::BooleanBuilder builder(pool);
                if (checkArrowColumnStatus(builder.Reserve(cd.n_rows), xsink,
                        "error reserving Arrow boolean array", col.name)) {
                    return nullptr;
                }
                for (int64_t i = 0; i < cd.n_rows; ++i) {
                    if (checkParquetCancel(i, "building Arrow boolean array", xsink)) {
                        return nullptr;
                    }
                    auto st = cd.isNull(i) ? builder.AppendNull() : builder.Append(cd.bool_data[i] != 0);
                    if (checkArrowColumnStatus(st, xsink, "error appending Arrow boolean value", col.name)) {
                        return nullptr;
                    }
                }
                std::shared_ptr<arrow::Array> arr;
                auto st = builder.Finish(&arr);
                if (!st.ok()) {
                    xsink->raiseException("DATAFRAME-IO-ERROR",
                        "error building Arrow boolean array for column '%s': %s",
                        col.name.c_str(), st.ToString().c_str());
                    return nullptr;
                }
                arrays.push_back(arr);
                break;
            }
            case ColumnType::DATE: {
                auto ts_type = arrow::timestamp(arrow::TimeUnit::MICRO, "UTC");
                fields.push_back(arrow::field(col.name, ts_type));
                arrow::TimestampBuilder builder(ts_type, pool);
                if (checkArrowColumnStatus(builder.Reserve(cd.n_rows), xsink,
                        "error reserving Arrow timestamp array", col.name)) {
                    return nullptr;
                }
                for (int64_t i = 0; i < cd.n_rows; ++i) {
                    if (checkParquetCancel(i, "building Arrow timestamp array", xsink)) {
                        return nullptr;
                    }
                    auto st = cd.isNull(i) ? builder.AppendNull() : builder.Append(cd.date_data[i]);
                    if (checkArrowColumnStatus(st, xsink, "error appending Arrow timestamp value", col.name)) {
                        return nullptr;
                    }
                }
                std::shared_ptr<arrow::Array> arr;
                auto st = builder.Finish(&arr);
                if (!st.ok()) {
                    xsink->raiseException("DATAFRAME-IO-ERROR",
                        "error building Arrow timestamp array for column '%s': %s",
                        col.name.c_str(), st.ToString().c_str());
                    return nullptr;
                }
                arrays.push_back(arr);
                break;
            }
            default:
                break;
        }
    }

    return arrow::Table::Make(arrow::schema(fields), arrays, n_rows);
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
        if (c && !(c % 100) && qore_check_cancel(xsink, "converting Parquet table to DataFrame")) {
            delete df;
            return nullptr;
        }

        auto arrow_col = table->column(c);
        std::string col_name = table->schema()->field(c)->name();

        auto cd = arrowColumnToDF(arrow_col, table->schema()->field(c), xsink);
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

QoreDataFrame* QoreDataFrame::fromArrowIpc(const BinaryNode* data,
        ExceptionSink* xsink) {
    if (qore_check_cancel(xsink, "reading Arrow IPC data")) {
        return nullptr;
    }

    auto buffer = arrow::Buffer::Wrap(static_cast<const uint8_t*>(data->getPtr()), data->size());
    auto stream_source = std::make_shared<arrow::io::BufferReader>(buffer);
    auto maybe_stream_reader = arrow::ipc::RecordBatchStreamReader::Open(stream_source);

    std::shared_ptr<arrow::Table> table;
    if (maybe_stream_reader.ok()) {
        auto maybe_table = maybe_stream_reader.ValueOrDie()->ToTable();
        if (!maybe_table.ok()) {
            xsink->raiseException("DATAFRAME-IO-ERROR",
                "error reading Arrow IPC stream: %s", maybe_table.status().ToString().c_str());
            return nullptr;
        }
        table = maybe_table.ValueOrDie();
    } else {
        auto file_source = std::make_shared<arrow::io::BufferReader>(buffer);
        auto maybe_file_reader = arrow::ipc::RecordBatchFileReader::Open(file_source);
        if (!maybe_file_reader.ok()) {
            xsink->raiseException("DATAFRAME-IO-ERROR",
                "cannot open Arrow IPC data as stream or file; stream error: %s; file error: %s",
                maybe_stream_reader.status().ToString().c_str(),
                maybe_file_reader.status().ToString().c_str());
            return nullptr;
        }
        auto maybe_table = maybe_file_reader.ValueOrDie()->ToTable();
        if (!maybe_table.ok()) {
            xsink->raiseException("DATAFRAME-IO-ERROR",
                "error reading Arrow IPC file: %s", maybe_table.status().ToString().c_str());
            return nullptr;
        }
        table = maybe_table.ValueOrDie();
    }

    if (qore_check_cancel(xsink, "converting Arrow IPC data to DataFrame")) {
        return nullptr;
    }

    auto* df = new QoreDataFrame();
    df->n_rows = table->num_rows();

    for (int c = 0; c < table->num_columns(); ++c) {
        if (c && !(c % 100) && qore_check_cancel(xsink, "converting Arrow IPC table to DataFrame")) {
            delete df;
            return nullptr;
        }

        auto arrow_col = table->column(c);
        std::string col_name = table->schema()->field(c)->name();

        auto cd = arrowColumnToDF(arrow_col, table->schema()->field(c), xsink);
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

BinaryNode* QoreDataFrame::toArrowIpc(ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "writing Arrow IPC data")) {
        return nullptr;
    }
    arrow::MemoryPool* pool = arrow::system_memory_pool();

    std::shared_ptr<arrow::Table> table;
    {
        std::lock_guard<std::mutex> lk(mtx);
        table = dataFrameToArrowTable(columns, n_rows, pool, xsink);
        if (*xsink) {
            return nullptr;
        }
    }

    auto maybe_sink = arrow::io::BufferOutputStream::Create(4096, pool);
    if (!maybe_sink.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "cannot create Arrow IPC output buffer: %s", maybe_sink.status().ToString().c_str());
        return nullptr;
    }
    auto sink = maybe_sink.ValueOrDie();

    auto maybe_writer = arrow::ipc::MakeStreamWriter(sink.get(), table->schema());
    if (!maybe_writer.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "cannot create Arrow IPC stream writer: %s", maybe_writer.status().ToString().c_str());
        return nullptr;
    }
    auto writer = maybe_writer.ValueOrDie();

    auto status = writer->WriteTable(*table);
    if (!status.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "error writing Arrow IPC stream: %s", status.ToString().c_str());
        return nullptr;
    }

    status = writer->Close();
    if (!status.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "error closing Arrow IPC stream writer: %s", status.ToString().c_str());
        return nullptr;
    }

    auto maybe_buffer = sink->Finish();
    if (!maybe_buffer.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "error finalizing Arrow IPC output buffer: %s", maybe_buffer.status().ToString().c_str());
        return nullptr;
    }

    auto buffer = maybe_buffer.ValueOrDie();
    SimpleRefHolder<BinaryNode> out(new BinaryNode());
    if (buffer->size()) {
        out->append(buffer->data(), static_cast<size_t>(buffer->size()));
    }
    return out.release();
}

void QoreDataFrame::writeParquet(const std::string& path,
        ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "writing Parquet file")) {
        return;
    }
    arrow::MemoryPool* pool = arrow::system_memory_pool();

    std::lock_guard<std::mutex> lk(mtx);

    auto table = dataFrameToArrowTable(columns, n_rows, pool, xsink);
    if (*xsink) {
        return;
    }

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
    auto status = parquet::arrow::WriteTable(*table, pool, outfile, table->num_rows() > 0 ? table->num_rows() : 1024);
    if (!status.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "error writing Parquet file '%s': %s", path.c_str(),
            status.ToString().c_str());
    }
}

} // namespace QoreDataFrameNS
