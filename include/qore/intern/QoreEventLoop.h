/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreEventLoop.h

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

#ifndef _QORE_QORE_EVENT_LOOP_H
#define _QORE_QORE_EVENT_LOOP_H

#include <qore/Qore.h>

#include <vector>
#include <unordered_map>

#ifdef DARWIN
#include <sys/event.h>
#elif defined(__linux__)
#include <sys/epoll.h>
#endif

#if defined HAVE_POLL
#include <poll.h>
#elif defined HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

//! Event flags for I/O operations
static constexpr int QORE_EV_READ  = (1 << 0);
static constexpr int QORE_EV_WRITE = (1 << 1);
static constexpr int QORE_EV_ERROR = (1 << 2);

//! Event information returned from poll operations
struct QoreEventInfo {
    int fd;         //!< File descriptor
    int events;     //!< Events that occurred (QORE_EV_READ, QORE_EV_WRITE, QORE_EV_ERROR)
    void* udata;    //!< User data associated with this fd
};

//! Persistent event loop abstraction
/**
    This class provides a persistent event loop using the most efficient
    mechanism available on each platform:
    - kqueue on macOS/BSD
    - epoll on Linux
    - poll() as fallback on other platforms

    Events are registered once and persist until explicitly removed, which
    allows proper detection of pending I/O operations (like pending connections
    on listener sockets) that may have occurred before registration.

    Usage:
    @code
    QoreEventLoop loop;
    loop.add(sock_fd, QORE_EV_READ, user_data);

    std::vector<QoreEventInfo> events;
    int count = loop.poll(events, timeout_ms);
    for (int i = 0; i < count; ++i) {
        // handle events[i]
    }

    loop.remove(sock_fd);
    @endcode
*/
class QoreEventLoop {
public:
    //! Creates a new event loop
    /** @param xsink exception sink for error reporting

        On error, an exception is raised and isValid() returns false.
    */
    DLLLOCAL QoreEventLoop(ExceptionSink* xsink);

    //! Destroys the event loop
    DLLLOCAL ~QoreEventLoop();

    //! Returns true if the event loop was created successfully
    DLLLOCAL bool isValid() const {
#if defined(DARWIN) || defined(__linux__)
        return event_fd >= 0;
#else
        return true;  // poll-based implementation is always valid
#endif
    }

    //! Adds a file descriptor to the event loop
    /** @param fd the file descriptor to add
        @param events events to monitor (QORE_EV_READ and/or QORE_EV_WRITE)
        @param udata user data pointer to associate with this fd
        @param xsink exception sink for error reporting

        @return 0 on success, -1 on error (exception raised)
    */
    DLLLOCAL int add(int fd, int events, void* udata, ExceptionSink* xsink);

    //! Modifies events for an existing file descriptor
    /** @param fd the file descriptor to modify
        @param events new events to monitor (QORE_EV_READ and/or QORE_EV_WRITE)
        @param xsink exception sink for error reporting

        @return 0 on success, -1 on error (exception raised)
    */
    DLLLOCAL int modify(int fd, int events, ExceptionSink* xsink);

    //! Removes a file descriptor from the event loop
    /** @param fd the file descriptor to remove
        @param xsink exception sink for error reporting

        @return 0 on success, -1 on error (exception raised)
    */
    DLLLOCAL int remove(int fd, ExceptionSink* xsink);

    //! Polls for events
    /** @param events vector to receive event information (will be resized as needed)
        @param timeout_ms timeout in milliseconds (-1 for infinite, 0 for non-blocking)
        @param xsink exception sink for error reporting

        @return number of ready fds (>= 0), or -1 on error (exception raised)
    */
    DLLLOCAL int poll(std::vector<QoreEventInfo>& events, int timeout_ms, ExceptionSink* xsink);

    //! Returns the number of registered file descriptors
    DLLLOCAL size_t size() const {
        return fd_map.size();
    }

    //! Returns true if no file descriptors are registered
    DLLLOCAL bool empty() const {
        return fd_map.empty();
    }

private:
    //! Internal fd info
    struct FdInfo {
        int events;     //!< Registered events
        void* udata;    //!< User data
    };

    //! Map of fd -> info
    std::unordered_map<int, FdInfo> fd_map;

#ifdef DARWIN
    int event_fd = -1;  //!< kqueue file descriptor
#elif defined(__linux__)
    int event_fd = -1;  //!< epoll file descriptor
#endif

    // Non-copyable
    QoreEventLoop(const QoreEventLoop&) = delete;
    QoreEventLoop& operator=(const QoreEventLoop&) = delete;
};

#endif // _QORE_QORE_EVENT_LOOP_H
