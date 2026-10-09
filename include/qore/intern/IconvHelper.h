//--------------------------------------------------------------------*- C++ -*-
//
//  Qore Programming Language
//
//  Copyright (C) 2016 - 2026 Qore Technologies, s.r.o.
//
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//
//------------------------------------------------------------------------------
///
/// \file
/// \brief Defines a helper class wrapping libiconv.
///
//------------------------------------------------------------------------------
#ifndef INCLUDE_QORE_INTERN_ICONVHELPER_H_
#define INCLUDE_QORE_INTERN_ICONVHELPER_H_

#include <qore/Qore.h>
#include "qore/intern/qore_encoding_private.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <iconv.h>

class IconvHelper {

public:
   //! opens the conversion
   /** @param to the target encoding
       @param from the source encoding
       @param xsink if an error occurs, the Qore-language exception is raised here
       @param validate_utf8 true to validate UTF-8 input here even if the platform's iconv rejects malformed UTF-8;
       by default, UTF-8 input is validated here only on a platform whose iconv accepts it (see rejectsMalformedUtf8())
   */
   DLLLOCAL IconvHelper(const QoreEncoding *to, const QoreEncoding *from, ExceptionSink *xsink,
         bool validate_utf8 = false) : to(to), from(from),
         validateUtf8(from == QCS_UTF8 && (validate_utf8 || !rejectsMalformedUtf8())),
         splitFinalDash(qore_encoding_private::get(*from)->utf7_final_dash_literal) {
#ifdef NEED_ICONV_TRANSLIT
      QoreString to_code(getIconvTargetCode(to));
      to_code.concat("//TRANSLIT");
      c = iconv_open(to_code.getBuffer(), getIconvCode(from));
#else
      c = iconv_open(getIconvTargetCode(to), getIconvCode(from));
#endif
      if (c == (iconv_t) -1) {
         if (xsink) {
            if (errno == EINVAL) {
               xsink->raiseException("ENCODING-CONVERSION-ERROR", "cannot convert from \"%s\" to \"%s\"",
                     from->getCode(), to->getCode());
            } else {
               reportUnknownError(xsink);
            }
         }
      }
   }

   DLLLOCAL ~IconvHelper() {
      if (c != (iconv_t) -1) {
         iconv_close(c);
      }
   }

   //! Converts input like iconv(3); malformed UTF-8 input is rejected on every platform
   /** If the platform's iconv accepts malformed UTF-8 input (see rejectsMalformedUtf8()), UTF-8 input is validated
       here, and the conversion stops at the first malformed sequence with \c EILSEQ, or at a sequence truncated by
       the end of the input with \c EINVAL, exactly as a strict iconv does.

       @param xsink if given, the validation is a cancellation point; when it is cancelled, the exception is raised
       here, and -1 is returned with \c errno set to \c ECANCELED
   */
   DLLLOCAL size_t iconv(char **inbuf, size_t *inavail, char **outbuf, size_t *outavail,
         ExceptionSink* xsink = nullptr) {
      if (c == (iconv_t)-1) {
         errno = EINVAL;
         return (size_t)-1;
      }
      if (splitFinalDash) {
         return iconvUtf7(inbuf, inavail, outbuf, outavail);
      }
      if (!validateUtf8 || !inbuf || !*inbuf) {
         return iconv_adapter(::iconv, c, inbuf, inavail, outbuf, outavail);
      }
      bool incomplete;
      bool cancelled = false;
      size_t valid = validUtf8Prefix(*inbuf, *inavail, incomplete, xsink, &cancelled);
      if (cancelled) {
         errno = ECANCELED;
         return (size_t)-1;
      }
      if (valid == *inavail) {
         return iconv_adapter(::iconv, c, inbuf, inavail, outbuf, outavail);
      }
      // convert the valid prefix, then report the sequence after it as iconv does
      size_t rest = *inavail - valid;
      size_t prefix = valid;
      size_t rc = iconv_adapter(::iconv, c, inbuf, &prefix, outbuf, outavail);
      *inavail = prefix + rest;
      if (rc == (size_t)-1) {
         return rc;
      }
      assert(!prefix);
      errno = incomplete ? EINVAL : EILSEQ;
      return (size_t)-1;
   }

