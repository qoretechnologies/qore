/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QorePreIncrementEqualsOperatorNode.h

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

#ifndef _QORE_QOREPREINCREMENTOPERATORNODE_H
#define _QORE_QOREPREINCREMENTOPERATORNODE_H

class QorePreIncrementOperatorNode : public QoreSingleExpressionOperatorNode<LValueOperatorNode> {
    friend class QorePostIncrementOperatorNode;

    OP_COMMON

protected:
    const QoreTypeInfo* typeInfo = nullptr;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL int parseInitIntern(const char* name, QoreParseContext& parse_context) {
        assert(!parse_context.typeInfo);
        // turn off "reference ok" and "return value ignored" flags
        QoreParseContextFlagHelper fh(parse_context);
        fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

        assert(!parse_context.typeInfo);
        int err = parse_init_value(exp, parse_context);

        if (!err && checkLValue(exp, parse_context.pflag)) {
            err = -1;
        }

        // make sure left side can take an integer or floating-point value
        if (check_lvalue_int_float_number(loc, parse_context.typeInfo, name) && !err) {
            err = -1;
        }

        // save return type
        typeInfo = parse_context.typeInfo;
        return err;
    }

    // always returns an int
    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return typeInfo;
    }

public:
    DLLLOCAL QorePreIncrementOperatorNode(const QoreProgramLocation* loc, QoreValue exp)
        : QoreSingleExpressionOperatorNode<LValueOperatorNode>(loc, exp) {
    }

    DLLLOCAL virtual bool hasEffect() const {
        return true;
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink* xsink) const {
        return copyBackgroundExplicit<QorePreIncrementOperatorNode>(xsink);
    }
};

#endif
