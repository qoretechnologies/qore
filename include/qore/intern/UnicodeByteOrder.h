/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    UnicodeByteOrder.h

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

#ifndef _QORE_INTERN_UNICODEBYTEORDER_H
#define _QORE_INTERN_UNICODEBYTEORDER_H

#include <cstddef>
#include <cstring>

//! The number of bytes at the start of data examined for the zero bytes of UTF-16 or UTF-32 text
static constexpr size_t QORE_UNICODE_BYTE_ORDER_SAMPLE = 4096;

//! The least number of characters with the zero bytes of a byte order that give the byte order of data without a mark
/** Fewer are not clear evidence: two CJK characters in UTF-16BE such as U+4E00 U+4E01 (4e 00 4e 01) have their only
    zero byte where UTF-16LE text has them
*/
static constexpr size_t QORE_UNICODE_BYTE_ORDER_MIN_CHARS = 8;

//! The byte order of UTF-16 or UTF-32 text found at the start of data
struct QoreUnicodeByteOrder {
    //! \c "UTF-16LE", \c "UTF-16BE", \c "UTF-32LE", or \c "UTF-32BE"; nullptr if no byte order was found
    const char* encoding = nullptr;
    //! true if the byte order is given by a byte order mark, false if by where the zero bytes fall
    bool bom = false;
};

//! Returns the UTF-16 or UTF-32 byte order of data, by the byte order mark at its start or where its zero bytes fall
/** Text in UTF-16 has a zero byte in most characters of the Latin, Greek, and Cyrillic scripts and in the digits,
    separators, and line breaks of delimited data, always on the same side: the high byte, which is the second byte of
    a character in UTF-16LE; in UTF-32, two or three of every four bytes are zero.  Text in an encoding with one-byte
    characters has no zero bytes.  The pattern must be clear: in UTF-16, a quarter of the characters with a zero high
    byte and almost no zero low bytes; in UTF-32, nine tenths of the characters with both high bytes zero; and in both,
    at least QORE_UNICODE_BYTE_ORDER_MIN_CHARS such characters; text without such a pattern (ex: CJK text in UTF-16,
    or a few characters) gives no byte order.

    @param p the start of the data
    @param size the number of bytes at \a p; up to the first QORE_UNICODE_BYTE_ORDER_SAMPLE bytes are examined for zero
    bytes

    @return the byte order found, if any
*/
DLLLOCAL inline QoreUnicodeByteOrder q_get_unicode_byte_order(const unsigned char* p, size_t size) {
    QoreUnicodeByteOrder rv;
    // the UTF-32 marks first: the UTF-32LE one starts with the UTF-16LE one
    if (size >= 4 && !memcmp(p, "\xff\xfe\x00\x00", 4)) {
        rv.encoding = "UTF-32LE";
        rv.bom = true;
        return rv;
    }
    if (size >= 4 && !memcmp(p, "\x00\x00\xfe\xff", 4)) {
        rv.encoding = "UTF-32BE";
        rv.bom = true;
        return rv;
    }
    if (size >= 2 && p[0] == 0xff && p[1] == 0xfe) {
        rv.encoding = "UTF-16LE";
        rv.bom = true;
        return rv;
    }
    if (size >= 2 && p[0] == 0xfe && p[1] == 0xff) {
        rv.encoding = "UTF-16BE";
        rv.bom = true;
        return rv;
    }
    size_t n = size < QORE_UNICODE_BYTE_ORDER_SAMPLE ? size : QORE_UNICODE_BYTE_ORDER_SAMPLE;
    n -= n % 4;
    if (n < 4) {
        return rv;
    }
    // zero bytes by their place in each group of four
    size_t zeros[4] = {0, 0, 0, 0};
    for (size_t i = 0; i < n; ++i) {
        if (!p[i]) {
            ++zeros[i % 4];
        }
    }
    size_t quads = n / 4;
    if (quads >= QORE_UNICODE_BYTE_ORDER_MIN_CHARS && zeros[2] * 10 >= quads * 9 && zeros[3] * 10 >= quads * 9
        && zeros[0] * 20 < quads) {
        rv.encoding = "UTF-32LE";
    } else if (quads >= QORE_UNICODE_BYTE_ORDER_MIN_CHARS && zeros[0] * 10 >= quads * 9
        && zeros[1] * 10 >= quads * 9 && zeros[3] * 20 < quads) {
        rv.encoding = "UTF-32BE";
    } else {
        size_t odd = zeros[1] + zeros[3];
        size_t even = zeros[0] + zeros[2];
        size_t pairs = n / 2;
        if (odd >= QORE_UNICODE_BYTE_ORDER_MIN_CHARS && odd * 4 >= pairs && even * 20 < pairs) {
            rv.encoding = "UTF-16LE";
        } else if (even >= QORE_UNICODE_BYTE_ORDER_MIN_CHARS && even * 4 >= pairs && odd * 20 < pairs) {
            rv.encoding = "UTF-16BE";
        }
    }
    return rv;
}

//! Raises ENCODING-BYTE-ORDER-ERROR if data is in the other byte order than its UTF-16 or UTF-32 encoding
/** Data read in the wrong byte order decodes to valid characters that are not in it: in little-endian UTF-16 read as
    big-endian, a line feed (0a 00) becomes U+0A00.  The generic \c "UTF-16" and \c "UTF-32" encodings are big-endian
    unless the data starts with a byte order mark, which then decides.  The data contradicts its encoding if it starts
    with the byte order mark of the other byte order, or, without one, if its zero bytes fall where text in the other
    byte order has them (see q_get_unicode_byte_order()); an encoding that is not a form of UTF-16 or UTF-32, or a byte
    order of another width, is not checked.

    @param p the start of the data
    @param size the number of bytes at \a p
    @param code the code of the declared encoding of the data (ex: \c "UTF-16")
    @param xsink receives the exception

    @return 0 for OK, -1 if an exception was raised
*/
DLLLOCAL inline int q_check_unicode_byte_order(const unsigned char* p, size_t size, const char* code,
        ExceptionSink* xsink) {
    // the width and the byte order of the declared encoding: "UTF-16", "UTF-16LE", ... "UTF-32BE"
    if (strncmp(code, "UTF-", 4) || (strncmp(code + 4, "16", 2) && strncmp(code + 4, "32", 2))) {
        return 0;
    }
    const char* declared_order = code + 6;
    if (*declared_order && strcmp(declared_order, "LE") && strcmp(declared_order, "BE")) {
        return 0;
    }
    QoreUnicodeByteOrder found = q_get_unicode_byte_order(p, size);
    // only the byte order of the same width is compared
    if (!found.encoding || strncmp(found.encoding + 4, code + 4, 2)) {
        return 0;
    }
    // a byte order mark decides the byte order of the generic encoding
    if (!*declared_order && found.bom) {
        return 0;
    }
    const char* found_order = found.encoding + 6;
    if (!strcmp(*declared_order ? declared_order : "BE", found_order)) {
        return 0;
    }
    QoreStringNode* desc = new QoreStringNodeMaker("the data is declared as %s%s, but it ", code,
        *declared_order ? "" : " (big-endian without a byte order mark)");
    if (found.bom) {
        desc->sprintf("starts with the %s byte order mark", found.encoding);
    } else {
        desc->sprintf("has its zero bytes where %s text has them", found.encoding);
    }
    desc->sprintf("; read as %s, its characters would not be the characters of the data; use %s", code,
        found.encoding);
    xsink->raiseException("ENCODING-BYTE-ORDER-ERROR", desc);
    return -1;
}

#endif
