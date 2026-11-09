/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreThreadList.h

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

#ifndef _QORE_QORETHREADLIST_H

#define _QORE_QORETHREADLIST_H

#include <qore/QoreRWLock.h>
#include <qore/qore_thread.h>

#include <qore/QoreCondition.h>

#include <atomic>
#include <chrono>
#include <string>
#include <vector>
#include <pthread.h>
#include <sys/types.h>

//! Thread creation flags (bitfield)
/** @since %Qore 3.0
*/
///@{
//! Default thread behavior
static constexpr int QTF_NONE = 0;
//! Skip Qore stack guard enforcement — for lightweight C++ threads (e.g. dedicated async I/O)
/** The full stack size is usable; check_stack() will never fire.
*/
static constexpr int QTF_NO_STACK_GUARD = (1 << 0);
//! Thread lifecycle managed externally (e.g. ThreadPool) — not counted in thread_counter
/** Threads with this flag use tp_thread_counter instead, so they don't block
    QoreProgramHelper shutdown. A native cleanup worker joins them before releasing
    the counter, including native TLS destructors, after program data destruction.
*/
static constexpr int QTF_EXTERNAL_LIFECYCLE = (1 << 1);
// QTF_NO_SANDBOX_INHERIT (1 << 2) is public; see qore/qore_thread.h
static_assert(!(QTF_NO_SANDBOX_INHERIT & (QTF_NO_STACK_GUARD | QTF_EXTERNAL_LIFECYCLE)),
    "QTF_NO_SANDBOX_INHERIT must not share a bit with another thread creation flag");
///@}

// FIXME: move to config.h or something like that
// not more than this number of threads can be running at the same time
#ifndef MAX_QORE_THREADS
#define MAX_QORE_THREADS 0x2000
#endif

class ThreadData;
class ProgramThreadHub;
class QoreSandboxBudgetStack;
class QoreStringNode;
class QoreCondition;

#define QTS_AVAIL    0
#define QTS_NA       1
#define QTS_ACTIVE   2
#define QTS_RESERVED 3

#if defined(DARWIN) && MAX_QORE_THREADS > 2560 && !defined(__MAC_10_7)
// testing has revealed that darwin's pthread_create will not return an error when more than 2560 threads
// are running, however the threads are not actually started, therefore we set MAX_QORE_THREADS to 2560 on
// Darwin.  This should be much more than any program/script should need (famous last words? :-) )
// this bug is not present on 10.7.3 at least - in 10.7.3 pthread_create() returns an error after 2047
// threads have been created and therefore works reliably
#warning Darwin cannot support more than 2560 threads, MAX_QORE_THREADS set to 2560
#undef MAX_QORE_THREADS
#define MAX_QORE_THREADS 2560
#endif

class tid_node {
public:
   int tid;
   tid_node* next, *prev;

   DLLLOCAL tid_node(int ntid);
   DLLLOCAL ~tid_node();
};

//! cancellation request bit: an explicit request with cancel_thread() / qore_cancel_thread(); raises THREAD-CANCELLED
constexpr unsigned QCR_THREAD_CANCEL = (1u << 0);
//! cancellation request bit: a sandbox call budget of the thread has expired; raises SANDBOX-TIMEOUT
/** Cannot be cleared with clear_thread_cancel(); see design/sandbox-resource-limits.md
*/
constexpr unsigned QCR_SANDBOX_LIMIT = (1u << 1);
//! all cancellation request bits
constexpr unsigned QCR_ALL = QCR_THREAD_CANCEL | QCR_SANDBOX_LIMIT;

// this structure holds all thread data that can be addressed with the qore tid
class ThreadEntry {
public:
    pthread_t ptid;
    tid_node* tidnode;
    ThreadData* thread_data;
    unsigned char status;
    bool joined; // if set to true then pthread_detach should not be called on exit

    //! incremented each time the entry is released, so a waiter can tell a TID's reuse from the thread it waits for
    /** only changed under thread_list.lck */
    uint64_t release_count = 0;

