/*
    QoreParseOptions.cpp

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

#include <qore/QoreParseOptions.h>

// Extended option constants (bits 64+)
const QoreParseOptions QoreParseOptions::NO_CLASS_DEFS(0, 1LL << 0);       // bit 64
const QoreParseOptions QoreParseOptions::NO_CONSTANT_DEFS(0, 1LL << 1);    // bit 65
const QoreParseOptions QoreParseOptions::NO_NAMESPACE_DEFS(0, 1LL << 2);   // bit 66
const QoreParseOptions QoreParseOptions::NO_NEW(0, 1LL << 3);              // bit 67
const QoreParseOptions QoreParseOptions::BROKEN_SOFT_TYPES(0, 1LL << 4);   // bit 68
const QoreParseOptions QoreParseOptions::NO_SUMMARIZE(0, 1LL << 5);        // bit 69
const QoreParseOptions QoreParseOptions::NO_MODULE_PATH_DIRECTIVES(0, 1LL << 6); // bit 70
const QoreParseOptions QoreParseOptions::FP_FAST_MATH(0, 1LL << 7);        // bit 71
const QoreParseOptions QoreParseOptions::NO_ITERATE(0, 1LL << 8);          // bit 72
const QoreParseOptions QoreParseOptions::NO_FIRST(0, 1LL << 9);            // bit 73
const QoreParseOptions QoreParseOptions::NO_ANY_OPERATOR(0, 1LL << 10);    // bit 74
const QoreParseOptions QoreParseOptions::NO_ALL_OPERATOR(0, 1LL << 11);    // bit 75
const QoreParseOptions QoreParseOptions::NO_COUNT(0, 1LL << 12);           // bit 76
const QoreParseOptions QoreParseOptions::NO_TAKE(0, 1LL << 13);            // bit 77
const QoreParseOptions QoreParseOptions::NO_DROP(0, 1LL << 14);            // bit 78
const QoreParseOptions QoreParseOptions::NO_TAKEWHILE(0, 1LL << 15);       // bit 79
const QoreParseOptions QoreParseOptions::NO_TAKEUNTIL(0, 1LL << 16);       // bit 80
const QoreParseOptions QoreParseOptions::NO_FIND_MODIFIERS(0, 1LL << 17);  // bit 81
const QoreParseOptions QoreParseOptions::NO_STREAM_FUSION(0, 1LL << 18);   // bit 82
const QoreParseOptions QoreParseOptions::STREAMING_ANY(0, 1LL << 19);      // bit 83
const QoreParseOptions QoreParseOptions::NO_CHAR_TYPE(0, 1LL << 20);       // bit 84
const QoreParseOptions QoreParseOptions::NO_STRING_INDEX_CHAR(0, 1LL << 21); // bit 85
const QoreParseOptions QoreParseOptions::NEGATIVE_OFFSETS(0, 1LL << 22);   // bit 86
const QoreParseOptions QoreParseOptions::BROKEN_AUTO_CAST(0, 1LL << 23);   // bit 87
const QoreParseOptions QoreParseOptions::NO_STREAMING_OPERATORS(
    0,
    (1LL << 8)  // NO_ITERATE
        | (1LL << 9)   // NO_FIRST
        | (1LL << 10)  // NO_ANY_OPERATOR
        | (1LL << 11)  // NO_ALL_OPERATOR
        | (1LL << 12)  // NO_COUNT
        | (1LL << 13)  // NO_TAKE
        | (1LL << 14)  // NO_DROP
        | (1LL << 15)  // NO_TAKEWHILE
        | (1LL << 16)  // NO_TAKEUNTIL
        | (1LL << 17)); // NO_FIND_MODIFIERS
