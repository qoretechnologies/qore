/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreLValue.h

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

#ifndef _QORE_INTERN_QORELVALUE_H

#define _QORE_INTERN_QORELVALUE_H

DLLLOCAL void check_lvalue_object_in_out(AbstractQoreNode* in, AbstractQoreNode* out);

template <typename U = qore_value_u>
class QoreLValue {
protected:
    // returns the old value just in case it needs to be dereferenced outside a lock
    template <class T, typename t, int nt>
    DLLLOCAL T* ensureUnique(AbstractQoreNode*& old) {
        assert(type == QV_Node);
        assert(assigned);
        assert(!old);

        if (!v.n) {
            return reinterpret_cast<T*>(v.n = new T);
        }

        if (v.n->getType() != nt) {
            t i = T::getValue(v.n);
            old = v.n;
            return reinterpret_cast<T*>(v.n = new T(i));
        }

        if (v.n->is_unique()) {
            return reinterpret_cast<T*>(v.n);
        }

        old = v.n;
        return reinterpret_cast<T*>((v.n = old->realCopy()));
    }

    DLLLOCAL void reset() {
        assert(!assigned || type != QV_Node || !v.n);
        if (assigned) {
            assigned = false;
            assert(!static_assignment);
        }
    }

public:
    U v;
    valtype_t type : 4;
    bool fixed_type : 1;
    bool assigned : 1;

    // true if the assigned value was not referenced for the assignment
    // and therefore should not be dereferenced
    bool static_assignment : 1;

    // true if assigned to a closure
    bool is_closure : 1;

    DLLLOCAL QoreLValue() : type(QV_Node), fixed_type(false), assigned(false), static_assignment(false),
            is_closure(false) {
#ifdef DEBUG
        v.n = 0;
#endif
        reset();
    }

    DLLLOCAL QoreLValue(valtype_t t) : type(t), fixed_type(t != QV_Node), assigned(false), static_assignment(false),
            is_closure(false) {
#ifdef DEBUG
        if (t == QV_Node)
            v.n = 0;
#endif
        reset();
    }

    // fixed_type is assigned in set()
    DLLLOCAL QoreLValue(const QoreTypeInfo* typeInfo) : assigned(false), static_assignment(false), is_closure(false) {
#ifdef DEBUG
        type = QV_Bool;
#endif
        set(typeInfo);
    }

    DLLLOCAL QoreLValue(const QoreLValue<U>& old) : type(old.type), fixed_type(old.fixed_type),
            assigned(old.assigned), static_assignment(false), is_closure(old.is_closure) {
        if (!assigned)
            return;
        switch (old.type) {
            case QV_Bool: v.b = old.v.b; break;
            case QV_Int: v.i = old.v.i; break;
            case QV_Float: v.f = old.v.f; break;
            case QV_Node:
                v.n = old.v.n ? old.v.n->refSelf() : nullptr;
                if (!is_closure)
                    check_lvalue_object_in_out(v.n, nullptr);
                break;
            default: assert(false);
            // no break
        }
    }

#ifdef DEBUG
    DLLLOCAL ~QoreLValue() {
        assert(!assigned || type != QV_Node || !v.n);
    }
#endif

    // Reset to initial empty state for reuse (when stack-allocated LocalVarValue is reused for a different variable)
    DLLLOCAL void reset_to_empty() {
        type = QV_Node;
        fixed_type = false;
        assigned = false;
        static_assignment = false;
        is_closure = false;
        // Always clear the node pointer to ensure destructor assertion doesn't fail
        v.n = 0;
    }

    DLLLOCAL void setClosure() {
        assert(!is_closure);
        is_closure = true;
    }

    DLLLOCAL valtype_t getOptimizedType() const {
        return fixed_type ? type : QV_Node;
    }

    DLLLOCAL bool optimized() const {
        return type != QV_Node && type != QV_Ref;
    }

    //! returns the value as the given type
    /** @note that if a pointer type is given and the object does not contain a node (i.e. type != QV_Node), then
        this call will cause a segfault, however it is always legal to cast to simple types (int64, bool, float), in which case type conversions are performed
    */
    template<typename T>
    DLLLOCAL typename detail::QoreValueCastHelper<T>::Result get() {
        return assigned ? detail::QoreValueCastHelper<T>::cast(this, type) : nullptr;
    }

    //! returns the value as the given type
    /** @note that if a pointer type is given and the object does not contain a node (i.e. type != QV_Node), then
        this call will cause a segfault, however it is always legal to cast to simple types (int64, bool, float), in which case type conversions are performed
    */
    template<typename T>
    DLLLOCAL typename detail::QoreValueCastHelper<const T>::Result get() const {
        return assigned ? detail::QoreValueCastHelper<const T>::cast(this, type) : nullptr;
    }

