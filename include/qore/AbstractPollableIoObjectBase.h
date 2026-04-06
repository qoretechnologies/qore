/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AbstractPollableIoObjectBase.h

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

#ifndef _QORE_ABSTRACTPOLLABLEIOOBJECTBASE_H

#define _QORE_ABSTRACTPOLLABLEIOOBJECTBASE_H

#include "qore/AbstractPrivateData.h"

class AbstractPollableIoObjectBase : public AbstractPrivateData {
public:
    virtual int getPollableDescriptor() const = 0;

    //! Closes the I/O object, releasing the underlying file descriptor
    /** Called by the async I/O controller when it owns the object's lifecycle
        (e.g., cancelAndClose).

        @param xsink exception sink

        @since %Qore 2.3
    */
    virtual void closeIo(ExceptionSink* xsink) = 0;

#ifdef DARWIN
    //! Sets the write end of a notification pipe for kqueue poll on macOS
    /** On macOS, closing a monitored FD silently removes its kqueue filter without
        delivering an event. This method allows Socket::poll() to register a notification
        pipe so that close_internal() can wake up kevent() when the FD is closed.

        @param fd the write end of the notification pipe, or -1 to clear

        @since %Qore 2.3
    */
    virtual void setPollNotifyFd(int fd) {}
#endif

    //! Returns true if the I/O object has buffered data ready to read
    /** For SSL sockets, this checks SSL_pending() — data decrypted by a
        previous SSL_read but not yet consumed. This data is invisible to
        epoll/kqueue (no TCP socket readiness), so the controller must check
        this to avoid missing buffered data.

        Safe to call from any thread. No syscalls, no exceptions.

        @return true if buffered data is available

        @since %Qore 2.3
    */
    DLLEXPORT virtual bool hasPendingData() const { return false; }
};

#endif
