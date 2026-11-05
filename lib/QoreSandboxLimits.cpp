/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSandboxLimits.cpp

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

// Per-call sandbox time limits: see design/sandbox-resource-limits.md

#include <qore/Qore.h>
#include <qore/QoreSandboxManager.h>
#include "qore/intern/QoreSandboxLimits.h"
#include "qore/intern/QoreSandboxProvenance.h"
#include "qore/intern/QoreThreadList.h"
#include "qore/intern/QoreForkHandlers.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <new>
#include <pthread.h>
#include <unistd.h>

//! The process-wide thread that expires call budgets at their deadlines
/** It waits on a condition for the earliest deadline; a new earliest deadline signals it, so there is no polling.
    The lock is a leaf: nothing else is acquired while it is held (expiry releases it before it takes the thread
    list lock).  Started on the first timed call, stopped by qore_cleanup(), and started again on demand in a child
    created with fork(), like the condition waker.
*/
class QoreSandboxWatchdog {
public:
    ~QoreSandboxWatchdog() {
        // qore_cleanup() stops the thread; this covers a host that reaches static destruction without it
        stop();
    }

    //! Schedules a new budget and attaches the current thread to it; returns -1 if it cannot (nothing is changed)
    int add(QoreSandboxCallBudget* budget, int tid) {
        AutoLocker al(lck);
        if (ensureStarted()) {
            return -1;
        }
        try {
            budget->tids.push_back(tid);
            try {
                budget->pos = deadlines.emplace(budget->deadline_us, budget);
            } catch (...) {
                budget->tids.pop_back();
                throw;
            }
        } catch (std::bad_alloc&) {
            return -1;
        }
        budget->scheduled = true;
        if (budget->pos == deadlines.begin()) {
            // a new earliest deadline
            cond.signal();
        }
        return 0;
    }

    //! Attaches a thread to a budget; returns -1 if it cannot (nothing is changed)
    int attach(QoreSandboxCallBudget* budget, int tid) {
        AutoLocker al(lck);
        try {
            budget->tids.push_back(tid);
        } catch (std::bad_alloc&) {
            return -1;
        }
        return 0;
    }

    //! Detaches a thread attached with add() or attach()
    void detach(QoreSandboxCallBudget* budget, int tid) {
        AutoLocker al(lck);
        auto i = std::find(budget->tids.begin(), budget->tids.end(), tid);
        assert(i != budget->tids.end());
        if (i != budget->tids.end()) {
            budget->tids.erase(i);
        }
    }

    //! Removes a budget whose last reference is gone from the deadline map
    void unschedule(QoreSandboxCallBudget* budget) {
        AutoLocker al(lck);
        if (budget->scheduled) {
            deadlines.erase(budget->pos);
            budget->scheduled = false;
        }
    }

    void stop() {
        {
            AutoLocker al(lck);
            if (!started || started_pid != getpid()) {
                return;
            }
            stopping = true;
            cond.signal();
        }
        int rc = pthread_join(native_id, nullptr);
        if (rc) {
            fprintf(stderr, "qore: cannot join the sandbox watchdog thread: %s\n", strerror(rc));
            abort();
        }
        AutoLocker al(lck);
        started = stopping = false;
    }

    //! Starts the thread in a child created with fork() if budgets are scheduled
    void afterFork() {
        AutoLocker al(lck);
        if (!deadlines.empty() && ensureStarted()) {
            // the budgets of the forking thread cannot expire; the next timed call fails closed if the thread still
            // cannot be started
            fprintf(stderr, "qore: cannot start the sandbox watchdog thread in the child process: %s\n",
                strerror(errno));
        }
    }

    //! Makes the watchdog usable in the child after fork(), where its thread does not exist
    /** The lock was acquired before fork(); the thread is started again when it is next needed
    */
    void forkChild() {
        started = stopping = false;
        cond.reinitAfterFork();
        lck.reinitAfterFork();
    }

