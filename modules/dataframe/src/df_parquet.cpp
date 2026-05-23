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
#include <arrow/util/decimal.h>
#include <arrow/util/float16.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>
#include <parquet/properties.h>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <limits>
#include <set>
#include <sstream>

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

static std::shared_ptr<arrow::Buffer> wrapBinaryAsArrowBuffer(const BinaryNode* data,
        ExceptionSink* xsink) {
    assert(data);
    if (data->size() > static_cast<size_t>(std::numeric_limits<int64_t>::max())) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "Arrow IPC input is too large: " QLLD " bytes exceeds the maximum supported size",
            static_cast<int64>(data->size()));
        return nullptr;
    }

    data->ref();
    std::shared_ptr<const BinaryNode> owner(data,
        [](const BinaryNode* owner) { const_cast<BinaryNode*>(owner)->deref(nullptr); });
    return std::shared_ptr<arrow::Buffer>(
        new arrow::Buffer(static_cast<const uint8_t*>(data->getPtr()), static_cast<int64_t>(data->size())),
        [owner = std::move(owner)](arrow::Buffer* buffer) {
            (void)owner;
            delete buffer;
        });
}

static size_t qoreDataFrameBitmapBytes(int64_t length, size_t bit_offset = 0) {
    return (bit_offset + static_cast<size_t>(length) + 7) / 8;
}

static std::shared_ptr<const QoreBufferNode> refQoreBufferOwner(const QoreBufferNode* buffer) {
    assert(buffer);
    buffer->ref();
    return std::shared_ptr<const QoreBufferNode>(buffer,
        [](const QoreBufferNode* owner) { const_cast<QoreBufferNode*>(owner)->deref(nullptr); });
}

static std::shared_ptr<arrow::Buffer> wrapQoreBufferMemory(const uint8_t* data, int64_t size,
        std::shared_ptr<const QoreBufferNode> owner) {
    if (!data || !size) {
        return nullptr;
    }
    return std::shared_ptr<arrow::Buffer>(new arrow::Buffer(data, size),
        [owner = std::move(owner)](arrow::Buffer* buffer) {
            (void)owner;
            delete buffer;
        });
}

struct QoreParquetReadOptions {
    std::vector<std::string> columns;
    std::vector<int> row_groups;
    int64_t batch_size = 0;
    bool has_columns = false;
    bool has_row_groups = false;
    bool has_batch_size = false;
    bool has_use_threads = false;
    bool use_threads = false;
};

struct QoreParquetWriteOptions {
    int64_t row_group_size = 0;
    parquet::Compression::type compression = parquet::Compression::UNCOMPRESSED;
    bool has_row_group_size = false;
    bool has_compression = false;
    bool has_use_dictionary = false;
    bool use_dictionary = false;
    bool has_write_statistics = false;
    bool write_statistics = false;
    bool has_use_threads = false;
    bool use_threads = false;
    bool store_schema = false;
};

static std::string qoreParquetLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

static std::string qoreParquetStringValue(QoreValue value) {
    QoreStringValueHelper str(value);
    return str->c_str();
}

static bool qoreParquetReadStringListOption(const QoreHashNode* options, const char* key,
        std::vector<std::string>& out, bool& has_option, ExceptionSink* xsink) {
    QoreValue value = options->getKeyValue(key);
    if (value.isNullOrNothing()) {
        return true;
    }

    has_option = true;
    if (value.getType() == NT_STRING) {
        out.push_back(qoreParquetStringValue(value));
        return true;
    }
    if (value.getType() != NT_LIST) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "Parquet option '%s' expects a string or list of strings, got %s",
            key, value.getFullTypeName());
        return false;
    }

    const QoreListNode* list = value.get<const QoreListNode>();
    for (size_t i = 0; i < list->size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "parsing Parquet string option list")) {
            return false;
        }
        QoreValue entry = list->retrieveEntry(i);
        if (entry.isNullOrNothing() || entry.getType() != NT_STRING) {
            xsink->raiseException("DATAFRAME-IO-ERROR",
                "Parquet option '%s' entry " QLLD " expects a string, got %s",
                key, static_cast<int64>(i), entry.getFullTypeName());
            return false;
        }
        out.push_back(qoreParquetStringValue(entry));
    }
    return true;
}

static bool qoreParquetReadIntListOption(const QoreHashNode* options, const char* key,
        std::vector<int>& out, bool& has_option, ExceptionSink* xsink) {
    QoreValue value = options->getKeyValue(key);
    if (value.isNullOrNothing()) {
        return true;
    }

    has_option = true;
    if (value.getType() != NT_LIST) {
        int64_t v = value.getAsBigInt();
        if (v < 0 || v > std::numeric_limits<int>::max()) {
            xsink->raiseException("DATAFRAME-IO-ERROR",
                "Parquet option '%s' row group " QLLD " is outside the supported range",
                key, v);
            return false;
        }
        out.push_back(static_cast<int>(v));
        return true;
    }

    const QoreListNode* list = value.get<const QoreListNode>();
    for (size_t i = 0; i < list->size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "parsing Parquet row-group option list")) {
            return false;
        }
        QoreValue entry = list->retrieveEntry(i);
        int64_t v = entry.getAsBigInt();
        if (v < 0 || v > std::numeric_limits<int>::max()) {
            xsink->raiseException("DATAFRAME-IO-ERROR",
                "Parquet option '%s' entry " QLLD " row group " QLLD " is outside the supported range",
                key, static_cast<int64>(i), v);
            return false;
        }
        out.push_back(static_cast<int>(v));
    }
    return true;
}

static bool qoreParquetReadOptions(const QoreHashNode* options, QoreParquetReadOptions& parsed,
        ExceptionSink* xsink) {
    if (!options) {
        return true;
    }
    if (!qoreParquetReadStringListOption(options, "columns", parsed.columns, parsed.has_columns, xsink)
            || !qoreParquetReadIntListOption(options, "row_groups", parsed.row_groups, parsed.has_row_groups, xsink)) {
        return false;
    }

    QoreValue batch_size = options->getKeyValue("batch_size");
    if (!batch_size.isNullOrNothing()) {
        parsed.batch_size = batch_size.getAsBigInt();
        parsed.has_batch_size = true;
        if (parsed.batch_size <= 0) {
            xsink->raiseException("DATAFRAME-IO-ERROR",
                "Parquet option 'batch_size' must be greater than zero; got " QLLD,
                parsed.batch_size);
            return false;
        }
    }

    QoreValue use_threads = options->getKeyValue("use_threads");
    if (!use_threads.isNullOrNothing()) {
        parsed.use_threads = use_threads.getAsBool();
        parsed.has_use_threads = true;
    }
    return true;
}

static bool qoreParquetCompressionFromName(const std::string& name,
        parquet::Compression::type& compression) {
    std::string lower = qoreParquetLower(name);
    if (lower == "none" || lower == "uncompressed") {
        compression = parquet::Compression::UNCOMPRESSED;
    } else if (lower == "snappy") {
        compression = parquet::Compression::SNAPPY;
    } else if (lower == "gzip" || lower == "gz") {
        compression = parquet::Compression::GZIP;
    } else if (lower == "brotli" || lower == "br") {
        compression = parquet::Compression::BROTLI;
    } else if (lower == "zstd") {
        compression = parquet::Compression::ZSTD;
    } else if (lower == "lz4") {
        compression = parquet::Compression::LZ4;
    } else if (lower == "lz4_frame" || lower == "lz4-frame") {
        compression = parquet::Compression::LZ4_FRAME;
    } else {
        return false;
    }
    return true;
}