    DLLLOCAL const char* getFixedTypeName() const {
        if (!fixed_type)
            return "any";
        switch (type) {
            case QV_Int: return "int";
            case QV_Float: return "float";
            case QV_Bool: return "bool";
            default:
                assert(false);
        }
    }

    DLLLOCAL bool isInt() {
        if (!assigned) {
            if (type != QV_Int) {
                if (fixed_type) {
                    return false;
                }
                type = QV_Int;
            }
            assigned = true;
            v.i = 0;
            return true;
        }
        if (type == QV_Int) {
            return true;
        }
        return false;
    }

    DLLLOCAL bool isFloat() {
        if (!assigned) {
            if (type != QV_Float) {
                if (fixed_type) {
                    return false;
                }
                type = QV_Float;
            }
            assigned = true;
            v.f = 0.0;
            return true;
        }
        if (type == QV_Float)
            return true;
        return false;
    }

    DLLLOCAL AbstractQoreNode* makeInt() {
        assert(!fixed_type || type == QV_Int);
        if (!assigned) {
            assigned = true;
            if (!type) {
                type = QV_Int;
            }
            v.i = 0;
            return nullptr;
        }
        switch (type) {
            case QV_Float: {
                int64 i = (int64)v.f;
                v.i = i;
                type = QV_Int;
                return nullptr;
            }

            case QV_Bool: {
                int64 i = (int64)v.b;
                v.i = i;
                type = QV_Int;
                return nullptr;
            }

            case QV_Node: {
                int64 i = v.n ? v.n->getAsBigInt() : 0;
                AbstractQoreNode* rv = v.n;
                v.i = i;
                type = QV_Int;
                if (!is_closure) {
                    check_lvalue_object_in_out(nullptr, rv);
                }
                return rv;
            }

            case QV_Int:
                break;

            default:
                assert(false);
        }

        return nullptr;
    }

    DLLLOCAL AbstractQoreNode* makeFloat() {
        assert(!fixed_type || type == QV_Float);
        if (!assigned) {
            assigned = true;
            if (!type)
                type = QV_Float;
            v.f = 0.0;
            return nullptr;
        }
        switch (type) {
            case QV_Int: {
                double f = v.i;
                v.f = f;
                type = QV_Float;
                return nullptr;
            }

            case QV_Bool: {
                double f = v.b;
                v.f = f;
                type = QV_Float;
                return nullptr;
            }

            case QV_Node: {
                double f = v.n ? v.n->getAsFloat() : 0.0;
                AbstractQoreNode* rv = v.n;
                v.f = f;
                type = QV_Float;
                if (!is_closure) {
                    check_lvalue_object_in_out(nullptr, rv);
                }
                return rv;
            }

            case QV_Float:
                break;

            default:
                assert(false);
        }

        return nullptr;
    }

    DLLLOCAL AbstractQoreNode* makeNumber() {
        assert(!fixed_type);
        if (!assigned) {
            assigned = true;
            if (!type)
                type = QV_Node;
            v.n = new QoreNumberNode;
            return nullptr;
        }
        switch (type) {
            case QV_Int: {
                QoreNumberNode* n = new QoreNumberNode(v.i);
                v.n = n;
                type = QV_Node;
                return nullptr;
            }

            case QV_Float: {
                QoreNumberNode* n = new QoreNumberNode(v.f);
                v.n = n;
                type = QV_Node;
                return nullptr;
            }

            case QV_Bool: {
                QoreNumberNode* n = new QoreNumberNode((int64)v.b);
                v.n = n;
                type = QV_Node;
                return nullptr;
            }

            case QV_Node: {
                if (v.n && v.n->getType() == NT_NUMBER) {
                    return nullptr;
                }
                QoreNumberNode* n = new QoreNumberNode(v.n);
                AbstractQoreNode* rv = v.n;
                v.n = n;
                type = QV_Node;
                if (!is_closure)
                check_lvalue_object_in_out(nullptr, rv);
                return rv;
            }

            default:
                assert(false);
        }

        return nullptr;
    }

    DLLLOCAL QoreValue takeValue() {
        if (!assigned)
            return QoreValue();

        assigned = false;
        assert(!static_assignment);

        switch (type) {
            case QV_Bool: return QoreValue(v.b);
            case QV_Int: return QoreValue(v.i);
            case QV_Float: return QoreValue(v.f);
            case QV_Node:
                if (!is_closure)
                    check_lvalue_object_in_out(nullptr, v.n);
                return QoreValue(v.n);
            default: assert(false);
            // no break
        }
        return false;
    }

