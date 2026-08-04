/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  ql_debug.h

  Qore Programming Language

  Copyright (C) 2003 - 2023 David Nichols

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

#ifndef QORE_LIB_DEBUG_H

#define QORE_LIB_DEBUG_H

DLLLOCAL void init_debug_functions(QoreNamespace& qns);

#ifdef DEBUG
//! a QCF_NO_DOMAIN_THROW violation aborts the process (the default)
#define QORE_FLAG_VIOLATION_ABORT   0
//! a QCF_NO_DOMAIN_THROW violation is only counted
/** Tests select this so that they can drive a deliberately-lying variant and assert that the check
    fired, instead of having the process abort out from under them.
 */
#define QORE_FLAG_VIOLATION_RECORD  1

//! selects how a detected code flag violation is reported
/** The mode and the counter are both process-global, not per-thread: a test that switches to
    QORE_FLAG_VIOLATION_RECORD suppresses the abort for every thread for as long as it is set, and a
    violation on any thread moves the same counter.  That is a deliberate trade for testability -
    the check runs in ~CodeEvaluationHelper(), which has no natural place to hang per-thread state -
    so a caller that reads the counter as a delta should not have other threads running code under
    test at the same time.
 */
DLLLOCAL void qore_set_flag_violation_mode(int mode);

//! returns the number of code flag violations detected since process start
DLLLOCAL int64 qore_get_flag_violations();
#endif

#endif
