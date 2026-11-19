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

//! the maximum buffer/read size for a single read
#define DefaultStreamReaderHelperBufferSize 4096

typedef std::function<qore_offset_t(void*, size_t, ExceptionSink*)> f_read_t;

//! what a text reader does with text that is not valid in its encoding (the \c encoding_errors option; qore#6604)
enum class qore_encoding_errors_t : unsigned char {
    //! raise \c INVALID-ENCODING (the default)
    RAISE = 0,
    //! tag the whole string read as ISO-8859-1, with its bytes
    LATIN1 = 1,
    //! tag the string read with the encoding of the reader, without a check
    KEEP = 2,
};

//! returns the option value for the given name: \c "raise", \c "latin1", or \c "keep"
/** @param name the name; nullptr gives the default (\c "raise")
    @param rv the value
    @param xsink for the \c ENCODING-ERRORS-ERROR exception raised for an unknown name

    @return 0 for OK, -1 if an exception was raised
*/
DLLLOCAL int q_get_encoding_errors(const QoreString* name, qore_encoding_errors_t& rv, ExceptionSink* xsink);

//! returns the name of the given option value
DLLLOCAL const char* q_encoding_errors_name(qore_encoding_errors_t mode);

//! returns true if text read in the given encoding can be invalid in it, so that it is checked
/** Multi-byte encodings, US-ASCII (bytes >= 0x80), and stateful encodings; in a single-byte encoding every byte is a
    character
*/
DLLLOCAL bool q_encoding_checks_text(const QoreEncoding* enc);

//! returns the byte offset of the first byte sequence that is not valid in the encoding, or -1 if all are valid
/** UTF-8 is checked as RFC 3629 defines it (see q_utf8_char_len()), other multi-byte encodings with their character
    function (QoreEncoding::getCharLen()), US-ASCII byte by byte, and a stateful encoding with a conversion

    @param p the bytes
    @param len the number of bytes
    @param enc the encoding
    @param allow_cut_end if true, a valid start of a character cut off by the end of the bytes is not an error (the
    end of a read of a number of bytes)
    @param ascii_prefix the number of bytes at the start known to be ASCII characters
*/
DLLLOCAL qore_offset_t q_text_invalid_offset(const char* p, size_t len, const QoreEncoding* enc,
        bool allow_cut_end = false, size_t ascii_prefix = 0);

//! returns the byte offset of the first byte sequence of the string that is not valid in its encoding, or -1
/** As q_text_invalid_offset(), with the ASCII prefix of the string, which is found a word at a time and kept with
    the string; strings in encodings whose text is not checked (see q_encoding_checks_text()) are valid

    @param str the string
    @param allow_cut_end if true, a valid start of a character cut off by the end of the string is not an error
*/
DLLLOCAL qore_offset_t q_string_invalid_offset(const QoreString& str, bool allow_cut_end = false);

//! raises the \c INVALID-ENCODING exception of a text reader
/** @param xsink the exception sink
    @param cls the name of the class of the reader, or nullptr for a function
    @param meth the name of the method of the reader
    @param what what was read (ex: \c "line read")
    @param enc the encoding of the text
    @param offset the byte offset of the invalid byte sequence in the text read
    @param line the line number of the text, or 0
*/
DLLLOCAL void q_raise_invalid_read_text(ExceptionSink* xsink, const char* cls, const char* meth, const char* what,
        const QoreEncoding* enc, size_t offset, int64 line = 0);

//! returns the encoding of the byte order given by a byte order mark at the start of text in an encoding without a
//! byte order
/** @param enc the encoding of the text: the generic \c "UTF-16" encoding, or a Unicode encoding created on the fly
    with a byte order mark (ex: \c "UTF-32")
    @param p the first bytes of the text
    @param len the number of bytes

    @return the encoding of the byte order found (ex: \c "UTF-16LE"), or nullptr if the bytes do not start with a
    byte order mark of the encoding or the encoding has a byte order

    @note exported, as the readers that call it are inline (StreamReader), and native tests use them
*/
DLLEXPORT const QoreEncoding* q_get_bom_byte_order(const QoreEncoding* enc, const char* p, size_t len);

//! applies the \c encoding_errors option to text read
/** @param str the text read; tagged ISO-8859-1 if it is not valid in its encoding and \a mode is
    qore_encoding_errors_t::LATIN1
    @param mode the option
    @param cls the name of the class of the reader, for the exception
    @param meth the name of the method of the reader, for the exception
    @param what what was read (ex: \c "line read"), for the exception
    @param xsink for the \c INVALID-ENCODING exception raised if the text is not valid in its encoding and \a mode is
    qore_encoding_errors_t::RAISE
    @param line the line number of the text, or 0
    @param allow_cut_end if true, a valid start of a character cut off by the end of the text is not an error

    @return 0 for OK, -1 if an exception was raised

    @note exported, as the readers that call it are inline (StreamReader), and native tests use them
*/
DLLEXPORT int q_check_read_text(QoreString& str, qore_encoding_errors_t mode, const char* cls, const char* meth,
        const char* what, ExceptionSink* xsink, int64 line = 0, bool allow_cut_end = false);

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
    @param mode the \c encoding_errors option, applied to the text read after any byte order mark is removed
    @param cls the name of the class of the reader, for the exception
    @param meth the name of the method of the reader, for the exception
 */