    DLLLOCAL const QoreValue getValue() const {
        if (!assigned)
            return QoreValue();

        switch (type) {
            case QV_Bool: return QoreValue(v.b);
            case QV_Int: return QoreValue(v.i);
            case QV_Float: return QoreValue(v.f);
            case QV_Node: return v.n;
            default: assert(false);
                // no break
        }
        return QoreValue();
    }

    DLLLOCAL QoreValue getValue() {
        if (!assigned)
            return QoreValue();

        switch (type) {
            case QV_Bool: return QoreValue(v.b);
            case QV_Int: return QoreValue(v.i);
            case QV_Float: return QoreValue(v.f);
            case QV_Node: return v.n;
            default: assert(false);
                // no break
        }
        return QoreValue();
    }

    DLLLOCAL QoreValue getReferencedValue() const {
        if (!assigned) {
            return QoreValue();
        }

        // For QV_Node, ref the existing node before returning
        if (type == QV_Node) {
            if (v.n) {
                v.n->ref();
            }
            return QoreValue(v.n);
        }

        // For scalar types, QoreValue constructor may allocate a new QoreBigIntNode/QoreBigFloatNode
        // for large values. These are already created with ref count 1, so no additional ref needed.
        switch (type) {
            case QV_Bool: return QoreValue(v.b);
            case QV_Int: return QoreValue(v.i);
            case QV_Float: return QoreValue(v.f);
            default: assert(false);
                // no break
        }
        return QoreValue();
    }

    DLLLOCAL const ReferenceNode* getReference() const {
        if (assigned && type == QV_Node && v.n && v.n->getType() == NT_REFERENCE) {
            return reinterpret_cast<const ReferenceNode*>(v.n);
        }
        return nullptr;
    }

    DLLLOCAL bool needsScan() const {
        if (!assigned || type != QV_Node || !v.n)
            return false;

        return needs_scan(v.n);
    }

    // never call when a lock is held
    DLLLOCAL QoreValue getReferencedValue(bool& needs_deref) const {
        if (!assigned) {
            needs_deref = false;
            return QoreValue();
        }

        if (type == QV_Node) {
            needs_deref = false;
            return QoreValue(v.n);
        }

        switch (type) {
            case QV_Bool:
                needs_deref = false;
                return QoreValue(v.b);
            case QV_Int: {
                // QoreValue(v.i) may allocate a QoreBigIntNode for large integers
                QoreValue result(v.i);
                // If a large integer was created (stored as pointer), set needs_deref = true
                needs_deref = result.isPointer();
                return result;
            }
            case QV_Float: {
                // QoreValue(v.f) may allocate a QoreBigFloatNode for problematic doubles
                QoreValue result(v.f);
                // If a node was created, set needs_deref = true
                needs_deref = result.isPointer();
                return result;
            }
            default: assert(false);
            // no break
        }
        needs_deref = false;
        return QoreValue();
    }

    DLLLOCAL void discard(ExceptionSink* xsink) {
        if (!assigned)
            return;

        if (type == QV_Node && v.n) {
            if (!is_closure)
                check_lvalue_object_in_out(nullptr, v.n);
            v.n->deref(xsink);
        }
        assigned = false;
    }

    DLLLOCAL bool hasValue() const {
        if (!assigned)
            return false;
        switch (type) {
            case QV_Bool: return v.b;
            case QV_Int: return (bool)v.i;
            case QV_Float: return (bool)v.f;
            case QV_Node: return !is_nothing(v.n);
            default: assert(false);
            // no break
        }
        return false;
    }

    DLLLOCAL void set(const QoreTypeInfo* typeInfo) {
        if (typeInfo == bigIntTypeInfo || typeInfo == softBigIntTypeInfo)
            set(QV_Int);
        else if (typeInfo == floatTypeInfo || typeInfo == softFloatTypeInfo)
            set(QV_Float);
        else if (typeInfo == boolTypeInfo || typeInfo == softBoolTypeInfo)
            set(QV_Bool);
        else
            set(QV_Node);
    }

    DLLLOCAL void set(valtype_t t) {
        assert(!assigned || type != QV_Node || !v.n);
        type = t;
        fixed_type = (t != QV_Node);
#ifdef DEBUG
        if (t == QV_Node)
            v.n = 0;
#endif
        reset();
    }

