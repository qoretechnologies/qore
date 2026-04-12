/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AbstractPollableState.h

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

#ifndef _QORE_INTERN_ABSTRACTPOLLSTATE_H

#define _QORE_INTERN_ABSTRACTPOLLSTATE_H

#include <vector>

//! Descriptor for an extra fd that a poll-state wants the sync wait helper to watch.
struct ExtraWaitFd {
    int fd;           //!< fd to include in the sync readiness wait
    bool want_read;   //!< true = wait for POLLIN
    bool want_write;  //!< true = wait for POLLOUT
};

class AbstractPollState {
public:
    DLLLOCAL virtual ~AbstractPollState() = default;

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 1 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink) = 0;

    //! Returns any data captured
    DLLLOCAL virtual QoreValue takeOutput() {
        return QoreValue();
    }

    //! Returns any additional fds the sync wait helper should watch alongside the Socket's primary fd.
    /** Used by multi-fd poll states (e.g. QUIC) so SocketSyncPoll can include
        the extra fds in a single combined readiness wait.  The default
        implementation returns an empty vector.

        @return a vector of ExtraWaitFd entries; empty if no extras are needed
     */
    DLLLOCAL virtual std::vector<ExtraWaitFd> getExtraWaitFds() {
        return {};
    }
};

#endif // _QORE_INTERN_ABSTRACTPOLLSTATE_H
