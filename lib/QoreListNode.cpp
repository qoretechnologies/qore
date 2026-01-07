/*
    QoreListNode.cpp

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

#include <qore/Qore.h>
#include "qore/intern/qore_list_private.h"
#include "qore/intern/QoreParseListNode.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreListNodeEvalOptionalRefHolder.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define LIST_BLOCK 20
#define LIST_PAD   15

#ifndef QORE_QUICKSORT_LIMIT
#define QORE_QUICKSORT_LIMIT 1000
#endif

static QoreListNode* do_args(const QoreValue& e1, const QoreValue& e2) {
    QoreListNode* l = new QoreListNode(autoTypeInfo);
    qore_list_private* ll = qore_list_private::get(*l);
    ll->pushIntern(e1.refSelf());
    ll->pushIntern(e2.refSelf());
    return l;
}

QoreValue& QoreListNodeEvalOptionalRefHolder::getEntryReference(size_t index) {
    editIntern();
    return qore_list_private::get(*val)->getEntryReference(index);
}

// with this variant, we can edit and reuse references from "exp"
void QoreListNodeEvalOptionalRefHolder::evalIntern(QoreListNode* exp) {
    assert(!exp || exp->reference_count() == 1);

    if (!exp || exp->empty()) {
        val = nullptr;
        needs_deref = false;
        return;
    }
    needs_deref = true;
    val = new QoreListNode(autoTypeInfo);
    qore_list_private* vl = qore_list_private::get(*val);
    vl->reserve(exp->size());

    for (unsigned i = 0; i < exp->size(); ++i) {
        QoreValue& ev = exp->getEntryReference(i);
        QoreValue& vv = val->getEntryReference(i);
        if (!ev.needsEval() || ev.getType() == NT_REFERENCE || ev.getType() == NT_WEAKREF
            || ev.getType() == NT_WEAKREF_HASH || ev.getType() == NT_WEAKREF_LIST) {
            vv.swap(ev);
            continue;
        }
        vv = ev.eval(xsink);
        if (*xsink) {
            return;
        }
    }
    assert(exp->size() == val->size());
}

int qore_list_private::getLValue(size_t ind, LValueHelper& lvh, bool for_remove, ExceptionSink* xsink) {
    if (checkValid(xsink)) {
        return -1;
    }

    if (ind >= length) {
        resize(ind + 1);
    }

    lvh.resetValue(entry[ind], getValueTypeInfo());
    return 0;
}

int qore_list_private::parseInitComplexListInitialization(const QoreProgramLocation* loc,
        QoreParseContext& parse_context, QoreParseListNode* args, QoreValue& new_args) {
    const QoreTypeInfo* vti = parse_context.typeInfo;
    parse_context.typeInfo = nullptr;
    int err = 0;
    if (!parseInitListInitialization(loc, parse_context, args, new_args, err)) {
        if (parseCheckComplexListInitialization(loc, vti, parse_context.typeInfo, new_args, "initialize", true)
            && !err) {
            err = -1;
        }
    }
    return err;
}

int qore_list_private::parseInitListInitialization(const QoreProgramLocation* loc, QoreParseContext& parse_context,
        QoreParseListNode* args, QoreValue& new_args, int& err) {
    assert(!parse_context.typeInfo);
    assert(new_args.isNothing());

    if (!args) {
        return -1;
    }

    // initialize argument(s)
    new_args = args;
    if (parse_init_value(new_args, parse_context) && !err) {
        err = -1;
        return -1;
    }
    return 0;
}

int qore_list_private::parseCheckComplexListInitialization(const QoreProgramLocation* loc,
        const QoreTypeInfo* typeInfo, const QoreTypeInfo* expTypeInfo, const QoreValue exp,
        const char* context_action, bool strict_check) {
    const QoreTypeInfo* vti2 = QoreTypeInfo::getUniqueReturnComplexList(expTypeInfo);
    if (vti2) {
        // issue #4497: in case the expression is a list, check each element for compatibility
        if (!QoreTypeInfo::hasType(vti2)) {
            switch (exp.getType()) {
                case NT_LIST: {
                    ConstListIterator i(exp.get<const QoreListNode>());
                    while (i.next()) {
                        const QoreTypeInfo* eti = i.getValue().getFullTypeInfo();
                        if (!QoreTypeInfo::parseAccepts(typeInfo, eti)) {
                            parse_error(*loc, "cannot %s 'list<%s>' from a list typed with incompatible value type "
                                "'%s' in list element %d (starting from 1)",
                                context_action, QoreTypeInfo::getName(typeInfo), QoreTypeInfo::getName(eti),
                                i.index() + 1);
                            return -1;
                        }
                    }
                }
                case NT_PARSE_LIST: {
                    const type_vec_t& value_types = exp.get<const QoreParseListNode>()->getValueTypes();
                    for (unsigned i = 0, e = value_types.size(); i < e; ++i) {
                        const QoreTypeInfo* eti = value_types[i];
                        if (!QoreTypeInfo::parseAccepts(typeInfo, eti)) {
                            parse_error(*loc, "cannot %s 'list<%s>' from a list typed with incompatible value type "
                                "'%s' in list element %d (starting from 1)",
                                context_action, QoreTypeInfo::getName(typeInfo), QoreTypeInfo::getName(eti),
                                i + 1);
                            return -1;
                        }
                    }
                }
            }
        } else if (!QoreTypeInfo::parseAccepts(typeInfo, vti2)) {
            parse_error(*loc, "cannot %s 'list<%s>' from a list typed with incompatible value type '%s'",
                context_action, QoreTypeInfo::getName(typeInfo), QoreTypeInfo::getName(vti2));
            return -1;
        }
    } else {
        return parseCheckTypedAssignment(loc, exp, typeInfo, context_action, strict_check);
    }
    return 0;
}

int qore_list_private::parseCheckTypedAssignment(const QoreProgramLocation* loc, const QoreValue arg,
        const QoreTypeInfo* vti, const char* context_action, bool strict_check) {
    int err = 0;
    switch (arg.getType()) {
        case NT_LIST: {
            ConstListIterator i(arg.get<const QoreListNode>());
            while (i.next()) {
                const QoreTypeInfo* vti2 = i.getValue().getTypeInfo();
                bool may_not_match = false;
                qore_type_result_e res = QoreTypeInfo::parseAccepts(vti, vti2, may_not_match);
                if (res && (res == QTI_IDENT || (!strict_check || !may_not_match)))
                    continue;
                parse_error(*loc, "cannot %s 'list<%s>' from element %d/%d of a list with incompatible value type " \
                    "'%s'", context_action, QoreTypeInfo::getName(vti), (int)i.index() + 1, (int)i.max(),
                    QoreTypeInfo::getName(vti2));
                if (!err) {
                    err = -1;
                }
            }
            break;
        }
        case NT_PARSE_LIST: {
            const QoreParseListNode* pln = arg.get<const QoreParseListNode>();
            const type_vec_t& vtypes = pln->getValueTypes();
            for (unsigned i = 0; i < pln->size(); ++i) {
                const QoreTypeInfo* vti2 = vtypes[i];
                bool may_not_match = false;
                qore_type_result_e res = QoreTypeInfo::parseAccepts(vti, vti2, may_not_match);
                if (res && (res == QTI_IDENT || (!strict_check || !may_not_match)))
                    continue;
                parse_error(*loc, "cannot %s 'list<%s>' from element %d/%d of a list with incompatible value type " \
                    "'%s'", context_action, QoreTypeInfo::getName(vti), (int)(i + 1), (int)vtypes.size(),
                    QoreTypeInfo::getName(vti2));
                if (!err) {
                    err = -1;
                }
            }
            break;
        }
        default: {
            const QoreTypeInfo* vti2 = arg.getTypeInfo();
            bool may_not_match = false;
            qore_type_result_e res = QoreTypeInfo::parseAccepts(vti, vti2, may_not_match);
            if (res && (res == QTI_IDENT || (!strict_check || !may_not_match)))
                break;

            parse_error(*loc, "cannot %s 'list<%s>' from a value with incompatible type '%s'", context_action,
                QoreTypeInfo::getName(vti), QoreTypeInfo::getName(vti2));
            if (!err) {
                err = -1;
            }
        }
        break;
    }
    return err;
}

QoreListNode* qore_list_private::newComplexList(const QoreTypeInfo* typeInfo, const QoreValue args, ExceptionSink* xsink) {
    QoreValue val{};

    if (!args.isNothing()) {
        ValueEvalOptimizedRefHolder a(args, xsink);
        if (*xsink) {
            return nullptr;
        }

        val = a.takeReferencedValue();
    }

    return newComplexListFromValue(typeInfo, val, xsink);
}

QoreListNode* qore_list_private::newComplexListFromValue(const QoreTypeInfo* typeInfo, QoreValue init,
        ExceptionSink* xsink) {
    ValueHolder holder(init, xsink);

    QoreListNode* l;
    if (init.getType() == NT_LIST) {
        l = init.get<QoreListNode>();
        const QoreTypeInfo* lti = qore_list_private::get(*l)->complexTypeInfo;
        if (lti && QoreTypeInfo::equal(typeInfo, lti)) {
            return holder.release().get<QoreListNode>();
        }
        // try to fold the type
        if (!l->is_unique()) {
            holder = init = l = l->copy();
        }

        const QoreTypeInfo* vti = QoreTypeInfo::getReturnComplexListOrNothing(typeInfo);
        qore_list_private* ll = qore_list_private::get(*l);
        ListIterator i(l);
        while (i.next()) {
            if (!QoreTypeInfo::superSetOf(vti, i.getValue().getTypeInfo())) {
                QoreTypeInfo::acceptInputParam(vti, i.index(), nullptr, ll->getEntryReference(i.index()), xsink);
                if (*xsink) {
                    return nullptr;
                }
            }
        }
    } else if (init.getType() == NT_NOTHING) {
        // throw an exception if the type
        holder = init = l = new QoreListNode(autoTypeInfo);
    } else {
        const QoreTypeInfo* vti = QoreTypeInfo::getReturnComplexListOrNothing(typeInfo);
        QoreTypeInfo::acceptAssignment(vti, "<list assignment>", init, xsink);
        holder.release();
        holder = init;
        if (*xsink) {
           return nullptr;
        }

        l = new QoreListNode(autoTypeInfo);
        l->priv->pushIntern(holder.getReferencedValue());
        holder = init = l;
    }

    // mark new hash with new type
    assert(l->is_unique());
    l->priv->complexTypeInfo = typeInfo;
    return holder.release().get<QoreListNode>();
}

QoreListNode::QoreListNode() : AbstractQoreNode(NT_LIST, true, false), priv(new qore_list_private) {
   //printd(5, "QoreListNode::QoreListNode() 1 this=%p ne=%d v=%d\n", this, needs_eval_flag, value);
}

QoreListNode::QoreListNode(bool i) : AbstractQoreNode(NT_LIST, !i, i), priv(new qore_list_private) {
   //printd(5, "QoreListNode::QoreListNode() 2 this=%p ne=%d v=%d\n", this, needs_eval_flag, value);
}

QoreListNode::QoreListNode(const QoreTypeInfo* valueTypeInfo) : QoreListNode() {
    if (QoreTypeInfo::hasType(valueTypeInfo) || valueTypeInfo == autoTypeInfo) {
        priv->complexTypeInfo = qore_get_complex_list_type(valueTypeInfo);
    }
}

QoreListNode::~QoreListNode() {
    delete priv;
}

AbstractQoreNode* QoreListNode::realCopy() const {
    return copy();
}

bool QoreListNode::is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    const QoreListNode* l = v && v->getType() == NT_LIST ? reinterpret_cast<const QoreListNode*>(v) : nullptr;
    if (!l || l->size() != size()) {
        return false;
    }

    for (size_t i = 0; i < l->size(); ++i) {
        if (!l->retrieveEntry(i).isEqualSoft(retrieveEntry(i), xsink) || *xsink) {
            return false;
        }
    }
    return true;
}

bool QoreListNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    const QoreListNode* l = v && v->getType() == NT_LIST ? reinterpret_cast<const QoreListNode*>(v) : nullptr;
    if (!l || l->size() != size()) {
        return false;
    }

    for (size_t i = 0; i < l->size(); i++) {
        if (!l->retrieveEntry(i).isEqualHard(retrieveEntry(i))) {
            return false;
        }
    }
    return true;
}

// returns the type name as a c string
const char* QoreListNode::getTypeName() const {
    return getStaticTypeName();
}

QoreValue& QoreListNode::getEntryReference(size_t index) {
    return priv->getEntryReference(index);
}

const QoreValue QoreListNode::retrieveEntry(size_t num) const {
    if (num >= priv->length) {
        return QoreValue();
    }
    return priv->entry[num];
}

QoreValue QoreListNode::retrieveEntry(size_t num) {
    if (num >= priv->length) {
        return QoreValue();
    }
    return priv->entry[num];
}

QoreValue QoreListNode::getReferencedEntry(size_t num) const {
    if (num >= priv->length) {
        return QoreValue();
    }
    return priv->entry[num].refSelf();
}

int QoreListNode::getEntryAsInt(size_t num) const {
    if (num >= priv->length) {
        return 0;
    }
    return (int)priv->entry[num].getAsBigInt();
}

int QoreListNode::merge(const QoreListNode* list, ExceptionSink* xsink) {
    return priv->merge(list, xsink);
}

int QoreListNode::setEntry(size_t index, QoreValue val, ExceptionSink* xsink) {
    assert(reference_count() == 1);
    if (index >= priv->length) {
        priv->resize(index + 1);
    }
    if (needs_scan(priv->entry[index])) {
        priv->incScanCount(-1);
    }
    priv->entry[index].discard(xsink);
    priv->entry[index] = val;

    if (needs_scan(val)) {
        priv->incScanCount(1);
    }
    return xsink && *xsink ? -1 : 0;
}

int QoreListNode::push(QoreValue val, ExceptionSink* xsink) {
    if (priv->checkValid(xsink)) {
        return -1;
    }
    return priv->push(val, xsink);
}

#pragma GCC diagnostic ignored "-Wclass-memaccess"
int QoreListNode::insert(QoreValue val, ExceptionSink* xsink) {
    assert(reference_count() == 1);
    ValueHolder holder(val, xsink);
    if (priv->checkVal(holder, xsink)) {
        return -1;
    }

    priv->resize(priv->length + 1);
    if (priv->length - 1) {
        memmove(priv->entry + 1, priv->entry, sizeof(QoreValue) * (priv->length - 1));
    }
    priv->entry[0] = holder.release();
    if (needs_scan(val)) {
        priv->incScanCount(1);
    }
    return 0;
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic ignored "-Wclass-memaccess"
QoreValue QoreListNode::shift() {
    assert(reference_count() == 1);
    if (!priv->length) {
        return QoreValue();
    }
    QoreValue rv = priv->entry[0];
    size_t pos = priv->length - 1;
    memmove(priv->entry, priv->entry + 1, sizeof(QoreValue) * pos);
    priv->entry[pos] = QoreValue();
    priv->resize(pos);

    if (needs_scan(rv)) {
        priv->incScanCount(-1);
    }

    return rv;
}
#pragma GCC diagnostic pop

QoreValue QoreListNode::pop() {
    assert(reference_count() == 1);
    if (!priv->length) {
        return QoreValue();
    }
    QoreValue rv = priv->entry[priv->length - 1];
    size_t pos = priv->length - 1;
    priv->entry[pos] = QoreValue();
    priv->resize(pos);

    if (needs_scan(rv)) {
        priv->incScanCount(-1);
    }

    return rv;
}

QoreValue QoreListNode::evalImpl(bool &needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    if (!value) {
        return priv->eval(xsink);
    }
    needs_deref = false;
    return const_cast<QoreListNode*>(this);
}

QoreListNode* QoreListNode::evalList(ExceptionSink* xsink) const {
    if (!value) {
        return priv->eval(xsink);
    }
    ref();
    return const_cast<QoreListNode*>(this);
}

QoreListNode* QoreListNode::evalList(bool &needs_deref, ExceptionSink* xsink) const {
    if (!value) {
        needs_deref = true;
        return priv->eval(xsink);
    }
    needs_deref = false;
    return const_cast<QoreListNode*>(this);
}

QoreListNode* QoreListNode::copy() const {
    return priv->copy();
}

QoreListNode* QoreListNode::copyListFrom(size_t index) const {
    QoreListNode* nl = priv->getCopy();
    for (size_t i = index; i < priv->length; ++i) {
        nl->priv->pushIntern(priv->entry[i].refSelf());
    }

    return nl;
}

QoreListNode* QoreListNode::splice(ptrdiff_t offset) {
    assert(reference_count() == 1);
    size_t n_offset = priv->checkOffset(offset);
    if (n_offset == priv->length) {
        return nullptr;
    }

    return priv->spliceIntern(n_offset, priv->length - n_offset, false);
}

QoreListNode* QoreListNode::splice(ptrdiff_t offset, ptrdiff_t len) {
    assert(reference_count() == 1);
    size_t n_offset, n_len;
    priv->checkOffset(offset, len, n_offset, n_len);
    if (n_offset == priv->length) {
        return nullptr;
    }

    return priv->spliceIntern(n_offset, n_len, false);
}

QoreListNode* QoreListNode::splice(ptrdiff_t offset, ptrdiff_t len, const QoreValue l, ExceptionSink* xsink) {
    assert(reference_count() == 1);
    size_t n_offset, n_len;
    priv->checkOffset(offset, len, n_offset, n_len);
    return priv->spliceIntern(n_offset, n_len, l, false, xsink);
}

QoreListNode* QoreListNode::extract(ptrdiff_t offset) {
    assert(reference_count() == 1);
    size_t n_offset = priv->checkOffset(offset);
    if (n_offset == priv->length)
        return priv->getCopy();

    return priv->spliceIntern(n_offset, priv->length - n_offset, true);
}

QoreListNode* QoreListNode::extract(ptrdiff_t offset, ptrdiff_t len) {
    assert(reference_count() == 1);
    size_t n_offset, n_len;
    priv->checkOffset(offset, len, n_offset, n_len);
    if (n_offset == priv->length)
        return priv->getCopy();
    return priv->spliceIntern(n_offset, n_len, true);
}

QoreListNode* QoreListNode::extract(ptrdiff_t offset, ptrdiff_t len, const QoreValue l, ExceptionSink* xsink) {
    assert(reference_count() == 1);
    size_t n_offset, n_len;
    priv->checkOffset(offset, len, n_offset, n_len);
    return priv->spliceIntern(n_offset, n_len, l, true, xsink);
}

QoreListNode* QoreListNode::sort(ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> rv(copy(), xsink);
    if (priv->length) {
        // issue #4355: our quicksort algorithm can exhaust the stack with larger lists
        // use mergesort for lists > 1000 elements
        if (priv->length > QORE_QUICKSORT_LIMIT) {
            if (rv->priv->mergesort(nullptr, true, xsink)) {
                return nullptr;
            }
        } else {
            if (rv->priv->qsort(nullptr, 0, priv->length - 1, true, xsink)) {
                return nullptr;
            }
        }
    }

    return rv.release();
}

QoreListNode* QoreListNode::sortDescending(ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> rv(copy(), xsink);
    if (priv->length) {
        // issue #4355: our quicksort algorithm can exhaust the stack with larger lists
        // use mergesort for lists > 1000 elements
        if (priv->length > QORE_QUICKSORT_LIMIT) {
            if (rv->priv->mergesort(nullptr, false, xsink)) {
                return nullptr;
            }
        } else {
            if (rv->priv->qsort(nullptr, 0, priv->length - 1, false, xsink)) {
                return nullptr;
            }
        }
    }

    return rv.release();
}

QoreListNode* QoreListNode::sortDescending(const ResolvedCallReferenceNode* fr, ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> rv(copy(), xsink);
    if (priv->length) {
        // issue #4355: our quicksort algorithm can exhaust the stack with larger lists
        // use mergesort for lists > 1000 elements
        if (priv->length > QORE_QUICKSORT_LIMIT) {
            if (rv->priv->mergesort(fr, false, xsink)) {
                return nullptr;
            }
        } else {
            if (rv->priv->qsort(fr, 0, priv->length - 1, false, xsink)) {
                return nullptr;
            }
        }
    }

    return rv.release();
}

QoreListNode* qore_list_private::eval(ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> nl(getCopy(), xsink);
    //printd(5, "qore_list_private::eval() '%s' -> '%s'\n", QoreTypeInfo::getName(complexTypeInfo), get_full_type_name(*nl));
    for (size_t i = 0; i < length; ++i) {
        ValueEvalOptimizedRefHolder v(entry[i], xsink);
        if (*xsink) {
            return nullptr;
        }
        //printd(5, "qore_list_private::eval() %d: '%s'\n", i, v->getFullTypeName());
        nl->push(v.takeReferencedValue(), xsink);
        assert(!*xsink);
    }
    return nl.release();
}

// mergesort for controlled and interruptible sorts (stable)
int qore_list_private::mergesort(const ResolvedCallReferenceNode* fr, bool ascending, ExceptionSink* xsink) {
    //printd(5, "List::mergesort() ENTER this: %p, pgm: %p, f: %p length: %d\n", this, pgm, f, length);

    if (length <= 1) {
        return 0;
    }

    // separate list into two equal-sized lists
    ReferenceHolder<QoreListNode> left(new QoreListNode(autoTypeInfo), xsink);
    ReferenceHolder<QoreListNode> right(new QoreListNode(autoTypeInfo), xsink);
    qore_list_private* l = left->priv;
    qore_list_private* r = right->priv;
    size_t mid = length / 2;
    {
        size_t i = 0;
        for (; i < mid; ++i) {
            l->pushIntern(entry[i]);
        }
        for (; i < length; ++i) {
            r->pushIntern(entry[i]);
        }
    }

    // set length to 0 - the temporary lists own the entry references now
    length = 0;

    // mergesort the two lists
    if (l->mergesort(fr, ascending, xsink) || r->mergesort(fr, ascending, xsink)) {
        return -1;
    }

    // merge the resulting lists
    // use offsets and getAndClear() to avoid moving a lot of memory around
    size_t li = 0, ri = 0;
    while ((li < l->length) && (ri < r->length)) {
        QoreValue lv = l->entry[li];
        QoreValue rv = r->entry[ri];
        int rc;
        if (fr) {
            safe_qorelist_t args(do_args(lv, rv), xsink);
            ValueHolder result(fr->execValue(*args, xsink), xsink);
            if (*xsink) {
                return -1;
            }
            rc = (int)result->getAsBigInt();
        } else {
            rc = QoreLogicalComparisonOperatorNode::doComparison(lv, rv, xsink);
            if (*xsink) {
                return -1;
            }
        }
        if ((ascending && rc <= 0)
            || (!ascending && rc >= 0)) {
            pushIntern(l->getAndClear(li++));
        } else {
            pushIntern(r->getAndClear(ri++));
        }
    }

    // only one list will have entries left...
    while (li < l->length) {
        pushIntern(l->getAndClear(li++));
    }
    while (ri < r->length) {
        pushIntern(r->getAndClear(ri++));
    }

    //printd(5, "List::mergesort() EXIT this: %p, length: %d\n", this, length);

    return 0;
}

// quicksort for controlled and interruptible sorts (unstable)
/** FIXME: uses excessive stack
*/
int qore_list_private::qsort(const ResolvedCallReferenceNode* fr, size_t left, size_t right, bool ascending,
        ExceptionSink* xsink) {
#ifdef QORE_MANAGE_STACK
    // issue #4355 this quicksort algorithm can result in stack exhaustion
    if (check_stack(xsink)) {
        return -1;
    }
#endif

    size_t l_hold = left;
    size_t r_hold = right;
    QoreValue pivot = entry[left];

    while (left < right) {
        while (true) {
            int rc;
            if (fr) {
                safe_qorelist_t args(do_args(entry[right], pivot), xsink);
                ValueHolder rv(fr->execValue(*args, xsink), xsink);
                if (*xsink) {
                    return -1;
                }
                rc = (int)rv->getAsBigInt();
            } else {
                rc = QoreLogicalComparisonOperatorNode::doComparison(entry[right], pivot, xsink);
                if (*xsink) {
                    return -1;
                }
            }
            if ((left < right)
                && ((rc >= 0 && ascending)
                    || (rc < 0 && !ascending))) {
                --right;
            } else {
                break;
            }
        }

        if (left != right) {
            entry[left] = entry[right];
            ++left;
        }

        while (true) {
            int rc;
            if (fr) {
                safe_qorelist_t args(do_args(entry[left], pivot), xsink);
                ValueHolder rv(fr->execValue(*args, xsink), xsink);
                if (*xsink) {
                    return -1;
                }
                rc = (int)rv->getAsBigInt();
            } else {
                rc = QoreLogicalComparisonOperatorNode::doComparison(entry[left], pivot, xsink);
                if (*xsink) {
                    return -1;
                }
            }
            if ((left < right)
                && ((rc <= 0 && ascending)
                    || (rc > 0 && !ascending))) {
                ++left;
            } else {
                break;
            }
        }

        if (left != right) {
            entry[right] = entry[left];
            --right;
        }
    }
    entry[left] = pivot;
    size_t t_left = left;
    left = l_hold;
    right = r_hold;
    int rc = 0;
    if (left < t_left) {
        rc = qsort(fr, left, t_left - 1, ascending, xsink);
    }
    if (!rc && right > t_left) {
        rc = qsort(fr, t_left + 1, right, ascending, xsink);
    }
    return rc;
}

