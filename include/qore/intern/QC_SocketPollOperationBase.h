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

#endif // _QORE_CLASS_SOCKETPOLLOPERATIONBASE_H