    DLLLOCAL AbstractQoreNode* assign(bool b) {
        if (fixed_type) {
            if (!assigned)
                assigned = true;
            switch (type) {
                case QV_Bool: v.b = false; return 0;
                case QV_Int: v.i = (int64)b; return 0;
                case QV_Float: v.f = (double)b; return 0;
                default: assert(false);
                // no break
            }
        }

        AbstractQoreNode* rv;
        if (assigned) {
            if (type == QV_Node) {
                if (!is_closure)
                    check_lvalue_object_in_out(nullptr, v.n);
                rv = v.n;
            }
            else
                rv = 0;
        }
        else {
            assigned = true;
            rv = 0;
        }

        v.b = b;
        type = QV_Bool;

        return rv;
    }

    DLLLOCAL AbstractQoreNode* assign(int64 i) {
        if (fixed_type) {
            if (!assigned)
                assigned = true;
            switch (type) {
                case QV_Bool: v.b = (bool)i; return 0;
                case QV_Int: v.i = i; return 0;
                case QV_Float: v.f = (double)i; return 0;
                default: assert(false);
                // no break
            }
        }

        AbstractQoreNode* rv;
        if (assigned) {
            if (type == QV_Node) {
                if (!is_closure)
                    check_lvalue_object_in_out(nullptr, v.n);
                rv = v.n;
            }
            else
                rv = 0;
        }
        else {
            assigned = true;
            rv = 0;
        }

        v.i = i;
        type = QV_Int;

        return rv;
    }

    DLLLOCAL AbstractQoreNode* assign(double f) {
        if (fixed_type) {
            if (!assigned)
                assigned = true;
            switch (type) {
                case QV_Bool: v.b = (bool)f; return 0;
                case QV_Int: v.i = (int64)f; return 0;
                case QV_Float: v.f = f; return 0;
                default: assert(false);
                // no break
            }
        }

        AbstractQoreNode* rv;
        if (assigned) {
            if (type == QV_Node) {
                if (!is_closure)
                    check_lvalue_object_in_out(nullptr, v.n);
                rv = v.n;
            }
            else
                rv = 0;
        }
        else {
            assigned = true;
            rv = 0;
        }

        v.f = f;
        type = QV_Float;

        return rv;
    }

    //! takes the value of an lvalue and returns that value in the output variable \a n
    /** @param n the output variable containing the old value of the lvalue
        @param val the default value for the type
    */
    DLLLOCAL void assignSetTakeInitial(QoreLValue<U>& n, QoreValue val) {
        assert(!assigned);
        assigned = true;
        type = n.type;
        if (n.static_assignment) {
            static_assignment = n.static_assignment;
            n.static_assignment = false;
        }
        switch (n.type) {
            case QV_Bool:
                v.b = n.v.b;
                assert(val.getType() == NT_BOOLEAN);
                n.v.b = val.getAsBool();
                break;

            case QV_Int:
                v.i = n.v.i;
                assert(val.getType() == NT_INT);
                n.v.i = val.getAsBigInt();
                break;

            case QV_Float:
                v.f = n.v.f;
                assert(val.getType() == NT_FLOAT);
                n.v.f = val.getAsFloat();
                break;

            case QV_Node:
                if (n.is_closure) {
                    assert(!is_closure);
                    is_closure = true;
                }
                v.n = n.v.n;
                n.v.n = val.takeNode();
                break;

            default:
                assert(false);
                // no break
        }
    }

    DLLLOCAL void assignSetTakeInitial(QoreLValue<U>& n) {
        assert(!assigned);
        if (!n.assigned)
            return;
        assigned = true;
        type = n.type;
        if (n.static_assignment) {
            static_assignment = n.static_assignment;
            n.static_assignment = false;
        }
        switch (n.type) {
            case QV_Bool: v.b = n.v.b; n.v.b = false; break;
            case QV_Int: v.i = n.v.i; n.v.i = 0; break;
            case QV_Float: v.f = n.v.f; n.v.f = 0; break;
            case QV_Node:
                if (n.is_closure) {
                    assert(!is_closure);
                    is_closure = true;
                }
                v.n = n.v.n;
                n.v.n = nullptr;
                break;
            default: assert(false);
            // no break
        }
        n.assigned = false;
    }

    // note: destructive for "n"
    // returns any AbstractQoreNode no longer needed (because it was converted to a base type)
    DLLLOCAL AbstractQoreNode* assignAssumeInitial(QoreValue& n, bool is_static_assignment = false) {
        assert(!assigned);
        assert(!static_assignment);
        //printd(5, "QoreLValue::assignAssumeInitial() this: %p n: %s sa: %d\n", this, n.getTypeName(), is_static_assignment);
        if (is_static_assignment)
            static_assignment = true;
        return assignAssume(n);
    }