    //! per-thread cooperative cancellation request bits (@ref QCR_THREAD_CANCEL, @ref QCR_SANDBOX_LIMIT), checked at
    //! cancellation points
    /** Any set bit wakes a cancellable wait and makes the next cancellation point evaluate the request; see "Reason
        codes" in design/sandbox-resource-limits.md.
    */
    std::atomic<unsigned> cancel_requested{0};

    //! optional cancellation reason string
    QoreStringNode* cancel_reason = nullptr;

    //! program ID scoping a pending cancellation request, or 0 if the request is unscoped
    /** Set by cancelThread() from the requesting thread's Program context; a scoped request is
        only honored if the target thread is executing in that Program or in a call that
        originated in it (see qore_check_cancel()).

        The scope is evaluated by the target thread itself, because the target's Program-context
        chain is made of stack-allocated ProgramThreadCountContextHelper objects in the target's
        own frames and therefore cannot be walked safely from another thread.
    */
    std::atomic<qore_program_id_t> cancel_scope_pgm_id{0};

    //! the condition the thread is blocked on in a cancellable wait, or nullptr; guarded by the wait registry lock
    /** Registered by qore_cond_wait_cancellable() (QoreCondition::waitWithInterrupt()) before the thread checks
        for a cancellation request and blocks; cleared after it wakes.  See "Condition waits" in
        design/cooperative-cancellation.md.
    */
    QoreCondition* waiting_on = nullptr;
    //! the mutex the thread waits with on waiting_on; guarded by the wait registry lock
    pthread_mutex_t* waiting_mutex = nullptr;
    //! the condition and mutex of a wakeup handed to the condition waker thread; guarded by the wait registry lock
    /** Set while wake_queued or wake_busy is set; the waiter cannot return from its wait (and so the condition and
        mutex stay alive) until both are clear.
    */
    QoreCondition* wake_cond = nullptr;
    pthread_mutex_t* wake_mutex = nullptr;
    //! the next entry in the condition waker's queue, or -1; guarded by the wait registry lock
    int wake_next = -1;
    //! true while the entry is in the condition waker's queue; guarded by the wait registry lock
    bool wake_queued = false;
    //! true while the condition waker is waking the entry's waiter; guarded by the wait registry lock
    bool wake_busy = false;

    //! the thread's wakeup channel (wake_fd) while it is blocked in qore_cancellable_poll(), or -1
    /** Set by the thread itself before it blocks and cleared under thread_list.lck after it wakes;
        cancelThread() / SandboxManager::requestInterrupt() signal it under thread_list.lck.  The same
        seq_cst pairing with the request flag defeats the lost-wakeup race (see
        design/cooperative-cancellation.md, "Wakeup descriptor").
    */
    std::atomic<int> waiting_fd{-1};

    //! the thread's cancellation wakeup channel, or -1 if it has not been created
    /** Created on the thread's first qore_cancellable_poll() and closed when its TID is released or the thread
        leaves a reserved TID; only changed under thread_list.lck.  The descriptor is polled for readability; how
        it is signalled depends on the platform (an EVFILT_USER kqueue on macOS, an eventfd on Linux, a pipe
        elsewhere).
    */
    int wake_fd = -1;
    //! the process that created the wakeup channel; a forked child creates its own instead of sharing it
    pid_t wake_pid = 0;
    //! the nesting depth of qore_cancel_wakeup_register() registrations; only used by the thread itself
    int wake_reg_depth = 0;

    //! the lifecycle event hub of the Program of a background thread while its code runs, or nullptr
    /** Set and cleared by the thread itself under thread_list.lck (QoreThreadList::setLifecycleHub() and
        clearLifecycleHub()); the thread holds a reference to the hub while it is set, so cancelThread() can reference
        it under the lock to publish a cancellation request.  See design/program-thread-lifecycle-events.md.
    */
    ProgramThreadHub* lifecycle_hub = nullptr;
    //! the process-unique ID of the background thread in lifecycle_hub
    int64 lifecycle_thread_id = 0;

#ifdef DEBUG
    //! the number of times a cancellation request woke a wait that the thread had registered (debug builds)
    /** Counted by the wake paths (QoreThreadList::wakeCondWaiter() and QoreThreadList::signalWaitingFd()) when the
        thread has a registered condition wait or wakeup channel; see q_get_cancel_wake_count()
    */
    std::atomic<int64> dbg_cancel_wakes{0};
    //! true while the thread has a registered cancellable wait (debug builds); see q_wait_for_cancellable_wait()
    std::atomic<bool> dbg_in_wait{false};
#endif

