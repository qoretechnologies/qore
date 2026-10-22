/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreForkHandlers.h

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

#ifndef _QORE_INTERN_QOREFORKHANDLERS_H
#define _QORE_INTERN_QOREFORKHANDLERS_H

#include <atomic>
#include <condition_variable>
#include <initializer_list>
#include <mutex>
#include <vector>

/** @file QoreForkHandlers.h
    The process-wide locks of the library and the fork handlers that make them usable in a child process created
    with fork(); see design/fork-handlers.md.

    A child process created with fork() runs only the thread that called fork().  A lock that another thread held
    at that moment stays held in the child forever, so the first time the child needs it, it waits forever (the
    signal thread that QoreSignalManager::postFork() starts in the child needs the thread list, for example).  The
    library therefore registers each of its process-wide locks with a fork handler, and one set of pthread_atfork()
    handlers runs them for every fork() in the process, whoever calls it:
    - before fork(), in increasing rank order (the lock order below), each handler of a lock with short critical
      sections acquires the lock, so that no other thread holds it, and the data it protects is consistent, when
      the process is copied
    - after fork(), in the parent, in decreasing rank order, each handler releases what it acquired
    - after fork(), in the child, in decreasing rank order, each handler re-initializes its lock, and the
      conditions bound to it, for the only thread that is left; a lock that the parent's other threads may hold
      for a long time or across blocking operations (module loading, parsing, initialization of compiled code) is
      not acquired before fork(), since the forking thread could wait for it forever, but it is re-initialized in
      the child like the others

    Lock order: a thread that holds a lock of a given rank may only acquire locks of a HIGHER rank (or locks that are
    not registered); the handlers acquire the locks in increasing rank order, so they cannot deadlock with a thread
    that keeps to this order.  Locks of the same rank are never held together.

    A thread must not call fork() while holding a registered lock that is acquired before fork() (it would wait for
    itself), nor while holding the environment lock (see SystemEnvironment).

    Modules register fork handlers for their own process-wide locks with qore_register_fork_handler() (see
    QoreForkHandler.h), at the ranks of QoreForkRank.
*/

//! the ranks of the process-wide locks: the order in which the fork handlers acquire them
/** See the file documentation and design/fork-handlers.md for the rules; the values are spaced so that a lock can be
    inserted between two others.  The comments name what each lock may acquire while it is held (always a lock of a
    higher rank).
*/
enum QoreForkLockRank : int {
    //! the registry of the fork handlers itself
    QFLR_REGISTRY = 0,
    //! the AOT lazy constant initialization lock: runs compiled initializers, which can take any lock; reset only
    //! (never acquired)
    QFLR_AOT_LAZY_CONSTANT = 40,
    //! the module manager (QMM): held across dlopen() and module registration; reset only (never acquired)
    QFLR_MODULE_MANAGER = 50,
    //! module locks held while user code runs or while a thread waits for input (QFR_MODULE_RESET): reset only
    QFLR_MODULE_RESET = 60,
    //! the signal manager (QSM.mutex): starting the signal thread takes the thread list and the signal thread counter
    QFLR_SIGNAL_MANAGER = 100,
    //! the signal manager's thread counter (QSM.tcount)
    QFLR_SIGNAL_THREAD_COUNTER = 110,
    //! the JIT compiler's compile lock: held across LLVM compilation; only tried before fork()
    QFLR_JIT_COMPILE = 150,
    //! the async I/O controller singleton lock: creating the controller takes the controller's own locks and the
    //! Program cleanup callback lock
    QFLR_ASYNC_IO_SINGLETON = 200,
    //! the AOT module state lock: its cancellable waits take the condition waker lock and the thread list
    QFLR_AOT_MODULE_STATE = 300,
    //! the plugin registry: checks for cancellation (thread list) while held
    QFLR_PLUGIN_REGISTRY = 310,
    //! the time zone manager's zone map (QTZM): loading a zone checks for cancellation (thread list)
    QFLR_TIME_ZONES = 320,
    //! the process-wide thread counters (thread_counter, tp_thread_counter): waits check for cancellation
    QFLR_THREAD_COUNTERS = 400,
    //! module locks whose holders check for cancellation (QFR_MODULE_CANCELLABLE): take the thread list
    QFLR_MODULE_CANCELLABLE = 450,
    //! the thread list (thread_list.lck): taken when threads start and exit; takes the condition waker lock and the
    //! stack lock
    QFLR_THREAD_LIST = 500,
    //! the condition waker's wait registry (cond_waker.lck)
    QFLR_COND_WAKER = 510,
    //! the thread stack lock (stack_lck): held only while a thread is created and its stack size set
    QFLR_STACK = 520,
    //! the AOT PC map attach lock: takes the AOT PC map lock
    QFLR_AOT_PC_ATTACH = 900,
    //! leaf locks: caches and registries whose critical sections take no other registered lock (but the ones below),
    //! also the leaf locks of modules (QFR_MODULE_LEAF)
    QFLR_LEAF = 1000,
    //! the process environment (SystemEnvironment): its holders take no other registered lock (but the ones below)
    QFLR_ENVIRONMENT = 1500,
    //! the debug output lock (printd())
    QFLR_DEBUG_OUTPUT = 1900,
    //! the allocation tracking locks (only with ENABLE_ALLOC_TRACKING), which malloc() and free() take
    QFLR_ALLOC_TRACKING = 1950,
};

