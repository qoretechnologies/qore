/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSandboxPollOps.h

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

#ifndef _QORE_INTERN_QORESANDBOXPOLLOPS_H
#define _QORE_INTERN_QORESANDBOXPOLLOPS_H

#include <qore/PollPipeline.h>
#include <qore/QoreSandboxManager.h>

#include <utility>

/** The non-virtual DLLLOCAL member functions of PollPipelinePriv, QoreSandboxManager and QoreNetworkSecurityManager were declared in the public headers although no module can call them (they are
    not exported); they are members of the classes below, in the implementation of the library.  Each class is a friend
    of the class it serves; call an operation through a temporary object:

    @code
    qore_sandbox_manager_ops(mgr).releaseThreadSlot();
    @endcode

    A function that stays out of line is a static fooImpl(Cls* b, ...) with an inline wrapper, so that the temporary is
    optimized away and the call costs what the member function cost (a temporary whose address a call takes makes GCC
    add a stack-protector canary to the caller).
*/

//! The library-internal operations of PollPipelinePriv (qore#4380)
class qore_poll_pipeline_ops {
public:
    //! the object
    PollPipelinePriv* b;

    DLLLOCAL explicit qore_poll_pipeline_ops(PollPipelinePriv* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_poll_pipeline_ops(H& holder) : qore_poll_pipeline_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_poll_pipeline_ops(const PollPipelinePriv* n_b) : b(const_cast<PollPipelinePriv*>(n_b)) {
    }

    DLLLOCAL explicit qore_poll_pipeline_ops(PollPipelinePriv& r) : b(&r) {
    }

    DLLLOCAL explicit qore_poll_pipeline_ops(const PollPipelinePriv& r) : b(const_cast<PollPipelinePriv*>(&r)) {
    }

    //! Build result hash from context
    DLLLOCAL static QoreHashNode* buildResultHashImpl(PollPipelinePriv* b, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* buildResultHash(ExceptionSink* xsink) {
        return buildResultHashImpl(b, xsink);
    }

    //! Create the inner poll operation for a step
    DLLLOCAL static SocketPollOperationBase* createInnerOpImpl(PollPipelinePriv* b, const PollPipelinePriv::Step& step, ExceptionSink* xsink);
    DLLLOCAL SocketPollOperationBase* createInnerOp(const PollPipelinePriv::Step& step, ExceptionSink* xsink) {
        return createInnerOpImpl(b, step, xsink);
    }

    //! Arms a CRLF-delimited line read as the current inner op (RECV_IMAP_RESPONSE)
    /** @return 0 on success, -1 on error (an exception has been raised)
    */
    DLLLOCAL static int armImapLineReadImpl(PollPipelinePriv* b, ExceptionSink* xsink);
    DLLLOCAL int armImapLineRead(ExceptionSink* xsink) {
        return armImapLineReadImpl(b, xsink);
    }

    //! Arms a fixed-size literal read as the current inner op (RECV_IMAP_RESPONSE)
    /** @return 0 on success, -1 on error (an exception has been raised)
    */
    DLLLOCAL static int armImapLiteralReadImpl(PollPipelinePriv* b, size_t count, ExceptionSink* xsink);
    DLLLOCAL int armImapLiteralRead(size_t count, ExceptionSink* xsink) {
        return armImapLiteralReadImpl(b, count, xsink);
    }

    //! Handles completion of an inner op for a RECV_IMAP_RESPONSE step
    /** Appends the chunk just read to ctx.imap_accum, then either arms the next inner
        read (line or literal) or completes the step.

        @return 1 if the step is complete and execution should advance, 0 if a new inner
        op was armed and the continuePoll loop should iterate, -1 on error (an exception
        has been raised)
    */
    DLLLOCAL static int handleImapResponseImpl(PollPipelinePriv* b, ExceptionSink* xsink);
    DLLLOCAL int handleImapResponse(ExceptionSink* xsink) {
        return handleImapResponseImpl(b, xsink);
    }

    //! Deliver result via action
    DLLLOCAL static void deliverResultImpl(PollPipelinePriv* b, ExceptionSink* xsink);
    DLLLOCAL void deliverResult(ExceptionSink* xsink) {
        deliverResultImpl(b, xsink);
    }

    //! Extract output from current_op into context
    DLLLOCAL static void extractOutputImpl(PollPipelinePriv* b, ExceptionSink* xsink);
    DLLLOCAL void extractOutput(ExceptionSink* xsink) {
        extractOutputImpl(b, xsink);
    }

    //! Release current inner operation
    DLLLOCAL static void releaseCurrentOpImpl(PollPipelinePriv* b, ExceptionSink* xsink);
    DLLLOCAL void releaseCurrentOp(ExceptionSink* xsink) {
        releaseCurrentOpImpl(b, xsink);
    }

    //! Set error state
    DLLLOCAL static void setErrorImpl(PollPipelinePriv* b, const char* err, const char* desc, ExceptionSink* xsink);
    DLLLOCAL void setError(const char* err, const char* desc, ExceptionSink* xsink) {
        setErrorImpl(b, err, desc, xsink);
    }

};

//! The library-internal operations of QoreSandboxManager (qore#4380)
class qore_sandbox_manager_ops {
public:
    //! the object
    QoreSandboxManager* b;

    DLLLOCAL explicit qore_sandbox_manager_ops(QoreSandboxManager* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_sandbox_manager_ops(H& holder) : qore_sandbox_manager_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_sandbox_manager_ops(const QoreSandboxManager* n_b) : b(const_cast<QoreSandboxManager*>(n_b)) {
    }

    DLLLOCAL explicit qore_sandbox_manager_ops(QoreSandboxManager& r) : b(&r) {
    }

    DLLLOCAL explicit qore_sandbox_manager_ops(const QoreSandboxManager& r) : b(const_cast<QoreSandboxManager*>(&r)) {
    }

    //! Counts a new thread governed by this sandbox; raises THREAD-CREATION-FAILURE at the thread limit
    /** @return 0 if the thread is counted, -1 if the limit has been reached (an exception is raised)
    */
    DLLLOCAL static int acquireThreadSlotImpl(QoreSandboxManager* b, ExceptionSink* xsink);
    DLLLOCAL int acquireThreadSlot(ExceptionSink* xsink) {
        return acquireThreadSlotImpl(b, xsink);
    }

    //! Counts a new thread like acquireThreadSlot() without waking the thread pools waiting for a slot
    /** For a ThreadPool taking a slot with its lock held.  A composition that cannot take a slot of every member
        releases the slots it took again; \a released is then set to true, and the caller must call
        notifyThreadSlotReleased() once it has released its lock.
    */
    DLLLOCAL static int acquireThreadSlotQuietImpl(QoreSandboxManager* b, ExceptionSink* xsink, bool& released);
    DLLLOCAL int acquireThreadSlotQuiet(ExceptionSink* xsink, bool& released) {
        return acquireThreadSlotQuietImpl(b, xsink, released);
    }

    //! Wakes the thread pools waiting for a slot of this manager or of a member of this composition
    /** Must not be called with the lock of a ThreadPool held

        @param except a pool not to wake: the pool that released slots itself, which retries only when another
        thread released one
    */
    DLLLOCAL static void notifyThreadSlotReleasedImpl(QoreSandboxManager* b, const AbstractPrivateData* except);
    DLLLOCAL void notifyThreadSlotReleased(const AbstractPrivateData* except = nullptr) {
        notifyThreadSlotReleasedImpl(b, except);
    }

    //! Stops counting a thread counted with acquireThreadSlot() and wakes the thread pools waiting for a slot
    /** Must not be called with the lock of a ThreadPool held; see releaseThreadSlotQuiet()
    */
    DLLLOCAL static void releaseThreadSlotImpl(QoreSandboxManager* b);
    DLLLOCAL void releaseThreadSlot() {
        releaseThreadSlotImpl(b);
    }

    //! Stops counting a thread counted with acquireThreadSlot() without waking the thread pools waiting for a slot
    /** For a ThreadPool releasing a slot with its lock held, which notifies the waiting pools once it has released
        its lock
    */
    DLLLOCAL static void releaseThreadSlotQuietImpl(QoreSandboxManager* b);
    DLLLOCAL void releaseThreadSlotQuiet() {
        releaseThreadSlotQuietImpl(b);
    }

};

//! The library-internal operations of QoreNetworkSecurityManager (qore#4380)
class qore_network_security_manager_ops {
public:
    //! the object
    QoreNetworkSecurityManager* b;

    DLLLOCAL explicit qore_network_security_manager_ops(QoreNetworkSecurityManager* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_network_security_manager_ops(H& holder) : qore_network_security_manager_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_network_security_manager_ops(const QoreNetworkSecurityManager* n_b) : b(const_cast<QoreNetworkSecurityManager*>(n_b)) {
    }

    DLLLOCAL explicit qore_network_security_manager_ops(QoreNetworkSecurityManager& r) : b(&r) {
    }

    DLLLOCAL explicit qore_network_security_manager_ops(const QoreNetworkSecurityManager& r) : b(const_cast<QoreNetworkSecurityManager*>(&r)) {
    }

    //! Returns true if a connection to the port with the protocol is allowed by the port rules
    /** True also when no port rule is set, which allows every port
    */
    DLLLOCAL static bool allowsPortImpl(QoreNetworkSecurityManager* b, int port, int proto);
    DLLLOCAL bool allowsPort(int port, int proto) {
        return allowsPortImpl(b, port, proto);
    }

};

#endif // _QORE_INTERN_QORESANDBOXPOLLOPS_H
