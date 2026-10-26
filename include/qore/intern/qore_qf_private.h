/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_qf_private.h

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

#ifndef _QORE_INTERN_QORE_QF_PRIVATE_H
#define _QORE_INTERN_QORE_QF_PRIVATE_H

#include "qore/intern/QC_Queue.h"
#ifdef HAVE_TERMIOS_H
#include "qore/intern/QC_TermIOS.h"
#endif

#include "qore/intern/StringReaderHelper.h"
#include "qore/intern/qore_string_private.h"
#include "qore/AbstractPollState.h"
#include "qore/intern/QorePollCompat.h"
#include <qore/QoreSandboxManager.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <string>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>

#if defined HAVE_POLL
#include <poll.h>
#elif defined HAVE_SYS_SELECT_H
#include <sys/select.h>
#elif (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
#define HAVE_SELECT 1
#else
#error no async I/O APIs available
#endif

#ifndef DEFAULT_FILE_BUFSIZE
#define DEFAULT_FILE_BUFSIZE 16384
#endif

//! Tracks the timeout of a synchronous file read with the monotonic clock
class qore_file_read_deadline {
public:
    DLLLOCAL qore_file_read_deadline(int timeout_ms)
            : timeout_ms(timeout_ms), deadline_us(timeout_ms >= 0
                ? q_get_monotonic_us() + static_cast<int64>(timeout_ms) * 1000 : 0) {
    }

    //! Returns true and sets the remaining poll timeout (-1 for none), or false if the deadline has expired.
    /** The poll is not bounded for cancellation: a cancellation request ends it at once (see
        qore_cancellable_poll()).
    */
    DLLLOCAL bool getPollTimeout(int& poll_timeout_ms) {
        if (timeout_ms < 0) {
            poll_timeout_ms = -1;
            return true;
        }

        // A zero timeout gets exactly one immediate readiness/read attempt.
        if (!timeout_ms) {
            if (!zero_poll_pending) {
                return false;
            }
            zero_poll_pending = false;
            poll_timeout_ms = 0;
            return true;
        }

        int64 remaining_us = deadline_us - q_get_monotonic_us();
        if (remaining_us <= 0) {
            return false;
        }

        int64 remaining_ms = (remaining_us + 999) / 1000;
        poll_timeout_ms = static_cast<int>(std::min<int64>(remaining_ms, INT_MAX));
        return true;
    }

    DLLLOCAL bool expired() const {
        if (timeout_ms < 0) {
            return false;
        }
        if (!timeout_ms) {
            return !zero_poll_pending;
        }
        return q_get_monotonic_us() >= deadline_us;
    }

    DLLLOCAL int getTimeout() const {
        return timeout_ms;
    }

private:
    int timeout_ms;
    int64 deadline_us;
    bool zero_poll_pending = true;
};

struct qore_qf_private {
    int fd = -1;
    bool is_open = false;
    //! a file made from a descriptor (stdin, stdout, stderr, the ends of File::getPipe()), which cannot be reopened
    bool special_file = false;
    //! a special file that owns its descriptor (the ends of File::getPipe()): it is closed by close() and with the
    //! object, while the descriptors of the standard streams are never closed
    bool owned_special = false;
    const QoreEncoding* charset;
    std::string filename;
    mutable QoreThreadLock m;
    Queue* event_queue = nullptr;
    //! argument for the event queue
    QoreValue event_arg{};
    //! if data should be included in file events
    bool event_data = false;
    bool in_non_block = false;
    //! true once a read of the descriptor, a named pipe on Darwin, has returned the end of the pipe
    /** select(2), the wait that reports the end of a named pipe on Darwin (see QorePollCompat.h), reports it only
        until a read has returned it, while read(2) returns the end again as long as the pipe has no writer; while
        this is set, a read with a wait therefore returns the end again unless the pipe has data at once (see
        readData()), a check for data reports the end at once, and the descriptor is recorded with
        qore_fifo_end_read_set(), so that the waits on it (Socket::poll(), the event loop, stream waits) report the end
        at once too.  Cleared by a read that returns data, by a nonblocking read that finds a new writer, and with the
        descriptor (see clearFifoEndRead())
    */
    mutable bool fifo_end_read = false;
    //! keeps the shift state of text in a stateful encoding between reads of characters; protected by the lock
    QoreStatefulTextDecoder decoder;

    DLLLOCAL qore_qf_private(const QoreEncoding* cs) : charset(cs) {
    }

    DLLLOCAL ~qore_qf_private() {
        close_intern();

        // must be dereferenced and removed before deleting
        assert(!event_queue);
     }

    //! closes the file or, with @p detach, gives up its descriptor without closing it; call with the lock held
    /** Either way the object is left closed and without a descriptor (\c fd is -1), so it can never close a
        descriptor it no longer owns, and getFD() and getPollableDescriptor() return -1 as documented.  Special files
        (stdin, stdout, stderr) are never closed or detached.

        The close event is raised for a detach as well: it reports that the File object's channel is closed, which
        is true once the object has given up its descriptor, and listeners that pair each EVENT_FILE_OPENED with an
        EVENT_CHANNEL_CLOSED would otherwise see the file still open until the object is deleted.

        @param detach if true, the descriptor is not closed; the caller of detachFd() owns it

        @return 0 for success, -1 if the file is a special file, or the return value of close(2) if the descriptor
        was closed
    */
    DLLLOCAL int close_intern(bool detach = false) {
        filename.clear();
        decoder.reset();
        clearFifoEndRead();

        int rc;
        if (is_open) {
            if (special_file && !owned_special) {
                rc = -1;
            } else {
                rc = detach ? 0 : ::close(fd);
                is_open = false;
                fd = -1;
                do_close_event_unlocked();
            }
        } else {
            rc = 0;
        }
        return rc;
    }

    DLLLOCAL int redirect(qore_qf_private& file, ExceptionSink* xsink) {
        if (&file == this)
            return 0;

        // lock both files
        AutoLocker al(m);
        if (checkNonBlock(xsink)) {
            return -1;
        }

        AutoLocker al2(file.m);
        if (file.checkNonBlock(xsink)) {
            return -1;
        }

        // flush any C stdio-buffered data to the current (old) target before
        // changing the underlying fd; this prevents data loss when redirecting
        // stdout/stderr to a different destination (e.g. pipe for output capture)
        if (fd == STDOUT_FILENO) {
            fflush(stdout);
        } else if (fd == STDERR_FILENO) {
            fflush(stderr);
        }

        // dup2() will close this file descriptor
        // the descriptor now refers to the other file
        clearFifoEndRead();
        int rc = dup2(file.fd, fd);
        if (rc == -1) {
            xsink->raiseErrnoException("FILE-REDIRECT-ERROR", errno, "error in dup2()");
            return -1;
        }
        filename = file.filename;
        // text from the new target starts in the initial shift state
        decoder.reset();

        return 0;
    }

