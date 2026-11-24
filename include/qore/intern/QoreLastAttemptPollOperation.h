/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreLastAttemptPollOperation.h

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

#ifndef _QORE_INTERN_QORELASTATTEMPTPOLLOPERATION_H
#define _QORE_INTERN_QORELASTATTEMPTPOLLOPERATION_H

#include "qore/SocketPollOperationBase.h"

//! An internal poll operation that can get a last step when its deadline has passed (qore#7480)
/** The async I/O controller ends an operation whose deadline has passed with a timeout, also when its descriptor was
    reported ready in the same iteration of its I/O thread.  An operation of the library that derives from this class
    as well as from SocketPollOperationBase, and whose completesInLastAttempt() returns true, gets one last step
    instead, whether or not its descriptor was reported ready in that iteration, unless something else woke it in that
    iteration or its descriptor was reported with an error or hang-up: a step that completes it (with a result or an
    error) is delivered, and a step that asks to wait again ends it with the timeout.  This makes a result that is
    available at the deadline available to the caller (ex: a connection in a listener's backlog when the I/O thread
    gets to the listener only after the deadline).

    It is internal, so that the layout of the public SocketPollOperationBase, which modules derive from, does not
    change; the controller finds it with qore_poll_op_completes_in_last_attempt(), only when a deadline has passed.
*/
class QoreLastAttemptPollOperation {
public:
    DLLLOCAL virtual ~QoreLastAttemptPollOperation() = default;

    //! Returns true if the operation gets a last step when its deadline has passed
    /** Return true only if the step of the operation, ready or not, either completes it or asks to wait again without
        starting anything (no other phase, no data consumed and dropped, no other I/O started), and only for an
        operation whose continuePoll() runs on the I/O thread (see SocketPollOperationBase::needsWorkerDispatch()); an
        accept that negotiates TLS, for example, must not, as its step can accept a connection and start the TLS
        handshake, which the timeout would then discard.
    */
    DLLLOCAL virtual bool completesInLastAttempt() const = 0;
};

//! Returns true if the operation gets a last step when its deadline has passed (see QoreLastAttemptPollOperation)
DLLLOCAL inline bool qore_poll_op_completes_in_last_attempt(const SocketPollOperationBase* op) {
    const QoreLastAttemptPollOperation* l = dynamic_cast<const QoreLastAttemptPollOperation*>(op);
    return l && l->completesInLastAttempt();
}

#endif
