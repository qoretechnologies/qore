/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    VLock.h

    Qore Programming Language

    Copyright (C) 2003 - 2023 David Nichols

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

#ifndef _QORE_VLOCK_H

#define _QORE_VLOCK_H

#include <qore/Qore.h>
#include <qore/AbstractSmartLock.h>

#include <vector>
#include <map>
#include <atomic>

typedef std::map<int, class VLock*> vlock_map_t;

// testing shows that a vector is slightly faster than a deque for this usage
// and must faster than a list
typedef std::vector<AbstractSmartLock*> abstract_lock_list_t;

// forward references
class QoreCondition;

//! returned by VLock::condWait() when the wait ended because cancellation is to be delivered
#define QORE_VLOCK_WAIT_CANCELLED -1

// for tracking locks per thread and detecting deadlocks
class VLock : protected abstract_lock_list_t {
public:
    DLLLOCAL VLock(int tid);

    DLLLOCAL ~VLock() {
        //printd(5, "VLock::~VLock() this=%p\n", this);
        assert(begin() == end());
    }

    DLLLOCAL void push(AbstractSmartLock* g);
    DLLLOCAL int pop(AbstractSmartLock* asl);
    DLLLOCAL void del();
    DLLLOCAL AbstractSmartLock* find(AbstractSmartLock* g) const;

    // for blocking smart locks with deadlock detection
    DLLLOCAL int waitOn(AbstractSmartLock* asl, VLock* vl, ExceptionSink* xsink, int timeout_ms = 0);
    // for smart locks using an alternate condition variable
    DLLLOCAL int waitOn(AbstractSmartLock* asl, QoreCondition* cond, VLock* vl, ExceptionSink* xsink, int timeout_ms = 0);
    // for smart locks that can be held by more than one thread
    DLLLOCAL int waitOn(AbstractSmartLock* asl, vlock_map_t& vmap, ExceptionSink* xsink, int timeout_ms = 0);
    DLLLOCAL int getTID() const { return tid; }

    //! Waits on a condition variable with the given lock's internal mutex held
    /** If the thread's lock waits are cancellation points (see VLockCancellationPoint), a cancellation request or
        program interrupt wakes the wait at once (see QoreCondition::waitWithInterrupt()); otherwise this is a
        plain wait.

        @param cond the condition to wait on
        @param asl the lock whose internal mutex (\c asl_lock) is held by the caller
        @param timeout_ms the timeout in milliseconds; 0 means no timeout

        @return 0 if woken (the caller rechecks its state), \c ETIMEDOUT on timeout, or
        @ref QORE_VLOCK_WAIT_CANCELLED if cancellation is to be delivered; no exception is raised here, so that a
        caller that must first restore its state (a condition wait reacquiring its lock) can raise it afterwards
        with raiseCancel()
    */
    DLLLOCAL int condWait(QoreCondition& cond, AbstractSmartLock* asl, int64 timeout_ms);

    //! Raises the pending cancellation exception for the operation of the current cancellation point
    /** @return true if an exception was raised; false if no request is to be delivered any more (for example
        it was out of the thread's scope and has been dropped)
    */
    DLLLOCAL bool raiseCancel(ExceptionSink* xsink) const {
        return cancel_op && qore_check_cancel(xsink, cancel_op);
    }

    //! Returns the operation named in cancellation exceptions while lock waits are cancellation points, else nullptr
    DLLLOCAL const char* getCancelOperation() const {
        return cancel_op;
    }

    //! Sets the operation named in cancellation exceptions, making lock waits cancellation points; nullptr ends it
    DLLLOCAL void setCancelOperation(const char* op) {
        cancel_op = op;
    }

#ifdef DEBUG
    DLLLOCAL void show(class VLock* nvl) const;
#endif

private:
    std::atomic<AbstractSmartLock*> waiting_on;   // the lock this object is waiting on
    int tid;
    //! while set, lock waits on this thread are cancellation points that name this operation; only the owning
    //! thread reads or writes it
    const char* cancel_op = nullptr;

    //! waits for a lock: the shared part of the waitOn() variants
    DLLLOCAL int waitIntern(QoreCondition& cond, AbstractSmartLock* asl, ExceptionSink* xsink, int timeout_ms);

    // not implemented
    VLock(const VLock&);
    VLock& operator=(const VLock&);
};

//! Makes lock waits on the current thread cancellation points for the lifetime of the object
/** The Qore-level lock APIs (Mutex::lock(), RWLock::readLock(), Gate::enter(), Condition::wait(), ...) are
    cancellation points; the same smart locks are also used internally (for example for synchronized methods), where
    a wait must not end with a cancellation exception, so a wait only becomes a cancellation point inside the
    scope of this helper.  A cancellation request or program interrupt then wakes the wait at once.
*/
class VLockCancellationPoint {
public:
    DLLLOCAL VLockCancellationPoint(VLock* vl, const char* operation) : vl(vl), old_op(vl->getCancelOperation()) {
        assert(operation);
        vl->setCancelOperation(operation);
    }

    DLLLOCAL ~VLockCancellationPoint() {
        vl->setCancelOperation(old_op);
    }

private:
    VLock* vl;
    const char* old_op;

    VLockCancellationPoint(const VLockCancellationPoint&) = delete;
    VLockCancellationPoint& operator=(const VLockCancellationPoint&) = delete;
};

//! Suspends a VLockCancellationPoint for the lifetime of the object
/** Used where a wait must complete regardless of cancellation, such as a condition wait reacquiring its lock
    before it reports the cancellation.
*/
class VLockCancellationSuspend {
public:
    DLLLOCAL VLockCancellationSuspend(VLock* vl) : vl(vl), old_op(vl->getCancelOperation()) {
        vl->setCancelOperation(nullptr);
    }

    DLLLOCAL ~VLockCancellationSuspend() {
        vl->setCancelOperation(old_op);
    }

private:
    VLock* vl;
    const char* old_op;

    VLockCancellationSuspend(const VLockCancellationSuspend&) = delete;
    VLockCancellationSuspend& operator=(const VLockCancellationSuspend&) = delete;
};

#endif
