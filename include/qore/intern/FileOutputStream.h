/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  FileOutputStream.h

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

#ifndef _QORE_FILEOUTPUTSTREAM_H
#define _QORE_FILEOUTPUTSTREAM_H

#include <stdint.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include "qore/OutputStream.h"
#include "qore/intern/FileStreamIo.h"

/**
 * @brief Private data for the Qore::FileOutputStream class.
 */
class FileOutputStream : public OutputStream {

public:
   DLLLOCAL FileOutputStream(const QoreStringNode *fileName, bool append, int mode, const QoreEncoding* encoding, ExceptionSink *xsink) {
      // a path with a NUL character would be cut at the NUL character by the operating system
      if (q_check_path(*fileName, "FILE-OPEN2-ERROR", xsink)) {
         return;
      }
      f.open2(xsink, fileName->getBuffer(), O_WRONLY | (append ? O_APPEND : O_TRUNC) | O_CREAT, mode, encoding);
   }

   DLLLOCAL FileOutputStream(const QoreStringNode *fileName, int flags, int mode, const QoreEncoding* encoding,
         ExceptionSink *xsink) {
      if (q_check_path(*fileName, "FILE-OPEN2-ERROR", xsink)) {
         return;
      }
      f.open2(xsink, fileName->getBuffer(), flags, mode, encoding);
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

   DLLLOCAL int getPollableDescriptor() const override { return f.isOpen() ? f.getFD() : -1; }

   //! Writes what can be written without waiting, without changing the descriptor's flags
   /** The descriptor's file status flags belong to its file description, which other holders can share (an
       inherited standard output, a duplicated descriptor, a child process), so O_NONBLOCK is never turned on for the
       write: a pipe, a FIFO or a terminal is written in chunks that a write after write readiness accepts without
       waiting, a socket with MSG_DONTWAIT, and a file or a descriptor that is already nonblocking directly (see
       FileStreamIo.h).

       @note The stream must not share a pipe with another concurrent writer: room that the other writer takes
       between the check and the write makes the write wait for the reader, also on the async I/O thread.
   */
   DLLLOCAL int64 writeNonBlock(const void* ptr, int64 count, ExceptionSink* xsink) override {
      int fd = f.isOpen() ? f.getFD() : -1;
      if (fd < 0) {
         xsink->raiseException("FILE-WRITE-ERROR", "file is not open");
         return -1;
      }
      int err;
      ssize_t rc = qore_file_stream_write_now(fd, ptr, static_cast<size_t>(count), err);
      if (rc < 0) {
         xsink->raiseException("FILE-WRITE-ERROR", "write failed: %s", strerror(err));
         return -1;
      }
      return rc;
   }

private:
   QoreFile f;
};

#endif // _QORE_FILEOUTPUTSTREAM_H