   //! Returns the length of the longest prefix of the input that is complete, well-formed UTF-8 (RFC 3629)
   /** Overlong forms, surrogate code points, values above U+10FFFF, unexpected continuation bytes and the bytes
       \c 0xc0, \c 0xc1 and \c 0xf5 - \c 0xff are malformed.

       @param p the input
       @param len the byte length of the input
       @param incomplete set to true if the input after the prefix is a well-formed sequence truncated by the end of
       the input, false if it is malformed or the whole input is well-formed
       @param xsink if given, cancellation is checked every \c CancelCheckBytes bytes and raised here
       @param cancelled if given, set to true if the validation was cancelled; the return value is then undefined

       @return the byte length of the well-formed prefix
   */
   DLLLOCAL static size_t validUtf8Prefix(const char* p, size_t len, bool& incomplete,
         ExceptionSink* xsink = nullptr, bool* cancelled = nullptr) {
      const unsigned char* s = reinterpret_cast<const unsigned char*>(p);
      size_t i = 0;
      size_t next_check = CancelCheckBytes;
      while (i < len) {
         if (xsink && i >= next_check) {
            if (qore_check_cancel(xsink, "UTF-8 validation")) {
               incomplete = false;
               if (cancelled) {
                  *cancelled = true;
               }
               return i;
            }
            next_check = i + CancelCheckBytes;
         }
         unsigned char ch = s[i];
         if (ch < 0x80) {
            ++i;
            continue;
         }
         // the number of continuation bytes and the range of the first one
         size_t n;
         unsigned char lo = 0x80;
         unsigned char hi = 0xbf;
         if (ch >= 0xc2 && ch <= 0xdf) {
            n = 1;
         } else if (ch == 0xe0) {
            n = 2;
            lo = 0xa0;
         } else if ((ch >= 0xe1 && ch <= 0xec) || ch == 0xee || ch == 0xef) {
            n = 2;
         } else if (ch == 0xed) {
            n = 2;
            hi = 0x9f;
         } else if (ch == 0xf0) {
            n = 3;
            lo = 0x90;
         } else if (ch >= 0xf1 && ch <= 0xf3) {
            n = 3;
         } else if (ch == 0xf4) {
            n = 3;
            hi = 0x8f;
         } else {
            incomplete = false;
            return i;
         }
         for (size_t k = 1; k <= n; ++k) {
            if (i + k >= len) {
               // every byte up to the end of the input fits the sequence
               incomplete = true;
               return i;
            }
            unsigned char cb = s[i + k];
            if (k == 1 ? (cb < lo || cb > hi) : (cb < 0x80 || cb > 0xbf)) {
               incomplete = false;
               return i;
            }
         }
         i += n + 1;
      }
      incomplete = false;
      return len;
   }

   //! Returns true if the platform's iconv rejects malformed UTF-8 input
   /** glibc and GNU libiconv fail with \c EILSEQ on malformed UTF-8 input; some versions of Apple's system libiconv
       convert some malformed sequences, such as a lead byte followed by an ASCII character, without an error.  The
       answer is a property of the library %Qore is linked against, so it is probed once with malformed sequences;
       where any of them is accepted, iconv() validates UTF-8 input itself.
   */
   DLLLOCAL static bool rejectsMalformedUtf8() {
      static bool rv = probeMalformedUtf8Rejection();
      return rv;
   }

   //! Returns true if the iconv handle is valid
   DLLLOCAL bool isValid() const {
      return c != (iconv_t)-1;
   }

   //! Returns true if the platform's iconv reports non-reversible conversions
   /** glibc and GNU libiconv either fail with \c EILSEQ or return the number of characters
       converted non-reversibly, so a zero return really does mean that nothing was lost.
       Apple's system libiconv transliterates a character the target cannot represent - with
       and without the \c "//TRANSLIT" suffix - and still returns 0, so on that platform a
       successful conversion proves nothing and the result has to be verified by converting
       it back (see qore_string_private::convert_encoding_intern()).

       The answer is a property of the library %Qore is linked against, so it is probed once
       with a conversion known to be lossy: U+0178 (LATIN CAPITAL LETTER Y WITH DIAERESIS)
       has no representation in ISO-8859-1.  A platform that reports the loss keeps the
       cheaper path with no round trip at all.
    */
   DLLLOCAL static bool reportsNonReversibleConversions() {
      static bool rv = probeNonReversibleReporting();
      return rv;
   }

   void reportIllegalSequence(size_t offset, ExceptionSink *xsink) {
      if (xsink) {
         xsink->raiseException("ENCODING-CONVERSION-ERROR",
                               "illegal character sequence at byte offset " QLLD " found in input type \"%s\" (while converting to \"%s\")",
                               (int64)offset, from->getCode(), to->getCode());
      }
   }

