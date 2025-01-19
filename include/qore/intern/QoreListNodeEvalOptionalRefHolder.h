/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreListNodeEvalOptionalRefHolder.h

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

#ifndef _QORE_QORELISTNODEEVALOPTIONALREFHOLDER_H

#define _QORE_QORELISTNODEEVALOPTIONALREFHOLDER_H

//! For use on the stack only: manages result of the optional evaluation of a QoreListNode
class QoreListNodeEvalOptionalRefHolder {
private:
    QoreListNode* val;
    ExceptionSink* xsink;
    bool needs_deref;

    DLLLOCAL void discardIntern() {
        if (needs_deref && val) {
            val->deref(xsink);
        }
    }

    DLLLOCAL void evalIntern(const QoreListNode* exp) {
        if (exp) {
            val = exp->evalList(needs_deref, xsink);
            //printd(0, "QoreListNodeEvalOptionalRefHolder::evalIntern() this: %p exp: %p '%s' (%d) val: %p '%s' "
            //    "(%d)\n", this, exp, exp ? get_full_type_name(exp) : "n/a", exp->size(), val,
            //    val ? get_full_type_name(val) : "n/a", val ? val->size() : 0);
        } else {
            val = nullptr;
            needs_deref = false;
        }
    }

    DLLLOCAL void evalIntern(QoreListNode* exp);

    DLLLOCAL void editIntern() {
        if (!val) {
            val = new QoreListNode(autoTypeInfo);
            needs_deref = true;
        }
        else if (!needs_deref || !val->is_unique()) {
            val = val->copy();
            needs_deref = true;
        }
    }

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreListNodeEvalOptionalRefHolder(const QoreListNodeEvalOptionalRefHolder&);
    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreListNodeEvalOptionalRefHolder& operator=(const QoreListNodeEvalOptionalRefHolder&);
    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL void *operator new(size_t);

public:
    //! initializes an empty object and saves the ExceptionSink object
    DLLLOCAL QoreListNodeEvalOptionalRefHolder(ExceptionSink* n_xsink) : val(nullptr), xsink(n_xsink), needs_deref(false) {
    }

    //! performs an optional evaluation of the list (sets the dereference flag)
    DLLLOCAL QoreListNodeEvalOptionalRefHolder(const QoreListNode* exp, ExceptionSink* n_xsink) : xsink(n_xsink) {
        evalIntern(exp);
    }

    //! clears the object (dereferences the old object if necessary)
    DLLLOCAL ~QoreListNodeEvalOptionalRefHolder() {
        discardIntern();
    }

    //! clears the object (dereferences the old object if necessary)
    DLLLOCAL void discard() {
        discardIntern();
        needs_deref = false;
        val = nullptr;
    }

    //! assigns a new value by executing the given list and dereference flag to this object, dereferences the old object if necessary
    DLLLOCAL void assignEval(const QoreListNode* exp) {
        discardIntern();
        evalIntern(exp);
    }

    //! assigns a new value by executing the given list and dereference flag to this object, dereferences the old object if necessary
    DLLLOCAL void assignEval(QoreListNode* exp) {
        discardIntern();
        evalIntern(exp);
    }

    //! assigns a new value and dereference flag to this object, dereferences the old object if necessary
    DLLLOCAL void assign(bool n_needs_deref, QoreListNode* n_val) {
        discardIntern();
        needs_deref = n_needs_deref;
        val = n_val;
    }

    //! returns true if the object contains a temporary (evaluated) value that needs a dereference
    DLLLOCAL bool needsDeref() const {
        return needs_deref;
    }

    //! returns a referenced value - the caller will own the reference
    /**
        The list is referenced if necessary (if it was a temporary value)
        @return the list value, where the caller will own the reference count
    */
    DLLLOCAL QoreListNode* getReferencedValue() {
        if (needs_deref) {
            needs_deref = false;
        }
        else if (val) {
            val->ref();
        }
        return val;
    }

    //! will create a unique list so the list can be edited
    DLLLOCAL void edit() {
        editIntern();
    }

    DLLLOCAL QoreValue& getEntryReference(size_t index);

    DLLLOCAL size_t size() const {
        return val ? val->size() : 0;
    }

    //! returns true if the value being managed can be edited/updated
    DLLLOCAL bool canEdit() const {
        return !val || needs_deref || val->is_unique();
    }

    //! returns a pointer to the QoreListNode object being managed
    /**
        if you need a referenced value, use getReferencedValue()
        @return a pointer to the QoreListNode object being managed (or 0 if none)
    */
    DLLLOCAL const QoreListNode* operator->() const { return val; }

    DLLLOCAL QoreListNode* operator->() { return val; }

    //! returns a pointer to the QoreListNode object being managed
    DLLLOCAL const QoreListNode* operator*() const { return val; }

    DLLLOCAL QoreListNode* operator*() { return val; }

    //! returns true if a QoreListNode object pointer is being managed, false if the pointer is 0
    DLLLOCAL operator bool() const { return val != 0; }
};

#endif
