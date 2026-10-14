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
#include "qore/intern/Utf8OffsetDecoder.h"
#include "qore/intern/qore_string_private.h"

#include <cassert>
#include <deque>
#include <memory>
#include <string>
#include <utility>

//! Converts a stream of text from one encoding to another
/** The conversion raises \c ENCODING-CONVERSION-ERROR for a character that the target encoding cannot hold, as
    convert_encoding() does, with the byte offset in the input of the first such character (qoretechnologies/qore#5479):
    - glibc and GNU libiconv fail with \c EILSEQ at the character
    - musl replaces the character and counts it; the converted block is then searched for the first character that
      cannot be converted
    - Apple's system libiconv replaces the character with an approximation and reports nothing, so where
      IconvHelper::rejectsUnrepresentable() is false, every converted block that can lose data (see
      qore_string_private::conversion_needs_roundtrip_check()) is verified by converting it on its own and back

    Text in a stateful encoding (ex: UTF-7, ISO-2022-JP) is decoded to UTF-8 one byte at a time (see Utf8OffsetDecoder),
    so that a block of input cut off in a character is decoded correctly with every iconv library, and the UTF-8 text
    is then converted to the target encoding; the offsets of errors are the offsets in the input.
*/
class EncodingConvertor : public Transform {

public:
   //! creates the convertor
   /** A byte order mark at the start of UTF-16 input (in the generic \c "UTF-16" encoding or in UTF-16LE or
       UTF-16BE) is not content and is removed; for the generic \c "UTF-16" encoding, it gives the byte order of
       the input, which is otherwise big-endian.  The same applies to the Unicode encodings created on the fly
       (ex: \c "UTF-32", \c "UTF-32LE", \c "UTF-32BE").
   */
   EncodingConvertor(const QoreEncoding *srcEncoding, const QoreEncoding *dstEncoding, ExceptionSink *xsink)
         : srcEncoding(srcEncoding), dstEncoding(dstEncoding), curSrcEncoding(srcEncoding),
         checkBom(srcEncoding == QCS_UTF16 || srcEncoding == QCS_UTF16LE || srcEncoding == QCS_UTF16BE
            || !qore_encoding_private::get(*srcEncoding)->bom.empty()),
         bomSize(qore_encoding_private::get(*srcEncoding)->bom.empty()
            ? 2 : qore_encoding_private::get(*srcEncoding)->bom.size()), inCount(0), outCount(0) {
      if (qore_encoding_private::isStateful(srcEncoding)) {
         // text in a stateful encoding is decoded to UTF-8 one byte at a time, and the UTF-8 text is converted
         decoder.reset(new Utf8OffsetDecoder(srcEncoding, xsink));
         if (xsink && *xsink) {
            return;
         }
         inner.reset(new EncodingConvertor(QCS_UTF8, dstEncoding, xsink));
         inner->outer = this;
         return;
      }
      conv.reset(new IconvHelper(dstEncoding, srcEncoding, xsink));
   }

   DLLLOCAL std::pair<int64, int64> apply(const void *src, int64 srcLen, void *dst, int64 dstLen, ExceptionSink *xsink) override {
      return decoder ? applyStateful(src, srcLen, dst, dstLen, xsink) : applyDirect(src, srcLen, dst, dstLen, xsink);
   }

private:
   static constexpr size_t BUFSIZE = 4096;
   const QoreEncoding *srcEncoding;
   const QoreEncoding *dstEncoding;
   //! the encoding of the input as converted: the encoding of a byte order mark found at the start of UTF-16 input
   const QoreEncoding *curSrcEncoding;
   //! true until the start of UTF-16 input has been checked for a byte order mark
   bool checkBom;
   //! the byte length of a byte order mark in the input encoding
   size_t bomSize;
   //! true once the bytes that return the output encoding to its initial state have been written at the end
   bool shiftWritten = false;
   std::unique_ptr<IconvHelper> conv;
   char inBuf[BUFSIZE];
   size_t inCount;
   char outBuf[BUFSIZE];
   size_t outCount;
   //! the byte offset in the input of the first byte in inBuf
   int64 inTotal = 0;

   //! decodes input in a stateful encoding to UTF-8; null for other input
   std::unique_ptr<Utf8OffsetDecoder> decoder;
   //! converts the UTF-8 text decoded from input in a stateful encoding to the target encoding
   std::unique_ptr<EncodingConvertor> inner;
   //! the UTF-8 text decoded and not yet given to the inner conversion
   std::string utf8;
   //! the offset in the decoded UTF-8 text of the first byte of utf8
   int64 utf8Base = 0;
   //! the offset in the decoded UTF-8 text and the offset in the input of the characters not yet converted
   std::deque<std::pair<int64, int64>> charOffsets;
   //! the convertor that decodes the input of this convertor from a stateful encoding, if any; errors are reported
   //! with its encodings and the offsets in its input
   EncodingConvertor* outer = nullptr;

