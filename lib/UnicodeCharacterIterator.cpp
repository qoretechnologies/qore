/*
    UnicodeCharacterIterator.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#include <qore/Qore.h>
#include "qore/intern/qore_string_private.h"

UnicodeCharacterIterator::UnicodeCharacterIterator(const QoreString& str) : str(str) {
}

UnicodeCharacterIterator::~UnicodeCharacterIterator() {
}

bool UnicodeCharacterIterator::next(ExceptionSink* xsink) {
    //printd(5, "UnicodeCharacterIterator::next(): str: '%s' (%zu) bp: %zu\n", str.c_str(), str.size(), byte_pos);
    if (!str.size()) {
        return false;
    }
    if (qore_string_private::get(str)->isStateful()) {
        return nextStateful(xsink);
    }
    // if at the end of the string, reset to the beginning and return false
    if (byte_pos == str.size()) {
        byte_pos = 0;
        current_code = -1;
        return false;
    }
    unsigned len;
    current_code = str.getUnicodePointFromBytePos(byte_pos, len, xsink);
    //printd(5, "UnicodeCharacterIterator::next(): c: %d bp: %zu len: %d x: %d\n", current_code, byte_pos, len, *xsink ? true : false);
    if (*xsink) {
        byte_pos = 0;
        current_code = -1;
        return false;
    }
    byte_pos += len;
    return true;
}

// a character has no bytes of its own in a stateful encoding (ex: ISO-2022-JP): byte_pos is the byte offset of the
// next character in the text of the string in UTF-8, which is converted for each character, as the layout of this
// public class cannot hold the converted text; functions in the library that walk the characters of a string in a
// stateful encoding walk its text in UTF-8 instead of using this class
bool UnicodeCharacterIterator::nextStateful(ExceptionSink* xsink) {
    QoreString utf8(QCS_UTF8);
    if (qore_string_private::get(str)->statefulToUtf8(utf8, xsink)) {
        byte_pos = 0;
        current_code = -1;
        return false;
    }
    if (byte_pos >= utf8.size()) {
        byte_pos = 0;
        current_code = -1;
        return false;
    }
    unsigned len;
    current_code = utf8.getUnicodePointFromBytePos(byte_pos, len, xsink);
    if (*xsink) {
        byte_pos = 0;
        current_code = -1;
        return false;
    }
    byte_pos += len;
    return true;
}

int UnicodeCharacterIterator::getValue() const {
    return current_code;
}

bool UnicodeCharacterIterator::valid() const {
    return current_code >= 0;
}
