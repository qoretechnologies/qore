/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  StringReaderHelper.h

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

#ifndef _QORE_STRINGREADERHELPER_H
#define _QORE_STRINGREADERHELPER_H

#include <functional>
#include <type_traits>

using namespace std::placeholders;

//! the maximum buffer/read size for a single read
#define DefaultStreamReaderHelperBufferSize 4096

typedef std::function<qore_offset_t(void*, size_t, ExceptionSink*)> f_read_t;

//! removes a byte order mark from the start of a string in a Unicode encoding and adjusts the encoding if required
/** For the UTF-16 encodings and the Unicode encodings created on the fly (UTF-32, UCS-2, ...): the byte order mark of
    the encoding's byte order is removed, and in an encoding without a byte order (ex: \c "UTF-16" or \c "UTF-32"),
    a byte order mark in either byte order is removed and the string and \a enc get the encoding of that byte order
    (ex: \c "UTF-32LE").  Strings in other encodings are not changed.
*/
DLLLOCAL QoreString* q_remove_bom(QoreString* str, const QoreEncoding*& enc);

//! removes a byte order mark from the start of a string in a Unicode encoding and adjusts the encoding if required
/** @see q_remove_bom(QoreString*, const QoreEncoding*&)
*/
DLLLOCAL QoreStringNode* q_remove_bom(QoreStringNode* str, const QoreEncoding*& enc);

//! helper function for reading all possible data and returning it as a string
/** @param xsink for Qore-language exceptions
    @param enc the encoding of the input data and the output string
    @param my_read a function object taking the arguments above, the return value means:
    - \c < 0: an error occurred (xsink has the exception info), 0 = end of data, > 0 the number of bytes read
 */
DLLLOCAL QoreStringNode* q_read_string_all(ExceptionSink* xsink, const QoreEncoding* enc, f_read_t my_read);

//! raises an exception if text in the given encoding cannot be read a number of characters at a time
/** Text in a stateful encoding (ex: UTF-7, ISO-2022-JP) cannot be read one character or a number of characters at a
    time: the bytes of the characters after the ones read depend on the shift state at the end of the ones read,
    which a following read would not have; such text is read in lines (which end in the initial shift state), all at
    once, or in bytes (see qoretechnologies/qorus#709 for readers that keep the shift state between reads)

    @param enc the encoding of the text
    @param xsink for the \c UNSUPPORTED-ENCODING exception

    @return 0 if the text can be read a number of characters at a time, -1 if an exception was raised
*/
DLLLOCAL int q_check_char_read_encoding(const QoreEncoding* enc, ExceptionSink* xsink);

//! helper function for reading valid strings with character semantics
/** @param xsink for Qore-language exceptions
    @param size the nubmer of characters to read, negative values = read all available data
    @param enc the encoding of the input data and the output string (must be ASCII compatible)
    @param my_read a function object taking the arguments above, the return value means:
    - \c < 0: an error occurred (xsink has the exception info), 0 = end of data, > 0 the number of bytes read

    @return the string returned, note that if \a size = 0 (or a Qore-language exception occurs), nullptr is returned, oitherwise the caller owns the QoreStringNode reference returned
 */
DLLLOCAL QoreStringNode* q_read_string(ExceptionSink* xsink, int64 size, const QoreEncoding* enc, f_read_t my_read);

//! helper function for reading a single block of string data with character semantics
/** @param xsink for Qore-language exceptions
    @param size the number of characters to read, negative values = read a single block of available data
    @param enc the encoding of the input data and the output string (must be ASCII compatible)
    @param my_read a function object taking the arguments above, the return value means:
    - \c < 0: an error occurred (xsink has the exception info), 0 = end of data, > 0 the number of bytes read

    @return the string returned; if no data could be read (or a Qore-language exception occurs), nullptr is returned;
    otherwise the caller owns the QoreStringNode reference returned
 */
DLLLOCAL QoreStringNode* q_read_string_short(ExceptionSink* xsink, int64 size, const QoreEncoding* enc,
    f_read_t my_read);

#endif