   //! converts input in an encoding that is not stateful
   DLLLOCAL std::pair<int64, int64> applyDirect(const void *src, int64 srcLen, void *dst, int64 dstLen,
         ExceptionSink *xsink) {
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

         // a UTF-16 code unit is never converted from fewer than two bytes (a UTF-32 character from fewer than
         // four), so the input cannot have been converted before the bytes of a byte order mark are available
         if (checkBom && (inCount >= bomSize || src == nullptr) && removeBom(xsink)) {
            return std::make_pair(0, 0);
         }

         char *inbuf = inBuf;
         size_t inavail = inCount;
         char *outbuf = outBuf + outCount;
         size_t outavail = BUFSIZE - outCount;
         size_t rc = conv->iconv(&inbuf, &inavail, &outbuf, &outavail, xsink);
         int err = errno;
         if (rc == static_cast<size_t>(-1) && err == ECANCELED) {
            // the exception has been raised
            return std::make_pair(0, 0);
         }
         // the input converted is checked for characters that the target encoding cannot hold before any error
         // after it is reported, so that the first problem in the input is reported
         size_t used = inCount - inavail;
         if (used && checkLoss(used, rc, xsink)) {
            return std::make_pair(0, 0);
         }
         if (rc == static_cast<size_t>(-1)) {
            switch (err) {
               case EINVAL:
                  if (src == nullptr) {         //flushing - there will be no more input
                     reportIllegalSequence(inTotal + (inbuf - inBuf), xsink);
                     return std::make_pair(0, 0);
                  }
                  break;
               case E2BIG:
                  break;
               case EILSEQ:
                  reportIllegalSequence(inTotal + (inbuf - inBuf), xsink);
                  return std::make_pair(0, 0);
               default:
                  errno = err;
                  conv->reportUnknownError(xsink);
                  return std::make_pair(0, 0);
            }
         }
         if (used) {
            inCount -= used;
            inTotal += used;
            memmove(inBuf, inBuf + used, inCount);
         }

         // at the end of the input, write the bytes that return a stateful encoding (ex: UTF-7, ISO-2022-JP) to its
         // initial state; without them, the last characters of the output cannot be decoded
         if (src == nullptr && !inCount && !shiftWritten) {
            if (conv->iconv(nullptr, nullptr, &outbuf, &outavail) == static_cast<size_t>(-1)) {
               if (errno != E2BIG) {
                  conv->reportUnknownError(xsink);
                  return std::make_pair(0, 0);
               }
               // the output buffer is full; the bytes are written when it has been read
            } else {
               shiftWritten = true;
            }
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

   //! decodes input in a stateful encoding to UTF-8 and converts the UTF-8 text
   DLLLOCAL std::pair<int64, int64> applyStateful(const void *src, int64 srcLen, void *dst, int64 dstLen,
         ExceptionSink *xsink) {
      if (src) {
         // all of the input is decoded; the UTF-8 text is kept until the inner conversion takes it
         if (srcLen) {
            int rc = decoder->decode(static_cast<const char*>(src), srcLen, utf8, [this] (size_t pos, int64 offset) {
               charOffsets.emplace_back(utf8Base + static_cast<int64>(pos), offset);
            }, xsink);
            if (rc == -2) {
               return std::make_pair(0, 0);
            }
            if (rc == -1) {
               IconvHelper::reportIllegalSequence(decoder->errorOffset(), srcEncoding, dstEncoding, xsink);
               return std::make_pair(0, 0);
            }
         }
         int64 w = passUtf8(dst, dstLen, xsink);
         if (w < 0) {
            return std::make_pair(0, 0);
         }
         return std::make_pair(srcLen, w);
      }
      // no more input: a character or shift sequence cut off by the end of the input is an error
      if (decoder->hasPending()) {
         IconvHelper::reportIllegalSequence(decoder->pendingOffset(), srcEncoding, dstEncoding, xsink);
         return std::make_pair(0, 0);
      }
      int64 w = 0;
      if (!utf8.empty()) {
         w = passUtf8(dst, dstLen, xsink);
         if (w < 0) {
            return std::make_pair(0, 0);
         }
         if (!utf8.empty()) {
            // the output is full; the rest is converted at the next call
            return std::make_pair(0, w);
         }
      }
      std::pair<int64, int64> r = inner->apply(nullptr, 0, static_cast<char*>(dst) + w, dstLen - w, xsink);
      if (xsink && *xsink) {
         return std::make_pair(0, 0);
      }
      return std::make_pair(0, w + r.second);
   }

   //! gives the UTF-8 text decoded to the inner conversion; returns the number of bytes written, or -1 for an error
   DLLLOCAL int64 passUtf8(void *dst, int64 dstLen, ExceptionSink *xsink) {
      std::pair<int64, int64> r = inner->apply(utf8.data(), utf8.size(), dst, dstLen, xsink);
      if (xsink && *xsink) {
         return -1;
      }
      if (r.first) {
         utf8.erase(0, r.first);
         utf8Base += r.first;
      }
      // the offsets of characters before the first character of the inner conversion's input are not needed
      while (charOffsets.size() > 1 && charOffsets[1].first <= inner->inTotal) {
         charOffsets.pop_front();
      }
      return r.second;
   }

   //! returns the offset in the input of the character at the given offset in the decoded UTF-8 text
   DLLLOCAL int64 inputOffset(int64 utf8_offset) const {
      int64 rv = 0;
      for (const auto& c : charOffsets) {
         if (c.first > utf8_offset) {
            break;
         }
         rv = c.second;
      }
      return rv;
   }

   //! raises ENCODING-CONVERSION-ERROR for the given offset in the input of this convertor
   DLLLOCAL void reportIllegalSequence(int64 offset, ExceptionSink *xsink) {
      if (outer) {
         IconvHelper::reportIllegalSequence(outer->inputOffset(offset), outer->srcEncoding, outer->dstEncoding,
            xsink);
         return;
      }
      IconvHelper::reportIllegalSequence(offset, curSrcEncoding, dstEncoding, xsink);
   }

   //! checks the input converted for a character that the target encoding cannot hold
   /** @param used the number of bytes at the start of inBuf that have been converted
       @param rc the return value of the conversion
       @param xsink the exception is raised here

       @return 0 if nothing was lost, -1 if an exception was raised
   */
   DLLLOCAL int checkLoss(size_t used, size_t rc, ExceptionSink *xsink) {
      size_t offset;
      if (rc != static_cast<size_t>(-1) && rc > 0) {
         // a character was replaced and counted (musl); its position is not given
         offset = qore_string_private::find_unconvertible_char(inBuf, used, curSrcEncoding, dstEncoding, 0);
      } else if (!IconvHelper::rejectsUnrepresentable()
            && qore_string_private::conversion_needs_roundtrip_check(curSrcEncoding, dstEncoding, inBuf, used)) {
         // a character can have been replaced without being reported (Apple's libiconv): the input converted is
         // converted on its own and back
         QoreString check(dstEncoding);
         offset = 0;
         if (!qore_string_private::convert_encoding_intern(inBuf, used, curSrcEncoding, check, dstEncoding, nullptr,
               true, true, &offset)) {
            return 0;
         }
      } else {
         return 0;
      }
      reportIllegalSequence(inTotal + static_cast<int64>(offset), xsink);
      return -1;
   }

   //! removes a byte order mark from the start of UTF-16 input and resolves the byte order of generic UTF-16 input
   /** @return 0 for OK, -1 if an exception was raised
   */
   DLLLOCAL int removeBom(ExceptionSink *xsink) {
      assert(checkBom);
      checkBom = false;
      if (srcEncoding != QCS_UTF16 && srcEncoding != QCS_UTF16LE && srcEncoding != QCS_UTF16BE) {
         // a Unicode encoding created on the fly
         size_t bom_len;
         const QoreEncoding *bomEncoding = qore_encoding_private::get(*srcEncoding)->getBomEncoding(srcEncoding,
            inBuf, inCount, bom_len);
         if (!bomEncoding) {
            return 0;
         }
         inCount -= bom_len;
         inTotal += bom_len;
         memmove(inBuf, inBuf + bom_len, inCount);
         if (bomEncoding != srcEncoding) {
            conv.reset(new IconvHelper(dstEncoding, bomEncoding, xsink));
            if (*xsink) {
               return -1;
            }
            curSrcEncoding = bomEncoding;
         }
         return 0;
      }
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
      inTotal += 2;
      memmove(inBuf, inBuf + 2, inCount);
      if (srcEncoding == QCS_UTF16 && bomEncoding != QCS_UTF16BE) {
         conv.reset(new IconvHelper(dstEncoding, bomEncoding, xsink));
         if (*xsink) {
            return -1;
         }
         curSrcEncoding = bomEncoding;
      }
      return 0;
   }
};

#endif // _QORE_ENCODINGCONVERTOR_H