    DLLLOCAL void assignInitial(bool n) {
        assert(!assigned);
        assigned = true;
        if (fixed_type) {
            switch (type) {
                case QV_Bool: v.b = n; return;
                case QV_Int: v.i = (int64)n; return;
                case QV_Float: v.f = (double)n; return;
                default: assert(false);
                // no break
            }
        }
        type = QV_Bool;
        v.b = n;
    }

    DLLLOCAL void assignInitial(int64 n) {
        assert(!assigned);
        assigned = true;
        if (fixed_type) {
            switch (type) {
                case QV_Bool: v.b = (bool)n; return;
                case QV_Int: v.i = n; return;
                case QV_Float: v.f = (double)n; return;
                default: assert(false);
                // no break
            }
        }
        type = QV_Int;
        v.i = n;
    }

    DLLLOCAL void assignInitial(double n) {
        assert(!assigned);
        assigned = true;
        if (fixed_type) {
            switch (type) {
                case QV_Bool: v.b = (bool)n; return;
                case QV_Int: v.i = (int64)n; return;
                case QV_Float: v.f = n; return;
                default: assert(false);
                // no break
            }
        }
        type = QV_Float;
        v.f = n;
    }

    DLLLOCAL AbstractQoreNode* assignInitial(AbstractQoreNode* n) {
        assert(!assigned);
        assigned = true;
        if (fixed_type) {
            switch (type) {
                case QV_Bool:
                v.b = n ? n->getAsBool() : false; return n;
                case QV_Int: v.i = n ? n->getAsBigInt() : 0; return n;
                case QV_Float: v.f = n ? n->getAsFloat() : 0; return n;
                default: assert(false);
                // no break
            }
        }
        type = QV_Node;
        v.n = n;
        if (!is_closure)
            check_lvalue_object_in_out(v.n, nullptr);
        return 0;
    }

    DLLLOCAL AbstractQoreNode* assignInitial(QoreValue n) {
        assert(!assigned);
        return assignAssume(n);
    }

    // NOTE: destructive for "val":
    DLLLOCAL AbstractQoreNode* assignAssume(QoreValue& val) {
        if (fixed_type) {
            if (!assigned)
                assigned = true;
            switch (type) {
                case QV_Bool: v.b = val.getAsBool(); break;
                case QV_Int: v.i = val.getAsBigInt(); break;
                case QV_Float: v.f = val.getAsFloat(); break;
                default: assert(false);
                // no break
            }
            return val.takeIfNode();
        }

        AbstractQoreNode* rv;
        if (assigned) {
            if (type == QV_Node) {
                if (!is_closure) {
                    check_lvalue_object_in_out(nullptr, v.n);
                }
                rv = v.n;
            } else {
                rv = nullptr;
            }
        } else {
            assigned = true;
            rv = nullptr;
        }

        // Use QoreValue's type info via getType()
        qore_type_t vt = val.getType();
        if (vt == NT_BOOLEAN) {
            v.b = val.getAsBool();
            if (type != QV_Bool) type = QV_Bool;
        } else if (vt == NT_INT) {
            v.i = val.getAsBigInt();
            if (type != QV_Int) type = QV_Int;
        } else if (vt == NT_FLOAT) {
            v.f = val.getAsFloat();
            if (type != QV_Float) type = QV_Float;
        } else if (val.hasNode()) {
            v.n = val.takeNode();
            if (type != QV_Node) {
                type = QV_Node;
            }
            if (!is_closure) {
                check_lvalue_object_in_out(v.n, nullptr);
            }
        } else {
            // nothing or null
            v.n = nullptr;
            if (type != QV_Node) {
                type = QV_Node;
            }
        }

        // Handle any unconsumed node from val (e.g., BigInt/Float node when storing inline)
        // This can happen when val contains a QoreBigIntNode or QoreFloatNode that was
        // converted to an inline scalar value above
        AbstractQoreNode* val_node = val.takeIfNode();
        if (val_node) {
            if (rv) {
                // We have both an old node and an unconsumed node - deref the val node inline
                val_node->deref(nullptr);
            } else {
                // No old node, so return the val node for cleanup by caller
                rv = val_node;
            }
        }

        return rv;
    }

