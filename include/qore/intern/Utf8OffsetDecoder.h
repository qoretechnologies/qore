/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  Utf8OffsetDecoder.h

  Qore Programming Language

  Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_INTERN_UTF8OFFSETDECODER_H
#define _QORE_INTERN_UTF8OFFSETDECODER_H

#include "qore/intern/IconvHelper.h"

#include <cassert>
#include <cerrno>
#include <string>

//! Decodes text to UTF-8 one byte at a time, giving the byte offset in the input of each character decoded
/** The bytes are given to iconv one at a time after the bytes of an incomplete character, so that its input never
    ends with a character cut off after complete characters: Apple's libiconv then decodes the bytes of the
    incomplete character as characters of their own and reports success instead of failing with \c EINVAL (see
    IconvHelper::reportsTruncatedStatefulInput()).  The shift state of a stateful encoding (ex: UTF-7, ISO-2022-JP) is
    kept across calls, so text can be decoded in blocks cut anywhere.

    The offset of a character is the offset of the first byte after the bytes of the character before it; in a
    stateful encoding, this is the offset of any shift sequence before it (ex: the \c "ESC $ B" before a JIS
    character in ISO-2022-JP, or the \c "+" that starts a base64 run in UTF-7), whether the platform's iconv
    consumes the shift sequence on its own or only with the character.

    The object is not thread-safe.
*/
class Utf8OffsetDecoder {
public:
    //! creates the decoder
    /** @param enc the encoding of the input
        @param xsink if the conversion cannot be opened, the exception is raised here (if given)
    */
    DLLLOCAL Utf8OffsetDecoder(const QoreEncoding* enc, ExceptionSink* xsink) : cd(QCS_UTF8, enc, xsink) {
    }

    //! returns true if the conversion could be opened
    DLLLOCAL bool isValid() const {
        return cd.isValid();
    }

    //! decodes the given bytes after any bytes of an incomplete character given before
    /** @param buf the input
        @param len the byte length of the input
        @param out the UTF-8 text decoded is appended here
        @param add_char called as \c add_char(size_t pos, int64 offset) for each character decoded, with the byte
        position of the character in \a out and its byte offset in the input
        @param xsink if given, the conversion is a cancellation point, and a cancellation is raised here

        @return 0 for OK, -1 for an invalid byte sequence, whose offset is given by errorOffset() (no exception is
        raised), or -2 if the conversion was cancelled (the exception has been raised)
    */
    template <typename F>
    DLLLOCAL int decode(const char* buf, size_t len, std::string& out, F add_char, ExceptionSink* xsink = nullptr) {
        for (size_t i = 0; i < len; ++i) {
            pending.push_back(buf[i]);
            ++offset;
            char* in = &pending[0];
            size_t in_left = pending.size();
            bool produced = false;
            while (in_left) {
                char obuf[64];
                char* op = obuf;
                size_t out_left = sizeof(obuf);
                size_t rc = cd.iconv(&in, &in_left, &op, &out_left, xsink);
                int err = errno;
                if (op != obuf) {
                    appendChars(obuf, op - obuf, out, add_char);
                    produced = true;
                }
                if (rc != static_cast<size_t>(-1) || err == E2BIG) {
                    continue;
                }
                if (err == EINVAL) {
                    // the start of a character or shift sequence: it is decoded with its next byte
                    break;
                }
                if (err == ECANCELED) {
                    assert(!xsink || *xsink);
                    return -2;
                }
                // the invalid sequence starts at the first byte that was not converted
                errOffset = pendingStart + (in - &pending[0]);
                return -1;
            }
            size_t used = pending.size() - in_left;
            if (used) {
                pending.erase(0, used);
                pendingStart += used;
            }
            if (produced) {
                // the next character starts with the bytes after the ones decoded
                charStart = pending.empty() ? offset : pendingStart;
            }
        }
        return 0;
    }

    //! returns true if bytes of an incomplete character or shift sequence have been given
    DLLLOCAL bool hasPending() const {
        return !pending.empty();
    }

    //! returns the byte offset in the input of an incomplete character (with any shift sequence before it), if any
    DLLLOCAL int64 pendingOffset() const {
        return charStart;
    }

    //! returns the byte offset in the input of the invalid byte sequence after decode() returned -1
    DLLLOCAL int64 errorOffset() const {
        return errOffset;
    }

    //! returns the number of bytes given
    DLLLOCAL int64 bytesDecoded() const {
        return offset;
    }

private:
    IconvHelper cd;
    //! bytes of an incomplete character or shift sequence
    std::string pending;
    //! the byte offset in the input of the next byte
    int64 offset = 0;
    //! the byte offset in the input of the first pending byte
    int64 pendingStart = 0;
    //! the byte offset in the input of the first byte after the last character decoded: the start of the next one
    int64 charStart = 0;
    //! the byte offset of an invalid byte sequence
    int64 errOffset = 0;

    //! appends UTF-8 output, calling add_char for each character in it
    template <typename F>
    DLLLOCAL void appendChars(const char* p, size_t len, std::string& out, F& add_char) {
        for (size_t i = 0; i < len; ++i) {
            // a byte that is not a continuation byte starts a character
            if ((static_cast<unsigned char>(p[i]) & 0xc0) != 0x80) {
                add_char(out.size() + i, charStart);
            }
        }
        out.append(p, len);
    }
};

#endif // _QORE_INTERN_UTF8OFFSETDECODER_H
