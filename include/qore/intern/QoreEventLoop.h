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

#include <functional>
#include <memory>
#include <vector>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

#ifdef DARWIN
#include <sys/event.h>
#elif defined(__linux__)
#include <sys/epoll.h>
#endif

#if defined(__linux__) && defined(HAVE_IO_URING)
#include "qore/intern/QoreIoUring.h"
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
static constexpr int QORE_EV_TIMER = (1 << 3);
//! io_uring completions are ready (Linux only)
static constexpr int QORE_EV_IOURING = (1 << 4);

//! Sentinel fd value for timer events in QoreEventInfo
static constexpr int QORE_TIMER_FD = -2;

//! Sentinel fd value for io_uring completion events
static constexpr int QORE_IOURING_FD = -3;

//! Event information returned from poll operations
struct QoreEventInfo {
    int fd;             //!< File descriptor (QORE_TIMER_FD for timer events)
    int events;         //!< Events that occurred (QORE_EV_READ, QORE_EV_WRITE, QORE_EV_ERROR, QORE_EV_TIMER)
    void* udata;        //!< User data of the registration the events are for
    int64_t timer_id;   //!< Timer ID (only valid when events & QORE_EV_TIMER)
    //! The key of the registration the events are for (see QoreEventLoop::addRegistration()); 0 for add()
    uint64_t key;

    QoreEventInfo() : fd(0), events(0), udata(nullptr), timer_id(0), key(0) {}
    QoreEventInfo(int fd, int events, void* udata, int64_t timer_id = 0, uint64_t key = 0)
        : fd(fd), events(events), udata(udata), timer_id(timer_id), key(key) {}
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

    A descriptor can have several registrations, each with its own key, events
    and user data (see addRegistration()): the descriptor is polled for the
    union of their events, poll() reports each registration's events with its
    key and user data, and removing a registration polls the descriptor for the
    events of the others, removing it from the system only with the last one.
    add(), modify() and remove() manage the registration with key 0.

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

    //! Adds a registration of a descriptor, or changes the events and user data of an existing one
    /** The descriptor is polled for the union of the events of its registrations; poll() reports the events of each
        registration that it waits for, and errors to every registration, with the registration's key and user data.

        @param fd the file descriptor
        @param key the key of the registration, unique among the registrations of the descriptor
        @param events events to monitor (QORE_EV_READ and/or QORE_EV_WRITE)
        @param udata user data reported with the registration's events
        @param xsink exception sink for error reporting

        @return 0 on success, -1 on error (exception raised; the registration is not made)

        @par Example:
        @code{.cpp}
// a socket and an operation that also waits for the socket's descriptor
loop.addRegistration(fd, socket_key, QORE_EV_READ, socket_obj, xsink);
loop.addRegistration(fd, op_key, QORE_EV_WRITE, op_obj, xsink);
// the descriptor is polled for reading and writing; removing one registration keeps the other
loop.removeRegistration(fd, op_key, xsink);
        @endcode
    */
    DLLLOCAL int addRegistration(int fd, uint64_t key, int events, void* udata, ExceptionSink* xsink);

    //! Removes a registration of a descriptor
    /** The descriptor is then polled for the events of its other registrations, and removed from the system with the
        last one.  Removing a registration that does not exist does nothing.

        @return 0 on success, -1 on error (exception raised)
    */
    DLLLOCAL int removeRegistration(int fd, uint64_t key, ExceptionSink* xsink);

    //! Returns true if the descriptor has a registration with the given key
    DLLLOCAL bool hasRegistration(int fd, uint64_t key) const;

    //! Calls @p f with the key and the events of each registration of the descriptor
    DLLLOCAL void forEachRegistration(int fd, const std::function<void(uint64_t key, int events)>& f) const;

    //! Polls for events
    /** @param events vector to receive event information (will be resized as needed)
        @param timeout_ms timeout in milliseconds (-1 for infinite, 0 for non-blocking)
        @param xsink exception sink for error reporting

        @return number of ready fds (>= 0), or -1 on error (exception raised)
    */
    DLLLOCAL int poll(std::vector<QoreEventInfo>& events, int timeout_ms, ExceptionSink* xsink);

    //! Adds a timer to the event loop
    /** @param deadline_us absolute deadline in microseconds since the epoch
        @param udata user data pointer to associate with this timer
        @param preset_id if > 0, use this ID instead of auto-allocating

        @return the timer ID
    */
    DLLLOCAL int64_t addTimer(int64_t deadline_us, void* udata, int64_t preset_id = 0);

    //! Cancels a timer
    /** @param id the timer ID returned by addTimer()

        @return true if the timer was found and canceled, false if not found
    */
    DLLLOCAL bool cancelTimer(int64_t id);

    //! Returns the number of active timers
    DLLLOCAL size_t timerCount() const {
        return timer_set.size();
    }

    //! Returns the number of registered event sources (fds + timers)
    DLLLOCAL size_t size() const {
#ifdef DARWIN
        return fd_map.size() + user_event_map.size() + timer_set.size();
#else
        return fd_map.size() + timer_set.size();
#endif
    }

