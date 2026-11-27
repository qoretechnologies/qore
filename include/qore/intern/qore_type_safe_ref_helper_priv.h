/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_type_safe_ref_helper_priv.h

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

#ifndef _QORE_INTERN_QORETYPESAFEREFERENCEHELPER_H

#define _QORE_INTERN_QORETYPESAFEREFERENCEHELPER_H

#include "qore/intern/Variable.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct qore_type_safe_ref_helper_priv_t {
    // A non-overlapping member can be transparently replaced by finishForCallback(); a base subobject cannot.
    LValueHelper lvalue;

    //! how the lvalue of the helper is resolved again for a store made again (qore#7391)
    enum class BindBase : uint8_t {
        //! not known: the helper was used in a way that it does not record (get()); a store that records reference
        //! elements to bind under the lock of the lvalue fails closed with REFERENCE-BIND-ERROR
        None,
        //! the lvalue of the reference ref, then bind_steps
        Reference,
        //! the member bind_steps[0] of the object base_obj, then the other steps
        Object,
    };

    //! how the lvalue of the helper is resolved again (qore#7391)
    BindBase bind_base = BindBase::None;
    //! the reference that the helper resolved (BindBase::Reference)
    const ReferenceNode* ref = nullptr;
    //! the object whose member the helper resolved (BindBase::Object), which the caller keeps while it uses the helper
    QoreObject* base_obj = nullptr;
    //! the keys that the helper moved to (setObjKey(), moveToHashObjKey()), each with its for_remove argument
    std::vector<std::pair<std::string, bool>> bind_steps;

    DLLLOCAL qore_type_safe_ref_helper_priv_t(const ReferenceNode* ref, ExceptionSink* xsink)
            : lvalue(*ref, xsink), bind_base(BindBase::Reference), ref(ref) {
    }

    DLLLOCAL qore_type_safe_ref_helper_priv_t(ExceptionSink* xsink) : lvalue(xsink) {
    }

    DLLLOCAL ~qore_type_safe_ref_helper_priv_t() {
    }

    DLLLOCAL AbstractQoreNode* getUnique(ExceptionSink *xsink) {
        // the caller can change the value in place without this helper knowing
        lvalue.disableRemovalScanSkip();
        // the caller modifies the value in place, which leaves a plain value of the base type of an enum value: an
        // enum lvalue refuses it (qore#6195)
        if (lvalue.ensureUnique("<reference>")) {
            return nullptr;
        }
        assert(!lvalue.getNodeValue() || (lvalue.getNodeValue()->reference_count() == 1));
        return lvalue.getNodeValue();
    }

    DLLLOCAL int assign(QoreValue val) {
        lvalue.disableRemovalScanSkip();
        // the lvalue of a reference whose type can bind a reference element is assigned out of line (qore#7391)
        if (bind_base != BindBase::None && QoreTypeInfo::mayBindReference(lvalue.getTypeInfo())) [[unlikely]] {
            return assignWithBindRetry(val);
        }
        return lvalue.assignNative(val, "<reference>");
    }

    //! assigns \a val to the lvalue of the helper, binding its reference elements with no lvalue lock held
    /** If the check of the value records reference elements to bind, the helper is reset, which releases its locks,
        the references are bound, and the lvalue is resolved again as the helper resolved it (resolveAgain()) and the
        value that the first round converted assigned (qore#7391).  The helper then holds the lvalue again, as before
        the assignment.
    */
    DLLLOCAL int assignWithBindRetry(QoreValue val);

    //! resolves the lvalue of the helper again in \a lvh, as the helper resolved it (qore#7391)
    /** @return -1 if it could not be resolved (an exception was raised, or the lvalue does not exist), 0 if it was
    */
    DLLLOCAL int resolveAgain(LValueHelper& lvh) const;

    //! records that the helper resolved the lvalue of \a r (qore#7391)
    DLLLOCAL void setReference(const ReferenceNode* r) {
        bind_base = BindBase::Reference;
        ref = r;
        base_obj = nullptr;
        bind_steps.clear();
    }

    //! records that the helper resolved the member \a mem of \a obj (qore#7391)
    DLLLOCAL void setObjectBase(QoreObject* obj, const char* mem, bool for_remove) {
        bind_base = BindBase::Object;
        ref = nullptr;
        base_obj = obj;
        bind_steps.clear();
        bind_steps.emplace_back(mem, for_remove);
    }

    //! records that the helper moved to the key \a mem of the hash or the object of its value (qore#7391)
    DLLLOCAL void addBindStep(const char* mem, bool for_remove) {
        if (bind_base != BindBase::None) {
            bind_steps.emplace_back(mem, for_remove);
        }
    }

    DLLLOCAL int doHashLValue(qore_type_t t, const char* mem, bool for_remove) {
        lvalue.disableRemovalScanSkip();
        return lvalue.doHashLValue(t, mem, for_remove);
    }

    DLLLOCAL int doObjLValue(QoreObject* obj, const char* mem, bool for_remove) {
        lvalue.disableRemovalScanSkip();
        return lvalue.doObjLValue(obj, mem, for_remove);
    }

    DLLLOCAL const QoreValue getValue() const {
        return lvalue.getValue();
    }

    DLLLOCAL qore_type_t getType() const {
        return lvalue.getType();
    }

    DLLLOCAL const char* getTypeName() const {
        return lvalue.getTypeName();
    }

    DLLLOCAL static LValueHelper& get(QoreTypeSafeReferenceHelper& ref) {
        // the caller can use the helper in any way, also to resolve another lvalue with it: the lvalue cannot be
        // resolved again, and a store through the helper that records reference elements to bind under its lock fails
        // closed (qore#7391)
        ref.priv->lvalue.disableRemovalScanSkip();
        ref.priv->bind_base = BindBase::None;
        return ref.priv->lvalue;
    }
};

#endif