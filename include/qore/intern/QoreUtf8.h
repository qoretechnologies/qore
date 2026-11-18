//--------------------------------------------------------------------*- C++ -*-
//
//  Qore Programming Language
//
//  Copyright (C) 2026 Qore Technologies, s.r.o.
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
/// \brief The one definition of a valid UTF-8 character (RFC 3629)
///
//------------------------------------------------------------------------------
#ifndef INCLUDE_QORE_INTERN_QOREUTF8_H_
#define INCLUDE_QORE_INTERN_QOREUTF8_H_

#include <cstddef>
#include <cstring>

//! Returns the byte length of the UTF-8 character at \a p, as RFC 3629 defines a valid character
/** The one definition of a valid UTF-8 character in %Qore (qore#6604): the UTF-8 encoding's character functions
    (QoreEncoding::getCharLen(), and so the length, offsets and validity of a string) and the validation of input
    (IconvHelper::validUtf8Prefix(), and so qore_string_encoding_valid() and the conversions) use it; a new check of
    UTF-8 must use it too.

    @param p the bytes
    @param len the number of bytes available at \a p

    @return the length (1 - 4) of a valid character; 0 if the bytes do not start a valid character: a continuation
    byte, an overlong form (\c C0, \c C1, \c E0 \c 80 - \c 9F, \c F0 \c 80 - \c 8F), a surrogate (\c ED \c A0 -
    \c BF), a code point above U+10FFFF (\c F4 \c 90 - \c BF, \c F5 - \c FF), or a byte that is not a continuation
    byte where one is needed; or minus the length of the character if the \a len bytes are a valid start of it
    (-1 if \a len is 0)
*/
inline long q_utf8_char_len(const char* p, size_t len) {
    if (!len) {
        return -1;
    }
    const unsigned char* u = reinterpret_cast<const unsigned char*>(p);
    unsigned c = u[0];
    if (c < 0x80) {
        return 1;
    }
    // straight-line code for each length, with no loop, as this is on the path of every non-ASCII character
    if (c < 0xe0) {
        // C0 and C1 would be overlong forms of ASCII characters
        if (c < 0xc2) {
            return 0;
        }
        if (len < 2) {
            return -2;
        }
        return (u[1] & 0xc0) == 0x80 ? 2 : 0;
    }
    if (c < 0xf0) {
        if (len < 2) {
            return -3;
        }
        // the range of the second byte excludes the overlong forms (E0) and the surrogates (ED)
        unsigned b1 = u[1];
        if (c == 0xe0 ? (b1 < 0xa0 || b1 > 0xbf) : (c == 0xed ? (b1 < 0x80 || b1 > 0x9f) : ((b1 & 0xc0) != 0x80))) {
            return 0;
        }
        if (len < 3) {
            return -3;
        }
        return (u[2] & 0xc0) == 0x80 ? 3 : 0;
    }
    // F5 - FF would be beyond U+10FFFF
    if (c > 0xf4) {
        return 0;
    }
    if (len < 2) {
        return -4;
    }
    // the range of the second byte excludes the overlong forms (F0) and the code points above U+10FFFF (F4)
    unsigned b1 = u[1];
    if (c == 0xf0 ? (b1 < 0x90 || b1 > 0xbf) : (c == 0xf4 ? (b1 < 0x80 || b1 > 0x8f) : ((b1 & 0xc0) != 0x80))) {
        return 0;
    }
    if (len < 3) {
        return -4;
    }
    if ((u[2] & 0xc0) != 0x80) {
        return 0;
    }
    if (len < 4) {
        return -4;
    }
    return (u[3] & 0xc0) == 0x80 ? 4 : 0;
}

//! Counts complete UTF-8 characters and calls on_invalid at the first invalid sequence.
template <typename OnInvalid>
inline size_t q_utf8_count_chars(const char* p, const char* end, OnInvalid on_invalid) {
    constexpr size_t ascii_high_bit_mask = (~static_cast<size_t>(0) / 0xff) * 0x80;
    size_t i = 0;
    while (p < end) {
        unsigned char c = static_cast<unsigned char>(*p);
        if (c < 0x80) {
            ++p;
            ++i;
            while (static_cast<size_t>(end - p) >= sizeof(size_t)) {
                size_t word;
                std::memcpy(&word, p, sizeof(word));
                if (word & ascii_high_bit_mask) {
                    break;
                }
                p += sizeof(size_t);
                i += sizeof(size_t);
            }
            continue;
        }

        long l = q_utf8_char_len(p, end - p);
        if (l <= 0) {
            on_invalid();
            return i;
        }
        p += l;
        ++i;
    }

    return i;
}

#endif // INCLUDE_QORE_INTERN_QOREUTF8_H_
