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

#include <atomic>
#include <pthread.h>

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
///@}

// FIXME: move to config.h or something like that
// not more than this number of threads can be running at the same time
#ifndef MAX_QORE_THREADS
#define MAX_QORE_THREADS 0x2000
#endif

class ThreadData;
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

// this structure holds all thread data that can be addressed with the qore tid
class ThreadEntry {
public:
    pthread_t ptid;
    tid_node* tidnode;
    ThreadData* thread_data;
    unsigned char status;
    bool joined; // if set to true then pthread_detach should not be called on exit

    //! per-thread cooperative cancellation flag (set by cancel_thread(), checked at cancellation points)
    std::atomic<bool> cancel_requested{false};

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
    std::atomic<unsigned> cancel_scope_pgm_id{0};

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

    DLLLOCAL void cleanup();

    //! Clears any pending cancellation request and its reason; must be called with thread_list.lck held
    /** The lock serializes the reason string against a concurrent cancelThread() call, which would
        otherwise be able to replace (and deref) it between the read and the deref here.
    */
    DLLLOCAL void clearCancelState() {
        cancel_requested.store(false, std::memory_order_release);
        cancel_scope_pgm_id.store(0, std::memory_order_release);
        if (cancel_reason) {
            cancel_reason->deref();
            cancel_reason = nullptr;
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
        AutoLocker al(lck);
        releaseIntern(tid);
    }

    DLLLOCAL int releaseReserved(int tid) {
        AutoLocker al(lck);
        if (entry[tid].status != QTS_RESERVED) {
            return -1;
        }

        releaseIntern(tid);
        return 0;
    }

    DLLLOCAL void activate(int tid, pthread_t ptid = pthread_self(), QoreProgram* p = nullptr, bool foreign = false,
            int flags = QTF_NONE) {
        AutoLocker al(lck);
        entry[tid].activate(tid, ptid, p, foreign, flags);
    }

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
        return num_threads;
    }

    DLLLOCAL unsigned cancelAllActiveThreads();

    DLLLOCAL QoreHashNode* getAllCallStacks();

    //! Build the callstack-element hash for a frame; when `override_loc` is non-null it
    //! replaces the frame's own location (used to repair AOT call-site locations via the
    //! lazy PC->loc registry when the eager value is stale/aggregate).
    DLLLOCAL static QoreHashNode* getCallStackHash(const QoreStackLocation& loc,
        const QoreProgramLocation* override_loc = nullptr);

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
    /** seq_cst pairs with the seq_cst store in cancelThread(); on x86 this is free, on weak-memory architectures
        it adds a fence per check, which is negligible at the rate cancellation points are traversed.
    */
    DLLLOCAL bool isCancelRequested(int tid) const {
        return tid >= 0 && tid < MAX_QORE_THREADS && entry[tid].cancel_requested.load(std::memory_order_seq_cst);
    }

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

    //! Wakes every thread blocked in a cancellable condition wait
    /** Used by SandboxManager::requestInterrupt().  Spurious wakeups for threads in other programs are harmless
        (they re-check their own program's interrupt state and resume waiting).
    */
    DLLLOCAL void wakeAllWaiters();


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
    DLLLOCAL unsigned getCancelScopeProgramId(int tid) const {
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
    DLLLOCAL int cancelThread(int tid, const char* reason, unsigned scope_pgm_id);

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
    DLLLOCAL void dropCancelRequest(int tid, unsigned scope_pgm_id);

protected:
    //! Clears all cancellation state for the given thread; lck must be held
    DLLLOCAL void clearCancelIntern(int tid) {
        entry[tid].clearCancelState();
    }

    // lock for reading the thread list
    mutable QoreThreadLock lck;
    unsigned num_threads = 0;
    ThreadEntry entry[MAX_QORE_THREADS];

    tid_node* tid_head = nullptr,
        * tid_tail = nullptr;

    // current TID to be issued next
    int current_tid = 1;

    // last TID issued to avoid reusing the same TID over and over again
    int last_tid = 0;

    bool exiting = false;

    DLLLOCAL void releaseIntern(int tid) {
        // NOTE: cannot safely call printd here, because normally the thread_data has been deleted
        //printf("DEBUG: ThreadList.releaseIntern() TID %d terminated\n", tid);
        entry[tid].cleanup();
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