DLLLOCAL QoreStringNode* q_read_string_all(ExceptionSink* xsink, const QoreEncoding* enc, f_read_t my_read,
        qore_encoding_errors_t mode, const char* cls, const char* meth, const QoreEncoding** text_enc = nullptr,
        bool remove_bom = true);

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
        @param xsink for Qore-language exceptions; \c INVALID-ENCODING is raised for an invalid byte sequence or for
        a character cut off by the end of the data

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

    //! where a read is made, for the \c INVALID-ENCODING exception raised for text that is not valid
    struct ReadContext {
        //! the name of the class of the reader
        const char* cls;
        //! the name of the method of the reader
        const char* meth;
        //! what is read (ex: \c "line read")
        const char* what;
        //! the line number, or 0
        int64 line;
    };

    //! reads a number of characters, or all of the remaining text, applying the \c encoding_errors option
    /** As readChars(); text that is not valid in the encoding raises \c INVALID-ENCODING with the byte offset of the
        invalid sequence in the bytes read by the call.  After an invalid sequence, the shift state and so the
        characters are not known until the end of the line: the bytes up to and including the next line feed (or the
        end of the data) are skipped, the decoder is reset, and the text after them is read in the initial shift
        state.  With qore_encoding_errors_t::LATIN1, the bytes read by the call (after the text decoded ahead before
        it, in the encoding) and the bytes after them up to and including the next line feed are returned instead,
        tagged ISO-8859-1
    */
    DLLLOCAL QoreStringNode* readChars(const QoreEncoding* enc, int64 size, f_read_t my_read,
            qore_encoding_errors_t mode, const ReadContext& ctx, ExceptionSink* xsink);

    //! reads a line of text, applying the \c encoding_errors option
    /** As readLine(); text that is not valid is handled as by readChars(const QoreEncoding*, int64, f_read_t,
        qore_encoding_errors_t, const ReadContext&, ExceptionSink*), and with qore_encoding_errors_t::LATIN1 the
        whole line is returned, up to its end-of-line marker (a line feed if \a eol is empty), which is removed if
        \a trim is true
    */
    DLLLOCAL QoreStringNode* readLine(const QoreEncoding* enc, const std::string& eol, bool trim, bool cr_lookahead,
            f_read_t my_read, qore_encoding_errors_t mode, const ReadContext& ctx, ExceptionSink* xsink,
            std::string* found_eol = nullptr);

private:
    //! true if the last exception was raised for input that is not valid in the encoding
    bool invalid = false;

    //! the context of the current read for an exception, or nullptr
    const ReadContext* ctx = nullptr;

    //! the number of bytes of input of the current read given to the conversion so far, after the input read before
    //! it that it starts with
    size_t call_bytes = 0;

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

    //! calls the given read with the encoding_errors option
    /** @param marker_utf8 the end of the line of text that is not valid, in UTF-8: after an invalid sequence, the
        bytes up to and including it are skipped, or returned with qore_encoding_errors_t::LATIN1
        @param trim if true, the marker is removed from the line returned with qore_encoding_errors_t::LATIN1
        @param found_eol if not null, set to \a marker_utf8 if the line returned with qore_encoding_errors_t::LATIN1
        ends with the marker, to an empty string otherwise
    */
    DLLLOCAL QoreStringNode* readMode(const QoreEncoding* enc, f_read_t& my_read, qore_encoding_errors_t mode,
            const ReadContext& ctx, ExceptionSink* xsink, const std::string& marker_utf8, bool trim,
            std::string* found_eol, const std::function<QoreStringNode*(f_read_t&, ExceptionSink*)>& read);

    //! raises the exception for input that is not valid in the encoding
    /** @param offset the byte offset of the invalid sequence in the input of the current read
    */
    DLLLOCAL void raiseInvalid(const QoreEncoding* enc, size_t offset, bool cut, ExceptionSink* xsink);
};

//! helper function for reading valid strings with character semantics
/** @param xsink for Qore-language exceptions
    @param size the nubmer of characters to read, negative values = read all available data
    @param enc the encoding of the input data and the output string; a stateful encoding is only supported if
    \a size is negative (characters of text in a stateful encoding are read with a QoreStatefulTextDecoder)
    @param my_read a function object taking the arguments above, the return value means:
    - \c < 0: an error occurred (xsink has the exception info), 0 = end of data, > 0 the number of bytes read
    @param mode the \c encoding_errors option: with qore_encoding_errors_t::RAISE, \c INVALID-ENCODING is raised for
    an invalid byte sequence or a character cut off by the end of the data; otherwise an invalid byte is read as a
    character of its own (a code unit of the encoding: the smallest width of its characters), and with
    qore_encoding_errors_t::LATIN1 the string is then tagged ISO-8859-1
    @param cls the name of the class of the reader, for the exception
    @param meth the name of the method of the reader, for the exception
    @param text_enc if not null, set to the encoding of the text read: the encoding of the byte order found in a byte
    order mark in an encoding without a byte order (ex: \c "UTF-16"), also if the string is then tagged ISO-8859-1
    with qore_encoding_errors_t::LATIN1; also set if an exception is raised
    @param remove_bom if true, a byte order mark at the start of the text is removed (the start of the text of the
    reader)
    @param leftover if not null, receives the bytes read after the characters returned (the bytes after an invalid
    code unit for which an exception is raised, or after the start of a character that turned out to be invalid),
    which the caller gives back to the reader for its next read; if null, they stay with the text returned

    @return the string returned, note that if \a size = 0 (or a Qore-language exception occurs), nullptr is returned, oitherwise the caller owns the QoreStringNode reference returned
 */
DLLLOCAL QoreStringNode* q_read_string(ExceptionSink* xsink, int64 size, const QoreEncoding* enc, f_read_t my_read,
        qore_encoding_errors_t mode, const char* cls, const char* meth, const QoreEncoding** text_enc = nullptr,
        bool remove_bom = true, std::string* leftover = nullptr);


#endif
