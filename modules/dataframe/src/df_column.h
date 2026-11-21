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

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
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

//! Returns a copy of all bytes of a Qore string, including any NUL characters
/** A DataFrame cell value or name may contain NUL characters, so a Qore string is always copied with its length,
    never as a C string, which would cut it at the first NUL character.
*/
static inline std::string dataFrameStdString(const QoreString* str) {
    assert(str);
    return std::string(str->c_str(), str->size());
}

//! Stores a value as the UTF-8 bytes of a string cell: a string in another encoding converted (qore#6092)
/** @param v the value; a value that is not a string is stored as its string form
    @param out the bytes of the cell; the cells of a string column are always in UTF-8
    @param xsink for a DATAFRAME-ENCODING-ERROR exception if the string cannot be converted to UTF-8

    @return 0 for success, -1 if an exception was raised

    A conversion error raises the exception from a function of its own, so that the inline part that stores a
    UTF-8 string adds nothing to the code that stores a cell.
*/
DLLLOCAL int dataFrameConvertUtf8String(QoreValue v, std::string& out, ExceptionSink* xsink);

static inline int dataFrameUtf8String(QoreValue v, std::string& out, ExceptionSink* xsink) {
    // the type of a value in a node is read from the node, with no call to QoreValue::getType(); a short string,
    // held in the value itself, is always in UTF-8
    if (v.isPointer()) {
        const AbstractQoreNode* node = v.getPtr();
        if (node->getType() == NT_STRING) {
            // note: safe; a value in a node (isPointer()) is never an inline short string, which is handled below
            const QoreStringNode* str = static_cast<const QoreStringNode*>(node);
            if (str->getEncoding() == QCS_UTF8) {
                out.assign(str->c_str(), str->size());
                return 0;
            }
        }
    } else if (v.isShortString()) {
        QoreStringDataHelper str(v);
        out.assign(str.c_str(), str.size());
        return 0;
    }
    return dataFrameConvertUtf8String(v, out, xsink);
}

DLLLOCAL int dataFrameConvertUtf8Bytes(const char* data, size_t len, const QoreEncoding* enc, std::string& out,
        ExceptionSink* xsink);

//! Converts the bytes of a string in the given encoding to the UTF-8 bytes of a string cell (qore#6092)
/** @return 0 for success, -1 if a DATAFRAME-ENCODING-ERROR exception was raised
*/
static inline int dataFrameUtf8Bytes(const char* data, size_t len, const QoreEncoding* enc, std::string& out,
        ExceptionSink* xsink) {
    if (enc == QCS_UTF8) {
        out.assign(data, len);
        return 0;
    }
    return dataFrameConvertUtf8Bytes(data, len, enc, out, xsink);
}

DLLLOCAL int dataFrameConvertFromUtf8(const std::string& utf8, const QoreEncoding* enc, std::string& out,
        ExceptionSink* xsink);

//! Converts the UTF-8 bytes of a string cell to the default encoding, the encoding of column names and of CSV files
/** @return 0 for success, -1 if a DATAFRAME-ENCODING-ERROR exception was raised for a character that the default
    encoding cannot represent
*/
static inline int dataFrameDefaultString(const std::string& utf8, std::string& out, ExceptionSink* xsink) {
    if (QCS_DEFAULT == QCS_UTF8) {
        out = utf8;
        return 0;
    }
    return dataFrameConvertFromUtf8(utf8, QCS_DEFAULT, out, xsink);
}

//! Returns the UTF-8 form of a value for a group or join key, so that equal strings in different encodings make
//! the same key; a string that cannot be converted keeps its bytes
DLLLOCAL std::string dataFrameUtf8Key(QoreValue v);

//! Raises DATAFRAME-DATE-RANGE-ERROR for a date or time value outside the range of a date cell (qore#6092)
/** @param desc a description of the value
*/
DLLLOCAL void dataFrameRaiseDateRangeError(const char* desc, ExceptionSink* xsink);

//! Raises DATAFRAME-DATE-RANGE-ERROR for a date outside the range of a date cell, with a description of the date
/** @return -1
*/
DLLLOCAL int dataFrameDateOutOfRange(const DateTimeNode* dt, ExceptionSink* xsink);

