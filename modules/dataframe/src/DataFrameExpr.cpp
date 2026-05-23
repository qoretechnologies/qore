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

QoreDataFrameRowMask* QoreDataFrameRowMask::fromList(const QoreListNode* values, ExceptionSink* xsink) {
    std::vector<uint8_t> mask;
    mask.reserve(values->size());
    for (size_t i = 0; i < values->size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame row mask from a list")) {
            return nullptr;
        }
        mask.push_back(values->retrieveEntry(i).getAsBool() ? 1 : 0);
    }
    return new QoreDataFrameRowMask(std::move(mask));
}

static bool validateMaskSizes(size_t lhs_size, size_t rhs_size, const char* operation, ExceptionSink* xsink) {
    if (lhs_size == rhs_size) {
        return true;
    }
    xsink->raiseException("DATAFRAME-ROWMASK-ERROR",
        "cannot %s DataFrame row masks with different lengths (left mask has %zu rows; right mask has %zu rows)",
        operation, lhs_size, rhs_size);
    return false;
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

QoreDataFrameRowMask* QoreDataFrameRowMask::intersect(const QoreDataFrameRowMask& other,
        ExceptionSink* xsink) const {
    if (!validateMaskSizes(mask.size(), other.mask.size(), "intersect", xsink)) {
        return nullptr;
    }

    std::vector<uint8_t> result(mask.size());
    for (size_t i = 0; i < mask.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "intersecting DataFrame row masks")) {
            return nullptr;
        }
        result[i] = mask[i] && other.mask[i];
    }
    return new QoreDataFrameRowMask(std::move(result));
}

QoreDataFrameRowMask* QoreDataFrameRowMask::unionWith(const QoreDataFrameRowMask& other,
        ExceptionSink* xsink) const {
    if (!validateMaskSizes(mask.size(), other.mask.size(), "build the union of", xsink)) {
        return nullptr;
    }

    std::vector<uint8_t> result(mask.size());
    for (size_t i = 0; i < mask.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "combining DataFrame row masks")) {
            return nullptr;
        }
        result[i] = mask[i] || other.mask[i];
    }
    return new QoreDataFrameRowMask(std::move(result));
}

QoreDataFrameRowMask* QoreDataFrameRowMask::symmetricDifference(const QoreDataFrameRowMask& other,
        ExceptionSink* xsink) const {
    if (!validateMaskSizes(mask.size(), other.mask.size(), "compute the symmetric difference of", xsink)) {
        return nullptr;
    }

    std::vector<uint8_t> result(mask.size());
    for (size_t i = 0; i < mask.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "combining DataFrame row masks")) {
            return nullptr;
        }
        result[i] = static_cast<bool>(mask[i]) != static_cast<bool>(other.mask[i]);
    }
    return new QoreDataFrameRowMask(std::move(result));
}

QoreDataFrameRowMask* QoreDataFrameRowMask::invert(ExceptionSink* xsink) const {
    std::vector<uint8_t> result(mask.size());
    for (size_t i = 0; i < mask.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "inverting DataFrame row mask")) {
            return nullptr;
        }
        result[i] = !mask[i];
    }
    return new QoreDataFrameRowMask(std::move(result));
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
