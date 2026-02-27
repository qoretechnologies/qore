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
#include <qore/AbstractPrivateData.h>

#include <string>

//! Shared implementation for Future and Promise
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
                err_code = errv.get<const QoreStringNode>()->c_str();
            }
            if (descv.getType() == NT_STRING) {
                err_desc = descv.get<const QoreStringNode>()->c_str();
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
        // Convention: timeout_ms <= 0 means infinite; waitWithInterrupt uses -1 for infinite
        int64 cond_timeout = (timeout_ms <= 0) ? -1 : timeout_ms;

        AutoLocker al(&lock);
        ++waiting;
        while (state == PENDING) {
            int rc = cond.waitWithInterrupt(&lock, cond_timeout, xsink);
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

//! Future class: consumer side of Promise/Future pair
class QoreFuture : public AbstractPrivateData {
public:
    DLLLOCAL QoreFuture(qore_future_private* p) : priv(p) {
        priv->ref();
    }

    DLLLOCAL QoreValue get(int64 timeout_ms, ExceptionSink* xsink) {
        return priv->get(timeout_ms, xsink);
    }

    DLLLOCAL bool isDone() const {
        return priv->isDone();
    }

    DLLLOCAL bool isError() const {
        return priv->isError();
    }

    DLLLOCAL void destructor(ExceptionSink* xsink) {
        priv->futureDestroyed(xsink);
    }

    using AbstractPrivateData::deref;
    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            priv->deref();
            delete this;
        }
    }

protected:
    DLLLOCAL virtual ~QoreFuture() {}

private:
    qore_future_private* priv;
};

//! Promise class: producer side of Promise/Future pair
class QorePromise : public AbstractPrivateData {
public:
    DLLLOCAL QorePromise() : priv(new qore_future_private()) {
    }

    DLLLOCAL void set(QoreValue val, ExceptionSink* xsink) {
        priv->set(val, xsink);
    }

    DLLLOCAL void setError(const char* err, const char* desc, QoreValue arg, ExceptionSink* xsink) {
        priv->setError(err, desc, arg, xsink);
    }

    DLLLOCAL void setException(ExceptionSink& xs) {
        priv->setException(xs);
    }

    DLLLOCAL QoreFuture* getFuture(ExceptionSink* xsink) {
        if (!priv->markFutureRetrieved()) {
            xsink->raiseException("PROMISE-ERROR", "Future has already been retrieved from this Promise");
            return nullptr;
        }
        return new QoreFuture(priv);
    }

    DLLLOCAL void destructor(ExceptionSink* xsink) {
        priv->promiseDestroyed(xsink);
    }

    using AbstractPrivateData::deref;
    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            priv->deref();
            delete this;
        }
    }

    //! Get internal private data (for call_async implementation)
    DLLLOCAL qore_future_private* getPriv() {
        return priv;
    }

protected:
    DLLLOCAL virtual ~QorePromise() {}

private:
    qore_future_private* priv;
};

#endif // _QORE_INTERN_QOREFUTURE_H
