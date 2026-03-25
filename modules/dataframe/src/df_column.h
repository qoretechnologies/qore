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

//! Returns the string name of a column type
DLLLOCAL const char* columnTypeName(ColumnType type);

//! Column data — immutable after construction (shared across DataFrames via shared_ptr)
struct ColumnData {
    ColumnType type;
    int64_t n_rows = 0;

    // Type-specific storage (only one is populated based on type)
    Eigen::VectorXd float_data;
    std::vector<int64_t> int_data;
    std::vector<std::string> str_data;
    std::vector<uint8_t> bool_data;
    std::vector<int64_t> date_data;     //!< microsecond epoch UTC

    //! Null/missing mask: 1 = null, 0 = present
    std::vector<uint8_t> null_mask;

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

//! Convert a column's values to a QoreListNode
DLLLOCAL QoreListNode* columnToQoreList(const ColumnData& col, ExceptionSink* xsink);

} // namespace QoreDataFrameNS

#endif // _QORE_DATAFRAME_DF_COLUMN_H
