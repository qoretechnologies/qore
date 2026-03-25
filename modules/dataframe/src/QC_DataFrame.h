/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_DataFrame.h

    DataFrame class declaration

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_DATAFRAME_QC_DATAFRAME_H
#define _QORE_DATAFRAME_QC_DATAFRAME_H

#include "df_column.h"

#include <mutex>
#include <unordered_map>
#include <vector>

// Forward declarations for QPP
DLLEXPORT extern qore_classid_t CID_DATAFRAME;
DLLLOCAL extern QoreClass* QC_DATAFRAME;

DLLLOCAL void preinitDataFrameClass();
DLLLOCAL QoreClass* initDataFrameClass(QoreNamespace& ns);

namespace QoreDataFrameNS {

//! Columnar DataFrame with typed columns and null support
/** Thread-safe: concurrent read operations after construction are safe.
    Mutation operations (addColumn, dropColumn, renameColumn) are serialized.
    Query operations (select, filter, etc.) return new DataFrames.
*/
class QoreDataFrame : public AbstractPrivateData {
public:
    //! Empty constructor
    DLLLOCAL QoreDataFrame();

    //! Construct from a list of records (list<hash>)
    DLLLOCAL static QoreDataFrame* fromRecords(const QoreListNode* records,
        ExceptionSink* xsink);

    //! Construct from a hash of column lists (hash<auto> where each value is a list)
    DLLLOCAL static QoreDataFrame* fromColumns(const QoreHashNode* columns,
        ExceptionSink* xsink);

    // --- Metadata ---

    DLLLOCAL int64_t numRows() const;
    DLLLOCAL int64_t numCols() const;
    DLLLOCAL QoreListNode* columnNames(ExceptionSink* xsink) const;
    DLLLOCAL QoreListNode* dtypes(ExceptionSink* xsink) const;
    DLLLOCAL QoreHashNode* shape(ExceptionSink* xsink) const;

    // --- Column/Row Access ---

    //! Get a column's values as a Qore list
    DLLLOCAL QoreListNode* getColumn(const std::string& name, ExceptionSink* xsink) const;

    //! Get a row as a hash
    DLLLOCAL QoreHashNode* getRow(int64_t index, ExceptionSink* xsink) const;

    //! Get the first n rows as a new DataFrame
    DLLLOCAL QoreDataFrame* head(int64_t n, ExceptionSink* xsink) const;

    //! Get the last n rows as a new DataFrame
    DLLLOCAL QoreDataFrame* tail(int64_t n, ExceptionSink* xsink) const;

    //! Get a slice [start, end) as a new DataFrame
    DLLLOCAL QoreDataFrame* slice(int64_t start, int64_t end, ExceptionSink* xsink) const;

    // --- Conversion ---

    //! Convert to list<hash<auto>>
    DLLLOCAL QoreListNode* toRecords(ExceptionSink* xsink) const;

    //! Convert to hash of lists (column-oriented)
    DLLLOCAL QoreHashNode* toColumnHash(ExceptionSink* xsink) const;

    // --- Statistics ---

    //! Per-column descriptive statistics
    DLLLOCAL QoreListNode* describe(ExceptionSink* xsink) const;

    // --- Display ---

    //! String representation for display
    DLLLOCAL QoreStringNode* toString(int64_t max_rows, ExceptionSink* xsink) const;

    // --- CSV I/O ---

    //! Read a CSV file into a DataFrame
    DLLLOCAL static QoreDataFrame* readCSV(const std::string& path,
        const QoreHashNode* options, ExceptionSink* xsink);

    //! Write this DataFrame to a CSV file
    DLLLOCAL void writeCSV(const std::string& path, const QoreHashNode* options,
        ExceptionSink* xsink) const;

    // --- Query Operations (return new DataFrames) ---

    //! Select a subset of columns
    DLLLOCAL QoreDataFrame* select(const QoreListNode* columns, ExceptionSink* xsink) const;

    //! Filter rows by a single condition: column op value
    DLLLOCAL QoreDataFrame* filter(const std::string& column, const std::string& op,
        QoreValue value, ExceptionSink* xsink) const;

    //! Sort by one or more columns
    DLLLOCAL QoreDataFrame* sortBy(const QoreListNode* columns,
        const QoreListNode* ascending, ExceptionSink* xsink) const;

    //! Concatenate DataFrames vertically (axis=0) or horizontally (axis=1)
    DLLLOCAL static QoreDataFrame* concat(const QoreListNode* dataframes, int axis,
        ExceptionSink* xsink);

    //! Fill null values with a constant
    DLLLOCAL QoreDataFrame* fillna(QoreValue value, ExceptionSink* xsink) const;

    //! Drop rows containing any null values
    DLLLOCAL QoreDataFrame* dropna(ExceptionSink* xsink) const;

    //! Access to internal columns for GroupedDataFrame (friend access)
    DLLLOCAL const std::vector<Column>& getColumns() const { return columns; }
    DLLLOCAL const std::unordered_map<std::string, size_t>& getColIndex() const {
        return col_index;
    }
    DLLLOCAL int64_t getNumRows() const { return n_rows; }
    DLLLOCAL std::mutex& getMutex() const { return mtx; }

    // --- Mutation (in-place, under lock) ---

    DLLLOCAL void addColumn(const std::string& name, const QoreListNode* data,
        ExceptionSink* xsink);
    DLLLOCAL void dropColumn(const std::string& name, ExceptionSink* xsink);
    DLLLOCAL void renameColumn(const std::string& old_name,
        const std::string& new_name, ExceptionSink* xsink);

private:
    friend class QoreGroupedDataFrame;

    std::vector<Column> columns;
    std::unordered_map<std::string, size_t> col_index;
    int64_t n_rows = 0;
    mutable std::mutex mtx;

    //! Get column index by name, or raise exception
    DLLLOCAL int getColIdx(const std::string& name, ExceptionSink* xsink) const;

    //! Build a new DataFrame from a contiguous row range
    DLLLOCAL QoreDataFrame* sliceRows(int64_t start, int64_t count,
        ExceptionSink* xsink) const;

    //! Build a new DataFrame from an arbitrary row index list
    DLLLOCAL QoreDataFrame* selectRows(const std::vector<int64_t>& indices,
        ExceptionSink* xsink) const;

    //! Rebuild col_index from columns vector
    DLLLOCAL void rebuildIndex();
};

} // namespace QoreDataFrameNS

#endif // _QORE_DATAFRAME_QC_DATAFRAME_H