#if defined(__SANITIZE_THREAD__)
#define QORE_FORK_TSAN 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define QORE_FORK_TSAN 1
#endif
#endif

//! the mode of the striped leaf lock arrays (a lock per stripe, 64 stripes each)
/** They are acquired before fork(), except with ThreadSanitizer: its deadlock detector handles at most 128 locks
    held by one thread, which the stripes alone would exceed, so TSan builds only re-initialize them in the child
*/
#ifdef QORE_FORK_TSAN
#define QFLM_STRIPED QFLM_RESET
#else
#define QFLM_STRIPED QFLM_ACQUIRE
#endif

//! returns the TID of the calling thread, or -1 for a thread unknown to Qore; for the fork handlers
DLLLOCAL int qore_fork_get_tid();

//! returns the number of fork() calls that created this process from the process that loaded the library
/** An object that belongs to the process it was created in (its threads, descriptors and waiters) records this when
    it is created; if it differs later, the object is used in a child process created with fork(), where its threads
    do not exist
*/
DLLLOCAL unsigned qore_fork_get_generation();

//! a process-wide resource that is prepared before fork() and fixed up in the parent and the child after it
/** Constructing the object registers it; destroying it unregisters it.  The handlers run with the registry lock
    held and must not throw, allocate memory in the child, or call anything that takes another registered lock.
*/
class QoreForkHandler {
public:
    //! registers the handler
    /** @param rank the lock order rank
        @param name the name of the resource (for debugging)
    */
    DLLLOCAL QoreForkHandler(int rank, const char* name);

    //! unregisters the handler
    DLLLOCAL virtual ~QoreForkHandler();

    //! called before fork(), in increasing rank order
    DLLLOCAL virtual void prepare() noexcept = 0;

    //! called after fork() in the parent, in decreasing rank order
    DLLLOCAL virtual void parent() noexcept = 0;

    //! called after fork() in the child, in decreasing rank order
    DLLLOCAL virtual void child() noexcept = 0;

    //! returns the rank
    DLLLOCAL int getRank() const {
        return rank;
    }

    //! returns the name
    DLLLOCAL const char* getName() const {
        return name;
    }

    QoreForkHandler(const QoreForkHandler&) = delete;
    QoreForkHandler& operator=(const QoreForkHandler&) = delete;

private:
    int rank;
    const char* name;
};

//! how the fork handlers treat a lock
enum QoreForkLockMode {
    //! the lock is acquired before fork(): its critical sections are short and do not block
    QFLM_ACQUIRE,
    //! the lock is only re-initialized in the child: it can be held for a long time or across blocking operations,
    //! so the data it protects can be inconsistent in the child if another thread held it (the owner of the data
    //! repairs or abandons it)
    QFLM_RESET,
    //! the lock is acquired before fork() only if it is free (it can be held for a long time); in the child it is
    //! re-initialized, and its owner learns whether it was acquired (see wasAcquired()), so that it can stop using
    //! the data the lock protects if another thread held it
    QFLM_TRY,
};

