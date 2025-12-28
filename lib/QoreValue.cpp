/* indent-tabs-mode: nil -*- */
/*
    QoreValue.cpp

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

#include "qore/intern/ParseNode.h"
#include "qore/intern/QoreObjectIntern.h"

#ifndef DEBUG
#define QOREVALUE_USE_MEMCPY 1
#endif

const char* qoreBoolTypeName = "bool";
const char* qoreIntTypeName = "integer";
const char* qoreFloatTypeName = "float";

void QoreSimpleValue::set(const QoreSimpleValue& n) {
#ifdef QOREVALUE_USE_MEMCPY
    memcpy((void*)this, (void*)&n, sizeof(QoreSimpleValue));
#else
    type = n.type;
    switch (type) {
        case QV_Bool: v.b = n.v.b; break;
        case QV_Int: v.i = n.v.i; break;
        case QV_Float: v.f = n.v.f; break;
        case QV_Node: v.n = n.v.n; break;
        default:
            assert(false);
            // no break
    }
#endif
}

void QoreSimpleValue::set(AbstractQoreNode* n) {
    type = QV_Node;
    v.n = (n == &Nothing ? nullptr : n);
}

AbstractQoreNode* QoreSimpleValue::takeNode() {
    assert(type == QV_Node);
    return takeNodeIntern();
}

AbstractQoreNode* QoreSimpleValue::takeNodeIntern() {
    assert(type == QV_Node);
    AbstractQoreNode* rv = v.n;
    v.n = nullptr;
    return rv;
}

void QoreSimpleValue::clear() {
#ifdef DEBUG
    // this makes valgrind happier, but is functionally equivalent to the below,
    // which is more efficient as it doesn't write to memory unnecessarily
    type = QV_Node;
    v.n = nullptr;
#else
    if (type != QV_Node)
        type = QV_Node;
    if (v.n)
        v.n = nullptr;
#endif
}

void QoreSimpleValue::discard(ExceptionSink* xsink) {
    if (type == QV_Node && v.n) {
        v.n->deref(xsink);
        v.n = nullptr;
    }
}

bool QoreSimpleValue::getAsBool() const {
    switch (type) {
        case QV_Bool: return v.b;
        case QV_Int: return (bool)v.i;
        case QV_Float: return (bool)v.f;
        case QV_Node: return v.n ? v.n->getAsBool() : false;
        default: assert(false);
            // no break
    }
    return false;
}

int64 QoreSimpleValue::getAsBigInt() const {
    switch (type) {
        case QV_Bool: return (int64)v.b;
        case QV_Int: return v.i;
        case QV_Float: return (int64)v.f;
        case QV_Node: return v.n ? v.n->getAsBigInt() : 0;
        default: assert(false);
            // no break
    }
    return 0;
}

double QoreSimpleValue::getAsFloat() const {
    switch (type) {
        case QV_Bool: return (double)v.b;
        case QV_Int: return (double)v.i;
        case QV_Float: return v.f;
        case QV_Node: return v.n ? v.n->getAsFloat() : 0.0;
        default: assert(false);
            // no break
    }
    return 0.0;
}

qore_type_t QoreSimpleValue::getType() const {
    switch (type) {
        case QV_Bool: return NT_BOOLEAN;
        case QV_Int: return NT_INT;
        case QV_Float: return NT_FLOAT;
        case QV_Node: return v.n ? v.n->getType() : NT_NOTHING;
        default: assert(false);
            // no break
    }
    // to avoid a warning
    return NT_NOTHING;
}

const char* QoreSimpleValue::getTypeName() const {
    switch (type) {
        case QV_Bool: return qoreBoolTypeName;
        case QV_Int: return qoreIntTypeName;
        case QV_Float: return qoreFloatTypeName;
        case QV_Node: return get_type_name(v.n);
        default: assert(false);
            // no break
    }
    return nullptr;
}

AbstractQoreNode* QoreSimpleValue::getInternalNode() {
    return type == QV_Node ? v.n : nullptr;
}

const AbstractQoreNode* QoreSimpleValue::getInternalNode() const {
    return type == QV_Node ? v.n : nullptr;
}

bool QoreSimpleValue::hasEffect() const {
    return type == QV_Node && v.n && node_has_effect(v.n);
}

bool QoreSimpleValue::isEqualHard(const QoreValue n) const {
    qore_type_t t = getType();
    if (t != n.getType())
        return false;
    switch (t) {
        case NT_INT: return getAsBigInt() == n.getAsBigInt();
        case NT_BOOLEAN: return getAsBool() == n.getAsBool();
        case NT_FLOAT: return getAsFloat() == n.getAsFloat();
        case NT_NOTHING:
        case NT_NULL:
            return true;
    }
    ExceptionSink xsink;
    bool rv = !compareHard(v.n, n.v.n, &xsink);
    xsink.clear();
    return rv;
}

bool QoreSimpleValue::isNothing() const {
    return type == QV_Node && is_nothing(v.n);
}

bool QoreSimpleValue::isNull() const {
    return type == QV_Node && is_null(v.n);
}

bool QoreSimpleValue::isNullOrNothing() const {
    return type == QV_Node && (is_nothing(v.n) || is_null(v.n));
}

bool QoreSimpleValue::isValue() const {
    return type != QV_Node || (v.n && v.n->is_value());
}

bool QoreSimpleValue::isConstant() const {
    qore_type_t t = getType();
    return t >= NT_NOTHING && t <= NT_NUMBER;
}

bool QoreSimpleValue::needsEval() const {
    return type == QV_Node && v.n && v.n->needs_eval();
}

bool QoreSimpleValue::isScalar() const {
    if (type != QV_Node) {
        return true;
    }
    qore_type_t t = getType();
    return t == NT_STRING || t == NT_NUMBER;
}

QoreSimpleValue::operator bool() const {
    return !isNothing();
}

QoreValue::QoreValue() {
    set(nullptr);
}

QoreValue::QoreValue(bool b) {
    set(b);
}

QoreValue::QoreValue(int64 i) {
    set(i);
}

QoreValue::QoreValue(unsigned long long i) {
    set((int64)i);
}

QoreValue::QoreValue(int i) {
    set((int64)i);
}

QoreValue::QoreValue(unsigned int i) {
    set((int64)i);
}

QoreValue::QoreValue(long i) {
    set((int64)i);
}

QoreValue::QoreValue(unsigned long i) {
    set((int64)i);
}

QoreValue::QoreValue(double f) {
    set(f);
}

QoreValue::QoreValue(AbstractQoreNode* n) {
    set(n);
}

QoreValue::QoreValue(const AbstractQoreNode* n) {
    type = QV_Node;
    v.n = get_node_type(n) == NT_NOTHING ? nullptr : const_cast<AbstractQoreNode*>(n);
}

QoreValue::QoreValue(const QoreSimpleValue& n) {
#ifdef QOREVALUE_USE_MEMCPY
    memcpy((void*)this, (void*)&n, sizeof(QoreValue));
#else
    type = n.type;
    switch (type) {
        case QV_Bool: v.b = n.v.b; break;
        case QV_Int: v.i = n.v.i; break;
        case QV_Float: v.f = n.v.f; break;
        case QV_Node: v.n = n.v.n; break;
        default:
            assert(false);
            // no break
    }
#endif
}

QoreValue::QoreValue(const QoreValue& n) {
#ifdef QOREVALUE_USE_MEMCPY
    memcpy((void*)this, (void*)&n, sizeof(QoreValue));
#else
    type = n.type;
    switch (type) {
        case QV_Bool: v.b = n.v.b; break;
        case QV_Int: v.i = n.v.i; break;
        case QV_Float: v.f = n.v.f; break;
        case QV_Node: v.n = n.v.n; break;
        default:
            assert(false);
            // no break
    }
#endif
}

void QoreValue::swap(QoreValue& val) {
    QoreValue v1(*this);
    *this = val;
    val = v1;
}

QoreValue& QoreValue::operator=(const QoreValue& n) {
#ifdef QOREVALUE_USE_MEMCPY
    memcpy((void*)this, (void*)&n, sizeof(QoreValue));
#else
    type = n.type;
    switch (type) {
        case QV_Bool: v.b = n.v.b; break;
        case QV_Int: v.i = n.v.i; break;
        case QV_Float: v.f = n.v.f; break;
        case QV_Node: v.n = n.v.n; break;
        default:
            assert(false);
            // no break
    }
#endif
    return *this;
}

QoreValue& QoreValue::operator=(const QoreSimpleValue& n) {
#ifdef QOREVALUE_USE_MEMCPY
    memcpy((void*)this, (void*)&n, sizeof(QoreValue));
#else
    type = n.type;
    switch (type) {
        case QV_Bool: v.b = n.v.b; break;
        case QV_Int: v.i = n.v.i; break;
        case QV_Float: v.f = n.v.f; break;
        case QV_Node: v.n = n.v.n; break;
        default:
            assert(false);
            // no break
    }
#endif
    return *this;
}

void QoreValue::ref() const {
    if (type == QV_Node && v.n)
        v.n->ref();
}

QoreValue QoreValue::refSelf() const {
    ref();
    return const_cast<QoreValue&>(*this);
}

AbstractQoreNode* QoreValue::assign(AbstractQoreNode* n) {
    AbstractQoreNode* rv = takeIfNode();
    type = QV_Node;
    v.n = n && n->getType() == NT_NOTHING ? nullptr : n;
    return rv;
}

AbstractQoreNode* QoreValue::assign(const QoreValue n) {
    AbstractQoreNode* rv = takeIfNode();
#ifdef QOREVALUE_USE_MEMCPY
    memcpy((void*)this, (void*)&n, sizeof(QoreValue));
#else
    type = n.type;
    switch (type) {
        case QV_Bool: v.b = n.v.b; break;
        case QV_Int: v.i = n.v.i; break;
        case QV_Float: v.f = n.v.f; break;
        case QV_Node: v.n = n.v.n; break;
        default:
            assert(false);
            // no break
    }
#endif
    return rv;
}

AbstractQoreNode* QoreValue::assign(int64 n) {
    AbstractQoreNode* rv = takeIfNode();
    type = QV_Int;
    v.i = n;
    return rv;
}

AbstractQoreNode* QoreValue::assign(double n) {
    AbstractQoreNode* rv = takeIfNode();
    type = QV_Float;
    v.f = n;
    return rv;
}

AbstractQoreNode* QoreValue::assign(bool n) {
    AbstractQoreNode* rv = takeIfNode();
    type = QV_Bool;
    v.b = n;
    return rv;
}

AbstractQoreNode* QoreValue::assignNothing() {
    AbstractQoreNode* rv = takeIfNode();
    type = QV_Node;
    v.n = nullptr;
    return rv;
}

bool QoreValue::isEqualSoft(const QoreValue v, ExceptionSink* xsink) const {
    return QoreLogicalEqualsOperatorNode::softEqual(*this, v, xsink);
}

bool QoreValue::isEqualValue(const QoreValue n) {
    if (type != n.type) {
        return false;
    }
    switch (type) {
        case QV_Bool: return n.v.b == v.b;
        case QV_Float: return n.v.f == v.f;
        case QV_Int: return n.v.i == v.i;
        case QV_Node: return n.v.n == v.n;
        default:
            assert(false);
    }
    return false;
}

void QoreValue::discard(ExceptionSink* xsink) {
    if (type == QV_Node && v.n) {
        v.n->deref(xsink);
        v.n = nullptr;
    }
}

int QoreValue::getAsString(QoreString& str, int format_offset, ExceptionSink *xsink) const {
    if (isNothing()) {
        str.concat((format_offset == FMT_YAML_SHORT || format_offset <= FMT_YAML_LONG)
            ? &YamlNullString : &NothingTypeString);
        return 0;
    }
    switch (type) {
        case QV_Int: str.sprintf(QLLD, v.i); break;
        case QV_Bool: str.concat(v.b ? &TrueString : &FalseString); break;
        case QV_Float: {
            size_t offset = str.size();
            str.sprintf("%.9g", v.f);
            q_fix_decimal(&str, offset);
            break;
        }
        case QV_Node: return v.n->getAsString(str, format_offset, xsink);
        default:
            assert(false);
            // no break;
    }
    return 0;
}

QoreString* QoreValue::getAsString(bool& del, int format_offset, ExceptionSink* xsink) const {
    if (isNothing()) {
        del = false;
        return (format_offset == FMT_YAML_SHORT || format_offset <= FMT_YAML_LONG)
            ? &YamlNullString : &NothingTypeString;
    }
    switch (type) {
        case QV_Int: del = true; return new QoreStringMaker(QLLD, v.i);
        case QV_Bool: del = false; return v.b ? &TrueString : &FalseString;
        case QV_Float: {
            del = true;
            return q_fix_decimal(new QoreStringMaker("%.9g", v.f), 0);
        }
        case QV_Node: return v.n->getAsString(del, format_offset, xsink);
        default:
            assert(false);
            // no break;
    }
    return nullptr;
}

QoreValue QoreValue::eval(ExceptionSink* xsink) const {
    if (type != QV_Node || !v.n) {
        return *this;
    }

    return v.n->eval(xsink);
}

QoreValue QoreValue::eval(bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref == true);
    if (type != QV_Node || !v.n) {
        needs_deref = false;
        return *this;
    }

    return v.n->eval(needs_deref, xsink);
}

AbstractQoreNode* QoreValue::takeIfNode() {
    return type == QV_Node ? takeNodeIntern() : nullptr;
}

const char* QoreValue::getFullTypeName() const {
    switch (type) {
        case QV_Bool: return qoreBoolTypeName;
        case QV_Int: return qoreIntTypeName;
        case QV_Float: return qoreFloatTypeName;
        case QV_Node: return get_full_type_name(v.n);
        default: assert(false);
            // no break
    }
    return nullptr;
}

const char* QoreValue::getFullTypeName(bool with_namespaces) const {
    switch (type) {
        case QV_Bool: return qoreBoolTypeName;
        case QV_Int: return qoreIntTypeName;
        case QV_Float: return qoreFloatTypeName;
        case QV_Node: return get_full_type_name(v.n, with_namespaces);
        default: assert(false);
            // no break
    }
    return nullptr;
}

const char* QoreValue::getFullTypeName(bool with_namespaces, QoreString& scratch) const {
    switch (type) {
        case QV_Bool: return qoreBoolTypeName;
        case QV_Int: return qoreIntTypeName;
        case QV_Float: return qoreFloatTypeName;
        case QV_Node: return get_full_type_name(v.n, with_namespaces, scratch);
        default: assert(false);
            // no break
    }
    return nullptr;
}

bool QoreValue::hasNode() const {
    return type == QV_Node && v.n;
}

bool QoreValue::isReferenceCounted() const {
    return type == QV_Node && v.n && v.n->isReferenceCounted();
}

bool QoreValue::derefCanThrowException() const {
    return needs_scan(*this);
}

const QoreTypeInfo* QoreValue::getTypeInfo() const {
    switch (type) {
        case QV_Bool: return boolTypeInfo;
        case QV_Int: return bigIntTypeInfo;
        case QV_Float: return floatTypeInfo;
        case QV_Node: return getTypeInfoForValue(v.n);
        default: assert(false);
    }
    return nullptr;
}

const QoreTypeInfo* QoreValue::getFullTypeInfo() const {
    switch (getType()) {
        case NT_OBJECT: return get<const QoreObject>()->getClass()->getTypeInfo();
        case NT_HASH: {
            const QoreHashNode* h = get<const QoreHashNode>();
            const TypedHashDecl* thd = h->getHashDecl();
            if (thd) {
                return thd->getTypeInfo();
            }
            break;
        }
        default:
            break;
    }
    return getTypeInfo();
}

ValueHolder::~ValueHolder() {
    discard(v.getInternalNode(), xsink);
}

QoreValue ValueHolder::getReferencedValue() {
    return v.refSelf();
}

QoreValue ValueHolder::release() {
    //printd(5, "ValueHolder::takeReferencedValue() %s\n", v.getTypeName());
    if (v.type == QV_Node)
        return v.takeNodeIntern();
    return v;
}

ValueOptionalRefHolder::~ValueOptionalRefHolder() {
    if (needs_deref) {
        v.discard(xsink);
    }
}

void ValueOptionalRefHolder::ensureReferencedValue() {
    if (!needs_deref) {
        // update the flag unconditionally in case the object will be updated as a QoreValue and a dereferencable
        // value will be stored here
        needs_deref = true;
        // reference any node value
        if (v.type == QV_Node && v.v.n) {
            v.v.n->ref();
        }
    }
}

QoreValue ValueOptionalRefHolder::getReferencedValue() {
    if (needs_deref) {
        needs_deref = false;
        return v;
    }
    return v.refSelf();
}

QoreValue ValueOptionalRefHolder::takeReferencedValue() {
    if (v.type == QV_Node) {
        if (needs_deref) {
            needs_deref = false;
            return v.takeNodeIntern();
        }
        if (v.v.n) {
            v.v.n->ref();
        }
        return v.takeNodeIntern();
    }
    if (needs_deref) {
        needs_deref = false;
    }
    return v;
}

ValueEvalOptimizedRefHolder::ValueEvalOptimizedRefHolder(const QoreValue& exp, ExceptionSink* xs)
        : ValueEvalRefHolder(xs) {
    evalIntern(exp);
}

ValueEvalOptimizedRefHolder::ValueEvalOptimizedRefHolder(ExceptionSink* xs) : ValueEvalRefHolder(xs) {
}

int ValueEvalOptimizedRefHolder::eval(const QoreValue& exp) {
    v.discard(xsink);
    return evalIntern(exp);
}

ValueEvalRefHolder::ValueEvalRefHolder(const AbstractQoreNode* exp, ExceptionSink* xs) : ValueOptionalRefHolder(xs) {
    evalIntern(exp);
}

ValueEvalRefHolder::ValueEvalRefHolder(const QoreValue exp, ExceptionSink* xs) : ValueOptionalRefHolder(xs) {
    evalIntern(exp);
}

ValueEvalRefHolder::ValueEvalRefHolder(ExceptionSink* xs) : ValueOptionalRefHolder(xs) {
}

int ValueEvalRefHolder::evalIntern(const AbstractQoreNode* exp) {
    if (!exp) {
        needs_deref = false;
        return 0;
    }

    needs_deref = true;
    v = exp->eval(needs_deref, xsink);
    return xsink && *xsink ? -1 : 0;
}

int ValueEvalRefHolder::evalIntern(const QoreValue& exp) {
    needs_deref = true;
    v = exp.eval(needs_deref, xsink);
    return xsink && *xsink ? -1 : 0;
}

int ValueEvalRefHolder::eval(const AbstractQoreNode* exp) {
    v.discard(xsink);
    return evalIntern(exp);
}

int ValueEvalRefHolder::eval(const QoreValue exp) {
    v.discard(xsink);
    return evalIntern(exp);
}
