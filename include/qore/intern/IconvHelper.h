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

#include <cerrno>
#include <iconv.h>

class IconvHelper {

public:
   DLLLOCAL IconvHelper(const QoreEncoding *to, const QoreEncoding *from, ExceptionSink *xsink) : to(to), from(from) {
#ifdef NEED_ICONV_TRANSLIT
      QoreString to_code(getIconvCode(to));
      to_code.concat("//TRANSLIT");
      c = iconv_open(to_code.getBuffer(), getIconvCode(from));
#else
      c = iconv_open(getIconvCode(to), getIconvCode(from));
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

   DLLLOCAL size_t iconv(char **inbuf, size_t *inavail, char **outbuf, size_t *outavail) {
      if (c == (iconv_t)-1) {
         errno = EINVAL;
         return (size_t)-1;
      }
      return iconv_adapter(::iconv, c, inbuf, inavail, outbuf, outavail);
   }

   //! Returns true if the iconv handle is valid
   DLLLOCAL bool isValid() const {
      return c != (iconv_t)-1;
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

private:
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
       q_remove_bom_utf16(), which strips the BOM and retags the string as \c QCS_UTF16BE or
       \c QCS_UTF16LE according to the BOM found.
    */
   DLLLOCAL static const char* getIconvCode(const QoreEncoding* enc) {
      return enc == QCS_UTF16 ? "UTF-16BE" : enc->getCode();
   }

   // needed for platforms where the input buffer is defined as "const char"
   template<typename T>
   static size_t iconv_adapter(size_t (*iconv_f)(iconv_t, T, size_t *, char **, size_t *), iconv_t handle,
         char **inbuf, size_t *inavail, char **outbuf, size_t *outavail) {
      return (*iconv_f) (handle, const_cast<T>(inbuf), inavail, outbuf, outavail);
   }

private:
   const QoreEncoding *to;
   const QoreEncoding *from;
   iconv_t c;
};

#endif // INCLUDE_QORE_INTERN_ICONVHELPER_H_
