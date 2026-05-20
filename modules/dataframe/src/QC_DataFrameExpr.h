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

    DLLLOCAL const std::vector<uint8_t>& getMask() const {
        return mask;
    }

    DLLLOCAL int64_t size() const;
    DLLLOCAL int64_t count(ExceptionSink* xsink) const;
    DLLLOCAL QoreListNode* toList(ExceptionSink* xsink) const;

private:
    std::vector<uint8_t> mask;
};

class QoreDataFrameColumnRef : public AbstractPrivateData {
public:
    //! Takes ownership of one already-referenced DataFrame private-data pointer.
    DLLLOCAL QoreDataFrameColumnRef(QoreDataFrame* df, std::string column);
    DLLLOCAL ~QoreDataFrameColumnRef() override;

    DLLLOCAL const std::string& getColumnName() const {
        return column;
    }

    DLLLOCAL QoreDataFrameRowMask* compare(const char* op, QoreValue value, ExceptionSink* xsink) const;
    DLLLOCAL QoreListNode* values(ExceptionSink* xsink) const;

private:
    QoreDataFrame* df = nullptr;
    std::string column;
};

} // namespace QoreDataFrameNS

#endif // _QORE_DATAFRAME_QC_DATAFRAMEEXPR_H
