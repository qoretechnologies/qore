/*
    QoreListAssignmentOperatorNode.cpp

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

#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreProgramObjectLockOps.h"

#include <optional>

QoreString QoreListAssignmentOperatorNode::op_str("list assignment operator expression");

namespace {
class LexicalListAssignmentOperatorNode : public QoreListAssignmentOperatorNode {
public:
    using QoreListAssignmentOperatorNode::QoreListAssignmentOperatorNode;
    QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const override {
        return evalCommon<true>(needs_deref, xsink);
    }
    QoreOperatorNode* copyBackground(ExceptionSink* xsink) const override {
        return copyBackgroundExplicit<LexicalListAssignmentOperatorNode>(xsink);
    }
};
}

int QoreListAssignmentOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    int err = parseInitIntern(parse_context);
    if (!err && getSemanticPolicy().needsFilter()) {
        auto* replacement = makeSpecialization<LexicalListAssignmentOperatorNode>();
        replacement->setParseAnalysis(parse_context.analysis);
        val = replacement;
    }
    return err;
}

int QoreListAssignmentOperatorNode::parseInitIntern(QoreParseContext& parse_context) {
    // turn off "return value ignored" flag
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    assert(left.getType() == NT_PARSE_LIST);
    QoreParseListNode* l = left.get<QoreParseListNode>();

    QoreParseListNodeParseInitHelper li(l);
    QorePossibleListNodeParseInitHelper ri(right, parse_context);

    int err = li.hasError() || ri.hasError() ? -1 : 0;

    const QoreTypeInfo* argInfo;
    while (li.next()) {
        ri.next();

        QoreValue v = li.parseInit(parse_context);
        const QoreTypeInfo* prototypeInfo = parse_context.typeInfo;
        // A failed parse (including a stack-limit error) can leave a declaration without its LocalVar. Only
        // validate initialized lvalues; keep parsing the right-hand side to collect its errors as usual.
        if (li.hasError()) {
            ri.parseInit(parse_context);
            continue;
        }
        //printd(5, "QoreListAssignmentOperatorNode::parseInitImpl() paI: '%s' (%s)\n", QoreTypeInfo::getName(prototypeInfo),
        //    v.getFullTypeName());
        if (int lvalue_err = check_lvalue(v)) {
            // -2: the error has been reported
            if (lvalue_err == -1 && !li.hasError()) {
                parse_error(*loc, "expecting lvalue in position %d of left-hand-side list in list assignment, got " \
                    "'%s' instead", li.index() + 1, v.getTypeName());
            }
            if (!err) {
                err = -1;
            }
        } else if (parse_context.pflag & PF_BACKGROUND_EXPR) {
            // a local, a member, or a value contained in one, which the thread copies (issue #5723)
            qore_background_lvalue_e kind = parse_check_background_lvalue(v);
            if (kind != qore_background_lvalue_e::SHARED) {
                QoreStringMaker what("position %d of left-hand-side list in list assignment", li.index() + 1);
                parse_raise_background_lvalue_error(loc, kind, what.c_str());
                if (!err) {
                    err = -1;
                }
            }
        }

        // check for illegal assignment to $self
        if (parse_context.oflag) {
            check_self_assignment(loc, v, parse_context.oflag);
        }

        ri.parseInit(parse_context);
        argInfo = parse_context.typeInfo;

        // a NOTHING element is not assigned, so any value is accepted for it (issue #6480)
        if (QoreTypeInfo::hasType(prototypeInfo) && !v.isNothing()) {
            bool may_not_match = false;
            bool may_need_filter = false;
            qore_type_result_e max_result = QTI_NOT_EQUAL;
            // only set the initial assignment flag if the lvalue is a declaration
            bool initial_assignment = (v.getType() == NT_VARREF && v.get<VarRefNode>()->parseIsDecl());
            qore_type_result_e res = QoreTypeInfo::parseAccepts(prototypeInfo, argInfo, may_not_match,
                may_need_filter, max_result, initial_assignment);
            //printd(5, "QoreListAssignmentOperatorNode::parseInitImpl() proto: %s <- %s: %d\n",
            //    QoreTypeInfo::getName(prototypeInfo), QoreTypeInfo::getName(argInfo), res);
            if (!res && !ri.hasError()) {
                // raise an exception only if parse exceptions are not disabled
                if (qore_program_ops(parse_context.pgm).getParseExceptionSink()) {
                    QoreStringNode* edesc = new QoreStringNode("lvalue for ListAssignment operator in position ");
                    edesc->sprintf("%d of list ListAssignment expects ", li.index() + 1);
                    QoreTypeInfo::describeTypeComparison(*edesc, prototypeInfo, ", but right-hand side is ", argInfo);
                    qore_program_private::makeParseException(parse_context.pgm, *loc, "PARSE-TYPE-ERROR", edesc);
                    if (!err) {
                        err = -1;
                    }
                }
            }
        }
    }

    while (ri.next()) {
        ri.parseInit(parse_context);
    }

    // the value of the expression is its right-hand side, not the last value assigned (issue #5704: a list
    // assignment in a typed hash initializer gave the key the type of the last lvalue, and the hash was refused)
    parse_context.typeInfo = ri.getTypeInfo();

    return li.hasError() || ri.hasError() || err ? -1 : 0;
}

QoreValue QoreListAssignmentOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    return evalCommon<false>(needs_deref, xsink);
}

//! Makes the retries of the store of the element \a i of a list assignment whose first round left its helper \a v
//! pending (qore#7391)
/** Out of line and cold: the reference elements that the check recorded are bound with no lvalue lock held, and the
    target \a lv is resolved again in \a v for each round, with \a keys if its keys were evaluated first; each round
    takes its value from the list again, and the value that the last round converted is released after the locks

    @return 0 if the store was made, -1 if not
*/
static QORE_INTERN_NOINLINE QORE_INTERN_COLD int qore_list_assign_bind_retry(LValueHelper& v, const QoreValue& lv,
        const QoreListNode* nv, const QoreValue& value, unsigned i, const QoreStorePathKeys* keys,
        const LexicalSemanticPolicy* policy) {
    auto resolve = [&lv, keys](LValueHelper& lvh) -> int {
        return qore_store_resolve_lvalue(lvh, lv, keys);
    };
    auto assign = [&](LValueHelper& lvh, QoreValue converted) -> int {
        lvh.saveTemp(converted);
        QoreValue assigned = nv ? nv->getReferencedEntry(i) : (!i ? value.refSelf() : QoreValue());
        return policy ? lvh.assign(assigned, "<lvalue>", *policy) : lvh.assign(assigned);
    };
    return qore_store_bind_retry_continue(v, resolve, assign);
}

