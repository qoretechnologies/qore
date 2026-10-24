/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  FileInputStream.h

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

#ifndef _QORE_FILEINPUTSTREAM_H
#define _QORE_FILEINPUTSTREAM_H

#include <stdint.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include "qore/InputStream.h"
#include "qore/intern/FileStreamIo.h"
#include "qore/intern/qore_qf_private.h"

/**
 * @brief Private data for the Qore::FileInputStream class.
 */
class FileInputStream : public InputStream {
public:
    DLLLOCAL FileInputStream(const QoreStringNode* fileName, int64 timeout, int flags, ExceptionSink* xsink)
            : timeout(timeout) {
        // a path with a NUL character would be cut at the NUL character by the operating system
        if (q_check_path(*fileName, "FILE-OPEN2-ERROR", xsink)) {
            return;
        }
        f.open2(xsink, fileName->c_str(), O_RDONLY | flags);
    }

    DLLLOCAL FileInputStream(int fd) : timeout(-1) {
        f.makeSpecial(fd);
    }

    DLLLOCAL const char *getName() override {
        return "FileInputStream";
    }

    DLLLOCAL int64 read(void *ptr, int64 limit, ExceptionSink *xsink) override {
        assert(limit > 0);
        if (peeked >= 0) {
            return readPeeked(ptr, limit);
        }
        return f.read(ptr, limit, timeout, xsink);
    }

    //! Returns the next byte without reading it
    /** A regular file or a block device is read again from the position before the byte.  Other descriptors (a
        pipe, a FIFO, a terminal, a socket) cannot be repositioned: the byte is kept by the stream and returned by
        the next read, as the first byte of the data read.

        @return the next byte, -1 at the end of the stream, or -2 if an exception was raised (\c FILE-READ-ERROR,
        \c FILE-READ-TIMEOUT-ERROR after the stream's timeout)
    */
    DLLLOCAL int64 peek(ExceptionSink *xsink) override {
        if (peeked >= 0) {
            return peeked;
        }
        if (!f.isOpen()) {
            xsink->raiseException("FILE-READ-ERROR", "file is not open");
            return -2;
        }
        unsigned char c;
        if (qore_file_stream_fd_kind(f.getFD()) == QFSFK_DIRECT) {
            size_t pos = f.getPos(); // Save initial position.
            size_t rc = f.read(&c, 1, timeout, xsink);
            if (*xsink) {
                return -2;
            }
            if (rc == 0) {
                return -1;
            }
            f.setPos(pos); // Restore initial position.
            return c;
        }
        size_t rc = f.read(&c, 1, timeout, xsink);
        if (*xsink) {
            return -2;
        }
        if (rc == 0) {
            return -1;
        }
        peeked = c;
        return c;
    }

    //! Reads with a timeout; the byte that peek() kept is returned at once
    DLLLOCAL int64 readWithTimeout(void* ptr, int64 limit, int64 timeout_ms, ExceptionSink* xsink) override {
        if (peeked >= 0 && limit > 0) {
            return readPeeked(ptr, limit);
        }
        return InputStream::readWithTimeout(ptr, limit, timeout_ms, xsink);
    }

    DLLLOCAL QoreFile& getFile() { return f; }

    DLLLOCAL int64 getTimeout() const { return timeout; }

    DLLLOCAL bool supportsNonBlockingIo() const override { return true; }

    DLLLOCAL int getPollableDescriptor() const override { return f.isOpen() ? f.getFD() : -1; }

    //! Reads what is available without waiting, without changing the descriptor's flags
    /** The descriptor's file status flags belong to its file description, which other holders can share (an
        inherited standard input, a duplicated descriptor, a child process), so O_NONBLOCK is never turned on for the
        read: a pipe, a FIFO or a terminal is read once a check that does not wait reports it readable, a socket is
        read with MSG_DONTWAIT, and a file or a descriptor that is already nonblocking is read directly (see
        FileStreamIo.h).

        @note The stream must not share a pipe with another concurrent reader: data that the other reader takes
        between the check and the read makes the read wait for more data, also on the async I/O thread.
    */
    DLLLOCAL int64 readNonBlock(void* ptr, int64 limit, ExceptionSink* xsink) override {
        int fd = f.isOpen() ? f.getFD() : -1;
        if (fd < 0) {
            xsink->raiseException("FILE-READ-ERROR", "file is not open");
            return -1;
        }
        if (peeked >= 0 && limit > 0) {
            return readPeeked(ptr, limit);
        }
#ifdef DARWIN
        // at the end of a named pipe that a read has returned, the waits report the pipe readable at once, so a read
        // is made only if the pipe has data (see qore_qf_private::fifo_end_read)
        if (f.priv->fifo_end_read) {
            int ready = f.priv->fifoEndReadReady();
            if (ready < 0) {
                xsink->raiseException("FILE-READ-ERROR", "read failed: %s", strerror(errno));
                return -1;
            }
            if (!ready) {
                return 0;
            }
        }
#endif
        int err;
        bool read_done;
        ssize_t rc = qore_file_stream_read_now(fd, ptr, static_cast<size_t>(limit), err, &read_done);
        if (rc < 0) {
            xsink->raiseException("FILE-READ-ERROR", "read failed: %s", strerror(err));
            return -1;
        }
        if (read_done) {
            // the end of a named pipe is recorded for the waits on the stream's descriptor
            f.priv->noteReadResult(rc, static_cast<size_t>(limit));
        }
        return rc;
    }

private:
    QoreFile f;
    int64 timeout;
    //! the byte that peek() read from a descriptor that cannot be repositioned, or -1
    /** Returned by the next read.  A wait for the stream's descriptor does not see it (for example a wait of the
        I/O thread before a read with readNonBlock()), so the byte is returned with the next data or at the end of the
        input
    */
    int peeked = -1;

    //! returns the byte that peek() kept, followed by the data that can be read at once, without waiting
    /** An error of the read after the byte is returned by the next read
    */
    DLLLOCAL int64 readPeeked(void* ptr, int64 limit) {
        assert(peeked >= 0 && limit > 0);
        unsigned char* p = static_cast<unsigned char*>(ptr);
        p[0] = static_cast<unsigned char>(peeked);
        peeked = -1;
        if (limit == 1 || !f.isOpen()) {
            return 1;
        }
        int err;
        bool read_done;
        ssize_t rc = qore_file_stream_read_now(f.getFD(), p + 1, static_cast<size_t>(limit - 1), err, &read_done);
        if (read_done) {
            // the end of a named pipe is recorded for the waits on the stream's descriptor
            f.priv->noteReadResult(rc, static_cast<size_t>(limit - 1));
        }
        return rc > 0 ? 1 + rc : 1;
    }
};

#endif // _QORE_FILEINPUTSTREAM_H
