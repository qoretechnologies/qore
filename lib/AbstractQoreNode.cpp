/*
    AbstractQoreNode.cpp

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
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/QoreClosureNode.h"
#include "qore/intern/QoreParseHashNode.h"
#include "qore/intern/RuntimeConfig.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define TRACK_REFS 0

#if TRACK_REFS
#define REF_LVL (type!=NT_HASH)
#endif

AbstractQoreNode::AbstractQoreNode(qore_type_t t, bool n_value, bool n_needs_eval, bool n_there_can_be_only_one, bool n_custom_reference_handlers) : type(t), value(n_value), needs_eval_flag(n_needs_eval), there_can_be_only_one(n_there_can_be_only_one), custom_reference_handlers(n_custom_reference_handlers) {
#if TRACK_REFS
   printd(REF_LVL, "AbstractQoreNode::ref() %p type: %d (0->1)\n", this, type);
#endif
}

AbstractQoreNode::AbstractQoreNode(const AbstractQoreNode& v) : type(v.type), value(v.value), needs_eval_flag(v.needs_eval_flag), there_can_be_only_one(v.there_can_be_only_one), custom_reference_handlers(v.custom_reference_handlers) {
#if TRACK_REFS
   printd(REF_LVL, "AbstractQoreNode::ref() %p type: %d (0->1)\n", this, type);
#endif
}

AbstractQoreNode::~AbstractQoreNode() {
#if 0
   printd(5, "AbstractQoreNode::~AbstractQoreNode() type: %d (%s)\n", type, getTypeName());
#endif
}

/*
bool test(const AbstractQoreNode* n) {
   return n->getType() == NT_RTCONSTREF;
}
static void break_ref() {}
static void break_deref() {}
*/

void AbstractQoreNode::ref() const {
#ifdef DEBUG
/*
    if (test(this)) {
        printd(0, "AbstractQoreNode::ref() %p type: %d %s (%d->%d)\n", this, type, getTypeName(), reference_count(), reference_count() + 1);
        break_ref();
    }
*/
#if TRACK_REFS
    if (type == NT_OBJECT) {
        const QoreObject *o = reinterpret_cast<const QoreObject*>(this);
        printd(REF_LVL, "AbstractQoreNode::ref() %p type: %d object (%d->%d) object: %p, class: %s\n", this, type, references.load(), references.load() + 1, o, o->getClass()->getName());
    } else
        printd(REF_LVL, "AbstractQoreNode::ref() %p type: %d %s (%d->%d)\n", this, type, getTypeName(), references.load(), references.load() + 1);
#endif
#endif
    if (!there_can_be_only_one) {
        if (custom_reference_handlers)
            customRef();
        else
            ROreference();
    }
}

AbstractQoreNode* AbstractQoreNode::refSelf() const {
    ref();
    return const_cast<AbstractQoreNode*>(this);
}

bool AbstractQoreNode::derefImpl(ExceptionSink* xsink) {
    return true;
}

void AbstractQoreNode::customRef() const {
    assert(false);
}

void AbstractQoreNode::customDeref(ExceptionSink* xsink) {
    assert(false);
}

void AbstractQoreNode::deref(ExceptionSink* xsink) {
    //QORE_TRACE("AbstractQoreNode::deref()");
#ifdef DEBUG
/*
    if (test(this)) {
        printd(0, "AbstractQoreNode::deref() %p type: %d %s (%d->%d)\n", this, type, getTypeName(), reference_count(), reference_count() - 1);
        break_deref();
    }
*/
#if TRACK_REFS
    if (type == NT_OBJECT) {
        printd(REF_LVL, "QoreObject::deref() %p class: %s (%d->%d) %d\n", this, ((QoreObject*)this)->getClassName(),
            references.load(), references.load() - 1, custom_reference_handlers);
    } else {
        printd(REF_LVL, "AbstractQoreNode::deref() %p type: %d %s (%d->%d)\n", this, type, getTypeName(),
            references.load(), references.load() - 1);
    }
#endif
    if (references.load() > 10000000 || references.load() <= 0){
        if (type == NT_STRING) {
            printd(0, "AbstractQoreNode::deref() WARNING, node %p references: %d (type: %s) (val=\"%s\")\n",
                    this, references.load(), getTypeName(), ((QoreStringNode*)this)->getBuffer());
        } else {
            printd(0, "AbstractQoreNode::deref() WARNING, node %p references: %d (type: %s)\n", this,
                references.load(), getTypeName());
        }
        assert(false);
    }
#endif
    assert(references.load() > 0);

    if (there_can_be_only_one) {
        assert(is_unique());
        return;
    }

    if (custom_reference_handlers) {
        customDeref(xsink);
    } else if (ROdereference()) {
        if (type < NUM_SIMPLE_TYPES || derefImpl(xsink))
            delete this;
    }
}

