/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreEventNotifier.h

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

#ifndef _QORE_QORE_EVENT_NOTIFIER_H
#define _QORE_QORE_EVENT_NOTIFIER_H

#include <qore/Qore.h>
#include <qore/AbstractPollableIoObjectBase.h>

#ifdef __linux__
#include <sys/eventfd.h>
#endif

//! Cross-thread event notification abstraction
/**
    This class provides an efficient mechanism for cross-thread event
    notification, using the most efficient mechanism available on each platform:
    - eventfd on Linux (single fd, counter-based, very efficient)
    - pipe on other platforms (BSD, macOS, etc.)

    The notifier can be registered with an EventLoop for integration with
    event-driven I/O.

    Usage:
    @code
    QoreEventNotifier notifier(&xsink);

    // In event loop setup:
    loop.add(notifier.fd(), QORE_EV_READ, &notifier, &xsink);

    // In another thread, to signal:
    notifier.notify();

    // After poll returns with event on notifier fd:
    notifier.acknowledge();
    @endcode

    Thread Safety:
    - notify() is thread-safe and can be called from any thread
    - acknowledge() should only be called from the thread that owns the notifier
    - fd() is thread-safe (returns immutable value)
*/
class QoreEventNotifier : public AbstractPollableIoObjectBase {
public:
    //! Creates a new event notifier
    /** @param xsink exception sink for error reporting

        On error, an exception is raised and isValid() returns false.
    */
    DLLLOCAL QoreEventNotifier(ExceptionSink* xsink);

    //! Destroys the event notifier
    DLLLOCAL ~QoreEventNotifier();

    //! Returns true if the notifier was created successfully
    DLLLOCAL bool isValid() const {
#ifdef __linux__
        return notify_fd >= 0;
#else
        return pipe_fd[0] >= 0 && pipe_fd[1] >= 0;
#endif
    }

    //! Returns the file descriptor to register with EventLoop
    /** This fd should be registered for QORE_EV_READ events.

        @return the notification fd, or -1 if not valid
    */
    DLLLOCAL int fd() const {
#ifdef __linux__
        return notify_fd;
#else
        return pipe_fd[0];  // read end
#endif
    }

    //! Implements AbstractPollableIoObjectBase::getPollableDescriptor()
    DLLLOCAL int getPollableDescriptor() const override {
        return fd();
    }

    //! Signals the notifier (thread-safe)
    /** This method can be called from any thread to wake up the event loop.
        Multiple calls to notify() before acknowledge() may be coalesced
        (on Linux with eventfd) or queued (on other platforms with pipe).
    */
    DLLLOCAL void notify();

    //! Acknowledges/consumes the notification
    /** This method should be called after the event loop returns with an
        event on this notifier's fd. It drains any pending notifications.

        @param xsink exception sink for error reporting
    */
    DLLLOCAL void acknowledge(ExceptionSink* xsink);

private:
#ifdef __linux__
    int notify_fd = -1;     //!< eventfd file descriptor
#else
    int pipe_fd[2] = {-1, -1};  //!< pipe file descriptors [read, write]
#endif

    // Non-copyable
    QoreEventNotifier(const QoreEventNotifier&) = delete;
    QoreEventNotifier& operator=(const QoreEventNotifier&) = delete;
};

#endif // _QORE_QORE_EVENT_NOTIFIER_H