    DLLLOCAL void cleanup();

    //! frees the entry of a thread of the parent process in a child created with fork(), where it does not exist
    DLLLOCAL void abandonAfterFork();

    //! Closes the cancellation wakeup channel, if any; must be called with thread_list.lck held
    /** The thread must not be blocked in qore_cancellable_poll(); a thread clears waiting_fd before it returns
        from the wait, so the descriptors cannot be in use by any canceller (cancellers only write while
        holding the lock).
    */
    DLLLOCAL void closeCancelWakeup();

    //! Clears any pending cancellation request and its reason; must be called with thread_list.lck held
    /** The lock serializes the reason string against a concurrent cancelThread() call, which would
        otherwise be able to replace (and deref) it between the read and the deref here.
    */
    DLLLOCAL void clearCancelState(unsigned bits = QCR_ALL) {
        cancel_requested.fetch_and(~bits, std::memory_order_release);
        if (bits & QCR_THREAD_CANCEL) {
            cancel_scope_pgm_id.store(0, std::memory_order_release);
            if (cancel_reason) {
                cancel_reason->deref();
                cancel_reason = nullptr;
            }
        }
    }

    DLLLOCAL void allocate(tid_node* tn, int stat = QTS_NA);

    DLLLOCAL void activate(int tid, pthread_t n_ptid, QoreProgram* p, bool foreign = false,
        int flags = QTF_NONE);

    DLLLOCAL bool active() const {
        return status == QTS_ACTIVE;
    }

    DLLLOCAL bool available() const {
        return status == QTS_AVAIL;
    }
};

class QoreThreadList {
friend class QoreThreadListIterator;
friend class QoreThreadDataHelper;
friend class tid_node;
public:
    DLLLOCAL QoreThreadList() {
    }

    //! Returns the thread entry array; for the condition waker thread, which accesses the wait registry fields only
    DLLLOCAL ThreadEntry* getEntryArray() {
        return entry;
    }

    DLLLOCAL ThreadData* getThreadData(int tid) {
        return entry[tid].active()
            ? entry[tid].thread_data
            : nullptr;
    }

    DLLLOCAL int get(int status = QTS_NA, bool reuse_last = false) {
        int tid = -1;
        AutoLocker al(lck);

        if (current_tid == MAX_QORE_THREADS) {
            int i = last_tid + 1;
            while (true) {
                // never try to assign TID 0
                if (i == MAX_QORE_THREADS) {
                    if (!last_tid) {
                        break;
                    }
                    i = 1;
                }
                assert(i && i < MAX_QORE_THREADS);
                if (entry[i].available()) {
                    tid = last_tid = i;
                    break;
                }
                ++i;
                if (i == last_tid) {
                    break;
                }
            }
            if (tid == -1) {
                return -1;
            }
        } else if (reuse_last && current_tid && entry[current_tid - 1].available()) {
            printd(5, "QoreThreadList::get() reusing TID %d\n", current_tid - 1);
            // re-assign the last assigned TID
            tid = current_tid - 1;
        } else {
            tid = current_tid++;
        }
        assert(entry[tid].available());

        entry[tid].allocate(new tid_node(tid), status);
        ++num_threads;
        //printf("t%d cs=0\n", tid);

        return tid;
    }

    DLLLOCAL int getSignalThreadEntry() {
        AutoLocker al(lck);
        entry[0].allocate(0);
        return 0;
    }

    DLLLOCAL void release(int tid) {
        {
            AutoLocker al(lck);
            releaseIntern(tid);
        }
        notifyJoiners();
    }

    //! wakes the threads in joinThread(); lck must not be held
    DLLLOCAL void notifyJoiners() {
        AutoLocker al(join_lck);
        join_cond.broadcast();
    }