    DLLLOCAL int open_intern(const char* fn, int flags, int mode, const QoreEncoding* cs) {
        close_intern();

        if (!flags)
            flags = O_RDONLY;

#ifdef _Q_WINDOWS
        // open files in binary mode by default on Windows
        if (!(flags & O_TEXT))
            flags |= O_BINARY;
#endif

        do_open_event_unlocked(fn, flags, mode, cs);

        // a file opened by the library is never inherited by a program the process starts (with system(),
        // backquote(), fork() and exec(), or a module)
#ifdef O_CLOEXEC
        fd = ::open(fn, flags | O_CLOEXEC, mode);
#else
        fd = ::open(fn, flags, mode);
#endif
        if (fd < 0)
            return fd;

        do_opened_event_unlocked(fn, flags, mode, cs);

        filename = fn;
        if (cs)
            charset = cs;
        is_open = true;
        decoder.reset();
        return 0;
    }

    //! checks the sandbox filesystem policy for opening a file with the given open() flags
    /** @return 0 if access is allowed, -1 if it is denied, in which case an exception has been raised
    */
    DLLLOCAL static int checkOpenAccess(const char* fn, int flags, ExceptionSink* xsink);

    DLLLOCAL int open(const char* fn, int flags, int mode, const QoreEncoding* cs) {
        if (!fn || special_file)
            return -1;

        AutoLocker al(m);
        return open_intern(fn, flags, mode, cs);
    }

    // returns -1 for exception
    DLLLOCAL int checkReadOpen(ExceptionSink* xsink) const {
        if (is_open) {
            return 0;
        }

        xsink->raiseException("FILE-READ-ERROR", "file has not been opened");
        return -1;
    }

    // returns -1 for exception
    DLLLOCAL int checkWriteOpen(ExceptionSink* xsink) const {
        if (is_open) {
            return 0;
        }

        xsink->raiseException("FILE-WRITE-ERROR", "file has not been opened");
        return -1;
    }

    // returns -1 for exception
    DLLLOCAL int checkOpen(ExceptionSink* xsink) const {
        if (is_open) {
            return 0;
        }

        xsink->raiseException("FILE-OPERATION-ERROR", "file has not been opened");
        return -1;
    }

    DLLLOCAL bool isOpen() const {
        return is_open;
    }

    DLLLOCAL bool isDataAvailable(int timeout_ms, ExceptionSink *xsink) const {
        AutoLocker al(m);
        if (checkNonBlock(xsink)) {
            return false;
        }

        if (checkReadOpen(xsink))
            return false;

        return isDataAvailableIntern(timeout_ms, "isDataAvailable", xsink);
    }

    // fd must be open or -1 is returned and a Qore-language exception is raised
    /* return values:
        -1: error
        0: timeout
        > 0: I/O can continue
        */
    DLLLOCAL int select(int timeout_ms, bool read, const char* mname, ExceptionSink* xsink) const {
        //printd(5, "select() to: %d read: %d mname: '%s'\n", timeout_ms, read, mname);
        if (checkOpen(xsink))
            return -1;

#if defined HAVE_POLL
        return poll_intern(timeout_ms, read, mname, xsink);
#elif defined HAVE_SELECT
        return select_intern(timeout_ms, read, mname, xsink);
#else
#error no async I/O operations supported
#endif
    }

#if defined HAVE_POLL
    DLLLOCAL int poll_intern(int timeout_ms, bool read, const char* mname, ExceptionSink* xsink) const {
#ifdef DARWIN
        // select(2) does not report the end of a named pipe again once a read has returned it, while a read returns
        // the end again at once (see fifo_end_read)
        if (read && fifo_end_read) {
            return 1;
        }
#endif
#ifdef DEBUG
        if (timeout_ms) {
            qore_dbg_fd_wait(fd);
        }
#endif
        // the wait ends at once when the thread is cancelled or its Program is interrupted; EINTR is handled by
        // qore_cancellable_poll(), which also waits on a named pipe with select(2) on Darwin, where poll(2) misses
        // the end of the pipe (see QorePollCompat.h)
        pollfd fds = {fd, static_cast<short>(read ? POLLIN : POLLOUT), 0};
        int rc = qore_cancellable_poll(&fds, 1, timeout_ms, xsink, read ? "file read" : "file write");
        if (rc == QORE_POLL_CANCELLED) {
            return -1;
        }
        if (rc < 0 && !*xsink)
            xsink->raiseException("FILE-SELECT-ERROR", "poll(2) returned an error in call to File::%s()", mname);
        else if (!rc && ((fds.revents & POLLHUP) || (fds.revents & (POLLERR|POLLNVAL))))
            rc = -1;

        return rc;
    }
#elif defined HAVE_SELECT
    DLLLOCAL int select_intern(int timeout_ms, bool read, const char* mname, ExceptionSink* xsink) const {
#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
        // async I/O ignored on files on windows
        return 1;
#else
        // select is inherently broken since it can only handle descriptors < FD_SETSIZE, which is 1024 on Linux for example
        if (fd >= FD_SETSIZE) {
            if (xsink)
                xsink->raiseException("FILE-SELECT-ERROR", "fd is %d in call to File::%s() which is >= %d; contact "
                    "the Qore developers to implement an alternative to select() on this platform", fd, mname,
                    FD_SETSIZE);
            return -1;
        }
        int rc;
        int select_timeout_ms = timeout_ms;
        int64 deadline_us = timeout_ms >= 0
            ? q_get_monotonic_us() + static_cast<int64>(timeout_ms) * 1000 : 0;
        while (true) {
            // to be safe, we set the file descriptor arg after each EINTR (required on Linux for example)
            fd_set sfs;

            FD_ZERO(&sfs);
            FD_SET(fd, &sfs);

            struct timeval tv;
            tv.tv_sec  = select_timeout_ms / 1000;
            tv.tv_usec = (select_timeout_ms % 1000) * 1000;

            // a negative timeout means no timeout
            struct timeval* ptv = select_timeout_ms < 0 ? nullptr : &tv;
            rc = read ? ::select(fd + 1, &sfs, 0, 0, ptv) : ::select(fd + 1, 0, &sfs, 0, ptv);
            if (rc >= 0 || errno != EINTR) {
                break;
            }
            if (qore_check_cancel(xsink, read ? "file read" : "file write")) {
                return -1;
            }
            if (timeout_ms >= 0) {
                int64 remaining_us = deadline_us - q_get_monotonic_us();
                if (remaining_us <= 0) {
                    return 0;
                }
                select_timeout_ms = static_cast<int>((remaining_us + 999) / 1000);
            }
        }
        if (rc == -1) {
            rc = 0;
            if (!*xsink) {
                xsink->raiseException("FILE-SELECT-ERROR", "select(2) returned an error in call to File::%s()", mname);
            }
        }

        return rc;
#endif
    }
#endif

