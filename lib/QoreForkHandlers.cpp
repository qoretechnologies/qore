/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreForkHandlers.cpp

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

#include <qore/Qore.h>
#include "qore/intern/QoreForkHandlers.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <new>
#include <pthread.h>
#include <unistd.h>

DLLLOCAL const pthread_mutexattr_t* qore_recursive_mutex_attr();

namespace {
//! incremented in each child process created with fork(); see qore_fork_get_generation()
std::atomic<unsigned> qore_fork_generation{0};

//! true while the child handlers of a fork() run in the child; see qore_fork_child_exit_quickly()
std::atomic<bool> qore_fork_in_child_handlers{false};

//! the fork generation of the child process in which qore_fork_child_exit_quickly() was called, 0 if none
/** The generation of the parent of all forks is 0, so the request is never in effect there; a child created by a
    later fork() has another generation
*/
std::atomic<unsigned> qore_fork_quick_exit_generation{0};

//! the registered fork handlers, sorted by rank (handlers of one rank in registration order)
class QoreForkRegistry {
public:
    QoreForkRegistry();

    void add(QoreForkHandler* h) {
        LockHolder lh(m);
        auto i = std::upper_bound(handlers.begin(), handlers.end(), h->getRank(),
            [](int rank, const QoreForkHandler* e) { return rank < e->getRank(); });
        handlers.insert(i, h);
    }

    void remove(QoreForkHandler* h) noexcept {
        LockHolder lh(m);
        auto i = std::find(handlers.begin(), handlers.end(), h);
        if (i != handlers.end()) {
            handlers.erase(i);
        }
    }

    //! acquires the registry lock and prepares every handler in increasing rank order
    void prepare() noexcept {
        int rc = pthread_mutex_lock(&m);
        assert(!rc);
        (void)rc;
        for (QoreForkHandler* h : handlers) {
#ifdef DEBUG
            if (void (*hook)(const char*) = qore_fork_prepare_hook.load()) {
                hook(h->getName());
            }
#endif
            h->prepare();
        }
    }

#ifdef DEBUG
    QoreForkHandler* find(const char* name) {
        LockHolder lh(m);
        for (QoreForkHandler* h : handlers) {
            if (!strcmp(h->getName(), name)) {
                return h;
            }
        }
        return nullptr;
    }
#endif

    //! releases the handlers in decreasing rank order and then the registry lock
    void parent() noexcept {
        for (auto i = handlers.rbegin(), e = handlers.rend(); i != e; ++i) {
            (*i)->parent();
        }
        int rc = pthread_mutex_unlock(&m);
        assert(!rc);
        (void)rc;
    }

    //! re-initializes the handlers in decreasing rank order and then the registry lock (only this thread is left)
    void child() noexcept {
        qore_fork_generation.fetch_add(1, std::memory_order_relaxed);
        qore_fork_in_child_handlers.store(true, std::memory_order_relaxed);
        for (auto i = handlers.rbegin(), e = handlers.rend(); i != e; ++i) {
            (*i)->child();
        }
        qore_fork_in_child_handlers.store(false, std::memory_order_relaxed);
        // held by this thread since prepare(); a mutex held by a thread of the parent cannot be unlocked in the
        // child on every platform, so it is reset to the static initializer (see QoreThreadLock::reinitAfterFork())
        static const pthread_mutex_t unlocked = PTHREAD_MUTEX_INITIALIZER;
        m = unlocked;
        int rc = pthread_mutex_init(&m, nullptr);
        assert(!rc);
        (void)rc;
    }

private:
    //! holds the registry lock for its lifetime
    class LockHolder {
    public:
        LockHolder(pthread_mutex_t& m) : m(m) {
            int rc = pthread_mutex_lock(&m);
            assert(!rc);
            (void)rc;
        }

        ~LockHolder() {
            int rc = pthread_mutex_unlock(&m);
            assert(!rc);
            (void)rc;
        }

