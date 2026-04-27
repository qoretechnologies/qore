/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  BinaryOutputStream.h

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

#ifndef _QORE_BINARYOUTPUTSTREAM_H
#define _QORE_BINARYOUTPUTSTREAM_H

#include <stdint.h>
#include "qore/OutputStream.h"

/**
 * @brief Private data for the Qore::BinaryOutputStream class.
 */
class BinaryOutputStream : public OutputStream {

public:
   DLLLOCAL BinaryOutputStream() : buf(new BinaryNode) {
   }

   DLLLOCAL const char *getName() override {
      return "BinaryOutputStream";
   }

   //! BinaryOutputStream is memory-backed and write() never blocks; safe for I/O-thread-driven streaming
   DLLLOCAL bool isIoThreadSafe() const override { return true; }

   DLLLOCAL bool isClosed() override {
      return !buf;
   }

   DLLLOCAL void close(ExceptionSink* xsink) override {
      assert(!isClosed());
      buf = 0;
   }

   DLLLOCAL void write(const void *ptr, int64 count, ExceptionSink *xsink) override {
      assert(!isClosed());
      assert(count >= 0);
      buf->append(ptr, count);
   }

   DLLLOCAL BinaryNode *getData(ExceptionSink *xsink) {
      if (!check(xsink)) {
         return 0;
      }
      assert(!isClosed());
      BinaryNode *ret = buf.release();
      buf = new BinaryNode();
      return ret;
   }

private:
   SimpleRefHolder<BinaryNode> buf;
};

#endif // _QORE_BINARYOUTPUTSTREAM_H
