/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSandboxLimits.h

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

#ifndef _QORE_INTERN_QORESANDBOXLIMITS_H
#define _QORE_INTERN_QORESANDBOXLIMITS_H

#include <atomic>
#include <map>
#include <vector>

class QoreSandboxManager;
class ExceptionSink;
class QoreSandboxCallBudget;

//! a per-call limit: the wall-time limit (see QoreSandboxManager::setWallTimeLimit())
constexpr unsigned QSB_LIMIT_WALL_TIME = (1u << 0);
//! a per-call limit: the stack size limit (see QoreSandboxManager::setMaxStackSize())
constexpr unsigned QSB_LIMIT_STACK = (1u << 1);

//! Returns the static budget that marks a continuation of a call whose limits cannot be enforced
/** @param kinds the limits that cannot be enforced (@ref QSB_LIMIT_WALL_TIME, @ref QSB_LIMIT_STACK); not 0

    See QoreSandboxBudgetStack: a call whose budget could not be recorded fails closed.
*/
DLLLOCAL QoreSandboxCallBudget* qore_sandbox_lost_budget(unsigned kinds);

//! The budget of one call into a sandbox with a wall-time or stack limit
/** Created when a thread enters a sandbox manager with a per-call limit that has no budget on the thread yet, and
    shared by everything the call continues on other threads (threads it starts, pool tasks, async I/O operations).
    With a wall-time limit, the process-wide watchdog expires it at its deadline and sets the sandbox limit bit of
    every attached thread; the stack limit is applied by each thread from the stack position where it attaches.
    See design/sandbox-resource-limits.md.

    The watchdog's deadline map does not hold a reference: the last dereference removes the budget from the map, and
    the watchdog only acts on a budget that it could reference with tryRef().
*/
class QoreSandboxCallBudget {
public:
    //! Creates a budget with one reference; the deadline is now + limit_ms; references the manager
    /** @param manager the manager whose limits the budget enforces
        @param limit_ms the wall-time limit in milliseconds, or 0 for none
        @param stack_bytes the stack limit in bytes, or 0 for none
    */
    DLLLOCAL QoreSandboxCallBudget(QoreSandboxManager* manager, int64 limit_ms, int64 stack_bytes);

    DLLLOCAL QoreSandboxCallBudget(const QoreSandboxCallBudget&) = delete;
    DLLLOCAL QoreSandboxCallBudget& operator=(const QoreSandboxCallBudget&) = delete;

    DLLLOCAL void ref() {
        refs.fetch_add(1, std::memory_order_relaxed);
    }

    //! References the budget unless its last reference is gone; returns true if referenced
    DLLLOCAL bool tryRef();

    //! Dereferences the budget; the last dereference unschedules and deletes it
    DLLLOCAL void deref();

    //! The manager whose limit this budget enforces
    DLLLOCAL const QoreSandboxManager* getManager() const {
        return manager;
    }

    //! The limit in milliseconds that the budget was created with
    DLLLOCAL int64 getLimitMs() const {
        return limit_ms;
    }

    //! The stack limit in bytes that the budget was created with, 0 for none
    DLLLOCAL int64 getStackBytes() const {
        return stack_bytes;
    }

    //! Returns true if the budget has a wall-time limit and so a deadline that the watchdog enforces
    DLLLOCAL bool isTimed() const {
        return limit_ms > 0;
    }

    //! Returns true if the deadline has passed (set by the watchdog)
    DLLLOCAL bool isExpired() const {
        return expired.load(std::memory_order_acquire);
    }

private:
    friend class QoreSandboxWatchdog;
    friend class QoreSandboxBudgetStack;
    friend QoreSandboxCallBudget* qore_sandbox_lost_budget(unsigned kinds);

    //! the reference count
    std::atomic<int> refs{1};
    //! the manager (referenced), or nullptr for the static budget that marks an unenforceable call
    QoreSandboxManager* manager;
    //! the wall-time limit in milliseconds, 0 for none
    int64 limit_ms;
    //! the stack limit in bytes, 0 for none
    int64 stack_bytes;
    //! the monotonic deadline (q_get_monotonic_us())
    int64 deadline_us;
    //! true once the deadline has passed; set under the watchdog lock
    std::atomic<bool> expired{false};

