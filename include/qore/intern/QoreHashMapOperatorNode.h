/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHashMapOperatorNode.h

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#ifndef _QORE_QOREHASHMAPOPERATORNODE_H
#define _QORE_QOREHASHMAPOPERATORNODE_H

#include "qore/intern/AbstractIteratorHelper.h"

class QoreHashMapOperatorNode : public QoreNOperatorNodeBase<3> {
public:
    DLLLOCAL QoreHashMapOperatorNode(const QoreProgramLocation* loc, QoreValue p0, QoreValue p1, QoreValue p2) :
        QoreNOperatorNodeBase<3>(loc, p0, p1, p2) {
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;
    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const {
        return map_str.getBuffer();
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink* xsink) const {
        ValueHolder n_e0(copy_value_and_resolve_lvar_refs(e[0], xsink), xsink);
        if (*xsink)
            return nullptr;
        ValueHolder n_e1(copy_value_and_resolve_lvar_refs(e[1], xsink), xsink);
        if (*xsink)
            return nullptr;
        ValueHolder n_e2(copy_value_and_resolve_lvar_refs(e[2], xsink), xsink);
        if (*xsink)
            return nullptr;
        return new QoreHashMapOperatorNode(loc, n_e0.release(), n_e1.release(), n_e2.release());
    }

    DLLLOCAL static const QoreTypeInfo* setReturnTypeInfo(const QoreTypeInfo*& returnTypeInfo, const QoreTypeInfo* expTypeInfo2, const QoreTypeInfo* iteratorTypeInfo);

    DLLLOCAL virtual bool hasEffectAsRoot() const {
        return true;
    }

protected:
    const QoreTypeInfo* returnTypeInfo = nullptr;

    DLLLOCAL static QoreString map_str;

    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

    /* Destructor
    */
    DLLLOCAL virtual ~QoreHashMapOperatorNode() {
    }

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    inline DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return returnTypeInfo;
    }

    DLLLOCAL QoreValue mapIterator(RuntimeConfig& rc, AbstractIteratorHelper& h, ExceptionSink* xsink) const;

    DLLLOCAL QoreHashNode* getNewHash() const;
};

#endif // QOREHASHMAPOPERATORNODE_H
