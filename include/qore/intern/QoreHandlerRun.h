/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHandlerRun.h

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

#ifndef _QORE_INTERN_QOREHANDLERRUN_H
#define _QORE_INTERN_QOREHANDLERRUN_H

#include <qore/Qore.h>

#include <cassert>

//! The context of a handler (on_exit, on_success, on_error) that runs out of line, as its own function (#6074)
/** The AST interpreter runs the handlers of a block with a reference to the return value of the function
    (StatementBlock::execIntern()): a return statement in a handler, or in a handler of a handler, replaces it when it
    executes and releases the value that it replaces (ReturnStatement::execImpl()); a return without a value replaces
    it with no value; a handler that ends without a return leaves it as it is.  A handler that the compiled modes run
    out of line is a function of its own, which hands the value of each of its return statements to the function that
    it runs for with QoreIROpcode::HandlerReturn (qore_rt_handler_return()) when the statement executes, through the
    context of the run: the code that runs the handler (the IR interpreter's executeHandlerBody(), the native
    qore_rt_exec_on_block_exit_hold()) pushes a context for each handler with QoreHandlerRunGuard.

    The hand-over matters only while the handler runs during an unwind (an exception or a thread_exit): the value
    replaces the value of the return that the unwind interrupted, which the function holds until its locals are
    released, as the AST interpreter keeps the value of a return until the function ends; or in the block cleanups of
    a return statement whose handlers run out of line past the inline cap of the lowering (returning, #6137): the
    value replaces the value of the return, which takes it when its cleanups end
    (QoreIROpcode::TakeHandedOverReturn).  Otherwise the return value of the function is not pending (a block left
    without a return, or a return whose handlers were lowered inline, which replace its value themselves), and the
    value is ignored, as the AST interpreter's is when the block is not left by a return.
*/
struct QoreHandlerRunContext {
    //! Takes the value of a return statement of the handler (borrowed: the function takes a reference of its own)
    using hand_over_t = void (*)(void* data, QoreValue value, ExceptionSink* xsink);

    //! true if the handler runs while an exception or a thread_exit unwinds the block of its function
    bool unwinding = false;
    //! true if the handler runs in the block cleanups of a return statement past the inline cap of the lowering, whose
    //! value the values handed over replace (QoreIRScopeExitInstruction::hand_over_return, #6137)
    bool returning = false;
    //! the function that takes the values that the handler hands over, if any
    hand_over_t hand_over = nullptr;
    //! the data of hand_over
    void* data = nullptr;
    //! the context of the run that this one is nested in, if any
    QoreHandlerRunContext* prev = nullptr;
    //! the index of the IR frame of the handler in the thread's IR frame pool, if the handler runs in the IR
    //! interpreter: a run of the handlers of a block of the handler itself, in that frame, forwards the values that
    //! its handlers hand over to this run's function (see forwardFrom())
    size_t ir_frame_index = static_cast<size_t>(-1);

    //! Makes this run of the handlers of a block of a handler body forward the values that its handlers hand over to
    //! the function that the handler runs for, as the AST interpreter runs the handlers of a handler with the same
    //! reference to the return value of its function (#6074)
    DLLLOCAL void forwardFrom(const QoreHandlerRunContext& outer) {
        unwinding = outer.unwinding;
        returning = outer.returning;
        hand_over = outer.hand_over;
        data = outer.data;
    }
};

//! Returns the context of the innermost handler run of the current thread, if any (see QoreHandlerRunContext)
DLLLOCAL QoreHandlerRunContext*& qore_handler_run_context();

//! Pushes the context of a handler run for the lifetime of the object, on every way out (an exception, a thread_exit,
//! a cancellation), and pops it
class QoreHandlerRunGuard {
public:
    DLLLOCAL QoreHandlerRunGuard(QoreHandlerRunContext& ctx) : ctx(ctx) {
        QoreHandlerRunContext*& top = qore_handler_run_context();
        ctx.prev = top;
        top = &ctx;
    }

    DLLLOCAL ~QoreHandlerRunGuard() {
        QoreHandlerRunContext*& top = qore_handler_run_context();
        // the runs are nested: the stack is balanced when a run ends
        assert(top == &ctx);
        top = ctx.prev;
    }

    QoreHandlerRunGuard(const QoreHandlerRunGuard&) = delete;
    QoreHandlerRunGuard& operator=(const QoreHandlerRunGuard&) = delete;

private:
    QoreHandlerRunContext& ctx;
};

//! Hands the value of a return statement of a handler run out of line to the function that it runs for (#6074)
/** See QoreHandlerRunContext.  \a value is borrowed.  Does nothing outside an unwind or the cleanups of a return past
    the inline cap, or outside a handler run.
*/
DLLLOCAL void qore_handler_return(QoreValue value, ExceptionSink* xsink);

#endif
