/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreValueExceptionOps.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_INTERN_QOREVALUEEXCEPTIONOPS_H
#define _QORE_INTERN_QOREVALUEEXCEPTIONOPS_H

#include <qore/QoreValue.h>
#include <qore/ExceptionSink.h>

#include <utility>

/** The non-virtual DLLLOCAL member functions of ExceptionSink, QoreValue and ValueEvalRefHolder were declared in the public headers although no module can call them (they are
    not exported); they are members of the classes below, in the implementation of the library.  Each class is a friend
    of the class it serves; call an operation through a temporary object:

    @code
    qore_exception_sink_ops(xsink).getException();
    @endcode

    A function that stays out of line is a static fooImpl(Cls* b, ...) with an inline wrapper, so that the temporary is
    optimized away and the call costs what the member function cost (a temporary whose address a call takes makes GCC
    add a stack-protector canary to the caller).
*/

//! The library-internal operations of ExceptionSink (qore#4380)
class qore_exception_sink_ops {
public:
    //! the object
    ExceptionSink* b;

    DLLLOCAL explicit qore_exception_sink_ops(ExceptionSink* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_exception_sink_ops(H& holder) : qore_exception_sink_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_exception_sink_ops(const ExceptionSink* n_b) : b(const_cast<ExceptionSink*>(n_b)) {
    }

    DLLLOCAL explicit qore_exception_sink_ops(ExceptionSink& r) : b(&r) {
    }

    DLLLOCAL explicit qore_exception_sink_ops(const ExceptionSink& r) : b(const_cast<ExceptionSink*>(&r)) {
    }

    DLLLOCAL static QoreException* catchExceptionImpl(ExceptionSink* b);
    DLLLOCAL QoreException* catchException() {
        return catchExceptionImpl(b);
    }

    DLLLOCAL static QoreException* getExceptionImpl(ExceptionSink* b);
    DLLLOCAL QoreException* getException() {
        return getExceptionImpl(b);
    }

    DLLLOCAL static void defaultExceptionHandler(QoreException* e);

    DLLLOCAL static void defaultWarningHandler(QoreException* e);

    //! appends the location of an exception to the text of its report
    DLLLOCAL static void outputExceptionLocation(QoreString& out, const char* fns, int start_line, int end_line, const char* srcs, int offset, const char* langs, const char* types);

    DLLLOCAL static void overrideLocationImpl(ExceptionSink* b, const QoreProgramLocation& loc);
    DLLLOCAL void overrideLocation(const QoreProgramLocation& loc) {
        overrideLocationImpl(b, loc);
    }

    DLLLOCAL static void raiseExceptionImpl(ExceptionSink* b, QoreException* e);
    DLLLOCAL void raiseException(QoreException* e) {
        raiseExceptionImpl(b, e);
    }

    DLLLOCAL static void raiseExceptionImpl(ExceptionSink* b, const QoreListNode* n);
    DLLLOCAL void raiseException(const QoreListNode* n) {
        raiseExceptionImpl(b, n);
    }

    DLLLOCAL static void setLastLocationImpl(ExceptionSink* b, const QoreProgramLocation& loc);
    DLLLOCAL void setLastLocation(const QoreProgramLocation& loc) {
        setLastLocationImpl(b, loc);
    }

};

//! The library-internal operations of QoreValue (qore#4380)
class qore_value_ops {
public:
    //! the object
    QoreValue* b;

