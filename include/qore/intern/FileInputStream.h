/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  FileInputStream.h

  Qore Programming Language

  Copyright (C) 2016 - 2024 Qore Technologies, s.r.o.

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

/**
 * @brief Private data for the Qore::FileInputStream class.
 */
class FileInputStream : public InputStream {
public:
    DLLLOCAL FileInputStream(const QoreStringNode* fileName, int64 timeout, int flags, ExceptionSink* xsink)
            : timeout(timeout) {
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
        return f.read(ptr, limit, timeout, xsink);
    }

    DLLLOCAL int64 peek(ExceptionSink *xsink) override {
        size_t pos = f.getPos(); // Save initial position.
        unsigned char c;
        size_t rc = f.read(&c, 1, -1, xsink);
        if (*xsink)
            return -2;
        if (rc == 0)
            return -1;
        f.setPos(pos); // Restore initial position.
        return c;
    }

    DLLLOCAL QoreFile& getFile() { return f; }

    DLLLOCAL int64 getTimeout() const { return timeout; }

    DLLLOCAL bool supportsNonBlockingIo() const override { return true; }

    DLLLOCAL int getPollableDescriptor() const override { return f.getFD(); }

    //! Non-blocking read — sets O_NONBLOCK for the duration of the read and restores it.
    /** @note Stream classes are single-threaded by design (enforced by thread affinity checks).
        The O_NONBLOCK flag toggle is safe because no other thread should access this stream
        concurrently.  However, if the underlying fd is shared with another descriptor (e.g. via
        dup()), the flag change is visible to all descriptors sharing the same file description.
    */
    DLLLOCAL int64 readNonBlock(void* ptr, int64 limit, ExceptionSink* xsink) override {
        int fd = f.getFD();
        if (fd < 0) {
            xsink->raiseException("FILE-READ-ERROR", "file is not open");
            return -1;
        }

        // Set O_NONBLOCK
        int flags = fcntl(fd, F_GETFL);
        if (flags == -1) {
            xsink->raiseException("FILE-READ-ERROR", "fcntl F_GETFL failed: %s", strerror(errno));
            return -1;
        }
        if (!(flags & O_NONBLOCK)) {
            if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
                xsink->raiseException("FILE-READ-ERROR", "fcntl F_SETFL failed: %s", strerror(errno));
                return -1;
            }
        }

        ssize_t rc = ::read(fd, ptr, limit);
        int saved_errno = errno;

        // Restore blocking mode
        if (!(flags & O_NONBLOCK)) {
            if (fcntl(fd, F_SETFL, flags) == -1) {
                printd(0, "FileInputStream::readNonBlock() WARNING: failed to restore blocking mode: %s\n",
                    strerror(errno));
            }
        }

        if (rc < 0) {
            if (saved_errno == EAGAIN || saved_errno == EWOULDBLOCK) {
                return 0;
            }
            xsink->raiseException("FILE-READ-ERROR", "read failed: %s", strerror(saved_errno));
            return -1;
        }
        return rc;
    }

private:
    QoreFile f;
    int64 timeout;
};

#endif // _QORE_FILEINPUTSTREAM_H
