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

#include "qore/intern/IconvHelper.h"

#include <functional>
#include <memory>
#include <string>
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

//! raises an exception if text in the given encoding cannot be read a number of characters at a time without state
/** Text in a stateful encoding (ex: UTF-7, ISO-2022-JP) cannot be read a number of characters at a time by a reader
    that keeps no state: the bytes of the characters after the ones read depend on the shift state at the end of the
    ones read; readers keep it in a QoreStatefulTextDecoder

    @param enc the encoding of the text
    @param xsink for the \c UNSUPPORTED-ENCODING exception

    @return 0 if the text can be read a number of characters at a time, -1 if an exception was raised
*/
DLLLOCAL int q_check_char_read_encoding(const QoreEncoding* enc, ExceptionSink* xsink);

//! reads text in a stateful encoding (ex: UTF-7, ISO-2022-JP, IBM930) by its characters, keeping the shift state
/** In a stateful encoding, the bytes of a character depend on the shift state left by the characters before it, so a
    reader that returns some of the characters of a shifted run (ex: 3 characters after \c "ESC $ B" in ISO-2022-JP)
    must continue in that state at its next read.  A reader object keeps one decoder, which decodes the bytes read to
    \c UTF-8 with an iconv conversion that persists across reads; the text returned is converted back to the
    encoding, so each string returned starts and ends in the initial shift state and is valid on its own.

    Bytes are read no further than the end of the characters returned: every character has at least one byte, so the
    reader reads no more bytes than the number of characters still missing, and iconv leaves an incomplete sequence
    unconverted until its other bytes are read; a following character read with a byte read on its own after a
    carriage return (see readLine()) is kept for the next read of text.

    Once text has been read with the decoder, its shift state is not known to be the initial one, so the reader reads
    all of its text with the decoder (see active()), until reset() is called when the position or encoding of the
    reader changes.  Bytes read directly from the stream do not change the shift state.

    The object is not thread-safe; the reader serializes access to it.
*/
class QoreStatefulTextDecoder {
public:
    //! returns true if text has been read with the decoder since it was created or reset
    /** The text after it continues in the shift state kept by the decoder, so it must be read with the decoder
    */
    DLLLOCAL bool active() const {
        return (bool)cd;
    }

    //! returns the decoder to the initial shift state and discards any text decoded ahead
    /** Called when the position or the encoding of the reader changes
    */
    DLLLOCAL void reset() {
        cd.reset();
        cd_enc = nullptr;
        ahead.clear();
        pending.clear();
    }

    //! reads a number of characters, or all of the remaining text
    /** @param enc the stateful encoding of the text and of the string returned
        @param size the number of characters to read; negative values read all of the remaining text
        @param my_read a function object that reads bytes; see f_read_t
        @param xsink for Qore-language exceptions; \c STREAM-ENCODING-ERROR is raised for an invalid byte sequence or
        for a character cut off by the end of the data

        @return the text read in \a enc, or nullptr if there are no more characters, \a size is 0, or an exception
        was raised; the caller owns the reference returned
    */
    DLLLOCAL QoreStringNode* readChars(const QoreEncoding* enc, int64 size, f_read_t my_read, ExceptionSink* xsink);

    //! reads a line of text
    /** @param enc the stateful encoding of the text and of the string returned
        @param eol the end-of-line marker in \c UTF-8; if empty, a line ends with \c "\n", \c "\r", or
        \c "\r\n"
        @param trim if true, the end-of-line marker is not included in the string returned
        @param cr_lookahead if true, the character after a carriage return is read to find a \c "\r\n" line end
        (it is kept for the next read of text if it is not a line feed); if false, a carriage return ends the line
        (for terminals)
        @param my_read a function object that reads bytes; see f_read_t
        @param xsink for Qore-language exceptions
        @param found_eol if not null, set to the end-of-line marker that ended the line in \c UTF-8, or to an empty
        string if the line ended at the end of the text

        @return the line read in \a enc, or nullptr at the end of the text or if an exception was raised; the caller
        owns the reference returned
    */
    DLLLOCAL QoreStringNode* readLine(const QoreEncoding* enc, const std::string& eol, bool trim, bool cr_lookahead,
            f_read_t my_read, ExceptionSink* xsink, std::string* found_eol = nullptr);

private:
    //! the conversion from the encoding to UTF-8, which keeps the shift state; null until text is read
    std::unique_ptr<IconvHelper> cd;
    //! the encoding of the conversion
    const QoreEncoding* cd_enc = nullptr;
    //! characters in UTF-8 decoded ahead of the text returned
    std::string ahead;
    //! bytes read that are the start of a character or shift sequence that is not complete yet
    std::string pending;

    //! opens the conversion if necessary; returns 0 for OK, -1 if an exception was raised
    DLLLOCAL int open(const QoreEncoding* enc, ExceptionSink* xsink);

    //! decodes the given bytes after any pending bytes and appends the characters to \a out
    /** @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL int decode(const char* buf, size_t len, std::string& out, ExceptionSink* xsink);

    //! raises an exception if bytes of an incomplete character were read before the end of the data
    /** @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL int checkEnd(const QoreEncoding* enc, ExceptionSink* xsink);

    //! reads the next character in UTF-8 into \a c, reading bytes one at a time
    /** @return 1 if a character was read, 0 at the end of the data, -1 if an exception was raised
    */
    DLLLOCAL int nextChar(const QoreEncoding* enc, std::string& c, f_read_t& my_read, ExceptionSink* xsink);

    //! converts the given text from UTF-8 to the encoding
    DLLLOCAL static QoreStringNode* encode(const std::string& utf8, const QoreEncoding* enc, ExceptionSink* xsink);
};

//! helper function for reading valid strings with character semantics
/** @param xsink for Qore-language exceptions
    @param size the nubmer of characters to read, negative values = read all available data
    @param enc the encoding of the input data and the output string; a stateful encoding is only supported if
    \a size is negative (characters of text in a stateful encoding are read with a QoreStatefulTextDecoder)
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