    DLLLOCAL int releaseReserved(int tid) {
        {
            AutoLocker al(lck);
            if (entry[tid].status != QTS_RESERVED) {
                return -1;
            }

            releaseIntern(tid);
        }
        notifyJoiners();
        return 0;
    }

    //! activates the entry of the calling thread and initializes its data
    /** The thread's data for its Program is set up after the lock has been released: it runs the Program's thread
        initialization code (user code), which must not run with the thread list locked
    */
    DLLLOCAL void activate(int tid, pthread_t ptid = pthread_self(), QoreProgram* p = nullptr, bool foreign = false,
            int flags = QTF_NONE);

    DLLLOCAL void setStatus(int tid, int status) {
        AutoLocker al(lck);
        assert(entry[tid].status != status);
        entry[tid].status = status;
    }

    //! Marks a thread as joined so cleanup() will not call pthread_detach()
    DLLLOCAL void setJoined(int tid) {
        AutoLocker al(lck);
        entry[tid].joined = true;
    }

    DLLLOCAL void deleteData(int tid);

    DLLLOCAL void deleteDataRelease(int tid);

    //! waits until the thread with the given TID has left the thread list
    /** @param tid the TID of the thread to wait for
        @param timeout_ms the maximum time to wait in milliseconds; a negative value waits without a limit
        @param xsink for the exception raised when the wait is interrupted or the current thread is the one given

        @return 0 if the thread has terminated (also if no such thread is running), ETIMEDOUT if the timeout expired
        first, or -1 if an exception was raised

        The wait ends when the TID is released, i.e. when the thread is no longer in the list returned by
        thread_list() and no longer counted by num_threads(); a thread that later gets the same TID is not waited for.
    */
    DLLLOCAL int joinThread(int tid, int64 timeout_ms, ExceptionSink* xsink);

    //! returns true if the thread that had the given release count has been released; acquires lck
    DLLLOCAL bool released(int tid, uint64_t count) {
        AutoLocker al(lck);
        return entry[tid].available() || entry[tid].release_count != count;
    }

    DLLLOCAL void deleteDataReleaseSignalThread();

    DLLLOCAL int activateReserved(int tid) {
        AutoLocker al(lck);

        if (entry[tid].status != QTS_RESERVED) {
            return -1;
        }

        entry[tid].activate(tid, pthread_self(), nullptr, true);
        return 0;
    }

    DLLLOCAL unsigned getNumThreads() const {
        return num_threads.load(std::memory_order_relaxed);
    }

    //! acquires the lock before fork() (see QoreForkHandlers.h)
    DLLLOCAL void forkPrepare() {
        lck.lock();
    }

    //! releases the lock in the parent after fork()
    DLLLOCAL void forkParent() {
        lck.unlock();
    }

    //! leaves only the forking thread's entry in a child created with fork(), and initializes the lock again
    /** @param tid the forking thread's TID, or a negative value if it has none
    */
    DLLLOCAL void forkChild(int tid);

    DLLLOCAL QoreHashNode* getAllCallStacks();

    //! Build the callstack-element hash for a frame
    DLLLOCAL static QoreHashNode* getCallStackHash(const QoreStackLocation& loc);

    DLLLOCAL static QoreHashNode* getCallStackHash(qore_call_t type, const std::string& code,
        const QoreProgramLocation& loc);

    DLLLOCAL QoreListNode* getCallStack(const QoreStackLocation* stack_location) const;

    //! Returns the call stack of another thread, or nullptr if it is empty
    /** @param td the thread's data, which the caller keeps in place by holding lck

        Each thread publishes its own stack without any lock; this makes the thread wait at its next pop until the
        walk has finished, so that no location the walk can reach is destroyed underneath it.  Only that thread
        waits, and only if it pops during the walk.  If both are needed, lck is acquired before the thread's
        stack_walk_lck, as here.
    */
    DLLLOCAL QoreListNode* walkCallStack(ThreadData* td) const;

    DLLLOCAL QoreHashNode* getParentCallerLocation(const QoreStackLocation* stack_location, size_t offset) const;

