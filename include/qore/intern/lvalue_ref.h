/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    lvalue_ref.h

    POSIX thread library for Qore

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

#ifndef _QORE_INTERN_LVALUE_REF_H
#define _QORE_INTERN_LVALUE_REF_H

class RSetHelper;

class lvalue_ref {
public:
    QoreValue vexp{};
    const QoreTypeInfo* typeInfo;
    QoreObject* self;
    //! the Program of the lvalue, with a dependency reference (QoreProgram::depRef())
    /** The dependency reference keeps the Program allocated after it has been deleted, and so its address from being
        given to another Program, while the reference exists: its use enters the Program, which refuses the entry
        once it is being deleted (see QoreReferenceProgramHelper)
    */
    QoreProgram* pgm;
    const void* lvalue_id;
    const qore_class_private* cls;
    //! weak references to the reference node, which keep it allocated after its last reference is released
    QoreReferenceCounter weak_refs;

    DLLLOCAL lvalue_ref(QoreValue lvexp, const QoreTypeInfo* typeInfo, QoreObject* self, const void* lvid, const qore_class_private* cls);

    DLLLOCAL lvalue_ref(const lvalue_ref& old);

    DLLLOCAL ~lvalue_ref() {
        //printd(5, "lvalue_ref::~lvalue_ref() this: %p vexp: %p self: %p pgm: %p\n", this, vexp, self, pgm);
        if (self)
            self->tDeref();
        vexp.discard(nullptr);
        if (pgm) {
            pgm->depDeref();
        }
    }

    DLLLOCAL void del(ExceptionSink* xsink) {
        //printd(5, "lvalue_ref::del() this: %p vexp: %p self: %p pgm: %p\n", this, vexp, self, pgm);
        if (vexp) {
            vexp.discard(xsink);
            vexp = QoreValue();
        }
    }

    DLLLOCAL const QoreTypeInfo* getLValueTypeInfo() const {
        return QoreTypeInfo::getUniqueReturnComplexReference(typeInfo);
    }

    // returns true if a lock error has occurred and the transaction should be aborted or restarted; the rsection lock is held when this function is called
    DLLLOCAL bool scanReference(RSetHelper& rsh);

    // returns true if the object needs to be scanned for recursive references (ie could contain an object or closure or a container containing one of those)
    DLLLOCAL bool needsScan();

    DLLLOCAL static lvalue_ref* get(const ReferenceNode* r) {
        return r->priv;
    }

    //! Increments the weak reference count of a reference node
    DLLLOCAL static void weakRef(ReferenceNode* r) {
        r->priv->weak_refs.ROreference();
    }

    //! Decrements the weak reference count of a reference node and deletes it when no references remain
    DLLLOCAL static void weakDeref(ReferenceNode* r) {
        if (r->priv->weak_refs.ROdereference()) {
            delete r;
        }
    }

    DLLLOCAL static bool scanNode(RSetHelper& rsh, QoreValue vexp);
};

//! Returns whether binding a reference to a parameter of type \c reference<target_ti> leaves its lvalue as it is
/** QoreTypeSpec::acceptInputImpl() reads the referenced value and assigns it back with the parameter's referenced type
    to apply that type; this returns 1 when that read and assignment are known to change nothing and to raise nothing,
    so that they can be skipped (qore#7264):
    - \a target_ti is \c hash<auto>, \c *hash<auto>, \c list<auto> or \c *list<auto>, whose acceptance of a value
      of their own type neither converts nor rejects it (see the QTS_COMPLEXHASH and QTS_COMPLEXLIST cases of
      QoreTypeSpec::acceptInputImpl()), and which LValueHelper::assign() stores as it is;
    - the reference's lvalue is a closure-bound variable itself (a local variable or a closure variable, not an
      element, a key or a member of one), which is not read-only, holds no reference (its type is not a reference
      type, so it cannot change, see ClosureVarValue::getReference()), and whose type is \a target_ti, so that its
      value is one that \a target_ti accepted;
    - resolving the reference raises nothing (qore_reference_resolution_check())

    @return 1 if the read and assignment can be skipped, 0 if not, -1 if resolving the reference raised an exception in
    \a xsink (the exception that the resolution with LValueHelper would raise)
*/
DLLLOCAL int qore_reference_binding_keeps_value(const ReferenceNode* r, const QoreTypeInfo* target_ti,
        ExceptionSink* xsink);

#endif
