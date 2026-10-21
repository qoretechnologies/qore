/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreEventLoop.cpp

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

#include <qore/Qore.h>
#include "qore/intern/QoreEventLoop.h"
#include "qore/intern/QorePollCompat.h"

#include <cerrno>
#include <climits>
#include <cstring>
#include <new>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif
#ifdef DARWIN
#include <sys/ioctl.h>
#endif

#include <atomic>

//! Returns the current wall-clock time in microseconds since the Unix epoch (CLOCK_REALTIME).
/** Used only for comparing against user-supplied absolute deadlines (\c addTimer() deadlines
    come from \c DateTime::getEpochMicrosecondsUTC() and must therefore be compared in
    realtime).  For internal "wait at most N ms" deadline arithmetic (EINTR retry remaining
    time), use \c q_get_monotonic_us() instead so the bound is not skewed by realtime clock
    adjustments.
*/
static int64_t q_epoch_us_fast() {
    int us;
    int64_t secs = q_epoch_us(us);
    return secs * 1000000LL + us;
}

//! Sets @p ts to a wait of @p us microseconds
static void q_wait_us_to_timespec(int64_t us, struct timespec& ts) {
    ts.tv_sec = static_cast<time_t>(us / 1000000);
    ts.tv_nsec = static_cast<long>((us % 1000000) * 1000);
}

//! Returns a wait of @p us microseconds in milliseconds, rounded up, for a call that only waits whole milliseconds
/** A timer must never fire early: a wait rounded down ends before the timer is due, and the loop then polls again
    without blocking until it is, busy-looping its thread.  A wait rounded up ends late by less than the resolution.
*/
static int q_wait_us_to_ms(int64_t us) {
    int64_t ms = us / 1000 + (us % 1000 ? 1 : 0);
    return ms > INT_MAX ? INT_MAX : static_cast<int>(ms);
}

#if defined(__linux__) && (defined(HAVE_EPOLL_PWAIT2) || (defined(SYS_epoll_pwait2) && defined(__LP64__)))
#define QORE_HAVE_EPOLL_NS_WAIT 1
//! Set when the system does not provide epoll_pwait2() (Linux before 5.11, or a seccomp filter refusing it)
static std::atomic<bool> q_epoll_pwait2_unavailable{false};

//! Waits for epoll events with nanosecond resolution: @p ts is the wait
/** @return the result of the call, or -2 if epoll_pwait2() is not available, in which case the caller waits with
    epoll_wait()
*/
static int q_epoll_pwait2(int epfd, struct epoll_event* events, int maxevents, const struct timespec* ts) {
    if (q_epoll_pwait2_unavailable.load(std::memory_order_relaxed)) {
        return -2;
    }
#ifdef HAVE_EPOLL_PWAIT2
    int rc = epoll_pwait2(epfd, events, maxevents, ts, nullptr);
#else
    // the C library has no wrapper (musl): on a 64-bit system struct timespec is the kernel's __kernel_timespec
    int rc = static_cast<int>(syscall(SYS_epoll_pwait2, epfd, events, maxevents, ts, nullptr, 0));
#endif
    if (rc < 0 && (errno == ENOSYS || errno == EPERM)) {
        q_epoll_pwait2_unavailable.store(true, std::memory_order_relaxed);
        return -2;
    }
    return rc;
}
#endif

QoreEventLoop::QoreEventLoop(ExceptionSink* xsink) {
#ifdef DARWIN
    event_fd = kqueue();
    if (event_fd < 0) {
        xsink->raiseErrnoException("EVENT-LOOP-ERROR", errno, "kqueue() failed");
    }
#elif defined(__linux__)
    event_fd = epoll_create1(EPOLL_CLOEXEC);
    if (event_fd < 0) {
        xsink->raiseErrnoException("EVENT-LOOP-ERROR", errno, "epoll_create1() failed");
    }
#if defined(HAVE_IO_URING)
    if (event_fd >= 0) {
        // Initialize io_uring (non-fatal if it fails — e.g., old kernel, seccomp)
        ExceptionSink uring_xsink;
        io_uring_ = std::make_unique<QoreIoUring>(64, &uring_xsink);
        if (!io_uring_->isValid()) {
            printd(1, "QoreEventLoop: io_uring unavailable, "
                "file I/O will use blocking reads in the event loop\n");
            uring_xsink.clear();
            io_uring_.reset();
        } else {
            // Register io_uring's eventfd with epoll
            struct epoll_event ev;
            memset(&ev, 0, sizeof(ev));
            ev.events = EPOLLIN;
            ev.data.fd = io_uring_->getEventFd();
            if (epoll_ctl(event_fd, EPOLL_CTL_ADD, ev.data.fd, &ev) < 0) {
                printd(0, "QoreEventLoop: failed to register io_uring eventfd "
                    "with epoll: %s\n", strerror(errno));
                io_uring_.reset();
            } else {
                printd(2, "QoreEventLoop: io_uring enabled, eventfd=%d\n",
                    io_uring_->getEventFd());
            }
        }
    }
#endif // HAVE_IO_URING
#endif
    // poll-based implementation doesn't need initialization
}

