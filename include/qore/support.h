/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  support.h

  Qore Programming Language

  Copyright (C) 2005 - 2024 Qore Technologies, s.r.o.

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

#ifndef QORE_SUPPORT_H

#define QORE_SUPPORT_H

#include <qore/common.h>

DLLEXPORT int printe(const char *fmt, ...);
DLLEXPORT char *remove_trailing_newlines(char *str);
DLLEXPORT char *remove_trailing_blanks(char *str);

// we supply debugging function also for non-debugging builds as library entry points
// in case a debugging-enabled binary is linked against a non-debugging-enabled lib

//! do not call this function directly, use the macro QORE_TRACE()
DLLEXPORT void trace_function(int code, const char *funcname);
//! do not call this function directly, use the macro printd()
DLLEXPORT int print_debug(int level, const char *fmt, ...);

DLLEXPORT extern int qore_trace;
DLLEXPORT extern int debug;

#define TRACE_IN   1
#define TRACE_OUT  2

#ifdef DEBUG
//! a macro for printing debugging statements; when "DEBUG" is not defined, the function call to print_debug() not be included in the compiled output
#ifdef __GNUC__
#define printd(level, args...) do { if ((level) <= debug) print_debug((level), args); } while (0)
#else
#define printd(level, ...) do { if ((level) <= debug) print_debug((level), __VA_ARGS__); } while (0)
#endif

//! legacy tracing macro; disabled because QORE_TRACE instrumentation is no longer used
#ifdef __GNUC__
#define QORE_TRACE(args...)
#else
#define QORE_TRACE(...)
#endif

#else
#ifdef __GNUC__
//! a macro for printing debugging statements; when "DEBUG" is not defined, the function call not be included in the compiled output
#define printd(args...)
//! a macro for tracing, when "DEBUG" is not defined, the function call to trace_function() will not be included in the compiled output
#define QORE_TRACE(args...)
#else
//! a macro for printing debugging statements; when "DEBUG" is not defined, the function call not be included in the compiled output
#define printd(args, ...)
//! a macro for tracing, when "DEBUG" is not defined, the function call to trace_function() will not be included in the compiled output
#define QORE_TRACE(x)
#endif
#endif

//#if !defined(HAVE_ISBLANK) && !defined(isblank)
//#define isblank(a) ((a) == ' ' || (a) == '\t')
//#endif

#define qore_isblank(a) ((a) == ' ' || (a) == '\t')

#endif
