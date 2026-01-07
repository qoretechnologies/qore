/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_qf_private.h

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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
#include "qore/AbstractPollState.h"
#include <qore/QoreSandboxManager.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
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

struct qore_qf_private {
    int fd = -1;
    bool is_open = false;
    bool special_file = false;
    const QoreEncoding* charset;
    std::string filename;
    mutable QoreThreadLock m;
    Queue* event_queue = nullptr;
    //! argument for the event queue
    QoreValue event_arg{};
    //! if data should be included in file events
    bool event_data = false;
    bool in_non_block = false;

    DLLLOCAL qore_qf_private(const QoreEncoding* cs) : charset(cs) {
    }

    DLLLOCAL ~qore_qf_private() {
        close_intern();

        // must be dereferenced and removed before deleting
        assert(!event_queue);
     }

    DLLLOCAL int close_intern(bool detach = false) {
        filename.clear();

        int rc;
        if (is_open) {
            if (special_file) {
                rc = -1;
            } else if (!detach) {
                rc = ::close(fd);
                is_open = false;
                do_close_event_unlocked();
            } else {
               rc = 0;
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

        // dup2() will close this file descriptor
        int rc = dup2(file.fd, fd);
        if (rc == -1) {
            xsink->raiseErrnoException("FILE-REDIRECT-ERROR", errno, "error in dup2()");
            return -1;
        }
        filename = file.filename;

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

        fd = ::open(fn, flags, mode);
        if (fd < 0)
            return fd;

        do_opened_event_unlocked(fn, flags, mode, cs);

        filename = fn;
        if (cs)
            charset = cs;
        is_open = true;
        return 0;
    }

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
        int rc;
        pollfd fds = {fd, (short)(read ? POLLIN : POLLOUT), 0};
        while (true) {
            rc = poll(&fds, 1, timeout_ms);
            if (rc == -1 && errno == EINTR)
                continue;
            break;
        }
        if (rc < 0)
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
        struct timeval tv;
        int rc;
        while (true) {
            // to be safe, we set the file descriptor arg after each EINTR (required on Linux for example)
            fd_set sfs;

            FD_ZERO(&sfs);
            FD_SET(fd, &sfs);

            tv.tv_sec  = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;

            rc = read ? ::select(fd + 1, &sfs, 0, 0, &tv) : ::select(fd + 1, 0, &sfs, 0, &tv);
            if (rc >= 0 || errno != EINTR)
                break;
        }
        if (rc == -1) {
            rc = 0;
            xsink->raiseException("FILE-SELECT-ERROR", "select(2) returned an error in call to File::%s()", mname);
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

    // unlocked, assumes file is open
    DLLLOCAL ssize_t readCheck(ExceptionSink* xsink, void* buf, size_t bs) const {
        if (checkNonBlock(xsink)) {
            return 0;
        }

        if (checkReadOpen(xsink)) {
            return 0;
        }

        return read(buf, bs);
    }

    // unlocked, assumes file is open
    DLLLOCAL ssize_t read(void* buf, size_t bs) const {
        // must be called with the lock held
        assert(m.trylock());

        ssize_t rc;
        while (true) {
            rc = ::read(fd, buf, bs);
            // try again if we were interrupted by a signal
            if (rc >= 0 || errno != EINTR)
                break;
        }

        if (rc > 0)
            do_read_event_unlocked(rc, rc, bs);

        return rc;
    }

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

    // unlocked, assumes file is open
    DLLLOCAL ssize_t write(const void* buf, size_t len, ExceptionSink* xsink = 0) const {
        // must be called with the lock held
        assert(m.trylock());

        // Check for sandbox interrupt support
        QoreSandboxManager* sm = runtime_get_sandbox_manager();

        const char* ptr = static_cast<const char*>(buf);
        size_t remaining = len;
        size_t total_written = 0;

        while (remaining > 0) {
            // Check for interrupt
            if (sm && xsink && sm->checkIOInterrupt(xsink, "file write")) {
                return -1;
            }

            ssize_t rc;
            while (true) {
                rc = ::write(fd, ptr, remaining);
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
                if (xsink) {
                    xsink->raiseErrnoException("FILE-WRITE-ERROR", errno, "failed writing " QSD " byte%s to File", len,
                        len == 1 ? "" : "s");
                }
                return rc;
            } else {
                // rc == 0, nothing written but no error
                break;
            }
        }

        return total_written;
    }

    // private function, unlocked
    DLLLOCAL int readChar() const {
        unsigned char ch = 0;
        if (read(&ch, 1) != 1)
            return -1;
        return (int)ch;
    }

    // private function, unlocked
    DLLLOCAL int readUnicode(int* n_len = 0) const;

    DLLLOCAL qore_offset_t readData(void* dest, size_t limit, int timeout_ms, const char* mname,
            ExceptionSink* xsink) {
        // wait for data
        if (timeout_ms >= 0 && !isDataAvailableIntern(timeout_ms, mname, xsink)) {
            if (!*xsink)
                xsink->raiseException("FILE-READ-TIMEOUT", "timeout limit exceeded (%d ms) reading file block in "
                    "ReadOnlyFile::%s()", timeout_ms, mname);
            return -1;
        }

        qore_offset_t rc;
        while (true) {
            rc = ::read(fd, dest, limit);
            //printd(5, "qore_qf_private::readData(%p, %ld, %d, '%s') fd: %d rc: %d\n", dest, limit, timeout_ms,
            //    mname, fd, rc);
            // try again if we were interrupted by a signal
            if (rc >= 0) {
                break;
            }
            if (errno != EINTR) {
                xsink->raiseErrnoException("FILE-READ-ERROR", errno, "error reading file in ReadOnlyFile::%s()",
                    mname);
                return -1;
            }
        }

        return rc;
    }

    DLLLOCAL QoreStringNode* readString(qore_offset_t size, int timeout_ms, const char* mname, ExceptionSink* xsink) {
        return q_read_string(xsink, size, charset, std::bind(&qore_qf_private::readData, this, _1, _2, timeout_ms,
            mname, _3));
    }

    DLLLOCAL char* readBlock(qore_offset_t &size, int timeout_ms, const char* mname, ExceptionSink* xsink) {
        size_t bs = size > 0 && size < DEFAULT_FILE_BUFSIZE ? size : DEFAULT_FILE_BUFSIZE;
        size_t br = 0;
        char* buf = (char* )malloc(sizeof(char) * bs);
        char* bbuf = 0;

        // Check for sandbox interrupt support
        QoreSandboxManager* sm = runtime_get_sandbox_manager();
        const int poll_interval = QORE_IO_POLL_INTERVAL_MS;

        while (true) {
            // Check for interrupt
            if (sm && sm->checkIOInterrupt(xsink, "file read")) {
                br = 0;
                break;
            }

            // wait for data with polling for interruptibility
            int effective_timeout = timeout_ms;
            if (sm && (timeout_ms < 0 || timeout_ms > poll_interval)) {
                effective_timeout = poll_interval;
            }

            if (effective_timeout >= 0 && !isDataAvailableIntern(effective_timeout, mname, xsink)) {
                if (*xsink) {
                    br = 0;
                    break;
                }
                // If we used a smaller timeout for polling, continue if not timed out yet
                if (sm && timeout_ms != effective_timeout) {
                    if (timeout_ms > 0) {
                        timeout_ms -= effective_timeout;
                        if (timeout_ms <= 0) {
                            xsink->raiseException("FILE-READ-TIMEOUT", "timeout limit exceeded reading file block in "
                                "ReadOnlyFile::%s()", mname);
                            br = 0;
                            break;
                        }
                    }
                    continue;  // Continue polling
                }
                xsink->raiseException("FILE-READ-TIMEOUT", "timeout limit exceeded (%d ms) reading file block in "
                    "ReadOnlyFile::%s()", timeout_ms, mname);
                br = 0;
                break;
            }

            qore_offset_t rc;
            while (true) {
                rc = ::read(fd, buf, bs);
                // try again if we were interrupted by a signal
                if (rc >= 0)
                    break;
                if (errno != EINTR) {
                    xsink->raiseErrnoException("FILE-READ-ERROR", errno, "error reading file after " QSD " bytes "
                        "read in File::%s()", br, mname);
                    break;
                }
            }
            //printd(5, "readBlock(fd: %d, buf: %p, bs: %d) rc: %d\n", fd, buf, bs, rc);
            if (rc <= 0)
                break;

            // enlarge bbuf (ensure buffer is 1 byte bigger than needed)
            bbuf = (char* )realloc(bbuf, br + rc + 1);
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

    DLLLOCAL QoreStringNode* readLine(bool incl_eol, ExceptionSink* xsink) {
        QoreStringNodeHolder str(new QoreStringNode(charset));

        int rc = readLine(**str, incl_eol);

        if (rc == -2) {
            xsink->raiseException("FILE-READLINE-ERROR", "file has not been opened");
            return 0;
        }

        return rc == -1 ? 0 : str.release();
    }

    DLLLOCAL int readLine(QoreString& str, bool incl_eol = true, ExceptionSink* xsink = nullptr) {
        str.clear();

        AutoLocker al(m);
        assert(!in_non_block);

        if (!is_open)
            return -2;

        bool tty = (bool)isatty(fd);

        // Check for sandbox interrupt support
        QoreSandboxManager* sm = runtime_get_sandbox_manager();

        int ch, rc = -1;
        int char_count = 0;
        const int check_interval = 100;  // Check interrupt every 100 characters

        while ((ch = readChar()) >= 0) {
            // Check for interrupt periodically
            if (sm && xsink && ++char_count >= check_interval) {
                char_count = 0;
                if (sm->checkIOInterrupt(xsink, "file readLine")) {
                    return -1;
                }
            }

            str.concat((char)ch);
            if (rc == -1)
                rc = 0;

            if (ch == '\r') {
                // see if next byte is \n' if we're not connected to a terminal device
                if (!tty) {
                    ch = readChar();
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

        // Check for sandbox interrupt support
        QoreSandboxManager* sm = runtime_get_sandbox_manager();

        int ch, rc = -1;
        int char_count = 0;
        const int check_interval = 100;  // Check interrupt every 100 characters

        while ((ch = readChar()) >= 0) {
            // Check for interrupt periodically
            if (sm && xsink && ++char_count >= check_interval) {
                char_count = 0;
                if (sm->checkIOInterrupt(xsink, "file readUntil")) {
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

        int rc = readUntil(bytes, **str, incl_bytes);

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

        while ((ch = readUnicode()) >= 0) {
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
                    ch = readUnicode(&len);
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

        while ((ch = readUnicode()) >= 0) {
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

        while ((ch = readUnicode()) >= 0) {
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

        // Check for sandbox interrupt support
        QoreSandboxManager* sm = runtime_get_sandbox_manager();

        // offset in bytes
        unsigned pos = 0;

        int ch, rc = -1;
        int char_count = 0;
        const int check_interval = 100;  // Check interrupt every 100 characters

        while ((ch = readChar()) >= 0) {
            // Check for interrupt periodically
            if (sm && xsink && ++char_count >= check_interval) {
                char_count = 0;
                if (sm->checkIOInterrupt(xsink, "file readUntil")) {
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
        // special files (stdout/stderr/stdin) are not closed anyway, so we don't mark the object closed in this case
        if (!special_file) {
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

    DLLLOCAL void do_read_event_unlocked(int bytes_read, int total_read, int bufsize) const {
        // post bytes read on event queue, if any
        if (event_queue) {
            QoreHashNode* h = getEvent(QORE_EVENT_DATA_READ);
            h->setKeyValue("read", bytes_read, nullptr);
            h->setKeyValue("total_read", total_read, nullptr);
            h->setKeyValue("total_to_read", bufsize, nullptr);
            event_queue->pushAndTakeRef(h);
        }
    }

    DLLLOCAL void do_write_event_unlocked(int bytes_written, int total_written, int bufsize) const {
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
