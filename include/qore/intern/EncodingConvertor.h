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
#include "qore/intern/StringReaderHelper.h"
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

    Input that is not valid in the source encoding (a byte sequence that is not a character of the encoding, or a
    character cut off by the end of the input) raises \c INVALID-ENCODING with the byte offset of the sequence in the
    input, as the text readers do (qoretechnologies/qore#6730).  iconv fails with \c EILSEQ both for invalid input and
    for a character that the target encoding cannot hold, so the input at the offset where it fails is checked in the
    source encoding with q_text_invalid_offset(), the definition of valid text of the readers; only a valid character
    there gives \c ENCODING-CONVERSION-ERROR.  The \c encoding_errors option (qore_encoding_errors_t) can instead take
    each byte that does not start a valid character (each byte of such a code unit in a Unicode encoding form) as the
    ISO-8859-1 character of that byte (\c "latin1"), or, where the source and target encodings are the same, pass the
    bytes through without a check (\c "keep").

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

       @param srcEncoding the encoding of the input
       @param dstEncoding the encoding of the output
       @param xsink for exceptions opening the conversion
       @param mode what input that is not valid in the source encoding gives; qore_encoding_errors_t::KEEP is only
       allowed when the encodings are the same (see checkEncodingErrors()): the bytes are then passed as they are
       @param cls the name of the class of the stream, for the \c INVALID-ENCODING exception
       @param what what the input is (ex: \c "input"), for the \c INVALID-ENCODING exception
   */
   EncodingConvertor(const QoreEncoding *srcEncoding, const QoreEncoding *dstEncoding, ExceptionSink *xsink,
         qore_encoding_errors_t mode = qore_encoding_errors_t::RAISE, const char* cls = "EncodingConvertor",
         const char* what = "input")
         : srcEncoding(srcEncoding), dstEncoding(dstEncoding), curSrcEncoding(srcEncoding),
         checkBom(srcEncoding == QCS_UTF16 || srcEncoding == QCS_UTF16LE || srcEncoding == QCS_UTF16BE
            || !qore_encoding_private::get(*srcEncoding)->bom.empty()),
         bomSize(qore_encoding_private::get(*srcEncoding)->bom.empty()
            ? 2 : qore_encoding_private::get(*srcEncoding)->bom.size()), inCount(0), outCount(0), mode(mode),
         cls(cls), what(what) {
      if (mode == qore_encoding_errors_t::KEEP) {
         // the bytes are passed as they are
         assert(srcEncoding == dstEncoding);
         checkBom = false;
         return;
      }
      if (qore_encoding_private::isStateful(srcEncoding)) {
         // text in a stateful encoding is decoded to UTF-8 one byte at a time, and the UTF-8 text is converted
         decoder.reset(new Utf8OffsetDecoder(srcEncoding, xsink));
         if (xsink && *xsink) {
            return;
         }
         // the decoded text is valid UTF-8, so the inner conversion fails only for a character that the target
         // encoding cannot hold
         inner.reset(new EncodingConvertor(QCS_UTF8, dstEncoding, xsink));
         inner->outer = this;
         return;
      }
      conv.reset(new IconvHelper(dstEncoding, srcEncoding, xsink));
   }

   DLLLOCAL std::pair<int64, int64> apply(const void *src, int64 srcLen, void *dst, int64 dstLen, ExceptionSink *xsink) override {
      if (mode == qore_encoding_errors_t::KEEP) {
         return applyKeep(src, srcLen, dst, dstLen);
      }
      return decoder ? applyStateful(src, srcLen, dst, dstLen, xsink) : applyDirect(src, srcLen, dst, dstLen, xsink);
   }

   //! sets the name of the method of the stream that converts, for the \c INVALID-ENCODING exception
   DLLLOCAL void setMethod(const char* m) {
      meth = m;
   }

   //! returns the name of the method of the stream that converts
   DLLLOCAL const char* getMethod() const {
      return meth;
   }

   //! checks the \c encoding_errors option of a conversion stream
   /** @param src the source encoding
       @param dst the target encoding
       @param mode the option
       @param cls the name of the class of the stream, for the exception
       @param xsink for the \c ENCODING-ERRORS-ERROR exception raised if \a mode is qore_encoding_errors_t::KEEP and
       the encodings are not the same

       @return 0 for OK, -1 if an exception was raised
   */
   DLLLOCAL static int checkEncodingErrors(const QoreEncoding* src, const QoreEncoding* dst,
         qore_encoding_errors_t mode, const char* cls, ExceptionSink* xsink) {
      if (mode != qore_encoding_errors_t::KEEP || src == dst) {
         return 0;
      }
      xsink->raiseException("ENCODING-ERRORS-ERROR", "%s::constructor(): encoding_errors 'keep' passes the bytes "
         "as they are, so it requires the same source and target encodings; got source encoding \"%s\" and "
         "target encoding \"%s\"", cls, src->getCode(), dst->getCode());
      return -1;
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

   //! what input that is not valid in the source encoding gives
   qore_encoding_errors_t mode;
   //! the name of the class of the stream, for the INVALID-ENCODING exception
   const char* cls;
   //! the name of the method of the stream that converts, for the INVALID-ENCODING exception
   const char* meth = "read";
   //! what the input is, for the INVALID-ENCODING exception
   const char* what;
   //! true once output has been written; see substituteLatin1()
   bool wroteOutput = false;

   //! the room in the output buffer kept for the bytes that return a stateful target encoding to its initial state
   //! before a substitution; see substituteLatin1()
   static constexpr size_t ShiftResetRoom = 32;

   //! passes the input through without a check (qore_encoding_errors_t::KEEP)
   DLLLOCAL std::pair<int64, int64> applyKeep(const void *src, int64 srcLen, void *dst, int64 dstLen) {
      if (!src) {
         return std::make_pair(0, 0);
      }
      int64 n = QORE_MIN(srcLen, dstLen);
      if (n) {
         memcpy(dst, src, n);
      }
      return std::make_pair(n, n);
   }

   //! raises INVALID-ENCODING for input that is not valid in the source encoding at the given offset in the input
   DLLLOCAL void reportInvalid(int64 offset, ExceptionSink *xsink) {
      // the inner conversion of decoded text has valid UTF-8 input
      assert(!outer);
      if (xsink) {
         q_raise_invalid_read_text(xsink, cls, meth, what, decoder ? srcEncoding : curSrcEncoding, offset);
      }
   }

   //! returns true if the input in inBuf at the given position does not start with a valid character
   /** The bytes of one character at most are checked with q_text_invalid_offset(), the definition of valid text of
       the text readers (qore#6604): UTF-8 as RFC 3629 defines it, other multi-byte encodings and the Unicode forms
       with the character functions of the encoding, US-ASCII byte by byte; in a single-byte encoding, every byte is a
       character.  A valid start of a character cut off by the end of the bytes in inBuf is not invalid here: iconv
       reports it with \c EINVAL.
   */
   DLLLOCAL bool isInvalidInput(size_t pos) const {
      assert(pos < inCount);
      size_t n = QORE_MIN(inCount - pos, static_cast<size_t>(curSrcEncoding->getMaxCharWidth()));
      return !q_text_invalid_offset(inBuf + pos, n, curSrcEncoding, true);
   }

   //! returns the number of bytes substituted for input that is not valid at the given position in inBuf
   /** One code unit of a Unicode encoding form (UTF-16, UTF-32, UCS-2, UCS-4: the minimum character width of the
       encoding), so that the input after it is converted from a code unit boundary, or the bytes of it before the
       end of the input; one byte in any other encoding
   */
   DLLLOCAL size_t substitutionSize(size_t pos) const {
      size_t unit = IconvHelper::isUnicodeForm(curSrcEncoding) ? curSrcEncoding->getMinCharWidth() : 1;
      return QORE_MIN(unit, inCount - pos);
   }

   //! takes the bytes of the input in inBuf at the given position that do not start a valid character as the
   //! ISO-8859-1 characters of those bytes
   /** The bytes are one byte, or one code unit of a Unicode encoding form (see substitutionSize()).  The characters
       are converted to the target encoding on their own and written to the output buffer; for a stateful target
       encoding, the conversion of the input is first returned to its initial state, as the characters converted on
       their own start and end in the initial state.

       @return the number of bytes substituted, 0 if there is not enough room in the output buffer (the bytes are
       substituted when the output buffer has been read), -1 if an exception was raised: the target encoding cannot
       hold a character
   */
   DLLLOCAL int substituteLatin1(size_t pos, char*& outbuf, size_t& outavail, ExceptionSink *xsink) {
      size_t n = substitutionSize(pos);
      assert(n);
      QoreString c(dstEncoding);
      if (qore_string_private::convert_encoding_intern(inBuf + pos, n, QCS_ISO_8859_1, c, dstEncoding, nullptr)) {
         reportIllegalSequence(inTotal + static_cast<int64>(pos), xsink);
         return -1;
      }
      bool stateful = qore_encoding_private::isStateful(dstEncoding);
      if (stateful && (wroteOutput || outbuf != outBuf + outCount)) {
         // the bytes that start text in the encoding (ex: the header of ISO-2022-KR) are already in the output
         qore_string_private::get(c)->removeStatefulPrefix();
      }
      if (outavail < c.size() + (stateful ? ShiftResetRoom : 0)) {
         return 0;
      }
      if (stateful && conv->iconv(nullptr, nullptr, &outbuf, &outavail) == static_cast<size_t>(-1)) {
         conv->reportUnknownError(xsink);
         return -1;
      }
      memcpy(outbuf, c.c_str(), c.size());
      outbuf += c.size();
      outavail -= c.size();
      return static_cast<int>(n);
   }

   //! handles input that is not valid in the source encoding at the given position in inBuf
   /** @return the number of bytes at the position substituted (qore_encoding_errors_t::LATIN1), 0 if the
       substitution has to wait for room in the output buffer, -1 if an exception was raised
   */
   DLLLOCAL int handleInvalid(size_t pos, char*& outbuf, size_t& outavail, ExceptionSink *xsink) {
      if (mode != qore_encoding_errors_t::LATIN1) {
         reportInvalid(inTotal + static_cast<int64>(pos), xsink);
         return -1;
      }
      return substituteLatin1(pos, outbuf, outavail, xsink);
   }

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
            // true if the input at the first byte not converted is not valid in the source encoding
            bool invalid = false;
            switch (err) {
               case EINVAL:
                  if (src == nullptr) {         //flushing - there will be no more input
                     // a character cut off by the end of the input
                     invalid = true;
                  }
                  break;
               case E2BIG:
                  break;
               case EILSEQ:
                  // iconv fails with EILSEQ for invalid input and for a character that the target encoding cannot
                  // hold: the input is checked in the source encoding to tell them apart
                  if (!isInvalidInput(used)) {
                     reportIllegalSequence(inTotal + used, xsink);
                     return std::make_pair(0, 0);
                  }
                  invalid = true;
                  break;
               default:
                  errno = err;
                  conv->reportUnknownError(xsink);
                  return std::make_pair(0, 0);
            }
            if (invalid) {
               int hrc = handleInvalid(used, outbuf, outavail, xsink);
               if (hrc < 0) {
                  return std::make_pair(0, 0);
               }
               // the bytes substituted, if any, are consumed
               used += static_cast<size_t>(hrc);
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
         if (wc) {
            wroteOutput = true;
         }

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
            }, xsink, mode == qore_encoding_errors_t::LATIN1);
            if (rc == -2) {
               return std::make_pair(0, 0);
            }
            if (rc == -1) {
               // decoding to UTF-8 fails only for input that is not valid in the source encoding
               reportInvalid(decoder->errorOffset(), xsink);
               return std::make_pair(0, 0);
            }
         }
         int64 w = passUtf8(dst, dstLen, xsink);
         if (w < 0) {
            return std::make_pair(0, 0);
         }
         return std::make_pair(srcLen, w);
      }
      // no more input: a character or shift sequence cut off by the end of the input is invalid input
      while (decoder->hasPending()) {
         if (mode != qore_encoding_errors_t::LATIN1) {
            reportInvalid(decoder->pendingOffset(), xsink);
            return std::make_pair(0, 0);
         }
         // the first byte is taken as an ISO-8859-1 character, and the bytes after it are decoded again
         if (decoder->substituteFirstPending(utf8, [this] (size_t pos, int64 offset) {
               charOffsets.emplace_back(utf8Base + static_cast<int64>(pos), offset);
            }, xsink)) {
            return std::make_pair(0, 0);
         }
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

   //! raises ENCODING-CONVERSION-ERROR for a character that the target encoding cannot hold at the given offset in
   //! the input of this convertor
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
