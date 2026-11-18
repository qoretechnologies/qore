/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreIRInterpreterGivenArgs.cpp

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

// The code of the IR interpreter's calls that give the references of their temporary arguments to the callee (the
// GIVEN instantiations of tryExecuteInterpreterInlineIRFunction() and the like and of evalInvoke(), issue #5882),
// compiled from QoreIRInterpreter.cpp in a unit of its own, outside the single compilation unit: in that unit, it
// would change how the compiler inlines the helpers that the calls that give nothing share with it, and so the code of
// these calls.  QoreIRInterpreter.cpp leaves out what this code does not use.
#define QORE_IR_INTERPRETER_GIVEN_ARGS_UNIT 1

// the helpers of QoreIRInterpreter.cpp that this code does not use are not compiled
#pragma GCC diagnostic ignored "-Wunused-function"

#include "QoreIRInterpreter.cpp"