    //! Returns true if no event sources are registered (no fds and no timers)
    DLLLOCAL bool empty() const {
#ifdef DARWIN
        return fd_map.empty() && user_event_map.empty() && timer_set.empty();
#else
        return fd_map.empty() && timer_set.empty();
#endif
    }

#if defined(__linux__) && defined(HAVE_IO_URING)
    //! Returns the io_uring instance (nullptr if unavailable)
    /** @since %Qore 3.0
    */
    DLLLOCAL QoreIoUring* getIoUring() { return io_uring_.get(); }
#endif

#ifdef DARWIN
    //! Returns the kqueue file descriptor (macOS only)
    /** Used by QoreEventNotifier to bind for EVFILT_USER optimization.
    */
    DLLLOCAL int getKqueueFd() const {
        return event_fd;
    }

    //! Adds a user event (EVFILT_USER) to the event loop
    /** @param ident unique identifier for the user event
        @param udata user data pointer to associate with this event
        @param xsink exception sink for error reporting

        @return 0 on success, -1 on error (exception raised)
    */
    DLLLOCAL int addUserEvent(uintptr_t ident, void* udata, ExceptionSink* xsink);

    //! Removes a user event from the event loop
    /** @param ident unique identifier for the user event
        @param xsink exception sink for error reporting

        @return 0 on success, -1 on error (exception raised)
    */
    DLLLOCAL int removeUserEvent(uintptr_t ident, ExceptionSink* xsink);
#endif

#ifdef DEBUG
    //! Debug-only: sets a hook that add() and modify() call with the descriptor before they register it
    /** Lets a test close the descriptor's socket from another thread at the point where the descriptor is about to
        be registered with the system's event queue; see AsyncIoRegistrationCloseTest in the C++ unit tests.

        @param hook the hook, or an empty function to remove it; the loop must only be used by the calling thread
    */
    DLLLOCAL void dbgSetBeforeRegister(std::function<void(int)> hook) {
        dbg_before_register = std::move(hook);
    }

    //! Debug-only: sets a hook that remove() calls with a registered descriptor before it removes it
    /** Lets a test check that a descriptor is still open when the loop removes it; see AsyncIoDescriptorHoldTest in
        the C++ unit tests.

        @param hook the hook, or an empty function to remove it; set before the loop's thread starts, or by it
    */
    DLLLOCAL void dbgSetBeforeRemove(std::function<void(int)> hook) {
        dbg_before_remove = std::move(hook);
    }
#endif

private:
#ifdef DEBUG
    //! Debug-only: called with the descriptor before add() or modify() registers it; see dbgSetBeforeRegister()
    std::function<void(int)> dbg_before_register;
    //! Debug-only: called with a registered descriptor before remove() removes it; see dbgSetBeforeRemove()
    std::function<void(int)> dbg_before_remove;
#endif

    //! A registration of a descriptor; see addRegistration()
    struct Registration {
        int events;     //!< Registered events
        void* udata;    //!< User data
    };

    //! Internal fd info
    struct FdInfo {
        int events = 0;         //!< The events registered with the system: the union of the registrations' events
        void* udata = nullptr;  //!< User data given to the system (that of the first registration)
        //! The registrations of the descriptor, by key
        std::map<uint64_t, Registration> regs;
    };

    //! Registers the descriptor with the system; @p events are the union of its registrations' events
    DLLLOCAL int kernelAdd(int fd, int events, void* udata, ExceptionSink* xsink);

    //! Changes the events of a descriptor registered with the system (adds it if the system has dropped it)
    DLLLOCAL int kernelModify(int fd, int events, void* udata, ExceptionSink* xsink);

    //! Removes a descriptor from the system
    DLLLOCAL int kernelRemove(int fd, ExceptionSink* xsink);

    //! Replaces the events of each descriptor in @p events with the events of each of its registrations
    DLLLOCAL void expandRegistrations(std::vector<QoreEventInfo>& events);

    //! Scratch buffer for expandRegistrations()
    std::vector<QoreEventInfo> expand_buf;

    //! Timer entry for the sorted timer set
    struct TimerEntry {
        int64_t deadline_us;    //!< Absolute deadline (usec since epoch)
        int64_t id;             //!< Unique timer ID
        void* udata;            //!< Caller-managed user data

        bool operator<(const TimerEntry& o) const {
            if (deadline_us != o.deadline_us) {
                return deadline_us < o.deadline_us;
            }
            return id < o.id;
        }
    };

    //! Sorted set of timers (ordered by deadline, then ID)
    std::set<TimerEntry> timer_set;

    //! Map of timer ID -> iterator into timer_set for O(log n) cancel
    std::unordered_map<int64_t, std::set<TimerEntry>::iterator> timer_id_map;

    //! Next timer ID to allocate
    int64_t next_timer_id = 1;

    //! Map of fd -> info
    std::unordered_map<int, FdInfo> fd_map;

#ifdef DARWIN
    int event_fd = -1;  //!< kqueue file descriptor
    //! Map of user event ident -> udata (for EVFILT_USER)
    std::unordered_map<uintptr_t, void*> user_event_map;
#elif defined(__linux__)
    int event_fd = -1;  //!< epoll file descriptor
#if defined(HAVE_IO_URING)
    //! Optional io_uring for async file I/O (nullptr if unavailable)
    std::unique_ptr<QoreIoUring> io_uring_;
#endif
#endif

    //! Collect expired timers and append to events vector
    DLLLOCAL void collectExpiredTimers(std::vector<QoreEventInfo>& events);

    // Non-copyable
    QoreEventLoop(const QoreEventLoop&) = delete;
    QoreEventLoop& operator=(const QoreEventLoop&) = delete;
};

#endif // _QORE_QORE_EVENT_LOOP_H