static bool qoreParquetWriteOptions(const QoreHashNode* options, QoreParquetWriteOptions& parsed,
        ExceptionSink* xsink) {
    if (!options) {
        return true;
    }

    QoreValue row_group_size = options->getKeyValue("row_group_size");
    if (!row_group_size.isNullOrNothing()) {
        parsed.row_group_size = row_group_size.getAsBigInt();
        parsed.has_row_group_size = true;
        if (parsed.row_group_size <= 0) {
            xsink->raiseException("DATAFRAME-IO-ERROR",
                "Parquet option 'row_group_size' must be greater than zero; got " QLLD,
                parsed.row_group_size);
            return false;
        }
    }

    QoreValue compression = options->getKeyValue("compression");
    if (!compression.isNullOrNothing()) {
        std::string codec = qoreParquetStringValue(compression);
        parsed.has_compression = true;
        if (!qoreParquetCompressionFromName(codec, parsed.compression)) {
            xsink->raiseException("DATAFRAME-IO-ERROR",
                "unsupported Parquet compression '%s'; expected one of: none, snappy, gzip, brotli, zstd, lz4, lz4_frame",
                codec.c_str());
            return false;
        }
    }

    QoreValue use_dictionary = options->getKeyValue("use_dictionary");
    if (!use_dictionary.isNullOrNothing()) {
        parsed.use_dictionary = use_dictionary.getAsBool();
        parsed.has_use_dictionary = true;
    }

    QoreValue write_statistics = options->getKeyValue("write_statistics");
    if (!write_statistics.isNullOrNothing()) {
        parsed.write_statistics = write_statistics.getAsBool();
        parsed.has_write_statistics = true;
    }

    QoreValue use_threads = options->getKeyValue("use_threads");
    if (!use_threads.isNullOrNothing()) {
        parsed.use_threads = use_threads.getAsBool();
        parsed.has_use_threads = true;
    }

    QoreValue store_schema = options->getKeyValue("store_schema");
    if (!store_schema.isNullOrNothing()) {
        parsed.store_schema = store_schema.getAsBool();
    }
    return true;
}

static std::string qoreParquetTopLevelName(const std::string& path) {
    size_t dot = path.find('.');
    return dot == std::string::npos ? path : path.substr(0, dot);
}

static std::vector<int> qoreParquetColumnIndicesForNames(
        const std::unique_ptr<parquet::arrow::FileReader>& reader,
        const std::vector<std::string>& names, ExceptionSink* xsink) {
    std::vector<int> indices;
    std::set<std::string> available;
    const parquet::SchemaDescriptor* schema = reader->parquet_reader()->metadata()->schema();

    for (int i = 0; i < schema->num_columns(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "collecting Parquet column names")) {
            return {};
        }
        const parquet::ColumnDescriptor* column = schema->Column(i);
        available.insert(qoreParquetTopLevelName(column->path()->ToDotString()));
    }

    for (size_t n = 0; n < names.size(); ++n) {
        if (n && !(n % 100) && qore_check_cancel(xsink, "resolving Parquet projected columns")) {
            return {};
        }
        const std::string& name = names[n];
        bool found = false;
        for (int i = 0; i < schema->num_columns(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "matching Parquet projected columns")) {
                return {};
            }
            const parquet::ColumnDescriptor* column = schema->Column(i);
            if (qoreParquetTopLevelName(column->path()->ToDotString()) == name) {
                found = true;
                if (std::find(indices.begin(), indices.end(), i) == indices.end()) {
                    indices.push_back(i);
                }
            }
        }
        if (!found) {
            std::ostringstream available_names;
            size_t i = 0;
            for (const auto& value : available) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "formatting Parquet column names")) {
                    return {};
                }
                if (i++) {
                    available_names << ", ";
                }
                available_names << value;
            }
            xsink->raiseException("DATAFRAME-IO-ERROR",
                "Parquet file has no top-level column '%s'; available columns: %s",
                name.c_str(), available_names.str().c_str());
            return {};
        }
    }
    return indices;
}

static std::shared_ptr<arrow::Table> qoreParquetReorderTopLevelColumns(
        const std::shared_ptr<arrow::Table>& table, const std::vector<std::string>& names,
        ExceptionSink* xsink) {
    if (names.empty()) {
        return table;
    }

    std::vector<int> indices;
    indices.reserve(names.size());
    for (size_t i = 0; i < names.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "ordering Parquet projected columns")) {
            return nullptr;
        }
        int index = table->schema()->GetFieldIndex(names[i]);
        if (index < 0) {
            xsink->raiseException("DATAFRAME-IO-ERROR",
                "Parquet projection did not produce top-level column '%s'",
                names[i].c_str());
            return nullptr;
        }
        if (std::find(indices.begin(), indices.end(), index) == indices.end()) {
            indices.push_back(index);
        }
    }

    auto projected = table->SelectColumns(indices);
    if (!projected.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "error ordering Parquet projected columns: %s",
            projected.status().ToString().c_str());
        return nullptr;
    }
    return projected.ValueOrDie();
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
        case arrow::Type::DECIMAL128:
            return QoreBufferElementType::Decimal128;
        default:
            return QoreBufferElementType::Invalid;
    }
}

static std::string arrowTimeUnitName(arrow::TimeUnit::type unit) {
    switch (unit) {
        case arrow::TimeUnit::SECOND:
            return "s";
        case arrow::TimeUnit::MILLI:
            return "ms";
        case arrow::TimeUnit::MICRO:
            return "us";
        case arrow::TimeUnit::NANO:
            return "ns";
    }
    return "us";
}

static arrow::TimeUnit::type arrowTimeUnitFromName(const std::string& unit) {
    if (unit == "s" || unit == "second") {
        return arrow::TimeUnit::SECOND;
    }
    if (unit == "ms" || unit == "milli") {
        return arrow::TimeUnit::MILLI;
    }
    if (unit == "ns" || unit == "nano") {
        return arrow::TimeUnit::NANO;
    }
    return arrow::TimeUnit::MICRO;
}

static std::shared_ptr<arrow::DataType> bufferElementTypeToArrowType(QoreBufferElementType type) {
    switch (type) {
        case QoreBufferElementType::Int8:
            return arrow::int8();
        case QoreBufferElementType::Int16:
            return arrow::int16();
        case QoreBufferElementType::Int32:
            return arrow::int32();
        case QoreBufferElementType::Int64:
            return arrow::int64();
        case QoreBufferElementType::Float32:
            return arrow::float32();
        case QoreBufferElementType::Float64:
            return arrow::float64();
        case QoreBufferElementType::Bool:
            return arrow::boolean();
        case QoreBufferElementType::String:
            return arrow::utf8();
        case QoreBufferElementType::Invalid:
        default:
            return nullptr;
    }
}