    //! Check if the given thread has cancellation requested (lock-free, atomic read)
    /** seq_cst pairs with seq_cst on the cancel side and is required for the Dekker-style
        lost-wakeup race in qore_cancellable_poll() (where the waiter's store of waiting_fd and load of
        cancel_requested must be in the same total order as the canceller's store of cancel_requested and load of
        waiting_fd).  On x86 this is free; on weak-memory architectures it adds a fence per check, which is
        negligible at the rate cancellation points are traversed.
    */
    DLLLOCAL bool isCancelRequested(int tid) const {
        return getCancelBits(tid);
    }

    //! Returns the pending cancellation request bits of the given thread (lock-free); see isCancelRequested()
    DLLLOCAL unsigned getCancelBits(int tid) const {
        return (tid >= 0 && tid < MAX_QORE_THREADS) ? entry[tid].cancel_requested.load(std::memory_order_seq_cst)
            : 0;
    }

    //! Sets the sandbox limit bit of the given threads and wakes them if they are blocked; acquires lck
    /** Called by the sandbox watchdog when a call budget expires (see design/sandbox-resource-limits.md); a TID
        whose thread has meanwhile left the call (or ended) gets a spurious bit, which the thread clears at its next
        cancellation point (see clearSandboxLimit())
    */
    DLLLOCAL void requestSandboxLimit(const std::vector<int>& tids);

    //! Sets the sandbox limit bit of one thread and wakes it if it is blocked; acquires lck; never throws
    DLLLOCAL void requestSandboxLimit(int tid);

    //! Clears the current thread's sandbox limit bit unless one of its budgets has expired; acquires lck
    /** The budgets are checked under lck: the watchdog sets a budget's expired flag before it takes lck to set the
        bits, so a budget expiring concurrently is either seen here or sets the bit again afterwards.

        @param tid the current thread's TID
        @param budgets the current thread's call budgets

        @return true if the bit was cleared, false if it stays set
    */
    DLLLOCAL bool clearSandboxLimit(int tid, const QoreSandboxBudgetStack& budgets);

    //! Registers the condition and mutex the current thread is about to block on in a cancellable wait
    /** The caller holds \a m and must check for a cancellation request after this returns and before it blocks,
        with \a m still held.

        @return 0 for success, -1 if the condition waker thread could not be started (errno is set)
    */
    DLLLOCAL int registerCondWait(int tid, QoreCondition* cond, pthread_mutex_t* m);

    //! Clears the current thread's registration after its wait has ended; the caller holds \a m
    /** If a wakeup for the thread is still with the condition waker thread, \a m is released while it completes and
        then reacquired, so that the condition and mutex are no longer used by any other thread when this returns.
    */
    DLLLOCAL void unregisterCondWait(int tid, pthread_mutex_t* m);

    //! Wakes the thread if it is blocked, or about to block, in a cancellable condition wait
    /** Called after the request flag has been set.  If the thread's mutex can be taken at once, the thread is not
        between its check for the request and its wait, so a broadcast cannot be lost; otherwise the condition waker
        thread acquires the mutex (which the waiter releases when it blocks) and then broadcasts.  Never blocks.
    */
    DLLLOCAL void wakeCondWaiter(int tid);

    //! Wakes every thread blocked in a cancellable wait (condition or descriptor)
    /** Used by SandboxManager::requestInterrupt().  Spurious wakeups for threads in other programs are harmless
        (they re-check their own program's interrupt state and resume waiting).
    */
    DLLLOCAL void wakeAllWaiters();

    //! Returns the current thread's cancellation wakeup channel, creating it if necessary
    /** @param tid the current thread's TID

        @return the descriptor to poll for readability, or -1 if the channel could not be created (errno is set)

        A channel inherited from the parent process over fork() is abandoned and replaced, so that a child never
        shares its wakeup channel with its parent.  The descriptor is close-on-exec.
    */
    DLLLOCAL int getCancelWakeupFd(int tid);

    //! Signals the wakeup channel of a thread blocked in qore_cancellable_poll(), if any; lck must be held
    DLLLOCAL void signalWaitingFd(int tid);

