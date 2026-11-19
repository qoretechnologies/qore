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
        - no value is used after its release on any path to the use: a release that clears the slot of the value
          (ReleaseOwnedTemp, IteratorRelease, the release of an iterator at the end of its loop or on an exception
          of its step, the values that a step releases) can be repeated, as it then finds nothing to release, but a
          release of the object (ContextDestroy, RefForeachCleanup, RefForeachFinalize, ExceptionResume, Decref,
          DecrefNoThrow, and PopImplicitArg, which releases the arguments that its push set) cannot; the handle of
          an object and its copies by phis are released together, as releasing one leaves the others pointing to
          the released object
        - the stacks that the frame and the thread keep are balanced: the same temp scopes, scopes, implicit
          arguments, implicit elements and contexts are open on every path into a point, and each pop is of the
          innermost entry open (an implicit argument is restored from the value of its push, a context destroyed
          through the value of its creation; an implicit element is a value that a pop restores, which also closes
          the elements pushed after it, and is refused only when it is not open); a landing pad unwinds the scopes
          to its depth.  The paths of the exceptions are followed with the stacks of their raise points through
          the blocks that only exceptions reach, up to a block of the normal control flow, whose stacks they must
          have (qore#6658): a handler that closes again a scope or an implicit element that its path has closed
          (qore#6643) is refused.  The temp scopes are not followed on these paths: an exception drains them to
          the temp scope of its handler, which IR read from a module does not give
        - the container that an instruction changes without a check of its type (ListAppend, ListSetInt,
          ListSetFloat, ListSetValue, ListSetLength, SetImplicitArgv: a list; HashSetKeyValue,
          HashSetKeyValueFold: a hash) is created by an instruction of the function that creates a container of
          that type, and a list whose entries native code reads through their address (ListGetInt, ListGetFloat,
          ListGetValueNoRefUnchecked, a step of a typed foreach) is not changed by the function; the indexes
          are not checked (the lowering bounds them, and a module whose IR could give others gives native code too,
          which nothing verifies)

        Blocks that cannot run (that are not reached from the entry block by any edge) are not checked.  The cost is
        linear in the size of the function for the checks, plus the iterations of the dominator computation, which
        are few for the reducible control flow of lowered code, and of the dataflow of the releases, whose state at
        the entry of a block only rises; the state of a block holds the values released on the paths to it, which
        are few in lowered code, as a release ends the use of a temporary on the path that leaves its statement.
        A pop of an implicit argument, an implicit element or a context with an exception target, and a landing pad
        that is not the first instruction of its block, which the lowering does not give, are refused.  An opcode that releases or transfers a value, or that pushes on or pops from
        a stack of the frame or of the thread, is listed in qore_ir_verify_released_operand() or
        qore_ir_verify_stack_effect() in QoreIRVerifier.cpp; any other opcode takes its operands as uses, defines its
        result and has no effect on the stacks, and its terminator flag and successors are those of the opcode
        registry and qore_ir_visit_successors().

        @return true if the IR is valid, false with \a error set otherwise
    */
    static bool verifyModuleIR(const QoreIRFunction& func, std::string& error);
};

#endif
