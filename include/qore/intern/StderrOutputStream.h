/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  StderrOutputStream.h

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

#ifndef _QORE_STDERROUTPUTSTREAM_H
#define _QORE_STDERROUTPUTSTREAM_H

#include "qore/OutputStream.h"
#include <qore/QoreSandboxManager.h>

#include <unistd.h>
#include <poll.h>
#include <cerrno>

/**
 * @brief Private data for the Qore::StderrOutputStream class.
 */
class StderrOutputStream : public OutputStream {

public:
   DLLLOCAL StderrOutputStream() {
   }

   DLLLOCAL const char *getName() override {
      return "StderrOutputStream";
   }

   DLLLOCAL bool isClosed() override {
      return false;
   }

   DLLLOCAL void close(ExceptionSink* xsink) override {
      return;
   }

   DLLLOCAL void write(const void *ptr, int64 count, ExceptionSink *xsink) override {
      assert(count >= 0);
      if (count <= 0)
         return;

      // Check for sandbox interrupt support
      QoreSandboxManager* sm = runtime_get_sandbox_manager();

      const char* current = reinterpret_cast<const char*>(ptr);
      int64 remaining = count;

      while (remaining > 0) {
         // Check for interrupt
         if (sm && sm->checkIOInterrupt(xsink, "stderr write")) {
            return;
         }

         // Write in blocks
         size_t to_write = remaining > WRITE_BLOCK_SIZE ? WRITE_BLOCK_SIZE : remaining;
         ssize_t written = ::write(STDERR_FILENO, current, to_write);

         if (written < 0) {
            if (errno == EINTR) {
               continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
               // Would block - poll for write readiness
               if (sm) {
                  const int poll_ms = QORE_IO_POLL_INTERVAL_MS;
                  struct pollfd pfd;
                  pfd.fd = STDERR_FILENO;
                  pfd.events = POLLOUT;
                  poll(&pfd, 1, poll_ms);
               }
               continue;
            }
            xsink->raiseErrnoException("STDERR-WRITE-ERROR", errno, "error writing to stderr");
            return;
         }

         current += written;
         remaining -= written;
      }
   }
private:
   static const int WRITE_BLOCK_SIZE = 1024;
};

#endif // _QORE_STDERROUTPUTSTREAM_H
