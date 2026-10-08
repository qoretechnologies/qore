/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  EncodingConvertor.h

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

#ifndef _QORE_ENCODINGCONVERTOR_H
#define _QORE_ENCODINGCONVERTOR_H

#include "qore/Transform.h"
#include "qore/intern/IconvHelper.h"

#include <cassert>
#include <memory>

class EncodingConvertor : public Transform {

public:
   //! creates the convertor
   /** A byte order mark at the start of UTF-16 input (in the generic \c "UTF-16" encoding or in UTF-16LE or
       UTF-16BE) is not content and is removed; for the generic \c "UTF-16" encoding, it gives the byte order of
       the input, which is otherwise big-endian.
   */
   EncodingConvertor(const QoreEncoding *srcEncoding, const QoreEncoding *dstEncoding, ExceptionSink *xsink)
         : srcEncoding(srcEncoding), dstEncoding(dstEncoding),
         checkBom(srcEncoding == QCS_UTF16 || srcEncoding == QCS_UTF16LE || srcEncoding == QCS_UTF16BE),
         conv(new IconvHelper(dstEncoding, srcEncoding, xsink)), inCount(0), outCount(0) {
   }

   DLLLOCAL std::pair<int64, int64> apply(const void *src, int64 srcLen, void *dst, int64 dstLen, ExceptionSink *xsink) override {

      const char *srcPtr = static_cast<const char *>(src);
      char *dstPtr = static_cast<char *>(dst);
      while (true) {
         size_t r = QORE_MIN(static_cast<size_t>(srcLen), BUFSIZE - inCount);
         if (r) {
            memcpy(inBuf + inCount, srcPtr, r);
            srcLen -= r;
            srcPtr += r;
            inCount += r;
         }

         // a UTF-16 code unit is never converted from fewer than two bytes, so the input cannot have been
         // converted before its first two bytes are available
         if (checkBom && (inCount >= 2 || src == nullptr) && removeBom(xsink)) {
            return std::make_pair(0, 0);
         }

         char *inbuf = inBuf;
         size_t inavail = inCount;
         char *outbuf = outBuf + outCount;
         size_t outavail = BUFSIZE - outCount;
         if (conv->iconv(&inbuf, &inavail, &outbuf, &outavail) == (size_t) -1) {
            switch (errno) {
               case EINVAL:
                  if (src == nullptr) {         //flushing - there will be no more input
                     conv->reportIllegalSequence(inbuf - inBuf, xsink);
                     return std::make_pair(0, 0);
                  }
                  break;
               case E2BIG:
                  break;
               case EILSEQ:
                  conv->reportIllegalSequence(inbuf - inBuf, xsink);
                  return std::make_pair(0, 0);
               default:
                  conv->reportUnknownError(xsink);
                  return std::make_pair(0, 0);
            }
         }
         size_t rc = inCount - inavail;
         if (rc) {
            inCount -= rc;
            memmove(inBuf, inBuf + rc, inCount);
         }

         size_t wc = BUFSIZE - outCount - outavail;
         outCount += wc;

         size_t w = QORE_MIN(static_cast<size_t>(dstLen), outCount);
         if (w) {
            memcpy(dstPtr, outBuf, w);
            dstPtr += w;
            dstLen -= w;
            outCount -= w;
            memmove(outBuf, outBuf + w, outCount);
         }

         if (r == 0 && w == 0) {
            return std::make_pair(srcPtr - static_cast<const char *>(src), dstPtr - static_cast<char *>(dst));
         }
      }
   }

private:
   static constexpr size_t BUFSIZE = 4096;
   const QoreEncoding *srcEncoding;
   const QoreEncoding *dstEncoding;
   //! true until the start of UTF-16 input has been checked for a byte order mark
   bool checkBom;
   std::unique_ptr<IconvHelper> conv;
   char inBuf[BUFSIZE];
   size_t inCount;
   char outBuf[BUFSIZE];
   size_t outCount;

   //! removes a byte order mark from the start of UTF-16 input and resolves the byte order of generic UTF-16 input
   /** @return 0 for OK, -1 if an exception was raised
   */
   DLLLOCAL int removeBom(ExceptionSink *xsink) {
      assert(checkBom);
      checkBom = false;
      if (inCount < 2) {
         return 0;
      }
      unsigned char b0 = static_cast<unsigned char>(inBuf[0]);
      unsigned char b1 = static_cast<unsigned char>(inBuf[1]);
      const QoreEncoding *bomEncoding;
      if (b0 == 0xfe && b1 == 0xff) {
         bomEncoding = QCS_UTF16BE;
      } else if (b0 == 0xff && b1 == 0xfe) {
         bomEncoding = QCS_UTF16LE;
      } else {
         return 0;
      }
      // in UTF-16LE or UTF-16BE, only the byte order mark of the declared byte order is one
      if (srcEncoding != QCS_UTF16 && srcEncoding != bomEncoding) {
         return 0;
      }
      inCount -= 2;
      memmove(inBuf, inBuf + 2, inCount);
      if (srcEncoding == QCS_UTF16 && bomEncoding != QCS_UTF16BE) {
         conv.reset(new IconvHelper(dstEncoding, bomEncoding, xsink));
         if (*xsink) {
            return -1;
         }
      }
      return 0;
   }
};

#endif // _QORE_ENCODINGCONVERTOR_H