QoreListNode* QoreListNode::sort(const ResolvedCallReferenceNode* fr, ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> rv(copy(), xsink);
    if (priv->length) {
        // issue #4355: our quicksort algorithm can exhaust the stack with larger lists
        // use mergesort for lists > 1000 elements
        if (priv->length > QORE_QUICKSORT_LIMIT) {
            if (rv->priv->mergesort(fr, true, xsink)) {
                return nullptr;
            }
        } else {
            if (rv->priv->qsort(fr, 0, priv->length - 1, true, xsink)) {
                return nullptr;
            }
        }
    }

    return rv.release();
}

QoreListNode* QoreListNode::sortStable(ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> rv(copy(), xsink);
    if (priv->length) {
        if (rv->priv->mergesort(0, true, xsink)) {
            return nullptr;
        }
    }

    return rv.release();
}

QoreListNode* QoreListNode::sortDescendingStable(ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> rv(copy(), xsink);
    if (priv->length) {
        if (rv->priv->mergesort(0, false, xsink)) {
            return nullptr;
        }
    }

    return rv.release();
}

QoreListNode* QoreListNode::sortDescendingStable(const ResolvedCallReferenceNode* fr, ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> rv(copy(), xsink);
    if (priv->length) {
        if (rv->priv->mergesort(fr, false, xsink)) {
            return nullptr;
        }
    }

    return rv.release();
}