    //! the TIDs of the threads attached to the budget, one entry per attachment; guarded by the watchdog lock
    std::vector<int> tids;
    //! the budget's entry in the watchdog's deadline map; valid while scheduled is set (watchdog lock)
    std::multimap<int64, QoreSandboxCallBudget*>::iterator pos;
    //! true while the budget is in the watchdog's deadline map; guarded by the watchdog lock
    bool scheduled = false;
    //! for a static budget marking a call whose limits cannot be enforced: those limits; 0 for any other budget
    unsigned lost_kinds = 0;

    //! creates the static budget that marks a call whose limits (\a kinds) cannot be enforced
    DLLLOCAL QoreSandboxCallBudget(unsigned kinds);

    //! Returns the limits of the budget (@ref QSB_LIMIT_WALL_TIME, @ref QSB_LIMIT_STACK)
    DLLLOCAL unsigned getKinds() const {
        return lost_kinds ? lost_kinds : ((limit_ms > 0 ? QSB_LIMIT_WALL_TIME : 0)
            | (stack_bytes > 0 ? QSB_LIMIT_STACK : 0));
    }

    DLLLOCAL ~QoreSandboxCallBudget();
};

//! A list of referenced budgets, captured for a continuation of a call on another thread
typedef std::vector<QoreSandboxCallBudget*> QoreSandboxBudgetList;

//! Dereferences and removes all budgets in the list
DLLLOCAL void qore_sandbox_budgets_deref(QoreSandboxBudgetList& budgets);

//! The call budgets that apply to a thread, innermost last; only used by the thread itself
/** Each frame records the helper that attached it (the owner) and is removed by that helper.  See "What a call is"
    in design/sandbox-resource-limits.md.

    If a frame cannot be recorded (memory or the watchdog thread cannot be allocated), the call fails closed until the
    helper that failed leaves: \c SANDBOX-ERROR, naming the limits that cannot be enforced, is raised at every
    cancellation point and, if the call has a stack limit, at the next stack check.
*/
class QoreSandboxBudgetStack {
public:
    DLLLOCAL QoreSandboxBudgetStack() = default;

    DLLLOCAL ~QoreSandboxBudgetStack();

    DLLLOCAL QoreSandboxBudgetStack(const QoreSandboxBudgetStack&) = delete;
    DLLLOCAL QoreSandboxBudgetStack& operator=(const QoreSandboxBudgetStack&) = delete;

    //! Returns true if no frame applies to the thread
    DLLLOCAL bool empty() const {
        return frames.empty() && !lost_owner;
    }

    //! Starts a new budget for \a sm and for each constituent of a composite manager that has a per-call limit (wall
    //! time or stack) and no budget on this thread yet
    /** @param tid the current thread's TID
        @param sm the manager that governs the code the thread enters
        @param owner the helper that will call leave()
    */
    DLLLOCAL void enter(int tid, QoreSandboxManager* sm, const void* owner);

    //! Attaches budgets captured for a continuation of a call; an expired budget sets the limit bit at once
    DLLLOCAL void attach(int tid, const QoreSandboxBudgetList& budgets, const void* owner);

    //! Removes the frames that \a owner attached, which must be on the top of the stack
    DLLLOCAL void leave(int tid, const void* owner) {
        if (!empty()) {
            leaveIntern(tid, owner);
        }
    }

    //! Adds a reference to each budget of the thread to \a out; may throw std::bad_alloc
    DLLLOCAL void capture(QoreSandboxBudgetList& out) const;

    //! Returns the outermost expired budget, or nullptr if no budget of the thread has expired
    DLLLOCAL const QoreSandboxCallBudget* findExpired() const;

    //! Returns the stack limit in bytes of the frame that set the given stack limit address, or 0 if none did
    /** Used to report which limit a thread exceeded (see check_stack())
    */
    DLLLOCAL int64 findStackLimit(size_t limit) const;