static QoreColumnarTypeDescriptor arrowTypeToColumnarDescriptor(const std::string& name,
        const std::shared_ptr<arrow::DataType>& type, bool nullable, ExceptionSink* xsink) {
    QoreColumnarTypeDescriptor desc;
    desc.name = name;
    desc.nullable = nullable;
    desc.native_type = type->ToString();
    desc.buffer_type = arrowTypeToBufferElementType(type);

    switch (type->id()) {
        case arrow::Type::BOOL:
            desc.kind = QoreColumnarTypeKind::Bool;
            desc.column_type = QoreColumnarColumnType::Bool;
            break;
        case arrow::Type::INT8:
        case arrow::Type::INT16:
        case arrow::Type::INT32:
        case arrow::Type::INT64:
        case arrow::Type::UINT8:
        case arrow::Type::UINT16:
        case arrow::Type::UINT32:
        case arrow::Type::UINT64:
            desc.kind = QoreColumnarTypeKind::Int;
            desc.column_type = QoreColumnarColumnType::Int;
            break;
        case arrow::Type::HALF_FLOAT:
        case arrow::Type::FLOAT:
        case arrow::Type::DOUBLE:
            desc.kind = QoreColumnarTypeKind::Float;
            desc.column_type = QoreColumnarColumnType::Float;
            break;
        case arrow::Type::STRING:
        case arrow::Type::LARGE_STRING:
            desc.kind = QoreColumnarTypeKind::String;
            desc.column_type = QoreColumnarColumnType::String;
            break;
        case arrow::Type::BINARY:
        case arrow::Type::LARGE_BINARY:
        case arrow::Type::FIXED_SIZE_BINARY:
            desc.kind = QoreColumnarTypeKind::Binary;
            desc.column_type = QoreColumnarColumnType::Binary;
            if (type->id() == arrow::Type::FIXED_SIZE_BINARY) {
                desc.fixed_size = std::static_pointer_cast<arrow::FixedSizeBinaryType>(type)->byte_width();
            }
            break;
        case arrow::Type::DATE32:
        case arrow::Type::DATE64:
            desc.kind = QoreColumnarTypeKind::Date;
            desc.column_type = QoreColumnarColumnType::Date;
            break;
        case arrow::Type::TIMESTAMP: {
            auto ts_type = std::static_pointer_cast<arrow::TimestampType>(type);
            desc.kind = QoreColumnarTypeKind::Timestamp;
            desc.column_type = QoreColumnarColumnType::Date;
            desc.time_unit = arrowTimeUnitName(ts_type->unit());
            desc.timezone = ts_type->timezone();
            break;
        }
        case arrow::Type::TIME32:
            desc.kind = QoreColumnarTypeKind::Date;
            desc.column_type = QoreColumnarColumnType::Date;
            desc.time_unit = arrowTimeUnitName(std::static_pointer_cast<arrow::Time32Type>(type)->unit());
            break;
        case arrow::Type::TIME64:
            desc.kind = QoreColumnarTypeKind::Date;
            desc.column_type = QoreColumnarColumnType::Date;
            desc.time_unit = arrowTimeUnitName(std::static_pointer_cast<arrow::Time64Type>(type)->unit());
            break;
        case arrow::Type::DURATION: {
            auto dur_type = std::static_pointer_cast<arrow::DurationType>(type);
            desc.kind = QoreColumnarTypeKind::Duration;
            desc.column_type = QoreColumnarColumnType::Date;
            desc.time_unit = arrowTimeUnitName(dur_type->unit());
            break;
        }
        case arrow::Type::DECIMAL128: {
            auto dec_type = std::static_pointer_cast<arrow::Decimal128Type>(type);
            desc.kind = QoreColumnarTypeKind::Decimal128;
            desc.column_type = QoreColumnarColumnType::Number;
            desc.buffer_type = QoreBufferElementType::Decimal128;
            desc.precision = dec_type->precision();
            desc.scale = dec_type->scale();
            break;
        }
        case arrow::Type::DECIMAL256: {
            auto dec_type = std::static_pointer_cast<arrow::Decimal256Type>(type);
            desc.kind = QoreColumnarTypeKind::Number;
            desc.column_type = QoreColumnarColumnType::Number;
            desc.precision = dec_type->precision();
            desc.scale = dec_type->scale();
            break;
        }
        case arrow::Type::LIST:
            desc.kind = QoreColumnarTypeKind::List;
            desc.column_type = QoreColumnarColumnType::Auto;
            break;
        case arrow::Type::LARGE_LIST:
            desc.kind = QoreColumnarTypeKind::LargeList;
            desc.column_type = QoreColumnarColumnType::Auto;
            break;
        case arrow::Type::FIXED_SIZE_LIST:
            desc.kind = QoreColumnarTypeKind::FixedSizeList;
            desc.column_type = QoreColumnarColumnType::Auto;
            desc.fixed_size = std::static_pointer_cast<arrow::FixedSizeListType>(type)->list_size();
            break;
        case arrow::Type::STRUCT:
            desc.kind = QoreColumnarTypeKind::Struct;
            desc.column_type = QoreColumnarColumnType::Auto;
            break;
        case arrow::Type::MAP: {
            auto map_type = std::static_pointer_cast<arrow::MapType>(type);
            desc.kind = QoreColumnarTypeKind::Map;
            desc.column_type = QoreColumnarColumnType::Auto;
            desc.children.push_back(arrowTypeToColumnarDescriptor("key", map_type->key_type(), false, xsink));
            desc.children.push_back(arrowTypeToColumnarDescriptor("item", map_type->item_type(), true, xsink));
            return desc;
        }
        case arrow::Type::DICTIONARY: {
            auto dict_type = std::static_pointer_cast<arrow::DictionaryType>(type);
            desc.kind = QoreColumnarTypeKind::Dictionary;
            desc.column_type = QoreColumnarColumnType::Auto;
            desc.dictionary_index_type = dict_type->index_type()->ToString();
            desc.children.push_back(arrowTypeToColumnarDescriptor("dictionary", dict_type->value_type(), true,
                xsink));
            return desc;
        }
        case arrow::Type::NA:
        default:
            desc.kind = QoreColumnarTypeKind::Auto;
            desc.column_type = QoreColumnarColumnType::Auto;
            break;
    }

    for (int i = 0; i < type->num_fields(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame Arrow schema metadata")) {
            return desc;
        }
        auto child = type->field(i);
        desc.children.push_back(arrowTypeToColumnarDescriptor(child->name(), child->type(), child->nullable(),
            xsink));
    }
    return desc;
}

static QoreColumnarTypeDescriptor arrowFieldToColumnarDescriptor(const std::shared_ptr<arrow::Field>& field,
        ExceptionSink* xsink) {
    return arrowTypeToColumnarDescriptor(field->name(), field->type(), field->nullable(), xsink);
}

static void setArrowColumnarSchema(ColumnData& data, const std::shared_ptr<arrow::Field>& field,
        ExceptionSink* xsink) {
    data.columnar_schema = arrowFieldToColumnarDescriptor(field, xsink);
    data.columnar_schema.name = field->name();
    data.has_columnar_schema = !*xsink;
}

static bool arrowTypeIsNestedOrDictionary(const std::shared_ptr<arrow::DataType>& type) {
    switch (type->id()) {
        case arrow::Type::LIST:
        case arrow::Type::LARGE_LIST:
        case arrow::Type::FIXED_SIZE_LIST:
        case arrow::Type::STRUCT:
        case arrow::Type::MAP:
        case arrow::Type::DICTIONARY:
            return true;
        default:
            return false;
    }
}

static std::shared_ptr<arrow::DataType> columnarDescriptorToArrowType(
        const QoreColumnarTypeDescriptor& desc, ExceptionSink* xsink);

static std::shared_ptr<arrow::Field> columnarDescriptorToArrowField(
        const QoreColumnarTypeDescriptor& desc, const std::string& fallback_name, ExceptionSink* xsink) {
    std::string name = desc.name.empty() ? fallback_name : desc.name;
    auto type = columnarDescriptorToArrowType(desc, xsink);
    if (!type) {
        return nullptr;
    }
    return arrow::field(name, type, desc.nullable);
}

static arrow::FieldVector columnarChildrenToArrowFields(const QoreColumnarTypeDescriptor& desc,
        ExceptionSink* xsink) {
    arrow::FieldVector fields;
    fields.reserve(desc.children.size());
    for (size_t i = 0; i < desc.children.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "building Arrow schema children")) {
            return {};
        }
        auto child = columnarDescriptorToArrowField(desc.children[i],
            desc.children[i].name.empty() ? "item" : desc.children[i].name, xsink);
        if (!child) {
            return {};
        }
        fields.push_back(child);
    }
    return fields;
}