QoreValue AbstractQoreNode::eval(ExceptionSink* xsink) const {
    if (!needs_eval_flag)
        return refSelf();

    bool needs_deref = true;
    QoreValue rv = evalImpl(needs_deref, xsink);
    return !needs_deref ? rv.refSelf() : rv;
}

QoreValue AbstractQoreNode::eval(bool& needs_deref, ExceptionSink* xsink) const {
    if (!needs_eval_flag) {
        needs_deref = false;
        return this;
    }

    needs_deref = true;
    QoreValue rv = evalImpl(needs_deref, xsink);

    switch (rv.getType()) {
        case NT_WEAKREF: {
            QoreObject* o = rv.get<WeakReferenceNode>()->get();
            if (needs_deref) {
                o->ref();
                rv.discard(xsink);
            }
            return o;
        }

        case NT_WEAKREF_HASH: {
            QoreHashNode* h = rv.get<WeakHashReferenceNode>()->get();
            if (needs_deref) {
                h->ref();
                rv.discard(xsink);
            }
            return h;
        }

        case NT_WEAKREF_LIST: {
            QoreListNode* l = rv.get<WeakListReferenceNode>()->get();
            if (needs_deref) {
                l->ref();
                rv.discard(xsink);
            }
            return l;
        }
    }

    return rv;
}

QoreValue AbstractQoreNode::eval(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    if (!needs_eval_flag) {
        needs_deref = false;
        return this;
    }

    needs_deref = true;
    QoreValue rv = evalImpl(rc, needs_deref, xsink);

    switch (rv.getType()) {
        case NT_WEAKREF: {
            QoreObject* o = rv.get<WeakReferenceNode>()->get();
            if (needs_deref) {
                o->ref();
                rv.discard(xsink);
            }
            return o;
        }

        case NT_WEAKREF_HASH: {
            QoreHashNode* h = rv.get<WeakHashReferenceNode>()->get();
            if (needs_deref) {
                h->ref();
                rv.discard(xsink);
            }
            return h;
        }

        case NT_WEAKREF_LIST: {
            QoreListNode* l = rv.get<WeakListReferenceNode>()->get();
            if (needs_deref) {
                l->ref();
                rv.discard(xsink);
            }
            return l;
        }
    }

    return rv;
}

bool AbstractQoreNode::getAsBool() const {
    return getAsBoolImpl();
}

int AbstractQoreNode::getAsInt() const {
    return getAsIntImpl();
}

int64 AbstractQoreNode::getAsBigInt() const {
    return getAsBigIntImpl();
}

double AbstractQoreNode::getAsFloat() const {
    return getAsFloatImpl();
}

// get the value of the type in a string context, empty string for complex types (default implementation)
QoreString* AbstractQoreNode::getStringRepresentation(bool& del) const {
    del = false;
    return NullString;
}

// empty default implementation
void AbstractQoreNode::getStringRepresentation(QoreString& str) const {
}

// if del is true, then the returned DateTime * should be deleted, if false, then it should not
DateTime *AbstractQoreNode::getDateTimeRepresentation(bool& del) const {
    del = false;
    return ZeroDate;
}

// assign date representation to a DateTime (no action for complex types = default implementation)
void AbstractQoreNode::getDateTimeRepresentation(DateTime& dt) const {
   dt.setDate(0LL);
}

int AbstractQoreNode::parseInit(QoreValue& val, QoreParseContext& parse_context) {
    // no action taken by default
    return 0;
}

