/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHashMapSelectOperatorNode.h

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

#ifndef _QORE_QOREHASHMAPSELECTOPERATORNODE_H
#define _QORE_QOREHASHMAPSELECTOPERATORNODE_H

#include "qore/intern/AbstractIteratorHelper.h"

class QoreHashMapSelectOperatorNode : public QoreNOperatorNodeBase<4> {
    template <bool Lexical>
    friend class QoreGenericHashMapSelectOperatorNode;

public:
    DLLLOCAL static QoreHashMapSelectOperatorNode* create(LexicalSemanticPolicy policy, const QoreProgramLocation* loc,
        QoreValue p0, QoreValue p1, QoreValue p2, QoreValue p3);
    DLLLOCAL QoreHashMapSelectOperatorNode(const QoreProgramLocation* loc, QoreValue p0, QoreValue p1, QoreValue p2, QoreValue p3) :
        QoreNOperatorNodeBase<4>(loc, p0, p1, p2, p3) {
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;
    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const {
        return map_str.getBuffer();
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink* xsink) const {
        // the key, value and select expressions are evaluated for each element with the operator's implicit
        // arguments
        ValueHolder n_e0(copy_value_and_resolve_lvar_refs_bound_implicit(e[0], xsink), xsink);
        if (*xsink)
            return nullptr;
        ValueHolder n_e1(copy_value_and_resolve_lvar_refs_bound_implicit(e[1], xsink), xsink);
        if (*xsink)
            return nullptr;
        ValueHolder n_e2(copy_value_and_resolve_lvar_refs(e[2], xsink), xsink);
        if (*xsink)
            return nullptr;
        ValueHolder n_e3(copy_value_and_resolve_lvar_refs_bound_implicit(e[3], xsink), xsink);
        if (*xsink)
            return nullptr;
        auto* node = createBackgroundCopy(n_e0.release(), n_e1.release(), n_e2.release(), n_e3.release());
        node->returnTypeInfo = returnTypeInfo;
        return node;
    }

    DLLLOCAL virtual bool hasEffectAsRoot() const {
        return true;
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return returnTypeInfo;
    }

protected:
    //! Evaluates the operator; \a Generic is true if its result type has a type parameter (issue #5988)
    template <bool Lexical, bool Generic = false>
    DLLLOCAL QoreValue evalCommon(bool& needs_deref, ExceptionSink* xsink) const;
    const QoreTypeInfo* returnTypeInfo = nullptr;

    DLLLOCAL static QoreString map_str;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    /* Destructor
    */
    DLLLOCAL virtual ~QoreHashMapSelectOperatorNode() {
    }

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    template <bool Lexical, bool Generic = false>
    DLLLOCAL QoreValue mapIterator(AbstractIteratorHelper& h, ExceptionSink* xsink) const;

    DLLLOCAL QoreHashNode* getNewHash() const;

    //! Returns a new hash for the result with the type argument of the call in place of the type parameter
    /** For an operator whose result type has a type parameter (issue #5988)
    */
    DLLLOCAL QoreHashNode* getNewGenericHash() const;

    //! Returns a new operator of the class of this operator with the given operands for a background expression
    DLLLOCAL virtual QoreHashMapSelectOperatorNode* createBackgroundCopy(QoreValue p0, QoreValue p1, QoreValue p2,
            QoreValue p3) const {
        return create(getSemanticPolicy(), loc, p0, p1, p2, p3);
    }
};

//! A hash map select operator whose result type has a type parameter
/** Its result has the type argument of the call in place of the type parameter; an operator is replaced by this
    class when it is parsed, so the other operators keep their code path (issue #5988).  \a Lexical is the
    lexical semantic policy of the operator, as for the other operators.
*/
template <bool Lexical>
class QoreGenericHashMapSelectOperatorNode : public QoreHashMapSelectOperatorNode {
public:
    //! Takes over the operands and the parse state of the given operator
    DLLLOCAL QoreGenericHashMapSelectOperatorNode(QoreHashMapSelectOperatorNode& old);

    //! Creates an operator for a background expression
    DLLLOCAL QoreGenericHashMapSelectOperatorNode(const QoreProgramLocation* loc, QoreValue p0, QoreValue p1,
            QoreValue p2, QoreValue p3);

protected:
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreHashMapSelectOperatorNode* createBackgroundCopy(QoreValue p0, QoreValue p1, QoreValue p2,
            QoreValue p3) const;
};

#endif // QOREHASHMAPSELECTOPERATORNODE_H
