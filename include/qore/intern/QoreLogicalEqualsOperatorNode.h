/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreLogicalEqualsOperatorNode.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_QORELOGICALEQUALSOPERATORNODE_H

#define _QORE_QORELOGICALEQUALSOPERATORNODE_H

#include "qore/intern/qore_string_private.h"
class QoreLogicalEqualsOperatorNode : public QoreBinaryOperatorNode<> {
public:
    DLLLOCAL static QoreLogicalEqualsOperatorNode* create(LexicalSemanticPolicy policy, const QoreProgramLocation* loc,
            QoreValue left, QoreValue right);
    DLLLOCAL QoreLogicalEqualsOperatorNode(const QoreProgramLocation* loc, QoreValue left, QoreValue right)
            : QoreBinaryOperatorNode<QoreOperatorNode>(loc, left, right), pfunc(nullptr) {
    }

    DLLLOCAL ~QoreLogicalEqualsOperatorNode() {
    }

    // if del is true, then the returned QoreString * should be deleted, if false, then it must not be
    DLLLOCAL virtual QoreString *getAsString(bool &del, int foff, ExceptionSink *xsink) const {
        del = false;
        return &logical_equals_str;
    }

    DLLLOCAL virtual int getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
        qore_string_private::get(str)->concatAsciiName(logical_equals_str);
        return 0;
    }

    // returns the type name as a c string
    DLLLOCAL virtual const char *getTypeName() const {
        return logical_equals_str.getBuffer();
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink *xsink) const {
        return copyBackgroundExplicit<QoreLogicalEqualsOperatorNode>(xsink);
    }

    //! compares the values of the operands as the operator does, with the specialization for their types that parse
    //! initialization selected; used by a switch case with the operator, whose value is the switch value (qore#6252)
    /** @param strict_boolean true if the operator has strict boolean semantics (create())
    */
    DLLLOCAL QoreValue evalValues(const QoreValue& l, const QoreValue& r, bool strict_boolean,
            ExceptionSink* xsink) const;

    //! returns true if parse initialization selected a specialization of the comparison for the types of the operands
    DLLLOCAL bool hasSpecialization() const {
        return pfunc != nullptr;
    }

    DLLLOCAL static bool softEqual(const QoreValue& l, const QoreValue& r, ExceptionSink *xsink);
    DLLLOCAL static bool softEqualLexical(const QoreValue& l, const QoreValue& r, ExceptionSink* xsink,
            LexicalSemanticPolicy policy);

    //! Native value API: obtain invocation policy only if comparison reaches boolean coercion.
    DLLLOCAL static bool softEqualNative(const QoreValue& l, const QoreValue& r, ExceptionSink* xsink);

    template <bool Lexical, bool Native = false>
    DLLLOCAL static bool softEqualImpl(const QoreValue& l, const QoreValue& r, ExceptionSink* xsink,
            LexicalSemanticPolicy policy);


    DLLLOCAL virtual const QoreTypeInfo *getTypeInfo() const {
        return typeInfo;
    }

protected:
    template <bool StrictBoolean>
    DLLLOCAL QORE_INTERN_ALWAYS_INLINE QoreValue evalCommon(bool& needs_deref, ExceptionSink* xsink) const;

    //! compares the values of the operands as evalCommon() does once it has evaluated them
    template <bool StrictBoolean>
    DLLLOCAL QORE_INTERN_ALWAYS_INLINE QoreValue evalValuesCommon(const QoreValue& l, const QoreValue& r,
            ExceptionSink* xsink) const;

    //! compares the values of the operands once the buffer and plugin operations do not apply
    template <bool StrictBoolean>
    DLLLOCAL QORE_INTERN_ALWAYS_INLINE QoreValue evalScalarsCommon(const QoreValue& l, const QoreValue& r,
            ExceptionSink* xsink) const;

    // type of pointer to optimized versions depending on arguments found at parse-time
    typedef bool(QoreLogicalEqualsOperatorNode::*eval_t)(ExceptionSink *xsink) const;
    // pointer to optimized versions depending on arguments found at parse-time
    eval_t pfunc;
    const QoreTypeInfo* typeInfo = boolTypeInfo;

    DLLLOCAL static QoreString logical_equals_str;

    DLLLOCAL virtual QoreValue evalImpl(bool &needs_deref, ExceptionSink *xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual const char* getPluginOperationName() const {
        return "eq";
    }

    DLLLOCAL virtual bool invertFallbackResult() const {
        return false;
    }

    DLLLOCAL bool floatSoftEqual(ExceptionSink *xsink) const;
    DLLLOCAL bool bigIntSoftEqual(ExceptionSink *xsink) const;
    DLLLOCAL bool boolSoftEqual(ExceptionSink *xsink) const;
};

#endif
