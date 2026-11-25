/*
    QorePopOperatorNode.cpp

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

#include <qore/Qore.h>
#include "qore/intern/QoreStatementTemps.h"
#include "qore/intern/qore_list_private.h"

#include <optional>

QoreString QorePushOperatorNode::push_str("push operator expression");

QoreValue QorePushOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    return evalImplInternal<false>(needs_deref, xsink);
}

QoreValue QorePushOperatorNode::evalLexical(bool& needs_deref, ExceptionSink* xsink) const {
    return evalImplInternal<true>(needs_deref, xsink);
}

template <bool Lexical>
QORE_INTERN_ALWAYS_INLINE int QorePushOperatorNode::pushOn(LValueHelper& val, QoreValue converted,
        QoreMutationOperandHolder& res, QoreValue& rv, ExceptionSink* xsink) const {
    if (!converted.isNothing()) [[unlikely]] {
        val.saveTemp(converted);
    }

    // an element type that can bind a reference: the value is checked with a probe before anything is changed,
    // also before an lvalue with no value is assigned the list that the operation makes for it; the store is
    // made again if the probe records references (qore#7391)
    // a weak reference to a list: the helper designates its list before the probe of qore#7391 looks at it
    // (qore#7297)
    if (val.getType() == NT_WEAKREF_LIST && val.viewWeakContainer()) {
        return -1;
    }
    std::optional<QoreReferenceBindProbe> probe;
    {
        qore_type_t vt = val.getType();
        if ((vt == NT_LIST && qore_list_private::get(*val.getValue().get<const QoreListNode>())
                    ->elementMayBindReference())
                || (vt == NT_NOTHING && qore_list_push_nothing_may_bind(val))) [[unlikely]] {
            LexicalSemanticPolicy policy;
            if constexpr (Lexical) {
                policy = getSemanticPolicy();
            }
            val.enableBindRetry();
            if (qore_container_store_probe(val, *res, false, Lexical ? &policy : nullptr, probe)) {
                return -1;
            }
        }
    }

    // assign to a blank list if the lvalue has no value yet but is typed as a list or a softlist
    if (val.getType() == NT_NOTHING) {
        const QoreTypeInfo* vti = val.getTypeInfo();
        if (QoreTypeInfo::parseAcceptsReturns(vti, NT_LIST)) {
            // issue #3317: if the lvar can be a list, assign the current runtime type based on the declared
            // complex list type
            const QoreTypeInfo* lti = vti == autoTypeInfo
                ? autoTypeInfo
                : QoreTypeInfo::getReturnComplexListOrNothing(vti);
            if (val.assign(new QoreListNode(lti))) {
                assert(*xsink);
                return -1;
            }
        }
    }

    // value is not a list, so throw exception
    if (!val.holdsList()) {
        if (runtime_check_parse_option(PO_STRICT_ARGS))
            xsink->raiseException("PUSH-ERROR", "the lvalue argument to push is type \"%s\"; expecting \"list\"",
                val.getTypeName());
        return -1;
    }

    // a weak reference to a list: its list is changed, and the lvalue keeps the weak reference (qore#7297)
    QoreListNode* l = val.getListForChange();
    if (!l) {
        return -1;
    }
    val.changedScannedValue(*res);

    if constexpr (Lexical) {
        qore_list_private::push(*l, res.getReferencedValue(), xsink, getSemanticPolicy());
    } else {
        qore_list_private::pushCanonical(*l, res.getReferencedValue(), xsink);
    }
    if (probe) [[unlikely]] {
        probe.reset();
    }

    // reference for return value
    if (ref_rv) {
        rv = l->refSelf();
    }
    return 0;
}

template <bool Lexical>
QORE_INTERN_ALWAYS_INLINE QoreValue QorePushOperatorNode::evalImplInternal(bool& needs_deref, ExceptionSink* xsink) const {
    // the operand is a statement of its own where the other execution modes evaluate it so
    QoreMutationOperandHolder res(left, right, false, xsink);
    if (*xsink)
        return QoreValue();

    QoreValue rv;
    // the operation on the lvalue that the helper holds, inline on the normal path (pushOn()); made again in a retry
    // of the store, which takes the operand again (qore#7391)
    struct Op {
        const QorePushOperatorNode* node;
        QoreMutationOperandHolder& res;
        QoreValue& rv;
        ExceptionSink* xsink;
        QORE_INTERN_ALWAYS_INLINE int operator()(LValueHelper& h, QoreValue converted) {
            return node->pushOn<Lexical>(h, converted, res, rv, xsink);
        }
    } op{this, res, rv, xsink};

    // the first run of the operation and an lvalue path whose keys are evaluated first are handled out of line
    // (qore#7391)
    if (store_mode.get() != QoreStorePathMode::Known) [[unlikely]] {
        qore_ast_store_slow(store_mode, left, xsink, false, op);
    } else {
        qore_ast_store<false>(left, xsink, false, nullptr, op);
    }
    return rv;
}
