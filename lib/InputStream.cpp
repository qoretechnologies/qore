/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    InputStream.cpp

    Qore Programming Language

    Copyright (C) 2016 - 2026 Qore Technologies, s.r.o.

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
#include <qore/InputStream.h>

#ifdef HAVE_POLL
#include <poll.h>
#else
#include <sys/select.h>
#endif

int64 InputStream::readWithTimeout(void* ptr, int64 limit, int64 timeout_ms, ExceptionSink* xsink) {
    // If blocking or non-pollable, use readNonBlock directly
    if (timeout_ms < 0) {
        return read(ptr, limit, xsink);
    }

    if (timeout_ms == 0) {
        return readNonBlock(ptr, limit, xsink);
    }

    // Timeout > 0: need to poll
    int fd = getPollableDescriptor();
    if (fd < 0) {
        // Not pollable - fall back to non-blocking read (works for memory streams)
        if (!supportsNonBlockingIo()) {
            return readNonBlock(ptr, limit, xsink);
        }
        xsink->raiseException("INPUT-STREAM-ERROR",
            "%s::readWithTimeout() called with timeout on a non-pollable stream", getName());
        return -1;
    }

#ifdef HAVE_POLL
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int rv = poll(&pfd, 1, static_cast<int>(timeout_ms));
    if (rv < 0) {
        xsink->raiseException("INPUT-STREAM-ERROR", "poll() failed: %s", strerror(errno));
        return -1;
    }
    if (rv == 0) {
        // Timeout
        return 0;
    }
    if (pfd.revents & (POLLERR | POLLNVAL)) {
        xsink->raiseException("INPUT-STREAM-ERROR", "poll() returned error condition (revents=0x%x)",
            pfd.revents);
        return -1;
    }
    // POLLHUP without POLLERR: fall through to readNonBlock() to drain buffered data
#else
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int rv = select(fd + 1, &rfds, nullptr, nullptr, &tv);
    if (rv < 0) {
        xsink->raiseException("INPUT-STREAM-ERROR", "select() failed: %s", strerror(errno));
        return -1;
    }
    if (rv == 0) {
        return 0;
    }
#endif

    return readNonBlock(ptr, limit, xsink);
}
