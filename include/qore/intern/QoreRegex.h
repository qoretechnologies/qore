/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreRegex.h

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

/*
  PCRE-based matching (Perl-compatible regular expression matching)
  see: http://www.pcre.org for more information on this library

  NOTE: all regular expression matching is done with UTF-8 encoding, so character set
  encodings are converted if necessary
 */

#ifndef _QORE_QOREREGEX_H

#define _QORE_QOREREGEX_H

#include "qore/intern/QoreRegexBase.h"

#include <functional>

struct QoreProgramLocation;

class QoreRegex : public QoreRegexBase, public QoreReferenceCounter {
public:
    // function pointer to allow the parse location for parse errors to be generated on demand
    typedef std::function<const QoreProgramLocation* ()> q_get_loc_t;

    DLLLOCAL QoreRegex();

    // used at run-time, does not change str
    DLLLOCAL QoreRegex(const QoreString& str, int64 options, ExceptionSink* xsink);

    // used at run-time, does not change str; str must be in UTF-8 encoding
    DLLLOCAL QoreRegex(const char* str, int64 options, ExceptionSink* xsink);

    DLLLOCAL ~QoreRegex();

    DLLLOCAL void concat(char c);
    // called at parse time; the get_loc lambda is only called if an error occurs parsing the regular expression
    // this allows the source location to only be created if it's needed due to the error
    DLLLOCAL void parse(q_get_loc_t get_loc);
    DLLLOCAL void parseRT(const QoreString* pattern, ExceptionSink* xsink);
    // pattern must be in UTF-8 encoding
    DLLLOCAL void parseRT(const char* pattern, ExceptionSink* xsink);
    DLLLOCAL bool exec(const QoreString* target, ExceptionSink* xsink) const;
    DLLLOCAL bool exec(const char* str, size_t len) const;
    DLLLOCAL QoreListNode* extractSubstrings(const QoreString* target, ExceptionSink* xsink) const;
    DLLLOCAL QoreListNode* extractWithPattern(const QoreString& target, bool include_pattern,
            ExceptionSink* xsink, int limit = -1) const;
    DLLLOCAL int64 countMatches(const QoreString* target, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue extractNamedGroups(const QoreString* target, ExceptionSink* xsink) const;
    DLLLOCAL QoreListNode* extractDetailed(const QoreString* target, ExceptionSink* xsink) const;
    // caller owns QoreString returned
    DLLLOCAL QoreString* getString();

    DLLLOCAL void setGlobal() {
        global = true;
    }

    DLLLOCAL bool isGlobal() const {
        return global;
    }

    DLLLOCAL void ref() const {
        ROreference();
    }

    DLLLOCAL void deref() {
        if (ROdereference())
            delete this;
    }

    DLLLOCAL QoreRegex* refSelf() const {
        ref();
        return const_cast<QoreRegex*>(this);
    }

private:
    bool global = false;
};

#endif // _QORE_QOREREGEX_H
