/*
  QoreCounter.cpp

  Qore Programming Language

  Copyright (C) 2005 - 2024 Qore Technologies, s.r.o.

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
#include <qore/QoreCounter.h>
#include <qore/QoreCondition.h>

#include <cerrno>

struct qore_counter_private {
    enum cond_status_e { Cond_Deleted = -1 };

    QoreThreadLock l;
    QoreCondition cond;
    int cnt;
    int waiting;

    DLLLOCAL qore_counter_private(int nc) : cnt(nc), waiting(0) {
        assert(nc >= 0);
    }

    DLLLOCAL ~qore_counter_private() {
    }

    DLLLOCAL void destructor(ExceptionSink* xsink) {
        AutoLocker al(&l);
        //printd(5, "qore_counter_private::destructor() this: %p waiting: %d cnt: %d\n", this, waiting, cnt);
        assert(cnt != Cond_Deleted);
        cnt = Cond_Deleted;
        if (waiting) {
            xsink->raiseException("COUNTER-ERROR", "Counter deleted while there %s %d waiting thread%s",
                                waiting == 1 ? "is" : "are", waiting, waiting == 1 ? "" : "s");
            cond.broadcast();
        }
    }

    DLLLOCAL int inc() {
        AutoLocker al(&l);
        if (cnt >= 0) {
            return ++cnt;
        }
        return 0;
    }

    DLLLOCAL int dec(ExceptionSink* xsink) {
        AutoLocker al(&l);
        if (cnt == Cond_Deleted) {
            xsink->raiseException("COUNTER-ERROR", "cannot execute Counter::dec(): Counter has been deleted in another thread");
            return -1;
        }
        if (!cnt) {
            xsink->raiseException("COUNTER-ERROR", "cannot execute Counter::dec(): Counter is already at 0; you must call Counter::inc() once before every call to Counter::dec()");
            return -1;
        }

        if (!--cnt && waiting)
            cond.broadcast();

        return cnt;
    }

    DLLLOCAL int waitForZero(ExceptionSink* xsink, int timeout_ms) {
        // NOTE that we do not do a while(true) { cond.wait(); } because any broadcast means that the
        // counter hit zero, so even it it's bigger than zero by the time we are allowed to execute, it's ok
        // --- synchronization must be done externally
        int rc = 0;
        SafeLocker sl(&l);
        ++waiting;
        while (cnt && cnt != Cond_Deleted) {
            if (!timeout_ms) {
                // Use interruptible wait for sandbox support
                rc = cond.waitWithInterrupt(&l, xsink);
                if (rc == QORE_COND_RESULT_INTERRUPTED) {
                    --waiting;
                    return -1;  // Exception already raised
                }
            } else {
                // Use interruptible wait with timeout for sandbox support
                rc = cond.waitWithInterrupt(&l, timeout_ms, xsink);
                if (rc == QORE_COND_RESULT_INTERRUPTED) {
                    --waiting;
                    return -1;  // Exception already raised
                }
                if (rc == QORE_COND_RESULT_TIMEOUT) {
                    break;
                }
            }
        }
        --waiting;
        if (cnt == Cond_Deleted) {
            xsink->raiseException("COUNTER-ERROR", "cannot execute Counter::waitForZero(); Counter was deleted in "
                "another thread while waiting %p", this);
            return -1;
        }
        return rc == QORE_COND_RESULT_TIMEOUT ? ETIMEDOUT : 0;
    }

    DLLLOCAL void waitForZero() {
        AutoLocker al(&l);
        ++waiting;
        while (cnt)
            cond.wait(&l);
        --waiting;
    }

    DLLLOCAL void dec() {
        AutoLocker al(&l);
        if (!--cnt && waiting)
            cond.broadcast();
    }
};

QoreCounter::QoreCounter(int nc) : priv(new qore_counter_private(nc)) {
}

QoreCounter::~QoreCounter() {
    delete priv;
}

void QoreCounter::destructor(ExceptionSink* xsink) {
    priv->destructor(xsink);
}

int QoreCounter::inc() {
    return priv->inc();
}

int QoreCounter::dec(ExceptionSink* xsink) {
    return priv->dec(xsink);
}

int QoreCounter::waitForZero(ExceptionSink* xsink, int timeout_ms) {
    return priv->waitForZero(xsink, timeout_ms);
}

int QoreCounter::getCount() const {
    return priv->cnt;
}

int QoreCounter::getWaiting() const {
    return priv->waiting;
}

// internal only
void QoreCounter::waitForZero() {
    priv->waitForZero();
}

// internal only
void QoreCounter::dec() {
    priv->dec();
}
