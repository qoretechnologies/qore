/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreIRFrameCheck.h

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

#ifndef _QORE_INTERN_QOREIRFRAMECHECK_H
#define _QORE_INTERN_QOREIRFRAMECHECK_H

#include <cstdint>
#include <functional>
#include <map>
#include <string>

namespace llvm {
class Module;
}

//! The stack check of compiled functions whose frame is larger than the stack guard allows (qore#6414)
/** Every compiled function checks the stack in its entry block (qore_rt_check_stack()), after its prologue has
    allocated its frame: a frame that does not fit in the stack guard (QORE_STACK_GUARD) can reach past the end of the
    thread's stack before the check runs, and the check's own call then faults.  A function whose frame can be larger
    than half the guard is therefore emitted as a thunk with the function's name and signature, which checks the stack
    for the frame before it calls the function's code (qore_rt_check_stack_reserve()), and the code as a function of
    its own (QoreIRFrameCheck::BodySuffix), which the thunk calls with a tail call.

    Whether a function's frame can be that large is decided before code generation with a bound of the frame (the
    function's allocas and every value it computes, as though each was spilled), so that the few large functions get a
    thunk.  The frame size is known only once the machine code is generated: the "warn-stack-size" diagnostic of the
    code's frame sets the variable with the size that the thunk checks, which the code generator emits after the code
    of every function of the module.  Every other function's frame is checked against the threshold in the same way: a
    frame above it with a stack check fails the code generation.
*/
//! The function attribute of a fast entry that takes its arguments over: a mask with a '1' for each argument whose
//! reference it owns, which the thunk releases when the stack has no room for the entry's frame (issue #6105)
#define QORE_OWNED_FAST_ENTRY_ARGS_ATTR "qore-owned-args"

class QoreIRFrameCheck {
public:
    //! the suffix of the name of the code of a function whose frame is checked by a thunk
    DLLLOCAL static constexpr const char* BodySuffix = ".qore.frame";

    //! the frame size above which a compiled function checks the stack for its frame before it allocates it
    DLLLOCAL static uint64_t getThreshold();

    //! emits \a m with \a emit, with a thunk that checks the stack for each function whose frame can exceed the
    //! threshold
    /** @param m the module to emit, optimized; code generation changes it
        @param emit generates the code of a module (an object file or buffer)
        @param error the error, if any
        @param thunked if not null, gets the names of the functions given a thunk with their frame sizes

        @return true for success, false if \a emit failed or a function without a thunk has a frame above the
        threshold
    */
    DLLLOCAL static bool emit(llvm::Module& m, const std::function<bool(llvm::Module&, std::string&)>& emit,
            std::string& error, std::map<std::string, uint64_t>* thunked = nullptr);
};

#endif
