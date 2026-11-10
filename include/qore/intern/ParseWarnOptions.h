/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ParseWarnOptions.h

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

#ifndef _QORE_INTERN_PARSEWARNOPTIONS_H
#define _QORE_INTERN_PARSEWARNOPTIONS_H

#include <qore/QoreParseOptions.h>

struct ParseWarnOptions {
    QoreParseOptions parse_options;
    int warn_mask = 0;

    DLLLOCAL ParseWarnOptions() {
    }

    DLLLOCAL ParseWarnOptions(const QoreParseOptions& n_parse_options, int n_warn_mask = 0)
            : parse_options(n_parse_options), warn_mask(n_warn_mask) {
    }

    DLLLOCAL void operator=(const ParseWarnOptions& pwo) {
        parse_options = pwo.parse_options;
        warn_mask = pwo.warn_mask;
    }

    DLLLOCAL bool operator==(const ParseWarnOptions& pwo) const {
        return parse_options == pwo.parse_options && warn_mask == pwo.warn_mask;
    }
};

#endif
