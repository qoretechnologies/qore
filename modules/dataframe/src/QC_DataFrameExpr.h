/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_DataFrameExpr.h

    DataFrame expression helper class declarations

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_DATAFRAME_QC_DATAFRAMEEXPR_H
#define _QORE_DATAFRAME_QC_DATAFRAMEEXPR_H

#include "QC_DataFrame.h"

#include <string>
#include <vector>

// Forward declarations for QPP
DLLEXPORT extern qore_classid_t CID_COLUMNREF;
DLLEXPORT extern qore_classid_t CID_ROWMASK;
DLLLOCAL extern QoreClass* QC_COLUMNREF;
DLLLOCAL extern QoreClass* QC_ROWMASK;

DLLLOCAL void preinitColumnRefClass();
DLLLOCAL void preinitRowMaskClass();
DLLLOCAL QoreClass* initColumnRefClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initRowMaskClass(QoreNamespace& ns);

namespace QoreDataFrameNS {

class QoreDataFrameRowMask : public AbstractPrivateData {
public:
    DLLLOCAL explicit QoreDataFrameRowMask(std::vector<uint8_t> mask);

    DLLLOCAL static QoreDataFrameRowMask* fromList(const QoreListNode* values, ExceptionSink* xsink);

    DLLLOCAL const std::vector<uint8_t>& getMask() const {
        return mask;
    }

    DLLLOCAL int64_t size() const;
    DLLLOCAL int64_t count(ExceptionSink* xsink) const;
    DLLLOCAL QoreListNode* toList(ExceptionSink* xsink) const;
    DLLLOCAL QoreDataFrameRowMask* intersect(const QoreDataFrameRowMask& other, ExceptionSink* xsink) const;
    DLLLOCAL QoreDataFrameRowMask* unionWith(const QoreDataFrameRowMask& other, ExceptionSink* xsink) const;
    DLLLOCAL QoreDataFrameRowMask* symmetricDifference(const QoreDataFrameRowMask& other,
        ExceptionSink* xsink) const;
    DLLLOCAL QoreDataFrameRowMask* invert(ExceptionSink* xsink) const;

private:
    std::vector<uint8_t> mask;
};

//! The private data of a ColumnRef: the name of the column
/** The DataFrame of a ColumnRef is held by the \c df internal member of its object, where the garbage collector
    follows it (Pattern A in design/dgc.md); see getColumnRefDataFrame().
*/
class QoreDataFrameColumnRef : public AbstractPrivateData {
public:
    DLLLOCAL explicit QoreDataFrameColumnRef(std::string column);

    DLLLOCAL const std::string& getColumnName() const {
        return column;
    }

    //! @param df the DataFrame of the ColumnRef
    DLLLOCAL QoreDataFrameRowMask* compare(const QoreDataFrame& df, const std::string& op, QoreValue value,
            ExceptionSink* xsink) const;
    //! @param df the DataFrame of the ColumnRef
    DLLLOCAL QoreListNode* values(const QoreDataFrame& df, ExceptionSink* xsink) const;

private:
    std::string column;
};

//! Returns the DataFrame of a ColumnRef object with a reference for the caller
/** @return the DataFrame, or nullptr if an exception was raised: \c OBJECT-ALREADY-DELETED if the DataFrame object
    was deleted
*/
DLLLOCAL QoreDataFrame* getColumnRefDataFrame(const QoreObject& colref, ExceptionSink* xsink);

} // namespace QoreDataFrameNS

#endif // _QORE_DATAFRAME_QC_DATAFRAMEEXPR_H
