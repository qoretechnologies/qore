/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreDotEvalOperatorNode.h

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

#ifndef _QORE_QOREDOTEVALOPERATORNODE_H

#define _QORE_QOREDOTEVALOPERATORNODE_H

#include "qore/intern/qore_string_private.h"

//! Check if a hash member is a call reference or closure
/** Used by both AST (evalWithBase) and JIT (qore_rt_dot_eval_method_direct)
    to dispatch hash-member call references.
*/
static inline const AbstractQoreNode* check_call_ref(const AbstractQoreNode* op, const char* name) {
    const QoreHashNode* h = reinterpret_cast<const QoreHashNode*>(op);
    const QoreValue ref = h->getKeyValue(name);
    return (ref.getType() == NT_FUNCREF || ref.getType() == NT_RUNTIME_CLOSURE) ? ref.getInternalNode() : nullptr;
}

//! Raises the exception that a method call raises for its receiver @a base before it evaluates its arguments, if any
/** As QoreDotEvalOperatorNode::evalWithBase() dispatches the call: an object that has been deleted (also through a
    weak reference), or a value whose pseudo-class has no method @a name; nothing for a hash whose member @a name is a
    call reference.  The call is given by its method name and by what parse initialization resolved (@a qc,
    @a method, @a variant, @a pseudo), so that the AOT runtime can make the check from the call's target as well
    (issue #5784).

    @return -1 if an exception was raised, 0 if not
*/
DLLLOCAL int qore_check_method_call_receiver(QoreValue base, const char* name, const QoreClass* qc,
        const QoreMethod* method, const AbstractQoreFunctionVariant* variant, bool pseudo, ExceptionSink* xsink);

class QoreDotEvalOperatorNode : public QoreOperatorNode {
protected:
    static QoreString name;

    QoreValue left{};
    MethodCallNode *m;

    const QoreTypeInfo *returnTypeInfo;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue evalLexical(bool& needs_deref, ExceptionSink* xsink) const;
    template <bool Lexical>
    DLLLOCAL QoreValue evalCommon(bool& needs_deref, ExceptionSink* xsink) const;
    template <bool Lexical>
    DLLLOCAL QoreValue evalWithBaseCommon(QoreValue base, ExceptionSink* xsink) const;


    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual const QoreTypeInfo *getTypeInfo() const {
        return returnTypeInfo;
    }

public:
    DLLLOCAL QoreDotEvalOperatorNode(const QoreProgramLocation* loc, QoreValue n_left, MethodCallNode* n_m)
            : QoreOperatorNode(loc), left(n_left), m(n_m), returnTypeInfo(nullptr) {
        //printd(5, "QoreDotEvalOperatorNode::QoreDotEvalOperatorNode() this: %p left: %p m: %p\n", this, &left, m);
    }

    DLLLOCAL ~QoreDotEvalOperatorNode() {
        // only a node deleted without being released holds values here; see derefImpl()
        left.discard(nullptr);
        if (m) {
            m->deref(nullptr);
        }
    }

    //! Releases the expression and the method call with the caller's exception sink
    /** See QoreSingleExpressionOperatorNode::derefImpl().
    */
    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink) {
        left.discard(xsink);
        left = QoreValue();
        if (m) {
            m->deref(xsink);
            m = nullptr;
        }
        return QoreOperatorNode::derefImpl(xsink);
    }

    DLLLOCAL QoreValue getExpression() const {
        return left;
    }

    //! Returns the method call node
    DLLLOCAL MethodCallNode* getMethodCall() const {
        return m;
    }

    DLLLOCAL void replaceExpression(QoreValue n_left) {
        left.discard(nullptr);
        left = n_left;
    }

    DLLLOCAL AbstractQoreNode* makeCallReference();

    // if del is true, then the returned QoreString * should be removed, if false, then it must not be
    DLLLOCAL virtual QoreString *getAsString(bool &del, int foff, ExceptionSink *xsink) const {
        del = false;
        return &name;
    }

    DLLLOCAL virtual int getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
        qore_string_private::get(str)->concat(&name);
        return 0;
    }

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const {
        return name.getBuffer();
    }

    DLLLOCAL virtual bool hasEffect() const {
        return true;
    }

    DLLLOCAL virtual bool hasEffectAsRoot() const {
        return true;
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink* xsink) const;

    //! Evaluate the method call dispatch on a pre-evaluated base value.
    /** Used by JIT/IR to avoid double-evaluating the base expression.
        The caller is responsible for managing the lifetime of @a base.
    */
    DLLLOCAL QoreValue evalWithBase(QoreValue base, ExceptionSink* xsink) const;

    //! Raises the exception that evalWithBase() raises for @a base before it evaluates the arguments, if any
    /** The compiled execution modes evaluate the arguments of a call before they dispatch it; they check the
        receiver first when the arguments can have side effects, so that a call on an object that has been deleted
        or on a value without the method evaluates no argument, as in the AST interpreter (issue #5784).

        @return -1 if an exception was raised, 0 if not
    */
    DLLLOCAL int checkReceiver(QoreValue base, ExceptionSink* xsink) const;
};

//! Parse-time choice of invocation entry; ordinary dot calls keep their existing virtual dispatch.
class LexicalDotEvalOperatorNode : public QoreDotEvalOperatorNode {
public:
    using QoreDotEvalOperatorNode::QoreDotEvalOperatorNode;
protected:
    QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const override {
        return evalLexical(needs_deref, xsink);
    }
};

template <typename... Args>
QoreDotEvalOperatorNode* qore_make_dot_call(LexicalSemanticPolicy policy, Args&&... args) {
    QoreDotEvalOperatorNode* node = policy.isDefault() ? new QoreDotEvalOperatorNode(std::forward<Args>(args)...)
        : new LexicalDotEvalOperatorNode(std::forward<Args>(args)...);
    ParseNode::bindSemanticPolicy(QoreValue(node), policy);
    return node;
}

#endif