static std::shared_ptr<arrow::DataType> columnarDescriptorToArrowType(
        const QoreColumnarTypeDescriptor& desc, ExceptionSink* xsink) {
    if (desc.buffer_type != QoreBufferElementType::Invalid) {
        auto type = bufferElementTypeToArrowType(desc.buffer_type);
        if (type) {
            return type;
        }
    }

    switch (desc.kind) {
        case QoreColumnarTypeKind::Bool:
            return arrow::boolean();
        case QoreColumnarTypeKind::Int:
            return arrow::int64();
        case QoreColumnarTypeKind::Float:
            return arrow::float64();
        case QoreColumnarTypeKind::Number:
            return arrow::decimal128(desc.precision > 0 ? desc.precision : 38,
                desc.precision > 0 ? desc.scale : 10);
        case QoreColumnarTypeKind::String:
            return arrow::utf8();
        case QoreColumnarTypeKind::Date:
        case QoreColumnarTypeKind::Timestamp:
            return arrow::timestamp(arrowTimeUnitFromName(desc.time_unit), desc.timezone);
        case QoreColumnarTypeKind::Duration:
            return arrow::duration(arrowTimeUnitFromName(desc.time_unit));
        case QoreColumnarTypeKind::Decimal128:
            return arrow::decimal128(desc.precision > 0 ? desc.precision : 38, desc.scale);
        case QoreColumnarTypeKind::Binary:
            return desc.fixed_size > 0 ? arrow::fixed_size_binary(desc.fixed_size) : arrow::binary();
        case QoreColumnarTypeKind::List: {
            auto child = desc.children.empty()
                ? arrow::field("item", arrow::utf8(), true)
                : columnarDescriptorToArrowField(desc.children[0], "item", xsink);
            return child ? arrow::list(child) : nullptr;
        }
        case QoreColumnarTypeKind::LargeList: {
            auto child = desc.children.empty()
                ? arrow::field("item", arrow::utf8(), true)
                : columnarDescriptorToArrowField(desc.children[0], "item", xsink);
            return child ? arrow::large_list(child) : nullptr;
        }
        case QoreColumnarTypeKind::FixedSizeList: {
            if (desc.fixed_size <= 0) {
                xsink->raiseException("DATAFRAME-IO-ERROR",
                    "fixed_size_list column '%s' requires a positive fixed_size", desc.name.c_str());
                return nullptr;
            }
            auto child = desc.children.empty()
                ? arrow::field("item", arrow::utf8(), true)
                : columnarDescriptorToArrowField(desc.children[0], "item", xsink);
            return child ? arrow::fixed_size_list(child, desc.fixed_size) : nullptr;
        }
        case QoreColumnarTypeKind::Struct: {
            auto fields = columnarChildrenToArrowFields(desc, xsink);
            if (*xsink) {
                return nullptr;
            }
            return arrow::struct_(fields);
        }
        case QoreColumnarTypeKind::Map: {
            auto key_type = desc.children.size() >= 1 ? columnarDescriptorToArrowType(desc.children[0], xsink)
                : arrow::utf8();
            if (!key_type || *xsink) {
                return nullptr;
            }
            auto item_type = desc.children.size() >= 2 ? columnarDescriptorToArrowType(desc.children[1], xsink)
                : arrow::utf8();
            if (!item_type || *xsink) {
                return nullptr;
            }
            return arrow::map(key_type, item_type);
        }
        case QoreColumnarTypeKind::Dictionary:
            if (!desc.children.empty()) {
                return columnarDescriptorToArrowType(desc.children[0], xsink);
            }
            return arrow::utf8();
        case QoreColumnarTypeKind::Auto:
        default:
            break;
    }

    switch (desc.column_type) {
        case QoreColumnarColumnType::Bool:
            return arrow::boolean();
        case QoreColumnarColumnType::Int:
            return arrow::int64();
        case QoreColumnarColumnType::Float:
            return arrow::float64();
        case QoreColumnarColumnType::Number:
            return arrow::decimal128(desc.precision > 0 ? desc.precision : 38,
                desc.precision > 0 ? desc.scale : 10);
        case QoreColumnarColumnType::String:
            return arrow::utf8();
        case QoreColumnarColumnType::Date:
            return arrow::timestamp(arrow::TimeUnit::MICRO);
        case QoreColumnarColumnType::Binary:
            return arrow::binary();
        case QoreColumnarColumnType::Auto:
        default:
            return arrow::utf8();
    }
}

static QoreValue arrowScalarToQore(const std::shared_ptr<arrow::Array>& array,
        int64_t index, ExceptionSink* xsink) {
    if (array->IsNull(index)) {
        return QoreValue();
    }

    switch (array->type_id()) {
        case arrow::Type::BOOL:
            return std::static_pointer_cast<arrow::BooleanArray>(array)->Value(index);
        case arrow::Type::INT8:
            return static_cast<int64_t>(std::static_pointer_cast<arrow::Int8Array>(array)->Value(index));
        case arrow::Type::INT16:
            return static_cast<int64_t>(std::static_pointer_cast<arrow::Int16Array>(array)->Value(index));
        case arrow::Type::INT32:
            return static_cast<int64_t>(std::static_pointer_cast<arrow::Int32Array>(array)->Value(index));
        case arrow::Type::INT64:
            return std::static_pointer_cast<arrow::Int64Array>(array)->Value(index);
        case arrow::Type::UINT8:
            return static_cast<int64_t>(std::static_pointer_cast<arrow::UInt8Array>(array)->Value(index));
        case arrow::Type::UINT16:
            return static_cast<int64_t>(std::static_pointer_cast<arrow::UInt16Array>(array)->Value(index));
        case arrow::Type::UINT32:
            return static_cast<int64_t>(std::static_pointer_cast<arrow::UInt32Array>(array)->Value(index));
        case arrow::Type::UINT64: {
            uint64_t v = std::static_pointer_cast<arrow::UInt64Array>(array)->Value(index);
            if (v > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                return new QoreNumberNode(std::to_string(v).c_str());
            }
            return static_cast<int64_t>(v);
        }
        case arrow::Type::HALF_FLOAT: {
            uint16_t raw = std::static_pointer_cast<arrow::HalfFloatArray>(array)->Value(index);
            return static_cast<double>(arrow::util::Float16::FromBits(raw).ToFloat());
        }
        case arrow::Type::FLOAT:
            return static_cast<double>(std::static_pointer_cast<arrow::FloatArray>(array)->Value(index));
        case arrow::Type::DOUBLE:
            return std::static_pointer_cast<arrow::DoubleArray>(array)->Value(index);
        case arrow::Type::STRING: {
            auto str_array = std::static_pointer_cast<arrow::StringArray>(array);
            auto val = str_array->GetView(index);
            return new QoreStringNode(val.data(), val.size(), QCS_UTF8);
        }
        case arrow::Type::LARGE_STRING: {
            auto str_array = std::static_pointer_cast<arrow::LargeStringArray>(array);
            auto val = str_array->GetView(index);
            return new QoreStringNode(val.data(), val.size(), QCS_UTF8);
        }
        case arrow::Type::BINARY: {
            auto bin_array = std::static_pointer_cast<arrow::BinaryArray>(array);
            auto val = bin_array->GetView(index);
            SimpleRefHolder<BinaryNode> bin(new BinaryNode);
            bin->append(val.data(), val.size());
            return bin.release();
        }
        case arrow::Type::LARGE_BINARY: {
            auto bin_array = std::static_pointer_cast<arrow::LargeBinaryArray>(array);
            auto val = bin_array->GetView(index);
            SimpleRefHolder<BinaryNode> bin(new BinaryNode);
            bin->append(val.data(), val.size());
            return bin.release();
        }
        case arrow::Type::FIXED_SIZE_BINARY: {
            auto bin_array = std::static_pointer_cast<arrow::FixedSizeBinaryArray>(array);
            auto val = bin_array->GetView(index);
            SimpleRefHolder<BinaryNode> bin(new BinaryNode);
            bin->append(val.data(), val.size());
            return bin.release();
        }
        case arrow::Type::DATE32: {
            int32_t days = std::static_pointer_cast<arrow::Date32Array>(array)->Value(index);
            return DateTimeNode::makeAbsolute(currentTZ(), static_cast<int64_t>(days) * 86400, 0);
        }
        case arrow::Type::DATE64: {
            int64_t ms = std::static_pointer_cast<arrow::Date64Array>(array)->Value(index);
            return DateTimeNode::makeAbsolute(currentTZ(), ms / 1000, (ms % 1000) * 1000);
        }
        case arrow::Type::TIMESTAMP: {
            auto ts_array = std::static_pointer_cast<arrow::TimestampArray>(array);
            auto ts_type = std::static_pointer_cast<arrow::TimestampType>(array->type());
            int64_t val = ts_array->Value(index);
            const AbstractQoreZoneInfo* zone = currentTZ();
            if (!ts_type->timezone().empty()) {
                zone = find_create_timezone(ts_type->timezone().c_str(), xsink);
                if (*xsink) {
                    return QoreValue();
                }
            }
            int64_t secs = 0;
            int us = 0;
            switch (ts_type->unit()) {
                case arrow::TimeUnit::SECOND:
                    secs = val;
                    break;
                case arrow::TimeUnit::MILLI:
                    secs = val / 1000;
                    us = (val % 1000) * 1000;
                    break;
                case arrow::TimeUnit::MICRO:
                    secs = val / 1000000;
                    us = val % 1000000;
                    break;
                case arrow::TimeUnit::NANO:
                    secs = val / 1000000000;
                    us = (val % 1000000000) / 1000;
                    break;
            }
            return DateTimeNode::makeAbsolute(zone, secs, us);
        }
        case arrow::Type::DURATION: {
            auto dur_array = std::static_pointer_cast<arrow::DurationArray>(array);
            auto dur_type = std::static_pointer_cast<arrow::DurationType>(array->type());
            int64_t val = dur_array->Value(index);
            int64_t secs = 0;
            int us = 0;
            switch (dur_type->unit()) {
                case arrow::TimeUnit::SECOND:
                    secs = val;
                    break;
                case arrow::TimeUnit::MILLI:
                    secs = val / 1000;
                    us = (val % 1000) * 1000;
                    break;
                case arrow::TimeUnit::MICRO:
                    secs = val / 1000000;
                    us = val % 1000000;
                    break;
                case arrow::TimeUnit::NANO:
                    secs = val / 1000000000;
                    us = (val % 1000000000) / 1000;
                    break;
            }
            return DateTimeNode::makeRelative(0, 0, 0, 0, 0, secs, us);
        }
        case arrow::Type::DECIMAL128: {
            auto dec_array = std::static_pointer_cast<arrow::Decimal128Array>(array);
            auto dec_type = std::static_pointer_cast<arrow::Decimal128Type>(array->type());
            arrow::Decimal128 dec_val(dec_array->GetValue(index));
            return new QoreNumberNode(dec_val.ToString(dec_type->scale()).c_str());
        }
        case arrow::Type::DECIMAL256: {
            auto dec_array = std::static_pointer_cast<arrow::Decimal256Array>(array);
            auto dec_type = std::static_pointer_cast<arrow::Decimal256Type>(array->type());
            arrow::Decimal256 dec_val(dec_array->GetValue(index));
            return new QoreNumberNode(dec_val.ToString(dec_type->scale()).c_str());
        }
        case arrow::Type::STRUCT: {
            auto struct_array = std::static_pointer_cast<arrow::StructArray>(array);
            auto struct_type = struct_array->type();
            ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), xsink);
            for (int i = 0; i < struct_type->num_fields(); ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "converting Arrow struct value")) {
                    return QoreValue();
                }
                QoreValue child_val = arrowScalarToQore(struct_array->field(i), index, xsink);
                if (*xsink) {
                    return QoreValue();
                }
                hash->setKeyValue(struct_type->field(i)->name(), child_val, xsink);
            }
            return hash.release();
        }
        case arrow::Type::LIST: {
            auto list_array = std::static_pointer_cast<arrow::ListArray>(array);
            auto values = list_array->values();
            int64_t start = list_array->value_offset(index);
            int64_t length = list_array->value_length(index);
            ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);
            for (int64_t i = 0; i < length; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "converting Arrow list value")) {
                    return QoreValue();
                }
                QoreValue elem = arrowScalarToQore(values, start + i, xsink);
                if (*xsink) {
                    return QoreValue();
                }
                list->push(elem, xsink);
            }
            return list.release();
        }
        case arrow::Type::LARGE_LIST: {
            auto list_array = std::static_pointer_cast<arrow::LargeListArray>(array);
            auto values = list_array->values();
            int64_t start = list_array->value_offset(index);
            int64_t length = list_array->value_length(index);
            ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);
            for (int64_t i = 0; i < length; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "converting Arrow large_list value")) {
                    return QoreValue();
                }
                QoreValue elem = arrowScalarToQore(values, start + i, xsink);
                if (*xsink) {
                    return QoreValue();
                }
                list->push(elem, xsink);
            }
            return list.release();
        }
        case arrow::Type::FIXED_SIZE_LIST: {
            auto list_array = std::static_pointer_cast<arrow::FixedSizeListArray>(array);
            auto values = list_array->values();
            int64_t start = list_array->value_offset(index);
            int64_t length = list_array->value_length(index);
            ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);
            for (int64_t i = 0; i < length; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "converting Arrow fixed_size_list value")) {
                    return QoreValue();
                }
                QoreValue elem = arrowScalarToQore(values, start + i, xsink);
                if (*xsink) {
                    return QoreValue();
                }
                list->push(elem, xsink);
            }
            return list.release();
        }
        case arrow::Type::MAP: {
            auto map_array = std::static_pointer_cast<arrow::MapArray>(array);
            auto keys = map_array->keys();
            auto items = map_array->items();
            int64_t start = map_array->value_offset(index);
            int64_t length = map_array->value_length(index);
            ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), xsink);
            for (int64_t i = 0; i < length; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "converting Arrow map value")) {
                    return QoreValue();
                }
                ValueHolder key_val(arrowScalarToQore(keys, start + i, xsink), xsink);
                if (*xsink) {
                    return QoreValue();
                }
                QoreStringValueHelper key_str(*key_val);
                QoreValue item_val = arrowScalarToQore(items, start + i, xsink);
                if (*xsink) {
                    return QoreValue();
                }
                hash->setKeyValue(key_str->c_str(), item_val, xsink);
            }
            return hash.release();
        }
        case arrow::Type::DICTIONARY: {
            auto dict_array = std::static_pointer_cast<arrow::DictionaryArray>(array);
            ValueHolder idx_val(arrowScalarToQore(dict_array->indices(), index, xsink), xsink);
            if (*xsink || idx_val->isNothing()) {
                return QoreValue();
            }
            int64_t dict_index = idx_val->getAsBigInt();
            auto dictionary = dict_array->dictionary();
            if (dict_index < 0 || dict_index >= dictionary->length()) {
                xsink->raiseException("DATAFRAME-IO-ERROR",
                    "Arrow dictionary index " QLLD " out of range (0.." QLLD ")",
                    dict_index, dictionary->length() - 1);
                return QoreValue();
            }
            return arrowScalarToQore(dictionary, dict_index, xsink);
        }
        case arrow::Type::NA:
            return QoreValue();
        default:
            break;
    }

    auto scalar_result = array->GetScalar(index);
    if (!scalar_result.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "error reading Arrow scalar: %s", scalar_result.status().ToString().c_str());
        return QoreValue();
    }
    return new QoreStringNode(scalar_result.ValueOrDie()->ToString());
}

