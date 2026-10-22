/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QorePollCompat.h

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

#ifndef _QORE_INTERN_QOREPOLLCOMPAT_H
#define _QORE_INTERN_QOREPOLLCOMPAT_H

/** @file QorePollCompat.h

    Readiness waits on descriptors that can be named pipes (FIFOs).

    On Darwin, poll(2) and kqueue never report the end of a named pipe whose writers have all closed (no POLLIN, no
    POLLHUP, no EV_EOF), nor the write end of one whose readers have all closed, while read(2) returns 0 and write(2)
    fails with EPIPE; select(2) reports both.  Anonymous pipes, sockets and terminals are not affected.  A wait that
    can include a named pipe therefore uses select(2) on Darwin, without the FD_SETSIZE limit (libqore is built with
    _DARWIN_C_SOURCE on Darwin, which selects the select(2) variant without it); everywhere else, and for every other
    kind of descriptor, it is poll(2).
*/

#ifdef HAVE_POLL
#include <poll.h>

//! Returns true if the descriptor is a named pipe on a platform where poll(2) and kqueue miss its hang-up (Darwin)
/** A named pipe and an anonymous pipe are both S_IFIFO; on Darwin, an anonymous pipe has no device (st_dev == 0),
    and a named pipe has the device of its file system, also once it has been unlinked.  Were an anonymous pipe
    reported here, its wait would only use select(2), which handles it the same way.

    Always false on other platforms.
*/
DLLLOCAL bool qore_fd_poll_misses_hangup(int fd);

//! Returns true if a wait on these descriptors must use qore_select_poll() (see qore_fd_poll_misses_hangup())
DLLLOCAL bool qore_poll_needs_select(const struct pollfd* fds, nfds_t nfds);

#ifdef DARWIN
//! Waits like poll(2), with select(2) and without the FD_SETSIZE limit (Darwin only)
/** Reports POLLIN for read readiness (data or the end of the input), POLLOUT for write readiness (also when the
    readers have closed, so that the write fails with EPIPE) and POLLPRI for exceptional conditions, each only when
    requested; a closed descriptor is reported with POLLNVAL, as poll(2) does.  Negative descriptors are ignored.

    select(2) reports the end of a named pipe only until a read has returned it (see qore_fifo_end_read_set()): a
    descriptor whose end a read of its File has returned is reported readable at once, with the others as they are
    then, without waiting.

    @param fds the descriptors and their requested events; \c revents is set for each
    @param nfds the number of descriptors
    @param timeout_ms the timeout in milliseconds; negative for none

    @return the number of descriptors with events, 0 on timeout, or -1 with errno set (EINTR when interrupted by a
    signal; ENOMEM when the descriptor sets cannot be allocated)
*/
DLLLOCAL int qore_select_poll(struct pollfd* fds, nfds_t nfds, int timeout_ms);

//! qore_select_poll() with a timeout in microseconds; negative for none (Darwin only)
DLLLOCAL int qore_select_poll_us(struct pollfd* fds, nfds_t nfds, int64_t timeout_us);

//! Records that a read of the descriptor, a named pipe of a File, returned the end of the pipe (Darwin only)
/** select(2) reports the end of a named pipe only until a read has returned it: the kernel clears the pipe's end
    condition when read(2) returns 0, so that a new writer can connect, while read(2) returns 0 again without waiting
    as long as the pipe has no writer.  The waits on the descriptor (qore_select_poll() and the event loop) then report
    it readable at once, as a wait on Linux reports the hang-up, until qore_fifo_end_read_clear() is called for it: a
    File clears it when a read returns data, and when it closes or changes its descriptor.  The descriptor's device and
    inode are recorded, so that another descriptor that is given the same number is not taken for it.

    The registry is process-wide, under a leaf lock with a fork handler; a process that never reads the end of a named
    pipe through a File never takes the lock.
*/
DLLLOCAL void qore_fifo_end_read_set(int fd);

//! Forgets that a read returned the end of the descriptor (see qore_fifo_end_read_set()) (Darwin only)
DLLLOCAL void qore_fifo_end_read_clear(int fd);

//! Returns 1 if the descriptor is readable now, 0 if not, -1 with errno set for an error (Darwin only)
/** A check with select(2) and no timeout that does not report the ends recorded with qore_fifo_end_read_set(): a
    File whose read returned the end of a named pipe uses it to tell a new writer's data from the end
*/
DLLLOCAL int qore_fd_readable_now(int fd);
#endif

//! poll(2), or qore_select_poll() when a descriptor is a named pipe on Darwin (see qore_fd_poll_misses_hangup())
DLLLOCAL int qore_poll_compat(struct pollfd* fds, nfds_t nfds, int timeout_ms);
#endif

#endif
