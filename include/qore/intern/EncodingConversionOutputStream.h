/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  EncodingConversionOutputStream.h

  Qore Programming Language

  Copyright (C) 2023 - 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_ENCODINGCONVERSIONOUTPUTSTREAM_H
#define _QORE_ENCODINGCONVERSIONOUTPUTSTREAM_H

#include "qore/intern/TransformOutputStream.h"
#include "qore/intern/EncodingConvertor.h"

/**
 * @brief Private data for the Qore::EncodingConversionOutputStream class.
 */
class EncodingConversionOutputStream : public TransformOutputStream {

public:
   DLLLOCAL EncodingConversionOutputStream(OutputStream *os, const QoreEncoding *srcEncoding,
         const QoreEncoding *dstEncoding, ExceptionSink *xsink,
         qore_encoding_errors_t mode = qore_encoding_errors_t::RAISE)
         : EncodingConversionOutputStream(os, new EncodingConvertor(srcEncoding, dstEncoding, xsink, mode,
            "EncodingConversionOutputStream", "data written"), mode) {
   }

   DLLLOCAL const char *getName() override {
      return "EncodingConversionOutputStream";
   }

   //! returns the encoding_errors option of the stream
   DLLLOCAL qore_encoding_errors_t getEncodingErrors() const {
      return mode;
   }

   DLLLOCAL void close(ExceptionSink* xsink) override {
      // errors in the data written (a character cut off at the end) are reported for the method called; the
      // method is not restored, as the stream cannot be used once it has been closed
      conv->setMethod("close");
      TransformOutputStream::close(xsink);
   }

private:
   //! the convertor, owned by the base class
   EncodingConvertor* conv;
   qore_encoding_errors_t mode;

   DLLLOCAL EncodingConversionOutputStream(OutputStream *os, EncodingConvertor* conv, qore_encoding_errors_t mode)
         : TransformOutputStream(os, conv), conv(conv), mode(mode) {
      conv->setMethod("write");
   }
};

#endif // _QORE_ENCODINGCONVERSIONOUTPUTSTREAM_H
