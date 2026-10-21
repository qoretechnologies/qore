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
#elif defined(DARWIN)
#include <sys/event.h>
#endif

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>

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
    - notify() is thread-safe and lock-free and can be called from any thread
    - acknowledge() should only be called from the thread that owns the notifier
    - fd() is thread-safe; it returns -1 once closeFd() has closed the descriptor
    - closeFd() is thread-safe: it waits until no notify() or acknowledge() call is using the descriptor before it
      closes it, so no call can use a descriptor that is closed and possibly reused by the system; after it,
      notify() and acknowledge() do nothing.  It must not be called from a notify() or acknowledge() call on the
      same thread.
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

    //! Returns true if the notifier was created successfully and is not closed
    DLLLOCAL bool isValid() const {
#ifdef __linux__
        return notify_fd.load(std::memory_order_acquire) >= 0;
#else
        return pipe_fd[0].load(std::memory_order_acquire) >= 0 && pipe_fd[1].load(std::memory_order_acquire) >= 0;
#endif
    }

    //! Returns the file descriptor to register with EventLoop
    /** This fd should be registered for QORE_EV_READ events.

        @return the notification fd, or -1 if not valid or closed
    */
    DLLLOCAL int fd() const {
#ifdef __linux__
        return notify_fd.load(std::memory_order_acquire);
#else
        return pipe_fd[0].load(std::memory_order_acquire);  // read end
#endif
    }

    //! Implements AbstractPollableIoObjectBase::getPollableDescriptor()
    DLLLOCAL int getPollableDescriptor() const override {
        return fd();
    }

    //! Keeps the descriptor open while the async I/O controller has it registered in an event loop
    /** closeFd() releases the notifier's own reference to the descriptor; the descriptor is closed when the last
        hold is released too

        @see AbstractPollableIoObjectBase::holdPollableDescriptor()
    */
    DLLLOCAL int holdPollableDescriptor(int fd, std::shared_ptr<void>& hold) const override;

    //! No-op for event notifiers (they don't represent closeable connections)
    DLLLOCAL void closeIo(ExceptionSink* /* xsink */) override {}

    //! Deterministically closes the underlying fd/pipe
    /** Called from the Qore-level destructor to release the OS resource
        immediately, even if C++ code (e.g., EventNotifierAction, or a connection that the notifier is registered
        with to be signaled when it becomes ready) still holds a reference to this object.  Waits until no
        notify() or acknowledge() call is using the descriptor, then releases it; subsequent notify() and
        acknowledge() calls do nothing and fd() returns -1.  Idempotent.

        The descriptor that the notifier is polled with stays open while an async I/O event loop has it registered
        (see holdPollableDescriptor()): the object can be destroyed while an operation waits on it, and the number of
        a closed descriptor can be given to another connection.
    */
    DLLLOCAL void closeFd();

    //! Signals the notifier (thread-safe)
    /** This method can be called from any thread to wake up the event loop.
        Multiple calls to notify() before acknowledge() may be coalesced
        (on Linux with eventfd) or queued (on other platforms with pipe).
        Does nothing once the notifier is closed.
    */
    DLLLOCAL void notify();

    //! Acknowledges/consumes the notification
    /** This method should be called after the event loop returns with an
        event on this notifier's fd. It drains any pending notifications.
        Does nothing once the notifier is closed.

        @param xsink exception sink for error reporting
    */
    DLLLOCAL void acknowledge(ExceptionSink* xsink);

#ifdef DEBUG
    //! Debug builds: called by notify() and acknowledge() while they use the descriptor
    /** Lets a test hold a call in the window in which the descriptor is in use
    */
    DLLLOCAL static std::atomic<void (*)(QoreEventNotifier* notifier)> dbg_in_use_hook;

    //! Debug builds: called by closeFd() when it waits for calls that use the descriptor
    DLLLOCAL static std::atomic<void (*)(QoreEventNotifier* notifier)> dbg_close_wait_hook;
#endif

