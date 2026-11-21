/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SocketPollEventsOps.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_INTERN_SOCKETPOLLEVENTSOPS_H
#define _QORE_INTERN_SOCKETPOLLEVENTSOPS_H

#include <qore/SocketPollOperation.h>

//! The library-internal poll operations of SocketSendPollOperation and SocketRecvPollOperationBase (qore#6916)
/** A native parent operation (the HTTP/2 and WebSocket drivers) drives these child operations and takes their
    readiness mask directly, without the hash that continuePoll() returns.
*/
class qore_socket_poll_events_ops {
public:
    //! Drives a native child send operation and returns its readiness mask directly to the parent
    DLLLOCAL static int continuePollEvents(SocketSendPollOperation* b, ExceptionSink* xsink);

    //! Drives a native child receive operation and returns its readiness mask directly to the parent
    DLLLOCAL static int continuePollEvents(SocketRecvPollOperationBase* b, ExceptionSink* xsink);

    //! Continues a send operation; with \a events, its readiness mask is stored there and no hash is returned
    DLLLOCAL static QoreHashNode* continuePollImpl(SocketSendPollOperation* b, ExceptionSink* xsink, int* events);

    //! Continues a receive operation; with \a events, its readiness mask is stored there and no hash is returned
    DLLLOCAL static QoreHashNode* continuePollImpl(SocketRecvPollOperationBase* b, ExceptionSink* xsink,
            int* events);
};

#endif
