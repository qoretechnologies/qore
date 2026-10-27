/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
// A recursive lock is recursive however early it is created, and its fork handler initializes it again as a
// recursive lock in a child created before qore_init(), while the library is initialized and after qore_cleanup().
// Exercises the public embedding and fork handler interfaces despite the core build's directory-wide define.
#undef _QORE_LIB_INTERN
#include <qore/Qore.h>
#include <qore/QoreForkHandler.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #condition); _exit(99); \
} } while (false)

// created while the program's static objects are initialized, before qore_init()
static QoreRecursiveThreadLock static_lock;

// the lock whose fork handler is registered before qore_init(); only the thread that calls fork() takes it
static QoreRecursiveThreadLock fork_lock;

static void prepareFork(void* arg) {
}

static void parentFork(void* arg) {
}

static void childFork(void* arg) {
    qore_fork_reinit_recursive_thread_lock(*static_cast<QoreRecursiveThreadLock*>(arg));
}

//! checks that the lock can be taken again by the thread that holds it, and released as often
static void checkRecursive(QoreRecursiveThreadLock& lock, const char* what) {
    lock.lock();
    int rc = lock.trylock();
    if (rc) {
        fprintf(stderr, "%s: a second trylock() by the holder returned %d: the lock is not recursive\n", what, rc);
        _exit(98);
    }
    lock.unlock();
    lock.unlock();
    // and it is free again
    CHECK(!lock.trylock());
    lock.unlock();
}

//! forks; the child checks the lock of the fork handler and runs the given function; returns the child's status
static void forkCase(const char* what, void (*in_child)()) {
    fflush(stdout);
    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) {
        alarm(30);
        checkRecursive(fork_lock, what);
        if (in_child) {
            in_child();
        }
        _exit(0);
    }
    int status;
    pid_t waited;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    CHECK(waited == child);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
        fprintf(stderr, "%s: the child ended with status %d\n", what, status);
        exit(1);
    }
}

static void initAndCleanup() {
    qore_init(QL_MIT, nullptr, false, QLO_DISABLE_SIGNAL_HANDLING);
    checkRecursive(fork_lock, "in a child created before qore_init(), after qore_init()");
    qore_cleanup();
}

int main() {
    checkRecursive(static_lock, "a lock created during static initialization");
    QoreRecursiveThreadLock early_lock;
    checkRecursive(early_lock, "a lock created before qore_init()");
    QoreForkHandlerRegistration* reg = qore_register_fork_handler(QFR_MODULE_LEAF, "fork_before_init_test",
        prepareFork, parentFork, childFork, &fork_lock);
    CHECK(reg);

    forkCase("a child created before qore_init()", initAndCleanup);

    qore_init(QL_MIT, nullptr, false, QLO_DISABLE_SIGNAL_HANDLING);
    forkCase("a child created while the library is initialized", nullptr);
    qore_cleanup();

    forkCase("a child created after qore_cleanup()", nullptr);
    QoreRecursiveThreadLock late_lock;
    checkRecursive(late_lock, "a lock created after qore_cleanup()");

    qore_unregister_fork_handler(reg);
    puts("Passed 6 recursive lock cases");
    return 0;
}
