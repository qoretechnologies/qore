/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QorePollCompat.cpp

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
#include "qore/intern/QorePollCompat.h"

#ifdef HAVE_POLL

#include <cerrno>
#include <new>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>

#ifdef DARWIN
#include <sys/param.h>
#include <sys/select.h>
// select(2) takes descriptors at and above FD_SETSIZE only in its _DARWIN_C_SOURCE (or _DARWIN_UNLIMITED_SELECT)
// variant; the default one fails with EINVAL
#if !defined(_DARWIN_C_SOURCE) && !defined(_DARWIN_UNLIMITED_SELECT)
#error "libqore must be built with _DARWIN_C_SOURCE on Darwin (see cmake/QoreMacrosIntern.cmake)"
#endif
#endif

bool qore_fd_poll_misses_hangup(int fd) {
#ifdef DARWIN
    struct stat st;
    return fd >= 0 && !fstat(fd, &st) && S_ISFIFO(st.st_mode) && st.st_dev;
#else
    (void)fd;
    return false;
#endif
}

bool qore_poll_needs_select(const struct pollfd* fds, nfds_t nfds) {
#ifdef DARWIN
    for (nfds_t i = 0; i < nfds; ++i) {
        if ((fds[i].events & (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM)) && qore_fd_poll_misses_hangup(fds[i].fd)) {
            return true;
        }
    }
#else
    (void)fds;
    (void)nfds;
#endif
    return false;
}

#ifdef DARWIN
int qore_select_poll(struct pollfd* fds, nfds_t nfds, int timeout_ms) {
    return qore_select_poll_us(fds, nfds, timeout_ms < 0 ? -1 : static_cast<int64_t>(timeout_ms) * 1000);
}

int qore_select_poll_us(struct pollfd* fds, nfds_t nfds, int64_t timeout_us) {
    int maxfd = -1;
    for (nfds_t i = 0; i < nfds; ++i) {
        fds[i].revents = 0;
        if (fds[i].fd > maxfd) {
            maxfd = fds[i].fd;
        }
    }

    // the sets are sized for the highest descriptor instead of FD_SETSIZE
    size_t words = maxfd < 0 ? 0 : static_cast<size_t>((maxfd + NFDBITS) / NFDBITS);
    std::vector<fd_mask> rbits, wbits, xbits;
    try {
        rbits.resize(words);
        wbits.resize(words);
        xbits.resize(words);
    } catch (std::bad_alloc&) {
        errno = ENOMEM;
        return -1;
    }
    fd_set* rset = words ? reinterpret_cast<fd_set*>(rbits.data()) : nullptr;
    fd_set* wset = words ? reinterpret_cast<fd_set*>(wbits.data()) : nullptr;
    fd_set* xset = words ? reinterpret_cast<fd_set*>(xbits.data()) : nullptr;

    for (nfds_t i = 0; i < nfds; ++i) {
        int fd = fds[i].fd;
        if (fd < 0) {
            continue;
        }
        if (fds[i].events & (POLLIN | POLLRDNORM)) {
            FD_SET(fd, rset);
        }
        if (fds[i].events & (POLLOUT | POLLWRNORM)) {
            FD_SET(fd, wset);
        }
        if (fds[i].events & (POLLPRI | POLLRDBAND)) {
            FD_SET(fd, xset);
        }
    }

    struct timeval tv;
    struct timeval* ptv = nullptr;
    if (timeout_us >= 0) {
        tv.tv_sec = static_cast<time_t>(timeout_us / 1000000);
        tv.tv_usec = static_cast<suseconds_t>(timeout_us % 1000000);
        ptv = &tv;
    }

    int rc = select(maxfd + 1, rset, wset, xset, ptv);
    if (rc < 0) {
        if (errno != EBADF) {
            return -1;
        }
        // poll(2) reports a closed descriptor with POLLNVAL instead of failing
        int count = 0;
        for (nfds_t i = 0; i < nfds; ++i) {
            if (fds[i].fd >= 0 && fcntl(fds[i].fd, F_GETFD) < 0 && errno == EBADF) {
                fds[i].revents = POLLNVAL;
                ++count;
            }
        }
        if (!count) {
            errno = EBADF;
            return -1;
        }
        return count;
    }
    if (!rc) {
        return 0;
    }

    int count = 0;
    for (nfds_t i = 0; i < nfds; ++i) {
        int fd = fds[i].fd;
        if (fd < 0) {
            continue;
        }
        short revents = 0;
        if (FD_ISSET(fd, rset)) {
            revents |= fds[i].events & (POLLIN | POLLRDNORM);
        }
        if (FD_ISSET(fd, wset)) {
            revents |= fds[i].events & (POLLOUT | POLLWRNORM);
        }
        if (FD_ISSET(fd, xset)) {
            revents |= fds[i].events & (POLLPRI | POLLRDBAND);
        }
        fds[i].revents = revents;
        if (revents) {
            ++count;
        }
    }
    return count;
}
#endif

int qore_poll_compat(struct pollfd* fds, nfds_t nfds, int timeout_ms) {
#ifdef DARWIN
    if (qore_poll_needs_select(fds, nfds)) {
        return qore_select_poll(fds, nfds, timeout_ms);
    }
#endif
    return ::poll(fds, nfds, timeout_ms);
}

#endif