QoreListNode* QoreListNode::sortStable(const ResolvedCallReferenceNode* fr, ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> rv(copy(), xsink);
    if (priv->length) {
        if (rv->priv->mergesort(fr, true, xsink)) {
            return nullptr;
        }
    }

    return rv.release();
}

// does a deep dereference
bool QoreListNode::derefImpl(ExceptionSink* xsink) {
    for (size_t i = 0; i < priv->length; ++i) {
        priv->entry[i].discard(xsink);
    }
#ifdef DEBUG
    priv->length = 0;
#endif
    priv->valid = false;
    weakDeref();
    return false;
}

size_t QoreListNode::size() const {
    return priv->length;
}

bool QoreListNode::empty() const {
    return !priv->length;
}

QoreValue QoreListNode::min(ExceptionSink* xsink) const {
    if (!priv->length) {
        return QoreValue();
    }
    QoreValue rv = priv->entry[0];

    for (size_t i = 1; i < priv->length; ++i) {
        QoreValue v = priv->entry[i];
        if (QoreLogicalLessThanOperatorNode::doLessThan(v, rv, xsink)) {
            rv = v;
        }
        if (*xsink) {
            return QoreValue();
        }
    }
    return rv.refSelf();
}

QoreValue QoreListNode::max(ExceptionSink* xsink) const {
    if (!priv->length) {
        return QoreValue();
    }
    QoreValue rv = priv->entry[0];

    for (size_t i = 0; i < priv->length; ++i) {
        QoreValue v = priv->entry[i];

        if (QoreLogicalGreaterThanOperatorNode::doGreaterThan(v, rv, xsink)) {
            rv = v;
        }
        if (*xsink) {
            return QoreValue();
        }
    }
    return rv.refSelf();
}

