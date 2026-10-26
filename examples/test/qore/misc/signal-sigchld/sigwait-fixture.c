/* Copyright (C) 2026 Qore Technologies, s.r.o.
   SPDX-License-Identifier: LGPL-2.1-or-later

   Test interposer for signals-sigchld.qtest: records the signal sets that the process waits for with sigwait()
   (macOS) and sigtimedwait() (Linux), which only the signal thread of the library calls.  At exit, it writes to the
   file named by SIGWAIT_FIXTURE_REPORT the number of waits and the number of waits whose set included SIGCHLD.
*/
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#ifndef __APPLE__
#include <dlfcn.h>
#endif

static atomic_int waits;
static atomic_int sigchld_waits;

static void record(const sigset_t* set) {
    atomic_fetch_add(&waits, 1);
    if (set && sigismember(set, SIGCHLD) == 1) {
        atomic_fetch_add(&sigchld_waits, 1);
    }
}

#ifdef __APPLE__
static int fixture_sigwait(const sigset_t* set, int* sig) {
    record(set);
    return sigwait(set, sig);
}

__attribute__((used, section("__DATA,__interpose"))) static const struct {
    const void* replacement;
    const void* replaced;
} fixture_interpose[] = {
    {(const void*)fixture_sigwait, (const void*)sigwait},
};
#else
int sigtimedwait(const sigset_t* set, siginfo_t* info, const struct timespec* timeout) {
    static int (*real)(const sigset_t*, siginfo_t*, const struct timespec*);
    if (!real) {
        real = (int (*)(const sigset_t*, siginfo_t*, const struct timespec*))dlsym(RTLD_NEXT, "sigtimedwait");
        if (!real) {
            errno = ENOSYS;
            return -1;
        }
    }
    record(set);
    return real(set, info, timeout);
}

int sigwait(const sigset_t* set, int* sig) {
    static int (*real)(const sigset_t*, int*);
    if (!real) {
        real = (int (*)(const sigset_t*, int*))dlsym(RTLD_NEXT, "sigwait");
        if (!real) {
            return ENOSYS;
        }
    }
    record(set);
    return real(set, sig);
}
#endif

__attribute__((destructor)) static void report(void) {
    const char* path = getenv("SIGWAIT_FIXTURE_REPORT");
    if (!path || !*path) {
        return;
    }
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
    if (fd == -1) {
        return;
    }
    char buf[64];
    int len = snprintf(buf, sizeof buf, "%d %d\n", atomic_load(&waits), atomic_load(&sigchld_waits));
    if (len > 0) {
        (void)!write(fd, buf, (size_t)len);
    }
    close(fd);
}
