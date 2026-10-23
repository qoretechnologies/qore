/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    FileStreamIo.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_INTERN_FILESTREAMIO_H
#define _QORE_INTERN_FILESTREAMIO_H

/** @file FileStreamIo.h

    Reads and writes of the file streams that must not wait (FileInputStream::readNonBlock() and
    FileOutputStream::writeNonBlock(), which the async I/O thread calls), made without changing the descriptor's
    file status flags.

    The flags (O_NONBLOCK among them) belong to the open file description, which other holders can share: an
    inherited standard input or output, a duplicated descriptor, a child process.  Turning O_NONBLOCK on for the
    length of the call made their reads and writes fail with EAGAIN in the meantime.  Instead:
    - a descriptor that is already nonblocking is read and written directly;
    - a socket is read and written with MSG_DONTWAIT;
    - a regular file or a block device is read and written directly: its reads and writes never wait for a peer;
    - any other descriptor (a pipe, a FIFO, a terminal, another device) is read once a check that does not wait
      reports it readable, as such a read returns what is available, and written in chunks that a write after
      write readiness accepts without waiting: PIPE_BUF bytes for a pipe, a FIFO or a device other than a terminal,
      and 1 byte for a terminal, for which write readiness only guarantees room for 1 byte on Linux.  Each chunk
      waits for its own readiness check, so a call writes as much as the descriptor accepts at once.

    The readiness checks use qore_poll_compat(), so that the end of a named pipe is seen on Darwin.  On Linux and
    macOS (measured on Linux 7.0 and macOS 26.6), write readiness on a pipe or a FIFO leaves room for at least PIPE_BUF
    bytes (4096 and 512; on macOS this holds for select(2), which qore_poll_compat() uses for named pipes, while
    poll(2) can report a named pipe with 1 byte of room).

    The one case that can still wait: another holder of the same pipe takes the data or the room between the check
    and the read or write.  A file stream must therefore not share a pipe with other concurrent readers or writers;
    the read or write would then wait for the pipe, also on the async I/O thread.
*/

#include "qore/intern/QorePollCompat.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <climits>
#include <cstddef>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef PIPE_BUF
#define PIPE_BUF 512
#endif

#ifdef DEBUG
//! Debug-only: called with a file stream's descriptor right before each read or write that must not wait
/** Lets a test see the descriptor's file status flags at that moment; see dbg_file_stream_io_flags() in
    ql_debug.cpp.  The make jobserver client of qcc (QoreAOT.cpp) calls it as well, right before each token read.
*/
DLLLOCAL extern std::atomic<void (*)(int)> qore_dbg_file_stream_io_hook;

//! Debug-only: calls qore_dbg_file_stream_io_hook, if set
DLLLOCAL inline void qore_dbg_file_stream_io(int fd) {
    void (*hook)(int) = qore_dbg_file_stream_io_hook.load();
    if (hook) {
        hook(fd);
    }
}
#endif

//! The kinds of descriptors that the file streams read and write differently
enum QoreFileStreamFdKind {
    QFSFK_DIRECT,   //!< a regular file or a block device: its reads and writes never wait for a peer
    QFSFK_SOCKET,   //!< a socket: read and written with MSG_DONTWAIT
    QFSFK_PIPE,     //!< a pipe, a FIFO or a device other than a terminal
    QFSFK_TTY,      //!< a terminal
};

//! Returns the kind of the descriptor, or -1 with errno set
DLLLOCAL inline int qore_file_stream_fd_kind(int fd) {
    struct stat st;
    if (fstat(fd, &st)) {
        return -1;
    }
    if (S_ISREG(st.st_mode) || S_ISBLK(st.st_mode) || S_ISDIR(st.st_mode)) {
        return QFSFK_DIRECT;
    }
    if (S_ISSOCK(st.st_mode)) {
        return QFSFK_SOCKET;
    }
    if (S_ISCHR(st.st_mode) && isatty(fd)) {
        return QFSFK_TTY;
    }
    return QFSFK_PIPE;
}

//! Returns 1 if the descriptor is ready for the events, 0 if not, or -1 with errno set; does not wait
DLLLOCAL inline int qore_file_stream_ready_now(int fd, short events) {
    struct pollfd pfd = {fd, events, 0};
    int rc;
    while ((rc = qore_poll_compat(&pfd, 1, 0)) < 0 && errno == EINTR) {
    }
    if (rc < 0) {
        return -1;
    }
    if (rc && (pfd.revents & POLLNVAL)) {
        errno = EBADF;
        return -1;
    }
    // POLLHUP and POLLERR: the read or write that follows returns the end or the error
    return rc ? 1 : 0;
}

