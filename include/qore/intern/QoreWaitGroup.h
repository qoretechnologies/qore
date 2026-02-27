/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreWaitGroup.h

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

#ifndef _QORE_INTERN_QOREWAITGROUP_H
#define _QORE_INTERN_QOREWAITGROUP_H

#include <qore/Qore.h>
#include <qore/QoreCondition.h>
#include <qore/AbstractPrivateData.h>

//! A WaitGroup waits for a collection of tasks to finish
class QoreWaitGroup : public AbstractPrivateData {
public:
    DLLLOCAL QoreWaitGroup() : count(0), waiting(0) {
    }

    //! Increments the counter by n
    DLLLOCAL void add(int n, ExceptionSink* xsink) {
        AutoLocker al(&lock);
        if (n < 1) {
            xsink->raiseException("WAITGROUP-ERROR", "WaitGroup::add() called with invalid value %d; must be >= 1", n);
            return;
        }
        if (count == 0 && waiting > 0) {
            xsink->raiseException("WAITGROUP-ERROR", "WaitGroup::add() called after count reached 0 with waiting threads");
            return;
        }
        count += n;
    }

    //! Decrements the counter by 1
    DLLLOCAL void done(ExceptionSink* xsink) {
        AutoLocker al(&lock);
        if (count <= 0) {
            xsink->raiseException("WAITGROUP-ERROR", "WaitGroup::done() called when count is already 0");
            return;
        }
        --count;
        if (count == 0) {
            cond.broadcast();
        }
    }

    //! Blocks until the counter reaches 0
    /** @param timeout_ms timeout in milliseconds; 0 = infinite
        @return 0 on success, -1 on timeout
    */
    DLLLOCAL int wait(int64 timeout_ms, ExceptionSink* xsink) {
        // Convention: timeout_ms <= 0 means infinite; waitWithInterrupt uses -1 for infinite
        int64 cond_timeout = (timeout_ms <= 0) ? -1 : timeout_ms;

        AutoLocker al(&lock);
        ++waiting;
        while (count > 0) {
            int rc = cond.waitWithInterrupt(&lock, cond_timeout, xsink);
            if (rc == QORE_COND_RESULT_INTERRUPTED) {
                --waiting;
                return -1;
            }
            if (count <= 0) {
                break;
            }
            if (rc == QORE_COND_RESULT_TIMEOUT) {
                --waiting;
                return -1;
            }
        }
        --waiting;
        return 0;
    }

    //! Returns the current count
    DLLLOCAL int getCount() const {
        AutoLocker al(&lock);
        return count;
    }

    //! Called from destructor
    DLLLOCAL void destructor(ExceptionSink* xsink) {
        AutoLocker al(&lock);
        if (waiting > 0) {
            xsink->raiseException("WAITGROUP-ERROR",
                "WaitGroup deleted while %d thread%s blocked on it",
                waiting, waiting == 1 ? " is" : "s are");
            cond.broadcast();
        }
    }

protected:
    DLLLOCAL virtual ~QoreWaitGroup() {}

private:
    mutable QoreThreadLock lock;
    QoreCondition cond;
    int count;
    int waiting;
};

#endif // _QORE_INTERN_QOREWAITGROUP_H
