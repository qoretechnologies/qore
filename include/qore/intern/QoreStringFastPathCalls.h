/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreStringFastPathCalls.h

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

#ifndef _QORE_INTERN_QORESTRINGFASTPATHCALLS_H
#define _QORE_INTERN_QORESTRINGFASTPATHCALLS_H

// The ordinary calls of the string builtins that the fast paths of the IR interpreter and of JIT and AOT code stand for
// (qore#7711, see design/string-fast-path-exceptions.md).  A fast path runs the builtin's work without the call: no
// argument list, no variant search and no frame on the call stack.  An exception must be raised in the builtin's frame,
// as the call raises it, so a fast path whose work raised one discards it and makes the ordinary call instead, which
// raises it again in its frame; a fast path also makes the ordinary call if an exception is pending when it starts, as
// its work would add an exception without the frame to it.  The work of these builtins has no effect but its result, so
// running it again gives the exception of the call.  Each function returns the result of the call, or NOTHING if it
// raised an exception.  They are compiled outside the single compilation unit (lib/QoreAOTStringRuntime.cpp).

#include <qore/Qore.h>

//! the search pseudo-methods of <string> that the fast paths make
enum class QoreStringSearchCall : int32_t {
    StartsWith = 0,
    EndsWith = 1,
    Contains = 2,
    Find = 3,
    RFind = 4,
};

//! makes the ordinary call of <string>::lwr() or <string>::upr() (\a function false), or of tolower() or toupper()
DLLLOCAL QoreValue qore_string_call_case(QoreValue str, bool upper, bool function, ExceptionSink* xsink);

//! makes the ordinary call of <string>::length() (\a function false) or of length()
DLLLOCAL QoreValue qore_string_call_length(QoreValue str, bool function, ExceptionSink* xsink);

//! makes the ordinary call of <string>::size()
DLLLOCAL QoreValue qore_string_call_size(QoreValue str, ExceptionSink* xsink);

//! makes the ordinary call of <string>::substr() (\a function false), or of substr(), with one argument after the
//! string, or with two (\a has_length)
DLLLOCAL QoreValue qore_string_call_substr(QoreValue str, int64 start, int64 length, bool has_length, bool function,
        ExceptionSink* xsink);

//! makes the ordinary call of a search pseudo-method of <string>; \a offset is given to find() and rfind() only
DLLLOCAL QoreValue qore_string_call_search(QoreValue str, QoreValue pattern, QoreStringSearchCall call,
        int64 offset, ExceptionSink* xsink);

//! makes the ordinary call of index() or rindex() (\a reverse)
DLLLOCAL QoreValue qore_string_call_index(QoreValue str, QoreValue pattern, bool reverse, int64 offset,
        ExceptionSink* xsink);

#endif
