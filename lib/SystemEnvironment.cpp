/*
  SystemEnvironment.cpp

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
#include "qore/intern/ql_debug.h"

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <string>

SystemEnvironment SysEnv;

namespace {
// The process environment is protected by two read-write locks (see design/fork-handlers.md, "The environment"):
//
// - the environment change lock: held shared while code reads the environment with the C library directly
//   (QoreEnvironmentReadLock): to pass environ to a new process, or to call a library that reads the environment
//   itself, such as c-ares, OpenSSL or the C library's time functions; such a call can take a long time and can take
//   other locks, among them the lock of the dynamic linker (c-ares loads and unloads libSystem on macOS whenever it
//   creates a channel).  It is held exclusively, before the environment lock, while the environment is changed.
//   fork() does not wait for it: its holders do not change the environment, and the dynamic linker of macOS takes
//   its lock before it runs the fork handlers, so a fork handler that waited for a hold across dlclose() would
//   deadlock.  In the child it is reset.
// - the environment lock: a leaf lock, held shared while the library copies a variable (SystemEnvironment::get(),
//   QoreEnvironmentValue) and exclusively while the environment is changed; its holders only copy strings and change
//   the environment, so the fork handler acquires it exclusively before the process is copied, and the child gets a
//   consistent environment.
//
// Both locks are initialized statically, so they work in code that runs during static initialization, and they are
// never destroyed, so they work in code that runs during static destruction.
pthread_rwlock_t qore_env_change_lock = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t qore_env_lock = PTHREAD_RWLOCK_INITIALIZER;

// how the current thread holds each lock: 0 = not held, > 0 = shared (the nesting depth), < 0 = exclusively (the
// negated nesting depth).  A thread that already holds a lock only counts a nested acquisition, so a lock is never
// acquired twice by one thread: a second shared acquisition can wait forever behind a waiting writer where writers
// are preferred, as on macOS.
thread_local int qore_env_change_depth = 0;
thread_local int qore_env_lock_depth = 0;

void qore_env_rwlock_shared(pthread_rwlock_t& lock, int& depth) {
    if (depth > 0) {
        ++depth;
        return;
    }
    if (depth < 0) {
        --depth;
        return;
    }
    int rc = pthread_rwlock_rdlock(&lock);
    assert(!rc);
    (void)rc;
    depth = 1;
}

void qore_env_rwlock_exclusive(pthread_rwlock_t& lock, int& depth) {
    // a thread holding the lock shared cannot wait for it exclusively; the callers check this first
    assert(depth <= 0);
    if (depth < 0) {
        --depth;
        return;
    }
    int rc = pthread_rwlock_wrlock(&lock);
    assert(!rc);
    (void)rc;
    depth = -1;
}

void qore_env_rwlock_release(pthread_rwlock_t& lock, int& depth) {
    assert(depth);
    if (depth > 1) {
        --depth;
        return;
    }
    if (depth < -1) {
        ++depth;
        return;
    }
    depth = 0;
    // lock objects are released right after calls whose errno the caller reads (fork(), popen(), execvp())
    int saved_errno = errno;
    int rc = pthread_rwlock_unlock(&lock);
    assert(!rc);
    (void)rc;
    errno = saved_errno;
}

// holds the change lock shared (QoreEnvironmentReadLock)
void qore_env_acquire_read() {
    // the change lock is acquired before the environment lock: a thread copying a variable takes no other lock
    assert(qore_env_change_depth || !qore_env_lock_depth);
    qore_env_rwlock_shared(qore_env_change_lock, qore_env_change_depth);
}

void qore_env_release_read() {
    qore_env_rwlock_release(qore_env_change_lock, qore_env_change_depth);
}

// acquires both locks exclusively, or, if the thread already holds the change lock shared, only nests the shared
// hold: the thread cannot wait for the exclusive lock while holding it shared, since it would wait for itself;
// changes to the environment are then refused (see qore_env_held_exclusively())
void qore_env_acquire_exclusive() {
    if (qore_env_change_depth > 0) {
        ++qore_env_change_depth;
        return;
    }
    // the change lock is acquired before the environment lock: a thread copying a variable takes no other lock
    assert(qore_env_change_depth || !qore_env_lock_depth);
    qore_env_rwlock_exclusive(qore_env_change_lock, qore_env_change_depth);
    qore_env_rwlock_exclusive(qore_env_lock, qore_env_lock_depth);
}

void qore_env_release_exclusive() {
    if (qore_env_change_depth > 0) {
        qore_env_rwlock_release(qore_env_change_lock, qore_env_change_depth);
        return;
    }
    qore_env_rwlock_release(qore_env_lock, qore_env_lock_depth);
    qore_env_rwlock_release(qore_env_change_lock, qore_env_change_depth);
}

bool qore_env_held_exclusively() {
    return qore_env_change_depth < 0;
}

//! the fork handler of the environment locks (see QoreForkHandler)
/** Before fork(), the environment lock is acquired exclusively, so that the child gets a consistent copy of the
    environment: it waits only for threads that copy a variable or change the environment.  It ranks last among the
    locks that the fork handlers acquire, since its holders take no other lock.

    The change lock is not acquired: its holders run library code that can wait for locks that the forking thread
    holds while the fork handlers run (the lock of the dynamic linker on macOS, which fork() takes before it runs
    them), and they do not change the environment.  A thread changing the environment acquires the change lock
    first, so while the forking thread holds the environment lock, no other thread can change the environment.

    A thread must not call fork() while it holds the environment lock (inside an AtomicEnvironmentSetter): the fork
    handlers would acquire the other locks while it is held, and a thread that holds one of them while it waits for
    the environment lock would deadlock with the forking thread (with a shared hold, on platforms whose read-write
    locks prefer writers, such as macOS, behind a waiting writer).  In a debug build this is an assertion; otherwise
    the forking thread's hold, which also excludes changes, is kept.  It can call fork() while it holds a
    QoreEnvironmentReadLock.

    In the child, which runs only the thread that called fork(), the holds of all other threads are gone with them,
    so both locks are reset, and the holds of the forking thread are restored, so that the lock objects it holds
    release the locks in the child as in the parent.  The locks are reset by assigning the static initializer: a
    lock that was held cannot be initialized again with pthread_rwlock_init() (macOS refuses it with EBUSY).
*/
class QoreEnvForkHandler : public QoreForkHandler {
public:
    QoreEnvForkHandler() : QoreForkHandler(QFLR_ENVIRONMENT, "environment") {
    }

