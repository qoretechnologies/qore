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
    DLLLOCAL void setError(const char* err, const char* desc, QoreValue arg, ExceptionSink* xsink) {
        AutoLocker al(&lock);
        if (state != PENDING) {
            arg.discard(xsink);
            xsink->raiseException("PROMISE-ERROR", "Promise has already been %s",
                state == RESOLVED ? "resolved" : "rejected");
            return;
        }
        err_code = err;
        err_desc = desc;
        err_arg = arg;
        state = REJECTED;
        cond.broadcast();
    }

    //! Rejects the future by assimilating exceptions from an ExceptionSink
    DLLLOCAL void setException(ExceptionSink& xs) {
        AutoLocker al(&lock);
        if (state != PENDING) {
            xs.clear();
            return;
        }
        // Store the exception info from the sink
        QoreHashNode* ex = xs.getExceptionInfo();
        if (ex) {
            QoreValue errv = ex->getKeyValue("err");
            QoreValue descv = ex->getKeyValue("desc");
            QoreValue argv = ex->getKeyValue("arg");
            if (errv.getType() == NT_STRING) {
                QoreStringValueHelper err(errv);
                err_code = err->c_str();
            }
            if (descv.getType() == NT_STRING) {
                QoreStringValueHelper desc(descv);
                err_desc = desc->c_str();
            }
            err_arg = argv.refSelf();
            ex->deref(nullptr);
        }
        xs.clear();
        state = REJECTED;
        cond.broadcast();
    }

    //! Gets the result (blocking)
    /** Returns the result value; on error, raises exception and returns nothing
        @param timeout_ms timeout in milliseconds; 0 = infinite
        @param xsink exception sink
        @return the result value
    */
    DLLLOCAL QoreValue get(int64 timeout_ms, ExceptionSink* xsink) {
        // The timeout is a wall-clock deadline, not a per-wait window.
        //
        // Earlier versions passed `timeout_ms` directly to waitWithInterrupt
        // on every loop iteration.  If the cond fired (signal OR pthread
        // spurious wake) but state was still PENDING, the loop re-entered
        // the wait with the FULL ORIGINAL timeout — restarting the
        // countdown.  Empirically this caused HTTPCLIENT-TIMEOUT failures
        // observed in Qorus issue 1704 (120s request_timeout failed to
        // fire; thread parked in FutureImpl::get() for 30+ minutes against
        // slow-streaming LLM endpoints).  Compute an absolute deadline in
        // microseconds and recompute `remaining` per iteration so the
        // timeout fires at the intended wall-clock time regardless of
        // spurious wakes.  Mirrors the deadline pattern in
        // include/qore/AbstractHttpPollConnection.h:109-111.
        const bool has_deadline = (timeout_ms > 0);
        int64 deadline_us = 0;
        if (has_deadline) {
            int us;
            deadline_us = q_epoch_us(us) * 1000000LL + us + timeout_ms * 1000LL;
        }

        AutoLocker al(&lock);
        ++waiting;
        while (state == PENDING) {
            int64 cond_timeout_ms;
            if (has_deadline) {
                int us;
                int64 now_us = q_epoch_us(us) * 1000000LL + us;
                int64 remaining_us = deadline_us - now_us;
                if (remaining_us <= 0) {
                    --waiting;
                    xsink->raiseException("FUTURE-TIMEOUT",
                        "timed out after " QLLD "ms waiting for future result",
                        timeout_ms);
                    return QoreValue();
                }
                // Round up sub-millisecond remainder so we never pass 0
                // (which waitWithInterrupt treats as poll-don't-wait at
                // some sites; here ms < 0 means infinite, but 0 would be
                // a no-wait that consumes CPU until the deadline).
                cond_timeout_ms = (remaining_us + 999) / 1000;
            } else {
                cond_timeout_ms = -1;  // infinite
            }
            int rc = cond.waitWithInterrupt(&lock, cond_timeout_ms, xsink);
            if (rc == QORE_COND_RESULT_INTERRUPTED) {
                --waiting;
                return QoreValue();
            }
            if (state != PENDING) {
                break;
            }
            if (rc == QORE_COND_RESULT_TIMEOUT) {
                --waiting;
                xsink->raiseException("FUTURE-TIMEOUT",
                    "timed out after " QLLD "ms waiting for future result", timeout_ms);
                return QoreValue();
            }
            // Spurious wake or unrelated broadcast: the loop re-enters
            // waitWithInterrupt with a freshly-computed `remaining` so the
            // overall wait still respects the wall-clock deadline.
        }
        --waiting;

        if (state == REJECTED) {
            xsink->raiseExceptionArg(err_code.c_str(), err_arg.refSelf(), "%s", err_desc.c_str());
            return QoreValue();
        }

        assert(state == RESOLVED);
        return result.refSelf();
    }

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
    std::string err_desc;
    QoreValue err_arg;
    State state;
    int waiting;
    bool future_retrieved;
};

#endif // _QORE_INTERN_QOREFUTURE_H