// for getting relative time values or integer values
int getSecZeroInt(QoreValue a) {
    if (a.isNullOrNothing())
        return 0;

    if (a.getType() == NT_DATE)
        return (int)(a.get<const DateTimeNode>()->getRelativeSeconds());

    return (int)a.getAsBigInt();
}

int64 getSecZeroBigInt(QoreValue a) {
    if (a.isNullOrNothing())
        return 0;

    if (a.getType() == NT_DATE)
        return a.get<const DateTimeNode>()->getRelativeSeconds();

    return a.getAsBigInt();
}

// for getting relative time values or integer values
int getSecMinusOneInt(QoreValue a) {
    if (a.isNullOrNothing())
        return -1;

    if (a.getType() == NT_DATE)
        return (int)(a.get<const DateTimeNode>()->getRelativeSeconds());

    return (int)a.getAsBigInt();
}

int64 getSecMinusOneBigInt(QoreValue a) {
    if (a.isNullOrNothing())
        return -1;

    if (a.getType() == NT_DATE)
        return a.get<const DateTimeNode>()->getRelativeSeconds();

    return a.getAsBigInt();
}

int getMsZeroInt(QoreValue a) {
    if (a.isNullOrNothing())
        return 0;

    if (a.getType() == NT_DATE)
        return (int)(a.get<const DateTimeNode>()->getRelativeMilliseconds());

    return (int)a.getAsBigInt();
}

int64 getMsZeroBigInt(QoreValue a) {
    if (a.isNullOrNothing())
        return 0;

    if (a.getType() == NT_DATE)
        return a.get<const DateTimeNode>()->getRelativeMilliseconds();

    return a.getAsBigInt();
}

// for getting relative time values or integer values
int getMsMinusOneInt(QoreValue a) {
    if (a.isNullOrNothing())
        return -1;

    if (a.getType() == NT_DATE)
        return (int)(a.get<const DateTimeNode>()->getRelativeMilliseconds());

    return (int)a.getAsBigInt();
}

int64 getMsMinusOneBigInt(QoreValue a) {
    if (a.isNullOrNothing())
        return -1;

    if (a.getType() == NT_DATE)
        return a.get<const DateTimeNode>()->getRelativeMilliseconds();

    return a.getAsBigInt();
}

int getMicroSecZeroInt(QoreValue a) {
    if (a.isNullOrNothing())
        return 0;

    if (a.getType() == NT_DATE)
        return (int)(a.get<const DateTimeNode>()->getRelativeMicroseconds());

    return (int)a.getAsBigInt();
}

int64 getMicroSecZeroInt64(QoreValue a) {
    if (a.isNullOrNothing())
        return 0;

    if (a.getType() == NT_DATE)
        return (a.get<const DateTimeNode>()->getRelativeMicroseconds());

    return a.getAsBigInt();
}

// RuntimeConfig versions - primary implementations
static QoreListNode* crlr_list_copy(RuntimeConfig& rc, const QoreListNode* n, ExceptionSink* xsink) {
    assert(xsink);
    ReferenceHolder<QoreListNode> l(qore_list_private::get(*n)->getEmptyCopy(n->is_value()), xsink);
    for (unsigned i = 0; i < n->size(); i++) {
        l->push(copy_value_and_resolve_lvar_refs(rc, n->retrieveEntry(i), xsink), nullptr);
        if (*xsink) {
            return nullptr;
        }
    }
    return l.release();
}

static QoreParseListNode* crlr_list_copy(RuntimeConfig& rc, const QoreParseListNode* n, ExceptionSink* xsink) {
    assert(xsink);
    ReferenceHolder<QoreParseListNode> l(new QoreParseListNode(*n, xsink), xsink);
    return *xsink ? nullptr : l.release();
}

