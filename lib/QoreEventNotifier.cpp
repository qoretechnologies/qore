/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreEventNotifier.cpp

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
#include "qore/intern/QoreEventNotifier.h"

#include <cerrno>
#include <unistd.h>
#include <fcntl.h>

#ifdef __linux__
#include <sys/eventfd.h>
#elif defined(DARWIN)
#include <sys/event.h>
#endif

#ifdef DARWIN
// Initialize static counter for unique EVFILT_USER identifiers
// Start at 1 to avoid using 0 as an identifier
std::atomic<uintptr_t> QoreEventNotifier::next_ident{1};
#endif

QoreEventNotifier::QoreEventNotifier(ExceptionSink* xsink) {
#ifdef __linux__
    // Linux: use eventfd for efficient signaling
    // EFD_NONBLOCK: non-blocking reads
    // EFD_CLOEXEC: close on exec (security best practice)
    notify_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (notify_fd < 0) {
        xsink->raiseErrnoException("EVENT-NOTIFIER-ERROR", errno, "eventfd() failed");
    }
#elif defined(DARWIN)
    // macOS: allocate unique identifier for EVFILT_USER optimization
    // The actual kqueue binding happens later via bindToKqueue()
    user_ident = next_ident.fetch_add(1, std::memory_order_relaxed);

    // Create fallback pipe (used when not bound to kqueue)
    if (pipe(pipe_fd) < 0) {
        xsink->raiseErrnoException("EVENT-NOTIFIER-ERROR", errno, "pipe() failed");
        return;
    }

    // Set both ends to non-blocking
    int flags = fcntl(pipe_fd[0], F_GETFL);
    if (flags >= 0) {
        fcntl(pipe_fd[0], F_SETFL, flags | O_NONBLOCK);
    }
    flags = fcntl(pipe_fd[1], F_GETFL);
    if (flags >= 0) {
        fcntl(pipe_fd[1], F_SETFL, flags | O_NONBLOCK);
    }

    // Set close-on-exec
    fcntl(pipe_fd[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipe_fd[1], F_SETFD, FD_CLOEXEC);

    // Prevent SIGPIPE on write to closed pipe (macOS-specific)
    fcntl(pipe_fd[1], F_SETNOSIGPIPE, 1);
#else
    // Other platforms: use pipe
    if (pipe(pipe_fd) < 0) {
        xsink->raiseErrnoException("EVENT-NOTIFIER-ERROR", errno, "pipe() failed");
        return;
    }

    // Set both ends to non-blocking
    int flags = fcntl(pipe_fd[0], F_GETFL);
    if (flags >= 0) {
        fcntl(pipe_fd[0], F_SETFL, flags | O_NONBLOCK);
    }
    flags = fcntl(pipe_fd[1], F_GETFL);
    if (flags >= 0) {
        fcntl(pipe_fd[1], F_SETFL, flags | O_NONBLOCK);
    }

    // Set close-on-exec
    fcntl(pipe_fd[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipe_fd[1], F_SETFD, FD_CLOEXEC);
#endif
}

QoreEventNotifier::~QoreEventNotifier() {
#ifdef __linux__
    if (notify_fd >= 0) {
        ::close(notify_fd);
    }
#elif defined(DARWIN)
    // Note: we don't need to explicitly remove the EVFILT_USER from kqueue
    // as the EventLoop will handle that, or it will be auto-cleaned if the
    // kqueue is closed. We just close the fallback pipe if still open.
    if (pipe_fd[0] >= 0) {
        ::close(pipe_fd[0]);
    }
    if (pipe_fd[1] >= 0) {
        ::close(pipe_fd[1]);
    }
#else
    if (pipe_fd[0] >= 0) {
        ::close(pipe_fd[0]);
    }
    if (pipe_fd[1] >= 0) {
        ::close(pipe_fd[1]);
    }
#endif
}

void QoreEventNotifier::notify() {
#ifdef __linux__
    // eventfd: write a counter value (1) to signal
    // Multiple writes before read will accumulate
    uint64_t val = 1;
    // We ignore errors here - if the counter overflows (very unlikely),
    // it just means we've signaled many times, which is fine
    ssize_t rc;
    do {
        rc = ::write(notify_fd, &val, sizeof(val));
    } while (rc < 0 && errno == EINTR);
#elif defined(DARWIN)
    if (optimized) {
        // Optimized path: trigger EVFILT_USER directly
        // This is very efficient - no data copy, just a flag set in the kernel
        // Multiple triggers before the event is processed will coalesce
        struct kevent ev;
        EV_SET(&ev, user_ident, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);
        // Use zero timeout - this is a non-blocking trigger
        struct timespec ts = {0, 0};
        kevent(kq_fd, &ev, 1, nullptr, 0, &ts);
    } else {
        // Fallback path: write to pipe
        char c = '.';
        ssize_t rc;
        do {
            rc = ::write(pipe_fd[1], &c, 1);
        } while (rc < 0 && errno == EINTR);
        // Ignore EAGAIN - pipe is full, notification is already pending
    }
#else
    // pipe: write a byte to signal
    // If pipe is full, that's fine - there's already a pending notification
    char c = '.';
    ssize_t rc;
    do {
        rc = ::write(pipe_fd[1], &c, 1);
    } while (rc < 0 && errno == EINTR);
    // Ignore EAGAIN - pipe is full, notification is already pending
#endif
}

void QoreEventNotifier::acknowledge(ExceptionSink* xsink) {
#ifdef __linux__
    // eventfd: read the counter to acknowledge all pending notifications
    uint64_t val;
    ssize_t rc;
    do {
        rc = ::read(notify_fd, &val, sizeof(val));
    } while (rc < 0 && errno == EINTR);

    // EAGAIN is fine - no pending notification (shouldn't happen if called after poll)
    if (rc < 0 && errno != EAGAIN) {
        xsink->raiseErrnoException("EVENT-NOTIFIER-ERROR", errno, "eventfd read failed");
    }
#elif defined(DARWIN)
    if (optimized) {
        // Optimized path: EVFILT_USER with EV_CLEAR auto-resets
        // Nothing to do - the event is automatically acknowledged when returned from kevent()
        return;
    }
    // Fallback path: drain pipe
    char buf[64];
    ssize_t rc;
    while (true) {
        do {
            rc = ::read(pipe_fd[0], buf, sizeof(buf));
        } while (rc < 0 && errno == EINTR);

        if (rc <= 0) {
            // EAGAIN means pipe is empty - we're done
            if (rc < 0 && errno != EAGAIN) {
                xsink->raiseErrnoException("EVENT-NOTIFIER-ERROR", errno, "pipe read failed");
            }
            break;
        }
    }
#else
    // pipe: drain all bytes from the read end
    char buf[64];
    ssize_t rc;
    while (true) {
        do {
            rc = ::read(pipe_fd[0], buf, sizeof(buf));
        } while (rc < 0 && errno == EINTR);

        if (rc <= 0) {
            // EAGAIN means pipe is empty - we're done
            // rc == 0 means pipe write end was closed (shouldn't happen)
            if (rc < 0 && errno != EAGAIN) {
                xsink->raiseErrnoException("EVENT-NOTIFIER-ERROR", errno, "pipe read failed");
            }
            break;
        }
        // Continue draining if we got data
    }
#endif
}

#ifdef DARWIN
int QoreEventNotifier::bindToKqueue(int kqueue_fd, ExceptionSink* xsink) {
    if (optimized) {
        xsink->raiseException("EVENT-NOTIFIER-ERROR", "already bound to a kqueue");
        return -1;
    }

    kq_fd = kqueue_fd;

    // Register EVFILT_USER with kqueue
    // EV_ADD: add the event
    // EV_CLEAR: auto-reset after delivery (like edge-triggered, but for user events)
    struct kevent ev;
    EV_SET(&ev, user_ident, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, this);

    struct timespec ts = {0, 0};
    if (kevent(kq_fd, &ev, 1, nullptr, 0, &ts) < 0) {
        xsink->raiseErrnoException("EVENT-NOTIFIER-ERROR", errno,
            "kevent() failed to register EVFILT_USER");
        return -1;
    }

    optimized = true;

    // Close the fallback pipe - no longer needed
    if (pipe_fd[0] >= 0) {
        ::close(pipe_fd[0]);
        ::close(pipe_fd[1]);
        pipe_fd[0] = pipe_fd[1] = -1;
    }

    return 0;
}

void QoreEventNotifier::unbindFromKqueue() {
    if (!optimized) {
        return;
    }

    // Remove EVFILT_USER from kqueue
    struct kevent ev;
    EV_SET(&ev, user_ident, EVFILT_USER, EV_DELETE, 0, 0, nullptr);

    struct timespec ts = {0, 0};
    // Ignore errors - the kqueue may have been closed already
    kevent(kq_fd, &ev, 1, nullptr, 0, &ts);

    optimized = false;
    kq_fd = -1;

    // Note: we don't recreate the pipe here since the notifier is being
    // removed from the event loop and typically won't be reused
}
#endif