QoreValue QoreListNode::min(const ResolvedCallReferenceNode* fr, ExceptionSink* xsink) const {
    if (!priv->length) {
        return QoreValue();
    }
    QoreValue rv = priv->entry[0];

    for (size_t i = 1; i < priv->length; ++i) {
        QoreValue v = priv->entry[i];

        safe_qorelist_t args(do_args(v, rv), xsink);
        ValueHolder result(fr->execValue(*args, xsink), xsink);
        if (*xsink) {
            return QoreValue();
        }
        if (result->getAsBigInt() < 0) {
            rv = v;
        }
    }
    return rv.refSelf();
}

QoreValue QoreListNode::max(const ResolvedCallReferenceNode* fr, ExceptionSink* xsink) const {
    if (!priv->length) {
        return QoreValue();
    }
    QoreValue rv = priv->entry[0];

    for (size_t i = 1; i < priv->length; ++i) {
        QoreValue v = priv->entry[i];

        safe_qorelist_t args(do_args(v, rv), xsink);
        ValueHolder result(fr->execValue(*args, xsink), xsink);
        if (*xsink) {
            return QoreValue();
        }
        if (result->getAsBigInt() > 0) {
            rv = v;
        }
    }
    return rv.refSelf();
}

QoreListNode* QoreListNode::reverse() const {
    QoreListNode* l = priv->getCopy();
    l->priv->resize(priv->length);
    for (size_t i = 0; i < priv->length; ++i) {
        l->priv->entry[i] = priv->entry[priv->length - i - 1].refSelf();
    }
    return l;
}