    //! Returns true if the current call's limits cannot be enforced (see the class documentation)
    DLLLOCAL bool isLost() const {
        return lost_owner;
    }

    //! Returns true if the current call's stack limit cannot be enforced, so its stack was closed
    DLLLOCAL bool isStackLost() const {
        return lost_stack_closed;
    }

    //! Raises the exception for a budget returned by findExpired(): \c SANDBOX-TIMEOUT, or \c SANDBOX-ERROR naming
    //! the limits that cannot be enforced
    DLLLOCAL void raise(const QoreSandboxCallBudget* budget, ExceptionSink* xsink, const char* operation) const;

    //! Raises \c SANDBOX-ERROR naming the limits of the call that cannot be enforced; the call must be lost
    DLLLOCAL void raiseLost(ExceptionSink* xsink, const char* operation) const;

    //! Raises \c STACK-LIMIT-EXCEEDED for the sandbox stack limit in bytes, with the limit in the exception argument
    DLLLOCAL static void raiseStackLimit(ExceptionSink* xsink, int64 bytes);

private:
    struct Frame {
        QoreSandboxCallBudget* budget;
        const void* owner;
        //! the thread's stack limit address before the frame was pushed
        size_t saved_stack_limit;
        //! the stack limit address the frame set, or 0 if it did not change the limit
        size_t applied_stack_limit;
    };
    std::vector<Frame> frames;
    //! the helper whose frame could not be recorded, or nullptr; see the class documentation
    const void* lost_owner = nullptr;
    //! the limits of the lost call (@ref QSB_LIMIT_WALL_TIME, @ref QSB_LIMIT_STACK)
    unsigned lost_kinds = 0;
    //! the wall-time limit of the lost call, or 0 if it is not known (a continuation of a lost call)
    int64 lost_limit_ms = 0;
    //! the stack limit of the lost call, or 0 if it is not known (a continuation of a lost call)
    int64 lost_stack_bytes = 0;
    //! the thread's stack limit address before a lost call with a stack limit closed it, see lose()
    size_t lost_saved_stack_limit = 0;
    //! true if lose() closed the thread's stack
    bool lost_stack_closed = false;

    DLLLOCAL bool hasBudgetFor(const QoreSandboxManager* sm) const;
    DLLLOCAL void startBudget(int tid, QoreSandboxManager* sm, int64 limit_ms, int64 stack_bytes, const void* owner);
    //! Pushes a frame for a referenced budget and applies its stack limit; may throw std::bad_alloc (nothing changed)
    DLLLOCAL void pushFrame(QoreSandboxCallBudget* budget, const void* owner);
    //! Pops the top frame and restores the stack limit; returns its budget (the reference passes to the caller)
    DLLLOCAL QoreSandboxCallBudget* popFrame();
    //! Fails the call closed: \a kinds are the limits that cannot be enforced, the values 0 if they are not known
    DLLLOCAL void lose(int tid, const void* owner, unsigned kinds, int64 limit_ms, int64 stack_bytes);
    DLLLOCAL void leaveIntern(int tid, const void* owner);
};

#ifdef DEBUG
//! Makes the next \a count budget recordings fail as if no memory could be allocated (debug builds, for tests)
DLLLOCAL void qore_sandbox_dbg_fail_next_budgets(int count);
#endif

//! Tightens the current thread's stack limit to \a bytes from the current stack position if that is tighter
/** Defined in thread.cpp, which owns the thread's stack guard (check_stack())

    @param saved set to the thread's previous stack limit address
    @return the new stack limit address, or 0 if the limit was not changed
*/
DLLLOCAL size_t qore_sandbox_tighten_stack(int64 bytes, size_t& saved);

//! Restores the current thread's stack limit address saved by qore_sandbox_tighten_stack()
DLLLOCAL void qore_sandbox_restore_stack(size_t saved);

//! Stops the watchdog thread; called by qore_cleanup()
DLLLOCAL void qore_sandbox_watchdog_stop();

//! Starts the watchdog thread again in a child created with fork() if calls with a deadline are pending
DLLLOCAL void qore_sandbox_watchdog_after_fork();

#endif