    //! Registers the current thread's wakeup channel before it blocks in a poll
    /** seq_cst pairs with the seq_cst store of the cancellation flag on the cancel side; the waiter must check
        for cancellation after this store and before it blocks.
    */
    DLLLOCAL void setCurrentWaitingFd(int tid, int fd) {
        assert(tid >= 0 && tid < MAX_QORE_THREADS);
        assert(fd == entry[tid].wake_fd);
        entry[tid].waiting_fd.store(fd, std::memory_order_seq_cst);
#ifdef DEBUG
        dbgNoteWait(tid, true);
#endif
    }

#ifdef DEBUG
    //! Records that a thread has registered or released a cancellable wait, and wakes threads waiting for that
    DLLLOCAL void dbgNoteWait(int tid, bool in_wait);

    //! Waits until the given thread has a registered cancellable wait; returns false on timeout
    DLLLOCAL bool dbgWaitForWait(int tid, int64 timeout_ms);

    //! Requests the cancellation of the given thread while it is blocked in a registered cancellable wait
    /** Returns false if the thread has no registered wait in \a timeout_ms or the request cannot be made
    */
    DLLLOCAL bool dbgCancelInWait(int tid, const char* reason, int64 timeout_ms);

    //! Returns the number of times cancellation requests woke waits of the given thread
    DLLLOCAL int64 dbgGetCancelWakes(int tid) const {
        assert(tid >= 0 && tid < MAX_QORE_THREADS);
        return entry[tid].dbg_cancel_wakes.load();
    }
#endif

    //! Clears the current thread's registered wakeup descriptor under lck
    /** After this returns, no canceller can write to the channel any more, so draining it afterwards leaves it
        empty for the next wait.
    */
    DLLLOCAL void clearCurrentWaitingFd(int tid);

    //! Returns the thread's entry; for the thread's own wakeup registration state
    DLLLOCAL ThreadEntry& getEntry(int tid) {
        assert(tid >= 0 && tid < MAX_QORE_THREADS);
        return entry[tid];
    }

    //! Get a reference to the cancel reason for the given thread, or nullptr if there is none
    /** The reference is acquired under the lock, so the string cannot be replaced and freed by a
        concurrent cancelThread() call while the caller is using it; the caller owns the reference
        returned.
    */
    DLLLOCAL QoreStringNode* getCancelReasonRef(int tid) {
        AutoLocker al(lck);
        QoreStringNode* rv = (tid >= 0 && tid < MAX_QORE_THREADS) ? entry[tid].cancel_reason : nullptr;
        return rv ? rv->stringRefSelf() : nullptr;
    }

    //! Get the program ID scoping the pending cancellation request for the given thread
    /** @return the program ID that the target must be executing in (or under) for the request to
        be honored, or 0 if the request is unscoped

        Acquire pairs with the release store in cancelThread(), which publishes the scope before
        the request flag; a thread that has observed the flag therefore observes the scope.
    */
    DLLLOCAL qore_program_id_t getCancelScopeProgramId(int tid) const {
        return (tid >= 0 && tid < MAX_QORE_THREADS)
            ? entry[tid].cancel_scope_pgm_id.load(std::memory_order_acquire)
            : 0;
    }

    //! Mark a thread entry so that pthread_detach() is not called on exit
    /** Used for the main (initial) thread which is not created by pthread_create()
        and therefore should not be detached — doing so causes ASAN failures on macOS.
    */
    DLLLOCAL void markNoDetach(int tid) {
        AutoLocker al(lck);
        entry[tid].joined = true;
    }

    //! Request cancellation of a thread; acquires lock internally
    /** @param tid the thread to cancel
        @param reason optional reason string included in the \c THREAD-CANCELLED exception
        @param scope_pgm_id the program ID scoping the request, or 0 for an unscoped request

        @return 0 if the request was delivered, -1 if the thread is not active
    */
    DLLLOCAL int cancelThread(int tid, const char* reason, qore_program_id_t scope_pgm_id);

    //! Sets the lifecycle event hub of the calling background thread, so that cancellation requests are published
    /** @param tid the calling thread's TID
        @param hub the hub of the thread's Program; the thread holds a reference until it calls clearLifecycleHub()
        @param thread_id the thread's process-unique ID
        @param reason set to the reason of a request already pending, if it has one
        @param has_reason set to true if \a reason was set

        @return true if a cancellation request is already pending: it was made before the hub was set, so it was not
        published, and the caller publishes it
    */
    DLLLOCAL bool setLifecycleHub(int tid, ProgramThreadHub* hub, int64 thread_id, std::string& reason,
            bool& has_reason);