    QoreThreadLock lck;

private:
    QoreCondition cond;
    //! the scheduled budgets by deadline; no references are held (see QoreSandboxCallBudget)
    std::multimap<int64, QoreSandboxCallBudget*> deadlines;
    pthread_t native_id{};
    pid_t started_pid = 0;
    bool started = false;
    bool stopping = false;

    //! Starts the thread in this process if it is not running; lck must be held; returns -1 with errno set on error
    int ensureStarted() {
        pid_t pid = getpid();
        if (started && started_pid == pid) {
            return 0;
        }
        // after fork() the thread does not exist in this process
        started = stopping = false;
        int rc = pthread_create(&native_id, nullptr, entry, this);
        if (rc) {
            errno = rc;
            return -1;
        }
        started = true;
        started_pid = pid;
        return 0;
    }

    static void* entry(void* arg) {
        static_cast<QoreSandboxWatchdog*>(arg)->run();
        return nullptr;
    }

    void run();
};

static QoreSandboxWatchdog sandbox_watchdog;

void QoreSandboxWatchdog::run() {
#ifdef QORE_HAVE_THREAD_NAME
    q_set_thread_name("qore-sb-watchdog");
#endif
    std::vector<QoreSandboxCallBudget*> due;
    std::vector<int> tids;
    SafeLocker sl(lck);
    while (!stopping) {
        if (deadlines.empty()) {
            cond.wait(lck);
            continue;
        }
        int64 now = q_get_monotonic_us();
        int64 next = deadlines.begin()->first;
        if (next > now) {
            // round up, so that the wait never ends before the deadline
            cond.wait2(lck, (next - now + 999) / 1000);
            continue;
        }
        // expire every budget whose deadline has passed; the TIDs are copied before the lock is released, since the
        // threads detach under it
        try {
            while (!deadlines.empty() && deadlines.begin()->first <= now) {
                QoreSandboxCallBudget* budget = deadlines.begin()->second;
                // allocate before referencing: a reference cannot be dropped under the lock (the last dereference
                // takes it)
                due.reserve(due.size() + 1);
                size_t old_size = tids.size();
                tids.insert(tids.end(), budget->tids.begin(), budget->tids.end());
                if (!budget->tryRef()) {
                    // being destroyed; its destructor waits for the lock
                    tids.resize(old_size);
                } else {
                    due.push_back(budget);
                    // a thread that attaches from now on sees the flag (see QoreSandboxBudgetStack::attach())
                    budget->expired.store(true, std::memory_order_release);
                }
                deadlines.erase(deadlines.begin());
                budget->scheduled = false;
            }
        } catch (std::bad_alloc&) {
            // the budgets processed so far are delivered below; the rest stay scheduled and are processed in the
            // next iteration
        }
        sl.unlock();
        // the expired flags are set before the bits: a thread that clears its bit under the thread list lock checks
        // its budgets under the same lock (see QoreThreadList::clearSandboxLimit())
        thread_list.requestSandboxLimit(tids);
        tids.clear();
        for (QoreSandboxCallBudget* budget : due) {
            budget->deref();
        }
        due.clear();
        sl.lock();
    }
}

namespace {
//! the fork handler of the sandbox watchdog: its thread is started again in the child when it is needed
class QoreSandboxWatchdogForkHandler : public QoreForkHandler {
public:
    QoreSandboxWatchdogForkHandler() : QoreForkHandler(QFLR_LEAF, "sandbox_watchdog") {
    }

    void prepare() noexcept override {
        sandbox_watchdog.lck.lock();
    }

    void parent() noexcept override {
        sandbox_watchdog.lck.unlock();
    }

    void child() noexcept override {
        sandbox_watchdog.forkChild();
    }
};

QoreSandboxWatchdogForkHandler sandbox_watchdog_fork_handler;
}

QoreSandboxCallBudget* qore_sandbox_lost_budget() {
    // never freed: it is referenced like any other budget, and its count never drops to zero
    static QoreSandboxCallBudget* lost = new QoreSandboxCallBudget;
    return lost;
}

void qore_sandbox_watchdog_stop() {
    sandbox_watchdog.stop();
}

