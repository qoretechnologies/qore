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

struct qore_type_safe_ref_helper_priv_t {
    // A non-overlapping member can be transparently replaced by finishForCallback(); a base subobject cannot.
    LValueHelper lvalue;
    //! the reference that the helper resolved, while its lvalue is the lvalue of the reference (qore#7391)
    /** A store whose check records reference elements to bind is made again through it: see assignWithBindRetry()
    */
    const ReferenceNode* ref = nullptr;

    DLLLOCAL qore_type_safe_ref_helper_priv_t(const ReferenceNode* ref, ExceptionSink* xsink)
            : lvalue(*ref, xsink), ref(ref) {
    }

    DLLLOCAL qore_type_safe_ref_helper_priv_t(const AbstractQoreNode* exp, ExceptionSink* xsink)
            : lvalue(exp, xsink) {
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
        if (ref && QoreTypeInfo::mayBindReference(lvalue.getTypeInfo())) [[unlikely]] {
            return assignWithBindRetry(val);
        }
        return lvalue.assignNative(val, "<reference>");
    }

    //! assigns \a val to the lvalue of the reference, binding its reference elements with no lvalue lock held
    /** If the check of the value records reference elements to bind, the helper is reset, which releases its locks,
        the references are bound, and the reference is resolved again and the value that the first round converted
        assigned (qore#7391).  The helper then holds the lvalue of the reference again, as before the assignment.
    */
    DLLLOCAL int assignWithBindRetry(QoreValue val);

    //! records that the helper resolved the lvalue of \a r (qore#7391)
    DLLLOCAL void setReference(const ReferenceNode* r) {
        ref = r;
    }

    DLLLOCAL int doHashLValue(qore_type_t t, const char* mem, bool for_remove) {
        lvalue.disableRemovalScanSkip();
        // the lvalue is not that of the reference anymore
        ref = nullptr;
        return lvalue.doHashLValue(t, mem, for_remove);
    }

    DLLLOCAL int doObjLValue(QoreObject* obj, const char* mem, bool for_remove) {
        lvalue.disableRemovalScanSkip();
        // the lvalue is not that of the reference anymore
        ref = nullptr;
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
        // the caller can use the helper in any way, also to resolve another lvalue with it
        ref.priv->lvalue.disableRemovalScanSkip();
        ref.priv->ref = nullptr;
        return ref.priv->lvalue;
    }
};

#endif