//! Stores a date as the microseconds since the epoch of a date cell, an int64 (qore#6092)
/** @return 0 for success, -1 if a DATAFRAME-DATE-RANGE-ERROR exception was raised for a date outside the range of
    an int64 of microseconds since the epoch, which the cell cannot hold
*/
static inline int dataFrameDateMicros(const DateTimeNode* dt, int64_t& out, ExceptionSink* xsink) {
    // the core computes the microseconds of an absolute date or a time value with overflow-checked arithmetic, in
    // one call, as DateTime::getEpochMicrosecondsUTC() does without the check
    int64 micros;
    if (dt->getEpochMicrosecondsUTCChecked(micros)) {
        out = micros;
        return 0;
    }
    return dataFrameDateOutOfRange(dt, xsink);
}

//! The kind of a date stored in a date cell, as DateTime::getEpochMicrosecondsUTCCheckedKind() returns it
//! (qore#6107)
enum DataFrameDateKind : int {
    DFDK_ABSOLUTE = 1,
    DFDK_RELATIVE = 2,
};

//! Compares two values as the core <=> operator does (qore#6097)
/** @param cmp the result: -1, 0 or 1
    @return 0 for success, -1 if the operator raises an exception (NAN-COMPARE-ERROR for a NaN value)
*/
DLLLOCAL int qoreDataFrameCompareValues(QoreValue lhs, QoreValue rhs, int& cmp, ExceptionSink* xsink);

//! Safely extracts a Qore string value, including short-string values and NUL characters.
static inline bool getDataFrameString(QoreValue v, std::string& out) {
    if (v.getType() != NT_STRING) {
        return false;
    }

    QoreStringValueHelper str(v);
    out.assign(str->c_str(), str->size());
    return true;
}

//! Appends a null component to a key made of several values (a group or join key)
/** Each component of a key is tagged, and a value component has its length, so that a key can be built from any
    values without two different value tuples making the same key: a null value differs from an empty string, and
    a value with a NUL or separator character cannot be confused with two values.
*/
static inline void appendDataFrameKeyNull(std::string& key) {
    key += 'N';
}

//! Appends a value component to a key made of several values (a group or join key)
/** @param key the key to append to
    @param value the bytes of the value as a string, which may contain NUL characters
    @param len the length of the value in bytes

    @see appendDataFrameKeyNull()
*/
static inline void appendDataFrameKeyValue(std::string& key, const char* value, size_t len) {
    key += 'V';
    uint64_t n = len;
    key.append(reinterpret_cast<const char*>(&n), sizeof(n));
    key.append(value, len);
}

//! Appends a value component to a key made of several values (a group or join key)
static inline void appendDataFrameKeyValue(std::string& key, const std::string& value) {
    appendDataFrameKeyValue(key, value.data(), value.size());
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

    //! Returns a copy of this column for changing its values
    /** The copy has the values, null mask and schema of this column, but not its preserved dense buffer or external
        storage: they hold the values of this column, so they would no longer match the changed values of the copy.
        The typed storage of a column always has all of its values, so nothing is lost.

        Every operation that copies a column and then changes the values of the copy must use this function.
    */
    DLLLOCAL std::shared_ptr<ColumnData> copyForUpdate() const;

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

    //! Returns true if the column is a date column of durations, whose cells are relative dates
    bool isDuration() const {
        return has_columnar_schema && columnar_schema.kind == QoreColumnarTypeKind::Duration;
    }

    //! Returns the count of non-null values
    int64_t countNonNull() const;

    //! Returns the count of null values
    int64_t countNull() const;

    //! Get a value as QoreValue at index i
    QoreValue getValueAt(int64_t i, ExceptionSink* xsink) const;

    //! Returns true if the column has a value that can be part of a cycle
    /** The values of a column do not change once it is part of a DataFrame (an operation that changes them works on
        a copy: see copyForUpdate()), so the answer is computed once.
    */
    DLLLOCAL bool needsScan() const;

    //! Makes the caller the one holder that reports the values of this column to the garbage collector
    /** A column is shared between DataFrames, but its values are referenced once: if two objects reported them,
        the collector would count two references and could delete a value that is still held from outside.

        @return true if the caller reports the values, false if another holder does already: the caller must then
        use a copy of the column, which references the values again
    */
    DLLLOCAL bool claimScan() {
        bool expected = false;
        return scan_claimed.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
    }

    //! Called by the holder that claimed the column with claimScan() when it no longer reports its values
    DLLLOCAL void releaseScan() {
        assert(scan_claimed.load(std::memory_order_relaxed));
        scan_claimed.store(false, std::memory_order_release);
    }

private:
    //! -1 if needsScan() has not been computed yet, otherwise its result as 0 or 1
    mutable std::atomic<int> scan_state{-1};
    //! true while a holder reports the values to the garbage collector; see claimScan()
    std::atomic<bool> scan_claimed{false};
};

