/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
// Exercise the public embedding interface despite the core build's directory-wide define.
#undef _QORE_LIB_INTERN
#include <qore/Qore.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <initializer_list>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #condition); _exit(99); \
} } while (false)

enum class Fault { None, CheckInterrupted, OpenInterrupted, DupInterrupted, CheckFailed, OpenFailed,
    DupFailed, CloseFailed, DifferentFd };

// Each forked child sets these before startup; no test state is shared with another thread.
static Fault fault = Fault::None;
static bool injected = false;
static int extra_fd = -1;

static int test_fcntl(int fd, int command) {
    if (!injected && (fault == Fault::CheckInterrupted || fault == Fault::CheckFailed)) {
        injected = true;
        errno = fault == Fault::CheckInterrupted ? EINTR : EIO;
        return -1;
    }
    return fcntl(fd, command);
}

static int test_open(const char* path, int flags) {
    if (!injected && (fault == Fault::OpenInterrupted || fault == Fault::OpenFailed)) {
        injected = true;
        errno = fault == Fault::OpenInterrupted ? EINTR : EACCES;
        return -1;
    }
    int fd = open(path, flags);
    if (fd >= 0 && (fault == Fault::DifferentFd || fault == Fault::DupInterrupted
            || fault == Fault::DupFailed || fault == Fault::CloseFailed)) {
        extra_fd = fcntl(fd, F_DUPFD, 10);
        CHECK(extra_fd >= 10);
        CHECK(!close(fd));
        return extra_fd;
    }
    return fd;
}

static int test_dup2(int old_fd, int new_fd) {
    if (!injected && (fault == Fault::DupInterrupted || fault == Fault::DupFailed)) {
        injected = true;
        errno = fault == Fault::DupInterrupted ? EINTR : EMFILE;
        return -1;
    }
    return dup2(old_fd, new_fd);
}

static int test_close(int fd) {
    int result = close(fd);
    if (!injected && fault == Fault::CloseFailed) {
        injected = true;
        errno = EINTR;
        return -1;
    }
    return result;
}

// Intercept only the private header's syscalls. qore_init() below uses the real
// library implementation, so embedding coverage cannot pass through a test substitute.
#define fcntl test_fcntl
#define open test_open
#define dup2 test_dup2
#define close test_close
#include <qore/intern/qore_stdio.h>
#undef fcntl
#undef open
#undef dup2
#undef close

static void checkNullDescriptors(int mask) {
    struct stat null_stat;
    CHECK(!stat("/dev/null", &null_stat));
    for (int fd = 0; fd < 3; ++fd) {
        CHECK(fcntl(fd, F_GETFD) >= 0);
        if (!(mask & (1 << fd))) {
            continue;
        }
        CHECK(fcntl(fd, F_GETFD) == 0);
        CHECK((fcntl(fd, F_GETFL) & O_ACCMODE) == O_RDWR);
        struct stat actual;
        CHECK(!fstat(fd, &actual));
        CHECK(actual.st_dev == null_stat.st_dev && actual.st_ino == null_stat.st_ino);
        char byte;
        CHECK(read(fd, &byte, 1) == 0);
        CHECK(write(fd, "x", 1) == 1);
    }
    int next = open("/dev/null", O_RDONLY);
    CHECK(next > 2);
    CHECK(!close(next));
}

static void checkPreservation() {
    struct stat before[3];
    int flags[3];
    int status[3];
    CHECK(!fcntl(1, F_SETFD, FD_CLOEXEC));
    CHECK(!fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK));
    for (int fd = 0; fd < 3; ++fd) {
        CHECK(!fstat(fd, &before[fd]));
        flags[fd] = fcntl(fd, F_GETFD);
        status[fd] = fcntl(fd, F_GETFL);
    }
    qore_ensure_standard_fds();
    qore_ensure_standard_fds();
    for (int fd = 0; fd < 3; ++fd) {
        struct stat after;
        CHECK(!fstat(fd, &after));
        CHECK(before[fd].st_dev == after.st_dev && before[fd].st_ino == after.st_ino);
        CHECK(flags[fd] == fcntl(fd, F_GETFD));
        CHECK(status[fd] == fcntl(fd, F_GETFL));
    }
}

