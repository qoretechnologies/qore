/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_ThreadCapabilityScope.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_CLASS_THREADCAPABILITYSCOPE_H
#define _QORE_CLASS_THREADCAPABILITYSCOPE_H

#include "qore/intern/qore_thread_intern.h"
#include "qore/intern/CallReferenceNode.h"
#include "qore/intern/QoreLibIntern.h"

DLLLOCAL extern qore_classid_t CID_THREADCAPABILITYSCOPE;
DLLLOCAL extern QoreClass* QC_THREADCAPABILITYSCOPE;
DLLLOCAL QoreClass* initThreadCapabilityScopeClass(QoreNamespace& ns);

//! The private data of a ThreadCapabilityScope object: the scope it entered (see design/thread-capability-scope.md)
class QoreThreadCapabilityScope : public AbstractPrivateData {
public:
    DLLLOCAL QoreThreadCapabilityScope(size_t level, uint64_t gen) : level(level), gen(gen) {
    }

    //! leaves the scope if it is still open in the current thread
    DLLLOCAL void leave(ExceptionSink* xsink) {
        thread_pop_capability_scope(level, gen, xsink);
    }

private:
    //! the level of the scope on its thread's stack
    size_t level;
    //! the identity of the scope, unique in the process
    uint64_t gen;
};

//! Returns true if the code calling a builtin may see the current thread's capability set
DLLLOCAL bool thread_capabilities_visible();

//! Checks that code may be run by a capability (qorus#867, design/thread-capability-scope.md)
/** The code must be of a Program that may control threads and that no sandbox governs: a capability's code, and the
    code that a capability's guard passes to it.  Code without a Program of its own (a call reference to builtin
    code) is refused, as it would run with no restrictions at all.

    @param v the value to check; a value that is not code is refused
    @param name the name of the capability, for the exception
    @param what what the code is, for the exception (ex: "the code")

    @return 0 if the code may run, -1 if an exception was raised in \a xsink
*/
DLLLOCAL int thread_capability_check_code(const QoreValue v, const char* name, const char* what,
        ExceptionSink* xsink);

//! Checks that the calling code may set the capabilities of a thread: no sandbox governs it (qorus#867)
/** Decided by the calling code (its Program and its callers on the thread, a continued operation's sandbox, the
    provenance of the callbacks it runs in), not by the sandbox that the thread inherited from the code that started
    it alone (see qore_scope_creator_is_sandboxed())

    @return 0 if it may, -1 if an exception was raised in \a xsink
*/
DLLLOCAL int thread_capability_check_creator(ExceptionSink* xsink);

//! The guard of a thread capability: the call reference that get_thread_capability() returns (qorus#867)
/** A call passes the arguments to the capability's code once each argument that is code has been checked
    (thread_capability_check_code()): the capability runs only code that could control threads itself, whoever holds
    the guard.  The capability's code is held here, where no Qore code can reach it.
*/
class ThreadCapabilityGuardReferenceNode : public ResolvedCallReferenceNodeIntern {
public:
    DLLLOCAL ThreadCapabilityGuardReferenceNode(const char* name, ResolvedCallReferenceNode* code)
            : ResolvedCallReferenceNodeIntern(&loc_builtin), name(name), code(code) {
    }

    DLLLOCAL QoreValue dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const override {
        // the arguments of a call that an interpreter makes without evaluating them first (a direct call of the
        // reference in the AST interpreter): they are evaluated here, so that the values are checked, not the
        // expressions that produce them
        ReferenceHolder<QoreListNode> evaluated(xsink);
        if (args && args->needs_eval()) {
            evaluated = args->evalList(xsink);
            if (*xsink) {
                return QoreValue();
            }
            args = *evaluated;
        }
        // only code, which is checked, and plain values are passed: a reference, a container or an object could carry
        // code that the capability would run unchecked; a call with no arguments has no list
        if (args) {
            ConstListIterator li(args);
            while (li.next()) {
                QoreValue v = li.getValue();
                switch (v.getType()) {
                    case NT_RUNTIME_CLOSURE:
                    case NT_FUNCREF:
                        if (thread_capability_check_code(v, name.c_str(), "the code given to it", xsink)) {
                            return QoreValue();
                        }
                        break;
                    case NT_NOTHING:
                    case NT_NULL:
                    case NT_INT:
                    case NT_FLOAT:
                    case NT_NUMBER:
                    case NT_BOOLEAN:
                    case NT_STRING:
                    case NT_DATE:
                    case NT_BINARY:
                        break;
                    default:
                        xsink->raiseException("THREAD-CAPABILITY-ERROR", "capability '%s': argument %d is of type %s; "
                            "only code and plain values can be given to a capability", name.c_str(),
                            static_cast<int>(li.index()) + 1, v.getFullTypeName());
                        return QoreValue();
                }
            }
        }
        return code->invoke(args, xsink);
    }

    DLLLOCAL bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const override {
        return is_equal_hard(v, xsink);
    }

    DLLLOCAL bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const override {
        return v == this;
    }

    DLLLOCAL QoreFunction* getFunction() const override {
        return nullptr;
    }

protected:
    DLLLOCAL bool derefImpl(ExceptionSink* xsink) override {
        code->deref(xsink);
        code = nullptr;
        return ResolvedCallReferenceNodeIntern::derefImpl(xsink);
    }

private:
    //! the name of the capability
    std::string name;
    //! the capability's code (a reference)
    ResolvedCallReferenceNode* code;
};

//! Returns a new guard of the capability \a code: the call reference that get_thread_capability() returns
/** A call of the guard passes its arguments to \a code once each argument that is code has been checked with
    thread_capability_check_code()
*/
DLLLOCAL ResolvedCallReferenceNode* thread_capability_make_guard(const char* name, const QoreValue code);

#endif
