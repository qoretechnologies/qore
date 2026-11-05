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

#ifdef DARWIN
#include <mach/mach.h>
#endif

//! the shortest interval between two checks of the CPU time of a call, in microseconds
/** Close to the end of a CPU budget, the earliest point it could run out comes ever sooner; the floor bounds the
    number of checks, and so the amount by which a call can exceed its CPU-time limit, to this interval per thread
*/
static constexpr int64 QSB_MIN_CPU_CHECK_US = 1000;

int QoreThreadCpuClock::init() {
#if defined(HAVE_PTHREAD_GETCPUCLOCKID)
    static_assert(sizeof(clockid_t) <= sizeof(handle), "a clockid_t fits the handle");
    clockid_t cid;
    if (pthread_getcpuclockid(pthread_self(), &cid)) {
        return -1;
    }
    handle = static_cast<uint64_t>(cid);
    return 0;
#elif defined(DARWIN)
    static_assert(sizeof(mach_port_t) <= sizeof(handle), "a Mach port fits the handle");
    mach_port_t port = mach_thread_self();
    if (port == MACH_PORT_NULL) {
        return -1;
    }
    handle = port;
    return 0;
#else
    return -1;
#endif
}

int64 QoreThreadCpuClock::read() const {
#if defined(HAVE_PTHREAD_GETCPUCLOCKID)
    struct timespec ts;
    if (clock_gettime(static_cast<clockid_t>(handle), &ts)) {
        return -1;
    }
    return static_cast<int64>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
#elif defined(DARWIN)
    thread_basic_info_data_t info;
    mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
    if (thread_info(static_cast<mach_port_t>(handle), THREAD_BASIC_INFO, reinterpret_cast<thread_info_t>(&info),
            &count) != KERN_SUCCESS) {
        return -1;
    }
    return (static_cast<int64>(info.user_time.seconds) + info.system_time.seconds) * 1000000000LL
        + (static_cast<int64>(info.user_time.microseconds) + info.system_time.microseconds) * 1000LL;
#else
    return -1;
#endif
}

void QoreThreadCpuClock::release() {
#if defined(DARWIN) && !defined(HAVE_PTHREAD_GETCPUCLOCKID)
    if (handle) {
        mach_port_deallocate(mach_task_self(), static_cast<mach_port_t>(handle));
        handle = 0;
    }
#endif
}

//! The process-wide thread that expires call budgets
/** It waits on a condition for the earliest event of any budget: a wall-time deadline, or the earliest point at which
    the CPU time of a call could be used up (the remaining CPU time divided by the number of its threads); a new
    earliest event signals it, so there is no polling interval.  At a CPU check it measures the CPU time of the
    call's threads and either expires the budget or schedules the next check.

    The lock is a leaf: nothing else is acquired while it is held (expiry releases it before it takes the thread
    list lock).  Started on the first timed call, stopped by qore_cleanup(), and started again on demand in a child
    created with fork(), like the condition waker.
*/
class QoreSandboxWatchdog {
public:
    //! the result of attaching a thread
    enum AttachResult {
        //! attached
        ATTACHED = 0,
        //! no memory, or the watchdog thread cannot be started
        NO_RESOURCES = -1,
        //! the platform has no thread CPU clock
        NO_CPU_CLOCK = -2,
    };

    ~QoreSandboxWatchdog() {
        // qore_cleanup() stops the thread; this covers a host that reaches static destruction without it
        stop();
    }

    //! Attaches the current thread to a timed budget and schedules the budget's next event
    /** @param first true for the thread that starts the call, whose budget is not scheduled yet
    */
    AttachResult attach(QoreSandboxCallBudget* budget, int tid, bool first) {
        assert(budget->isTimed());
        AutoLocker al(lck);
        if (ensureStarted()) {
            return NO_RESOURCES;
        }
        for (auto& a : budget->attachments) {
            if (a.tid == tid) {
                // the thread already runs the call; it is counted once
                ++a.count;
                return ATTACHED;
            }
        }
        QoreSandboxCallBudget::Attachment a{tid, 1, QoreThreadCpuClock(), 0};
        if (budget->cpu_limit_ms > 0) {
            if (a.clock.init()) {
                return NO_CPU_CLOCK;
            }
            a.baseline_ns = a.clock.read();
            if (a.baseline_ns < 0) {
                a.clock.release();
                return NO_CPU_CLOCK;
            }
        }
        try {
            budget->attachments.push_back(a);
        } catch (std::bad_alloc&) {
            a.clock.release();
            return NO_RESOURCES;
        }
        // the first thread of a call: its next event can be computed without measuring (it has used nothing yet);
        // a thread joining a call with a CPU limit makes the CPU time run out sooner: check at once
        int64 now = q_get_monotonic_us();
        int64 next;
        if (first) {
            next = nextEvent(budget, now, 0);
        } else if (budget->cpu_limit_ms > 0 && !budget->isExpired()) {
            next = now;
        } else {
            // the call's events do not change
            return ATTACHED;
        }
        if (schedule(budget, next)) {
            budget->attachments.back().clock.release();
            budget->attachments.pop_back();
            return NO_RESOURCES;
        }
        return ATTACHED;
    }