enum class Mode { Embedding, Helper, Inheritance, Preservation };

static void runCase(const char* executable, Mode mode, int mask, Fault selected = Fault::None,
        const char* expected_error = nullptr) {
    int diagnostic[2];
    CHECK(!pipe(diagnostic));
    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) {
        alarm(30);
        CHECK(!close(diagnostic[0]));
        CHECK(dup2(diagnostic[1], 2) == 2);
        CHECK(!close(diagnostic[1]));
        // Do not change the parent's shared file-status flags in preservation tests.
        int input = open("/dev/null", O_RDWR);
        CHECK(input > 2 && dup2(input, 0) == 0);
        CHECK(!close(input));
        for (int fd = 0; fd < 3; ++fd) {
            if (mask & (1 << fd)) {
                CHECK(!close(fd));
            }
        }
        fault = selected;
        if (expected_error) {
            CHECK(!atexit([]() { _exit(98); }));
        }
        if (mode == Mode::Preservation) {
            checkPreservation();
        } else if (mode == Mode::Embedding) {
            qore_init(QL_MIT, nullptr, false, QLO_DISABLE_SIGNAL_HANDLING);
            checkNullDescriptors(mask);
            qore_cleanup();
            // Cleanup must not release the repaired descriptors either.
            checkNullDescriptors(mask);
        } else {
            qore_ensure_standard_fds();
            checkNullDescriptors(mask);
            if (extra_fd >= 0) {
                CHECK(fcntl(extra_fd, F_GETFD) == -1 && errno == EBADF);
            }
            if (mode == Mode::Inheritance) {
                execl(executable, executable, "--check-exec", nullptr);
                _exit(97);
            }
        }
        _exit(expected_error ? 96 : 0);
    }
    CHECK(!close(diagnostic[1]));
    int status;
    pid_t waited;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    CHECK(waited == child);
    char output[2048];
    ssize_t length = read(diagnostic[0], output, sizeof(output) - 1);
    CHECK(length >= 0);
    output[length] = '\0';
    CHECK(!close(diagnostic[0]));
    if (!WIFEXITED(status) || WEXITSTATUS(status) != (expected_error ? 1 : 0)
            || (expected_error && !strstr(output, expected_error))) {
        fprintf(stderr, "mode %d, mask %d, fault %d: status %d; %s\n",
            static_cast<int>(mode), mask, static_cast<int>(selected), status, output);
        exit(1);
    }
}

int main(int argc, char** argv) {
    if (argc == 2 && !strcmp(argv[1], "--check-exec")) {
        checkNullDescriptors(7);
        return 0;
    }
    const struct rlimit core_limit = {0, 0};
    CHECK(!setrlimit(RLIMIT_CORE, &core_limit));
    qore_ensure_standard_fds();
    for (int mask = 0; mask < 8; ++mask) {
        runCase(argv[0], Mode::Embedding, mask);
    }
    runCase(argv[0], Mode::Preservation, 0);
    runCase(argv[0], Mode::Inheritance, 7);
    for (Fault selected : {Fault::CheckInterrupted, Fault::OpenInterrupted, Fault::DupInterrupted,
            Fault::DifferentFd}) {
        // Repair stdout, so dup2() must be accepted when it returns 1, not just 0.
        runCase(argv[0], Mode::Helper, 2, selected);
    }
    runCase(argv[0], Mode::Helper, 2, Fault::CheckFailed, "fcntl(F_GETFD) failed");
    runCase(argv[0], Mode::Helper, 2, Fault::OpenFailed, "open(/dev/null) failed");
    runCase(argv[0], Mode::Helper, 2, Fault::DupFailed, "dup2 failed");
    runCase(argv[0], Mode::Helper, 2, Fault::CloseFailed, "close failed");
    puts("Passed 18 standard-descriptor startup cases");
}
