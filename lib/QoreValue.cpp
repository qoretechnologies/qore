/* indent-tabs-mode: nil -*- */
/*
    QoreValue.cpp

    NaN-boxed QoreValue implementation - 8 bytes instead of 16

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
#include <qore/QoreBigIntNode.h>
#include <qore/QoreBigFloatNode.h>
#include "qore/intern/qore_string_private.h"

#include "qore/intern/QoreLogicalEqualsOperatorNode.h"

// Type name constants
const char* qoreBoolTypeName = "bool";
const char* qoreIntTypeName = "integer";
const char* qoreFloatTypeName = "float";

// ============================================================================
// Constructors
// ============================================================================

// Note: QoreValue() and QoreValue(bool) are now constexpr inline in header

QoreValue::QoreValue(int i) {
    set(static_cast<int64>(i));
}

QoreValue::QoreValue(unsigned int i) {
    set(static_cast<int64>(i));
}

QoreValue::QoreValue(long i) {
    set(static_cast<int64>(i));
}

QoreValue::QoreValue(unsigned long i) {
    set(static_cast<int64>(i));
}

QoreValue::QoreValue(unsigned long long i) {
    set(static_cast<int64>(i));
}

QoreValue::QoreValue(int64 i) {
    set(i);
}

QoreValue::QoreValue(double f) {
    set(f);
}

QoreValue::QoreValue(AbstractQoreNode* n) {
    set(n);
}

QoreValue::QoreValue(const AbstractQoreNode* n) {
    if (!n || n->getType() == NT_NOTHING) {
        bits = VAL_NOTHING;
    } else {
        bits = TAG_POINTER | (reinterpret_cast<uint64_t>(const_cast<AbstractQoreNode*>(n)) & PAYLOAD_MASK);
    }
}

// Note: Copy constructor is now constexpr defaulted in header

// ============================================================================
// Type checking
// ============================================================================

bool QoreValue::isNothing() const {
    if (bits == VAL_NOTHING) {
        return true;
    }
    if (isPointer()) {
        return is_nothing(getPointerUnsafe());
    }
    return false;
}

bool QoreValue::isNull() const {
    if (bits == VAL_NULL) {
        return true;
    }
    if (isPointer()) {
        return is_null(getPointerUnsafe());
    }
    return false;
}

bool QoreValue::isNullOrNothing() const {
    if (bits == VAL_NOTHING || bits == VAL_NULL) {
        return true;
    }
    if (isPointer()) {
        AbstractQoreNode* n = getPointerUnsafe();
        return is_nothing(n) || is_null(n);
    }
    return false;
}

// ============================================================================
// Short string operations
// ============================================================================

bool QoreValue::tryMakeShortString(QoreValue& out, const char* str, size_t len) {
    if (len > SHORTSTR_MAX_BYTES) {
        return false;
    }

    // Encoding: bits 63-52 = 0xFFC (base tag), bits 51-48 = length (0-6)
    uint64_t tagWithLen = (0xFFCULL << 52) | (static_cast<uint64_t>(len) << 48);

    // Pack string bytes into bits 0-47 (big-endian for lexicographic comparison)
    uint64_t strBits = 0;
    for (size_t i = 0; i < len; i++) {
        strBits |= static_cast<uint64_t>(static_cast<unsigned char>(str[i])) << (40 - i * 8);
    }

    out.bits = tagWithLen | strBits;
    return true;
}

QoreValue QoreValue::makeShortString(const char* str, size_t len) {
    QoreValue v{};
    bool ok = tryMakeShortString(v, str, len);
    assert(ok && "String too long for inline storage");
    (void)ok;
    return v;
}

void QoreValue::getShortString(char* buf) const {
    assert(isShortString());
    size_t len = shortStringLen();
    for (size_t i = 0; i < len; i++) {
        buf[i] = static_cast<char>((bits >> (40 - i * 8)) & 0xFF);
    }
    buf[len] = '\0';
}

// ============================================================================
// Type conversions
// ============================================================================

int64 QoreValue::getAsBigInt() const {
    if (isInt()) {
        return getInt();
    }
    if (isFloat()) {
        return static_cast<int64>(getDouble());
    }
    if (isBool()) {
        return getBool() ? 1 : 0;
    }
    if (isPointer()) {
        AbstractQoreNode* n = getPointerUnsafe();
        return n ? n->getAsBigInt() : 0;
    }
    if (isShortString()) {
        // Convert short string to int
        char buf[8];
        getShortString(buf);
        return strtoll(buf, nullptr, 10);
    }
    return 0;
}

double QoreValue::getAsFloat() const {
    if (isFloat()) {
        return getDouble();
    }
    if (isInt()) {
        return static_cast<double>(getInt());
    }
    if (isBool()) {
        return getBool() ? 1.0 : 0.0;
    }
    if (isPointer()) {
        AbstractQoreNode* n = getPointerUnsafe();
        return n ? n->getAsFloat() : 0.0;
    }
    if (isShortString()) {
        char buf[8];
        getShortString(buf);
        return strtod(buf, nullptr);
    }
    return 0.0;
}

bool QoreValue::getAsBool() const {
    int type = getType();
    if (isBool()) {
        return getBool();
    }
    if (isInt()) {
        return getInt() != 0;
    }
    if (isFloat()) {
        return getDouble() != 0.0;
    }
    if (isPointer()) {
        AbstractQoreNode* n = getPointerUnsafe();
        return n ? n->getAsBool() : false;
    }
    if (isShortString()) {
        return shortStringLen() > 0;
    }
    if (isNothing() || isNull()) {
        return false;
    }
    return false;
}

// ============================================================================
// QoreValue API
// ============================================================================

qore_type_t QoreValue::getType() const {
    if (isFloat()) {
        return NT_FLOAT;
    }
    if (isInt()) {
        return NT_INT;
    }
    if (isBool()) {
        return NT_BOOLEAN;
    }
    if (isNothing()) {
        return NT_NOTHING;
    }
    if (isNull()) {
        return NT_NULL;
    }
    if (isShortString()) {
        return NT_STRING;
    }
    if (isPointer()) {
        AbstractQoreNode* n = getPointerUnsafe();
        return n ? n->getType() : NT_NOTHING;
    }
    return NT_NOTHING;
}

const char* QoreValue::getTypeName() const {
    if (isFloat()) {
        return qoreFloatTypeName;
    }
    if (isInt()) {
        return qoreIntTypeName;
    }
    if (isBool()) {
        return qoreBoolTypeName;
    }
    if (isShortString()) {
        return "string";
    }
    if (isPointer()) {
        return get_type_name(getPointerUnsafe());
    }
    return "nothing";
}

AbstractQoreNode* QoreValue::getInternalNode() {
    return isPointer() ? getPointerUnsafe() : nullptr;
}

const AbstractQoreNode* QoreValue::getInternalNode() const {
    return isPointer() ? getPointerUnsafe() : nullptr;
}

bool QoreValue::isValue() const {
    // Match old behavior: NOTHING is not considered a "value" for constant folding
    // In the old implementation, NOTHING was represented as QV_Node with v.n=nullptr,
    // and isValue() returned false for it
    if (isNothing()) {
        return false;
    }
    if (!isPointer()) {
        return true;
    }
    AbstractQoreNode* n = getPointerUnsafe();
    return n && n->is_value();
}

bool QoreValue::needsEval() const {
    if (!isPointer()) {
        return false;
    }
    AbstractQoreNode* n = getPointerUnsafe();
    return n && n->needs_eval();
}

bool QoreValue::hasEffect() const {
    if (!isPointer()) {
        return false;
    }
    AbstractQoreNode* n = getPointerUnsafe();
    return n && node_has_effect(n);
}

bool QoreValue::isScalar() const {
    if (!isPointer()) {
        return true;  // int, float, bool, short string are all scalar
    }
    qore_type_t t = getType();
    return t == NT_STRING || t == NT_NUMBER;
}

bool QoreValue::isConstant() const {
    qore_type_t t = getType();
    return t >= NT_NOTHING && t <= NT_NUMBER;
}

bool QoreValue::hasNode() const {
    return isPointer() && getPointerUnsafe() != nullptr;
}

bool QoreValue::isReferenceCounted() const {
    if (!isPointer()) {
        return false;
    }
    AbstractQoreNode* n = getPointerUnsafe();
    return n && n->isReferenceCounted();
}

bool QoreValue::derefCanThrowException() const {
    if (!isPointer()) {
        return false;
    }
    AbstractQoreNode* n = getPointerUnsafe();
    if (!n) {
        return false;
    }
    qore_type_t t = n->getType();
    return t == NT_OBJECT || t == NT_HASH || t == NT_LIST || t == NT_CLOSURE || t == NT_RUNTIME_CLOSURE;
}

// ============================================================================
// Comparison operations
// ============================================================================

bool QoreValue::isEqualHard(const QoreValue& other) const {
    // Fast path: identical bits (but not for floats due to NaN != NaN)
    if (bits == other.bits) {
        // Must check for NaN - two identical NaN values are not equal per IEEE 754
        if (isFloat()) {
            double v = getAsFloat();
            return v == v;  // NaN != NaN, so this returns false for NaN
        }
        return true;
    }

    // Get types
    qore_type_t t = getType();
    if (t != other.getType()) {
        return false;
    }

    switch (t) {
        case NT_INT:
            return getAsBigInt() == other.getAsBigInt();
        case NT_BOOLEAN:
            return getAsBool() == other.getAsBool();
        case NT_FLOAT: {
            // Handle -0.0 == 0.0 and NaN != NaN
            double a = getAsFloat();
            double b = other.getAsFloat();
            return a == b;
        }
        case NT_STRING:
            // Short strings would have equal bits if equal, so must be node comparison
            if (isShortString() && other.isShortString()) {
                return bits == other.bits;
            }
            break;
        case NT_NOTHING:
        case NT_NULL:
            return true;
        default:
            break;
    }

    // Node comparison
    if (isPointer() && other.isPointer()) {
        ExceptionSink xsink;
        bool rv = !compareHard(getPointerUnsafe(), other.getPointerUnsafe(), &xsink);
        xsink.clear();
        return rv;
    }

    return false;
}

bool QoreValue::isEqualSoft(const QoreValue& other, ExceptionSink* xsink) const {
    return QoreLogicalEqualsOperatorNode::softEqual(*this, other, xsink);
}

bool QoreValue::isEqualValue(const QoreValue& other) const {
    // Check if exactly the same value (for nodes, same pointer)
    return bits == other.bits;
}

// ============================================================================
// Value assignment and manipulation
// ============================================================================

void QoreValue::set(const QoreValue& val) {
    bits = val.bits;
}

void QoreValue::set(AbstractQoreNode* n) {
    if (!n || n == &Nothing || n->getType() == NT_NOTHING) {
        bits = VAL_NOTHING;
    } else {
        bits = TAG_POINTER | (reinterpret_cast<uint64_t>(n) & PAYLOAD_MASK);
    }
}

void QoreValue::set(double f) {
    // Copy double bits
    uint64_t double_bits;
    memcpy(&double_bits, &f, sizeof(f));

    // Check if this double would collide with internal tags after encoding.
    // Doubles with bits >= 0xFFF8000000000000 (including negative NaN values)
    // would become >= TAG_INT48 after adding DOUBLE_ENCODE_OFFSET.
    // These must be stored as QoreBigFloatNode to preserve correct behavior.
    constexpr uint64_t PROBLEMATIC_THRESHOLD = 0xFFF8000000000000ULL;
    if (double_bits >= PROBLEMATIC_THRESHOLD) {
        // Allocate QoreBigFloatNode for problematic doubles (negative NaN, etc.)
        QoreBigFloatNode* n = new QoreBigFloatNode(f);
        bits = TAG_POINTER | (reinterpret_cast<uint64_t>(n) & PAYLOAD_MASK);
    } else {
        // Normal inline encoding
        bits = double_bits + DOUBLE_ENCODE_OFFSET;
    }
}

void QoreValue::setLargeInt(int64 i) {
    // Allocate a QoreBigIntNode for large integers
    QoreBigIntNode* n = new QoreBigIntNode(i);
    bits = TAG_POINTER | (reinterpret_cast<uint64_t>(n) & PAYLOAD_MASK);
}

void QoreValue::clear() {
    bits = VAL_NOTHING;
}

void QoreValue::discard(ExceptionSink* xsink) {
    if (isPointer()) {
        AbstractQoreNode* n = getPointerUnsafe();
        if (n) {
            n->deref(xsink);
        }
    }
    bits = VAL_NOTHING;
}

AbstractQoreNode* QoreValue::assign(const QoreValue& n) {
    AbstractQoreNode* rv = takeIfNode();
    bits = n.bits;
    return rv;
}

AbstractQoreNode* QoreValue::assign(AbstractQoreNode* n) {
    AbstractQoreNode* rv = takeIfNode();
    set(n);
    return rv;
}

AbstractQoreNode* QoreValue::assign(int64 n) {
    AbstractQoreNode* rv = takeIfNode();
    set(n);
    return rv;
}

AbstractQoreNode* QoreValue::assign(double n) {
    AbstractQoreNode* rv = takeIfNode();
    set(n);
    return rv;
}

AbstractQoreNode* QoreValue::assign(bool n) {
    AbstractQoreNode* rv = takeIfNode();
    set(n);
    return rv;
}

AbstractQoreNode* QoreValue::assignNothing() {
    AbstractQoreNode* rv = takeIfNode();
    bits = VAL_NOTHING;
    return rv;
}

void QoreValue::swap(QoreValue& val) {
    uint64_t tmp = bits;
    bits = val.bits;
    val.bits = tmp;
}

void QoreValue::sanitize() {
    if (!isPointer()) {
        return;
    }
    AbstractQoreNode* n = getPointerUnsafe();
    if (!n) {
        bits = VAL_NOTHING;
        return;
    }

    // Try to convert node to inline representation
    qore_type_t t = n->getType();
    switch (t) {
        case NT_INT: {
            int64 i = n->getAsBigInt();
            if (fitsInline(i)) {
                n->deref(nullptr);
                set(i);
            }
            break;
        }
        case NT_FLOAT: {
            double f = n->getAsFloat();
            n->deref(nullptr);
            set(f);
            break;
        }
        case NT_BOOLEAN: {
            bool b = n->getAsBool();
            n->deref(nullptr);
            set(b);
            break;
        }
        case NT_NOTHING: {
            n->deref(nullptr);
            bits = VAL_NOTHING;
            break;
        }
        case NT_NULL: {
            n->deref(nullptr);
            bits = VAL_NULL;
            break;
        }
        case NT_STRING: {
            // Try to convert short strings to inline
            QoreStringNode* str = reinterpret_cast<QoreStringNode*>(n);
            size_t len = str->size();
            if (len <= SHORTSTR_MAX_BYTES && str->getEncoding() == QCS_UTF8) {
                QoreValue shortStr;
                if (tryMakeShortString(shortStr, str->c_str(), len)) {
                    n->deref(nullptr);
                    bits = shortStr.bits;
                }
            }
            break;
        }
        default:
            // Keep as pointer
            break;
    }
}

// ============================================================================
// Reference counting
// ============================================================================

void QoreValue::ref() const {
    if (isPointer()) {
        AbstractQoreNode* n = getPointerUnsafe();
        if (n) {
            n->ref();
        }
    }
}

QoreValue QoreValue::refSelf() const {
    ref();
    return *this;
}

// ============================================================================
// Node operations
// ============================================================================

AbstractQoreNode* QoreValue::takeNode() {
    assert(isPointer());
    return takeNodeIntern();
}

AbstractQoreNode* QoreValue::takeNodeIntern() {
    assert(isPointer());
    AbstractQoreNode* rv = getPointerUnsafe();
    bits = VAL_NOTHING;
    return rv;
}

AbstractQoreNode* QoreValue::takeIfNode() {
    if (isPointer()) {
        return takeNodeIntern();
    }
    return nullptr;
}

// ============================================================================
// String conversion
// ============================================================================

int QoreValue::getAsString(QoreString& str, int format_offset, ExceptionSink* xsink) const {
    if (isNothing()) {
        qore_string_private::get(str)->concat(
            (format_offset == FMT_YAML_SHORT || format_offset <= FMT_YAML_LONG)
                ? &YamlNullString : &NothingTypeString);
        return 0;
    }

    if (isInt()) {
        str.sprintf(QLLD, getInt());
    } else if (isBool()) {
        qore_string_private::get(str)->concat(getBool() ? &TrueString : &FalseString);
    } else if (isFloat()) {
        size_t offset = str.size();
        str.sprintf("%.9g", getDouble());
        q_fix_decimal(&str, offset);
    } else if (isShortString()) {
        char buf[8];
        getShortString(buf);
        str.concat(buf);
    } else if (isPointer()) {
        AbstractQoreNode* n = getPointerUnsafe();
        if (n) {
            return n->getAsString(str, format_offset, xsink);
        }
    }
    return 0;
}

QoreString* QoreValue::getAsString(bool& del, int format_offset, ExceptionSink* xsink) const {
    if (isNothing()) {
        del = false;
        return (format_offset == FMT_YAML_SHORT || format_offset <= FMT_YAML_LONG)
            ? &YamlNullString : &NothingTypeString;
    }

    if (isInt()) {
        del = true;
        return new QoreStringMaker(QLLD, getInt());
    }
    if (isBool()) {
        del = false;
        return getBool() ? &TrueString : &FalseString;
    }
    if (isFloat()) {
        del = true;
        return q_fix_decimal(new QoreStringMaker("%.9g", getDouble()), 0);
    }
    if (isShortString()) {
        del = true;
        char buf[8];
        getShortString(buf);
        return new QoreString(buf);
    }
    if (isPointer()) {
        AbstractQoreNode* n = getPointerUnsafe();
        if (n) {
            return n->getAsString(del, format_offset, xsink);
        }
    }
    del = false;
    return &NothingTypeString;
}

// ============================================================================
// Evaluation
// ============================================================================

QoreValue QoreValue::eval(ExceptionSink* xsink) const {
    if (!isPointer()) {
        return *this;
    }
    AbstractQoreNode* n = getPointerUnsafe();
    if (!n) {
        return *this;
    }
    // Need to use internal evaluation - AbstractQoreNode::eval returns QoreValue
    // which is now the new NaN-boxed type
    bool needs_deref = true;
    QoreValue result = n->eval(needs_deref, xsink);
    // If needs_deref is false, the node returned itself without adding a reference.
    // Since the caller of this single-arg version expects a referenced value,
    // we need to add a reference.
    if (!needs_deref) {
        result.ref();
    }
    return result;
}

QoreValue QoreValue::eval(bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref == true);
    if (!isPointer()) {
        needs_deref = false;
        return *this;
    }
    AbstractQoreNode* n = getPointerUnsafe();
    if (!n) {
        needs_deref = false;
        return *this;
    }
    return n->eval(needs_deref, xsink);
}

QoreValue QoreValue::eval(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref == true);
    if (!isPointer()) {
        needs_deref = false;
        return *this;
    }
    AbstractQoreNode* n = getPointerUnsafe();
    if (!n) {
        needs_deref = false;
        return *this;
    }
    return n->eval(rc, needs_deref, xsink);
}

// ============================================================================
// Type information
// ============================================================================

const QoreTypeInfo* QoreValue::getTypeInfo() const {
    if (isFloat()) {
        return floatTypeInfo;
    }
    if (isInt()) {
        return bigIntTypeInfo;
    }
    if (isBool()) {
        return boolTypeInfo;
    }
    if (isShortString()) {
        return stringTypeInfo;
    }
    if (isPointer()) {
        return getTypeInfoForValue(getPointerUnsafe());
    }
    return nothingTypeInfo;
}

const QoreTypeInfo* QoreValue::getFullTypeInfo() const {
    qore_type_t t = getType();
    if (t == NT_OBJECT && isPointer()) {
        const QoreObject* obj = reinterpret_cast<const QoreObject*>(getPointerUnsafe());
        if (obj) {
            return obj->getClass()->getTypeInfo();
        }
    }
    if (t == NT_HASH && isPointer()) {
        const QoreHashNode* h = reinterpret_cast<const QoreHashNode*>(getPointerUnsafe());
        if (h) {
            const TypedHashDecl* thd = h->getHashDecl();
            if (thd) {
                return thd->getTypeInfo();
            }
        }
    }
    return getTypeInfo();
}

const char* QoreValue::getFullTypeName() const {
    if (isFloat()) {
        return qoreFloatTypeName;
    }
    if (isInt()) {
        return qoreIntTypeName;
    }
    if (isBool()) {
        return qoreBoolTypeName;
    }
    if (isShortString()) {
        return "string";
    }
    if (isPointer()) {
        return get_full_type_name(getPointerUnsafe());
    }
    return "nothing";
}

const char* QoreValue::getFullTypeName(bool with_namespaces) const {
    if (isFloat()) {
        return qoreFloatTypeName;
    }
    if (isInt()) {
        return qoreIntTypeName;
    }
    if (isBool()) {
        return qoreBoolTypeName;
    }
    if (isShortString()) {
        return "string";
    }
    if (isPointer()) {
        return get_full_type_name(getPointerUnsafe(), with_namespaces);
    }
    return "nothing";
}

const char* QoreValue::getFullTypeName(bool with_namespaces, QoreString& scratch) const {
    if (isFloat()) {
        return qoreFloatTypeName;
    }
    if (isInt()) {
        return qoreIntTypeName;
    }
    if (isBool()) {
        return qoreBoolTypeName;
    }
    if (isShortString()) {
        return "string";
    }
    if (isPointer()) {
        return get_full_type_name(getPointerUnsafe(), with_namespaces, scratch);
    }
    return "nothing";
}

// ============================================================================
// Operators
// ============================================================================

// Note: operator= and operator bool are now inline in header

// ============================================================================
// RAII Helper Classes
// ============================================================================

ValueHolder::~ValueHolder() {
    v.discard(xsink);
}

QoreValue ValueHolder::getReferencedValue() {
    return v.refSelf();
}

QoreValue ValueHolder::release() {
    if (v.isPointer()) {
        return QoreValue(v.takeNodeIntern());
    }
    QoreValue rv = v;
    v.clear();
    return rv;
}

ValueOptionalRefHolder::~ValueOptionalRefHolder() {
    if (needs_deref) {
        v.discard(xsink);
    }
}

void ValueOptionalRefHolder::ensureReferencedValue() {
    if (!needs_deref) {
        needs_deref = true;
        if (v.isPointer()) {
            AbstractQoreNode* n = v.getPointerUnsafe();
            if (n) {
                n->ref();
            }
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
    if (v.isPointer()) {
        if (needs_deref) {
            needs_deref = false;
            return QoreValue(v.takeNodeIntern());
        }
        AbstractQoreNode* n = v.getPointerUnsafe();
        if (n) {
            n->ref();
        }
        return QoreValue(v.takeNodeIntern());
    }
    if (needs_deref) {
        needs_deref = false;
    }
    return v;
}

ValueEvalRefHolder::ValueEvalRefHolder(ExceptionSink* xs)
    : ValueOptionalRefHolder(xs) {
}

// Backwards-compatible constructor that gets RuntimeConfig from TLS
ValueEvalRefHolder::ValueEvalRefHolder(const AbstractQoreNode* exp, ExceptionSink* xs)
    : ValueOptionalRefHolder(xs) {
    RuntimeConfig& rc = rc_get_current_ref();
    evalIntern(rc, exp);
}

// Backwards-compatible constructor that gets RuntimeConfig from TLS
ValueEvalRefHolder::ValueEvalRefHolder(const QoreValue exp, ExceptionSink* xs)
    : ValueOptionalRefHolder(xs) {
    RuntimeConfig& rc = rc_get_current_ref();
    evalIntern(rc, exp);
}

ValueEvalRefHolder::ValueEvalRefHolder(RuntimeConfig& rc, const AbstractQoreNode* exp, ExceptionSink* xs)
    : ValueOptionalRefHolder(xs) {
    evalIntern(rc, exp);
}

ValueEvalRefHolder::ValueEvalRefHolder(RuntimeConfig& rc, const QoreValue exp, ExceptionSink* xs)
    : ValueOptionalRefHolder(xs) {
    evalIntern(rc, exp);
}

int ValueEvalRefHolder::evalIntern(RuntimeConfig& rc, const AbstractQoreNode* exp) {
    if (!exp) {
        needs_deref = false;
        return 0;
    }
    needs_deref = true;
    v = exp->eval(rc, needs_deref, xsink);
    return xsink && *xsink ? -1 : 0;
}

int ValueEvalRefHolder::evalIntern(RuntimeConfig& rc, const QoreValue& exp) {
    needs_deref = true;
    v = exp.eval(rc, needs_deref, xsink);
    return xsink && *xsink ? -1 : 0;
}

// Backwards-compatible eval that gets RuntimeConfig from TLS
int ValueEvalRefHolder::eval(const AbstractQoreNode* exp) {
    v.discard(xsink);
    RuntimeConfig& rc = rc_get_current_ref();
    return evalIntern(rc, exp);
}

// Backwards-compatible eval that gets RuntimeConfig from TLS
int ValueEvalRefHolder::eval(const QoreValue exp) {
    v.discard(xsink);
    RuntimeConfig& rc = rc_get_current_ref();
    return evalIntern(rc, exp);
}

int ValueEvalRefHolder::eval(RuntimeConfig& rc, const AbstractQoreNode* exp) {
    v.discard(xsink);
    return evalIntern(rc, exp);
}

int ValueEvalRefHolder::eval(RuntimeConfig& rc, const QoreValue exp) {
    v.discard(xsink);
    return evalIntern(rc, exp);
}

ValueEvalOptimizedRefHolder::ValueEvalOptimizedRefHolder(const QoreValue& exp, ExceptionSink* xs)
    : ValueEvalRefHolder(xs) {
    eval(exp);
}

ValueEvalOptimizedRefHolder::ValueEvalOptimizedRefHolder(ExceptionSink* xs)
    : ValueEvalRefHolder(xs) {
}

int ValueEvalOptimizedRefHolder::eval(const QoreValue& exp) {
    if (!exp.needsEval()) {
        needs_deref = false;
        v = exp;
        return 0;
    }
    this->needs_deref = true;
    bool local_nd = true;
    v = exp.eval(local_nd, xsink);
    this->needs_deref = local_nd;
    return xsink && *xsink ? -1 : 0;
}