    DLLLOCAL explicit qore_value_ops(QoreValue* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_value_ops(H& holder) : qore_value_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_value_ops(const QoreValue* n_b) : b(const_cast<QoreValue*>(n_b)) {
    }

    DLLLOCAL explicit qore_value_ops(QoreValue& r) : b(&r) {
    }

    DLLLOCAL explicit qore_value_ops(const QoreValue& r) : b(const_cast<QoreValue*>(&r)) {
    }

    //! Evaluates the value with RuntimeConfig and returns the result with deref flag
    DLLLOCAL static QoreValue evalImpl(QoreValue* b, RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink);
    DLLLOCAL QoreValue eval(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) {
        return evalImpl(b, rc, needs_deref, xsink);
    }

    //! Hard comparison; an exception raised comparing nodes is discarded if xsink is nullptr (internal use only)
    DLLLOCAL static bool isEqualHardInternImpl(QoreValue* b, const QoreValue& other, ExceptionSink* xsink);
    DLLLOCAL bool isEqualHardIntern(const QoreValue& other, ExceptionSink* xsink) {
        return isEqualHardInternImpl(b, other, xsink);
    }

    //! Encodes a node as an opaque reference of the given kind, taking a reference to the node
    DLLLOCAL static QoreValue makeOpaqueIntern(AbstractQoreNode* n, uint64_t opaque_tag);

    //! Records that one strong reference held through this opaque value has been released
    DLLLOCAL static void opaqueDeregisterImpl(QoreValue* b);
    DLLLOCAL void opaqueDeregister() {
        opaqueDeregisterImpl(b);
    }

    //! Records one more strong reference held through this opaque value with its Program
    DLLLOCAL static void opaqueRegisterImpl(QoreValue* b);
    DLLLOCAL void opaqueRegister() {
        opaqueRegisterImpl(b);
    }

};

//! The library-internal operations of ValueEvalRefHolder (qore#4380)
class qore_value_eval_ref_holder_ops {
public:
    //! the object
    ValueEvalRefHolder* b;

    DLLLOCAL explicit qore_value_eval_ref_holder_ops(ValueEvalRefHolder* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_value_eval_ref_holder_ops(H& holder) : qore_value_eval_ref_holder_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_value_eval_ref_holder_ops(const ValueEvalRefHolder* n_b) : b(const_cast<ValueEvalRefHolder*>(n_b)) {
    }

    DLLLOCAL explicit qore_value_eval_ref_holder_ops(ValueEvalRefHolder& r) : b(&r) {
    }

    DLLLOCAL explicit qore_value_eval_ref_holder_ops(const ValueEvalRefHolder& r) : b(const_cast<ValueEvalRefHolder*>(&r)) {
    }

    //! evaluates the argument with RuntimeConfig, returns -1 for error, 0 = OK
    DLLLOCAL static int evalImpl(ValueEvalRefHolder* b, RuntimeConfig& rc, const AbstractQoreNode* exp);
    DLLLOCAL int eval(RuntimeConfig& rc, const AbstractQoreNode* exp) {
        return evalImpl(b, rc, exp);
    }

    //! evaluates the argument with RuntimeConfig, returns -1 for error, 0 = OK
    DLLLOCAL static int evalImpl(ValueEvalRefHolder* b, RuntimeConfig& rc, const QoreValue exp);
    DLLLOCAL int eval(RuntimeConfig& rc, const QoreValue exp) {
        return evalImpl(b, rc, exp);
    }

    //! evaluates the argument with RuntimeConfig, returns -1 for error, 0 = OK
    DLLLOCAL static int evalInternImpl(ValueEvalRefHolder* b, RuntimeConfig& rc, const AbstractQoreNode* exp);
    DLLLOCAL int evalIntern(RuntimeConfig& rc, const AbstractQoreNode* exp) {
        return evalInternImpl(b, rc, exp);
    }

    //! evaluates the argument with RuntimeConfig, returns -1 for error, 0 = OK
    DLLLOCAL static int evalInternImpl(ValueEvalRefHolder* b, RuntimeConfig& rc, const QoreValue& exp);
    DLLLOCAL int evalIntern(RuntimeConfig& rc, const QoreValue& exp) {
        return evalInternImpl(b, rc, exp);
    }

};

#endif // _QORE_INTERN_QOREVALUEEXCEPTIONOPS_H
