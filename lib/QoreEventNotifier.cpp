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
