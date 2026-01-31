/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  FileOutputStream.h

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

#ifndef _QORE_FILEOUTPUTSTREAM_H
#define _QORE_FILEOUTPUTSTREAM_H

#include <stdint.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include "qore/OutputStream.h"

/**
 * @brief Private data for the Qore::FileOutputStream class.
 */
class FileOutputStream : public OutputStream {

public:
   DLLLOCAL FileOutputStream(const QoreStringNode *fileName, bool append, int mode, const QoreEncoding* encoding, ExceptionSink *xsink) {
      f.open2(xsink, fileName->getBuffer(), O_WRONLY | (append ? O_APPEND : O_TRUNC) | O_CREAT, mode, encoding);
   }

   DLLLOCAL FileOutputStream(int fd) {
      f.makeSpecial(fd);
   }

   DLLLOCAL const char *getName() override {
      return "FileOutputStream";
   }

   DLLLOCAL bool isClosed() override {
      return !f.isOpen();
   }

   DLLLOCAL void close(ExceptionSink* xsink) override {
      assert(!isClosed());
      int rc = f.close();
      if (rc) {
         xsink->raiseException("FILE-CLOSE-ERROR", "Error %d closing file", rc);
      }
   }

   DLLLOCAL void write(const void *ptr, int64 count, ExceptionSink *xsink) override {
      assert(!isClosed());
      assert(count >= 0);
      if (f.write(ptr, count, xsink) != count) {
         xsink->raiseException("FILE-WRITE-ERROR", "Error writing to file");
      }
   }

   DLLLOCAL const QoreEncoding* getEncoding() const { return f.getEncoding(); }

   DLLLOCAL bool supportsNonBlockingIo() const override { return true; }

   DLLLOCAL int getPollableDescriptor() const override { return f.getFD(); }

   //! Non-blocking write — sets O_NONBLOCK for the duration of the write and restores it.
   /** @note Stream classes are single-threaded by design (enforced by thread affinity checks).
       The O_NONBLOCK flag toggle is safe because no other thread should access this stream
       concurrently.  However, if the underlying fd is shared with another descriptor (e.g. via
       dup()), the flag change is visible to all descriptors sharing the same file description.
   */
   DLLLOCAL int64 writeNonBlock(const void* ptr, int64 count, ExceptionSink* xsink) override {
      int fd = f.getFD();
      if (fd < 0) {
         xsink->raiseException("FILE-WRITE-ERROR", "file is not open");
         return -1;
      }

      // Set O_NONBLOCK
      int flags = fcntl(fd, F_GETFL);
      if (flags == -1) {
         xsink->raiseException("FILE-WRITE-ERROR", "fcntl F_GETFL failed: %s", strerror(errno));
         return -1;
      }
      if (!(flags & O_NONBLOCK)) {
         if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            xsink->raiseException("FILE-WRITE-ERROR", "fcntl F_SETFL failed: %s", strerror(errno));
            return -1;
         }
      }

      ssize_t rc = ::write(fd, ptr, count);
      int saved_errno = errno;

      // Restore blocking mode
      if (!(flags & O_NONBLOCK)) {
         if (fcntl(fd, F_SETFL, flags) == -1) {
            printd(0, "FileOutputStream::writeNonBlock() WARNING: failed to restore blocking mode: %s\n",
               strerror(errno));
         }
      }

      if (rc < 0) {
         if (saved_errno == EAGAIN || saved_errno == EWOULDBLOCK) {
            return 0;
         }
         xsink->raiseException("FILE-WRITE-ERROR", "write failed: %s", strerror(saved_errno));
         return -1;
      }
      return rc;
   }

private:
   QoreFile f;
};

#endif // _QORE_FILEOUTPUTSTREAM_H
