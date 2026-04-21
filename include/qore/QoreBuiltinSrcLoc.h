/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreBuiltinSrcLoc.h

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

#ifndef _QORE_QOREBUILTINSRCLOC_H
#define _QORE_QOREBUILTINSRCLOC_H

//! RAII helper that records a .qpp source location for the duration of
//! its scope, so that builtin variant constructors running inside can
//! stash it on the variant.  `qpp` emits one of these around each
//! `addBuiltinVariant` / `addMethod` / `addConstructor` / `addCopy` /
//! `addStaticMethod` / `addAbstractMethod` call it generates, so that
//! reflection (`FunctionVariant::getSourceLocation()`) can report the
//! declaring `.qpp` file and line instead of the generic `<builtin>`
//! sentinel.
/**
    Usage (hand-written code — normally only `qpp` emits these):
    @code
        QoreBuiltinSrcLocHelper _l("my_module.qpp", 42);
        ns.addBuiltinVariant("my_fn", (q_func_t)f_my_fn_Vs, ...);
    @endcode

    Nested helpers are supported: each constructor saves the prior
    thread-local value and the destructor restores it.  Locations are
    interned in an immortal pool keyed by `(file, line)` so memory use
    is bounded by the number of distinct builtin registration sites,
    not by the number of times the library is loaded.

    @since %Qore 2.3
*/
class QoreBuiltinSrcLocHelper {
public:
    //! Push the given source location on the thread-local stack.
    /**
        @param file the source file name (typically a `.qpp` path); must
            outlive the helper, but interning ensures the stored pointer
            on the variant remains valid indefinitely
        @param line the declaring line number within the file

        @since %Qore 2.3
    */
    DLLEXPORT QoreBuiltinSrcLocHelper(const char* file, int line);

    //! Restore the previous thread-local source location.
    DLLEXPORT ~QoreBuiltinSrcLocHelper();

private:
    //! Opaque pointer to the previously-active thread-local source
    //! location; restored on destruction.  Typed as `const void*` to
    //! keep the private internal type (`QoreProgramLocation`) out of
    //! the public ABI.
    const void* saved;

    QoreBuiltinSrcLocHelper(const QoreBuiltinSrcLocHelper&) = delete;
    QoreBuiltinSrcLocHelper& operator=(const QoreBuiltinSrcLocHelper&) = delete;
};

#endif