QoreEventLoop::~QoreEventLoop() {
#if defined(__linux__) && defined(HAVE_IO_URING)
    // Remove io_uring eventfd from epoll and destroy io_uring before closing epoll fd
    if (io_uring_ && event_fd >= 0) {
        epoll_ctl(event_fd, EPOLL_CTL_DEL, io_uring_->getEventFd(), nullptr);
        io_uring_.reset();
    }
#endif
#if defined(DARWIN) || defined(__linux__)
    if (event_fd >= 0) {
        ::close(event_fd);
    }
#endif
}

int QoreEventLoop::add(int fd, int events, void* udata, ExceptionSink* xsink) {
    return addRegistration(fd, 0, events, udata, xsink);
}

int QoreEventLoop::modify(int fd, int events, ExceptionSink* xsink) {
    auto it = fd_map.find(fd);
    if (it == fd_map.end()) {
        xsink->raiseException("EVENT-LOOP-ERROR", "fd %d is not registered", fd);
        return -1;
    }
    auto rit = it->second.regs.find(0);
    if (rit == it->second.regs.end()) {
        xsink->raiseException("EVENT-LOOP-ERROR", "fd %d is not registered", fd);
        return -1;
    }
    return addRegistration(fd, 0, events, rit->second.udata, xsink);
}

int QoreEventLoop::remove(int fd, ExceptionSink* xsink) {
    return removeRegistration(fd, 0, xsink);
}

int QoreEventLoop::addRegistration(int fd, uint64_t key, int events, void* udata, ExceptionSink* xsink) {
    auto it = fd_map.find(fd);
    if (it == fd_map.end()) {
#ifdef DEBUG
        if (dbg_before_register) {
            dbg_before_register(fd);
        }
#endif
        // a named pipe is watched with select(2) in poll() instead of by kqueue (see fifo_count)
        bool fifo = false;
#ifdef DARWIN
        fifo = qore_fd_poll_misses_hangup(fd);
#endif
        if (!fifo && kernelAdd(fd, events, udata, xsink)) {
            return -1;
        }
        FdInfo& info = fd_map[fd];
        info.events = events;
        info.udata = udata;
        info.regs[key] = {events, udata};
#ifdef DARWIN
        if (fifo) {
            info.fifo = true;
            ++fifo_count;
        }
#endif
        return 0;
    }

    FdInfo& info = it->second;
    auto rit = info.regs.find(key);
    const bool existed = rit != info.regs.end();
    const Registration prev = existed ? rit->second : Registration{0, nullptr};
    info.regs[key] = {events, udata};
    int all = 0;
    for (const auto& [k, reg] : info.regs) {
        all |= reg.events;
    }
#ifdef DEBUG
    if (dbg_before_register) {
        dbg_before_register(fd);
    }
#endif
    // the system is given the union of the events again also when it is unchanged: the system drops a descriptor
    // that is closed, and a registration without a hold of its descriptor can have outlived it (see kernelModify());
    // a named pipe is watched for info.events by poll()
    if (!info.fifo && kernelModify(fd, all, info.udata, xsink)) {
        if (existed) {
            info.regs[key] = prev;
        } else {
            info.regs.erase(key);
        }
        return -1;
    }
    info.events = all;
    return 0;
}

int QoreEventLoop::removeRegistration(int fd, uint64_t key, ExceptionSink* xsink) {
    auto it = fd_map.find(fd);
    if (it == fd_map.end()) {
        // Not an error - fd might have been closed and auto-removed
        return 0;
    }
    FdInfo& info = it->second;
    auto rit = info.regs.find(key);
    if (rit == info.regs.end()) {
        return 0;
    }
    void* removed_udata = rit->second.udata;
    info.regs.erase(rit);
    if (info.regs.empty()) {
        const bool fifo = info.fifo;
        fd_map.erase(it);
#ifdef DEBUG
        if (dbg_before_remove) {
            dbg_before_remove(fd);
        }
#endif
        if (fifo) {
            // a named pipe is not registered with kqueue
#ifdef DARWIN
            assert(fifo_count);
            --fifo_count;
#endif
            return 0;
        }
        return kernelRemove(fd, xsink);
    }

    // the other registrations stay: the descriptor is polled for their events
    if (info.udata == removed_udata) {
        info.udata = info.regs.begin()->second.udata;
    }
    int all = 0;
    for (const auto& [k, reg] : info.regs) {
        all |= reg.events;
    }
    if (all == info.events) {
        return 0;
    }
    info.events = all;
    // a named pipe is watched for info.events by poll()
    return info.fifo ? 0 : kernelModify(fd, all, info.udata, xsink);
}