template <bool Lexical, bool Slow>
QORE_INTERN_ALWAYS_INLINE QoreValue QoreListAssignmentOperatorNode::evalStores(ValueEvalOptimizedRefHolder& new_value,
        bool& needs_deref, ExceptionSink* xsink) const {
    const QoreParseListNode* llv = left.get<const QoreParseListNode>();
    const QoreListNode* nv = (new_value->getType() == NT_LIST ? new_value->get<QoreListNode>() : nullptr);

    // get values and save
    for (unsigned i = 0; i < llv->size(); i++) {
        QoreValue lv = llv->get(i);
        // NOTHING is an lvalue for check_lvalue(): its element is not assigned, and the elements after it are
        // (issue #6480)
        if (lv.isNothing()) {
            continue;
        }

        // the keys of a path that has key expressions which a retry of the store cannot evaluate again are evaluated
        // first, where the helper evaluates them, and released after the helper (qore#7391)
        std::optional<QoreStorePathKeys> path_keys;
        if constexpr (Slow) {
            if ((store_mode.get() & QoreStorePathMode::KeysFirst) && qore_lvalue_store_keys_first(lv)) {
                path_keys.emplace(xsink);
                if (path_keys->eval(lv)) {
                    return QoreValue();
                }
            }
        }
        const QoreStorePathKeys* keys = Slow && path_keys ? &*path_keys : nullptr;

        // get ptr to current value (lvalue is locked for the scope of the LValueHelper object); the helper of an
        // assignment has a retry (qore#7391)
        LValueHelper v(Slow && keys ? qore_store_no_lvalue : lv, xsink, false, true);
        if constexpr (Slow) {
            if (keys && v.resolveWithKeys(lv, *keys)) {
                return QoreValue();
            }
        }
        if (!v)
            return QoreValue();

        QoreValue assigned = nv ? nv->getReferencedEntry(i) : (!i ? new_value->refSelf() : QoreValue());
        int rc;
        if constexpr (Lexical) {
            rc = v.assign(assigned, "<lvalue>", getSemanticPolicy());
        } else {
            rc = v.assign(assigned);
        }
        // a store whose check records reference elements is made again out of line after they have been bound with
        // no lvalue lock held (qore#7391)
        if (rc && v.isBindPending()) [[unlikely]] {
            LexicalSemanticPolicy policy;
            if constexpr (Lexical) {
                policy = getSemanticPolicy();
            }
            if (qore_list_assign_bind_retry(v, lv, nv, *new_value, i, keys, Lexical ? &policy : nullptr)) {
                return QoreValue();
            }
        }
        if (*xsink) {
            return QoreValue();
        }
    }

    return ref_rv ? new_value.takeValue(needs_deref) : QoreValue();
}

template <bool Lexical>
QORE_INTERN_NOINLINE QoreValue QoreListAssignmentOperatorNode::evalStoresSlow(ValueEvalOptimizedRefHolder& new_value,
        bool& needs_deref, ExceptionSink* xsink) const {
    if (!store_mode.get()) {
        // the first run of the assignment decides whether the keys of a path are evaluated first (qore#7391)
        const QoreParseListNode* llv = left.get<const QoreParseListNode>();
        bool keys_first = false;
        for (unsigned i = 0; i < llv->size(); i++) {
            if (qore_lvalue_store_keys_first(llv->get(i))) {
                keys_first = true;
                break;
            }
        }
        store_mode.setKeysFirst(keys_first);
    }
    return evalStores<Lexical, true>(new_value, needs_deref, xsink);
}

template <bool Lexical>
QoreValue QoreListAssignmentOperatorNode::evalCommon(bool& needs_deref, ExceptionSink* xsink) const {
    assert(left.getType() == NT_PARSE_LIST);

    /* assign new value, this value gets referenced with the
        eval(xsink) call, so there's no need to reference it again
        for the variable assignment - however it does need to be
        copied/referenced for the return value
    */
    ValueEvalOptimizedRefHolder new_value(right, xsink);
    if (*xsink)
        return QoreValue();

    // the first run of the assignment and an assignment with an lvalue path whose keys are evaluated first are made out
    // of line (qore#7391)
    if (store_mode.get() != QoreStorePathMode::Known) [[unlikely]] {
        return evalStoresSlow<Lexical>(new_value, needs_deref, xsink);
    }
    return evalStores<Lexical, false>(new_value, needs_deref, xsink);
}
