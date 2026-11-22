/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreFuture.h

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

#ifndef _QORE_INTERN_QOREFUTURE_H
#define _QORE_INTERN_QOREFUTURE_H

#include <qore/Qore.h>
#include <qore/QoreCondition.h>
#include <qore/QoreFuture.h>

#include <string>

//! Shared implementation for Future and Promise (internal only)
class qore_future_private : public QoreReferenceCounter {
public:
    enum State {
        PENDING,
        RESOLVED,
        REJECTED,
    };

    DLLLOCAL qore_future_private() : state(PENDING), waiting(0), future_retrieved(false) {
    }

    DLLLOCAL ~qore_future_private() {
        assert(state != PENDING || !waiting);
        ExceptionSink xsink;
        result.discard(&xsink);
        err_arg.discard(&xsink);
    }

    //! Resolves the future with a value
    DLLLOCAL void set(QoreValue val, ExceptionSink* xsink) {
        AutoLocker al(&lock);
        if (state != PENDING) {
            val.discard(xsink);
            xsink->raiseException("PROMISE-ERROR", "Promise has already been %s",
                state == RESOLVED ? "resolved" : "rejected");
            return;
        }
        result = val;
        state = RESOLVED;
        cond.broadcast();
    }

    //! Rejects the future with an error
    DLLLOCAL void setError(const char* err, const char* desc, QoreValue arg, ExceptionSink* xsink);

    //! Rejects the future by assimilating exceptions from an ExceptionSink
    DLLLOCAL void setException(ExceptionSink& xs);

    //! Gets the result (blocking)
    /** Returns the result value; on error, raises exception and returns nothing
        @param timeout_ms timeout in milliseconds; 0 = infinite
        @param xsink exception sink
        @return the result value
    */
    DLLLOCAL QoreValue get(int64 timeout_ms, ExceptionSink* xsink);

    //! Non-blocking check if the future is done (resolved or rejected)
    DLLLOCAL bool isDone() const {
        AutoLocker al(&lock);
        return state != PENDING;
    }

    //! Non-blocking check if the future was rejected
    DLLLOCAL bool isError() const {
        AutoLocker al(&lock);
        return state == REJECTED;
    }

    //! Cancels the future if still pending
    /** @return true if cancelled, false if already resolved/rejected
    */
    DLLLOCAL bool cancel() {
        AutoLocker al(&lock);
        if (state != PENDING) {
            return false;
        }
        err_code = "FUTURE-CANCELLED";
        err_desc = "Future was cancelled";
        state = REJECTED;
        cond.broadcast();
        return true;
    }

    //! Called from Promise destructor: reject if still pending
    DLLLOCAL void promiseDestroyed(ExceptionSink* xsink) {
        AutoLocker al(&lock);
        if (state == PENDING) {
            err_code = "PROMISE-ERROR";
            err_desc = "Promise destroyed without setting a value";
            state = REJECTED;
            cond.broadcast();
        }
    }

    //! Called from Future destructor: wake waiting threads with error
    DLLLOCAL void futureDestroyed(ExceptionSink* xsink) {
        AutoLocker al(&lock);
        if (waiting > 0) {
            xsink->raiseException("FUTURE-ERROR",
                "Future deleted while %d thread%s blocked on it",
                waiting, waiting == 1 ? " is" : "s are");
            err_code = "FUTURE-ERROR";
            err_desc = "Future deleted while thread(s) blocked on it";
            state = REJECTED;
            cond.broadcast();
        }
    }

    //! Mark that the future object has been retrieved
    DLLLOCAL bool markFutureRetrieved() {
        AutoLocker al(&lock);
        if (future_retrieved) {
            return false;
        }
        future_retrieved = true;
        return true;
    }

    DLLLOCAL void ref() {
        ROreference();
    }

    DLLLOCAL void deref() {
        if (ROdereference()) {
            delete this;
        }
    }

    //! Get the internal private data from a QorePromise object
    DLLLOCAL static qore_future_private* getFromPromise(QorePromise* p);

private:
    mutable QoreThreadLock lock;
    QoreCondition cond;
    QoreValue result;
    std::string err_code;
    //! true if code that is sandboxed or acts for sandboxed code rejected the future: get() raises its error marked
    //! (qore#7139)
    bool err_sandboxed = false;
    //! the ID of the sandboxed Program of that code, or 0 if it is not known
    qore_program_id_t err_pgm_id = 0;
    std::string err_desc;
    QoreValue err_arg;
    State state;
    int waiting;
    bool future_retrieved;
};

#endif // _QORE_INTERN_QOREFUTURE_H