static bool appendToArrowBuilder(arrow::ArrayBuilder* builder,
        const std::shared_ptr<arrow::DataType>& type, QoreValue val, ExceptionSink* xsink) {
    if (val.isNullOrNothing()) {
        auto status = builder->AppendNull();
        if (!status.ok()) {
            xsink->raiseException("DATAFRAME-IO-ERROR",
                "failed to append Arrow null: %s", status.ToString().c_str());
            return false;
        }
        return true;
    }

    arrow::Status status;
    switch (type->id()) {
        case arrow::Type::BOOL:
            status = static_cast<arrow::BooleanBuilder*>(builder)->Append(val.getAsBool());
            break;
        case arrow::Type::INT8:
            status = static_cast<arrow::Int8Builder*>(builder)->Append(static_cast<int8_t>(val.getAsBigInt()));
            break;
        case arrow::Type::INT16:
            status = static_cast<arrow::Int16Builder*>(builder)->Append(static_cast<int16_t>(val.getAsBigInt()));
            break;
        case arrow::Type::INT32:
            status = static_cast<arrow::Int32Builder*>(builder)->Append(static_cast<int32_t>(val.getAsBigInt()));
            break;
        case arrow::Type::INT64:
            status = static_cast<arrow::Int64Builder*>(builder)->Append(val.getAsBigInt());
            break;
        case arrow::Type::UINT8:
            status = static_cast<arrow::UInt8Builder*>(builder)->Append(static_cast<uint8_t>(val.getAsBigInt()));
            break;
        case arrow::Type::UINT16:
            status = static_cast<arrow::UInt16Builder*>(builder)->Append(static_cast<uint16_t>(val.getAsBigInt()));
            break;
        case arrow::Type::UINT32:
            status = static_cast<arrow::UInt32Builder*>(builder)->Append(static_cast<uint32_t>(val.getAsBigInt()));
            break;
        case arrow::Type::UINT64: {
            uint64_t uv = 0;
            if (val.getType() == NT_NUMBER) {
                QoreStringValueHelper str(val);
                uv = strtoull(str->c_str(), nullptr, 10);
            } else {
                uv = static_cast<uint64_t>(val.getAsBigInt());
            }
            status = static_cast<arrow::UInt64Builder*>(builder)->Append(uv);
            break;
        }
        case arrow::Type::HALF_FLOAT:
            status = static_cast<arrow::HalfFloatBuilder*>(builder)->Append(
                arrow::util::Float16::FromFloat(static_cast<float>(val.getAsFloat())).bits());
            break;
        case arrow::Type::FLOAT:
            status = static_cast<arrow::FloatBuilder*>(builder)->Append(static_cast<float>(val.getAsFloat()));
            break;
        case arrow::Type::DOUBLE:
            status = static_cast<arrow::DoubleBuilder*>(builder)->Append(val.getAsFloat());
            break;
        case arrow::Type::STRING: {
            QoreStringValueHelper str(val);
            status = static_cast<arrow::StringBuilder*>(builder)->Append(str->c_str(), str->size());
            break;
        }
        case arrow::Type::LARGE_STRING: {
            QoreStringValueHelper str(val);
            status = static_cast<arrow::LargeStringBuilder*>(builder)->Append(str->c_str(), str->size());
            break;
        }
        case arrow::Type::BINARY: {
            const BinaryNode* bin = val.get<BinaryNode>();
            status = bin ? static_cast<arrow::BinaryBuilder*>(builder)->Append(
                static_cast<const uint8_t*>(bin->getPtr()), bin->size()) : builder->AppendNull();
            break;
        }
        case arrow::Type::LARGE_BINARY: {
            const BinaryNode* bin = val.get<BinaryNode>();
            status = bin ? static_cast<arrow::LargeBinaryBuilder*>(builder)->Append(
                static_cast<const uint8_t*>(bin->getPtr()), bin->size()) : builder->AppendNull();
            break;
        }
        case arrow::Type::FIXED_SIZE_BINARY: {
            const BinaryNode* bin = val.get<BinaryNode>();
            if (!bin) {
                status = builder->AppendNull();
                break;
            }
            auto fsb_type = std::static_pointer_cast<arrow::FixedSizeBinaryType>(type);
            if (static_cast<int>(bin->size()) != fsb_type->byte_width()) {
                xsink->raiseException("DATAFRAME-IO-ERROR",
                    "fixed_size_binary expects %d bytes, got %d",
                    fsb_type->byte_width(), static_cast<int>(bin->size()));
                return false;
            }
            status = static_cast<arrow::FixedSizeBinaryBuilder*>(builder)->Append(
                static_cast<const uint8_t*>(bin->getPtr()));
            break;
        }
        case arrow::Type::DATE32:
        case arrow::Type::DATE64:
        case arrow::Type::TIMESTAMP: {
            const DateTimeNode* dt = val.get<DateTimeNode>();
            if (!dt) {
                status = builder->AppendNull();
                break;
            }
            int64_t epoch = dt->getEpochSecondsUTC();
            int64_t us = dt->getMicrosecond();
            if (type->id() == arrow::Type::DATE32) {
                status = static_cast<arrow::Date32Builder*>(builder)->Append(static_cast<int32_t>(epoch / 86400));
            } else if (type->id() == arrow::Type::DATE64) {
                status = static_cast<arrow::Date64Builder*>(builder)->Append(epoch * 1000 + us / 1000);
            } else {
                auto ts_type = std::static_pointer_cast<arrow::TimestampType>(type);
                int64_t ts_val = 0;
                switch (ts_type->unit()) {
                    case arrow::TimeUnit::SECOND:
                        ts_val = epoch;
                        break;
                    case arrow::TimeUnit::MILLI:
                        ts_val = epoch * 1000 + us / 1000;
                        break;
                    case arrow::TimeUnit::MICRO:
                        ts_val = epoch * 1000000 + us;
                        break;
                    case arrow::TimeUnit::NANO:
                        ts_val = epoch * 1000000000 + us * 1000;
                        break;
                }
                status = static_cast<arrow::TimestampBuilder*>(builder)->Append(ts_val);
            }
            break;
        }
        case arrow::Type::DURATION: {
            const DateTimeNode* dt = val.get<DateTimeNode>();
            if (!dt) {
                status = builder->AppendNull();
                break;
            }
            auto dur_type = std::static_pointer_cast<arrow::DurationType>(type);
            int64_t secs = dt->getRelativeSeconds();
            int64_t us = dt->getRelativeMicroseconds();
            int64_t dv = 0;
            switch (dur_type->unit()) {
                case arrow::TimeUnit::SECOND:
                    dv = secs;
                    break;
                case arrow::TimeUnit::MILLI:
                    dv = secs * 1000 + us / 1000;
                    break;
                case arrow::TimeUnit::MICRO:
                    dv = secs * 1000000 + us;
                    break;
                case arrow::TimeUnit::NANO:
                    dv = secs * 1000000000 + us * 1000;
                    break;
            }
            status = static_cast<arrow::DurationBuilder*>(builder)->Append(dv);
            break;
        }
        case arrow::Type::DECIMAL128: {
            auto dec_type = std::static_pointer_cast<arrow::Decimal128Type>(type);
            QoreStringValueHelper str(val);
            arrow::Decimal128 dec_val;
            int32_t out_precision = 0;
            int32_t out_scale = 0;
            auto parse_status = arrow::Decimal128::FromString(std::string(str->c_str()), &dec_val, &out_precision,
                &out_scale);
            if (!parse_status.ok()) {
                xsink->raiseException("DATAFRAME-IO-ERROR",
                    "failed to parse decimal128 value '%s': %s", str->c_str(), parse_status.ToString().c_str());
                return false;
            }
            if (out_scale != dec_type->scale()) {
                auto rescale_result = dec_val.Rescale(out_scale, dec_type->scale());
                if (!rescale_result.ok()) {
                    xsink->raiseException("DATAFRAME-IO-ERROR",
                        "failed to rescale decimal128 value: %s",
                        rescale_result.status().ToString().c_str());
                    return false;
                }
                dec_val = std::move(rescale_result).ValueUnsafe();
            }
            status = static_cast<arrow::Decimal128Builder*>(builder)->Append(dec_val);
            break;
        }
        case arrow::Type::STRUCT: {
            auto struct_builder = static_cast<arrow::StructBuilder*>(builder);
            auto struct_type = std::static_pointer_cast<arrow::StructType>(type);
            const QoreHashNode* hash = val.get<QoreHashNode>();
            if (!hash) {
                xsink->raiseException("DATAFRAME-IO-ERROR",
                    "expected hash for Arrow struct value, got %s", val.getFullTypeName());
                return false;
            }
            status = struct_builder->Append();
            if (!status.ok()) {
                break;
            }
            for (int i = 0; i < struct_type->num_fields(); ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "building Arrow struct value")) {
                    return false;
                }
                auto child_field = struct_type->field(i);
                if (!appendToArrowBuilder(struct_builder->child_builder(i).get(), child_field->type(),
                        hash->getKeyValue(child_field->name().c_str()), xsink)) {
                    return false;
                }
            }
            return true;
        }
        case arrow::Type::LIST:
        case arrow::Type::LARGE_LIST:
        case arrow::Type::FIXED_SIZE_LIST: {
            const QoreListNode* qlist = val.get<QoreListNode>();
            if (!qlist) {
                xsink->raiseException("DATAFRAME-IO-ERROR",
                    "expected list for Arrow list value, got %s", val.getFullTypeName());
                return false;
            }
            arrow::ArrayBuilder* value_builder = nullptr;
            std::shared_ptr<arrow::DataType> value_type;
            if (type->id() == arrow::Type::LIST) {
                auto list_builder = static_cast<arrow::ListBuilder*>(builder);
                status = list_builder->Append();
                value_builder = list_builder->value_builder();
                value_type = std::static_pointer_cast<arrow::ListType>(type)->value_type();
            } else if (type->id() == arrow::Type::LARGE_LIST) {
                auto list_builder = static_cast<arrow::LargeListBuilder*>(builder);
                status = list_builder->Append();
                value_builder = list_builder->value_builder();
                value_type = std::static_pointer_cast<arrow::LargeListType>(type)->value_type();
            } else {
                auto list_builder = static_cast<arrow::FixedSizeListBuilder*>(builder);
                auto list_type = std::static_pointer_cast<arrow::FixedSizeListType>(type);
                if (static_cast<int32_t>(qlist->size()) != list_type->list_size()) {
                    xsink->raiseException("DATAFRAME-IO-ERROR",
                        "fixed_size_list expects %d elements, got %d",
                        list_type->list_size(), static_cast<int>(qlist->size()));
                    return false;
                }
                status = list_builder->Append();
                value_builder = list_builder->value_builder();
                value_type = list_type->value_type();
            }
            if (!status.ok()) {
                break;
            }
            for (size_t i = 0; i < qlist->size(); ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "building Arrow list value")) {
                    return false;
                }
                if (!appendToArrowBuilder(value_builder, value_type, qlist->retrieveEntry(i), xsink)) {
                    return false;
                }
            }
            return true;
        }
        case arrow::Type::MAP: {
            auto map_builder = static_cast<arrow::MapBuilder*>(builder);
            auto map_type = std::static_pointer_cast<arrow::MapType>(type);
            const QoreHashNode* hash = val.get<QoreHashNode>();
            if (!hash) {
                xsink->raiseException("DATAFRAME-IO-ERROR",
                    "expected hash for Arrow map value, got %s", val.getFullTypeName());
                return false;
            }
            status = map_builder->Append();
            if (!status.ok()) {
                break;
            }
            ConstHashIterator hi(hash);
            size_t i = 0;
            while (hi.next()) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "building Arrow map value")) {
                    return false;
                }
                SimpleRefHolder<QoreStringNode> key_str(new QoreStringNode(hi.getKey()));
                if (!appendToArrowBuilder(map_builder->key_builder(), map_type->key_type(), QoreValue(*key_str),
                        xsink)
                        || !appendToArrowBuilder(map_builder->item_builder(), map_type->item_type(), hi.get(),
                            xsink)) {
                    return false;
                }
                ++i;
            }
            return true;
        }
        case arrow::Type::NA:
            status = builder->AppendNull();
            break;
        default:
            xsink->raiseException("DATAFRAME-IO-ERROR",
                "unsupported Arrow type for DataFrame export: %s", type->ToString().c_str());
            return false;
    }

    if (!status.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "failed to append Arrow value: %s", status.ToString().c_str());
        return false;
    }
    return true;
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
    if (element_type == QoreBufferElementType::Decimal128) {
        auto decimal_type = std::static_pointer_cast<arrow::Decimal128Type>(arr->type());
        return QoreBufferNode::wrapExternalStorage(element_type, nullable, static_cast<size_t>(chunk->offset()),
            static_cast<size_t>(chunk->length()), values, validity, std::move(owner), chunk->null_count(),
            decimal_type->precision(), decimal_type->scale(), xsink);
    }
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
        auto cd = buildColumnDataFromBuffer(*dense_buffer, xsink);
        if (cd) {
            setArrowColumnarSchema(*cd, field, xsink);
        }
        return cd;
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
        cd->type = ColumnType::AUTO;
        cd->auto_data.resize(n);
        int64_t idx = 0;
        for (int c = 0; c < arr->num_chunks(); ++c) {
            auto chunk = arr->chunk(c);
            for (int64_t i = 0; i < chunk->length(); ++i) {
                if (checkParquetCancel(idx, "converting Arrow complex column", xsink)) {
                    return nullptr;
                }
                if (chunk->IsNull(i)) {
                    cd->null_mask[idx] = 1;
                } else {
                    ValueHolder value(arrowScalarToQore(chunk, i, xsink), xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    cd->setAutoValue(idx, *value, xsink);
                }
                ++idx;
            }
        }
    }

    setArrowColumnarSchema(*cd, field, xsink);
    if (*xsink) {
        return nullptr;
    }
    if (arrowTypeIsNestedOrDictionary(arr->type())) {
        cd->setExternalColumnRef(ExternalColumnKind::ARROW_CHUNKED_ARRAY,
            std::shared_ptr<void>(arr, static_cast<void*>(arr.get())));
    }
    return cd;
}

