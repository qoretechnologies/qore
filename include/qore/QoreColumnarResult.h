/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreColumnarResult.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_QORECOLUMNARRESULT_H
#define _QORE_QORECOLUMNARRESULT_H

#include <qore/Qore.h>
#include <qore/QoreArrowCData.h>
#include <qore/QoreBufferNode.h>

#include <cstdint>
#include <string>
#include <vector>

//! Dense column storage category used by QoreColumnarResult.
enum class QoreColumnarColumnType : uint8_t {
    Auto = 0,
    Bool,
    Int,
    Float,
    Number,
    String,
    Date,
    Binary,
};

//! Recursive columnar schema node kinds used by QoreColumnarResult v2 metadata.
enum class QoreColumnarTypeKind : uint8_t {
    Auto = 0,
    Bool,
    Int,
    Float,
    Number,
    String,
    Date,
    Binary,
    Timestamp,
    Duration,
    Decimal128,
    List,
    LargeList,
    FixedSizeList,
    Struct,
    Map,
    Dictionary,
};

//! Recursive columnar type descriptor used to preserve Arrow/Parquet-style schema metadata.
struct QoreColumnarTypeDescriptor {
    std::string name;
    QoreColumnarTypeKind kind = QoreColumnarTypeKind::Auto;
    QoreColumnarColumnType column_type = QoreColumnarColumnType::Auto;
    QoreBufferElementType buffer_type = QoreBufferElementType::Invalid;
    bool nullable = false;
    int32_t precision = 0;
    int32_t scale = 0;
    int32_t fixed_size = 0;
    std::string native_type;
    std::string time_unit;
    std::string timezone;
    std::string dictionary_index_type;
    std::vector<QoreColumnarTypeDescriptor> children;
};

//! Returns the source-level name for a columnar result column type.
DLLEXPORT const char* qore_columnar_column_type_name(QoreColumnarColumnType type);

//! Returns the source-level name for a recursive columnar schema kind.
DLLEXPORT const char* qore_columnar_type_kind_name(QoreColumnarTypeKind kind);

//! Column-oriented result set storage for DBI and DataFrame integrations.
class QoreColumnarResult : public AbstractPrivateData {
public:
    //! A single result column.
    struct Column {
        std::string name;
        QoreColumnarColumnType column_type = QoreColumnarColumnType::Auto;
        QoreBufferElementType buffer_type = QoreBufferElementType::Invalid;
        bool nullable = false;
        std::string native_type;
        QoreColumnarTypeDescriptor schema;
        QoreValue data;

        DLLLOCAL Column() = default;
        DLLLOCAL Column(std::string n_name, QoreColumnarColumnType n_column_type,
            QoreBufferElementType n_buffer_type, bool n_nullable, std::string n_native_type, QoreValue n_data);
        DLLLOCAL Column(std::string n_name, const QoreColumnarTypeDescriptor& n_schema, QoreValue n_data);
        DLLLOCAL Column(Column&& old) noexcept;
        DLLLOCAL Column& operator=(Column&& old) noexcept;
        DLLLOCAL Column(const Column&) = delete;
        DLLLOCAL Column& operator=(const Column&) = delete;
        DLLLOCAL ~Column();
    };

    //! Creates an empty columnar result.
    DLLEXPORT QoreColumnarResult();

    //! Creates a columnar result from an existing hash-of-lists result.
    DLLEXPORT static QoreColumnarResult* fromColumnHash(const QoreHashNode* columns, const QoreHashNode* desc,
        ExceptionSink* xsink);

    //! Creates a columnar result from an existing list-of-row-hashes result.
    DLLEXPORT static QoreColumnarResult* fromRows(const QoreListNode* rows, const QoreHashNode* desc,
        ExceptionSink* xsink);

