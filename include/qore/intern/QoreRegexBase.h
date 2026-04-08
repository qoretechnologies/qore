/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreRegexBase.h

    regular expression substitution node definition

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

#ifndef _QORE_REGEXBASE_H

#define _QORE_REGEXBASE_H

#define PCRE2_CODE_UNIT_WIDTH 8

// base class for regex and regex substitution classes
#include <pcre2.h>
#include <string>

//! Size of error buffer for PCRE2 error messages
constexpr size_t qore_pcre2_errorbuf_size = 512;

#define check_re_options(a) (a & ~(PCRE2_CASELESS|PCRE2_DOTALL|PCRE2_EXTENDED|PCRE2_MULTILINE|PCRE2_UTF|PCRE2_UCP|PCRE2_UNGREEDY|PCRE2_DOLLAR_ENDONLY|PCRE2_ANCHORED|PCRE2_DUPNAMES))

class QoreRegexBase {
public:
    DLLLOCAL QoreRegexBase() {
    }

    DLLLOCAL QoreRegexBase(int options) : options(options) {
    }

    DLLLOCAL QoreRegexBase(QoreString* str, int options = PCRE2_UTF) : str(str), options(options) {
    }

    DLLLOCAL ~QoreRegexBase() {
        if (p) {
            pcre2_code_free(p);
        }
        delete str;
    }

    DLLLOCAL void setCaseInsensitive();
    DLLLOCAL void setDotAll();
    DLLLOCAL void setExtended();
    DLLLOCAL void setMultiline();
    DLLLOCAL void setUnicode();
    DLLLOCAL void setUngreedy();

    //! Returns the regex options flags
    DLLLOCAL int getOptions() const { return options; }

    //! Returns the original pattern string (preserved for AOT serialization)
    DLLLOCAL const char* getPatternCStr() const {
        return pattern_cache.empty() ? nullptr : pattern_cache.c_str();
    }

    //! Saves the pattern string before it's deleted during compilation
    DLLLOCAL void savePattern() {
        if (str && pattern_cache.empty()) {
            pattern_cache = str->c_str();
        }
    }

protected:
    pcre2_code* p = nullptr;
    QoreString* str = nullptr;
    int options = PCRE2_UTF;
    std::string pattern_cache;  //!< preserved for AOT serialization
};

#endif
