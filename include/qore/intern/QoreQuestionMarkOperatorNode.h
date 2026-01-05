/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreQuestionMarkOperatorNode.h

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

#ifndef _QORE_QOREQUESTIONMARKOPERATORNODE_H
#define _QORE_QOREQUESTIONMARKOPERATORNODE_H

class QoreQuestionMarkOperatorNode : public QoreNOperatorNodeBase<3> {
protected:
    static QoreString question_mark_str;

    const QoreTypeInfo* typeInfo = nullptr;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

public:
    DLLLOCAL QoreQuestionMarkOperatorNode(const QoreProgramLocation* loc, QoreValue e0, QoreValue e1, QoreValue e2)
            : QoreNOperatorNodeBase<3>(loc, e0, e1, e2) {
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
        del = false;
        return &question_mark_str;
    }

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
        str.concat(&question_mark_str);
        return 0;
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return typeInfo;
    }

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const {
        return question_mark_str.getBuffer();
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
        return new QoreQuestionMarkOperatorNode(get_runtime_location(), n_e0.release(), n_e1.release(),
            n_e2.release());
    }

    DLLLOCAL virtual bool hasEffectAsRoot() const {
        return true;
    }
};

#endif
