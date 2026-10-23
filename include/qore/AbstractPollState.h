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

//! Descriptor for an extra fd that a poll-state wants the controller to watch.
struct ExtraWaitFd {
    int fd;           //!< fd to include in the readiness wait
    bool want_read;   //!< true = wait for POLLIN
    bool want_write;  //!< true = wait for POLLOUT
};

//! The state of a non-blocking socket action, such as a connection, that is advanced with continuePoll()
/** The caller, such as the poll operation of a module, waits for the events that continuePoll() returns on the
    socket's descriptor, for the descriptors that getExtraWaitFds() returns, and at most for the time that
    getPollTimeoutMs() returns, and then calls continuePoll() again.  While the socket has no descriptor (it returns
    -1 as its descriptor), as while the host name of a connection is resolved, only the extra descriptors and the
    timeout can continue the action, and the caller waits for no events of the socket.
*/
class AbstractPollState {
public:
    DLLLOCAL virtual ~AbstractPollState() = default;

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 1 = error (exception raised)

        The events are those of the socket's descriptor; see the class description for a socket that has none.
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink) = 0;

    //! Returns any data captured
    DLLLOCAL virtual QoreValue takeOutput() {
        return QoreValue();
    }

    //! Returns any additional fds the async I/O controller should watch alongside the Socket's primary fd.
    /** Used by multi-fd poll states (e.g. QUIC) so controller-backed waits
        can include the extra fds in a single combined readiness wait.  The
        default implementation returns an empty vector.

        @return a vector of ExtraWaitFd entries; empty if no extras are needed

        @note the descriptors can change with each call of continuePoll(), so they are read again after each call
     */
    DLLLOCAL virtual std::vector<ExtraWaitFd> getExtraWaitFds() {
        return {};
    }

    //! Returns the time after which continuePoll() must be called again, also if no descriptor is ready
    /** The default implementation returns -1.

        @return the time in milliseconds, or -1 if only the descriptors continue the state

        @par Example:
        @code{.cpp}
// waits for the next step of a connection made with QoreSocket::startConnect()
int rc = state->continuePoll(xsink);
if (rc > 0) {
    std::vector<struct pollfd> fds;
    // there is no socket descriptor while the host name is resolved
    if (sock.getSocket() >= 0) {
        short events = ((rc & SOCK_POLLIN) ? POLLIN : 0) | ((rc & SOCK_POLLOUT) ? POLLOUT : 0);
        fds.push_back({sock.getSocket(), events, 0});
    }
    for (const ExtraWaitFd& extra : state->getExtraWaitFds()) {
        short events = (extra.want_read ? POLLIN : 0) | (extra.want_write ? POLLOUT : 0);
        fds.push_back({extra.fd, events, 0});
    }
    poll(fds.data(), fds.size(), state->getPollTimeoutMs());
    // ... and call continuePoll() again
}
        @endcode

        @since %Qore 3.0
     */
    DLLLOCAL virtual int getPollTimeoutMs() const {
        return -1;
    }
};

#endif // _QORE_INTERN_ABSTRACTPOLLSTATE_H
