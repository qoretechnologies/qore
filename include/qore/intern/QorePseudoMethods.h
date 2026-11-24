/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QorePseudoMethods.h

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

#ifndef _QORE_PSEUDOMETHODS_H

#define _QORE_PSEUDOMETHODS_H

DLLLOCAL void pseudo_classes_init();
DLLLOCAL void pseudo_classes_del();
//! Resolves a receiver or declaring pseudo-class by its serialized class path.
DLLLOCAL const QoreClass* qore_pseudo_find_class_by_path(const char* path);
DLLLOCAL QoreValue pseudo_classes_eval(const QoreValue n, const char* name, const QoreListNode* args, ExceptionSink* xsink);
DLLLOCAL QoreValue pseudo_classes_eval(LexicalSemanticPolicy policy, const QoreValue n, const char* name,
    const QoreListNode* args, ExceptionSink* xsink);

//! Raises the exception that pseudo_classes_eval() raises for a method that does not exist before it evaluates the
//! arguments (issue #5784)
/** @return -1 if the exception was raised, 0 if not
*/
DLLLOCAL int pseudo_classes_check_method(const QoreValue n, const char* name, ExceptionSink* xsink);
//! Returns true if a pseudo-method of the pseudo-class @a qc, resolved at parse time, can run on the value @a n
/** The class of a value of another type than the one the parser assumed for the expression (a call through a call
    reference whose declared return type the code does not honor, ...) does not accept it: the method of the name is
    then looked up for the value's own type (pseudo_classes_eval()), which raises the usual exception if there is none
    (qore#7363).  The pseudo-class of all values accepts every value, and the pseudo-class of bool accepts a buffer (the
    result of a comparison with a buffer operand, qore#6356).
*/
DLLLOCAL bool qore_pseudo_receiver_accepts_slow(const QoreClass* qc, const QoreValue n);
DLLLOCAL const QoreMethod* pseudo_classes_find_method(qore_type_t t, const char* mname, QoreClass*& qc);
DLLLOCAL const QoreMethod* pseudo_classes_find_method(const QoreTypeInfo* typeInfo, const char* mname, QoreClass*& qc, bool& possible_match);
DLLLOCAL void qore_pseudo_check_return_type_info(const QoreFunction* func, const QoreTypeInfo* pseudoTypeInfo, const QoreTypeInfo*& returnTypeInfo);

#define NODE_ARRAY_LEN (NT_NUMBER + 1)

//! the pseudo-classes of the basic types by type code (qore_pseudo_receiver_accepts() decides a call inline with it)
extern DLLLOCAL QoreClass* qore_pseudo_basic_classes[];

//! Returns true if a pseudo-method of the pseudo-class @a qc, resolved at parse time, can run on the value @a n
/** The common case, a value of the type that the parser assumed (or a call on the pseudo-class of all values), is
    decided here with two comparisons; any other value goes to qore_pseudo_receiver_accepts_slow()
*/
QORE_INTERN_ALWAYS_INLINE bool qore_pseudo_receiver_accepts(const QoreClass* qc, const QoreValue n) {
    // the tags of a node and of an integer cover most receivers; reading the type directly avoids the call of
    // QoreValue::getType()
    if (n.isPlainPointer()) {
        const AbstractQoreNode* p = n.getPtr();
        if (__builtin_expect(p != nullptr, 1)) {
            const qore_type_t t = p->getType();
            if (__builtin_expect(t >= 0 && t < NODE_ARRAY_LEN && qc == qore_pseudo_basic_classes[t], 1)) {
                return true;
            }
        }
    } else if (n.isInt() && qc == qore_pseudo_basic_classes[NT_INT]) {
        return true;
    }
    return qore_pseudo_receiver_accepts_slow(qc, n);
}

#endif
