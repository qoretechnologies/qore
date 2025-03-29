/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreRegexInterface.h

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

#ifndef _QORE_REGEX_INTERFACE_H
#define _QORE_REGEX_INTERFACE_H

#include <memory>

#define QRE_CASELESS           0x00000008u  /* C       */
#define QRE_MULTILINE          0x00000400u  /* C       */
#define QRE_DOTALL             0x00000020u  /* C       */
#define QRE_EXTENDED           0x00000080u  /* C       */
#define QRE_ANCHORED           0x80000000u
#define QRE_DOLLAR_ENDONLY     0x00000010u  /*   J M D */
#define QRE_NOTBOL             0x00000001u
#define QRE_NOTEOL             0x00000002u
#define QRE_UNGREEDY           0x00040000u  /* C       */
#define QRE_NOTEMPTY           0x00000004u  /* ) These two must be kept */
#define QRE_UTF                0x00080000u  /* C J M D */
#define QRE_UCP                0x00020000u  /* C J M D */

#define QRE_UTF8 (QRE_UTF | QRE_UCP)


// note that the following constant is > 32 bits
#define QRE_GLOBAL            0x100000000LL

class QoreRegexInterface {
public:
    DLLEXPORT QoreRegexInterface(ExceptionSink* xsink, const QoreString& pattern);
    DLLEXPORT QoreRegexInterface(ExceptionSink* xsink, const QoreString& pattern, int64 opts);

    DLLEXPORT QoreRegexInterface(ExceptionSink* xsink, const char* pattern);
    DLLEXPORT QoreRegexInterface(ExceptionSink* xsink, const char* pattern, int64 opts);

    DLLEXPORT ~QoreRegexInterface();

    DLLEXPORT bool match(const QoreString& target, ExceptionSink* xsink) const;

    DLLEXPORT QoreListNode* extractSubstrings(const QoreString& target, ExceptionSink* xsink) const;

private:
    class qore_regex_private* priv;
};

class QoreRegexSubstInterface {
public:
    DLLEXPORT QoreRegexSubstInterface(ExceptionSink* xsink, const QoreString& pattern);

    DLLEXPORT QoreRegexSubstInterface(ExceptionSink* xsink, const QoreString& pattern, int64 opts);

    DLLEXPORT QoreRegexSubstInterface(ExceptionSink* xsink, const char* pattern);

    DLLEXPORT QoreRegexSubstInterface(ExceptionSink* xsink, const char* pattern, int64 opts);

    DLLEXPORT ~QoreRegexSubstInterface();

    DLLEXPORT QoreStringNode* subst(const QoreString& target, const QoreString& newstr, ExceptionSink* xsink) const;

    // replaces with an empty string
    DLLEXPORT QoreStringNode* subst(const QoreString& target, ExceptionSink* xsink) const;

private:
    class qore_regex_subst_private* priv;
};

#endif