   void reportUnknownError(ExceptionSink *xsink) {
      if (xsink) {
         xsink->raiseErrnoException("ENCODING-CONVERSION-ERROR", errno, "unknown error converting from \"%s\" to \"%s\"",
                                    from->getCode(), to->getCode());
      }
   }

   //! The number of bytes validated between cancellation checks
   static constexpr size_t CancelCheckBytes = 65536;

private:
   //! Performs the one-time probe described by rejectsMalformedUtf8()
   DLLLOCAL static bool probeMalformedUtf8Rejection() {
      iconv_t cd = iconv_open("UTF-16LE", "UTF-8");
      if (cd == (iconv_t)-1) {
         // the probe cannot run; assume the worst and validate UTF-8 input
         return false;
      }
      // a lead byte followed by ASCII, a lone continuation byte, an overlong form, a surrogate and a value above
      // U+10FFFF, each followed by ASCII text
      static const char* const samples[] = {
         "a\xc2 b", "a\x80 b", "a\xc0\x80 b", "a\xed\xa0\x80 b", "a\xf4\x90\x80\x80 b",
      };
      bool rv = true;
      for (const char* sample : samples) {
         char in[16];
         size_t il = strlen(sample);
         assert(il < sizeof(in));
         memcpy(in, sample, il);
         char out[64];
         char* ib = in;
         char* ob = out;
         size_t ol = sizeof(out);
         // reset the conversion state before each sample
         iconv_adapter(::iconv, cd, nullptr, nullptr, nullptr, nullptr);
         if (iconv_adapter(::iconv, cd, &ib, &il, &ob, &ol) != (size_t)-1) {
            rv = false;
            break;
         }
      }
      iconv_close(cd);
      return rv;
   }

   //! Performs the one-time probe described by reportsNonReversibleConversions()
   DLLLOCAL static bool probeNonReversibleReporting() {
#ifdef NEED_ICONV_TRANSLIT
      iconv_t cd = iconv_open("ISO-8859-1//TRANSLIT", "UTF-8");
#else
      iconv_t cd = iconv_open("ISO-8859-1", "UTF-8");
#endif
      if (cd == (iconv_t)-1) {
         // the probe cannot run; assume the worst and verify conversions by round trip
         return false;
      }
      char in[] = "\xc5\xb8";      // U+0178 in UTF-8
      char out[8];
      char* ib = in;
      char* ob = out;
      size_t il = 2;
      size_t ol = sizeof(out);
      errno = 0;
      size_t rc = iconv_adapter(::iconv, cd, &ib, &il, &ob, &ol);
      iconv_close(cd);
      // a failure (EILSEQ) or a non-zero count both mean the loss was reported
      return rc == (size_t)-1 || rc > 0;
   }

   //! Returns the encoding name to pass to iconv_open() for the given %Qore encoding
   /** %Qore's canonical form for a string tagged with the unsuffixed \c "UTF-16" encoding is
       big-endian with no byte order mark; this is what the decoding handlers registered for
       \c QCS_UTF16 in lib/charset.cpp expect, and it matches the Unicode default for a UTF-16
       stream that carries no BOM.

       iconv's \c "UTF-16" conversion does not produce that form: it emits a BOM followed by
       native-endian code units, so on a little-endian host every character would come back
       byte-swapped and the BOM would be decoded as U+FFFE.  \c "UTF-16BE" is therefore requested
       explicitly, in both directions, so that the bytes always agree with the encoding tag on
       the string.

       Byte order marks in externally-supplied data are resolved separately by
       q_remove_bom(), which strips the BOM and retags the string as \c QCS_UTF16BE or
       \c QCS_UTF16LE according to the BOM found.
    */
   DLLLOCAL static const char* getIconvCode(const QoreEncoding* enc) {
      return enc == QCS_UTF16 ? "UTF-16BE" : qore_encoding_private::get(*enc)->getIconvSourceCode();
   }

   //! Returns the encoding name to pass to iconv_open() to convert text to the given %Qore encoding
   /** As getIconvCode(), and for an encoding created on the fly whose iconv conversion writes a byte order mark
       (ex: \c "UTF-32"), the name of the encoding in the byte order of the characters without one (ex:
       \c "UTF-32BE"), which is what the character functions of the encoding expect; see
       qore_encoding_private::probe().  Text converted from a generic Unicode encoding (ex: \c "UTF-32") is converted
       with the explicit big-endian name (see getIconvCode()); a byte order mark at the start of the input is found
       by the caller with qore_encoding_private::getBomEncoding().
   */
   DLLLOCAL static const char* getIconvTargetCode(const QoreEncoding* enc) {
      return enc == QCS_UTF16 ? "UTF-16BE" : qore_encoding_private::get(*enc)->getIconvTargetCode();
   }