//! A named column with shared data
/** A column name may contain NUL characters; it is kept with its length everywhere in the DataFrame.  A name with a
    NUL character cannot be a hash key, so an operation that returns the column names as hash keys raises a
    \c HASH-KEY-ERROR for it (see checkDataFrameColumnKeys()).
*/
struct Column {
    std::string name;
    std::shared_ptr<ColumnData> data;
};

//! Checks that the names of the given columns can be hash keys
/** @param columns the columns whose names will be keys of a hash returned to the caller
    @param xsink a \c HASH-KEY-ERROR is raised here for the first name with a NUL character

    @return 0 if all names can be keys, -1 if an exception was raised
*/
static inline int checkDataFrameColumnKeys(const std::vector<Column>& columns, ExceptionSink* xsink) {
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "checking DataFrame column names")) {
            return -1;
        }
        if (QoreHashNode::checkKey(columns[i].name.data(), columns[i].name.size(), xsink)) {
            return -1;
        }
    }
    return 0;
}

//! Infer the column type from a list of Qore values
/** The holder of these columns becomes the one holder that reports the values of each column that needs a scan to
    the garbage collector (see ColumnData::claimScan()); each column that another holder reports already, or that
    appears twice in \a columns, is replaced by a copy.

    @param columns the columns of the holder
    @param replaced receives the columns that were replaced; the caller releases them after its locks, as their last
    reference can be released with them

    @return the number of columns whose values the holder reports
*/
DLLLOCAL size_t claimScanColumns(std::vector<Column>& columns, std::vector<std::shared_ptr<ColumnData>>& replaced);

//! Releases the columns claimed by claimScanColumns()
DLLLOCAL void releaseScanColumns(const std::vector<Column>& columns);

//! Marks each value that needs a scan in the columns as stored in the private data of \a self
DLLLOCAL void markScanColumnValuesStored(QoreObject& self, const std::vector<Column>& columns);

//! Reports each value that needs a scan in the columns to the garbage collector
/** @return true if the scan must end
*/
DLLLOCAL bool scanColumnValues(const std::vector<Column>& columns, QorePrivateDataScanner& scanner);

//! Appends the tag and the key of a float to a key: its bits, with -0.0 as 0.0 and every NaN as one NaN
/** Two floats have the same key if they are equal (as with the == operator) or both NaN (qore#6107).
*/
static inline void appendDataFrameFloatKey(std::string& key, char tag, double value) {
    if (value == 0.0) {
        // -0.0 == 0.0
        value = 0.0;
    } else if (std::isnan(value)) {
        // the NaN values make one group
        value = std::numeric_limits<double>::quiet_NaN();
    }
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    key += tag;
    key.append(reinterpret_cast<const char*>(&bits), sizeof(bits));
}

//! Makes a duration of the given seconds and microseconds; the seconds of a relative date are an int, so a duration
//! of more seconds than an int holds (about 68 years) is made of days and seconds (qore#6107)
/** The days must fit in an int, as they do for the microseconds of a date cell; a caller with more seconds checks
    them first.
*/
static inline DateTimeNode* dataFrameMakeDuration(int64_t seconds, int microseconds) {
    if (seconds >= std::numeric_limits<int>::min() && seconds <= std::numeric_limits<int>::max()) {
        return DateTimeNode::makeRelative(0, 0, 0, 0, 0, static_cast<int>(seconds), microseconds);
    }
    return DateTimeNode::makeRelative(0, 0, static_cast<int>(seconds / 86400), 0, 0,
        static_cast<int>(seconds % 86400), microseconds);
}

