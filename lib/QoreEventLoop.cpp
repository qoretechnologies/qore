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

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <poll.h>

//! Returns the current time in microseconds since the epoch
static int64_t q_epoch_us_fast() {
    int us;
    int64_t secs = q_epoch_us(us);
    return secs * 1000000LL + us;
}

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
    // Store in our map
    fd_map[fd] = {events, udata};

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
            fd_map.erase(fd);
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
                fd_map.erase(fd);
                xsink->raiseErrnoException("EVENT-LOOP-ERROR", errno,
                    "epoll_ctl(MOD after ADD EEXIST) failed for fd %d", fd);
                return -1;
            }
            return 0;
        }
        fd_map.erase(fd);
        xsink->raiseErrnoException("EVENT-LOOP-ERROR", errno, "epoll_ctl(ADD) failed for fd %d", fd);
        return -1;
    }
    return 0;

#else
    // poll-based implementation: just stored in fd_map, nothing else needed
    return 0;
#endif
}

int QoreEventLoop::modify(int fd, int events, ExceptionSink* xsink) {
    auto it = fd_map.find(fd);
    if (it == fd_map.end()) {
        xsink->raiseException("EVENT-LOOP-ERROR", "fd %d is not registered", fd);
        return -1;
    }

    void* udata = it->second.udata;
    it->second.events = events;

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

int QoreEventLoop::remove(int fd, ExceptionSink* xsink) {
    auto it = fd_map.find(fd);
    if (it == fd_map.end()) {
        // Not an error - fd might have been closed and auto-removed
        return 0;
    }
    fd_map.erase(it);

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
        if (errno != EBADF && errno != ENOENT) {
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
    // Clamp timeout based on earliest timer deadline
    int effective_timeout_ms = timeout_ms;
    if (!timer_set.empty()) {
        int64_t now_us = q_epoch_us_fast();
        int64_t remaining_us = timer_set.begin()->deadline_us - now_us;
        if (remaining_us <= 0) {
            effective_timeout_ms = 0;
        } else {
            int timer_ms = (int)(remaining_us / 1000);
            // Sub-millisecond deadline: poll immediately (truncation to 0
            // is correct — don't oversleep by rounding up to 1ms)
            if (effective_timeout_ms < 0 || timer_ms < effective_timeout_ms) {
                effective_timeout_ms = timer_ms;
            }
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

    // Timer-only mode: sleep using portable poll(nullptr, 0, timeout)
    if (!has_fds) {
        if (effective_timeout_ms > 0) {
            ::poll(nullptr, 0, effective_timeout_ms);
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

    struct timespec ts;
    struct timespec* pts = nullptr;
    if (effective_timeout_ms >= 0) {
        ts.tv_sec = effective_timeout_ms / 1000;
        ts.tv_nsec = (effective_timeout_ms % 1000) * 1000000;
        pts = &ts;
    }

    int rc;
    while (true) {
        rc = kevent(event_fd, nullptr, 0, kevents.data(), kevents.size(), pts);
        if (rc >= 0 || errno != EINTR) {
            break;
        }
    }

    if (rc < 0) {
        xsink->raiseErrnoException("EVENT-LOOP-ERROR", errno, "kevent() failed");
        return -1;
    }

    // Convert kevent results to QoreEventInfo
    events.clear();
    events.reserve(rc);

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
    while (true) {
        rc = epoll_wait(event_fd, epevents.data(), epevents.size(), effective_timeout_ms);
        if (rc >= 0 || errno != EINTR) {
            break;
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
    while (true) {
        rc = ::poll(pollfds.data(), pollfds.size(), effective_timeout_ms);
        if (rc >= 0 || errno != EINTR) {
            break;
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