    // assumes lock is held and file is open
    DLLLOCAL bool isDataAvailableIntern(int timeout_ms, const char* mname, ExceptionSink *xsink) const {
        return select(timeout_ms, true, mname, xsink);
    }

#ifdef HAVE_TERMIOS_H
    DLLLOCAL int setTerminalAttributes(int action, QoreTermIOS* ios, ExceptionSink* xsink) const {
        AutoLocker al(m);
        if (checkNonBlock(xsink)) {
            return -1;
        }

        if (checkOpen(xsink))
            return -1;

        return ios->set(fd, action, xsink);
    }

    DLLLOCAL int getTerminalAttributes(QoreTermIOS* ios, ExceptionSink* xsink) const {
        AutoLocker al(m);
        if (checkNonBlock(xsink)) {
            return -1;
        }

        if (checkOpen(xsink))
            return -1;

        return ios->get(fd, xsink);
    }
#endif

    //! reads exactly @p bs bytes, unless the end of the file comes first; the lock must be held
    /** A pipe can return fewer bytes than requested: the read continues until all of them have been read.

        @return the number of bytes read (less than @p bs only at the end of the file), or -1 for an error or an
        exception (\c THREAD-CANCELLED or \c PROGRAM-INTERRUPTED while waiting for data; see read())
    */
    DLLLOCAL ssize_t readCheck(ExceptionSink* xsink, void* buf, size_t bs) const {
        if (checkNonBlock(xsink)) {
            return -1;
        }

        if (checkReadOpen(xsink)) {
            return -1;
        }

        char* p = static_cast<char*>(buf);
        size_t total = 0;
        while (total < bs) {
            ssize_t rc = read(p + total, bs - total, xsink);
            if (rc < 0) {
                return -1;
            }
            if (!rc) {
                break;
            }
            total += static_cast<size_t>(rc);
        }
        return static_cast<ssize_t>(total);
    }

    //! returns true if the error is that of a read or write of a nonblocking descriptor that would have waited
    DLLLOCAL static bool wouldBlock(int err) {
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
        return err == EAGAIN || err == EWOULDBLOCK;
#else
        return err == EAGAIN;
#endif
    }

    //! reads up to @p bs bytes; the lock must be held
    /** Waits for data when the descriptor is nonblocking and has none (see qore_fd_wait_ready()): the file's reads
        are blocking, also when another holder of its open file description (an embedded runtime, a parent process)
        has set \c O_NONBLOCK on it.

        @param buf the buffer for the data
        @param bs the size of the buffer
        @param xsink for the exception raised when the thread is cancelled or its Program is interrupted while waiting
        for data; if nullptr, the wait cannot be cancelled

        @return the number of bytes read, 0 at the end of the file, or -1 for an error, with \c errno set, or with
        an exception raised in @p xsink
    */
    DLLLOCAL ssize_t read(void* buf, size_t bs, ExceptionSink* xsink) const {
        // must be called with the lock held
        assert(m.trylock());

        ssize_t rc;
        while (true) {
            rc = ::read(fd, buf, std::min(bs, QORE_FD_IO_MAX_SIZE));
            if (rc >= 0) {
                break;
            }
            // try again if we were interrupted by a signal
            if (errno == EINTR) {
                continue;
            }
            if (!wouldBlock(errno)) {
                break;
            }
            // a nonblocking descriptor without data: a named pipe has a writer again (see fifo_end_read)
            clearFifoEndRead();
            int wait_rc = qore_fd_wait_ready(fd, true, xsink, "file read");
            if (wait_rc) {
                return -1;
            }
        }
        noteReadResult(rc, bs);

        if (rc > 0)
            do_read_event_unlocked(rc, rc, bs);

        return rc;
    }

    //! records whether a read of @a limit bytes that returned @a rc read the end of a named pipe (see fifo_end_read)
    DLLLOCAL void noteReadResult(ssize_t rc, size_t limit) const {
#ifdef DARWIN
        if (rc > 0) {
            clearFifoEndRead();
        } else if (!rc && limit && !fifo_end_read && qore_fd_poll_misses_hangup(fd)) {
            fifo_end_read = true;
            qore_fifo_end_read_set(fd);
        }
#else
        (void)rc;
        (void)limit;
#endif
    }

    //! forgets that a read returned the end of a named pipe (see fifo_end_read); call before the descriptor changes
    DLLLOCAL void clearFifoEndRead() const {
#ifdef DARWIN
        if (fifo_end_read) {
            fifo_end_read = false;
            qore_fifo_end_read_clear(fd);
        }
#endif
    }

#ifdef DARWIN
    //! Returns 1 if a read with a wait at the end of a named pipe returns now, 0 if it returns the end, -1 for an error
    /** At the end that a read has returned (see fifo_end_read), the pipe is read if it has data at once, a new
        writer's; else the end is returned again.  A read would return the end without waiting while the pipe has no
        writer, but would wait for a new writer that has written nothing, which cannot be told apart without making the
        descriptor nonblocking, which its other holders would see (see FileStreamIo.h): the end is returned then too,
        and the new writer's data once the pipe has it.

        @return 1 if the pipe has data (it is read), 0 if the end is returned again, -1 with errno set for an error
    */
    DLLLOCAL int fifoEndReadReady() const {
        assert(fifo_end_read);
        int ready = qore_fd_readable_now(fd);
        return ready;
    }
#endif


    // unlocked, assumes file is open
    DLLLOCAL ssize_t writeCheck(const void* buf, size_t len, ExceptionSink* xsink) const {
        if (checkNonBlock(xsink)) {
            return 0;
        }

        if (checkWriteOpen(xsink)) {
            return 0;
        }

        return write(buf, len, xsink);
    }