    //! Creates a columnar result from a serialized payload.
    /** @param sdata serialized payload produced by serialize()
        @param xsink exception sink for validation, allocation, and cancellation errors
        @return a new columnar result, or nullptr if an exception was raised
        @throw DESERIALIZATION-ERROR if the payload has an unsupported version or invalid shape
     */
    DLLEXPORT static QoreColumnarResult* deserialize(const QoreHashNode* sdata, ExceptionSink* xsink);

    //! Returns a serialized payload for this columnar result.
    /** @param xsink exception sink for allocation and cancellation errors
        @return a versioned hash payload that can be passed to deserialize()
        @throw SERIALIZATION-ERROR if a column cannot be represented in the serialization format
     */
    DLLEXPORT QoreHashNode* serialize(ExceptionSink* xsink) const;

    //! Adds a column and takes ownership of @a data.
    DLLEXPORT int addColumn(const char* name, QoreValue data, QoreColumnarColumnType column_type,
        QoreBufferElementType buffer_type, bool nullable, const char* native_type, ExceptionSink* xsink);

    //! Adds a column with recursive schema metadata and takes ownership of @a data.
    DLLEXPORT int addColumn(const char* name, QoreValue data, const QoreColumnarTypeDescriptor& schema,
        ExceptionSink* xsink);

    //! Returns the number of rows.
    DLLEXPORT size_t numRows() const {
        return row_count;
    }

    //! Returns the number of columns.
    DLLEXPORT size_t numColumns() const {
        return columns.size();
    }

    //! Returns true if this result has no columns.
    DLLEXPORT bool empty() const {
        return columns.empty();
    }

    //! Returns the column at @a index.
    DLLEXPORT const Column* getColumn(size_t index) const;

    //! Finds a column by name.
    DLLEXPORT const Column* findColumn(const char* name) const;

    //! Returns column names in result order.
    DLLEXPORT QoreListNode* getColumnNames(ExceptionSink* xsink) const;

    //! Returns schema metadata in result order.
    DLLEXPORT QoreListNode* getSchema(ExceptionSink* xsink) const;

    //! Returns recursive schema metadata in result order.
    DLLEXPORT QoreListNode* getSchemaV2(ExceptionSink* xsink) const;

    //! Returns a hash of column values.
    /** Preserves the result's efficient storage: temporal columns (Date, Timestamp,
        Duration) keep their underlying \c buffer<int64> form (microsecond payload).
        Use this when the consumer can operate on column buffers directly.

        @see toMaterializedColumnHash() for a hash where temporal columns are
             reconstructed as \c list<date> for callers that need native Qore typing.
    */
    DLLEXPORT QoreHashNode* toColumnHash(ExceptionSink* xsink) const;

    //! Returns a hash of column values with native Qore typing restored.
    /** Like \c toColumnHash() but every temporal column is materialized into a
        \c list<date> with the result's current timezone applied; non-temporal
        columns pass through with no copy.  Use this only when the consumer
        requires native typing (e.g., assembling rows for a downstream API that
        cannot accept \c buffer<int64> microsecond payloads); prefer
        \c toColumnHash() in performance-sensitive code paths so the columnar
        representation survives.
    */
    DLLEXPORT QoreHashNode* toMaterializedColumnHash(ExceptionSink* xsink) const;

    //! Returns a list of row hashes.
    DLLEXPORT QoreListNode* toRows(ExceptionSink* xsink) const;

    //! Returns a new result with rows selected by a boolean list or buffer mask.
    DLLEXPORT QoreColumnarResult* filter(QoreValue mask, ExceptionSink* xsink) const;

    //! Returns a new result with rows selected by explicit zero-based indices.
    DLLEXPORT QoreColumnarResult* take(const QoreListNode* indices, ExceptionSink* xsink) const;

    //! Returns a contiguous row slice, preserving dense buffer columns as views.
    DLLEXPORT QoreColumnarResult* slice(size_t offset, size_t count, ExceptionSink* xsink) const;

    //! Combines two boolean row masks with && or || and returns a dense buffer<bool> mask.
    DLLEXPORT static QoreValue combineMasks(QoreValue lhs, QoreValue rhs, const char* op, ExceptionSink* xsink);

