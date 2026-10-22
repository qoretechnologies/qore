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

#include <memory>
#include <string>

//! The private data of a builtin class whose objects have a descriptor that can be polled for I/O readiness
/** A builtin class whose private data derives this class declares \c vparent=AbstractPollableIoObjectBase in its
    \c .qpp file: its objects can then be passed as the \c socket of a @ref Qore::SocketPollInfo "SocketPollInfo"
    hash and as the I/O object of an operation submitted to the @ref Qore::AsyncIoController "AsyncIoController", and
    the async I/O controller keeps their descriptor open while it has it registered (see holdPollableDescriptor()).
    Classes in binary modules can derive it as well as the classes of the library.

    @par Example:
    @code{.cpp}
// the private data of a module class whose objects wrap a socket
class MyConnection : public AbstractPollableIoObjectBase {
public:
    int getPollableDescriptor() const override {
        return socket.getSocket();
    }

    int holdPollableDescriptor(int fd, std::shared_ptr<void>& hold) const override {
        return socket.holdDescriptor(fd, hold);
    }

    void closeIo(ExceptionSink* xsink) override {
        socket.close();
    }

private:
    QoreSocket socket;
};
    @endcode
*/
class AbstractPollableIoObjectBase : public AbstractPrivateData {
public:
    //! Default constructor — assigns a unique hash for stable identity
    /** @since %Qore 3.0 exported, so that the classes of binary modules can derive this class
    */
    DLLEXPORT AbstractPollableIoObjectBase();

    //! Returns the descriptor that the async I/O controller waits on for the object's operations
    /** @return the descriptor, or -1 if the object has none, as once it is closed

        An operation that waits for events of an object without a descriptor cannot be woken by them: the controller
        runs it again at once, also when another thread closed the object without the controller before the I/O
        thread could register its descriptor, so that the operation sees the close and ends (for example with an
        exception); an operation that waits for the closed object again ends with a \c SOCKET-CLOSED exception.  An
        operation that waits only for its extra descriptors or for its poll timeout while the object has no
        descriptor, as while a host name is resolved before a connection is made, reports no events for the object.
    */
    virtual int getPollableDescriptor() const = 0;

    //! Keeps the pollable descriptor open while the async I/O controller has it registered in an event loop
    /** The async I/O controller registers the descriptor with the event queue of its I/O thread (kqueue on macOS,
        epoll on Linux) only with a hold that this method returns, and it keeps the hold until it has removed the
        descriptor from the queue again: the system gives the lowest free descriptor number to the next socket or file
        opened by any thread, so a registration, change or removal made with the number of a descriptor that another
        thread closed would act on the object that got the number next, and on macOS a close() made while kevent()
        registers the same descriptor can wait forever, so that not even SIGKILL ends the process.

        An I/O object that another thread can close while an asynchronous operation uses it must keep its descriptor
        open while a hold exists: its close shuts the descriptor down, so the operations waiting on it wake up, and the
        descriptor is closed when the last hold is released; Socket objects and event notifiers do.  The default
        implementation returns no hold, which is correct for an object that cannot be closed while an asynchronous
        operation uses it, such as a File, whose close() fails during a non-blocking operation.

        @param fd the descriptor that getPollableDescriptor() returned
        @param hold set to the hold, which keeps @p fd open until it is released; left empty if no hold is needed

        @return 0 if @p fd can be registered, -1 if it is no longer the object's descriptor: the object was closed or
        its descriptor replaced since getPollableDescriptor() returned it, so the number can already belong to
        another object

        @par Example:
        @code{.cpp}
// registers the descriptor of an I/O object with a kqueue; a close in another thread does not release it until the
// hold is released, after the registration has been deleted
int fd = io->getPollableDescriptor();
std::shared_ptr<void> hold;
if (fd >= 0 && !io->holdPollableDescriptor(fd, hold)) {
    struct kevent change;
    EV_SET(&change, fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
    kevent(kq, &change, 1, nullptr, 0, nullptr);
    // ... until the descriptor is no longer waited for
    EV_SET(&change, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    kevent(kq, &change, 1, nullptr, 0, nullptr);
    hold.reset();
}
        @endcode

        @since %Qore 3.0
    */
    DLLEXPORT virtual int holdPollableDescriptor(int fd, std::shared_ptr<void>& hold) const;

    //! Closes the I/O object, releasing the underlying file descriptor
    /** Called by the async I/O controller when it owns the object's lifecycle
        (e.g., cancelAndClose).

        @param xsink exception sink

        @since %Qore 3.0
    */
    virtual void closeIo(ExceptionSink* xsink) = 0;

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

    //! Sets the write end of a notification pipe for kqueue poll on macOS
    /** On macOS, closing a monitored FD silently removes its kqueue filter without
        delivering an event. This method allows Socket::poll() to register a notification
        pipe so that close_internal() can wake up kevent() when the FD is closed.

        It is called only on macOS but declared on every platform, as the last virtual method: binary modules do not
        see the macros of the build of %Qore, so a declaration for macOS only gave the class one layout in %Qore and
        another in modules, and declaring it last keeps the layout of the methods declared before it.

        @param fd the write end of the notification pipe, or -1 to clear

        @since %Qore 3.0
    */
    virtual void setPollNotifyFd([[maybe_unused]] int fd) {}

private:
    std::string io_unique_hash;   //!< Stable unique identity (address reuse safe)
};

#endif