   // needed for platforms where the input buffer is defined as "const char"
   template<typename T>
   static size_t iconv_adapter(size_t (*iconv_f)(iconv_t, T, size_t *, char **, size_t *), iconv_t handle,
         char **inbuf, size_t *inavail, char **outbuf, size_t *outavail) {
      return (*iconv_f) (handle, const_cast<T>(inbuf), inavail, outbuf, outavail);
   }

   //! the state of UTF-7 input (RFC 2152) after the bytes converted so far
   enum Utf7State : unsigned char {
      //! directly encoded characters
      Utf7Direct,
      //! after the "+" that starts a modified base64 run (or "+-", which encodes "+")
      Utf7Plus,
      //! in a modified base64 run
      Utf7Base64,
   };

   //! returns true if the byte is a character of modified base64 in UTF-7
   DLLLOCAL static bool isUtf7Base64(unsigned char ch) {
      return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '+'
         || ch == '/';
   }

   //! returns the UTF-7 state after the given bytes
   DLLLOCAL static Utf7State advanceUtf7(Utf7State state, const char* p, size_t len) {
      for (size_t i = 0; i < len; ++i) {
         unsigned char ch = static_cast<unsigned char>(p[i]);
         switch (state) {
            case Utf7Direct:
               if (ch == '+') {
                  state = Utf7Plus;
               }
               break;
            case Utf7Plus:
               // "+-" is "+"; any other character that is not base64 ends the (empty) run
               state = isUtf7Base64(ch) ? Utf7Base64 : Utf7Direct;
               break;
            case Utf7Base64:
               if (!isUtf7Base64(ch)) {
                  // a "-" that ends a run is absorbed; any other character is a direct character
                  state = Utf7Direct;
               }
               break;
         }
      }
      return state;
   }

   //! converts UTF-7 input with an iconv that outputs a "-" ending a base64 run at the end of its input
   /** Apple's libiconv decodes the "-" that ends a modified base64 run (RFC 2152), which is part of the encoding and
       not a character, as a "-" character when it is the last byte of the input given to iconv(), and absorbs it
       otherwise; for example, it decodes "+AOk-" as "é-" but "+AOk-b" as "éb".  Such a "-" is therefore given to
       iconv() on its own, which it decodes correctly.  The encodings that need this are found by
       qore_encoding_private::probe().
   */
   DLLLOCAL size_t iconvUtf7(char **inbuf, size_t *inavail, char **outbuf, size_t *outavail) {
      if (!inbuf || !*inbuf) {
         // a reset or the end of the output returns to the initial state
         utf7State = Utf7Direct;
         return iconv_adapter(::iconv, c, inbuf, inavail, outbuf, outavail);
      }
      const char* start = *inbuf;
      const size_t total = *inavail;
      size_t rc;
      if (total > 1 && start[total - 1] == '-' && advanceUtf7(utf7State, start, total - 1) == Utf7Base64) {
         size_t head = total - 1;
         rc = iconv_adapter(::iconv, c, inbuf, &head, outbuf, outavail);
         *inavail = head + 1;
         if (rc != static_cast<size_t>(-1)) {
            assert(!head);
            size_t dash = 1;
            size_t rc2 = iconv_adapter(::iconv, c, inbuf, &dash, outbuf, outavail);
            *inavail = dash;
            rc = (rc2 == static_cast<size_t>(-1)) ? rc2 : rc + rc2;
         }
      } else {
         rc = iconv_adapter(::iconv, c, inbuf, inavail, outbuf, outavail);
      }
      // errno is not changed by advanceUtf7()
      utf7State = advanceUtf7(utf7State, start, *inbuf - start);
      return rc;
   }

private:
   const QoreEncoding *to;
   const QoreEncoding *from;
   //! true if UTF-8 input is validated here because the platform's iconv accepts malformed UTF-8
   bool validateUtf8;
   //! true if a "-" ending a UTF-7 base64 run at the end of the input is converted on its own; see iconvUtf7()
   bool splitFinalDash;
   //! the UTF-7 state after the input converted so far, if splitFinalDash is set
   Utf7State utf7State = Utf7Direct;
   iconv_t c;
};

#endif // INCLUDE_QORE_INTERN_ICONVHELPER_H_