        LockHolder(const LockHolder&) = delete;
        LockHolder& operator=(const LockHolder&) = delete;

    private:
        pthread_mutex_t& m;
    };

    //! the registry lock (rank QFLR_REGISTRY): taken first before fork(), released last after it
    pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    std::vector<QoreForkHandler*> handlers;
};

//! returns the registry, which is never destroyed: handlers with static storage duration unregister themselves
//! while static objects are destroyed, in any order
QoreForkRegistry& qore_fork_registry() {
    static QoreForkRegistry* registry = new QoreForkRegistry;
    return *registry;
}

extern "C" void qore_fork_prepare() {
    qore_fork_registry().prepare();
}

extern "C" void qore_fork_parent() {
    qore_fork_registry().parent();
}

extern "C" void qore_fork_child() {
    qore_fork_registry().child();
}

QoreForkRegistry::QoreForkRegistry() {
    // the child handlers of recursive locks that do not ask for the attributes themselves (as the lazy constant lock
    // of the AOT runtime) use ma_recursive: it is initialized before the first handler can run
    (void)qore_recursive_mutex_attr();
#ifdef HAVE_FORK
    // the registry is created by the first handler, while the library's static objects are initialized; the
    // allocator has registered its fork handlers by then, so its prepare handler runs after ours (prepare handlers
    // run in the reverse order of their registration), and a thread holding one of our locks can still allocate
    // memory while our prepare handler waits for it
    int rc = pthread_atfork(qore_fork_prepare, qore_fork_parent, qore_fork_child);
    assert(!rc);
    (void)rc;
#endif
}
}

#ifdef DEBUG
std::atomic<void (*)(const char*)> qore_fork_prepare_hook{nullptr};

QoreForkHandler* qore_fork_find_handler(const char* name) {
    return qore_fork_registry().find(name);
}
#endif

unsigned qore_fork_get_generation() {
    return qore_fork_generation.load(std::memory_order_relaxed);
}

void qore_fork_child_exit_quickly() {
    // only a child handler of the current fork() can request it, so the request is never in effect in the parent
    if (!qore_fork_in_child_handlers.load(std::memory_order_relaxed)) {
        return;
    }
    qore_fork_quick_exit_generation.store(qore_fork_generation.load(std::memory_order_relaxed),
        std::memory_order_relaxed);
}

bool qore_fork_child_exits_quickly() {
    unsigned gen = qore_fork_quick_exit_generation.load(std::memory_order_relaxed);
    return gen && gen == qore_fork_generation.load(std::memory_order_relaxed);
}

void qore_fork_child_quick_exit(int rc) {
    if (qore_fork_child_exits_quickly()) {
        // no atexit() handler and no static destructor runs; the buffered output of the standard I/O streams is
        // written, as exit() would
        fflush(nullptr);
        _exit(rc);
    }
}

QoreForkHandler::QoreForkHandler(int rank, const char* name) : rank(rank), name(name) {
    qore_fork_registry().add(this);
}

QoreForkHandler::~QoreForkHandler() {
    qore_fork_registry().remove(this);
}

// the ranks that modules use are the library's
static_assert(static_cast<int>(QFR_MODULE_RESET) == static_cast<int>(QFLR_MODULE_RESET), "rank mismatch");
static_assert(static_cast<int>(QFR_MODULE_CANCELLABLE) == static_cast<int>(QFLR_MODULE_CANCELLABLE),
    "rank mismatch");
static_assert(static_cast<int>(QFR_MODULE_LEAF) == static_cast<int>(QFLR_LEAF), "rank mismatch");

//! the fork handlers that a module registered with qore_register_fork_handler()
struct QoreForkHandlerRegistration : public QoreForkHandler {
    DLLLOCAL QoreForkHandlerRegistration(int rank, const char* name, q_fork_handler_t prepare_handler,
            q_fork_handler_t parent_handler, q_fork_handler_t child_handler, void* arg)
            : QoreForkHandler(rank, name), prepare_handler(prepare_handler), parent_handler(parent_handler),
            child_handler(child_handler), arg(arg) {
    }

