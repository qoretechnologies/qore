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
DLLLOCAL QoreValue pseudo_classes_eval(const QoreValue n, const char* name, const QoreListNode* args, ExceptionSink* xsink);
DLLLOCAL QoreValue pseudo_classes_eval(LexicalSemanticPolicy policy, const QoreValue n, const char* name,
    const QoreListNode* args, ExceptionSink* xsink);

//! Raises the exception that pseudo_classes_eval() raises for a method that does not exist before it evaluates the
//! arguments (issue #5784)
/** @return -1 if the exception was raised, 0 if not
*/
DLLLOCAL int pseudo_classes_check_method(const QoreValue n, const char* name, ExceptionSink* xsink);

//! Returns true if a pseudo-method of the class \a qc, resolved at parse time, can be called on a value of type \a t
/** It can if the pseudo-class of the type is \a qc or a class that inherits it, or \a qc is the <value> class; a value
    of another type (the value of a builtin whose declared type is wrong) finds the method by name in the pseudo-class
    of its type instead (qore#6813).  The one predicate of the check of the receiver before the arguments are evaluated
    (qore_check_method_call_receiver()) and of the call (MethodCallNode::execPseudo(), the pseudo-method dispatch of
    the IR interpreter and native code), so that both take the same path.  A value with no value is not given: it
    uses the <nothing> class, as before
*/
DLLLOCAL bool qore_pseudo_class_accepts_type(const QoreClass* qc, qore_type_t t);
DLLLOCAL const QoreMethod* pseudo_classes_find_method(qore_type_t t, const char* mname, QoreClass*& qc);
DLLLOCAL const QoreMethod* pseudo_classes_find_method(const QoreTypeInfo* typeInfo, const char* mname, QoreClass*& qc, bool& possible_match);
DLLLOCAL void qore_pseudo_check_return_type_info(const QoreFunction* func, const QoreTypeInfo* pseudoTypeInfo, const QoreTypeInfo*& returnTypeInfo);

#define NODE_ARRAY_LEN (NT_NUMBER + 1)

#endif
