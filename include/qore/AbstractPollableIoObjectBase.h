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

#include <functional>
#include <string>

class AbstractPollableIoObjectBase : public AbstractPrivateData {
public:
    //! Default constructor — assigns a unique hash for stable identity
    DLLLOCAL AbstractPollableIoObjectBase();

    virtual int getPollableDescriptor() const = 0;

    //! Calls a function with the pollable descriptor, which stays open until the function returns
    /** The async I/O controller registers the descriptor with the event queue of its I/O thread (kqueue on macOS,
        epoll on Linux), and changes the events it is registered for, only in such a call: the system must not
        register a descriptor while another thread closes it.  On macOS a close() made while kevent() registers the
        same descriptor can wait forever, so that not even SIGKILL ends the process; on every system, a descriptor
        number used after its close can already belong to another connection.

        An I/O object that another thread can close while an asynchronous operation uses it must keep the descriptor
        open until @p f returns, and close it then if the object was closed meanwhile; Socket objects do.  The default
        implementation calls @p f with getPollableDescriptor(), which is correct for an object that cannot be closed
        while an asynchronous operation uses it, such as a File, whose close() fails during a non-blocking operation.

        @param f the function to call; it is passed the descriptor, or -1 if the object has no open descriptor; it
        must not block and must not release the object

        @par Example:
        @code{.cpp}
// registers the descriptor of an I/O object with a kqueue; a close in another thread meanwhile does not release it
io->withPollableDescriptor([&](int fd) {
    if (fd >= 0) {
        struct kevent change;
        EV_SET(&change, fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
        kevent(kq, &change, 1, nullptr, 0, nullptr);
    }
});
        @endcode

        @since %Qore 3.0
    */
    DLLEXPORT virtual void withPollableDescriptor(const std::function<void(int)>& f) const;

    //! Closes the I/O object, releasing the underlying file descriptor
    /** Called by the async I/O controller when it owns the object's lifecycle
        (e.g., cancelAndClose).

        @param xsink exception sink

        @since %Qore 3.0
    */
    virtual void closeIo(ExceptionSink* xsink) = 0;

#ifdef DARWIN
    //! Sets the write end of a notification pipe for kqueue poll on macOS
    /** On macOS, closing a monitored FD silently removes its kqueue filter without
        delivering an event. This method allows Socket::poll() to register a notification
        pipe so that close_internal() can wake up kevent() when the FD is closed.

        @param fd the write end of the notification pipe, or -1 to clear

        @since %Qore 3.0
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

        @since %Qore 3.0
    */
    DLLEXPORT virtual bool hasPendingData() const { return false; }

    //! Returns the unique hash for this I/O object
    /** Computed once at construction from an atomic sequence + pointer,
        so it remains stable even if malloc reuses the same address.

        @return unique hash string

        @since %Qore 3.0
    */
    DLLLOCAL const std::string& getUniqueHash() const { return io_unique_hash; }

    //! Returns the async I/O identity hash used to group operations on one I/O object
    /** Subclasses that are lightweight wrappers around another object can
        override this so independently-created wrappers still map to the same
        controller-level socket identity.

        @return stable async I/O identity hash

        @since %Qore 3.0
    */
    virtual const std::string& getIoIdentityHash() const { return io_unique_hash; }

private:
    std::string io_unique_hash;   //!< Stable unique identity (address reuse safe)
};

#endif