bool QoreEventLoop::hasRegistration(int fd, uint64_t key) const {
    auto it = fd_map.find(fd);
    return it != fd_map.end() && it->second.regs.count(key);
}

void QoreEventLoop::forEachRegistration(int fd, const std::function<void(uint64_t key, int events)>& f) const {
    auto it = fd_map.find(fd);
    if (it == fd_map.end()) {
        return;
    }
    for (const auto& [key, reg] : it->second.regs) {
        f(key, reg.events);
    }
}

void QoreEventLoop::expandRegistrations(std::vector<QoreEventInfo>& events) {
    expand_buf.clear();
    for (const QoreEventInfo& e : events) {
        if (e.fd < 0) {
            // user events (EVFILT_USER) and io_uring completions are not descriptor registrations
            expand_buf.push_back(e);
            continue;
        }
        auto it = fd_map.find(e.fd);
        if (it == fd_map.end()) {
            // the descriptor was removed after the system reported it
            continue;
        }
        for (const auto& [key, reg] : it->second.regs) {
            int ev = e.events & (reg.events | QORE_EV_ERROR);
            if (!ev) {
                continue;
            }
            if (!(ev & (QORE_EV_READ | QORE_EV_WRITE))) {
                // an error is reported with the events the registration waits for, so its operation runs and sees it
                ev |= reg.events & (QORE_EV_READ | QORE_EV_WRITE);
            }
            expand_buf.emplace_back(e.fd, ev, reg.udata, 0, key);
        }
    }
    events.swap(expand_buf);
}