void qore_sandbox_watchdog_after_fork() {
    sandbox_watchdog.afterFork();
}

QoreSandboxCallBudget::QoreSandboxCallBudget(QoreSandboxManager* manager, int64 limit_ms, int64 stack_bytes)
        : manager(manager), limit_ms(limit_ms > 0 ? limit_ms : 0), stack_bytes(stack_bytes > 0 ? stack_bytes : 0),
        deadline_us(limit_ms > 0 ? q_get_monotonic_us() + limit_ms * 1000 : 0) {
    assert(manager);
    assert(limit_ms > 0 || stack_bytes > 0);
    manager->ref();
}

QoreSandboxCallBudget::QoreSandboxCallBudget() : manager(nullptr), limit_ms(0), stack_bytes(0), deadline_us(0),
        expired(true) {
}

QoreSandboxCallBudget::~QoreSandboxCallBudget() {
    assert(tids.empty());
    if (manager) {
        manager->deref(nullptr);
    }
}

bool QoreSandboxCallBudget::tryRef() {
    int count = refs.load(std::memory_order_relaxed);
    while (count > 0) {
        if (refs.compare_exchange_weak(count, count + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
            return true;
        }
    }
    return false;
}

void QoreSandboxCallBudget::deref() {
    if (refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        // the watchdog acts only on budgets it could reference, so after this nothing refers to the budget
        sandbox_watchdog.unschedule(this);
        delete this;
    }
}

void qore_sandbox_budgets_deref(QoreSandboxBudgetList& budgets) {
    for (QoreSandboxCallBudget* budget : budgets) {
        budget->deref();
    }
    budgets.clear();
}

QoreSandboxBudgetStack::~QoreSandboxBudgetStack() {
    // every helper that attached a frame removes it before the thread's data is destroyed
    assert(frames.empty());
    for (Frame& frame : frames) {
        frame.budget->deref();
    }
}

bool QoreSandboxBudgetStack::hasBudgetFor(const QoreSandboxManager* sm) const {
    for (const Frame& frame : frames) {
        if (frame.budget->manager == sm) {
            return true;
        }
    }
    return false;
}

void QoreSandboxBudgetStack::enter(int tid, QoreSandboxManager* sm, const void* owner) {
    assert(sm);
    // inside a call whose limit cannot be enforced, nothing more is recorded until it is left
    if (lost_owner) {
        return;
    }
    if (const std::vector<QoreSandboxManager*>* members = QoreSandboxComposition::members(sm)) {
        for (QoreSandboxManager* member : *members) {
            // the constituents are plain managers, so their getters report their own limits
            int64 limit = member->getWallTimeLimit();
            int64 stack = member->getMaxStackSize();
            if ((limit > 0 || stack > 0) && !hasBudgetFor(member)) {
                startBudget(tid, member, limit, stack, owner);
                if (lost_owner) {
                    return;
                }
            }
        }
        return;
    }
    int64 limit = sm->getWallTimeLimit();
    int64 stack = sm->getMaxStackSize();
    if ((limit > 0 || stack > 0) && !hasBudgetFor(sm)) {
        startBudget(tid, sm, limit, stack, owner);
    }
}

void QoreSandboxBudgetStack::pushFrame(QoreSandboxCallBudget* budget, const void* owner) {
    frames.push_back({budget, owner, 0, 0});
    if (budget->stack_bytes > 0) {
        Frame& frame = frames.back();
        frame.applied_stack_limit = qore_sandbox_tighten_stack(budget->stack_bytes, frame.saved_stack_limit);
    }
}

QoreSandboxCallBudget* QoreSandboxBudgetStack::popFrame() {
    assert(!frames.empty());
    Frame& frame = frames.back();
    if (frame.applied_stack_limit) {
        qore_sandbox_restore_stack(frame.saved_stack_limit);
    }
    QoreSandboxCallBudget* budget = frame.budget;
    frames.pop_back();
    return budget;
}

void QoreSandboxBudgetStack::startBudget(int tid, QoreSandboxManager* sm, int64 limit_ms, int64 stack_bytes,
        const void* owner) {
    QoreSandboxCallBudget* budget;
    try {
        budget = new QoreSandboxCallBudget(sm, limit_ms, stack_bytes);
    } catch (std::bad_alloc&) {
        lose(tid, owner);
        return;
    }
    try {
        pushFrame(budget, owner);
    } catch (std::bad_alloc&) {
        budget->deref();
        lose(tid, owner);
        return;
    }
    if (budget->isTimed() && sandbox_watchdog.add(budget, tid)) {
        popFrame()->deref();
        lose(tid, owner);
    }
}

void QoreSandboxBudgetStack::attach(int tid, const QoreSandboxBudgetList& budgets, const void* owner) {
    if (lost_owner) {
        return;
    }
    for (QoreSandboxCallBudget* budget : budgets) {
        if (budget == qore_sandbox_lost_budget()) {
            lose(tid, owner);
            return;
        }
        try {
            pushFrame(budget, owner);
        } catch (std::bad_alloc&) {
            lose(tid, owner);
            return;
        }
        budget->ref();
        if (budget->isTimed() && sandbox_watchdog.attach(budget, tid)) {
            popFrame()->deref();
            lose(tid, owner);
            return;
        }
        // checked after the TID was added under the watchdog lock: either the watchdog copied the TID and sets the
        // bit, or it had already set the flag, which is seen here
        if (budget->isExpired()) {
            thread_list.requestSandboxLimit({tid});
        }
    }
}

void QoreSandboxBudgetStack::lose(int tid, const void* owner) {
    assert(!lost_owner);
    lost_owner = owner;
    thread_list.requestSandboxLimit({tid});
}

void QoreSandboxBudgetStack::leaveIntern(int tid, const void* owner) {
    bool limited = false;
    if (lost_owner == owner) {
        lost_owner = nullptr;
        limited = true;
    }
    while (!frames.empty() && frames.back().owner == owner) {
        QoreSandboxCallBudget* budget = popFrame();
        if (budget->isTimed()) {
            sandbox_watchdog.detach(budget, tid);
        }
        if (budget->isExpired()) {
            limited = true;
        }
        budget->deref();
    }
    // the limit bit is cleared when the thread has left every expired call; it is checked again under the thread
    // list lock, so a budget expiring meanwhile keeps it set
    if (limited && !lost_owner) {
        thread_list.clearSandboxLimit(tid, *this);
    }
}

void QoreSandboxBudgetStack::capture(QoreSandboxBudgetList& out) const {
    if (lost_owner) {
        // the static budget is referenced like any other, so that dereferencing the list never frees it
        out.push_back(qore_sandbox_lost_budget());
        qore_sandbox_lost_budget()->ref();
        return;
    }
    out.reserve(out.size() + frames.size());
    for (const Frame& frame : frames) {
        frame.budget->ref();
        out.push_back(frame.budget);
    }
}

const QoreSandboxCallBudget* QoreSandboxBudgetStack::findExpired() const {
    if (lost_owner) {
        return qore_sandbox_lost_budget();
    }
    for (const Frame& frame : frames) {
        if (frame.budget->isExpired()) {
            return frame.budget;
        }
    }
    return nullptr;
}

int64 QoreSandboxBudgetStack::findStackLimit(size_t limit) const {
    for (const Frame& frame : frames) {
        if (frame.applied_stack_limit && frame.applied_stack_limit == limit) {
            return frame.budget->stack_bytes;
        }
    }
    return 0;
}

void qore_sandbox_raise_limit(const QoreSandboxCallBudget* budget, ExceptionSink* xsink, const char* operation) {
    assert(budget);
    if (budget == qore_sandbox_lost_budget()) {
        xsink->raiseException("SANDBOX-ERROR", "%s: the sandbox limits cannot be enforced for this call: the "
            "resources to track them could not be allocated", operation);
        return;
    }
    xsink->raiseException("SANDBOX-TIMEOUT", "%s: the sandbox wall time limit of " QLLD " ms was exceeded",
        operation, budget->getLimitMs());
}
