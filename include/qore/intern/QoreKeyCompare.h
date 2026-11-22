/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreKeyCompare.h

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

#ifndef _QORE_INTERN_QOREKEYCOMPARE_H

#define _QORE_INTERN_QOREKEYCOMPARE_H

#include <cstddef>
#include <cstdint>
#include <cstring>

//! Returns true if the \a len bytes at \a a and the \a len bytes at \a b are equal (qore#7194)
/** Compares hash keys and member names whose lengths are known and equal, in place of strcmp() and memcmp().  It is
    inline and reads only the bytes of the two ranges, in loads of 8, 4, 2 or 1 bytes (the last load of a range can
    overlap the one before it), so its cost depends only on \a len.  The cost of glibc's strcmp() and memcmp() also
    depends on where the strings are: they take a slower path when the page offsets of their arguments are near the
    end of a page, so that a change that only moves the strings changed the cost of a member access.

    @param a the first range of \a len bytes
    @param b the second range of \a len bytes
    @param len the number of bytes to compare

    @return true if the ranges are equal
*/
inline bool qore_key_bytes_equal(const char* a, const char* b, size_t len) {
    if (len >= 8) {
        uint64_t x, y;
        const size_t last = len - 8;
        for (size_t i = 0; i < last; i += 8) {
            memcpy(&x, a + i, 8);
            memcpy(&y, b + i, 8);
            if (x != y) {
                return false;
            }
        }
        memcpy(&x, a + last, 8);
        memcpy(&y, b + last, 8);
        return x == y;
    }
    if (len >= 4) {
        uint32_t x0, y0, x1, y1;
        memcpy(&x0, a, 4);
        memcpy(&y0, b, 4);
        memcpy(&x1, a + len - 4, 4);
        memcpy(&y1, b + len - 4, 4);
        return x0 == y0 && x1 == y1;
    }
    if (len >= 2) {
        uint16_t x0, y0, x1, y1;
        memcpy(&x0, a, 2);
        memcpy(&y0, b, 2);
        memcpy(&x1, a + len - 2, 2);
        memcpy(&y1, b + len - 2, 2);
        return x0 == y0 && x1 == y1;
    }
    return !len || *a == *b;
}

//! Returns true if the string \a a of \a alen bytes is equal to the string \a b of \a blen bytes (qore#7194)
/** @see qore_key_bytes_equal()
*/
inline bool qore_key_equal(const char* a, size_t alen, const char* b, size_t blen) {
    return alen == blen && qore_key_bytes_equal(a, b, alen);
}

#endif