    DLLLOCAL void prepare() noexcept override {
        if (prepare_handler) {
            prepare_handler(arg);
        }
    }

    DLLLOCAL void parent() noexcept override {
        if (parent_handler) {
            parent_handler(arg);
        }
    }

    DLLLOCAL void child() noexcept override {
        if (child_handler) {
            child_handler(arg);
        }
    }

    q_fork_handler_t prepare_handler;
    q_fork_handler_t parent_handler;
    q_fork_handler_t child_handler;
    void* arg;
};

QoreForkHandlerRegistration* qore_register_fork_handler(int rank, const char* name, q_fork_handler_t prepare,
        q_fork_handler_t parent, q_fork_handler_t child, void* arg) {
    // the registry itself has the lowest rank
    assert(rank > QFLR_REGISTRY);
    assert(name);
    try {
        return new QoreForkHandlerRegistration(rank, name, prepare, parent, child, arg);
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

void qore_unregister_fork_handler(QoreForkHandlerRegistration* reg) {
    delete reg;
}

void qore_fork_prepare_thread_lock(void* lock) {
    static_cast<QoreThreadLock*>(lock)->lock();
}

void qore_fork_parent_thread_lock(void* lock) {
    static_cast<QoreThreadLock*>(lock)->unlock();
}

void qore_fork_child_thread_lock(void* lock) {
    static_cast<QoreThreadLock*>(lock)->reinitAfterFork();
}

void qore_fork_reinit_recursive_thread_lock(QoreRecursiveThreadLock& lock) {
    lock.reinitAfterFork(qore_recursive_mutex_attr());
}

void QoreForkThreadLock::prepare() noexcept {
    if (mode == QFLM_ACQUIRE) {
        l.lock();
        acquired = true;
    } else if (mode == QFLM_TRY) {
        acquired = !l.trylock();
    }
}

void QoreForkThreadLock::parent() noexcept {
    if (acquired) {
        acquired = false;
        l.unlock();
    }
}

void QoreForkThreadLock::child() noexcept {
    // the waiters are gone with their threads
    for (QoreCondition* c : conds) {
        c->reinitAfterFork();
    }
    // held by this thread since prepare(), or by a thread that no longer exists, or not held at all
    l.reinitAfterFork(recursive ? qore_recursive_mutex_attr() : nullptr);
}

void QoreForkRWLock::prepare() noexcept {
    // a QoreRWLock cannot be tried exclusively without waiting for its readers, so QFLM_TRY is treated as
    // QFLM_RESET
    if (mode == QFLM_ACQUIRE) {
        l.wrlock();
    }
}

void QoreForkRWLock::parent() noexcept {
    if (mode == QFLM_ACQUIRE) {
        l.unlock();
    }
}

void QoreForkRWLock::child() noexcept {
    l.reinitAfterFork();
}

void QoreForkStdMutex::prepare() noexcept {
    if (mode == QFLM_ACQUIRE) {
        m.lock();
        acquired = true;
    } else if (mode == QFLM_TRY) {
        acquired = m.try_lock();
    }
}

void QoreForkStdMutex::parent() noexcept {
    if (acquired) {
        acquired = false;
        m.unlock();
    }
}

void QoreForkStdMutex::child() noexcept {
    // the waiters are gone with their threads: the objects are created again in place; the old ones are not
    // destroyed, since destroying a condition variable waits for its waiters
    for (std::condition_variable* c : conds) {
        new (c) std::condition_variable();
    }
    new (&m) std::mutex();
}

void QoreForkCounter::prepare() noexcept {
    c.lockForFork();
}

void QoreForkCounter::parent() noexcept {
    c.unlockForFork();
}

void QoreForkCounter::child() noexcept {
    c.reinitAfterFork();
}