    //! writes all of the data; the lock must be held
    /** Waits for room when the descriptor is nonblocking and full (see qore_fd_wait_ready()): the file's writes are
        blocking, also when another holder of its open file description (an embedded runtime such as Node.js, a parent
        process) has set \c O_NONBLOCK on it.  A write that the descriptor takes only in part is continued with the
        rest of the data.

        @param buf the data
        @param len the size of the data
        @param xsink for the exceptions: \c FILE-WRITE-ERROR for an I/O error, \c THREAD-CANCELLED or
        \c PROGRAM-INTERRUPTED when the thread is cancelled or its Program is interrupted while the write waits for
        room; if nullptr, no exception is raised, and the wait cannot be cancelled

        @return the number of bytes written, or -1 for an error
    */
    DLLLOCAL ssize_t write(const void* buf, size_t len, ExceptionSink* xsink) const {
        // must be called with the lock held
        assert(m.trylock());

        const char* ptr = static_cast<const char*>(buf);
        size_t remaining = len;
        size_t total_written = 0;

        while (remaining > 0) {
            // Check for cancellation or program interrupt
            if (xsink && qore_check_cancel(xsink, "file write")) {
                return -1;
            }

            ssize_t rc;
            while (true) {
                rc = ::write(fd, ptr, std::min(remaining, QORE_FD_IO_MAX_SIZE));
                // try again if we are interrupted by a signal
                if (rc >= 0 || errno != EINTR)
                    break;
            }

            if (rc > 0) {
                ptr += rc;
                remaining -= rc;
                total_written += rc;
                do_write_event_unlocked(rc, total_written, len);
            } else if (rc < 0) {
                int write_errno = errno;
                if (wouldBlock(write_errno)) {
                    // a nonblocking descriptor that is full: the write continues once there is room
                    int wait_rc = qore_fd_wait_ready(fd, false, xsink, "file write");
                    if (wait_rc == QORE_POLL_CANCELLED) {
                        return -1;
                    }
                    if (wait_rc) {
                        if (xsink) {
                            xsink->raiseErrnoException("FILE-WRITE-ERROR", errno, "failed waiting to write " QSD
                                " byte%s to File", remaining, remaining == 1 ? "" : "s");
                        }
                        return -1;
                    }
                    continue;
                }
                if (xsink) {
                    xsink->raiseErrnoException("FILE-WRITE-ERROR", write_errno, "failed writing " QSD
                        " byte%s to File", len, len == 1 ? "" : "s");
                }
                return rc;
            } else {
                // rc == 0, nothing written but no error
                break;
            }
        }

        return total_written;
    }

    //! reads one byte; the lock must be held
    /** @param xsink for the exception raised when the thread is cancelled or its Program is interrupted while waiting
        for data (see read()); if nullptr, the wait cannot be cancelled

        @return the byte, or -1 at the end of the file, for an error, or with an exception raised in @p xsink
    */
    DLLLOCAL int readChar(ExceptionSink* xsink) const {
        unsigned char ch = 0;
        if (read(&ch, 1, xsink) != 1)
            return -1;
        return (int)ch;
    }

    //! reads one character and returns its code point; the lock must be held (see readChar())
    DLLLOCAL int readUnicode(ExceptionSink* xsink, int* n_len = nullptr) const;

    DLLLOCAL qore_offset_t readData(void* dest, size_t limit, qore_file_read_deadline& read_deadline,
            const char* mname, ExceptionSink* xsink, const char* timeout_error = "FILE-READ-TIMEOUT",
            bool readonly_context = true) {
#ifndef _Q_WINDOWS
        // A nonblocking read before polling returns buffered data or EOF immediately and avoids platform-specific
        // cases where poll() does not report EOF on a FIFO with no writers. Blocking descriptors must poll first.
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0 && (flags & O_NONBLOCK)) {
            while (true) {
                if (qore_check_cancel(xsink, "file read")) {
                    return -1;
                }
                if (read_deadline.getTimeout() > 0 && read_deadline.expired()) {
                    raiseReadTimeout(read_deadline, mname, timeout_error, readonly_context, xsink);
                    return -1;
                }

                qore_offset_t rc = ::read(fd, dest, std::min(limit, QORE_FD_IO_MAX_SIZE));
                int read_errno = errno;
                if (rc >= 0) {
                    noteReadResult(rc, limit);
                    return rc;
                }
                if (read_errno == EINTR) {
                    continue;
                }
                if (read_errno == EAGAIN
#ifdef EWOULDBLOCK
                        || read_errno == EWOULDBLOCK
#endif
                ) {
                    // the pipe has a writer again: the wait below reports its data and the end of the pipe
                    clearFifoEndRead();
                    break;
                }

                if (readonly_context) {
                    xsink->raiseErrnoException("FILE-READ-ERROR", read_errno,
                        "error reading file in ReadOnlyFile::%s()", mname);
                } else {
                    xsink->raiseErrnoException("FILE-READ-ERROR", read_errno, "error reading file");
                }
                return -1;
            }
        }

#ifdef DARWIN
        // select(2) does not report the end of a named pipe again once a read has returned it (see fifo_end_read):
        // the end is returned again unless the pipe has data at once (a new writer's).  A read would return the end
        // without waiting while the pipe has no writer, but would wait for a new writer that has written nothing,
        // which cannot be told apart without making the descriptor nonblocking, which its other holders would see
        // (see FileStreamIo.h); the end is returned then too, and the new writer's data once select(2) reports it.
        // A nonblocking descriptor has been read above
        if (fifo_end_read) {
            if (qore_check_cancel(xsink, "file read")) {
                return -1;
            }
            int ready = fifoEndReadReady();
            if (ready < 0) {
                xsink->raiseErrnoException("FILE-SELECT-ERROR", errno, "select(2) returned an error in call to "
                    "File::%s()", mname);
                return -1;
            }
            if (!ready) {
                return 0;
            }
        }
#endif
#endif