    // the node is already referenced for the assignment
    DLLLOCAL AbstractQoreNode* assign(AbstractQoreNode* n) {
        if (fixed_type) {
            if (!assigned)
                assigned = true;

            switch (type) {
                case QV_Bool: {
                    if (n) {
                        v.b = n->getAsBool();
                        return n;
                    }
                    v.b = false;
                    return 0;
                }
                case QV_Int: {
                    if (n) {
                        v.i = n->getAsBigInt();
                        return n;
                    }
                    v.i = 0;
                    return 0;
                }
                case QV_Float: {
                    if (n) {
                        v.f = n->getAsFloat();
                        return n;
                    }
                    v.f = 0.0;
                    return 0;
                }
                case QV_Node: {
                    if (!is_closure)
                        check_lvalue_object_in_out(n, v.n);
                    AbstractQoreNode* rv = v.n;
                    v.n = n;
                    return rv;
                }
                default:
                assert(false);
                // no break
            }
            return 0;
        }

        AbstractQoreNode* rv;
        if (assigned) {
            if (type == QV_Node) {
                if (!is_closure)
                    check_lvalue_object_in_out(nullptr, v.n);
                rv = v.n;
            }
            else
                rv = 0;
        }
        else {
            assigned = true;
            rv = 0;
        }

        v.n = n;
        if (!is_closure)
            check_lvalue_object_in_out(v.n, nullptr);
        if (type != QV_Node)
            type = QV_Node;

        return rv;
    }

    // Assign from QoreValue - delegates to the appropriate typed assign method
    DLLLOCAL AbstractQoreNode* assign(QoreValue val) {
        qore_type_t vt = val.getType();
        if (vt == NT_BOOLEAN) {
            return assign(val.getAsBool());
        } else if (vt == NT_INT) {
            return assign(val.getAsBigInt());
        } else if (vt == NT_FLOAT) {
            return assign(val.getAsFloat());
        } else if (val.hasNode()) {
            return assign(val.takeNode());
        } else {
            // nothing or null
            return assign(static_cast<AbstractQoreNode*>(nullptr));
        }
    }

    DLLLOCAL bool exists() const {
        return assigned && (type != QV_Node || !is_nothing(v.n));
    }

    DLLLOCAL bool getAsBool() const {
        if (assigned) {
            switch (type) {
                case QV_Bool: return v.b;
                case QV_Int: return (bool)v.i;
                case QV_Float: return (bool)v.f;
                case QV_Node: return v.n ? v.n->getAsBool() : false;
                default: assert(false);
                // no break
            }
        }
        return false;
    }

    DLLLOCAL int64 getAsBigInt() const {
        if (assigned) {
            switch (type) {
                case QV_Bool: return (int64)v.b;
                case QV_Int: return v.i;
                case QV_Float: return (int64)v.f;
                case QV_Node: return v.n ? v.n->getAsBigInt() : 0;
                default: assert(false);
                // no break
            }
        }
        return 0;
    }

    DLLLOCAL double getAsFloat() const {
        if (assigned) {
            switch (type) {
                case QV_Bool: return (double)v.b;
                case QV_Int: return (double)v.i;
                case QV_Float: return v.f;
                case QV_Node: return v.n ? v.n->getAsFloat() : 0.0;
                default: assert(false);
                // no break
            }
        }
        return 0.0;
    }

    DLLLOCAL AbstractQoreNode* getInternalNode() const {
        return assigned && type == QV_Node ? v.n : nullptr;
    }

    DLLLOCAL qore_type_t getType() const {
        if (assigned)
            switch (type) {
                case QV_Bool: return NT_BOOLEAN;
                case QV_Int: return NT_INT;
                case QV_Float: return NT_FLOAT;
                case QV_Node: return v.n ? v.n->getType() : NT_NOTHING;
                default: assert(false);
                // no break
            }
        return NT_NOTHING;
    }

    DLLLOCAL const char* getTypeName() const {
        if (assigned)
            switch (type) {
                case QV_Bool: return qoreBoolTypeName;
                case QV_Int: return qoreIntTypeName;
                case QV_Float: return qoreFloatTypeName;
                case QV_Node: return get_type_name(v.n);
                default: assert(false);
                // no break
            }
        return "NOTHING";
    }

    // lvalue operations
    DLLLOCAL int64 plusEqualsBigInt(int64 i) {
        assert(type == QV_Int);

        switch (type) {
            case QV_Int:
                if (!assigned) {
                    assigned = true;
                    return v.i = i;
                }
                return v.i += i;

                // to avoid warnings about missing enum values
            default:
                assert(false);
                // no break
      }

      return 0;
   }

    DLLLOCAL double plusEqualsFloat(double f) {
        assert(type == QV_Float);

        switch (type) {
            case QV_Float:
                if (!assigned) {
                    assigned = true;
                    return v.f = f;
                }
                return v.f += f;

                // to avoid warnings about missing enum values
            default:
                assert(false);
                    // no break
        }

        return 0.0;
    }

