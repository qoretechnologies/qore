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

// Stack guard must be larger than the thread startup overhead (the stack consumed
// between the actual thread stack top and the point where stack_start is set in
// ThreadData).  Measured overhead in debug builds: ~52KB (due to thread init,
// module loading, etc.), so 80KB provides ~28KB margin for check_stack to detect
// overflow before hitting the OS guard page.
#define QORE_STACK_GUARD (80 * 1024)

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
