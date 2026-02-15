/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  StringReaderHelper.h

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

#ifndef _QORE_STRINGREADERHELPER_H
#define _QORE_STRINGREADERHELPER_H

#include <functional>
#include <type_traits>

using namespace std::placeholders;

//! the maximum buffer/read size for a single read
#define DefaultStreamReaderHelperBufferSize 4096

typedef std::function<qore_offset_t(void*, size_t, ExceptionSink*)> f_read_t;

//! remove any BOM in UTF-16 strings, adjust the encoding if required
DLLLOCAL QoreString* q_remove_bom_utf16(QoreString* str, const QoreEncoding*& enc);

//! remove any BOM in UTF-16 strings, adjust the encoding if required
DLLLOCAL QoreStringNode* q_remove_bom_utf16(QoreStringNode* str, const QoreEncoding*& enc);

//! helper function for reading all possible data and returning it as a string
/** @param xsink for Qore-language exceptions
    @param enc the encoding of the input data and the output string
    @param my_read a function object taking the arguments above, the return value means:
    - \c < 0: an error occurred (xsink has the exception info), 0 = end of data, > 0 the number of bytes read
 */
DLLLOCAL QoreStringNode* q_read_string_all(ExceptionSink* xsink, const QoreEncoding* enc, f_read_t my_read);

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