    DLLLOCAL int64 minusEqualsBigInt(int64 i) {
        assert(type == QV_Int);

        switch (type) {
            case QV_Int:
                if (!assigned) {
                    assigned = true;
                    return v.i = -i;
                }
                return v.i -= i;

            // to avoid warnings about missing enum values
            default:
                assert(false);
                // no break
        }

        return 0;
    }

    DLLLOCAL double minusEqualsFloat(double f) {
        assert(type == QV_Float);

        switch (type) {
            case QV_Float:
                if (!assigned) {
                    assigned = true;
                    return v.f = -f;
                }
            return v.f -= f;

                // to avoid warnings about missing enum values
            default:
                assert(false);
                    // no break
        }

        return 0.0;
    }

    DLLLOCAL int64 orEqualsBigInt(int64 i) {
        assert(type == QV_Int);

        switch (type) {
            case QV_Int:
                if (!assigned) {
                    assigned = true;
                    return v.i = i;
                }
                    return v.i |= i;

            // to avoid warnings about missing enum values
            default:
                assert(false);
                    // no break
        }

        return 0;
    }

    DLLLOCAL int64 andEqualsBigInt(int64 i) {
        assert(type == QV_Int);

        switch (type) {
            case QV_Int:
                if (!assigned) {
                    assigned = true;
                    return v.i = 0;
                }
                return v.i &= i;

            // to avoid warnings about missing enum values
            default:
                assert(false);
                // no break
        }

        return 0;
    }

    DLLLOCAL int64 modulaEqualsBigInt(int64 i) {
        assert(type == QV_Int);

        switch (type) {
            case QV_Int:
                if (!assigned) {
                    assigned = true;
                    return v.i = 0;
                }
                return i ? v.i %= i : v.i = 0;

                // to avoid warnings about missing enum values
            default:
                assert(false);
                // no break
            }

            return 0;
        }

    DLLLOCAL int64 multiplyEqualsBigInt(int64 i) {
        assert(type == QV_Int);

        switch (type) {
            case QV_Int:
                if (!assigned) {
                    assigned = true;
                    return v.i = 0;
                }
                return v.i *= i;

            // to avoid warnings about missing enum values
            default:
                assert(false);
                // no break
        }

        return 0;
    }

    DLLLOCAL int64 divideEqualsBigInt(int64 i) {
        assert(type == QV_Int);
        assert(i);

        switch (type) {
            case QV_Int:
                if (!assigned) {
                    assigned = true;
                    return v.i = 0;
                }
                return v.i /= i;

            // to avoid warnings about missing enum values
            default:
                assert(false);
                // no break
        }

        return 0;
    }

    DLLLOCAL int64 xorEqualsBigInt(int64 i) {
        assert(type == QV_Int);

        switch (type) {
            case QV_Int:
                if (!assigned) {
                    assigned = true;
                    return v.i = i;
                }
                return v.i ^= i;

                // to avoid warnings about missing enum values
            default:
                assert(false);
                // no break
        }

        return 0;
    }

    DLLLOCAL int64 shiftLeftEqualsBigInt(int64 i) {
        assert(type == QV_Int);

        switch (type) {
            case QV_Int:
                if (!assigned) {
                    assigned = true;
                    return v.i = 0;
                }
                return v.i <<= i;

            // to avoid warnings about missing enum values
            default:
                assert(false);
                // no break
        }

        return 0;
    }

    DLLLOCAL int64 shiftRightEqualsBigInt(int64 i) {
        assert(type == QV_Int);

        switch (type) {
            case QV_Int:
                if (!assigned) {
                    assigned = true;
                    return v.i = 0;
                }
                return v.i >>= i;

            // to avoid warnings about missing enum values
            default:
                assert(false);
                // no break
        }

        return 0;
    }

    DLLLOCAL int64 postIncrementBigInt() {
        assert(type == QV_Int);

        switch (type) {
            case QV_Int: {
                if (!assigned) {
                    assigned = true;
                    v.i = 1;
                    return 0;
                }
                int64 rv = (int64)v.i;
                ++v.i;
                return rv;
            }

                // to avoid warnings about missing enum values
            default:
                assert(false);
                // no break
        }

        return 0;
    }

    DLLLOCAL int64 preIncrementBigInt() {
        assert(type == QV_Int);

        switch (type) {
            case QV_Int:
                if (!assigned) {
                    assigned = true;
                    return v.i = 1;
                }
                return ++v.i;

            // to avoid warnings about missing enum values
            default:
                assert(false);
                // no break
        }

        return 0;
    }

