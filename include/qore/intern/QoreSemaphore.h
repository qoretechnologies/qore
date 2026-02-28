/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSemaphore.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_INTERN_QORESEMAPHORE_H
#define _QORE_INTERN_QORESEMAPHORE_H

#include <qore/Qore.h>
#include <qore/QoreCondition.h>
#include <qore/AbstractPrivateData.h>

//! A counting semaphore for limiting concurrent access to N permits
class QoreSemaphore : public AbstractPrivateData {
public:
    DLLLOCAL QoreSemaphore(int initial_permits) : permits(initial_permits), waiting(0), deleted(false) {
    }

    //! Acquires a permit, blocking until one is available
    /** @param timeout_ms timeout in milliseconds; 0 = infinite
        @param xsink exception sink
        @param timed_out set to true if the operation timed out (no exception raised for timeout)
        @return 0 on success, -1 on timeout or error
    */
    DLLLOCAL int acquire(int64 timeout_ms, ExceptionSink* xsink, bool& timed_out) {
        // Convention: timeout_ms <= 0 means infinite; waitWithInterrupt uses -1 for infinite
        int64 cond_timeout = (timeout_ms <= 0) ? -1 : timeout_ms;
        timed_out = false;

        AutoLocker al(&lock);
        ++waiting;
        while (permits <= 0 && !deleted) {
            int rc = cond.waitWithInterrupt(&lock, cond_timeout, xsink);
            if (rc == QORE_COND_RESULT_INTERRUPTED) {
                --waiting;
                return -1;
            }
            if (deleted) {
                --waiting;
                xsink->raiseException("SEMAPHORE-ERROR", "Semaphore has been deleted in another thread");
                return -1;
            }
            if (permits > 0) {
                break;
            }
            if (rc == QORE_COND_RESULT_TIMEOUT) {
                --waiting;
                timed_out = true;
                return -1;
            }
        }
        --waiting;

        if (deleted) {
            xsink->raiseException("SEMAPHORE-ERROR", "Semaphore has been deleted in another thread");
            return -1;
        }

        --permits;
        return 0;
    }

    //! Non-blocking acquire attempt
    DLLLOCAL bool tryAcquire() {
        AutoLocker al(&lock);
        if (permits > 0) {
            --permits;
            return true;
        }
        return false;
    }

    //! Releases n permits
    DLLLOCAL void release(int n, ExceptionSink* xsink) {
        if (n < 1) {
            xsink->raiseException("SEMAPHORE-ERROR", "Semaphore::release() called with invalid value %d; must be >= 1", n);
            return;
        }
        AutoLocker al(&lock);
        permits += n;
        if (n == 1) {
            cond.signal();
        } else {
            cond.broadcast();
        }
    }

    //! Returns the current number of available permits
    DLLLOCAL int availablePermits() const {
        AutoLocker al(&lock);
        return permits;
    }

    //! Returns the number of threads waiting to acquire
    DLLLOCAL int getWaiting() const {
        AutoLocker al(&lock);
        return waiting;
    }

    //! Called from destructor
    DLLLOCAL void destructor(ExceptionSink* xsink) {
        AutoLocker al(&lock);
        if (waiting > 0) {
            xsink->raiseException("SEMAPHORE-ERROR",
                "Semaphore deleted while %d thread%s blocked on it",
                waiting, waiting == 1 ? " is" : "s are");
        }
        deleted = true;
        cond.broadcast();
    }

    //! Returns the initial permit count for copy
    DLLLOCAL int getPermits() const {
        AutoLocker al(&lock);
        return permits;
    }

protected:
    DLLLOCAL virtual ~QoreSemaphore() {}

private:
    mutable QoreThreadLock lock;
    QoreCondition cond;
    int permits;
    int waiting;
    bool deleted;
};

#endif // _QORE_INTERN_QORESEMAPHORE_H