#ifdef DARWIN
    //! Binds to a kqueue for optimized EVFILT_USER notification (macOS only)
    /** When bound, notify() uses the efficient EVFILT_USER mechanism instead
        of a pipe. This should be called before registering with an EventLoop.

        @param kqueue_fd the kqueue file descriptor to bind to
        @param xsink exception sink for error reporting

        @return 0 on success, -1 on error (exception raised)
    */
    DLLLOCAL int bindToKqueue(int kqueue_fd, ExceptionSink* xsink);

    //! Returns true if using optimized kqueue-based notification
    DLLLOCAL bool isOptimized() const {
        return optimized.load(std::memory_order_acquire);
    }

    //! Returns the unique EVFILT_USER identifier
    DLLLOCAL uintptr_t getUserIdent() const {
        return user_ident;
    }

    //! Unbinds from kqueue (called when removing from EventLoop)
    DLLLOCAL void unbindFromKqueue();
#endif

private:
    //! Set in @ref use_state by closeFd(); never cleared
    static constexpr uint32_t CLOSED_BIT = 0x80000000u;
    //! The number of notify() and acknowledge() calls using the descriptor, in @ref use_state
    static constexpr uint32_t IN_USE_MASK = 0x7fffffffu;

    //! The closed bit and the number of calls using the descriptor
    /** A call that uses the descriptor counts itself in before it reads the descriptor and out after its system
        call, and does nothing if the closed bit is set; closeFd() sets the bit and closes the descriptor only when
        the count is zero, so no call can use a descriptor that was closed and possibly reused by the system.
    */
    std::atomic<uint32_t> use_state{0};

    //! Guards the wait of closeFd() for the calls using the descriptor; a leaf lock
    std::mutex close_mutex;
    //! Signaled when the last call using the descriptor ends after the closed bit was set
    std::condition_variable close_cond;

    //! Guards @ref poll_fd_ref and the release of the polled descriptor
    mutable std::mutex poll_fd_lock;
    //! The notifier's reference to the descriptor it is polled with, which the last reference closes
    /** Async I/O event loops take further references while they have the descriptor registered (see
        holdPollableDescriptor()); guarded by @ref poll_fd_lock
    */
    std::shared_ptr<void> poll_fd_ref;

    //! Creates @ref poll_fd_ref for the descriptor the notifier is polled with
    DLLLOCAL void initPollFdRef(int fd);

#ifdef __linux__
    std::atomic<int> notify_fd{-1};     //!< eventfd file descriptor
#elif defined(DARWIN)
    std::atomic<int> kq_fd{-1};        //!< bound kqueue fd (when optimized)
    uintptr_t user_ident = 0;          //!< unique EVFILT_USER identifier
    std::atomic<bool> optimized{false}; //!< true when bound to kqueue
    std::atomic<int> pipe_fd[2] = {-1, -1};  //!< fallback pipe [read, write]

    //! Global counter for generating unique EVFILT_USER identifiers
    static std::atomic<uintptr_t> next_ident;
#else
    std::atomic<int> pipe_fd[2] = {-1, -1};  //!< pipe file descriptors [read, write]
#endif

    //! Counts a call in as using the descriptor; returns false (and counts nothing) if the notifier is closed
    DLLLOCAL bool beginUse();

    //! Counts a call out after it used the descriptor; wakes closeFd() if it waits for the last one
    DLLLOCAL void endUse();

    //! Counts a call using the descriptor in and out
    class UseHelper {
    public:
        DLLLOCAL UseHelper(QoreEventNotifier& n) : n(n), in_use(n.beginUse()) {
        }

        DLLLOCAL ~UseHelper() {
            if (in_use) {
                n.endUse();
            }
        }

        //! Returns true if the call may use the descriptor
        DLLLOCAL operator bool() const {
            return in_use;
        }

        UseHelper(const UseHelper&) = delete;
        UseHelper& operator=(const UseHelper&) = delete;

    private:
        QoreEventNotifier& n;
        bool in_use;
    };

    // Non-copyable
    QoreEventNotifier(const QoreEventNotifier&) = delete;
    QoreEventNotifier& operator=(const QoreEventNotifier&) = delete;
};

#endif // _QORE_QORE_EVENT_NOTIFIER_H
