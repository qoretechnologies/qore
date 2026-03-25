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
*/
class QoreGroupedDataFrame : public AbstractPrivateData {
public:
    //! Construct from a source DataFrame and group column names
    DLLLOCAL QoreGroupedDataFrame(const QoreDataFrame* source,
        const std::vector<std::string>& group_cols, ExceptionSink* xsink);

    //! Aggregate with per-column function specs: {"col": "agg_func"}
    DLLLOCAL QoreDataFrame* agg(const QoreHashNode* agg_spec, ExceptionSink* xsink) const;

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

    //! Helper: apply a single aggregation function to a column's values at given indices
    DLLLOCAL double aggregateNumeric(const ColumnData& cd,
        const std::vector<int64_t>& indices, const std::string& func) const;
};

} // namespace QoreDataFrameNS

#endif // _QORE_DATAFRAME_QC_GROUPEDDATAFRAME_H