static std::shared_ptr<arrow::Array> denseBufferToArrowArray(const ColumnData& cd,
        const std::shared_ptr<arrow::DataType>& type, const std::string& column_name, ExceptionSink* xsink) {
    if (!cd.hasDenseBuffer() || cd.n_rows < 0 || cd.dense_buffer->size() != static_cast<size_t>(cd.n_rows)) {
        return nullptr;
    }

    QoreBufferElementType expected = arrowTypeToBufferElementType(type);
    QoreBufferElementType actual = cd.dense_buffer_type;
    if (expected == QoreBufferElementType::Invalid || actual != expected
            || actual == QoreBufferElementType::String || actual == QoreBufferElementType::Decimal128) {
        return nullptr;
    }

    if (cd.dense_buffer->ensureHostStorage(xsink)) {
        return nullptr;
    }
    if (*xsink) {
        return nullptr;
    }

    const uint8_t* raw_data = static_cast<const uint8_t*>(cd.dense_buffer->getRawData());
    if (cd.n_rows && !raw_data) {
        return nullptr;
    }

    int64_t offset = 0;
    size_t data_size = 0;
    if (actual == QoreBufferElementType::Bool) {
        offset = static_cast<int64_t>(cd.dense_buffer->getRawDataBitOffset());
        data_size = qoreDataFrameBitmapBytes(cd.n_rows, static_cast<size_t>(offset));
    } else {
        if (cd.dense_buffer->hasNullableElements() && cd.dense_buffer->getRawValidityBitOffset()) {
            return nullptr;
        }
        size_t element_size = qore_buffer_element_storage_size(actual);
        if (static_cast<uint64_t>(cd.n_rows)
                > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) / element_size) {
            xsink->raiseException("DATAFRAME-IO-ERROR",
                "DataFrame column '%s' is too large for zero-copy Arrow export",
                column_name.c_str());
            return nullptr;
        }
        data_size = static_cast<size_t>(cd.n_rows) * element_size;
    }

    int64_t null_count = 0;
    std::shared_ptr<arrow::Buffer> validity_buffer;
    auto owner = refQoreBufferOwner(cd.dense_buffer);
    if (cd.dense_buffer->hasNullableElements()) {
        size_t valid_count = cd.dense_buffer->countValid(xsink);
        if (*xsink) {
            return nullptr;
        }
        null_count = cd.n_rows - static_cast<int64_t>(valid_count);
        if (null_count > 0) {
            size_t validity_bit_offset = cd.dense_buffer->getRawValidityBitOffset();
            if (actual != QoreBufferElementType::Bool && validity_bit_offset) {
                return nullptr;
            }
            if (actual == QoreBufferElementType::Bool
                    && validity_bit_offset != static_cast<size_t>(offset)) {
                return nullptr;
            }
            const uint8_t* validity = cd.dense_buffer->getRawValidityData();
            if (!validity) {
                return nullptr;
            }
            size_t validity_size = qoreDataFrameBitmapBytes(cd.n_rows, validity_bit_offset);
            validity_buffer = wrapQoreBufferMemory(validity, static_cast<int64_t>(validity_size), owner);
        }
    }

    auto data_buffer = wrapQoreBufferMemory(raw_data, static_cast<int64_t>(data_size), owner);
    std::vector<std::shared_ptr<arrow::Buffer>> buffers = {validity_buffer, data_buffer};
    auto data = arrow::ArrayData::Make(type, cd.n_rows, std::move(buffers), null_count, offset);
    return arrow::MakeArray(data);
}