//! Gives a date column made of the cells of another the columnar schema of the other (its time unit and zone, or
//! that its dates are durations), with no name, so that its cells are returned and exported as those of the other
//! (qore#6107)
static inline void dataFrameCopyDateSchema(ColumnData& dest, const ColumnData& src) {
    if (src.type == ColumnType::DATE && src.has_columnar_schema) {
        dest.columnar_schema = src.columnar_schema;
        dest.columnar_schema.name.clear();
        dest.has_columnar_schema = true;
    }
}

//! Gives a column made of the cells of another the columnar schema of the other (a decimal128, float32 or timestamp
//! schema, for example), with no name, so that its cells are returned and exported as those of the other (qore#6116)
static inline void dataFrameCopySchema(ColumnData& dest, const ColumnData& src) {
    if (src.has_columnar_schema) {
        dest.columnar_schema = src.columnar_schema;
        dest.columnar_schema.name.clear();
        dest.has_columnar_schema = true;
    }
}

//! Merges the columnar schema of a column into a schema for the cells of several columns of one type (qore#6116)
/** The first column gives the schema (has_schema and schema, with no name); each other column is merged into it:
    two decimal128 schemas make one with the largest number of integer digits and the largest scale (if it has at most
    38 digits), two temporal schemas one with the time unit "us" if their units differ and the time zone "UTC" if
    their zones differ (a plain date column is one of absolute dates in microseconds with no time zone), and a
    column that is nullable makes the schema nullable.

    @return true if the schema holds the cells of the column exactly, false if the columns cannot share one schema
    (columns of different types, a column of durations and one of absolute dates, decimals of more than 38 digits,
    buffers of different element types)
*/
DLLLOCAL bool dataFrameMergeColumnSchema(bool& has_schema, QoreColumnarTypeDescriptor& schema,
        const ColumnData& first, const ColumnData& src);

//! Returns true if the column is a decimal column: one that holds any value (numbers) with a decimal128 schema
static inline bool dataFrameIsDecimalColumn(const ColumnData& cd) {
    return cd.type == ColumnType::AUTO && cd.has_columnar_schema
        && cd.columnar_schema.kind == QoreColumnarTypeKind::Decimal128;
}

//! Gives the decimal schema of columns that are int64 and decimal columns, with at least one of each (qore#6144)
/** An int64 column counts as a decimal(19, 0) column: the schema has the largest number of integer digits and the
    largest scale of the columns, with the other fields of the first decimal schema, no native type, and nullable if
    a decimal schema is nullable or an int64 column has a null value (the schema of an int64 column is not read);
    if it would have more than 38 digits, there is none (has_schema is false), and the columns make a
    column that holds their exact values with no schema, as for the other decimal merges
    (dataFrameMergeColumnSchema()).

    @return true if the columns are int64 and decimal columns, with at least one of each, false otherwise (the
    schema is then not set)
*/
DLLLOCAL bool dataFrameIntDecimalSchema(const std::vector<const ColumnData*>& cols, bool& has_schema,
        QoreColumnarTypeDescriptor& schema);

//! Returns true if the columns are int64 and decimal columns, with at least one of each, with no scan of their
//! cells (qore#6144)
DLLLOCAL bool dataFrameIntDecimalTypes(const std::vector<const ColumnData*>& cols);

//! Returns a value of a column merged into a decimal column (qore#6144): an int of an int64 column as a number,
//! which holds it exactly, and any other value as it is, referenced
DLLLOCAL QoreValue dataFrameDecimalValue(const ColumnData& cd, int64_t row, ExceptionSink* xsink);

//! Appends the key of an arbitrary-precision number, as the key of a value of a column that holds any value
//! (qore#6144)
DLLLOCAL void dataFrameAppendNumberKey(std::string& key, const QoreNumberNode* n);