//! the fork handler for a QoreThreadLock (or a QoreRecursiveThreadLock) and the conditions bound to it
class QoreForkThreadLock : public QoreForkHandler {
public:
    //! registers the lock
    /** @param l the lock
        @param rank the lock order rank
        @param name the name of the lock
        @param mode how the lock is treated
        @param recursive true if the lock is a QoreRecursiveThreadLock
        @param conds the conditions used with the lock
    */
    DLLLOCAL QoreForkThreadLock(QoreThreadLock& l, int rank, const char* name, QoreForkLockMode mode = QFLM_ACQUIRE,
            bool recursive = false, std::initializer_list<QoreCondition*> conds = {})
            : QoreForkHandler(rank, name), l(l), mode(mode), recursive(recursive), conds(conds) {
    }

    DLLLOCAL void prepare() noexcept override;
    DLLLOCAL void parent() noexcept override;
    DLLLOCAL void child() noexcept override;

    //! returns true if the lock was acquired before fork()
    /** valid in the child, also in the child handlers of lower ranks, which run after this one
    */
    DLLLOCAL bool wasAcquired() const {
        return acquired;
    }

private:
    QoreThreadLock& l;
    QoreForkLockMode mode;
    bool recursive;
    std::vector<QoreCondition*> conds;
    //! true if prepare() acquired the lock; only accessed by the forking thread with the registry lock held
    bool acquired = false;
};

#ifdef DEBUG
//! a test hook called by the prepare handler, in the forking thread, before each handler is prepared
/** The hook is called with the handler's name and the registry lock held; see the fork unit tests in ql_debug.cpp
*/
DLLLOCAL extern std::atomic<void (*)(const char* name)> qore_fork_prepare_hook;

//! returns the registered handler with the given name, or nullptr; for tests
DLLLOCAL QoreForkHandler* qore_fork_find_handler(const char* name);
#endif

//! the fork handler for a QoreRWLock
class QoreForkRWLock : public QoreForkHandler {
public:
    //! registers the lock
    DLLLOCAL QoreForkRWLock(QoreRWLock& l, int rank, const char* name, QoreForkLockMode mode = QFLM_ACQUIRE)
            : QoreForkHandler(rank, name), l(l), mode(mode) {
    }

    DLLLOCAL void prepare() noexcept override;
    DLLLOCAL void parent() noexcept override;
    DLLLOCAL void child() noexcept override;

private:
    QoreRWLock& l;
    QoreForkLockMode mode;
};

//! the fork handler for a std::mutex and the condition variables bound to it
class QoreForkStdMutex : public QoreForkHandler {
public:
    //! registers the mutex
    DLLLOCAL QoreForkStdMutex(std::mutex& m, int rank, const char* name, QoreForkLockMode mode = QFLM_ACQUIRE,
            std::initializer_list<std::condition_variable*> conds = {})
            : QoreForkHandler(rank, name), m(m), mode(mode), conds(conds) {
    }

    DLLLOCAL void prepare() noexcept override;
    DLLLOCAL void parent() noexcept override;
    DLLLOCAL void child() noexcept override;

    //! returns true if the mutex was acquired before fork()
    /** valid in the child, also in the child handlers of lower ranks, which run after this one
    */
    DLLLOCAL bool wasAcquired() const {
        return acquired;
    }

private:
    std::mutex& m;
    QoreForkLockMode mode;
    std::vector<std::condition_variable*> conds;
    //! true if prepare() acquired the mutex; only accessed by the forking thread with the registry lock held
    bool acquired = false;
};

//! the fork handler for a QoreCounter with static storage duration
/** The count is kept; the waiters are gone in the child
*/
class QoreForkCounter : public QoreForkHandler {
public:
    //! registers the counter
    DLLLOCAL QoreForkCounter(QoreCounter& c, int rank, const char* name) : QoreForkHandler(rank, name), c(c) {
    }

    DLLLOCAL void prepare() noexcept override;
    DLLLOCAL void parent() noexcept override;
    DLLLOCAL void child() noexcept override;

private:
    QoreCounter& c;
};

#endif