    void prepare() noexcept override {
        // fork() must not be called with the environment lock held (see above)
        assert(!qore_env_lock_depth);
        if (!qore_env_lock_depth) {
#ifdef DEBUG
            qore_dbg_fork_env_prepare();
#endif
            int rc = pthread_rwlock_wrlock(&qore_env_lock);
            assert(!rc);
            (void)rc;
            acquired = true;
        }
    }

    void parent() noexcept override {
        if (acquired) {
            acquired = false;
            int rc = pthread_rwlock_unlock(&qore_env_lock);
            assert(!rc);
            (void)rc;
        }
    }

    void child() noexcept override {
        acquired = false;
        restore(qore_env_lock, qore_env_lock_depth);
        restore(qore_env_change_lock, qore_env_change_depth);
    }

private:
    //! set by prepare() if it acquired the lock; only accessed by the forking thread, with the registry lock held
    bool acquired = false;

    //! resets a lock in the child and acquires it again as the forking thread held it
    static void restore(pthread_rwlock_t& lock, int depth) {
        static const pthread_rwlock_t unlocked = PTHREAD_RWLOCK_INITIALIZER;
        lock = unlocked;
        if (depth > 0) {
            int rc = pthread_rwlock_rdlock(&lock);
            assert(!rc);
            (void)rc;
        } else if (depth < 0) {
            int rc = pthread_rwlock_wrlock(&lock);
            assert(!rc);
            (void)rc;
        }
    }
};

// holds the environment lock shared for its lifetime, to copy a variable
class QoreEnvSharedHelper {
public:
    QoreEnvSharedHelper() {
        qore_env_rwlock_shared(qore_env_lock, qore_env_lock_depth);
    }

    ~QoreEnvSharedHelper() {
        qore_env_rwlock_release(qore_env_lock, qore_env_lock_depth);
    }

    QoreEnvSharedHelper(const QoreEnvSharedHelper&) = delete;
    QoreEnvSharedHelper& operator=(const QoreEnvSharedHelper&) = delete;
};

// holds both locks exclusively for its lifetime, to change the environment
class QoreEnvExclusiveHelper {
public:
    QoreEnvExclusiveHelper() {
        qore_env_acquire_exclusive();
    }

    ~QoreEnvExclusiveHelper() {
        qore_env_release_exclusive();
    }

    QoreEnvExclusiveHelper(const QoreEnvExclusiveHelper&) = delete;
    QoreEnvExclusiveHelper& operator=(const QoreEnvExclusiveHelper&) = delete;
};

// returns -1 with errno EDEADLK: the current thread holds the lock shared only, so it cannot change the environment
int qore_env_refuse_change() {
    errno = EDEADLK;
    return -1;
}
}

#ifdef DEBUG
bool qore_dbg_env_lock_held_by_current_thread() {
    return qore_env_lock_depth != 0;
}
#endif

// makes the lock usable in child processes created with fork(), by Qore or any other code in the process
static QoreEnvForkHandler qore_env_fork_handler;