    DLLLOCAL int64 postDecrementBigInt() {
        assert(type == QV_Int);

        switch (type) {
            case QV_Int:
                if (!assigned) {
                    assigned = true;
                    v.i = -1;
                    return 0;
                }
                return v.i--;

            // to avoid warnings about missing enum values
            default:
                assert(false);
                // no break
        }

        return 0;
    }

    DLLLOCAL int64 preDecrementBigInt() {
        assert(type == QV_Int);

        switch (type) {
            case QV_Int:
                if (!assigned) {
                    assigned = true;
                    return v.i = -1;
                }
                return --v.i;

                // to avoid warnings about missing enum values
            default:
                assert(false);
                // no break
        }
        return 0;
    }

    DLLLOCAL double postIncrementFloat() {
        assert(type == QV_Float);

        switch (type) {
            case QV_Float:
                if (!assigned) {
                    assigned = true;
                    v.f = 1.0;
                    return 0.0;
                }
                return v.f++;

                // to avoid warnings about missing enum values
            default:
                assert(false);
                // no break
        }

        return 0.0;
    }

    DLLLOCAL double preIncrementFloat() {
        assert(type == QV_Float);

        switch (type) {
            case QV_Float:
                if (!assigned) {
                    assigned = true;
                    return v.f = 1.0;
                }
                return ++v.f;

            // to avoid warnings about missing enum values
            default:
                assert(false);
                // no break
        }

        return 0.0;
    }

    DLLLOCAL double postDecrementFloat() {
        assert(type == QV_Float);

        switch (type) {
            case QV_Float:
                if (!assigned) {
                    assigned = true;
                    v.f = -1.0;
                    return 0.0;
                }
                return v.f--;

            // to avoid warnings about missing enum values
            default:
                assert(false);
                // no break
        }

        return 0.0;
    }

    DLLLOCAL double preDecrementFloat() {
        assert(type == QV_Float);

        switch (type) {
            case QV_Float:
                if (!assigned) {
                    assigned = true;
                    return v.f = -1.0;
                }
                return --v.f;

                // to avoid warnings about missing enum values
            default:
                assert(false);
                // no break
        }
        return 0;
    }

    DLLLOCAL double multiplyEqualsFloat(double f) {
        assert(type == QV_Float);

        switch (type) {
            case QV_Float:
                if (!assigned) {
                    assigned = true;
                    return v.f = 0;
                }
            return v.f *= f;

            // to avoid warnings about missing enum values
            default:
                assert(false);
                    // no break
        }

        return 0;
    }

    DLLLOCAL double divideEqualsFloat(double f) {
        assert(type == QV_Float);
        assert(f);

        switch (type) {
            case QV_Float:
                if (!assigned) {
                    assigned = true;
                    return v.f = 0;
                }
                return v.f /= f;

                // to avoid warnings about missing enum values
            default:
                assert(false);
                // no break
        }

        return 0;
    }

    DLLLOCAL QoreValue remove(bool& was_static_assignment) {
        assert(!was_static_assignment);

        if (!assigned)
            return QoreValue();
        assigned = false;
        if (static_assignment) {
            static_assignment = false;
            was_static_assignment = true;
        }

        switch (type) {
            case QV_Bool:
                return v.b;
            case QV_Int:
                return v.i;
            case QV_Float:
                return v.f;
            case QV_Node:
                if (!is_closure)
                    check_lvalue_object_in_out(nullptr, v.n);
                return v.n;
            default:
                assert(false);
                // no break
        }
        return QoreValue();
    }

    // ignore any current value and sets the lvalue to unassigned
    DLLLOCAL void unassignIgnore() {
        if (assigned) {
            assigned = false;
            if (static_assignment)
                static_assignment = false;
            if (type == QV_Node && !is_closure)
                check_lvalue_object_in_out(nullptr, v.n);
        }
    }

    DLLLOCAL QoreValue removeValue(bool for_del) {
        if (!assigned)
            return QoreValue();
        assigned = false;
        assert(!static_assignment);

        switch (type) {
            case QV_Bool:
                return for_del ? QoreValue() : QoreValue(v.b);
            case QV_Int:
                return for_del ? QoreValue() : QoreValue(v.i);
            case QV_Float:
                return for_del ? QoreValue() : QoreValue(v.f);
            case QV_Node:
                if (!is_closure)
                    check_lvalue_object_in_out(nullptr, v.n);
                return v.n;
            default:
                assert(false);
                // no break
        }
        return QoreValue();
    }
};

typedef QoreLValue<> QoreLValueGeneric;

#endif
