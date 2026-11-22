/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreFuture.cpp

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

#include <qore/Qore.h>
#include "qore/intern/QoreFuture.h"
#include "qore/intern/qore_thread_intern.h"

// --- QoreFuture ---

QoreFuture::QoreFuture(qore_future_private* p) : priv(p) {
    priv->ref();
}

QoreFuture::~QoreFuture() {
}

void qore_future_private::setError(const char* err, const char* desc, QoreValue arg, ExceptionSink* xsink) {
    // whether code that is sandboxed or acts for sandboxed code chooses the text (qore#7139); not under the lock
    qore_program_id_t n_err_pgm_id = 0;
    const bool n_err_sandboxed = qore_caller_is_sandboxed(&n_err_pgm_id);
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
    err_sandboxed = n_err_sandboxed;
    err_pgm_id = n_err_pgm_id;
    state = REJECTED;
    cond.broadcast();
}

void qore_future_private::setException(ExceptionSink& xs) {
    AutoLocker al(&lock);
    if (state != PENDING) {
        xs.clear();
        return;
    }
    // the exception of code that is sandboxed or acts for sandboxed code is raised marked by get() (qore#7139)
    err_sandboxed = qore_exception_sandboxed_origin(xs, err_pgm_id);
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

QoreValue qore_future_private::get(int64 timeout_ms, ExceptionSink* xsink) {
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
    // timeout fires at the intended time regardless of spurious wakes.
    //
    // Use a monotonic clock for both the deadline and the per-iteration `now_us` below so the
    // timeout measures true elapsed (physical) time and cannot be shortened or extended by
    // wall-clock (CLOCK_REALTIME) steps from NTP / host time adjustments.  QoreCondition's timed
    // wait is likewise monotonic, so the relative cond_timeout_ms maps to the same physical
    // interval.  Mirrors the deadline pattern in
    // include/qore/AbstractHttpPollConnection.h:109-111.
    const bool has_deadline = (timeout_ms > 0);
    int64 deadline_us = 0;
    if (has_deadline) {
        deadline_us = q_clock_getmicros_monotonic() + timeout_ms * 1000LL;
    }

    AutoLocker al(&lock);
    ++waiting;
    while (state == PENDING) {
        int64 cond_timeout_ms;
        if (has_deadline) {
            int64 now_us = q_clock_getmicros_monotonic();
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
        // text that sandboxed code chose is reported marked, whoever gets the result (qore#7139)
        if (err_sandboxed) {
            qore_mark_last_exception_sandboxed(*xsink, err_pgm_id);
        }
        return QoreValue();
    }

    assert(state == RESOLVED);
    return result.refSelf();
}

QoreValue QoreFuture::get(int64 timeout_ms, ExceptionSink* xsink) {
    return priv->get(timeout_ms, xsink);
}

bool QoreFuture::isDone() const {
    return priv->isDone();
}

bool QoreFuture::isError() const {
    return priv->isError();
}

bool QoreFuture::cancel() {
    return priv->cancel();
}

void QoreFuture::destructor(ExceptionSink* xsink) {
    priv->futureDestroyed(xsink);
}

void QoreFuture::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        priv->deref();
        delete this;
    }
}

// --- QorePromise ---

QorePromise::QorePromise() : priv(new qore_future_private()) {
}

QorePromise::~QorePromise() {
}

void QorePromise::set(QoreValue val, ExceptionSink* xsink) {
    priv->set(val, xsink);
}

void QorePromise::setError(const char* err, const char* desc, QoreValue arg, ExceptionSink* xsink) {
    priv->setError(err, desc, arg, xsink);
}

void QorePromise::setException(ExceptionSink& xs) {
    priv->setException(xs);
}

QoreFuture* QorePromise::getFuture(ExceptionSink* xsink) {
    if (!priv->markFutureRetrieved()) {
        xsink->raiseException("PROMISE-ERROR", "Future has already been retrieved from this Promise");
        return nullptr;
    }
    return new QoreFuture(priv);
}

void QorePromise::destructor(ExceptionSink* xsink) {
    priv->promiseDestroyed(xsink);
}

void QorePromise::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        priv->deref();
        delete this;
    }
}

qore_future_private* qore_future_private::getFromPromise(QorePromise* p) {
    return p->priv;
}