//! Makes a date column a column of durations: gives it the columnar schema of durations in microseconds, so that
//! its cells are returned as relative dates (qore#6107)
DLLLOCAL void dataFrameSetDurationSchema(ColumnData& col);

//! Appends the key of a value of a column that holds any value: its type and its exact value (qore#6107)
/** The key tells every two values apart that are not equal or not of the same type; it delimits itself (each part
    has its tag, and a part of any length its length), so it can be appended to a key of several values with no
    length of its own.
*/
DLLLOCAL void dataFrameAppendAutoValueKey(std::string& key, QoreValue v);

//! Appends the length of a part of a key and the part
static inline void appendDataFrameKeyBytes(std::string& key, const char* data, size_t len) {
    uint64_t n = len;
    key.append(reinterpret_cast<const char*>(&n), sizeof(n));
    key.append(data, len);
}

//! Appends the key of a cell that is not null: the key of its value as a column that holds any value keys it
//! (dataFrameAppendAutoValueKey()), whatever the type of the column, so that equal values of the same type in two
//! columns of different storage (an int64 column and a column that holds any value) make the same key (qore#6116)
static inline void dataFrameAppendCellKey(std::string& key, const ColumnData& cd, int64_t row) {
    assert(!cd.isNull(row));
    switch (cd.type) {
        case ColumnType::FLOAT64:
            appendDataFrameFloatKey(key, 'g', cd.float_data(row));
            return;
        case ColumnType::INT64: {
            int64 iv = cd.int_data[row];
            key += 'i';
            key.append(reinterpret_cast<const char*>(&iv), sizeof(iv));
            return;
        }
        case ColumnType::STRING:
            // a string cell is in UTF-8
            key += 's';
            appendDataFrameKeyBytes(key, cd.str_data[row].data(), cd.str_data[row].size());
            return;
        case ColumnType::BOOL:
            key += cd.bool_data[row] ? 'T' : 'F';
            return;
        case ColumnType::DATE: {
            // a date cell is in the range of an int64 of microseconds: an absolute date or a duration
            int64 micros = cd.date_data[row];
            key += cd.isDuration() ? 'r' : 'd';
            key.append(reinterpret_cast<const char*>(&micros), sizeof(micros));
            return;
        }
        case ColumnType::AUTO:
            dataFrameAppendAutoValueKey(key, cd.auto_data[row]);
            return;
        default:
            return;
    }
}

//! Returns a key of the value of a row that is equal for equal values of the column and different for others
/** A null value has a key of its own.  A float (in a float64 column or a column that holds any value) is keyed by
    its bits, so that two floats have the same key only if they are equal (with -0.0 equal to 0.0) or both NaN.
*/
DLLLOCAL std::string dataFrameValueKey(const ColumnData& cd, int64_t row);

//! Build a ColumnData from a list of Qore values with the given type
DLLLOCAL std::shared_ptr<ColumnData> buildColumnData(const QoreListNode* values,
    ColumnType type, ExceptionSink* xsink);

//! Build a ColumnData from a list of Qore values with auto-inferred type
DLLLOCAL std::shared_ptr<ColumnData> buildColumnDataAuto(const QoreListNode* values,
    ExceptionSink* xsink);

//! Build a ColumnData from a typed Qore buffer
DLLLOCAL std::shared_ptr<ColumnData> buildColumnDataFromBuffer(const QoreBufferNode* values,
    ExceptionSink* xsink);

//! Builds the column of rows selected from a column with dense storage, as the column was built from its buffer
/** A date column (a timestamp, date or duration column) keeps its dense buffer<int64> of microseconds, which is
    built as a date column, not as an int column (qore#6097)
*/
DLLLOCAL std::shared_ptr<ColumnData> buildColumnDataFromDenseRows(const ColumnData& src,
        const QoreBufferNode* values, ExceptionSink* xsink);

//! Build a date/time ColumnData from a dense int64 microsecond buffer
DLLLOCAL std::shared_ptr<ColumnData> buildColumnDataFromTemporalBuffer(const QoreBufferNode* values,
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
