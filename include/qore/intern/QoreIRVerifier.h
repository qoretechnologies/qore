/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRVerifier.h

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

#ifndef _QORE_INTERN_QOREIRVERIFIER_H
#define _QORE_INTERN_QOREIRVERIFIER_H

#include <string>

class QoreIRFunction;

class QoreIRVerifier {
public:
    static bool verify(const QoreIRFunction& func, std::string& error);

    //! Verifies the control flow and the SSA form of IR read from an AOT module, which has no AST (qore#6468)
    /** Checks what the IR interpreter and the JIT take from the IR without a check of their own, beyond the value ids
        and the handles that the reader checks (qore_aot_check_ir_value_ids()):
        - every block that can run has a terminator, which ends it (the instructions after it never run and are
          not checked), and its phis first
        - every use of a value (an operand, or a value that an instruction holds in a field of its own) is dominated
          by the definition of the value, on the paths of both the control flow and the exceptions: a value defined
          by an instruction that can raise an exception, or after it, is not available in the handler of the
          exception, and the element of a step of an iterator is not available on its done edge; the values that are
          released only if the frame holds them (the temporary of ReleaseOwnedTemp and the values that a step of an
          iterator releases) must be defined by an instruction that can run, on any path
        - a phi has an incoming value for each predecessor of its block, which is available at the end of each edge
          from the predecessor, and the kind of its values is a kind that it can have
        - the list, index and limit of a step of a typed foreach are its operands

        Blocks that cannot run (that are not reached from the entry block by any edge) are not checked.  The cost is
        linear in the size of the function for the checks, plus the iterations of the dominator computation, which
        are few for the reducible control flow of lowered code.  The ownership of the values is not verified here.

        @return true if the IR is valid, false with \a error set otherwise
    */
    static bool verifyModuleIR(const QoreIRFunction& func, std::string& error);
};

#endif