static std::shared_ptr<arrow::Array> columnDataToArrowArray(const ColumnData& cd,
        const std::shared_ptr<arrow::DataType>& type, arrow::MemoryPool* pool, const std::string& column_name,
        ExceptionSink* xsink) {
    auto dense_arr = denseBufferToArrowArray(cd, type, column_name, xsink);
    if (*xsink || dense_arr) {
        return dense_arr;
    }

    auto builder_result = arrow::MakeBuilder(type, pool);
    if (!builder_result.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "cannot create Arrow builder for column '%s' type '%s': %s",
            column_name.c_str(), type->ToString().c_str(), builder_result.status().ToString().c_str());
        return nullptr;
    }
    auto builder = std::move(builder_result).ValueUnsafe();
    if (checkArrowColumnStatus(builder->Reserve(cd.n_rows), xsink,
            "error reserving Arrow array", column_name)) {
        return nullptr;
    }

    for (int64_t i = 0; i < cd.n_rows; ++i) {
        if (checkParquetCancel(i, "building Arrow array from DataFrame column", xsink)) {
            return nullptr;
        }
        ValueHolder value(cd.isNull(i) ? QoreValue() : cd.getValueAt(i, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        if (!appendToArrowBuilder(builder.get(), type, *value, xsink)) {
            return nullptr;
        }
    }

    std::shared_ptr<arrow::Array> arr;
    auto status = builder->Finish(&arr);
    if (!status.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "error building Arrow array for column '%s': %s",
            column_name.c_str(), status.ToString().c_str());
        return nullptr;
    }
    return arr;
}