int QoreEventLoop::kernelAdd(int fd, int events, void* udata, ExceptionSink* xsink) {
#ifdef DARWIN
    // kqueue: register events using kevent()
    // We may need up to 2 events (read + write)
    // Use level-triggered mode (no EV_CLEAR) for reliable wakeups
    struct kevent changes[2];
    int nchanges = 0;

    if (events & QORE_EV_READ) {
        EV_SET(&changes[nchanges], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, udata);
        ++nchanges;
    }
    if (events & QORE_EV_WRITE) {
        EV_SET(&changes[nchanges], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, udata);
        ++nchanges;
    }

    if (nchanges > 0) {
        // Use zero timeout to just register (don't wait)
        struct timespec ts = {0, 0};
        int rc = kevent(event_fd, changes, nchanges, nullptr, 0, &ts);
        if (rc < 0) {
            xsink->raiseErrnoException("EVENT-LOOP-ERROR", errno, "kevent() failed to add fd %d", fd);
            return -1;
        }
    }
    return 0;

#elif defined(__linux__)
    // epoll: register using epoll_ctl()
    // Store fd in data.fd so we can look up udata from fd_map
    struct epoll_event ev;
    ev.events = 0;
    if (events & QORE_EV_READ) {
        ev.events |= EPOLLIN;
    }
    if (events & QORE_EV_WRITE) {
        ev.events |= EPOLLOUT;
    }
    ev.data.fd = fd;

    if (epoll_ctl(event_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        if (errno == EEXIST) {
            // fd is already registered (e.g., a predecessor operation's
            // unregister skipped the epoll_ctl(DEL) because the socket
            // appeared closed at that moment).  Fall back to MOD.
            if (epoll_ctl(event_fd, EPOLL_CTL_MOD, fd, &ev) < 0) {
                xsink->raiseErrnoException("EVENT-LOOP-ERROR", errno,
                    "epoll_ctl(MOD after ADD EEXIST) failed for fd %d", fd);
                return -1;
            }
            return 0;
        }
        xsink->raiseErrnoException("EVENT-LOOP-ERROR", errno, "epoll_ctl(ADD) failed for fd %d", fd);
        return -1;
    }
    return 0;

#else
    // poll-based implementation: just stored in fd_map, nothing else needed
    return 0;
#endif
}

int QoreEventLoop::kernelModify(int fd, int events, void* udata, ExceptionSink* xsink) {
#ifdef DARWIN
    // kqueue: need to remove old events and add new ones
    // Use level-triggered mode (no EV_CLEAR)
    //
    // IMPORTANT: EV_DELETE and EV_ADD must be done in separate kevent() calls.
    // When kevent() is called with no eventlist (nullptr, 0) and an EV_DELETE
    // fails (e.g., ENOENT for a filter that wasn't registered), kevent() returns
    // -1 and stops processing the changelist — any subsequent EV_ADD entries
    // would never be applied.
    struct kevent change;
    struct timespec ts = {0, 0};

    // Step 1: Delete both filters individually, ignoring errors (filter may not exist)
    EV_SET(&change, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    kevent(event_fd, &change, 1, nullptr, 0, &ts);
    EV_SET(&change, fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    kevent(event_fd, &change, 1, nullptr, 0, &ts);

    // Step 2: Add the desired filters
    struct kevent adds[2];
    int nadds = 0;
    if (events & QORE_EV_READ) {
        EV_SET(&adds[nadds++], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, udata);
    }
    if (events & QORE_EV_WRITE) {
        EV_SET(&adds[nadds++], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, udata);
    }
    if (nadds > 0) {
        if (kevent(event_fd, adds, nadds, nullptr, 0, &ts) < 0) {
            xsink->raiseErrnoException("EVENT-LOOP-ERROR", errno,
                "kevent() failed to modify events for fd %d", fd);
            return -1;
        }
    }
    return 0;

#elif defined(__linux__)
    // epoll: use EPOLL_CTL_MOD
    struct epoll_event ev;
    ev.events = 0;
    if (events & QORE_EV_READ) {
        ev.events |= EPOLLIN;
    }
    if (events & QORE_EV_WRITE) {
        ev.events |= EPOLLOUT;
    }
    ev.data.fd = fd;

    if (epoll_ctl(event_fd, EPOLL_CTL_MOD, fd, &ev) < 0) {
        if (errno == ENOENT) {
            // fd was auto-removed by epoll (e.g., socket closed and reopened with same fd number);
            // re-add it to the epoll set
            if (epoll_ctl(event_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
                xsink->raiseErrnoException("EVENT-LOOP-ERROR", errno,
                    "epoll_ctl(ADD after MOD ENOENT) failed for fd %d", fd);
                return -1;
            }
            return 0;
        }
        xsink->raiseErrnoException("EVENT-LOOP-ERROR", errno, "epoll_ctl(MOD) failed for fd %d", fd);
        return -1;
    }
    return 0;

#else
    // poll-based implementation: just update fd_map
    return 0;
#endif
}

int QoreEventLoop::kernelRemove(int fd, ExceptionSink* xsink) {
#ifdef DARWIN
    // kqueue: remove both filters (ignore errors if not registered)
    struct kevent changes[2];
    EV_SET(&changes[0], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    EV_SET(&changes[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);

    struct timespec ts = {0, 0};
    // Ignore errors - filters might not exist
    kevent(event_fd, changes, 2, nullptr, 0, &ts);
    return 0;

#elif defined(__linux__)
    // epoll: use EPOLL_CTL_DEL
    // Note: On Linux 2.6.9+, the event parameter can be NULL for DEL
    if (epoll_ctl(event_fd, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        // EBADF or ENOENT is ok - fd was already closed or not registered
        //
        // EPERM is the same "the fd we registered is gone" case seen once more: the socket was
        // closed and the kernel handed the same fd number to an object that epoll cannot poll (a
        // regular file, a directory), for which EPOLL_CTL_DEL reports EPERM rather than ENOENT.
        // Reporting it as an error leaves an exception pending in the I/O thread's sink, where it
        // is misreported against the next operation that checks the sink
        if (errno != EBADF && errno != ENOENT && errno != EPERM) {
            xsink->raiseErrnoException("EVENT-LOOP-ERROR", errno, "epoll_ctl(DEL) failed for fd %d", fd);
            return -1;
        }
    }
    return 0;

#else
    // poll-based implementation: just removed from fd_map
    return 0;
#endif
}

#ifdef DARWIN
int QoreEventLoop::addUserEvent(uintptr_t ident, void* udata, ExceptionSink* xsink) {
    // Store in user event map
    user_event_map[ident] = udata;
    // Note: the actual kevent registration is done by QoreEventNotifier::bindToKqueue()
    // This method just tracks the user event for poll() result handling
    return 0;
}

int QoreEventLoop::removeUserEvent(uintptr_t ident, ExceptionSink* xsink) {
    auto it = user_event_map.find(ident);
    if (it == user_event_map.end()) {
        return 0;  // Not registered, not an error
    }
    user_event_map.erase(it);

    // Remove from kqueue
    struct kevent ev;
    EV_SET(&ev, ident, EVFILT_USER, EV_DELETE, 0, 0, nullptr);

    struct timespec ts = {0, 0};
    // Ignore errors - the event may not exist or kqueue may be closing
    kevent(event_fd, &ev, 1, nullptr, 0, &ts);
    return 0;
}

int QoreEventLoop::waitWithFifos(int64_t wait_us, std::vector<struct kevent>& kevents,
        std::vector<QoreEventInfo>& fifo_events, ExceptionSink* xsink) {
    assert(fifo_count);
    // the kqueue descriptor, which is readable when kqueue has events, then the named pipes
    std::vector<struct pollfd> pfds;
    try {
        pfds.reserve(fifo_count + 1);
        fifo_events.reserve(fifo_count);
    } catch (std::bad_alloc&) {
        xsink->outOfMemory();
        return -1;
    }
    pfds.push_back({event_fd, POLLIN, 0});
    for (const auto& [fd, info] : fd_map) {
        if (!info.fifo) {
            continue;
        }
        short ev = 0;
        if (info.events & QORE_EV_READ) {
            ev |= POLLIN;
        }
        if (info.events & QORE_EV_WRITE) {
            ev |= POLLOUT;
        }
        if (ev) {
            // within the reserved capacity
            pfds.push_back({fd, ev, 0});
        }
    }

    // select(2) waits in microseconds, like kevent() in poll(); the monotonic clock gives the EINTR-retry deadline
    int64_t deadline_us = wait_us > 0 ? q_get_monotonic_us() + wait_us : 0;
    int64_t remaining_us = wait_us;
    int rc;
    while (true) {
        rc = qore_select_poll_us(pfds.data(), pfds.size(), remaining_us);
        if (rc >= 0 || errno != EINTR) {
            break;
        }
        if (wait_us > 0) {
            remaining_us = deadline_us - q_get_monotonic_us();
            if (remaining_us <= 0) {
                rc = 0;
                break;
            }
        }
    }
    if (rc < 0) {
        xsink->raiseErrnoException("EVENT-LOOP-ERROR", errno, "select() failed");
        return -1;
    }

    int nkevents = 0;
    if (pfds[0].revents & POLLIN) {
        struct timespec ts = {0, 0};
        while (true) {
            nkevents = kevent(event_fd, nullptr, 0, kevents.data(), kevents.size(), &ts);
            if (nkevents >= 0 || errno != EINTR) {
                break;
            }
        }
        if (nkevents < 0) {
            xsink->raiseErrnoException("EVENT-LOOP-ERROR", errno, "kevent() failed");
            return -1;
        }
    }

    for (size_t i = 1; i < pfds.size(); ++i) {
        short re = pfds[i].revents;
        if (!re) {
            continue;
        }
        int fd = pfds[i].fd;
        auto it = fd_map.find(fd);
        assert(it != fd_map.end() && it->second.fifo);
        if (it == fd_map.end()) {
            continue;
        }
        if (re & POLLNVAL) {
            // closed without being removed: kqueue drops a closed descriptor, and so does this
            fd_map.erase(it);
            --fifo_count;
            continue;
        }
        int ev = 0;
        if (re & POLLIN) {
            ev |= QORE_EV_READ;
            // readable with nothing to read: the writers have closed; epoll reports this with EPOLLHUP, and kqueue
            // with EV_EOF for an anonymous pipe
            int avail = 0;
            if (!ioctl(fd, FIONREAD, &avail) && !avail) {
                ev |= QORE_EV_ERROR;
            }
        }
        if (re & POLLOUT) {
            ev |= QORE_EV_WRITE;
        }
        // within the reserved capacity
        fifo_events.push_back({fd, ev, it->second.udata});
    }
    return nkevents;
}
#endif

int64_t QoreEventLoop::addTimer(int64_t deadline_us, void* udata, int64_t preset_id) {
    int64_t id = preset_id > 0 ? preset_id : next_timer_id++;
    if (id >= next_timer_id) {
        next_timer_id = id + 1;  // keep monotonic
    }
    TimerEntry entry{deadline_us, id, udata};
    auto [it, ok] = timer_set.insert(entry);
    if (ok) {
        timer_id_map[id] = it;
    }
    return id;
}

bool QoreEventLoop::cancelTimer(int64_t id) {
    auto it = timer_id_map.find(id);
    if (it == timer_id_map.end()) {
        return false;
    }
    timer_set.erase(it->second);
    timer_id_map.erase(it);
    return true;
}

int QoreEventLoop::poll(std::vector<QoreEventInfo>& events, int timeout_ms, ExceptionSink* xsink) {
    // The wait in microseconds, -1 for no limit, clamped to the earliest timer deadline.  It is kept in microseconds
    // and waited for with the resolution the system provides (kevent()'s timespec, epoll_pwait2(), ppoll()), and
    // rounded up where only whole milliseconds can be waited for: a timer due in less than a millisecond used to make
    // the wait 0, so the loop polled without blocking until the timer was due, busy-looping its thread.
    int64_t wait_us = timeout_ms < 0 ? -1 : static_cast<int64_t>(timeout_ms) * 1000;
    if (!timer_set.empty()) {
        int64_t remaining_us = timer_set.begin()->deadline_us - q_epoch_us_fast();
        if (remaining_us <= 0) {
            wait_us = 0;
        } else if (wait_us < 0 || remaining_us < wait_us) {
            wait_us = remaining_us;
        }
    }

    // Check if we have any fds to poll
    bool has_fds;
#ifdef DARWIN
    has_fds = !fd_map.empty() || !user_event_map.empty();
#elif defined(__linux__) && defined(HAVE_IO_URING)
    // io_uring eventfd is registered with epoll but not in fd_map
    has_fds = !fd_map.empty() || (io_uring_ != nullptr);
#else
    has_fds = !fd_map.empty();
#endif

    if (!has_fds && timer_set.empty()) {
        events.clear();
        return 0;
    }

    // Timer-only mode: sleep until the earliest timer is due (wait_us >= 0, as there is a timer)
    if (!has_fds) {
        if (wait_us > 0) {
#ifdef HAVE_NANOSLEEP
            struct timespec ts, rem;
            q_wait_us_to_timespec(wait_us, ts);
            while (nanosleep(&ts, &rem) && errno == EINTR) {
                ts = rem;
            }
#else
            ::poll(nullptr, 0, q_wait_us_to_ms(wait_us));
#endif
        }
        // Collect expired timers only
        events.clear();
        collectExpiredTimers(events);
        return static_cast<int>(events.size());
    }

#ifdef DARWIN
    // kqueue: use kevent() to wait for events
    // Allocate space for events:
    // - at most 2 per fd (read + write)
    // - plus 1 per user event (EVFILT_USER)
    std::vector<struct kevent> kevents(fd_map.size() * 2 + user_event_map.size());

    // the events of the named pipes, which are watched with select(2)
    std::vector<QoreEventInfo> fifo_events;
    int rc;
    if (fifo_count) {
        rc = waitWithFifos(wait_us, kevents, fifo_events, xsink);
        if (rc < 0) {
            return -1;
        }
    } else {
        struct timespec ts;
        struct timespec* pts = nullptr;
        int64_t deadline_us = 0;
        if (wait_us >= 0) {
            q_wait_us_to_timespec(wait_us, ts);
            pts = &ts;
            // Monotonic clock for the EINTR-retry deadline: this is a relative "wait at most N ms"
            // bound, so it must not be skewed by realtime clock adjustments.
            deadline_us = q_get_monotonic_us() + wait_us;
        }

        while (true) {
            rc = kevent(event_fd, nullptr, 0, kevents.data(), kevents.size(), pts);
            if (rc >= 0 || errno != EINTR) {
                break;
            }
            // Recompute remaining time after EINTR — without this, each retry
            // restarts the full timeout and repeated signals can prevent the
            // timeout from ever firing (root cause of stale-detect stalls)
            if (pts) {
                int64_t remaining_us = deadline_us - q_get_monotonic_us();
                if (remaining_us <= 0) {
                    rc = 0;
                    break;
                }
                q_wait_us_to_timespec(remaining_us, ts);
            }
        }

        if (rc < 0) {
            xsink->raiseErrnoException("EVENT-LOOP-ERROR", errno, "kevent() failed");
            return -1;
        }
    }

    // Convert kevent results to QoreEventInfo
    events.clear();
    events.reserve(rc + fifo_events.size());

    // Track which fds we've seen to combine read+write events
    std::unordered_map<int, size_t> fd_to_idx;

    for (int i = 0; i < rc; ++i) {
        uintptr_t ident = kevents[i].ident;

        int ev = 0;
        if (kevents[i].flags & EV_ERROR) {
            ev = QORE_EV_ERROR;
        } else if (kevents[i].filter == EVFILT_USER) {
            // User event from EventNotifier - treat as read event
            // Look up the associated QoreObject* from user_event_map
            auto uit = user_event_map.find(ident);
            if (uit != user_event_map.end()) {
                ev = QORE_EV_READ;
                // Use -1 as a special fd value for user events
                // The udata is the QoreObject* stored in addUserEvent()
                events.push_back({-1, ev, uit->second});
            }
            continue;
        } else if (kevents[i].filter == EVFILT_READ) {
            ev = QORE_EV_READ;
            if (kevents[i].flags & EV_EOF) {
                ev |= QORE_EV_ERROR;
            }
        } else if (kevents[i].filter == EVFILT_WRITE) {
            ev = QORE_EV_WRITE;
            if (kevents[i].flags & EV_EOF) {
                ev |= QORE_EV_ERROR;
            }
        }

        int fd = static_cast<int>(ident);

        // Look up udata from our fd_map instead of using the kevent's udata directly;
        // the kevent udata can become stale if the fd was removed between kevent()
        // returning and processing the results
        auto fd_it = fd_map.find(fd);
        if (fd_it == fd_map.end()) {
            // fd was removed after kevent() returned; skip stale event
            continue;
        }
        void* udata = fd_it->second.udata;

        auto it = fd_to_idx.find(fd);
        if (it != fd_to_idx.end()) {
            // Combine with existing entry
            events[it->second].events |= ev;
        } else {
            // New entry
            fd_to_idx[fd] = events.size();
            events.push_back({fd, ev, udata});
        }
    }
    // named pipes are not registered with kqueue, so their events are separate
    events.insert(events.end(), fifo_events.begin(), fifo_events.end());

    // each registration of a descriptor gets the events it waits for
    expandRegistrations(events);
    collectExpiredTimers(events);
    return static_cast<int>(events.size());

#elif defined(__linux__)
    // epoll: use epoll_wait()
    // +1 for the io_uring eventfd which is registered with epoll but not in fd_map
    size_t epoll_size = fd_map.size();
#if defined(HAVE_IO_URING)
    if (io_uring_) {
        ++epoll_size;
    }
#endif
    std::vector<struct epoll_event> epevents(epoll_size);

    int rc;
    int64_t epoll_wait_us = wait_us;
    int64_t epoll_deadline_us = 0;
    if (epoll_wait_us >= 0) {
        // Monotonic clock for the EINTR-retry deadline (see kevent path above).
        epoll_deadline_us = q_get_monotonic_us() + epoll_wait_us;
    }
    while (true) {
        rc = -2;
#ifdef QORE_HAVE_EPOLL_NS_WAIT
        // a limited wait with nanosecond resolution; epoll_wait() is the same for no limit and for no wait
        if (epoll_wait_us > 0) {
            struct timespec ts;
            q_wait_us_to_timespec(epoll_wait_us, ts);
            rc = q_epoll_pwait2(event_fd, epevents.data(), static_cast<int>(epevents.size()), &ts);
        }
#endif
        if (rc == -2) {
            rc = epoll_wait(event_fd, epevents.data(), epevents.size(),
                epoll_wait_us < 0 ? -1 : q_wait_us_to_ms(epoll_wait_us));
        }
        if (rc >= 0 || errno != EINTR) {
            break;
        }
        // Recompute remaining time after EINTR
        if (epoll_wait_us >= 0) {
            int64_t remaining_us = epoll_deadline_us - q_get_monotonic_us();
            if (remaining_us <= 0) {
                rc = 0;
                break;
            }
            epoll_wait_us = remaining_us;
        }
    }

    if (rc < 0) {
        xsink->raiseErrnoException("EVENT-LOOP-ERROR", errno, "epoll_wait() failed");
        return -1;
    }

    // Convert epoll results to QoreEventInfo
    events.clear();
    events.reserve(rc);

    for (int i = 0; i < rc; ++i) {
        int ev = 0;
        // Get the fd directly from epoll_event data
        int fd = epevents[i].data.fd;

#if defined(HAVE_IO_URING)
        // Check if this is the io_uring eventfd
        if (io_uring_ && fd == io_uring_->getEventFd()) {
            events.push_back({QORE_IOURING_FD, QORE_EV_IOURING, nullptr});
            continue;
        }
#endif

        // Map epoll events to QORE_EV flags.  EPOLLERR and EPOLLHUP can arrive
        // alongside EPOLLIN/EPOLLOUT (e.g. peer FIN sets EPOLLIN|EPOLLHUP).
        // Always include QORE_EV_READ/WRITE so the operation gets continuePoll()
        // called and can detect EOF via recv()=0, instead of silently dropping
        // the read-readiness event and causing a timeout.
        if (epevents[i].events & EPOLLIN) {
            ev |= QORE_EV_READ;
        }
        if (epevents[i].events & EPOLLOUT) {
            ev |= QORE_EV_WRITE;
        }
        if (epevents[i].events & (EPOLLERR | EPOLLHUP)) {
            ev |= QORE_EV_ERROR;
            // If only HUP/ERR without POLLIN/POLLOUT, ensure at least READ so
            // the operation's continuePoll is invoked and can detect the error
            if (!(ev & (QORE_EV_READ | QORE_EV_WRITE))) {
                ev |= QORE_EV_READ;
            }
        }

        // Look up udata from our fd_map
        auto it = fd_map.find(fd);
        void* udata = (it != fd_map.end()) ? it->second.udata : nullptr;

        events.push_back({fd, ev, udata});
    }

    // each registration of a descriptor gets the events it waits for
    expandRegistrations(events);
    collectExpiredTimers(events);
    return static_cast<int>(events.size());

#else
    // poll-based fallback implementation
    std::vector<struct pollfd> pollfds;
    pollfds.reserve(fd_map.size());

    for (const auto& [fd, info] : fd_map) {
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = 0;
        if (info.events & QORE_EV_READ) {
            pfd.events |= POLLIN;
        }
        if (info.events & QORE_EV_WRITE) {
            pfd.events |= POLLOUT;
        }
        pfd.revents = 0;
        pollfds.push_back(pfd);
    }

    int rc;
    int64_t poll_wait_us = wait_us;
    int64_t poll_deadline_us = 0;
    if (poll_wait_us >= 0) {
        // Monotonic clock for the EINTR-retry deadline (see kevent/epoll paths above).
        poll_deadline_us = q_get_monotonic_us() + poll_wait_us;
    }
    while (true) {
#ifdef HAVE_PPOLL
        struct timespec ts;
        if (poll_wait_us >= 0) {
            q_wait_us_to_timespec(poll_wait_us, ts);
        }
        rc = ::ppoll(pollfds.data(), pollfds.size(), poll_wait_us < 0 ? nullptr : &ts, nullptr);
#else
        rc = ::poll(pollfds.data(), pollfds.size(), poll_wait_us < 0 ? -1 : q_wait_us_to_ms(poll_wait_us));
#endif
        if (rc >= 0 || errno != EINTR) {
            break;
        }
        // Recompute remaining time after EINTR
        if (poll_wait_us >= 0) {
            int64_t remaining_us = poll_deadline_us - q_get_monotonic_us();
            if (remaining_us <= 0) {
                rc = 0;
                break;
            }
            poll_wait_us = remaining_us;
        }
    }

    if (rc < 0) {
        xsink->raiseErrnoException("EVENT-LOOP-ERROR", errno, "poll() failed");
        return -1;
    }

    // Convert poll results to QoreEventInfo
    events.clear();
    if (rc > 0) {
        events.reserve(rc);

        size_t idx = 0;
        for (const auto& [fd, info] : fd_map) {
            if (pollfds[idx].revents) {
                int ev = 0;
                if (pollfds[idx].revents & POLLIN) {
                    ev |= QORE_EV_READ;
                }
                if (pollfds[idx].revents & POLLOUT) {
                    ev |= QORE_EV_WRITE;
                }
                if (pollfds[idx].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    ev |= QORE_EV_ERROR;
                    if (!(ev & (QORE_EV_READ | QORE_EV_WRITE))) {
                        ev |= QORE_EV_READ;
                    }
                }
                events.push_back({fd, ev, info.udata});
            }
            ++idx;
        }
    }

    // each registration of a descriptor gets the events it waits for
    expandRegistrations(events);
    collectExpiredTimers(events);
    return static_cast<int>(events.size());
#endif
}

void QoreEventLoop::collectExpiredTimers(std::vector<QoreEventInfo>& events) {
    if (!timer_set.empty()) {
        int64_t now_us = q_epoch_us_fast();
        while (!timer_set.empty() && timer_set.begin()->deadline_us <= now_us) {
            auto it = timer_set.begin();
            events.push_back({QORE_TIMER_FD, QORE_EV_TIMER, it->udata, it->id});
            timer_id_map.erase(it->id);
            timer_set.erase(it);
        }
    }
}
