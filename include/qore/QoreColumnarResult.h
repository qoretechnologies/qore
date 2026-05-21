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

//! Returns the source-level name for a columnar result column type.
DLLEXPORT const char* qore_columnar_column_type_name(QoreColumnarColumnType type);

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
        QoreValue data;

        DLLLOCAL Column() = default;
        DLLLOCAL Column(std::string n_name, QoreColumnarColumnType n_column_type,
            QoreBufferElementType n_buffer_type, bool n_nullable, std::string n_native_type, QoreValue n_data);
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

    //! Adds a column and takes ownership of @a data.
    DLLEXPORT int addColumn(const char* name, QoreValue data, QoreColumnarColumnType column_type,
        QoreBufferElementType buffer_type, bool nullable, const char* native_type, ExceptionSink* xsink);

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

    //! Returns a hash of column values.
    DLLEXPORT QoreHashNode* toColumnHash(ExceptionSink* xsink) const;

    //! Returns a list of row hashes.
    DLLEXPORT QoreListNode* toRows(ExceptionSink* xsink) const;

    //! Returns a new result with rows selected by a boolean list or buffer mask.
    DLLEXPORT QoreColumnarResult* filter(QoreValue mask, ExceptionSink* xsink) const;

    //! Combines two boolean row masks with && or || and returns a dense buffer<bool> mask.
    DLLEXPORT static QoreValue combineMasks(QoreValue lhs, QoreValue rhs, const char* op, ExceptionSink* xsink);

    //! Inverts a boolean row mask and returns a dense buffer<bool> mask.
    DLLEXPORT static QoreValue invertMask(QoreValue mask, ExceptionSink* xsink);

    //! Returns a dense buffer<bool> mask selecting null or non-null values in a column.
    DLLEXPORT QoreValue nullMask(const char* name, bool invert, ExceptionSink* xsink) const;

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

//! Creates a columnar result from a hash-of-columns, list-of-rows, or empty value.
DLLEXPORT QoreColumnarResult* qore_columnar_result_from_value(const QoreValue& value, const QoreHashNode* desc,
    const char* context, ExceptionSink* xsink);

#endif
