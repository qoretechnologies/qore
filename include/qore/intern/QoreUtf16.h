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
/// \brief The one definition of a valid UTF-16 character (paired surrogates)
///
//------------------------------------------------------------------------------
#ifndef INCLUDE_QORE_INTERN_QOREUTF16_H_
#define INCLUDE_QORE_INTERN_QOREUTF16_H_

#include <cstddef>

//! Returns the byte length of the UTF-16 character at \a p, a code unit or a pair of surrogates
/** The one definition of a valid UTF-16 character in %Qore (qore#6604): the character functions of the UTF-16
    encodings (QoreEncoding::getCharLen(), and so the length, offsets and validity of a string in \c "UTF-16",
    \c "UTF-16BE", \c "UTF-16LE" and the encodings created on the fly with their characters) and the validation of
    input in them where the platform's iconv accepts unpaired surrogates (IconvHelper::validFormPrefix()) use it; a
    new check of UTF-16 must use it too.

    @tparam BigEndian true for big-endian code units (\c "UTF-16BE", and \c "UTF-16", which is big-endian in %Qore),
    false for little-endian ones (\c "UTF-16LE")

    @param p the bytes
    @param len the number of bytes available at \a p

    @return 2 for a code unit outside \c D800 - \c DFFF; 4 for a high surrogate (\c D800 - \c DBFF) followed by a low
    surrogate (\c DC00 - \c DFFF); 0 if the bytes do not start a valid character: a low surrogate on its own, or a
    high surrogate followed by a code unit that is not a low surrogate; or minus the length that the character
    needs as far as the \a len bytes tell, if they are a valid start of a character: -2 for one byte that can start
    a code unit that is not a surrogate (also for no bytes), -4 for a high surrogate with no complete code unit after
    it; as only the high byte of a code unit says whether it is a surrogate, one byte of a little-endian code unit
    gives -2, and the length is then known with the second byte
*/
template <bool BigEndian>
inline long q_utf16_char_len(const char* p, size_t len) {
    const unsigned char* u = reinterpret_cast<const unsigned char*>(p);
    if (len < 2) {
        if (BigEndian && len) {
            // the high byte of the code unit: a low surrogate is invalid, and a high surrogate needs a second unit
            if ((u[0] & 0xfc) == 0xdc) {
                return 0;
            }
            if ((u[0] & 0xfc) == 0xd8) {
                return -4;
            }
        }
        return -2;
    }
    unsigned hi = BigEndian ? u[0] : u[1];
    // a code unit that is not a surrogate: the path of every character of the Basic Multilingual Plane
    if ((hi & 0xf8) != 0xd8) {
        return 2;
    }
    // a low surrogate on its own
    if (hi >= 0xdc) {
        return 0;
    }
    if (len < 4) {
        // in big-endian order, the third byte is the high byte of the next code unit, which must be a low surrogate
        if (BigEndian && len == 3 && (u[2] & 0xfc) != 0xdc) {
            return 0;
        }
        return -4;
    }
    return ((BigEndian ? u[2] : u[3]) & 0xfc) == 0xdc ? 4 : 0;
}

#endif // INCLUDE_QORE_INTERN_QOREUTF16_H_