int QoreListNode::getAsString(QoreString &str, int foff, ExceptionSink* xsink) const {
    QoreContainerHelper cch(this);
    if (!cch) {
        str.sprintf("[ERROR: recursive reference to list %p]", this);
        return 0;
    }

    if (foff == FMT_YAML_SHORT) {
        str.concat('[');
        ConstListIterator li(this);
        while (li.next()) {
            QoreValue n = li.getValue();
            if (n.getAsString(str, foff, xsink))
                return -1;
            if (!li.last()) {
                str.concat(", ");
            }
        }
        str.concat(']');
        return 0;
    }

    if (foff <= FMT_YAML_LONG) {
        // Multi-line YAML format; indent encoded as FMT_YAML_LONG - foff
        int indent = FMT_YAML_LONG - foff;
        if (!size()) {
            if (indent > 0)
                str.addch(' ', indent);
            str.concat("[]");
            return 0;
        }
        ConstListIterator li(this);
        while (li.next()) {
            if (indent > 0)
                str.addch(' ', indent);
            str.concat('-');
            QoreValue v = li.getValue();
            qore_type_t vtype = v.getType();
            if (vtype == NT_LIST) {
                // List value: newline then indented content
                str.concat('\n');
                if (v.getAsString(str, foff - 2, xsink))
                    return -1;
            } else if (vtype == NT_HASH) {
                // Check if this is a simple single-key hash with non-complex value
                const QoreHashNode* h = v.get<const QoreHashNode>();
                if (h->size() == 1) {
                    ConstHashIterator hi(h);
                    hi.next();
                    QoreValue hv = hi.get();
                    qore_type_t hvtype = hv.getType();
                    if (hvtype != NT_HASH && hvtype != NT_LIST) {
                        // Simple single-key hash: output inline "- key: value"
                        str.concat(' ');
                        str.sprintf("%s: ", hi.getKey());
                        if (hv.getAsString(str, FMT_YAML_SHORT, xsink))
                            return -1;
                    } else {
                        // Single-key hash with complex value: newline then indented
                        str.concat('\n');
                        if (v.getAsString(str, foff - 2, xsink))
                            return -1;
                    }
                } else {
                    // Multi-key hash: newline then indented content
                    str.concat('\n');
                    if (v.getAsString(str, foff - 2, xsink))
                        return -1;
                }
            } else {
                // Simple value: space then value in YAML short format
                str.concat(' ');
                if (v.getAsString(str, FMT_YAML_SHORT, xsink))
                    return -1;
            }
            if (!li.last())
                str.concat('\n');
        }
        return 0;
    }

    if (!size()) {
        str.concat(&EmptyListString);
        return 0;
    }
    str.concat("list: (");

    if (foff != FMT_NONE) {
        str.sprintf("%lu element%s)\n", priv->length, priv->length == 1 ? "" : "s");
    }

    for (size_t i = 0; i < priv->length; ++i) {
        if (foff != FMT_NONE) {
            str.addch(' ', foff + 2);
            str.sprintf("[%lu]=", i);
        }

        QoreValue n = priv->entry[i];
        if (n.getAsString(str, foff != FMT_NONE ? foff + 2 : foff, xsink)) {
            return -1;
        }

        if (i != (priv->length - 1)) {
            if (foff != FMT_NONE) {
                str.concat('\n');
            }
            else {
                str.concat(", ");
            }
        }
    }
    if (foff == FMT_NONE) {
        str.concat(')');
    }

    return 0;
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using this function directly
QoreString *QoreListNode::getAsString(bool &del, int foff, ExceptionSink* xsink) const {
    del = false;
    if (!priv->length && foff != FMT_YAML_SHORT && foff > FMT_YAML_LONG) {
        return &EmptyListString;
    }

    TempString rv(new QoreString);
    if (getAsString(*(*rv), foff, xsink)) {
        return nullptr;
    }

    del = true;
    return rv.release();
}

bool QoreListNode::getAsBoolImpl() const {
    // check if we should do perl-style boolean evaluation
    if (runtime_check_parse_option(PO_STRICT_BOOLEAN_EVAL)) {
        return false;
    }
    return !empty();
}

ListIterator::ListIterator(QoreListNode* lst, size_t n_pos) : l(lst) {
    set(n_pos);
}

ListIterator::ListIterator(QoreListNode& lst, size_t n_pos) : l(&lst) {
    set(n_pos);
}

bool ListIterator::next() {
    if (++pos == (qore_offset_t)l->size()) {
        pos = -1;
        return false; // finished
    }
    return true;
}

bool ListIterator::prev() {
    if (l->empty())
        return false; // empty
    if (pos == -1) {
        pos = l->size() - 1;
        return true;
    }
    if (!pos) {
        pos = -1;
        return false; // finished
    }
    --pos;
    return true;
}

int ListIterator::set(size_t n_pos) {
    if (n_pos >= l->size()) {
        pos = -1;
        return -1;
    }
    pos = n_pos;
    return 0;
}

QoreValue ListIterator::getValue() const {
    return l->retrieveEntry(pos);
}

QoreValue ListIterator::getReferencedValue() const {
    return l->retrieveEntry(pos).refSelf();
}

bool ListIterator::last() const {
    return (bool)(pos == (qore_offset_t)(l->size() - 1));
}

bool ListIterator::first() const {
    return !pos;
}

ConstListIterator::ConstListIterator(const QoreListNode* lst, size_t n_pos) : l(lst) {
    set(n_pos);
}

ConstListIterator::ConstListIterator(const QoreListNode& lst, size_t n_pos) : l(&lst) {
    set(n_pos);
}

bool ConstListIterator::next() {
    if (++pos == (qore_offset_t)l->size()) {
        pos = -1;
        return false; // finished
    }
    return true;
}

bool ConstListIterator::prev() {
    if (l->empty())
        return false; // empty
    if (pos == -1) {
        pos = l->size() - 1;
        return true;
    }
    if (!pos) {
        pos = -1;
        return false; // finished
    }
    --pos;
    return true;
}

int ConstListIterator::set(size_t n_pos) {
    if (n_pos >= l->size()) {
        pos = -1;
        return -1;
    }
    pos = n_pos;
    return 0;
}

const QoreValue ConstListIterator::getValue() const {
    return l->retrieveEntry(pos);
}

QoreValue ConstListIterator::getReferencedValue() const {
    return l->retrieveEntry(pos).refSelf();
}

bool ConstListIterator::last() const {
    return (bool)(pos == (qore_offset_t)(l->size() - 1));
}

bool ConstListIterator::first() const {
    return !pos;
}

void ConstListIterator::reset() {
    pos = -1;
}

bool QoreListNode::isFinalized() const {
    return priv->finalized;
}

void QoreListNode::setFinalized() {
    priv->finalized = true;
}

bool QoreListNode::isVariableList() const {
    return priv->vlist;
}

void QoreListNode::setVariableList() {
    priv->vlist = true;
}

QoreListNode* QoreListNode::listRefSelf() const {
    ref();
    return const_cast<QoreListNode*>(this);
}

int QoreListNode::parseInit(QoreValue& val, QoreParseContext& parse_context) {
    assert(value);
    parse_context.typeInfo = priv->getTypeInfo();
    return 0;
}

const QoreTypeInfo* QoreListNode::getValueTypeInfo() const {
    return priv->getValueTypeInfo();
}

const QoreTypeInfo* QoreListNode::getTypeInfo() const {
    return priv->getTypeInfo();
}

void QoreListNode::weakRef() {
    priv->weakRef();
}

bool QoreListNode::weakDeref() {
    bool b = priv->weakDeref();
    if (b) {
        delete this;
    }
    return b;
}