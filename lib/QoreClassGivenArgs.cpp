/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreClassGivenArgs.cpp

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

// The call of the code that a member of an object holds (a closure or a call reference) with an argument list whose
// values a call gave (qore_class_private::evalMethodGivenArgs(), issue #5958), compiled in a unit of its own, outside
// the single compilation unit of libqore: in that unit, it would change how the compiler inlines the code of the
// method calls of QoreClass.cpp and QoreObject.cpp, which it shares.

#include <qore/Qore.h>
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/QoreObjectIntern.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/CallReferenceNode.h"
#include "qore/intern/QoreClosureNode.h"

// documented in QoreClassIntern.h
bool qore_class_eval_member_code_given_args(const qore_class_private& qcp, QoreObject* self, const char* nme,
        QoreListNode* args, const qore_class_private* class_ctx, ExceptionSink* xsink, QoreValue& rv) {
    const QoreMemberInfo* mi = qcp.runtimeGetCallableMemberInfo(nme, class_ctx);
    if (!mi) {
        return false;
    }
    const qore_class_private* member_class_ctx = mi->getClassContext(class_ctx);
    qore_object_private* obj_priv = qore_object_private::get(*self);
    // the sandboxed read rule (qore#7033)
    if (obj_priv->checkSandboxedMemberRead(nme, class_ctx, class_ctx, xsink)) {
        rv = QoreValue();
        return true;
    }
    QoreValue mv{};
    bool exists = false;
    {
        QoreAutoVarRWReadLocker al(obj_priv->rml);
        if (obj_priv->status == OS_DELETED) {
            return false;
        }
        const QoreHashNode* odata = member_class_ctx ? obj_priv->getInternalData(member_class_ctx) : obj_priv->data;
        if (odata) {
            mv = qore_hash_private::get(*odata)->getReferencedKeyValueIntern(nme, exists);
        }
    }
    if (!exists) {
        return false;
    }
    ValueHolder mvh(mv, xsink);
    if (*xsink) {
        rv = QoreValue();
        return true;
    }
    if (mv.getType() != NT_FUNCREF && mv.getType() != NT_RUNTIME_CLOSURE) {
        return false;
    }
    const ResolvedCallReferenceNode* ref = dynamic_cast<const ResolvedCallReferenceNode*>(mv.getInternalNode());
    if (!ref) {
        return false;
    }
    rv = qore_invoke_call_ref_given_args(ref, args, xsink);
    return true;
}

// documented in QoreClassIntern.h
QoreValue qore_invoke_call_ref_given_args(const ResolvedCallReferenceNode* ref, QoreListNode* args,
        ExceptionSink* xsink) {
    // the parameters of a closure or of a reference to user code take the values of the argument list over; the call
    // runs with the sandbox provenance of the reference: a closure's execValueTmpArgs() and invoke() apply it
    // themselves, while the default ResolvedCallReferenceNodeIntern::execValueTmpArgs() calls execValue(), which does
    // not, so it is applied here for that branch only
    if (ref->getType() == NT_RUNTIME_CLOSURE) {
        return static_cast<const QoreClosureBase*>(ref)->execValueTmpArgs(args, xsink);
    }
    if (auto* rcr = dynamic_cast<const ResolvedCallReferenceNodeIntern*>(ref)) {
        QoreSandboxProvenanceHelper provenance(ref);
        return rcr->execValueTmpArgs(args, xsink);
    }
    return ref->invoke(args, xsink);
}
