/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreNativeParseInfo.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_NATIVE_PARSE_INFO_H
#define _QORE_NATIVE_PARSE_INFO_H

#include <qore/common.h>
#include <string>

class QoreProgramLocation;

//! Lexical native eligibility, collected by the parser before expressions can be folded away
/** The location and directive strings belong to the Program, like AST source locations.  This is immutable after
    parsing, is inspected only when lowering code, and is not part of IR or AOT metadata.  An empty grammar rule
    contributes neither modern nor legacy code; combining rules preserves the first legacy token's provenance.
*/
struct QoreNativeParseInfo {
    const QoreProgramLocation* legacy_location = nullptr;
    const char* directive = nullptr;
    bool has_modern = false;

    DLLLOCAL void merge(const QoreNativeParseInfo& other) {
        if (!legacy_location) {
            legacy_location = other.legacy_location;
            directive = other.directive;
        }
        has_modern |= other.has_modern;
    }

    //! Describes the first source context that prevents native lowering
    DLLLOCAL std::string describe() const;
};

#endif