    //! Inverts a boolean row mask and returns a dense buffer<bool> mask.
    DLLEXPORT static QoreValue invertMask(QoreValue mask, ExceptionSink* xsink);

    //! Returns a dense buffer<bool> mask selecting null or non-null values in a column.
    DLLEXPORT QoreValue nullMask(const char* name, bool invert, ExceptionSink* xsink) const;

    //! Returns a dense buffer<bool> mask for a string predicate over a column.
    DLLEXPORT QoreValue stringPredicateMask(const char* name, const char* op, const QoreStringNode* value,
        bool ignore_case, ExceptionSink* xsink) const;

    //! Returns a referenced column container by name.
    DLLEXPORT QoreValue getColumnValue(const char* name, ExceptionSink* xsink) const;

protected:
    DLLEXPORT virtual ~QoreColumnarResult();

private:
    std::vector<Column> columns;
    size_t row_count = 0;

    DLLLOCAL int setRowCount(size_t n_rows, ExceptionSink* xsink);
};

//! Creates a Qore SQL::ColumnarResult object taking ownership of @a result.
DLLEXPORT QoreObject* qore_columnar_result_to_object(QoreColumnarResult* result, ExceptionSink* xsink);

//! Returns referenced ColumnarResult private data from @a obj, or nullptr if @a obj is not a ColumnarResult.
/** The caller owns the returned reference. Exceptions are only raised when the object is no longer valid.
 */
DLLEXPORT QoreColumnarResult* qore_columnar_result_try_from_object(const QoreObject* obj, ExceptionSink* xsink);

//! Creates a columnar result from a hash-of-columns, list-of-rows, or empty value.
DLLEXPORT QoreColumnarResult* qore_columnar_result_from_value(const QoreValue& value, const QoreHashNode* desc,
    const char* context, ExceptionSink* xsink);

//! Exports a columnar result through the Arrow C Data Interface.
/** The caller supplies storage for @a schema and @a array and owns the exported
    Arrow C Data objects after a successful call.  Release them by calling the
    embedded release callbacks or qore_arrow_schema_release() and
    qore_arrow_array_release().

    Fixed-width dense Qore buffers are exported without copying when host
    storage is directly available.  Microsecond timestamp and duration columns
    backed by \c buffer<int64> are exported with Arrow temporal schemas without
    copying value storage.  String, binary, list, fixed-size-list, struct, and
    map columns are exported with Qore-owned Arrow-compatible buffers when the
    source data is not already Arrow compatible.  Dictionary columns are
    exported as their decoded value type.

    @return 0 on success, non-zero on error
    @since %Qore 3.0
 */
DLLEXPORT int qore_columnar_result_export_arrow_c_data(const QoreColumnarResult* result, ArrowSchema* schema,
    ArrowArray* array, ExceptionSink* xsink);

//! Imports an Arrow C Data struct/record batch as a QoreColumnarResult.
/** On success, this function consumes @a schema and @a array by moving their
    contents into Qore-owned holders.  The caller must not release the input
    Arrow C Data objects after a successful import.  Fixed-width primitive,
    decimal, and microsecond timestamp/duration top-level arrays are wrapped
    without copying, including when the top-level struct/record batch has a
    non-zero offset.  String top-level arrays are materialized as
    \c buffer<string>; binary, list, fixed-size-list, struct, map, and
    dictionary arrays are materialized into Qore containers while preserving
    recursive schema metadata.

    @return a new QoreColumnarResult, or nullptr on error
    @since %Qore 3.0
 */
DLLEXPORT QoreColumnarResult* qore_columnar_result_import_arrow_c_data(ArrowSchema* schema, ArrowArray* array,
    ExceptionSink* xsink);

//! Releases an ArrowSchema if it has a release callback.
DLLEXPORT void qore_arrow_schema_release(ArrowSchema* schema);

//! Releases an ArrowArray if it has a release callback.
DLLEXPORT void qore_arrow_array_release(ArrowArray* array);

#endif
