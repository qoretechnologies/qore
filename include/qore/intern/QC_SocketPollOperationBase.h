/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_SocketPollOperationBase.h

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

#ifndef _QORE_CLASS_SOCKETPOLLOPERATIONBASE_H

#define _QORE_CLASS_SOCKETPOLLOPERATIONBASE_H

// Public API — SocketPollOperationBase class definition
#include "qore/SocketPollOperationBase.h"

// Internal-only: QPP class init function
DLLLOCAL QoreClass* initSocketPollOperationBaseClass(QoreNamespace& qorens);

//! Called by the destructor of the Qore object of a C++ poll operation that releases its state with its last reference
/** An explicit \c delete runs the destructor while an async I/O controller can still hold the operation and continue
    it on an I/O thread; the controller then cancels the operation (see AsyncIoControllerPriv::opObjectDeleted()).
    Does not wait for the I/O threads and runs no Qore code.

    @param op the operation of the object
*/
DLLLOCAL void qore_socket_poll_op_object_deleted(SocketPollOperationBase* op);

//! Called by the destructor of the Qore object of a poll operation without a C++ poll operation (one implemented in
//! %Qore, or an HTTPClientPollOperation): a submitted operation is canceled (qore#6531)
/** Does not wait for the I/O threads and runs no Qore code; nothing is done for an operation that was not submitted
*/
DLLLOCAL void qore_poll_op_qore_object_deleted(const QoreObject* obj);

//! Releases a reference to a C++ poll operation; the last one is released by a callback worker
/** The release of the last reference runs the operation's cleanup(), which completes requests and releases objects,
    and so can run user code; a thread that holds a temporary reference in a method of the operation's connection
    (under its method guard, or a lock of the caller) or in a scan of the garbage collector must not run it.

    @param op the operation; the reference is released
*/
DLLLOCAL void qore_socket_poll_op_deref_off_thread(SocketPollOperationBase* op);

//! Holds a temporary reference to a C++ poll operation whose release must not run the operation's cleanup()
/** See qore_socket_poll_op_deref_off_thread()
*/
template <class T>
class QorePollOpOffThreadRef {
public:
    DLLLOCAL QorePollOpOffThreadRef() = default;

    //! Takes over the given reference
    DLLLOCAL explicit QorePollOpOffThreadRef(T* op) : op(op) {
    }

    DLLLOCAL ~QorePollOpOffThreadRef() {
        if (op) {
            qore_socket_poll_op_deref_off_thread(op);
        }
    }

    //! Takes over the given reference; the holder must be empty
    DLLLOCAL QorePollOpOffThreadRef& operator=(T* new_op) {
        assert(!op);
        op = new_op;
        return *this;
    }

    DLLLOCAL T* operator->() const {
        return op;
    }

    DLLLOCAL T* operator*() const {
        return op;
    }

    DLLLOCAL explicit operator bool() const {
        return op != nullptr;
    }

    DLLLOCAL QorePollOpOffThreadRef(const QorePollOpOffThreadRef&) = delete;
    DLLLOCAL QorePollOpOffThreadRef& operator=(const QorePollOpOffThreadRef&) = delete;

private:
    T* op = nullptr;
};

#endif // _QORE_CLASS_SOCKETPOLLOPERATIONBASE_H