//! Reads up to @p limit bytes that are available without waiting and without changing the descriptor's flags
/** @param read_done if given, set to true if the descriptor was read: a return value of 0 is then the end of the
    input, not a read that was not made because nothing could be read now

    @return the number of bytes read; 0 at the end of the input or if nothing can be read now (as the nonblocking read
    did), or -1 with @p err set to the error
*/
DLLLOCAL inline ssize_t qore_file_stream_read_now(int fd, void* ptr, size_t limit, int& err,
        bool* read_done = nullptr) {
    err = 0;
    if (read_done) {
        *read_done = false;
    }
    int fl = fcntl(fd, F_GETFL);
    if (fl == -1) {
        err = errno;
        return -1;
    }
    int kind = (fl & O_NONBLOCK) ? QFSFK_DIRECT : qore_file_stream_fd_kind(fd);
    if (kind < 0) {
        err = errno;
        return -1;
    }
    if (kind == QFSFK_PIPE || kind == QFSFK_TTY) {
        int ready = qore_file_stream_ready_now(fd, POLLIN);
        if (ready <= 0) {
            if (ready < 0) {
                err = errno;
            }
            return ready;
        }
    }
#ifdef DEBUG
    qore_dbg_file_stream_io(fd);
#endif
    ssize_t rc;
    while (true) {
        rc = kind == QFSFK_SOCKET ? ::recv(fd, ptr, limit, MSG_DONTWAIT) : ::read(fd, ptr, limit);
        if (rc >= 0 || errno != EINTR) {
            break;
        }
    }
    if (rc < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        err = errno;
        return -1;
    }
    if (read_done) {
        *read_done = true;
    }
    return rc;
}

//! Writes up to @p count bytes that can be written without waiting and without changing the descriptor's flags
/** @return the number of bytes written; 0 if nothing can be written now, or -1 with @p err set to the error (EPIPE
    when the reading end has closed); an error after some bytes have been written is returned by the next call
*/
DLLLOCAL inline ssize_t qore_file_stream_write_now(int fd, const void* ptr, size_t count, int& err) {
    err = 0;
    if (!count) {
        return 0;
    }
    int fl = fcntl(fd, F_GETFL);
    if (fl == -1) {
        err = errno;
        return -1;
    }
    int kind = (fl & O_NONBLOCK) ? QFSFK_DIRECT : qore_file_stream_fd_kind(fd);
    if (kind < 0) {
        err = errno;
        return -1;
    }
    if (kind == QFSFK_DIRECT || kind == QFSFK_SOCKET) {
#ifdef DEBUG
        qore_dbg_file_stream_io(fd);
#endif
        ssize_t rc;
        while (true) {
#ifdef MSG_NOSIGNAL
            rc = kind == QFSFK_SOCKET ? ::send(fd, ptr, count, MSG_DONTWAIT | MSG_NOSIGNAL) : ::write(fd, ptr, count);
#else
            rc = kind == QFSFK_SOCKET ? ::send(fd, ptr, count, MSG_DONTWAIT) : ::write(fd, ptr, count);
#endif
            if (rc >= 0 || errno != EINTR) {
                break;
            }
        }
        if (rc < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }
            err = errno;
            return -1;
        }
        return rc;
    }

    // chunks that a write after write readiness accepts without waiting
    const size_t chunk = kind == QFSFK_TTY ? 1 : PIPE_BUF;
    const char* p = static_cast<const char*>(ptr);
    size_t written = 0;
    unsigned loops = 0;
    while (written < count) {
        // a terminal that keeps accepting single bytes can make many rounds; a cancelled thread stops after what has
        // been written, which the next call returns or reports
        if (!(++loops % 100) && qore_check_cancel(nullptr, "file stream write")) {
            break;
        }
        int ready = qore_file_stream_ready_now(fd, POLLOUT);
        if (ready <= 0) {
            if (ready < 0 && !written) {
                err = errno;
                return -1;
            }
            break;
        }
        size_t n = std::min(chunk, count - written);
#ifdef DEBUG
        qore_dbg_file_stream_io(fd);
#endif
        ssize_t rc;
        while ((rc = ::write(fd, p + written, n)) < 0 && errno == EINTR) {
        }
        if (rc < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            if (!written) {
                err = errno;
                return -1;
            }
            break;
        }
        written += rc;
        if (static_cast<size_t>(rc) < n) {
            break;
        }
    }
    return written;
}

#endif
