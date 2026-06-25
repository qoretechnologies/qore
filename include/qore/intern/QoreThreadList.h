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
    QoreProgramHelper shutdown. They are waited on after program data destruction.
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

    //! the condition the thread is currently blocked on, or nullptr if not blocked
    /** Set by QoreCondition::waitWithInterrupt() before sleeping and cleared after waking,
        so that cancelThread() / SandboxManager::requestInterrupt() can wake the thread directly
        via QoreCondition::broadcast() instead of relying on periodic polling.

        Lifetime: the pointer is only read by code that has either set the per-thread cancel flag
        (paired with the waiter's flag-recheck after the seq_cst register) or holds thread_list.lck
        with the entry active.  The Qore object owning the QoreCondition cannot be freed while a
        thread is in waitWithInterrupt() (the caller holds a reference for the duration of the call),
        so a non-null pointer observed under those conditions is always live for at least a brief
        broadcast() call.
    */
    std::atomic<QoreCondition*> waiting_on{nullptr};

    DLLLOCAL void cleanup();

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
    // lock for reading / writing call stacks externally
    /** if both lck and stack_lck are grabbed concurrently (for example, when all threads stacks are read externally),
        then first lck must be acquired, and then stack_lck
    */
    mutable QoreRWLock stack_lck;

    DLLLOCAL QoreThreadList() {
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

    DLLLOCAL QoreHashNode* getParentCallerLocation(const QoreStackLocation* stack_location, size_t offset) const;

    //! Check if the given thread has cancellation requested (lock-free, atomic read)
    /** seq_cst pairs with seq_cst on the cancel side and is required for the Dekker-style
        lost-wakeup race in QoreCondition::waitWithInterrupt (where the waiter's store of
        waiting_on and load of cancel_requested must be in the same total order as the
        canceller's store of cancel_requested and load of waiting_on).  On x86 this is free;
        on weak-memory architectures it adds a fence per check, which is negligible at the
        rate cancellation points are traversed.
    */
    DLLLOCAL bool isCancelRequested(int tid) const {
        return tid >= 0 && tid < MAX_QORE_THREADS && entry[tid].cancel_requested.load(std::memory_order_seq_cst);
    }

    //! Register the QoreCondition the current thread is about to block on (for waitWithInterrupt)
    /** seq_cst pairs with seq_cst on the cancel side; see clearCurrentWaitingOn() for the lock-free
        rationale.  This store is lock-free — registration races against cancellation are resolved
        by the post-store re-check in QoreCondition::waitWithInterrupt.  See
        design/cooperative-cancellation.md.
    */
    DLLLOCAL void setCurrentWaitingOn(QoreCondition* cond) {
        int tid = q_gettid();
        if (tid >= 0 && tid < MAX_QORE_THREADS) {
            entry[tid].waiting_on.store(cond, std::memory_order_seq_cst);
        }
    }

    //! Clear the current thread's waiting_on slot under lck (for QoreCondition::waitWithInterrupt)
    /** Serializes against cancelThread() / wakeAllWaiters() so that any cond pointer those
        observed is still in use by us at the time of broadcast (and therefore the cond's
        owning object is still alive).  See design/cooperative-cancellation.md.
    */
    DLLLOCAL void clearCurrentWaitingOn();

    //! Broadcast to every QoreCondition that any active thread is currently waiting on
    /** Used by SandboxManager::requestInterrupt() to wake waiters out of waitWithInterrupt().
        Spurious wakeups for threads in other programs are harmless (they re-check their own
        program's interrupt state and resume waiting).
    */
    DLLLOCAL void wakeAllWaiters();

    //! Get the cancel reason for the given thread (caller must hold lck or be the owning thread)
    DLLLOCAL QoreStringNode* getCancelReason(int tid) const {
        return (tid >= 0 && tid < MAX_QORE_THREADS) ? entry[tid].cancel_reason : nullptr;
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
    DLLLOCAL int cancelThread(int tid, const char* reason);

    //! Clear cancellation for the given thread
    DLLLOCAL void clearCancel(int tid);

protected:
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
    DLLLOCAL QoreThreadListIterator(bool access_stack = false) : AutoLocker(thread_list.lck),
            access_stack(access_stack) {
        if (access_stack) {
            // grab the call stack write lock to get exclusive access to all thread stacks
            thread_list.stack_lck.wrlock();
        }
    }

    DLLLOCAL ~QoreThreadListIterator() {
        if (access_stack) {
            // release the call stack write lock
            thread_list.stack_lck.unlock();
        }
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
    bool access_stack;
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
