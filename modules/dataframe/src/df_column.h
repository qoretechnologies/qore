/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    df_column.h

    DataFrame column data structures and utilities

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_DATAFRAME_DF_COLUMN_H
#define _QORE_DATAFRAME_DF_COLUMN_H

#include "qore/Qore.h"
#include "qore/QoreColumnarResult.h"

#include <Eigen/Dense>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace QoreDataFrameNS {

//! Column data types
enum class ColumnType : int {
    FLOAT64 = 0,    //!< 64-bit floating point (Eigen VectorXd)
    INT64,          //!< 64-bit integer
    STRING,         //!< UTF-8 string
    BOOL,           //!< Boolean (stored as uint8_t)
    DATE,           //!< Date/time (microsecond epoch UTC)
    AUTO,           //!< Heterogeneous (QoreValue)
};

//! Optional external column storage preserved for zero-copy interchange APIs
enum class ExternalColumnKind : int {
    NONE = 0,
    ARROW_CHUNKED_ARRAY,
};

//! Returns the string name of a column type
DLLLOCAL const char* columnTypeName(ColumnType type);

//! Safely extracts a Qore string value, including short-string values.
static inline bool getDataFrameString(QoreValue v, std::string& out) {
    if (v.getType() != NT_STRING) {
        return false;
    }

    QoreStringValueHelper str(v);
    out = str->c_str();
    return true;
}

//! Column data — immutable after construction (shared across DataFrames via shared_ptr)
struct ColumnData {
    ColumnType type = ColumnType::AUTO;
    int64_t n_rows = 0;

    //! Optional dense source buffer preserved for zero-copy columnar APIs
    QoreBufferNode* dense_buffer = nullptr;
    QoreBufferElementType dense_buffer_type = QoreBufferElementType::Invalid;

    //! Optional immutable external column storage preserved for Arrow-style nested arrays
    ExternalColumnKind external_column_kind = ExternalColumnKind::NONE;
    std::shared_ptr<void> external_column_owner;

    // Type-specific storage (only one is populated based on type)
    Eigen::VectorXd float_data;
    std::vector<int64_t> int_data;
    std::vector<std::string> str_data;
    std::vector<uint8_t> bool_data;
    std::vector<int64_t> date_data;     //!< microsecond epoch UTC
    std::vector<QoreValue> auto_data;    //!< referenced heterogeneous Qore values

    //! Optional recursive columnar schema metadata preserved from ColumnarResult / Arrow
    QoreColumnarTypeDescriptor columnar_schema;
    bool has_columnar_schema = false;

    //! Null/missing mask: 1 = null, 0 = present
    std::vector<uint8_t> null_mask;

    DLLLOCAL ColumnData() = default;
    DLLLOCAL ColumnData(const ColumnData& old);
    DLLLOCAL ColumnData(ColumnData&& old) noexcept;
    DLLLOCAL ColumnData& operator=(const ColumnData& old);
    DLLLOCAL ColumnData& operator=(ColumnData&& old) noexcept;
    DLLLOCAL ~ColumnData();

    //! Preserves a referenced dense buffer for zero-copy columnar APIs
    DLLLOCAL void setDenseBufferRef(const QoreBufferNode* buffer);

    //! Preserves immutable external column storage for zero-copy interchange APIs
    DLLLOCAL void setExternalColumnRef(ExternalColumnKind kind, std::shared_ptr<void> owner);

    //! Clears preserved external column storage
    DLLLOCAL void clearExternalColumnRef();

    //! Stores a referenced Qore value in an AUTO column slot
    DLLLOCAL void setAutoValue(int64_t i, QoreValue value, ExceptionSink* xsink);

    //! Appends a referenced Qore value to an AUTO column
    DLLLOCAL void appendAutoValue(QoreValue value);

    //! Returns true if this column has a preserved dense buffer
    bool hasDenseBuffer() const {
        return dense_buffer;
    }

    //! Returns a referenced dense buffer, or nullptr when not available
    DLLLOCAL QoreBufferNode* refDenseBuffer() const;

    //! Returns true if the value at index i is null
    bool isNull(int64_t i) const {
        return i < (int64_t)null_mask.size() && null_mask[i];
    }

    //! Returns the count of non-null values
    int64_t countNonNull() const;

    //! Returns the count of null values
    int64_t countNull() const;

    //! Get a value as QoreValue at index i
    QoreValue getValueAt(int64_t i, ExceptionSink* xsink) const;
};

//! A named column with shared data
struct Column {
    std::string name;
    std::shared_ptr<ColumnData> data;
};

//! Infer the column type from a list of Qore values
DLLLOCAL ColumnType inferColumnType(const QoreListNode* values);

//! Build a ColumnData from a list of Qore values with the given type
DLLLOCAL std::shared_ptr<ColumnData> buildColumnData(const QoreListNode* values,
    ColumnType type, ExceptionSink* xsink);

//! Build a ColumnData from a list of Qore values with auto-inferred type
DLLLOCAL std::shared_ptr<ColumnData> buildColumnDataAuto(const QoreListNode* values,
    ExceptionSink* xsink);

//! Build a ColumnData from a typed Qore buffer
DLLLOCAL std::shared_ptr<ColumnData> buildColumnDataFromBuffer(const QoreBufferNode* values,
    ExceptionSink* xsink);

//! Convert a column's values to a QoreListNode
DLLLOCAL QoreListNode* columnToQoreList(const ColumnData& col, ExceptionSink* xsink);

//! Convert a range of column values to a QoreListNode
DLLLOCAL QoreListNode* columnToQoreListRange(const ColumnData& col, int64_t start,
    int64_t count, ExceptionSink* xsink);

//! Convert a dense-compatible column to a Qore buffer
DLLLOCAL QoreBufferNode* columnToQoreBuffer(const ColumnData& col, ExceptionSink* xsink);

//! Preserves external storage for a contiguous row slice when supported
DLLLOCAL void sliceExternalColumnRef(ColumnData& dest, const ColumnData& src, int64_t start, int64_t count);

} // namespace QoreDataFrameNS

#endif // _QORE_DATAFRAME_DF_COLUMN_H
