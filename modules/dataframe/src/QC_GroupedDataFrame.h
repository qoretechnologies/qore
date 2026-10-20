/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_GroupedDataFrame.h

    GroupedDataFrame class declaration

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_DATAFRAME_QC_GROUPEDDATAFRAME_H
#define _QORE_DATAFRAME_QC_GROUPEDDATAFRAME_H

#include "QC_DataFrame.h"

#include <mutex>
#include <unordered_map>
#include <vector>

// Forward declarations for QPP
DLLEXPORT extern qore_classid_t CID_GROUPEDDATAFRAME;
DLLLOCAL extern QoreClass* QC_GROUPEDDATAFRAME;

DLLLOCAL void preinitGroupedDataFrameClass();
DLLLOCAL QoreClass* initGroupedDataFrameClass(QoreNamespace& ns);

namespace QoreDataFrameNS {

//! A grouped DataFrame for aggregation operations
/** Created by DataFrame::groupBy(). Holds group keys and row indices per group.
    Thread-safe: concurrent agg() calls are safe.

    It holds the columns of its DataFrame, and the GroupedDataFrame of an object reports the values of those columns
    that can be part of a cycle to the garbage collector (Pattern B in design/dgc.md); see attach().
*/
class QoreGroupedDataFrame : public AbstractScannablePrivateData {
public:
    //! Construct from a source DataFrame and group column names
    DLLLOCAL QoreGroupedDataFrame(const QoreDataFrame* source,
        const std::vector<std::string>& group_cols, ExceptionSink* xsink);

    //! Releases the columns that it reports to the garbage collector if destructor() was not called
    DLLLOCAL ~QoreGroupedDataFrame() override;

    //! Called once, after the GroupedDataFrame has become the private data of the object
    /** From now on it reports the values of its columns that need a scan to the garbage collector; a column whose
        values another holder reports already is replaced by a copy (see claimScanColumns()).
    */
    DLLLOCAL void attach(QoreObject& self);

    //! Called by the object's destructor: releases the columns
    DLLLOCAL void destructor(QoreObject& self, ExceptionSink* xsink);

    //! Reports the values of the columns that need a scan; see AbstractScannablePrivateData::scanValues()
    DLLLOCAL bool scanValues(QorePrivateDataScanner& scanner) override;

    //! The aggregation of one source column: the column name and the functions applied to it
    struct AggSpec {
        std::string column;              //!< the source column name, which may contain NUL characters
        std::vector<std::string> funcs;  //!< the aggregation functions
    };

    //! Aggregate with per-column function specs: {"col": "agg_func"}
    DLLLOCAL QoreDataFrame* agg(const QoreHashNode* agg_spec, ExceptionSink* xsink) const;

    //! Aggregate with per-column function specs given in order
    /** Unlike agg(), the column names are not hash keys, so they may contain NUL characters, as all DataFrame
        column names may.
    */
    DLLLOCAL QoreDataFrame* aggSpecs(const std::vector<AggSpec>& specs, ExceptionSink* xsink) const;

    //! Count rows per group
    DLLLOCAL QoreDataFrame* count(ExceptionSink* xsink) const;

    //! Sum numeric columns per group
    DLLLOCAL QoreDataFrame* sum(ExceptionSink* xsink) const;

    //! Mean of numeric columns per group
    DLLLOCAL QoreDataFrame* mean(ExceptionSink* xsink) const;

    //! Number of groups
    DLLLOCAL int64_t numGroups() const;

private:
    //! Group column names
    std::vector<std::string> group_col_names;

    //! Source column data (copies of group and agg columns from source)
    std::vector<Column> source_columns;
    std::unordered_map<std::string, size_t> source_col_index;

    //! Group key → list of row indices in source
    struct Group {
        std::vector<std::string> key_values;  //!< string representation of group key
        std::vector<int64_t> row_indices;
    };
    std::vector<Group> groups;

    int64_t source_n_rows = 0;
    mutable std::mutex mtx;

    //! Held while source_columns, attached and scan_columns are changed, and by the garbage collector's scan
    /** A leaf lock, taken after mtx; see QoreDataFrame::scan_mtx */
    mutable std::mutex scan_mtx;
    //! True from attach() until destructor(): the GroupedDataFrame reports its values to the collector
    bool attached = false;
    //! The number of columns whose values the GroupedDataFrame reports while attached
    size_t scan_columns = 0;

    //! Returns the specs applying the given function to each numeric column that is not a group column
    DLLLOCAL std::vector<AggSpec> numericSpecs(const char* func, ExceptionSink* xsink) const;

    //! Helper: apply a single aggregation function to a column's values at given indices
    DLLLOCAL double aggregateNumeric(const ColumnData& cd,
        const std::vector<int64_t>& indices, const std::string& func, ExceptionSink* xsink) const;
};

//! Makes a GroupedDataFrame the private data of an object; takes the reference of \a gdf
DLLLOCAL void setGroupedDataFramePrivate(QoreObject& obj, QoreGroupedDataFrame* gdf);

} // namespace QoreDataFrameNS

#endif // _QORE_DATAFRAME_QC_GROUPEDDATAFRAME_H