        while (true) {
            if (qore_check_cancel(xsink, "file read")) {
                return -1;
            }

            int poll_timeout_ms;
            if (!read_deadline.getPollTimeout(poll_timeout_ms)) {
                raiseReadTimeout(read_deadline, mname, timeout_error, readonly_context, xsink);
                return -1;
            }

            int ready = isDataAvailableIntern(poll_timeout_ms, mname, xsink);
            if (*xsink) {
                return -1;
            }
            if (!ready) {
                if (read_deadline.expired()) {
                    raiseReadTimeout(read_deadline, mname, timeout_error, readonly_context, xsink);
                    return -1;
                }
                continue;
            }

            if (qore_check_cancel(xsink, "file read")) {
                return -1;
            }

#ifdef DEBUG
            qore_dbg_file_read_ready(fd);
#endif
            qore_offset_t rc = ::read(fd, dest, std::min(limit, QORE_FD_IO_MAX_SIZE));
            int read_errno = errno;
            if (rc >= 0) {
                noteReadResult(rc, limit);
                return rc;
            }

            if (read_errno == EINTR) {
                continue;
            }
            if (wouldBlock(read_errno)) {
                // the descriptor was reported readable, but another holder of the pipe took the data first, or the
                // descriptor was made nonblocking by another holder after the check above: the read waits again, for
                // the rest of the timeout.  A nonblocking read without data means that a named pipe has a writer, so
                // the end of the pipe that a read returned before is not reported at once any more (see
                // fifo_end_read), and the wait waits for data or for the end of the pipe
                clearFifoEndRead();
                continue;
            }

            if (readonly_context) {
                xsink->raiseErrnoException("FILE-READ-ERROR", read_errno,
                    "error reading file in ReadOnlyFile::%s()", mname);
            } else {
                xsink->raiseErrnoException("FILE-READ-ERROR", read_errno, "error reading file");
            }
            return -1;
        }
    }

    DLLLOCAL qore_offset_t readData(void* dest, size_t limit, int timeout_ms, const char* mname,
            ExceptionSink* xsink, const char* timeout_error = "FILE-READ-TIMEOUT", bool readonly_context = true) {
        qore_file_read_deadline read_deadline(timeout_ms);
        return readData(dest, limit, read_deadline, mname, xsink, timeout_error, readonly_context);
    }

    DLLLOCAL QoreStringNode* readString(qore_offset_t size, int timeout_ms, const char* mname, ExceptionSink* xsink) {
        qore_file_read_deadline read_deadline(timeout_ms);
        f_read_t my_read = [this, &read_deadline, mname](void* dest, size_t limit, ExceptionSink* read_xsink) {
            return readData(dest, limit, read_deadline, mname, read_xsink);
        };
        {
            AutoLocker al(m);
            // characters of text in a stateful encoding are read in the shift state left by the text read before
            // them; the lock serializes the use of the decoder
            if (useDecoder(size > 0)) {
                return decoder.readChars(charset, size, my_read, xsink);
            }
        }
        return q_read_string(xsink, size, charset, my_read);
    }

    //! returns true if text is read with the decoder; the lock must be held
    /** @param char_read true for a read of a number of characters, which needs the decoder in a stateful encoding;
        other reads of text need it once the decoder has been used, as the text continues in its shift state
    */
    DLLLOCAL bool useDecoder(bool char_read) const {
        return qore_encoding_private::isStateful(charset) && (char_read || decoder.active());
    }

    //! returns a function object that reads bytes from the file for the decoder; the lock must be held
    DLLLOCAL f_read_t getDecoderReadFunc() {
        return [this](void* dest, size_t limit, ExceptionSink* xsink) -> qore_offset_t {
            ssize_t rc = read(dest, limit, xsink);
            if (rc < 0) {
                // a wait for data that was cancelled has raised its exception
                if (!*xsink) {
                    xsink->raiseErrnoException("FILE-READ-ERROR", errno, "error reading file");
                }
                return -1;
            }
            return rc;
        };
    }

    //! reads a line with the decoder; the lock must be held
    /** @param eol the end-of-line marker in UTF-8, or empty for any of \c "\n", \c "\r", or \c "\r\n"

        @return 0 if data was read, -1 if no data was read (end of file) or an exception was raised
    */
    DLLLOCAL int readDecoderLine(const std::string& eol, QoreString& str, bool incl_eol, ExceptionSink* xsink) {
        // a terminal is not read past a carriage return
        SimpleRefHolder<QoreStringNode> line(decoder.readLine(charset, eol, !incl_eol, !isatty(fd),
            getDecoderReadFunc(), xsink));
        if (!line) {
            return -1;
        }
        // the line is in the encoding of the file, as \a str
        assert(line->getEncoding() == str.getEncoding());
        str.concat(line->c_str(), line->size());
        return 0;
    }

    //! returns the decoder to the initial shift state; the lock must be held
    DLLLOCAL void resetDecoder() {
        decoder.reset();
    }

    //! reads one character of text in a stateful encoding; the lock must be held
    DLLLOCAL QoreStringNode* readDecoderChar(ExceptionSink* xsink) {
        assert(qore_encoding_private::isStateful(charset));
        return decoder.readChars(charset, 1, getDecoderReadFunc(), xsink);
    }

    DLLLOCAL char* readBlock(qore_offset_t &size, int timeout_ms, const char* mname, ExceptionSink* xsink) {
        size_t bs = size > 0 && size < DEFAULT_FILE_BUFSIZE ? size : DEFAULT_FILE_BUFSIZE;
        size_t br = 0;
        char* buf = static_cast<char*>(malloc(sizeof(char) * bs));
        char* bbuf = nullptr;
        if (!buf) {
            xsink->outOfMemory();
            return nullptr;
        }

        qore_file_read_deadline read_deadline(timeout_ms);

        while (true) {
            qore_offset_t rc = readData(buf, bs, read_deadline, mname, xsink);
            //printd(5, "readBlock(fd: %d, buf: %p, bs: %d) rc: %d\n", fd, buf, bs, rc);
            if (rc <= 0)
                break;

            // enlarge bbuf (ensure buffer is 1 byte bigger than needed)
            char* new_bbuf = static_cast<char*>(realloc(bbuf, br + rc + 1));
            if (!new_bbuf) {
                xsink->outOfMemory();
                break;
            }
            bbuf = new_bbuf;
            // append buffer to bbuf
            memcpy(bbuf + br, buf, rc);
            br += rc;

            do_read_event_unlocked(rc, br, size);

            if (size > 0) {
                if (size - br < bs)
                    bs = size - br;
                if (br >= (size_t)size)
                    break;
            }
        }
        free(buf);
        if (*xsink) {
            if (bbuf)
                free(bbuf);
            return nullptr;
        }
        size = br;
        return bbuf;
    }

    DLLLOCAL static void raiseReadTimeout(const qore_file_read_deadline& read_deadline, const char* mname,
            const char* timeout_error, bool readonly_context, ExceptionSink* xsink) {
        if (readonly_context) {
            xsink->raiseException(timeout_error, "timeout limit exceeded (%d ms) reading file block in "
                "ReadOnlyFile::%s()", read_deadline.getTimeout(), mname);
        } else {
            xsink->raiseException(timeout_error, "timeout limit exceeded (%d ms) reading file",
                read_deadline.getTimeout());
        }
    }

    DLLLOCAL QoreStringNode* readLine(bool incl_eol, ExceptionSink* xsink) {
        QoreStringNodeHolder str(new QoreStringNode(charset));

        int rc = readLine(**str, incl_eol, xsink);
        if (*xsink) {
            return nullptr;
        }

        if (rc == -2) {
            xsink->raiseException("FILE-READLINE-ERROR", "file has not been opened");
            return 0;
        }

        return rc == -1 ? 0 : str.release();
    }

    //! returns the bytes of a character in the file's encoding as they are in the middle of text in it
    /** Without the bytes that iconv writes at the start of text in a stateful encoding; empty if the character
        cannot be represented in the encoding
    */
    DLLLOCAL std::string getCharBytes(unsigned cp) const {
        QoreString s(charset);
        if (s.concatUnicode(cp)) {
            return std::string();
        }
        qore_string_private::get(s)->removeStatefulPrefix();
        return std::string(s.c_str(), s.size());
    }

    //! reads until the given marker in the file's encoding is read at a character boundary; the lock must be held
    /** In an encoding with characters of a minimum width of more than one byte (UTF-16, UTF-32), a marker is only
        found at a character boundary

        @param marker the marker, in the encoding of the file
        @param str the string read
        @param include_marker if true, the marker is included in the string
        @param cr_marker if not empty, a carriage return in the encoding of the file that also ends the line, followed
        or not by the marker
        @param xsink for exceptions; may be nullptr

        @return 0 if data was read, -1 if no data was read (end of file) or an exception was raised
    */
    DLLLOCAL int readUntilMarker(const std::string& marker, QoreString& str, bool include_marker, const std::string& cr_marker,
            ExceptionSink* xsink) {
        assert(!marker.empty());
        const size_t width = charset->getMinCharWidth();
        const bool tty = (bool)isatty(fd);
        // true if the string ends with the given marker at a character boundary
        auto ends_with = [&str, width](const std::string& m) -> bool {
            size_t sz = str.size();
            return !m.empty() && sz >= m.size() && !((sz - m.size()) % width)
                && !memcmp(str.c_str() + sz - m.size(), m.data(), m.size());
        };
        int ch, rc = -1;
        int char_count = 0;
        while ((ch = readChar(xsink)) >= 0) {
            if (xsink && ++char_count >= 100) {
                char_count = 0;
                if (qore_check_cancel(xsink, "file readLine")) {
                    return -1;
                }
            }
            str.concat(static_cast<char>(ch));
            rc = 0;
            if (ends_with(marker)) {
                if (!include_marker) {
                    str.terminate(str.size() - marker.size());
                }
                break;
            }
            if (ends_with(cr_marker)) {
                size_t cr_end = str.size();
                // a following marker is part of the end of the line if the file is not a terminal
                if (!tty) {
                    std::string next;
                    while (next.size() < marker.size() && (ch = readChar(xsink)) >= 0) {
                        next.push_back(static_cast<char>(ch));
                    }
                    if (next == marker) {
                        str.concat(next.data(), next.size());
                    } else if (!next.empty()) {
                        lseek(fd, -static_cast<off_t>(next.size()), SEEK_CUR);
                    }
                }
                if (!include_marker) {
                    str.terminate(cr_end - cr_marker.size());
                }
                break;
            }
        }
        return rc;
    }

    //! reads until the given end-of-line marker, converted to the file's encoding, is read
    DLLLOCAL QoreStringNode* readUntilText(const QoreString& eol, bool incl_eol, ExceptionSink* xsink) {
        TempEncodingHelper teol(eol, charset, xsink);
        if (!teol) {
            return nullptr;
        }
        // the bytes that iconv writes at the start of text in a stateful encoding are not in the middle of the text
        teol.makeTemp();
        qore_string_private::get(const_cast<QoreString&>(**teol))->removeStatefulPrefix();
        if (!teol->size()) {
            return readLine(incl_eol, xsink);
        }
        std::string marker(teol->c_str(), teol->size());
        QoreStringNodeHolder str(new QoreStringNode(charset));
        int rc;
        {
            AutoLocker al(m);
            assert(!in_non_block);
            if (!is_open) {
                xsink->raiseException("FILE-READLINE-ERROR", "file has not been opened");
                return nullptr;
            }
            if (useDecoder(false)) {
                TempEncodingHelper eol_utf8(eol, QCS_UTF8, xsink);
                if (!eol_utf8) {
                    return nullptr;
                }
                rc = readDecoderLine(std::string(eol_utf8->c_str(), eol_utf8->size()), **str, incl_eol, xsink);
            } else {
                rc = readUntilMarker(marker, **str, incl_eol, std::string(), xsink);
            }
        }
        if (*xsink) {
            return nullptr;
        }
        return rc == -1 ? nullptr : str.release();
    }

    DLLLOCAL int readLine(QoreString& str, bool incl_eol = true, ExceptionSink* xsink = nullptr) {
        str.clear();

        AutoLocker al(m);
        assert(!in_non_block);

        if (!is_open)
            return -2;

        // text in a stateful encoding that continues the text read with the decoder is read with it
        if (useDecoder(false)) {
            if (xsink) {
                return readDecoderLine(std::string(), str, incl_eol, xsink);
            }
            ExceptionSink local_xsink;
            int rc = readDecoderLine(std::string(), str, incl_eol, &local_xsink);
            local_xsink.clear();
            return rc;
        }

        // the end-of-line characters are not single ASCII bytes in an encoding that is not ASCII-compatible
        // (UTF-16, EBCDIC); they are searched for in the encoding of the file
        if (!charset->isAsciiCompat()) {
            std::string lf = getCharBytes('\n');
            // an encoding that cannot represent a line feed is read byte by byte below
            if (!lf.empty()) {
                return readUntilMarker(lf, str, incl_eol, getCharBytes('\r'), xsink);
            }
        }

        bool tty = (bool)isatty(fd);

        int ch, rc = -1;
        int char_count = 0;
        const int check_interval = 100;  // Check cancel/interrupt every 100 characters

        while ((ch = readChar(xsink)) >= 0) {
            // Check for cancellation or program interrupt periodically
            if (xsink && ++char_count >= check_interval) {
                char_count = 0;
                if (qore_check_cancel(xsink, "file readLine")) {
                    return -1;
                }
            }

            str.concat((char)ch);
            if (rc == -1)
                rc = 0;

            if (ch == '\r') {
                // see if next byte is \n' if we're not connected to a terminal device
                if (!tty) {
                    ch = readChar(xsink);
                    if (ch >= 0) {
                        if (ch == '\n') {
                            if (incl_eol)
                                str.concat((char)ch);
                        } else {
                            // reset file to previous byte position
                            lseek(fd, -1, SEEK_CUR);
                        }
                    }
                }
                if (!incl_eol)
                    str.terminate(str.strlen() - 1);
                break;
            }

            if (ch == '\n') {
                if (!incl_eol)
                    str.terminate(str.strlen() - 1);
                break;
            }
        }

        return rc;
    }

    DLLLOCAL int readUntil(char byte, QoreString& str, bool incl_byte = true, ExceptionSink* xsink = nullptr) {
        str.clear();

        AutoLocker al(m);
        assert(!in_non_block);

        if (!is_open)
            return -2;

        int ch, rc = -1;
        int char_count = 0;
        const int check_interval = 100;  // Check cancel/interrupt every 100 characters

        while ((ch = readChar(xsink)) >= 0) {
            // Check for cancellation or program interrupt periodically
            if (xsink && ++char_count >= check_interval) {
                char_count = 0;
                if (qore_check_cancel(xsink, "file readUntil")) {
                    return -1;
                }
            }

            char c = ch;
            str.concat(c);
            if (rc == -1)
                rc = 0;
            if (c == byte) {
                if (!incl_byte)
                    str.terminate(str.strlen() - 1);
                break;
            }
        }

        return rc;
    }

    DLLLOCAL QoreStringNode* readUntil(const char* bytes, bool incl_bytes, ExceptionSink* xsink) {
        QoreStringNodeHolder str(new QoreStringNode(charset));

        int rc = readUntil(bytes, **str, incl_bytes, xsink);
        if (*xsink) {
            return nullptr;
        }

        if (rc == -2) {
            xsink->raiseException("FILE-READLINE-ERROR", "file has not been opened");
            return 0;
        }

        return rc == -1 ? 0 : str.release();
    }

    DLLLOCAL int readLineUnicode(QoreString& str, bool incl_eol = true) {
        str.clear();

        AutoLocker al(m);
        assert(!in_non_block);

        if (!is_open)
            return -2;

        bool tty = (bool)isatty(fd);

        int ch, rc = -1;

        while ((ch = readUnicode(nullptr)) >= 0) {
            // skip BOM
            if (ch == 0xfeff)
                continue;
            else if (ch == 0xfffe && charset == QCS_UTF16 && str.empty()) {
                charset = QCS_UTF16LE;
                continue;
            }

            str.concatUnicode(ch);

            if (rc == -1)
                rc = 0;

            char c = str[str.size() - 1];

            if (c == '\r') {
                // see if next byte is \n' if we're not connected to a terminal device
                if (!tty) {
                    int len = 0;
                    ch = readUnicode(nullptr, &len);
                    if (ch >= 0) {
                        if (ch == '\n') {
                            if (incl_eol)
                                str.concatUnicode(ch);
                        }
                        else {
                            // reset file to previous byte position
                            lseek(fd, -len, SEEK_CUR);
                        }
                    }
                }
                if (!incl_eol)
                    str.terminate(str.strlen() - 1);
                break;
            }

            if (ch == '\n') {
                if (!incl_eol)
                    str.terminate(str.strlen() - 1);
                break;
            }
        }

        return rc;
    }

    DLLLOCAL int readUntilUnicode(char byte, QoreString& str, bool incl_byte = true) {
        str.clear();

        AutoLocker al(m);
        assert(!in_non_block);

        if (!is_open)
            return -2;

        int ch, rc = -1;

        while ((ch = readUnicode(nullptr)) >= 0) {
            // skip BOM
            if (ch == 0xfeff)
                continue;
            else if (ch == 0xfffe && charset == QCS_UTF16 && str.empty()) {
                charset = QCS_UTF16LE;
                continue;
            }

            str.concatUnicode(ch);

            if (rc == -1)
                rc = 0;
            if (ch == byte) {
                if (!incl_byte)
                    str.terminate(str.strlen() - 1);
                break;
            }
        }

        return rc;
    }
    DLLLOCAL int readUntilUnicode(const char* bytes, QoreString& str, bool incl_bytes) {
        str.clear();

        AutoLocker al(m);
        assert(!in_non_block);

        if (!is_open)
            return -2;

        // offset in bytes
        unsigned pos = 0;

        int ch, rc = -1;

        while ((ch = readUnicode(nullptr)) >= 0) {
            // skip BOM
            if (ch == 0xfeff)
                continue;
            else if (ch == 0xfffe && charset == QCS_UTF16 && str.empty()) {
                charset = QCS_UTF16LE;
                continue;
            }

            str.concatUnicode(ch);

            if (rc == -1)
                rc = 0;

            if (ch == bytes[pos]) {
                ++pos;
                if (!bytes[pos]) {
                    if (!incl_bytes)
                        str.terminate(str.strlen() - pos);
                    break;
                }
            } else if (pos) {
                // bytes=aaac read=aaaac str=aaa pos=3
                //          ^         ^
                // restart search with characters already added to the string if more than 1 character was matched previously
                if (pos > 1) {
                    unsigned ps = 1;
                    while (ps < pos) {
                        if (!strncmp(str.getBuffer() + ps, bytes, pos - ps)) {
                            pos -= ps;
                            break;
                        }
                        ++ps;
                    }
                    if (pos == ps)
                        pos = 0;
                } else {
                    // restart search if failed
                    pos = 0;
                }
            }
        }

        return rc;
    }

    // not the most efficient search algorithm, restarts the search the byte after it fails for multi-byte patterns
    DLLLOCAL int readUntil(const char* bytes, QoreString& str, bool incl_bytes, ExceptionSink* xsink = nullptr) {
        if (!bytes[1])
            return readUntil(bytes[0], str, incl_bytes, xsink);

        str.clear();

        AutoLocker al(m);
        assert(!in_non_block);

        if (!is_open)
            return -2;

        // offset in bytes
        unsigned pos = 0;

        int ch, rc = -1;
        int char_count = 0;
        const int check_interval = 100;  // Check cancel/interrupt every 100 characters

        while ((ch = readChar(xsink)) >= 0) {
            // Check for cancellation or program interrupt periodically
            if (xsink && ++char_count >= check_interval) {
                char_count = 0;
                if (qore_check_cancel(xsink, "file readUntil")) {
                    return -1;
                }
            }

            char c = ch;
            str.concat(c);
            if (rc == -1)
                rc = 0;

            if (c == bytes[pos]) {
                ++pos;
                if (!bytes[pos]) {
                    if (!incl_bytes)
                        str.terminate(str.strlen() - pos);
                    break;
                }
            } else if (pos) {
                // bytes=aaac read=aaaac str=aaa pos=3
                //          ^         ^
                // restart search with characters already added to the string if more than 1 character was matched previously
                if (pos > 1) {
                unsigned ps = 1;
                while (ps < pos) {
                    if (!strncmp(str.getBuffer() + ps, bytes, pos - ps)) {
                        pos -= ps;
                        break;
                    }
                    ++ps;
                }
                if (pos == ps)
                    pos = 0;
                } else {
                    // restart search if failed
                    pos = 0;
                }
            }
        }

        return rc;
    }

    DLLLOCAL bool isTty() const {
        AutoLocker al(m);
        assert(!in_non_block);

        if (!is_open)
            return false;

        return (bool)isatty(fd);
    }

    DLLLOCAL int detachFd() {
        AutoLocker al(m);
        assert(!in_non_block);

        if (!is_open) {
            return -1;
        }

        int rc = fd;
        // special files (stdout/stderr/stdin) are never closed, so they are not detached either: the object keeps its
        // descriptor and stays open; any other file (an end of File::getPipe() included) is left closed and without a
        // descriptor, so it can never close the descriptor the caller now owns, and a second call returns -1
        if (!special_file || owned_special) {
            close_intern(true);
        }
        return rc;
    }

    DLLLOCAL size_t getPos() const {
        AutoLocker al(m);
        assert(!in_non_block);

        if (!is_open)
            return -1;

        return lseek(fd, 0, SEEK_CUR);
    }

    DLLLOCAL QoreHashNode* getEvent(int event, int source = QORE_SOURCE_FILE) const {
        QoreHashNode* h = new QoreHashNode(autoTypeInfo);
        if (event_arg) {
            h->setKeyValue("arg", event_arg.refSelf(), nullptr);
        }

        h->setKeyValue("event", event, nullptr);
        h->setKeyValue("source", source, nullptr);
        h->setKeyValue("id", (int64)this, nullptr);

        return h;
    }

    DLLLOCAL void setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
        AutoLocker al(m);
        if (event_queue) {
            if (event_arg) {
                event_arg.discard(xsink);
            }
            event_queue->deref(xsink);
        }
        event_queue = q;
        event_arg = arg;
        event_data = with_data;
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink) {
        AutoLocker al(m);
        if (event_queue) {
            // close the file before the delete message is put on the queue
            // the file would be closed anyway in the destructor
            close_intern();

            event_queue->pushAndTakeRef(getEvent(QORE_EVENT_DELETED));

            // deref and remove event queue and arg
            if (event_arg) {
                event_arg.discard(xsink);
                event_arg.clear();
            }
            event_queue->deref(xsink);
            event_queue = nullptr;
        }
    }

    DLLLOCAL void do_open_event_unlocked(const char* fn, int flags, int mode, const QoreEncoding* enc) const {
        if (event_queue) {
            QoreHashNode* h = getEvent(QORE_EVENT_OPEN_FILE);
            h->setKeyValue("filename", new QoreStringNode(fn), nullptr);
            h->setKeyValue("flags", flags, nullptr);
            h->setKeyValue("mode", mode, nullptr);
            h->setKeyValue("encoding", new QoreStringNode(enc->getCode()), nullptr);
            event_queue->pushAndTakeRef(h);
        }
    }

    DLLLOCAL void do_opened_event_unlocked(const char* fn, int flags, int mode, const QoreEncoding* enc) const {
        if (event_queue) {
            QoreHashNode* h = getEvent(QORE_EVENT_FILE_OPENED);
            h->setKeyValue("source", QORE_SOURCE_FILE, nullptr);
            h->setKeyValue("id", (int64)this, nullptr);
            h->setKeyValue("filename", new QoreStringNode(fn), nullptr);
            h->setKeyValue("flags", flags, nullptr);
            h->setKeyValue("mode", mode, nullptr);
            h->setKeyValue("encoding", new QoreStringNode(enc->getCode()), nullptr);
            event_queue->pushAndTakeRef(h);
        }
    }

    DLLLOCAL void do_close_event_unlocked() const {
        if (event_queue) {
            event_queue->pushAndTakeRef(getEvent(QORE_EVENT_CHANNEL_CLOSED));
        }
    }

    DLLLOCAL void do_read_event_unlocked(int64 bytes_read, int64 total_read, int64 bufsize) const {
        // post bytes read on event queue, if any
        if (event_queue) {
            QoreHashNode* h = getEvent(QORE_EVENT_DATA_READ);
            h->setKeyValue("read", bytes_read, nullptr);
            h->setKeyValue("total_read", total_read, nullptr);
            h->setKeyValue("total_to_read", bufsize, nullptr);
            event_queue->pushAndTakeRef(h);
        }
    }

    DLLLOCAL void do_write_event_unlocked(int64 bytes_written, int64 total_written, int64 bufsize) const {
        // post bytes sent on event queue, if any
        if (event_queue) {
            QoreHashNode* h = getEvent(QORE_EVENT_DATA_WRITTEN);
            h->setKeyValue("written", bytes_written, nullptr);
            h->setKeyValue("total_written", total_written, nullptr);
            h->setKeyValue("total_to_write", bufsize, nullptr);
            event_queue->pushAndTakeRef(h);
        }
    }

    DLLLOCAL QoreListNode* stat(ExceptionSink* xsink) const {
        AutoLocker al(m);
        if (checkNonBlock(xsink)) {
            return nullptr;
        }

        if (checkReadOpen(xsink))
            return nullptr;

        struct stat sbuf;
        if (fstat(fd, &sbuf)) {
            xsink->raiseErrnoException("FILE-STAT-ERROR", errno, "fstat() call failed");
            return nullptr;
        }

        return stat_to_list(sbuf);
    }

    DLLLOCAL QoreHashNode* hstat(ExceptionSink* xsink) const {
        AutoLocker al(m);
        if (checkNonBlock(xsink)) {
            return nullptr;
        }

        if (checkReadOpen(xsink))
            return nullptr;

        struct stat sbuf;
        if (fstat(fd, &sbuf)) {
            xsink->raiseErrnoException("FILE-HSTAT-ERROR", errno, "fstat() call failed");
            return nullptr;
        }

        return stat_to_hash(sbuf);
    }

