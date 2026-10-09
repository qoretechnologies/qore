/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    BackquoteNode.h

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

#ifndef _QORE_BACKQUOTENODE_H

#define _QORE_BACKQUOTENODE_H

class BackquoteNode : public ParseNode {
public:
    char* str;

    DLLLOCAL BackquoteNode(const QoreProgramLocation* loc, char* c_str);

    DLLLOCAL virtual ~BackquoteNode();

    // get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
    // the ExceptionSink is only needed for QoreObject where a method may be executed
    // use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
    // returns -1 for exception raised, 0 = OK
    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;
    // if del is true, then the returned QoreString * should be deleted, if false, then it must not be
    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const;

protected:
    //! optionally evaluates the argument
    /** return value requires a deref(xsink) if needs_deref is true
        @see AbstractQoreNode::eval()
    */
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        parse_context.typeInfo = stringTypeInfo;
        return 0;
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return stringTypeInfo;
    }
};

DLLLOCAL QoreStringNode *backquoteEval(const char* cmd, int& rc, ExceptionSink* xsink);

#ifndef _Q_WINDOWS
#include <sys/types.h>

//! the result of qore_wait_child_process()
enum class QoreChildWaitResult {
    EXITED,         //!< the process ended; its wait status is available
    WAIT_FAILED,    //!< waitpid() failed with the errno value given; no exception is raised
    RAISED,         //!< an exception was raised: the wait was cancelled or could not be made; the process is reaped
};

//! waits for a child process to end; the wait can be cancelled
/** The calling thread blocks until the process ends or the thread is cancelled, or its Program is interrupted by its
    SandboxManager; there is no polling: a helper thread blocks in waitpid() and wakes the caller, and cancellation
    wakes the caller through its interruptible condition wait.

    When the wait is cancelled, or the helper thread cannot be started, \a kill_target (the process, or its process
    group as a negative number) is killed with SIGKILL, the process is reaped, and an exception is raised.

    @param pid the child process to wait for
    @param kill_target the argument for kill() to terminate the process: \a pid or the process group as -pgid
    @param status the wait status of the process if QoreChildWaitResult::EXITED is returned
    @param wait_errno the errno value of waitpid() if QoreChildWaitResult::WAIT_FAILED is returned
    @param err the exception code raised if the helper thread cannot be started
    @param xsink for the exception raised

    @return the result of the wait
*/
DLLLOCAL QoreChildWaitResult qore_wait_child_process(pid_t pid, pid_t kill_target, int& status, int& wait_errno,
        const char* err, ExceptionSink* xsink);

//! reaps a child process, blocking until it ends; waitpid() is retried when interrupted by a signal
/** @return 0 if the process was reaped and \a status is set, -1 if waitpid() failed (errno is set)
*/
DLLLOCAL int qore_reap_child_process(pid_t pid, int& status);
#endif

#endif
