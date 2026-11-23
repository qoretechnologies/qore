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

//! Calls the pseudo-method \a name of a value with arguments bound to its parameters by name, in the pseudo-class
//! that pseudo_classes_eval() calls it in (a weak reference or a reference gives its target)
DLLLOCAL QoreValue pseudo_classes_eval_named(const QoreValue n, const char* name, const QoreListNode* args,
    bool tmp_args, const qore_class_private* class_ctx, const name_vec_t& arg_names, LexicalSemanticPolicy policy,
    RuntimeConfig& rc, ExceptionSink* xsink);

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

//! Raises the exception of a pseudo-method of the class named \a class_name given a value of another type (qore#7363)
DLLLOCAL void qore_raise_pseudo_receiver_type_error(const QoreValue& v, const char* class_name, ExceptionSink* xsink);

//! Returns the node of the value that a pseudo-method of a class of node type \a t is called on (qore#7363)
/** The code of a pseudo-method takes the node for one of its class's type; a value of another type is never taken
    for it: the exception is raised and nullptr is returned, and the method returns with no value (see the receiver
    of a pseudo-class in qpp)
*/
template <typename T>
DLLLOCAL inline const T* qore_pseudo_receiver(const QoreValue& v, qore_type_t t, const char* class_name,
        ExceptionSink* xsink) {
    if (!v.isEnum() && !v.isShortString() && v.getType() == t) [[likely]] {
        return v.get<const T>();
    }
    qore_raise_pseudo_receiver_type_error(v, class_name, xsink);
    return nullptr;
}

//! Returns the character that a pseudo-method of the <char> class is called on (qore#7363)
/** For a value of another type, the exception is raised and the character 0 is returned, whose result the call does
    not give (the value's bits are never taken for a character)
*/
DLLLOCAL inline unsigned qore_pseudo_char_receiver(const QoreValue& v, ExceptionSink* xsink) {
    if (v.isChar()) [[likely]] {
        return v.getChar();
    }
    qore_raise_pseudo_receiver_type_error(v, "<char>", xsink);
    return 0;
}

//! Returns the call reference that a pseudo-method of the <callref> class (\a closure_only false) or of the <closure>
//! class (\a closure_only true) is called on (see qore_pseudo_receiver())
DLLLOCAL inline const ResolvedCallReferenceNode* qore_pseudo_callref_receiver(const QoreValue& v, bool closure_only,
        const char* class_name, ExceptionSink* xsink) {
    if (!v.isEnum()) [[likely]] {
        qore_type_t t = v.getType();
        if (t == NT_RUNTIME_CLOSURE || (!closure_only && t == NT_FUNCREF)) [[likely]] {
            return v.get<const ResolvedCallReferenceNode>();
        }
    }
    qore_raise_pseudo_receiver_type_error(v, class_name, xsink);
    return nullptr;
}
DLLLOCAL const QoreMethod* pseudo_classes_find_method(qore_type_t t, const char* mname, QoreClass*& qc);
DLLLOCAL const QoreMethod* pseudo_classes_find_method(const QoreTypeInfo* typeInfo, const char* mname, QoreClass*& qc, bool& possible_match);
DLLLOCAL void qore_pseudo_check_return_type_info(const QoreFunction* func, const QoreTypeInfo* pseudoTypeInfo, const QoreTypeInfo*& returnTypeInfo);

#define NODE_ARRAY_LEN (NT_NUMBER + 1)

#endif