static AbstractQoreNode* crlr_hash_copy(RuntimeConfig& rc, const QoreHashNode* n, ExceptionSink* xsink) {
    assert(xsink);
    ReferenceHolder<QoreHashNode> h(qore_hash_private::get(*n)->getEmptyCopy(n->is_value()), xsink);
    ConstHashIterator hi(n);
    while (hi.next()) {
        h->setKeyValue(hi.getKey(), copy_value_and_resolve_lvar_refs(rc, hi.get(), xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    return h.release();
}

static AbstractQoreNode* crlr_hash_copy(RuntimeConfig& rc, const QoreParseHashNode* n, ExceptionSink* xsink) {
    assert(xsink);
    ReferenceHolder<QoreParseHashNode> h(new QoreParseHashNode(*n, xsink), xsink);
    return *xsink ? nullptr : h.release();
}

static AbstractQoreNode* crlr_selfcall_copy(RuntimeConfig& rc, const SelfFunctionCallNode* n, ExceptionSink* xsink) {
    QoreListNode* na = const_cast<QoreListNode*>(n->getArgs());
    if (na) {
        na = crlr_list_copy(rc, na, xsink);
    }
    return new SetSelfFunctionCallNode(*n, na);
}

static AbstractQoreNode* crlr_fcall_copy(RuntimeConfig& rc, const FunctionCallNode* n, ExceptionSink* xsink) {
    QoreListNode* na = const_cast<QoreListNode*>(n->getArgs());
    if (na) {
        na = crlr_list_copy(rc, na, xsink);
    }
    return new FunctionCallNode(*n, na);
}

static AbstractQoreNode* crlr_mcall_copy(RuntimeConfig& rc, const MethodCallNode* m, ExceptionSink* xsink) {
    assert(xsink);
    QoreListNode* args = const_cast<QoreListNode*>(m->getArgs());
    if (args) {
        ReferenceHolder<QoreListNode> args_holder(crlr_list_copy(rc, args, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        args = args_holder.release();
    }
    return new MethodCallNode(*m, args);
}

static AbstractQoreNode* crlr_smcall_copy(RuntimeConfig& rc, const StaticMethodCallNode* m, ExceptionSink* xsink) {
    assert(xsink);
    QoreListNode* args = const_cast<QoreListNode*>(m->getArgs());
    if (args) {
        ReferenceHolder<QoreListNode> args_holder(crlr_list_copy(rc, args, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        args = args_holder.release();
    }
    return new StaticMethodCallNode(*m, args);
}

static AbstractQoreNode* call_ref_call_copy(RuntimeConfig& rc, const CallReferenceCallNode* n, ExceptionSink* xsink) {
    assert(xsink);
    ValueHolder exp(copy_value_and_resolve_lvar_refs(rc, n->getExp(), xsink), xsink);
    if (*xsink) {
        return nullptr;
    }

    QoreListNode* args = const_cast<QoreListNode*>(n->getArgs());
    if (args) {
        ReferenceHolder<QoreListNode> args_holder(crlr_list_copy(rc, args, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        args = args_holder.release();
    }
    return new CallReferenceCallNode(n->loc, exp.release(), args);
}

// Non-RuntimeConfig versions - delegate to RuntimeConfig versions
static QoreListNode* crlr_list_copy(const QoreListNode* n, ExceptionSink* xsink) {
    RuntimeConfig rc = rc_get_current();
    return crlr_list_copy(rc, n, xsink);
}

static QoreParseListNode* crlr_list_copy(const QoreParseListNode* n, ExceptionSink* xsink) {
    RuntimeConfig rc = rc_get_current();
    return crlr_list_copy(rc, n, xsink);
}

static AbstractQoreNode* crlr_hash_copy(const QoreHashNode* n, ExceptionSink* xsink) {
    RuntimeConfig rc = rc_get_current();
    return crlr_hash_copy(rc, n, xsink);
}

static AbstractQoreNode* crlr_hash_copy(const QoreParseHashNode* n, ExceptionSink* xsink) {
    RuntimeConfig rc = rc_get_current();
    return crlr_hash_copy(rc, n, xsink);
}

static AbstractQoreNode* crlr_selfcall_copy(const SelfFunctionCallNode* n, ExceptionSink* xsink) {
    RuntimeConfig rc = rc_get_current();
    return crlr_selfcall_copy(rc, n, xsink);
}

static AbstractQoreNode* crlr_fcall_copy(const FunctionCallNode* n, ExceptionSink* xsink) {
    RuntimeConfig rc = rc_get_current();
    return crlr_fcall_copy(rc, n, xsink);
}

static AbstractQoreNode* crlr_mcall_copy(const MethodCallNode* m, ExceptionSink* xsink) {
    RuntimeConfig rc = rc_get_current();
    return crlr_mcall_copy(rc, m, xsink);
}

static AbstractQoreNode* crlr_smcall_copy(const StaticMethodCallNode* m, ExceptionSink* xsink) {
    RuntimeConfig rc = rc_get_current();
    return crlr_smcall_copy(rc, m, xsink);
}

static AbstractQoreNode* call_ref_call_copy(const CallReferenceCallNode* n, ExceptionSink* xsink) {
    RuntimeConfig rc = rc_get_current();
    return call_ref_call_copy(rc, n, xsink);
}

QoreValue copy_value_and_resolve_lvar_refs(const QoreValue& n, ExceptionSink* xsink) {
    RuntimeConfig rc = rc_get_current();
    return copy_value_and_resolve_lvar_refs(rc, n, xsink);
}

QoreValue copy_value_and_resolve_lvar_refs(RuntimeConfig& rc, const QoreValue& n, ExceptionSink* xsink) {
    if (!n.hasNode()) {
        return const_cast<QoreValue&>(n);
    }

    switch (n.getType()) {
        case NT_LIST:
            return crlr_list_copy(rc, n.get<const QoreListNode>(), xsink);

        case NT_PARSE_LIST:
            return crlr_list_copy(rc, n.get<const QoreParseListNode>(), xsink);

        case NT_HASH:
            return crlr_hash_copy(rc, n.get<const QoreHashNode>(), xsink);

        case NT_PARSE_HASH:
            return crlr_hash_copy(rc, n.get<const QoreParseHashNode>(), xsink);

        case NT_OPERATOR:
            return n.get<const QoreOperatorNode>()->copyBackground(xsink);

        case NT_SELF_CALL:
            return crlr_selfcall_copy(rc, n.get<const SelfFunctionCallNode>(), xsink);

        case NT_SELF_VARREF:
            {
                ValueEvalRefHolder vh(rc, n, xsink);
                return *xsink ? QoreValue() : vh.takeReferencedValue();
            }

        case NT_FUNCTION_CALL:
        case NT_PROGRAM_FUNC_CALL:
            return crlr_fcall_copy(rc, n.get<const FunctionCallNode>(), xsink);

        case NT_FIND:
            {
                ValueEvalRefHolder vh(rc, n, xsink);
                return *xsink ? QoreValue() : vh.takeReferencedValue();
            }

        case NT_VARREF: {
            const VarRefNode* var_ref = n.get<const VarRefNode>();
            if (var_ref->getType() != VT_GLOBAL) {
                ValueEvalRefHolder vh(rc, n, xsink);
                return *xsink ? QoreValue() : vh.takeReferencedValue();
            }
            break;
        }

        case NT_FUNCREFCALL:
            return call_ref_call_copy(rc, n.get<const CallReferenceCallNode>(), xsink);

        case NT_METHOD_CALL:
            return crlr_mcall_copy(rc, n.get<const MethodCallNode>(), xsink);

        case NT_STATIC_METHOD_CALL:
            return crlr_smcall_copy(rc, n.get<const StaticMethodCallNode>(), xsink);

        case NT_PARSEREFERENCE:
            return n.get<const ParseReferenceNode>()->evalToIntermediate(rc, xsink);

        case NT_CLOSURE:
            return n.get<const QoreClosureParseNode>()->evalBackground(xsink);
    }

    return n.refSelf();
}

// Default implementation - forwards to non-RuntimeConfig version for backwards compatibility
QoreValue AbstractQoreNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    return evalImpl(needs_deref, xsink);
}

void SimpleQoreNode::deref() {
    if (there_can_be_only_one) {
        assert(is_unique());
        return;
    }

    if (ROdereference())
        delete this;
}

QoreValue SimpleValueQoreNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
   assert(false);
   return QoreValue();
}

QoreValue SimpleValueQoreNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
   assert(false);
   return QoreValue();
}

AbstractQoreNode* UniqueValueQoreNode::realCopy() const {
   return const_cast<UniqueValueQoreNode*>(this);
}

int64 get_ms_zero(const QoreValue& n) {
    if (n.isNothing())
        return -1;
    if (n.getType() == NT_DATE)
        return n.get<const DateTimeNode>()->getRelativeMilliseconds();
    return n.getAsBigInt();
}

bool needs_scan(const AbstractQoreNode* n) {
    if (!n)
        return false;

    switch (n->getType()) {
        case NT_LIST: return qore_list_private::getScanCount(*static_cast<const QoreListNode*>(n)) ? true : false;
        case NT_HASH: return qore_hash_private::getScanCount(*static_cast<const QoreHashNode*>(n)) ? true : false;
        case NT_OBJECT: return true;
        case NT_RUNTIME_CLOSURE: return static_cast<const QoreClosureBase*>(n)->needsScan();
        case NT_REFERENCE: return lvalue_ref::get(static_cast<const ReferenceNode*>(n))->needsScan();
    }

    return false;
}

bool needs_scan(const QoreValue& v) {
    return needs_scan(v.getInternalNode());
}

void inc_container_obj(const AbstractQoreNode* n, int dt) {
    assert(n);
    switch (n->getType()) {
        case NT_LIST: qore_list_private::incScanCount(*static_cast<const QoreListNode*>(n), dt); break;
        case NT_HASH: qore_hash_private::incScanCount(*static_cast<const QoreHashNode*>(n), dt); break;
        case NT_OBJECT: qore_object_private::incScanCount(*static_cast<const QoreObject*>(n), dt); break;
        default: assert(false);
    }
}

bool has_complex_type(const AbstractQoreNode* n) {
    switch (get_node_type(n)) {
        case NT_LIST: {
            const QoreListNode* l = static_cast<const QoreListNode*>(n);
            const QoreTypeInfo* ti = l->getValueTypeInfo();
            return ti && ti != anyTypeInfo;
        }
        case NT_HASH: {
            const QoreHashNode* h = static_cast<const QoreHashNode*>(n);
            if (h->getHashDecl()) {
                return true;
            }
            const QoreTypeInfo* ti = h->getValueTypeInfo();
            return ti && ti != anyTypeInfo;
        }
        default:
            break;
    }
    return false;
}

static QoreHashNode* do_copy_strip(const QoreHashNode* h) {
    // issue #2791: do not strip types from a plain hash; no complex types can be stored there anyway
    if (h->getTypeInfo() == hashTypeInfo) {
        return h->hashRefSelf();
    }
    ReferenceHolder<QoreHashNode> nh(new QoreHashNode, nullptr);
    qore_hash_private* nhp = qore_hash_private::get(**nh);
    ConstHashIterator i(h);
    while (i.next()) {
        nhp->setKeyValueIntern(i.getKey(), copy_strip_complex_types(i.get()));
    }
    return nh.release();
}

static QoreListNode* do_copy_strip(const QoreListNode* l) {
    // issue #2791: do not strip types from a plain list; no complex types can be stored there anyway
    if (l->getTypeInfo() == listTypeInfo) {
        return l->listRefSelf();
    }
    ReferenceHolder<QoreListNode> nl(new QoreListNode, nullptr);
    ConstListIterator i(l);
    while (i.next()) {
        nl->push(copy_strip_complex_types(i.getValue()), nullptr);
    }
    return nl.release();
}

AbstractQoreNode* copy_strip_complex_types(const AbstractQoreNode* n) {
    if (!n)
        return nullptr;
    if (!has_complex_type(n))
        return n->refSelf();
    switch (get_node_type(n)) {
        case NT_LIST:
            return do_copy_strip(static_cast<const QoreListNode*>(n));
        case NT_HASH:
            return do_copy_strip(static_cast<const QoreHashNode*>(n));
        default:
            break;
    }
    return n->refSelf();
}

DLLLOCAL QoreValue copy_strip_complex_types(const QoreValue& n) {
    if (n.isNothing()) {
        return QoreValue();
    }
    if (!n.hasNode()) {
        return const_cast<QoreValue&>(n);
    }
    return QoreValue(copy_strip_complex_types(n.getInternalNode()));
}
