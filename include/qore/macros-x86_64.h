/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  Qore Programming Language

  Copyright (C) 2003 - 2026 David Nichols

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

#ifndef _QORE_MACHINE_MACROS_H
#define _QORE_MACHINE_MACROS_H

//#pragma message "using macros-x86_64.h"

#define STACK_DIRECTION_DOWN 1

// The stack guard: the stack kept free below a thread's stack limit for the code that runs between two stack checks
// and after a check fails (raising and unwinding STACK-LIMIT-EXCEEDED).  The limit is anchored to the real end of the
// stack, so the guard does not have to cover the stack used before a thread's ThreadData is created.  The worst case
// measured (2026, Linux glibc and musl, macOS; debug and release builds) is 21.8KB, for compiling a regular expression
// at the limit; with a signal frame delivered there (AT_MINSIGSTKSZ: 4.6KB on aarch64, 3.3KB on x86_64), 26.4KB.
// 40KB leaves about 50% margin.
#define QORE_STACK_GUARD (40 * 1024)

#ifdef __GNUC__
#define HAVE_CHECK_STACK_POS

static inline size_t get_stack_pos() {
    size_t addr;
    __asm("movq %%rsp, %0" : "=g" (addr) );
    return addr;
}
#endif // #ifdef __GNUC__

#ifdef __SUNPRO_CC
#define HAVE_CHECK_STACK_POS

extern "C" size_t get_stack_pos();
#endif // #ifdef __SUNPRO_CC
#endif // #ifndef _QORE_MACHINE_MACROS_H