SystemEnvironment::SystemEnvironment() {
}

SystemEnvironment::~SystemEnvironment() {
}

int SystemEnvironment::set_intern(const char* name, const char* value, bool overwrite) {
#ifdef HAVE_SETENV
    return setenv(name, value, static_cast<int>(overwrite));
#else
    if (!overwrite && getenv(name)) {
        return -1;
    }
    std::string str = name;
    str += "=";
    str += value;
    return putenv(strdup(str.c_str()));
#endif
}

int SystemEnvironment::get_intern(const char* name, QoreString& str) {
    const char* p = getenv(name);
    //printd(5, "SystemEnvironment::get_intern(name='%s') val=%s\n", name, p ? p : "(null)");
    if (p) {
        str.concat(p);
    }
    return p ? 0 : -1;
}

QoreString* SystemEnvironment::get_intern(const char* name) {
    const char* p = getenv(name);
    return p ? new QoreString(p) : nullptr;
}

QoreStringNode* SystemEnvironment::get_as_string_node_intern(const char* name) {
    const char* p = getenv(name);
    return p ? new QoreStringNode(p) : nullptr;
}

int SystemEnvironment::unset_intern(const char* name) {
#ifdef HAVE_UNSETENV
    return unsetenv(name);
#else
    // FIXME: here we fake it - we don't actually remove the variable from the environment, but we set it to nothing...
    std::string str = name;
    str += "=";
    return putenv(strdup(str.c_str()));
#endif
}

bool SystemEnvironment::value_exists_intern(const char* name) {
    if (!name || !name[0]) {
        return false;
    }
    const char* p = getenv(name);
    return p && p[0];
}

int SystemEnvironment::set(const char* name, const char* value, bool overwrite) {
    QoreEnvExclusiveHelper env_lock;
    if (!qore_env_held_exclusively()) {
        return qore_env_refuse_change();
    }
    return set_intern(name, value, overwrite);
}

int SystemEnvironment::get(const char* name, QoreString& str) {
    QoreEnvSharedHelper env_lock;
    return get_intern(name, str);
}

QoreString* SystemEnvironment::get(const char* name) {
    QoreEnvSharedHelper env_lock;
    return get_intern(name);
}

QoreStringNode* SystemEnvironment::getAsStringNode(const char* name) {
    QoreEnvSharedHelper env_lock;
    return get_as_string_node_intern(name);
}

int SystemEnvironment::unset(const char* name) {
    QoreEnvExclusiveHelper env_lock;
    if (!qore_env_held_exclusively()) {
        return qore_env_refuse_change();
    }
    return unset_intern(name);
}

bool SystemEnvironment::valueExists(const char* name) {
    QoreEnvSharedHelper env_lock;
    return value_exists_intern(name);
}

bool SystemEnvironment::isSet(const char* name) {
    assert(name);
    QoreEnvSharedHelper env_lock;
    return getenv(name) != nullptr;
}

AtomicEnvironmentSetter::AtomicEnvironmentSetter() {
    qore_env_acquire_exclusive();
}

AtomicEnvironmentSetter::~AtomicEnvironmentSetter() {
    qore_env_release_exclusive();
}

int AtomicEnvironmentSetter::set(const char* name, const char* value, bool overwrite) {
    if (!qore_env_held_exclusively()) {
        return qore_env_refuse_change();
    }
    return SystemEnvironment::set_intern(name, value, overwrite);
}

int AtomicEnvironmentSetter::get(const char* name, QoreString& str) {
    return SystemEnvironment::get_intern(name, str);
}

QoreString* AtomicEnvironmentSetter::get(const char* name) {
    return SystemEnvironment::get_intern(name);
}

QoreStringNode* AtomicEnvironmentSetter::getAsStringNode(const char* name) {
    return SystemEnvironment::get_as_string_node_intern(name);
}

int AtomicEnvironmentSetter::unset(const char* name) {
    if (!qore_env_held_exclusively()) {
        return qore_env_refuse_change();
    }
    return SystemEnvironment::unset_intern(name);
}

bool AtomicEnvironmentSetter::valueExists(const char* name) {
    // the lock is already held by this object
    return SystemEnvironment::value_exists_intern(name);
}

QoreEnvironmentReadLock::QoreEnvironmentReadLock() {
    qore_env_acquire_read();
}

QoreEnvironmentReadLock::~QoreEnvironmentReadLock() {
    qore_env_release_read();
}

QoreEnvironmentValue::QoreEnvironmentValue(const char* name) {
    assert(name);
    QoreEnvSharedHelper env_lock;
    const char* p = getenv(name);
    if (p) {
        // copied under the lock; if the copy throws, the lock is released by the helper
        value = p;
        is_set = true;
    }
}
