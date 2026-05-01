/*
    QoreCondition.cpp

    Qore Programming Language

    Copyright (C) 2005 - 2026 Qore Technologies, s.r.o.

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
#include <qore/QoreCondition.h>
#include <qore/QoreSandboxManager.h>
#include "qore/intern/QoreThreadList.h"

#include <cerrno>
#include <cstring>

QoreCondition::QoreCondition() {
    pthread_cond_init(&c, 0);
}

QoreCondition::~QoreCondition() {
    pthread_cond_destroy(&c);
}

int QoreCondition::signal() {
    signal_gen.fetch_add(1, std::memory_order_release);
    return pthread_cond_signal(&c);
}

int QoreCondition::broadcast() {
    signal_gen.fetch_add(1, std::memory_order_release);
    return pthread_cond_broadcast(&c);
}

uint64_t QoreCondition::getSignalGen() const {
    return signal_gen.load(std::memory_order_acquire);
}

int QoreCondition::wait(pthread_mutex_t *m) {
#ifdef DEBUG
    int rc = pthread_cond_wait(&c, m);
    if (rc) {
        printd(0, "QoreCondition::wait(%p) pthread_cond_wait() returned %d %s\n", m, rc, strerror(rc));
        // print out a backtrace if possible
        qore_machine_backtrace();
    }
    assert(!rc);
    return rc;
#else
    return pthread_cond_wait(&c, m);
#endif
}

// timeout is in milliseconds
int QoreCondition::wait(pthread_mutex_t* m, int timeout_ms) {
    return wait2(m, timeout_ms);
}

// timeout is in milliseconds
int QoreCondition::wait2(pthread_mutex_t* m, int64 timeout_ms) {
    // negative timeouts mean wait indefinitely
    if (timeout_ms < 0) {
        return wait(m);
    }
#ifdef DARWIN
    // use more efficient pthread_cond_timedwait_relative_np() on Darwin
    struct timespec tmout;
    tmout.tv_sec = timeout_ms / 1000;
    tmout.tv_nsec = (timeout_ms - tmout.tv_sec * 1000) * 1000000;

#ifndef DEBUG
    return pthread_cond_timedwait_relative_np(&c, m, &tmout);
#else // !DEBUG
    //printd(5, "QoreCondition::wait(%p, %lld) this=%p +trigger=%d.%09d\n", m, timeout_ms, this, tmout.tv_sec, tmout.tv_nsec);
    int rc = pthread_cond_timedwait_relative_np(&c, m, &tmout);
    if (rc && rc != ETIMEDOUT) {
        printd(0, "QoreCondition::wait(m=%p, timeout_ms=%lld) this=%p pthread_cond_timedwait_relative_np() returned %d %s (errno=%d %s)\n", m, timeout_ms, this, rc, strerror(rc), errno, strerror(errno));
        // print out a backtrace if possible
        qore_machine_backtrace();
    }
    assert(!rc || rc == ETIMEDOUT);
    return rc;
#endif // DEBUG
#else // !DARWIN
    struct timeval now;
    struct timespec tmout;

#ifdef DEBUG
    int64 timeout_ms_orig = timeout_ms;
#endif // DEBUG

    gettimeofday(&now, 0);
    int64 secs = timeout_ms / 1000;
    timeout_ms -= secs * 1000;
    int64 nsecs = now.tv_usec * 1000 + timeout_ms * 1000000;
    int64 dsecs = nsecs / 1000000000;
    nsecs -= dsecs * 1000000000;
    tmout.tv_sec = now.tv_sec + secs + dsecs;
    tmout.tv_nsec = nsecs;

    // make sure mutex is locked
    assert(pthread_mutex_trylock(m) == EBUSY);

#ifndef DEBUG
    return pthread_cond_timedwait(&c, m, &tmout);
#else // !DEBUG
    //printd(5, "QoreCondition::wait(%p, " QLLD ") this=%p now=%d.%09d trigger=%d.%09d\n", m, timeout_ms, this, now.tv_sec, now.tv_usec * 1000, tmout.tv_sec, tmout.tv_nsec);
    int rc = pthread_cond_timedwait(&c, m, &tmout);
    if (rc && rc != ETIMEDOUT) {
        printd(0, "QoreCondition::wait(m=%p, timeout_ms=%d) pthread_cond_timedwait() returned %d %s\n", m, timeout_ms_orig, rc, strerror(rc));
        // print out a backtrace if possible
        qore_machine_backtrace();
    }
    assert(!rc || rc == ETIMEDOUT);
    return rc;
#endif // DEBUG
#endif // DARWIN
}

int QoreCondition::waitWithInterrupt(pthread_mutex_t* m, ExceptionSink* xsink) {
    return waitWithInterrupt(m, -1, xsink);
}

int QoreCondition::waitWithInterrupt(pthread_mutex_t* m, int64 timeout_ms, ExceptionSink* xsink) {
    // pre-wait check: cancel set before we even tried to wait
    if (qore_check_cancel(xsink, "condition wait")) {
        return QORE_COND_RESULT_INTERRUPTED;
    }

    // Register so cancelThread()/SandboxManager::requestInterrupt() can wake us via broadcast().
    // The seq_cst on this store pairs with seq_cst on the cancel side (set flag, read waiting_on)
    // to defeat the lost-wakeup race: at least one of "waiter sees flag" or "canceller sees pointer"
    // is guaranteed.  See design/cooperative-cancellation.md.
    thread_list.setCurrentWaitingOn(this);

    // Re-check after registration: if cancel arrived just before our store became visible to the
    // canceller, the canceller may have read waiting_on==nullptr and skipped the broadcast — so
    // we must catch it ourselves.
    if (qore_check_cancel(xsink, "condition wait")) {
        thread_list.clearCurrentWaitingOn();
        return QORE_COND_RESULT_INTERRUPTED;
    }

    // Single wait — broadcast-on-cancel will wake us if cancellation is requested while we sleep,
    // so no polling loop is needed.
    int rc = wait2(m, timeout_ms);

    thread_list.clearCurrentWaitingOn();

    // If we were woken by a cancel-induced broadcast (rc==0) or cancel arrived between wakeup
    // and unregister, report INTERRUPTED rather than SUCCESS.
    if (qore_check_cancel(xsink, "condition wait")) {
        return QORE_COND_RESULT_INTERRUPTED;
    }

    if (rc == 0) {
        return QORE_COND_RESULT_SUCCESS;
    }
    return QORE_COND_RESULT_TIMEOUT;
}
