/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreLazyRangeNode.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.
*/

#ifndef _QORE_QORELAZYRANGENODE_H
#define _QORE_QORELAZYRANGENODE_H

#include <memory>
#include "qore/AbstractQoreNode.h"
#include "qore/QoreValue.h"
#include "qore/intern/QoreRangeOperatorNode.h"

class QoreLazyRangeNode : public AbstractQoreNode {
    QoreValue start;
    QoreValue end;
    mutable QoreValue cached_value;

public:
    QoreLazyRangeNode(const QoreValue& start_val, const QoreValue& end_val)
            : AbstractQoreNode(NT_LIST, false, true), start(start_val), end(end_val) {
    }

    DLLLOCAL virtual ~QoreLazyRangeNode() override;

    DLLLOCAL virtual const char* getTypeName() const override {
        return "list";
    }

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const override;
};

#endif