    //! Clears the lifecycle event hub of the calling background thread once its code has ended
    DLLLOCAL void clearLifecycleHub(int tid);

    //! Clear cancellation for the given thread; acquires lock internally
    DLLLOCAL void clearCancel(int tid);

    //! Drop a cancellation request that the target found to be out of scope; acquires lock internally
    /** The request is only cleared if its scope is unchanged, so that a request delivered after the
        scope was evaluated is left pending for the next cancellation check point instead of being
        silently lost.  A new request with the same scope would be evaluated identically, so
        clearing it is harmless.

        @param tid the thread whose request should be dropped
        @param scope_pgm_id the scope that was evaluated and found not to apply
    */
    DLLLOCAL void dropCancelRequest(int tid, qore_program_id_t scope_pgm_id);

    //! Gets the name of the active thread with the given ID; returns -1 if there is no such thread; acquires lck
    /** Only defined on platforms with thread names (QORE_HAVE_THREAD_NAME)
    */
    DLLLOCAL int getThreadName(int tid, QoreString& str);

protected:
    //! Clears all cancellation state for the given thread; lck must be held
    DLLLOCAL void clearCancelIntern(int tid) {
        entry[tid].clearCancelState(QCR_THREAD_CANCEL);
    }

    // lock for reading the thread list
    mutable QoreThreadLock lck;
    //! guards join_cond; see joinThread()
    /** cannot be lck: a cancellable wait takes lck (qore_check_cancel()) while it holds the mutex it waits with.
        Lock order: join_lck, then lck.  lck must never be held when join_lck is taken.
    */
    QoreThreadLock join_lck;
    //! broadcast with join_lck held, after lck has been released, each time a TID is released; see joinThread()
    QoreCondition join_cond;
    //! changed under the lock, read by getNumThreads() from any thread
    std::atomic<unsigned> num_threads {0};
    ThreadEntry entry[MAX_QORE_THREADS];

    tid_node* tid_head = nullptr,
        * tid_tail = nullptr;

    // current TID to be issued next
    int current_tid = 1;

    // last TID issued to avoid reusing the same TID over and over again
    int last_tid = 0;

#ifdef DEBUG
    //! guards waits for a thread's registered cancellable wait (debug builds; see dbgWaitForWait())
    QoreThreadLock dbg_wait_lck;
    //! signalled when a thread registers a cancellable wait (debug builds)
    QoreCondition dbg_wait_cond;
#endif

    DLLLOCAL void releaseIntern(int tid) {
        // NOTE: cannot safely call printd here, because normally the thread_data has been deleted
        //printf("DEBUG: ThreadList.releaseIntern() TID %d terminated\n", tid);
        entry[tid].cleanup();
        ++entry[tid].release_count;
        if (tid) {
            --num_threads;
        }
    }
};

DLLLOCAL extern QoreThreadList thread_list;

class QoreThreadListIterator : public AutoLocker {
public:
    DLLLOCAL QoreThreadListIterator() : AutoLocker(thread_list.lck) {
    }

    DLLLOCAL bool next() {
        do {
            w = w ? w->next : thread_list.tid_head;
        } while (w && (!w->tid || (thread_list.entry[w->tid].status != QTS_ACTIVE)));

        return (bool)w;
    }

    DLLLOCAL unsigned operator*() const {
        assert(w);
        return w->tid;
    }

protected:
    tid_node* w = nullptr;
};

class QoreThreadDataHelper : public AutoLocker {
public:
    DLLLOCAL QoreThreadDataHelper(int tid) : AutoLocker(thread_list.lck), tid(tid) {
    }

    DLLLOCAL ThreadData* get() {
        if (tid >= 0 && tid < MAX_QORE_THREADS) {
            return thread_list.getThreadData(tid);
        }
        return nullptr;
    }

private:
    int tid;
};
#endif
