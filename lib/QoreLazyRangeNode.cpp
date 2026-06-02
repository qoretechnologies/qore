/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreLazyRangeNode.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.
*/

#include <qore/ReferenceHolder.h>
#include <qore/intern/QoreLazyRangeNode.h>
#include <qore/intern/QoreRangeOperatorNode.h>

namespace {
class LazyRangeInvoker : public QoreRangeOperatorNode {
public:
    using QoreRangeOperatorNode::QoreRangeOperatorNode;
    using QoreRangeOperatorNode::evalImpl;
};
}

QoreValue QoreLazyRangeNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    needs_deref = true;

    if (!cached_value) {
        bool local_needs_deref = true;
        LazyRangeInvoker node(nullptr, start.refSelf(), end.refSelf());
        cached_value = node.evalImpl(local_needs_deref, xsink);
    }

    if (xsink && *xsink) {
        return QoreValue();
    }

    return cached_value.refSelf();
}

DLLLOCAL QoreLazyRangeNode::~QoreLazyRangeNode() = default;