#ifdef Q_HAVE_STATVFS
    DLLLOCAL QoreHashNode* statvfs(ExceptionSink* xsink) const {
        AutoLocker al(m);
        if (checkNonBlock(xsink)) {
            return nullptr;
        }

        if (checkReadOpen(xsink))
            return nullptr;

        struct statvfs vfs;
#ifdef HAVE_SYS_STATVFS_H
        if (fstatvfs(fd, &vfs)) {
            xsink->raiseErrnoException("FILE-STATVFS-ERROR", errno, "fstatvfs() call failed");
            return nullptr;
        }
#else
        if (q_fstatvfs(filename.c_str(), &vfs)) {
            xsink->raiseErrnoException("FILE-STATVFS-ERROR", errno, "fstatvfs() call failed");
            return nullptr;
        }
#endif

        return statvfs_to_hash(vfs);
    }
#endif

    DLLLOCAL AbstractPollState* startRead(ExceptionSink* xsink, ssize_t bytes);

    //! Sets non-blocking I/O on the file descriptor
    DLLLOCAL int setNonBlockingIo(bool non_blocking, ExceptionSink* xsink);

    //! Throws an exception if the in_non_block flag is set or is not valid
    DLLLOCAL int checkNonBlock(ExceptionSink* xsink) const {
        // must be called with the lock held
        assert(m.trylock());

        if (in_non_block) {
            xsink->raiseException("FILE-NON-BLOCK-ERROR", "a non-blocking operation is currently in progress");
            return -1;
        }

        return 0;
    }

    //! Sets the in_non_block flag
    DLLLOCAL int setNonBlock(ExceptionSink* xsink, bool do_io = true) {
        // must be called with the lock held
        assert(m.trylock());

        if (!checkNonBlock(xsink) && (!do_io || !setNonBlockingIo(true, xsink))) {
            in_non_block = true;
            return 0;
        }
        return -1;
    }

    //! Clears the in_non_block flag
    DLLLOCAL void clearNonBlock(ExceptionSink* xsink) {
        // must be called with the lock held
        assert(m.trylock());

        if (in_non_block) {
            setNonBlockingIo(false, xsink);
            in_non_block = false;
        }
    }

    DLLLOCAL QoreObject* startPollRead(ExceptionSink* xsink, QoreObject* self, const char* path, int64 to_read);
};

#endif
