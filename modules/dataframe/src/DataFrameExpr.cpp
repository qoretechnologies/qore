/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    DataFrameExpr.cpp

    DataFrame expression helper implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_DataFrameExpr.h"

#include <utility>

namespace QoreDataFrameNS {

QoreDataFrameRowMask::QoreDataFrameRowMask(std::vector<uint8_t> mask) : mask(std::move(mask)) {
}

int64_t QoreDataFrameRowMask::size() const {
    return static_cast<int64_t>(mask.size());
}

int64_t QoreDataFrameRowMask::count(ExceptionSink* xsink) const {
    int64_t count = 0;
    for (size_t i = 0; i < mask.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "counting DataFrame row mask entries")) {
            return 0;
        }
        if (mask[i]) {
            ++count;
        }
    }
    return count;
}

QoreListNode* QoreDataFrameRowMask::toList(ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> list(new QoreListNode(boolTypeInfo), xsink);
    for (size_t i = 0; i < mask.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "converting DataFrame row mask to a list")) {
            return nullptr;
        }
        list->push(static_cast<bool>(mask[i]), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    return list.release();
}

QoreDataFrameColumnRef::QoreDataFrameColumnRef(QoreDataFrame* df, std::string column)
        : df(df), column(std::move(column)) {
}

QoreDataFrameColumnRef::~QoreDataFrameColumnRef() {
    if (df) {
        df->deref(nullptr);
    }
}

QoreDataFrameRowMask* QoreDataFrameColumnRef::compare(const char* op, QoreValue value,
        ExceptionSink* xsink) const {
    std::vector<uint8_t> mask = df->compareColumnMask(column, op, value, xsink);
    if (*xsink) {
        return nullptr;
    }
    return new QoreDataFrameRowMask(std::move(mask));
}

QoreListNode* QoreDataFrameColumnRef::values(ExceptionSink* xsink) const {
    return df->getColumn(column, xsink);
}

} // namespace QoreDataFrameNS
