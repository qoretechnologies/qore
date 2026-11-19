/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AbstractIteratorHelper.h

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

#ifndef _QORE_ABSTRACTITERATORHELPER_H

#define _QORE_ABSTRACTITERATORHELPER_H

#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/RuntimeConfig.h"
#include "qore/QoreIteratorBase.h"

class AbstractIteratorHelper {
public:
    QoreObject* obj = nullptr;
    const QoreMethod* nextMethod = nullptr;
    const QoreExternalMethodVariant* nextVariant = nullptr;
    const QoreMethod* getValueMethod = nullptr;
    const QoreExternalMethodVariant* getValueVariant = nullptr;
    // When non-null, the iterator's C++ private data is a QoreIteratorBase
    // that exposes native iteration; next()/getValue() can bypass the Qore
    // method dispatch and call directly into the iterator's C++ methods.
    // This cuts per-element overhead by ~5-10x for map/select/foldl/foreach
    // over C++-native iterators (RangeIterator, StringSplitIterator, etc.).
    QoreIteratorBase* native_iter = nullptr;
    bool valid = false;

    DLLLOCAL AbstractIteratorHelper(ExceptionSink* xsink, const char* op, QoreObject* o, bool fwd = true,
            bool get_value = true) {
        bool priv;
        const QoreClass* qc = o->getClass()->getClass(fwd ? *QC_ABSTRACTITERATOR : *QC_ABSTRACTBIDIRECTIONALITERATOR,
            priv);
        if (!qc)
            return;

        const qore_class_private* class_ctx = runtime_get_class();
        if (class_ctx && !qore_class_private::runtimeCheckPrivateClassAccess(*o->getClass(), class_ctx))
            class_ctx = nullptr;
        obj = o;
        const qore_class_private* cls = qore_class_private::get(*o->getClass());
        // get the "next" and "getValue" methods: the methods that calls of them on the object make
        ClassAccess next_access;
        nextMethod = cls->runtimeFindCommittedMethod(fwd ? "next" : "prev", next_access, class_ctx);
        // method must be found because we have an instance of AbstractIterator/AbstractBidirectionalIterator
        assert(nextMethod);
        ClassAccess value_access = Public;
        if (get_value) {
            getValueMethod = cls->runtimeFindCommittedMethod("getValue", value_access, class_ctx);
            // method must be found because we have an instance of AbstractIterator
            assert(getValueMethod);
        }
        // without a class context that can access the private methods of the class, a method that is not accessible
        // is skipped for the accessible method of the name, as for a call of the method (qore#6704); one test in the
        // common case (Public is 0)
        if (!class_ctx && (next_access | value_access)) {
            findPublicMethods(cls, fwd, next_access, value_access);
        }
        nextVariant = getCheckVariant(xsink, op, nextMethod);
        if (!nextVariant)
            return;
        if (get_value) {
            getValueVariant = getCheckVariant(xsink, op, getValueMethod);
            if (!getValueVariant)
                return;
        }
        // Fast-path detection: forward iterators only (nativeNext matches
        // next(); the prev() path still goes through method dispatch).
        // Probing the priv data by the *specific* class's ID is the only
        // reliable way — there is no single CID for the abstract-iterator
        // family, so we use the concrete class ID from `o->getClass()`.
        // The helper stashes a raw pointer to the priv; correctness relies
        // on `obj` outliving the helper (always true by call-site contract —
        // the helper is a stack variable within functional-operator scope
        // and obj is ref'd for that duration).
        if (fwd) {
            ExceptionSink probe_xsink;
            AbstractPrivateData* raw_priv = o->getReferencedPrivateData(
                o->getClass()->getID(), &probe_xsink);
            if (probe_xsink) {
                probe_xsink.clear();
            } else if (raw_priv) {
                QoreIteratorBase* ib = dynamic_cast<QoreIteratorBase*>(raw_priv);
                if (ib && ib->supportsNativeIteration()) {
                    native_iter = ib;
                }
                raw_priv->deref(xsink);
            }
        }
        valid = true;
    }

    DLLLOCAL operator bool() const {
        return valid;
    }

    DLLLOCAL bool next(ExceptionSink* xsink) {
        if (native_iter) {
            return native_iter->nativeNext(xsink);
        }
        assert(nextMethod);
        assert(nextVariant);
        RuntimeConfig& rc = rc_get_current_ref();
        ValueHolder rv(qore_method_private::evalNormalVariant(*nextMethod, xsink, rc, obj, nextVariant, 0), xsink);
        return rv->getAsBool();
    }

    DLLLOCAL QoreValue getValue(ExceptionSink* xsink) {
        if (native_iter) {
            return native_iter->nativeGetValue(xsink);
        }
        assert(getValueMethod);
        assert(getValueVariant);
        RuntimeConfig& rc = rc_get_current_ref();
        return qore_method_private::evalNormalVariant(*getValueMethod, xsink, rc, obj, getValueVariant, 0);
    }

    //! replaces the iterator methods that a caller outside the class hierarchy cannot call with the methods of their
    //! names that it calls, if any (qore#6704)
    DLLLOCAL QORE_INTERN_NOINLINE void findPublicMethods(const qore_class_private* cls, bool fwd,
            ClassAccess next_access, ClassAccess value_access) {
        if (next_access > Public) {
            if (const QoreMethod* m = cls->runtimeFindPublicCommittedMethod(fwd ? "next" : "prev", QCCM_NORMAL)) {
                nextMethod = m;
            }
        }
        if (value_access > Public) {
            if (const QoreMethod* m = cls->runtimeFindPublicCommittedMethod("getValue", QCCM_NORMAL)) {
                getValueMethod = m;
            }
        }
    }

    // finds a method with no arguments
    DLLLOCAL static const QoreExternalMethodVariant* getCheckVariant(ExceptionSink* xsink, const char* op,
            const QoreMethod* m) {
        const qore_class_private* class_ctx = runtime_get_class();
        const MethodVariantBase* variant = reinterpret_cast<const MethodVariantBase*>(
            qore_method_private::get(*m)->getFunction()->runtimeFindVariant(xsink, (QoreListNode*)nullptr, false,
                class_ctx, nullptr, nullptr, nullptr, nullptr, nullptr, ::getProgram())
        );
        // this could throw an exception if the variant is builtin and has functional flags not allowed in the current
        // pgm, for example
        assert(xsink);
        if (*xsink)
            return nullptr;
        // we must have a variant here because we have an instance of AbstractIterator
        assert(variant);
        if (variant->isPrivate()) {
            // check for access to the class holding the private method
            if (!qore_class_private::runtimeCheckPrivateClassAccess(*(variant->method()->getClass()), class_ctx)) {
                QoreString opstr(op);
                opstr.toupr();
                opstr.concat("-ITERATOR-ERROR");
                xsink->raiseException(opstr.getBuffer(), "cannot access private %s::%s() iterator method with the %s",
                    variant->method()->getClass()->getName(), m->getName(), op);
                return nullptr;
            }
        }
        return reinterpret_cast<const QoreExternalMethodVariant*>(variant);
    }

    /*
    DLLLOCAL QoreObject* getReferencedObject() const {
        obj->ref();
        return obj;
    }
    */
};

#endif