    //! Detaches the current thread from a timed budget; its CPU time is added to the budget's total
    void detach(QoreSandboxCallBudget* budget, int tid) {
        AutoLocker al(lck);
        for (auto i = budget->attachments.begin(), e = budget->attachments.end(); i != e; ++i) {
            if (i->tid != tid) {
                continue;
            }
            if (--i->count) {
                return;
            }
            if (budget->cpu_limit_ms > 0) {
                int64 now = i->clock.read();
                if (now > i->baseline_ns) {
                    budget->cpu_used_ns += now - i->baseline_ns;
                }
                i->clock.release();
            }
            budget->attachments.erase(i);
            return;
        }
        assert(false);
    }

#ifdef DEBUG
    //! Returns the CPU time in nanoseconds that the threads of a budget have used (debug builds, for tests)
    int64 dbgMeasure(const QoreSandboxCallBudget* budget) {
        AutoLocker al(lck);
        return measure(budget);
    }
#endif

    //! Removes a budget whose last reference is gone from the event map
    void unschedule(QoreSandboxCallBudget* budget) {
        AutoLocker al(lck);
        unscheduleIntern(budget);
        assert(budget->attachments.empty());
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
        if (!events.empty() && ensureStarted()) {
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
    //! the scheduled budgets by the time of their next event; no references are held (see QoreSandboxCallBudget)
    std::multimap<int64, QoreSandboxCallBudget*> events;
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

    //! Returns the CPU time in nanoseconds that the budget's threads have used; lck must be held
    static int64 measure(const QoreSandboxCallBudget* budget) {
        int64 used = budget->cpu_used_ns;
        for (const auto& a : budget->attachments) {
            int64 now = a.clock.read();
            // a thread that cannot be read (in a child after fork(), the threads of the parent) counts nothing more
            if (now > a.baseline_ns) {
                used += now - a.baseline_ns;
            }
        }
        return used;
    }

    //! Returns the time of the budget's next event, or 0 if it has none; lck must be held
    /** @param now the current monotonic time
        @param used the CPU time used by the budget's threads in nanoseconds
    */
    static int64 nextEvent(const QoreSandboxCallBudget* budget, int64 now, int64 used) {
        int64 next = budget->limit_ms > 0 ? budget->deadline_us : 0;
        if (budget->cpu_limit_ms > 0 && !budget->attachments.empty()) {
            int64 remaining_us = (budget->cpu_limit_ms * 1000000LL - used) / 1000;
            int64 interval = remaining_us / static_cast<int64>(budget->attachments.size());
            int64 check = now + (interval > QSB_MIN_CPU_CHECK_US ? interval : QSB_MIN_CPU_CHECK_US);
            if (!next || check < next) {
                next = check;
            }
        }
        return next;
    }

    //! (Re)schedules the budget's next event; lck must be held; returns -1 if it cannot (nothing is changed)
    int schedule(QoreSandboxCallBudget* budget, int64 next) {
        if (budget->scheduled && budget->next_us == next) {
            return 0;
        }
        if (!next) {
            unscheduleIntern(budget);
            return 0;
        }
        std::multimap<int64, QoreSandboxCallBudget*>::iterator pos;
        try {
            pos = events.emplace(next, budget);
        } catch (std::bad_alloc&) {
            return -1;
        }
        unscheduleIntern(budget);
        budget->pos = pos;
        budget->next_us = next;
        budget->scheduled = true;
        if (pos == events.begin()) {
            // a new earliest event
            cond.signal();
        }
        return 0;
    }

    void unscheduleIntern(QoreSandboxCallBudget* budget) {
        if (budget->scheduled) {
            events.erase(budget->pos);
            budget->scheduled = false;
        }
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
        if (events.empty()) {
            cond.wait(lck);
            continue;
        }
        int64 now = q_get_monotonic_us();
        int64 next = events.begin()->first;
        if (next > now) {
            // round up, so that the wait never ends before the event
            cond.wait2(lck, (next - now + 999) / 1000);
            continue;
        }
        // handle every budget whose event has come; the TIDs of expired budgets are copied before the lock is
        // released, since the threads detach under it
        try {
            while (!events.empty() && events.begin()->first <= now) {
                QoreSandboxCallBudget* budget = events.begin()->second;
                unsigned kind = 0;
                if (budget->limit_ms > 0 && budget->deadline_us <= now) {
                    kind = QSB_LIMIT_WALL_TIME;
                } else if (budget->cpu_limit_ms > 0) {
                    int64 used = measure(budget);
                    if (used >= budget->cpu_limit_ms * 1000000LL) {
                        kind = QSB_LIMIT_CPU_TIME;
                    } else {
                        // not used up: check again at the earliest point it could be
                        if (schedule(budget, nextEvent(budget, now, used))) {
                            // cannot reschedule (no memory): check again at the next iteration
                            throw std::bad_alloc();
                        }
                        continue;
                    }
                }
                if (kind) {
                    // allocate before referencing: a reference cannot be dropped under the lock (the last
                    // dereference takes it)
                    due.reserve(due.size() + 1);
                    size_t old_size = tids.size();
                    tids.reserve(tids.size() + budget->attachments.size());
                    for (const auto& a : budget->attachments) {
                        tids.push_back(a.tid);
                    }
                    if (!budget->tryRef()) {
                        // being destroyed; its destructor waits for the lock
                        tids.resize(old_size);
                    } else {
                        due.push_back(budget);
                        budget->expired_kind = kind;
                        // a thread that attaches from now on sees the flag (see QoreSandboxBudgetStack::attach())
                        budget->expired.store(true, std::memory_order_release);
                    }
                }
                unscheduleIntern(budget);
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

QoreSandboxCallBudget* qore_sandbox_lost_budget(unsigned kinds) {
    assert(kinds && !(kinds & ~QSB_LIMIT_ALL));
    // one for each combination of limits; never freed: they are referenced like any other budget, and their counts
    // never drop to zero
    struct LostBudgets {
        QoreSandboxCallBudget* budgets[QSB_LIMIT_ALL];
        LostBudgets() {
            for (unsigned i = 0; i < QSB_LIMIT_ALL; ++i) {
                budgets[i] = new QoreSandboxCallBudget(i + 1);
            }
        }
    };
    static LostBudgets lost;
    return lost.budgets[kinds - 1];
}

#ifdef DEBUG
//! the number of budget recordings that fail as if no memory could be allocated; see qore_sandbox_dbg_fail_budget()
static std::atomic<int> sandbox_dbg_fail_budgets{0};

int64 qore_sandbox_dbg_measure_cpu(const QoreSandboxCallBudget* budget) {
    return sandbox_watchdog.dbgMeasure(budget);
}

void qore_sandbox_dbg_fail_next_budgets(int count) {
    sandbox_dbg_fail_budgets.store(count > 0 ? count : 0);
}

//! Returns true if a budget recording must fail (debug builds, for tests)
static bool qore_sandbox_dbg_fail_budget() {
    int count = sandbox_dbg_fail_budgets.load();
    while (count > 0) {
        if (sandbox_dbg_fail_budgets.compare_exchange_weak(count, count - 1)) {
            return true;
        }
    }
    return false;
}
#endif

void qore_sandbox_watchdog_stop() {
    sandbox_watchdog.stop();
}

void qore_sandbox_watchdog_after_fork() {
    sandbox_watchdog.afterFork();
}

QoreSandboxCallBudget::QoreSandboxCallBudget(QoreSandboxManager* manager, int64 limit_ms, int64 cpu_limit_ms,
        int64 stack_bytes, int64 memory_limit, QoreSandboxCharge* charge) : manager(manager),
        limit_ms(limit_ms > 0 ? limit_ms : 0), cpu_limit_ms(cpu_limit_ms > 0 ? cpu_limit_ms : 0),
        stack_bytes(stack_bytes > 0 ? stack_bytes : 0), memory_limit(memory_limit > 0 ? memory_limit : 0),
        charge(charge), deadline_us(limit_ms > 0 ? q_get_monotonic_us() + limit_ms * 1000 : 0) {
    assert(manager);
    assert(limit_ms > 0 || cpu_limit_ms > 0 || stack_bytes > 0 || charge);
    manager->ref();
}

QoreSandboxCallBudget::QoreSandboxCallBudget(unsigned kinds) : manager(nullptr), limit_ms(0), cpu_limit_ms(0),
        stack_bytes(0), memory_limit(0), charge(nullptr), deadline_us(0), expired(true), lost_kinds(kinds) {
    assert(kinds);
}

QoreSandboxCallBudget::~QoreSandboxCallBudget() {
    assert(attachments.empty());
    if (charge) {
        charge->deref();
    }
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
            int64 cpu = member->getCPUTimeLimit();
            int64 stack = member->getMaxStackSize();
            int64 memory = member->getMemoryLimit();
            if ((limit > 0 || cpu > 0 || stack > 0 || memory > 0) && !hasBudgetFor(member)) {
                startBudget(tid, member, limit, cpu, stack, memory, owner);
                if (lost_owner) {
                    return;
                }
            }
        }
        return;
    }
    int64 limit = sm->getWallTimeLimit();
    int64 cpu = sm->getCPUTimeLimit();
    int64 stack = sm->getMaxStackSize();
    int64 memory = sm->getMemoryLimit();
    if ((limit > 0 || cpu > 0 || stack > 0 || memory > 0) && !hasBudgetFor(sm)) {
        startBudget(tid, sm, limit, cpu, stack, memory, owner);
    }
}

void QoreSandboxBudgetStack::pushFrame(QoreSandboxCallBudget* budget, const void* owner) {
    frames.push_back({budget, owner, 0, 0, nullptr, false});
    Frame& frame = frames.back();
    if (budget->stack_bytes > 0) {
        frame.applied_stack_limit = qore_sandbox_tighten_stack(budget->stack_bytes, frame.saved_stack_limit);
    }
    // the memory of the storage the thread creates from now on is charged to the call
    if (budget->charge) {
        frame.saved_charge = qore_sb_set_charge(budget->charge);
        frame.charge_set = true;
    }
}

QoreSandboxCallBudget* QoreSandboxBudgetStack::popFrame() {
    assert(!frames.empty());
    Frame& frame = frames.back();
    if (frame.charge_set) {
        qore_sb_set_charge(frame.saved_charge);
    }
    if (frame.applied_stack_limit) {
        qore_sandbox_restore_stack(frame.saved_stack_limit);
    }
    QoreSandboxCallBudget* budget = frame.budget;
    frames.pop_back();
    return budget;
}

//! Creates the charge of a call into a sandbox with a memory limit: its account and the accounts of the enclosing call
//! with a memory limit, if any; may throw std::bad_alloc
static QoreSandboxCharge* qore_sandbox_new_charge(QoreSandboxManager* sm) {
    QoreSandboxCharge* outer = qore_sb_mem.charge;
    std::vector<QoreSandboxMemoryAccount*> accounts;
    accounts.reserve(1 + (outer ? outer->accounts.size() : 0));
    accounts.push_back(sm->getMemoryAccount());
    if (outer) {
        accounts.insert(accounts.end(), outer->accounts.begin(), outer->accounts.end());
    }
    // the references are taken once nothing can fail any more
    QoreSandboxCharge* charge = new QoreSandboxCharge(std::move(accounts));
    for (QoreSandboxMemoryAccount* account : charge->accounts) {
        account->ref();
    }
    return charge;
}

void QoreSandboxBudgetStack::startBudget(int tid, QoreSandboxManager* sm, int64 limit_ms, int64 cpu_limit_ms,
        int64 stack_bytes, int64 memory_limit, const void* owner) {
    unsigned kinds = (limit_ms > 0 ? QSB_LIMIT_WALL_TIME : 0) | (cpu_limit_ms > 0 ? QSB_LIMIT_CPU_TIME : 0)
        | (stack_bytes > 0 ? QSB_LIMIT_STACK : 0) | (memory_limit > 0 ? QSB_LIMIT_MEMORY : 0);
#ifdef DEBUG
    if (qore_sandbox_dbg_fail_budget()) {
        lose(tid, owner, kinds, limit_ms, cpu_limit_ms, stack_bytes, memory_limit);
        return;
    }
#endif
    QoreSandboxCallBudget* budget;
    try {
        QoreSandboxCharge* charge = memory_limit > 0 ? qore_sandbox_new_charge(sm) : nullptr;
        try {
            budget = new QoreSandboxCallBudget(sm, limit_ms, cpu_limit_ms, stack_bytes, memory_limit, charge);
        } catch (...) {
            if (charge) {
                charge->deref();
            }
            throw;
        }
    } catch (std::bad_alloc&) {
        lose(tid, owner, kinds, limit_ms, cpu_limit_ms, stack_bytes, memory_limit);
        return;
    }
    try {
        pushFrame(budget, owner);
    } catch (std::bad_alloc&) {
        budget->deref();
        lose(tid, owner, kinds, limit_ms, cpu_limit_ms, stack_bytes, memory_limit);
        return;
    }
    if (budget->isTimed() && sandbox_watchdog.attach(budget, tid, true) != QoreSandboxWatchdog::ATTACHED) {
        popFrame()->deref();
        lose(tid, owner, kinds, limit_ms, cpu_limit_ms, stack_bytes, memory_limit);
    }
}

void QoreSandboxBudgetStack::attach(int tid, const QoreSandboxBudgetList& budgets, const void* owner) {
    if (lost_owner) {
        return;
    }
    for (QoreSandboxCallBudget* budget : budgets) {
        if (budget->lost_kinds) {
            // a continuation of a call whose limits cannot be enforced; their values are not known here
            lose(tid, owner, budget->lost_kinds, 0, 0, 0, 0);
            return;
        }
#ifdef DEBUG
        if (qore_sandbox_dbg_fail_budget()) {
            lose(tid, owner, budget);
            return;
        }
#endif
        try {
            pushFrame(budget, owner);
        } catch (std::bad_alloc&) {
            lose(tid, owner, budget);
            return;
        }
        budget->ref();
        if (budget->isTimed() && sandbox_watchdog.attach(budget, tid, false) != QoreSandboxWatchdog::ATTACHED) {
            popFrame()->deref();
            lose(tid, owner, budget);
            return;
        }
        // checked after the thread was attached under the watchdog lock: either the watchdog copied the TID and sets
        // the bit, or it had already set the flag, which is seen here
        if (budget->isExpired()) {
            thread_list.requestSandboxLimit(tid);
        }
    }
}

void QoreSandboxBudgetStack::lose(int tid, const void* owner, const QoreSandboxCallBudget* budget) {
    lose(tid, owner, budget->getKinds(), budget->limit_ms, budget->cpu_limit_ms, budget->stack_bytes,
        budget->memory_limit);
}

void QoreSandboxBudgetStack::lose(int tid, const void* owner, unsigned kinds, int64 limit_ms, int64 cpu_limit_ms,
        int64 stack_bytes, int64 memory_limit) {
    assert(!lost_owner);
    assert(kinds);
    lost_owner = owner;
    lost_kinds = kinds;
    lost_limit_ms = limit_ms;
    lost_cpu_limit_ms = cpu_limit_ms;
    lost_stack_bytes = stack_bytes;
    lost_memory_limit = memory_limit;
    // a call with a stack limit that is not enforced must not recurse: the next stack check fails
    if (kinds & QSB_LIMIT_STACK) {
        lost_stack_closed = qore_sandbox_tighten_stack(1, lost_saved_stack_limit) != 0;
    }
    // the next cancellation point fails
    thread_list.requestSandboxLimit(tid);
}

void QoreSandboxBudgetStack::leaveIntern(int tid, const void* owner) {
    bool limited = false;
    if (lost_owner == owner) {
        // the lost call is left first: it was the last one entered
        if (lost_stack_closed) {
            qore_sandbox_restore_stack(lost_saved_stack_limit);
            lost_stack_closed = false;
        }
        lost_owner = nullptr;
        lost_kinds = 0;
        lost_limit_ms = lost_cpu_limit_ms = lost_stack_bytes = lost_memory_limit = 0;
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
    // the limit bit is cleared when the thread has left every expired call and is not over a memory limit any more;
    // it is checked again under the thread list lock, so a budget expiring meanwhile keeps it set
    if ((limited || (thread_list.getCancelBits(tid) & QCR_SANDBOX_LIMIT)) && !lost_owner) {
        thread_list.clearSandboxLimit(tid, *this);
    }
}

void QoreSandboxBudgetStack::capture(QoreSandboxBudgetList& out) const {
    if (lost_owner) {
        // the static budget is referenced like any other, so that dereferencing the list never frees it
        QoreSandboxCallBudget* lost = qore_sandbox_lost_budget(lost_kinds);
        out.push_back(lost);
        lost->ref();
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
        return qore_sandbox_lost_budget(lost_kinds);
    }
    for (const Frame& frame : frames) {
        if (frame.budget->isExpired()) {
            return frame.budget;
        }
    }
    return nullptr;
}

const QoreSandboxCallBudget* QoreSandboxBudgetStack::findCPUBudget() const {
    for (auto i = frames.rbegin(), e = frames.rend(); i != e; ++i) {
        if (i->budget->cpu_limit_ms > 0) {
            return i->budget;
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

void QoreSandboxBudgetStack::raise(const QoreSandboxCallBudget* budget, ExceptionSink* xsink,
        const char* operation) const {
    assert(budget);
    if (budget->lost_kinds) {
        raiseLost(xsink, operation);
        return;
    }
    ReferenceHolder<QoreHashNode> arg(new QoreHashNode(autoTypeInfo), xsink);
    if (budget->expired_kind == QSB_LIMIT_CPU_TIME) {
        arg->setKeyValue("cpu_time_limit", budget->cpu_limit_ms, xsink);
        xsink->raiseExceptionArg("SANDBOX-TIMEOUT", arg.release(), new QoreStringNodeMaker("%s: the sandbox CPU "
            "time limit of " QLLD " ms was exceeded", operation, budget->cpu_limit_ms));
        return;
    }
    assert(budget->expired_kind == QSB_LIMIT_WALL_TIME);
    arg->setKeyValue("wall_time_limit", budget->limit_ms, xsink);
    xsink->raiseExceptionArg("SANDBOX-TIMEOUT", arg.release(), new QoreStringNodeMaker("%s: the sandbox wall time "
        "limit of " QLLD " ms was exceeded", operation, budget->limit_ms));
}

void QoreSandboxBudgetStack::raiseLost(ExceptionSink* xsink, const char* operation) const {
    assert(lost_owner && lost_kinds);
    // the exception names exactly the limits that cannot be enforced, with their values where they are known; the
    // argument has the keys of the configuration (SandboxConfigInfo), with no value where it is not known
    struct LimitDesc {
        unsigned kind;
        const char* name;
        const char* key;
        int64 value;
        const char* unit;
    };
    const LimitDesc limits[] = {
        {QSB_LIMIT_WALL_TIME, "the sandbox wall time limit", "wall_time_limit", lost_limit_ms, "ms"},
        {QSB_LIMIT_CPU_TIME, "the sandbox CPU time limit", "cpu_time_limit", lost_cpu_limit_ms, "ms"},
        {QSB_LIMIT_STACK, "the sandbox stack size limit", "max_stack_size", lost_stack_bytes, "bytes"},
        {QSB_LIMIT_MEMORY, "the sandbox memory limit", "memory_limit", lost_memory_limit, "bytes"},
    };
    unsigned count = 0;
    for (const LimitDesc& limit : limits) {
        if (lost_kinds & limit.kind) {
            ++count;
        }
    }
    ReferenceHolder<QoreHashNode> arg(new QoreHashNode(autoTypeInfo), xsink);
    SimpleRefHolder<QoreStringNode> desc(new QoreStringNodeMaker("%s: ", operation));
    unsigned done = 0;
    for (const LimitDesc& limit : limits) {
        if (!(lost_kinds & limit.kind)) {
            continue;
        }
        if (done) {
            desc->concat(done == count - 1 ? " and " : ", ");
        }
        desc->concat(limit.name);
        if (limit.value) {
            desc->sprintf(" of " QLLD " %s", limit.value, limit.unit);
            arg->setKeyValue(limit.key, limit.value, xsink);
        } else {
            arg->setKeyValue(limit.key, QoreValue(), xsink);
        }
        ++done;
    }
    desc->sprintf(" cannot be enforced for this call: the resources to track %s could not be allocated",
        count > 1 ? "them" : "it");
    xsink->raiseExceptionArg("SANDBOX-ERROR", arg.release(), desc.release());
}

void QoreSandboxBudgetStack::raiseStackLimit(ExceptionSink* xsink, int64 bytes) {
    ReferenceHolder<QoreHashNode> arg(new QoreHashNode(autoTypeInfo), xsink);
    arg->setKeyValue("max_stack_size", bytes, xsink);
    xsink->raiseExceptionArg("STACK-LIMIT-EXCEEDED", arg.release(), new QoreStringNodeMaker("this thread's stack has "
        "exceeded the sandbox stack size limit of " QLLD " bytes", bytes));
}