static std::shared_ptr<arrow::Table> dataFrameToArrowTable(const std::vector<Column>& columns,
        int64_t n_rows, arrow::MemoryPool* pool, ExceptionSink* xsink) {
    std::vector<std::shared_ptr<arrow::Field>> fields;
    std::vector<std::shared_ptr<arrow::ChunkedArray>> arrays;

    size_t column_index = 0;
    for (const auto& col : columns) {
        if (column_index && !(column_index % 100)
                && qore_check_cancel(xsink, "building Arrow table from DataFrame columns")) {
            return nullptr;
        }
        ++column_index;

        const ColumnData& cd = *col.data;
        if (cd.has_columnar_schema) {
            QoreColumnarTypeDescriptor schema = cd.columnar_schema;
            schema.name = col.name;
            schema.nullable = schema.nullable || cd.countNull() > 0;
            auto field = columnarDescriptorToArrowField(schema, col.name, xsink);
            if (!field) {
                return nullptr;
            }
            if (cd.external_column_kind == ExternalColumnKind::ARROW_CHUNKED_ARRAY
                    && cd.external_column_owner) {
                auto chunked = std::static_pointer_cast<arrow::ChunkedArray>(cd.external_column_owner);
                if (chunked->length() == cd.n_rows && chunked->type()->Equals(field->type())) {
                    fields.push_back(field);
                    arrays.push_back(chunked);
                    continue;
                }
            }
            auto arr = columnDataToArrowArray(cd, field->type(), pool, col.name, xsink);
            if (!arr) {
                return nullptr;
            }
            fields.push_back(field);
            arrays.push_back(std::make_shared<arrow::ChunkedArray>(arr));
            continue;
        }

        switch (cd.type) {
            case ColumnType::INT64: {
                auto type = arrow::int64();
                fields.push_back(arrow::field(col.name, type));
                auto dense_arr = denseBufferToArrowArray(cd, type, col.name, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (dense_arr) {
                    arrays.push_back(std::make_shared<arrow::ChunkedArray>(dense_arr));
                    break;
                }
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
                arrays.push_back(std::make_shared<arrow::ChunkedArray>(arr));
                break;
            }
            case ColumnType::FLOAT64: {
                auto type = arrow::float64();
                fields.push_back(arrow::field(col.name, type));
                auto dense_arr = denseBufferToArrowArray(cd, type, col.name, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (dense_arr) {
                    arrays.push_back(std::make_shared<arrow::ChunkedArray>(dense_arr));
                    break;
                }
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
                arrays.push_back(std::make_shared<arrow::ChunkedArray>(arr));
                break;
            }
            case ColumnType::STRING: {
                fields.push_back(arrow::field(col.name, arrow::utf8()));
                arrow::StringBuilder builder(pool);
                if (checkArrowColumnStatus(builder.Reserve(cd.n_rows), xsink,
                        "error reserving Arrow string array", col.name)) {
                    return nullptr;
                }
                int64_t data_bytes = 0;
                for (int64_t i = 0; i < cd.n_rows; ++i) {
                    if (checkParquetCancel(i, "sizing Arrow string array", xsink)) {
                        return nullptr;
                    }
                    if (cd.isNull(i)) {
                        continue;
                    }
                    if (static_cast<uint64_t>(cd.str_data[i].size())
                            > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) - data_bytes) {
                        xsink->raiseException("DATAFRAME-IO-ERROR",
                            "DataFrame string column '%s' is too large for Arrow export",
                            col.name.c_str());
                        return nullptr;
                    }
                    data_bytes += static_cast<int64_t>(cd.str_data[i].size());
                }
                if (checkArrowColumnStatus(builder.ReserveData(data_bytes), xsink,
                        "error reserving Arrow string data", col.name)) {
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
                arrays.push_back(std::make_shared<arrow::ChunkedArray>(arr));
                break;
            }
            case ColumnType::BOOL: {
                auto type = arrow::boolean();
                fields.push_back(arrow::field(col.name, type));
                auto dense_arr = denseBufferToArrowArray(cd, type, col.name, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (dense_arr) {
                    arrays.push_back(std::make_shared<arrow::ChunkedArray>(dense_arr));
                    break;
                }
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
                arrays.push_back(std::make_shared<arrow::ChunkedArray>(arr));
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
                arrays.push_back(std::make_shared<arrow::ChunkedArray>(arr));
                break;
            }
            case ColumnType::AUTO: {
                xsink->raiseException("DATAFRAME-IO-ERROR",
                    "DataFrame column '%s' has type 'auto' and no Arrow schema metadata; construct it from "
                    "ColumnarResult with schema metadata or convert the column explicitly before Arrow export",
                    col.name.c_str());
                return nullptr;
            }
            default:
                break;
        }
    }

    return arrow::Table::Make(arrow::schema(fields), arrays, n_rows);
}

QoreDataFrame* QoreDataFrame::readParquet(const std::string& path,
        const QoreHashNode* options, ExceptionSink* xsink) {
    if (qore_check_cancel(xsink, "reading Parquet file")) {
        return nullptr;
    }
    arrow::MemoryPool* pool = arrow::system_memory_pool();
    QoreParquetReadOptions read_options;
    if (!qoreParquetReadOptions(options, read_options, xsink)) {
        return nullptr;
    }

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
    if (read_options.has_use_threads) {
        reader->set_use_threads(read_options.use_threads);
    }
    if (read_options.has_batch_size) {
        reader->set_batch_size(read_options.batch_size);
    }

    for (size_t i = 0; i < read_options.row_groups.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "validating Parquet row groups")) {
            return nullptr;
        }
        if (read_options.row_groups[i] >= reader->num_row_groups()) {
            xsink->raiseException("DATAFRAME-IO-ERROR",
                "Parquet row group %d is out of range; file has %d row groups",
                read_options.row_groups[i], reader->num_row_groups());
            return nullptr;
        }
    }

    // Read entire file into Arrow Table
    std::shared_ptr<arrow::Table> table;
    std::vector<int> column_indices;
    if (read_options.has_columns) {
        column_indices = qoreParquetColumnIndicesForNames(reader, read_options.columns, xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    if (read_options.has_row_groups && read_options.has_columns) {
        status = reader->ReadRowGroups(read_options.row_groups, column_indices, &table);
    } else if (read_options.has_row_groups) {
        status = reader->ReadRowGroups(read_options.row_groups, &table);
    } else if (read_options.has_columns) {
        status = reader->ReadTable(column_indices, &table);
    } else {
        status = reader->ReadTable(&table);
    }
    if (!status.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "error reading Parquet table from '%s': %s", path.c_str(),
            status.ToString().c_str());
        return nullptr;
    }
    if (read_options.has_columns) {
        table = qoreParquetReorderTopLevelColumns(table, read_options.columns, xsink);
        if (*xsink) {
            return nullptr;
        }
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

    auto buffer = wrapBinaryAsArrowBuffer(data, xsink);
    if (*xsink) {
        return nullptr;
    }
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

void QoreDataFrame::writeParquet(const std::string& path, const QoreHashNode* options,
        ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "writing Parquet file")) {
        return;
    }
    arrow::MemoryPool* pool = arrow::system_memory_pool();
    QoreParquetWriteOptions write_options;
    if (!qoreParquetWriteOptions(options, write_options, xsink)) {
        return;
    }

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

    parquet::WriterProperties::Builder properties_builder;
    if (write_options.has_compression) {
        properties_builder.compression(write_options.compression);
    }
    if (write_options.has_use_dictionary) {
        if (write_options.use_dictionary) {
            properties_builder.enable_dictionary();
        } else {
            properties_builder.disable_dictionary();
        }
    }
    if (write_options.has_write_statistics) {
        if (write_options.write_statistics) {
            properties_builder.enable_statistics();
        } else {
            properties_builder.disable_statistics();
        }
    }

    parquet::ArrowWriterProperties::Builder arrow_properties_builder;
    if (write_options.has_use_threads) {
        arrow_properties_builder.set_use_threads(write_options.use_threads);
    }
    if (write_options.store_schema) {
        arrow_properties_builder.store_schema();
    }

    int64_t row_group_size = write_options.has_row_group_size
        ? write_options.row_group_size
        : (table->num_rows() > 0 ? table->num_rows() : 1024);

    // Write table
    auto status = parquet::arrow::WriteTable(*table, pool, outfile, row_group_size,
        properties_builder.build(), arrow_properties_builder.build());
    if (!status.ok()) {
        xsink->raiseException("DATAFRAME-IO-ERROR",
            "error writing Parquet file '%s': %s", path.c_str(),
            status.ToString().c_str());
    }
}

} // namespace QoreDataFrameNS
