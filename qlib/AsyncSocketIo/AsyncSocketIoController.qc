# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore AsyncSocketIoController class definition

/*  AsyncSocketIoController.qc Copyright 2016 - 2026 Qore Technologies, s.r.o.

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
*/

#! The AsyncSocketIo namespace
/** All public declarations in the %AsyncSocketIo module are defined in this namespace
*/
public namespace AsyncSocketIo {
#! I/O thread commands used internally by the async socket I/O controller
/** These commands are sent to the I/O thread via an internal command queue to control
    its behavior. They are not intended for direct use by application code.
*/
public enum IoCommand : string {
    #! Add a new operation to the I/O thread's poll set
    Add = "add",
    #! Cancel a specific operation identified by socket hash
    Cancel = "cancel",
    #! Cancel all operations belonging to a specific owner
    CancelOwner = "cancel_owner",
    #! Shut down the I/O thread
    Quit = "quit",
    #! Wake the I/O thread to force an immediate re-poll cycle
    Wake = "wake",
}

#! Class for controlling async socket I/O
/** Thin wrapper around the C++ @ref Qore::AsyncIoController class.

    This class delegates all I/O operations to the native C++ implementation while maintaining
    the same public API for backward compatibility.

    @par Single-Controller Architecture
    The process should use a single global controller instance, obtained via
    @ref getGlobalAsyncIoController(). This avoids cross-controller deadlocks that
    can occur when object destructors fire on one controller's I/O thread and cancel
    operations on a different controller. The global controller is created during
    module initialization with \c autostop=True.

    All higher-level components (\c HttpServerAsyncIo::HttpAsyncSocketIoController,
    \c HttpClientIo::HttpClientConnectionManager) use the global controller by
    default.


    @see Qore::AsyncIoController
    @see getGlobalAsyncIoController()
*/
public class AsyncSocketIoController inherits LoggerWrapper {
    public {
        #! Polling event map
        const PollEventMap = {
            SOCK_POLLIN: "read",
            SOCK_POLLOUT: "write",
            SOCK_POLLIN | SOCK_POLLOUT: "read and write",
        };

        #! Default I/O timeout duration
        const DefaultIoOperationTimeout = 30s;
    }

    #! Creates the object with an explicit autostop flag
    /** @param autostop if @ref True "True", the I/O thread will automatically stop when all
        operations complete (after a grace period); if @ref False "False", the I/O thread runs
        until explicitly shut down
        @param logger an optional logger for diagnostic output
    */
    constructor(bool autostop, *LoggerInterface logger) : LoggerWrapper(logger) {
        Qore::AsyncIoController::setAutostop(autostop);
        if (logger) {
            Qore::AsyncIoController::setLogger(logger);
        }
    }

    #! Creates the object with autostop disabled by default
    /** @param logger an optional logger for diagnostic output
    */
    constructor(*LoggerInterface logger) : LoggerWrapper(logger) {
        if (logger) {
            Qore::AsyncIoController::setLogger(logger);
        }
    }

    #! Returns information about the controller state
    /** @return a hash with controller state information including thread status, cache size,
        and configuration details
    */
    hash<auto> getInfo() {
        return Qore::AsyncIoController::getInfo();
    }

    #! Returns the number of operations currently in the cache
    /** Useful for testing and verifying that operations have been properly cleaned up.

        @return the number of active poll operations in the controller's cache
    */
    int getCacheSize() {
        return Qore::AsyncIoController::getCacheSize();
    }

    #! Sets the autostop flag
    /** @param autostop if @ref True "True", the I/O thread will automatically stop when all
        operations complete (after a 2-second grace period to avoid unnecessary stop/restart cycles)
    */
    setAutostop(bool autostop = True) {
        Qore::AsyncIoController::setAutostop(autostop);
    }

    #! Returns the autostop flag
    /** @return @ref True "True" if the I/O thread will automatically stop when all operations
        complete
    */
    bool getAutostop() {
        return Qore::AsyncIoController::getAutostop();
    }

    #! No-op -- the I/O thread is automatically started on first submit
    start() {
    }

    #! No-op -- stopping the global singleton is unsafe; use cancelByOwner() instead
    stop() {
    }

    #! No-op -- stopping the global singleton is unsafe; use cancelByOwner() instead
    stopClear() {
    }

    #! No-op -- provided for backward compatibility
    waitStop() {
    }

    #! Returns @ref True "True" if the I/O thread is running
    /** @return @ref True "True" if at least one I/O thread is currently active
    */
    bool running() {
        return Qore::AsyncIoController::running();
    }

    #! Waits for the I/O thread to be ready to accept operations
    /** @param to an optional timeout; if not given, waits indefinitely
        @return @ref True "True" if the I/O thread became ready within the timeout
    */
    bool waitReady(*timeout to) {
        return Qore::AsyncIoController::waitReady(to);
    }

    #! Waits until the I/O thread has processed all currently submitted operations
    /** @param to an optional timeout; if not given, waits indefinitely
        @return @ref True "True" if all operations were processed within the timeout
    */
    bool waitForProcessing(*timeout to) {
        return Qore::AsyncIoController::waitForProcessing(to);
    }

    #! Submits a socket poll operation to the I/O controller
    /** @param info a hash describing the socket, poll operation, owner, and optional key/goal
        @param replace if @ref True "True", replaces any existing operation for the same socket

        @return a @ref Qore::Thread::Queue "Queue" if the operation has a goal, otherwise @ref nothing

        @throw CONTROLLER-ADD-ERROR missing required \c sock, \c spop, or \c owner field in \a info
    */
    *Queue submit(hash<SocketPollOperationInfo> info, *bool replace) {
        if (!info.sock) {
            throw "CONTROLLER-ADD-ERROR", sprintf("Missing 'sock' value; got: %y", info);
        }
        if (!info.spop) {
            throw "CONTROLLER-ADD-ERROR", sprintf("Missing 'spop' value; got: %y", info);
        }
        if (!info.owner) {
            throw "CONTROLLER-ADD-ERROR", sprintf("Missing 'owner' value; required for proper shutdown via "
                "cancelByOwner(); got: %y", info);
        }
        return Qore::AsyncIoController::submit(info, replace);
    }

    #! Waits for an @ref Qore::EventNotifier "EventNotifier" to be signaled via the I/O thread
    /** Async replacement for the pattern @c Socket::poll((<SocketPollInfo>{events:
        SOCK_POLLIN, socket: notifier},), timeout), which blocked the caller thread on a
        raw @ref Qore::Socket::poll() "Socket::poll()" syscall.  This method submits a
        @ref NotifierPollOperation to the I/O controller and blocks on the returned
        @ref Qore::Thread::Queue "Queue" — the I/O thread does the fd wait, and the
        caller is woken via CV when the notifier becomes readable or the timeout
        elapses.

        On success the caller should invoke @ref Qore::EventNotifier::acknowledge()
        "acknowledge()" to drain the notifier's fd, matching the @c Socket::poll
        pattern the caller is replacing.

        @param notifier the notifier to wait on
        @param to the maximum time to wait; a negative value means wait indefinitely

        @return @ref True if the notifier was signaled within the timeout, @ref False
        if the wait timed out

        @since AsyncSocketIo 1.3
    */
    bool waitForNotifier(EventNotifier notifier, timeout to = 30s) {
        NotifierPollOperation op(notifier);
        Queue q = submit(<SocketPollOperationInfo>{
            "sock": notifier,
            "spop": op,
            "owner": sprintf("notifier-wait-%d-%d", gettid(), clock_getmicros()),
            "to": to,
        });
        # Pull the completion result (the controller pushes it on goal-reached,
        # timeout, cancellation, or error).  Use Queue::get() with no timeout
        # here — the controller is the authority on timing; an additional
        # Queue-level timeout would only race with it.
        q.get();
        return op.goalReached();
    }

    #! Cancels the operation with the given Socket's unique hash as the key
    /** @param sock the socket whose operation should be canceled
        @return @ref True "True" if an operation was found and canceled
    */
    bool cancel(Socket sock) {
        return Qore::AsyncIoController::cancel(sock);
    }

    #! Cancels the operation and closes the socket
    /** Use this when the controller manages the socket lifecycle -- cancels the poll
        operation, then shuts down and closes the socket in C++ (no Qore round-trip).

        @param sock the socket whose operation should be canceled and then closed
        @return @ref True "True" if an operation was found and canceled

        @since Qore 2.3
    */
    bool cancelAndClose(Socket sock) {
        return Qore::AsyncIoController::cancelAndClose(sock);
    }

    #! Wakes the I/O thread for a specific socket with pending data
    /** @param sock the I/O object whose operation needs re-polling
    */
    wakeSocket(AbstractPollableIoObjectBase sock) {
        Qore::AsyncIoController::wakeSocket(sock);
    }

    #! Cancels any operation in progress for the given Socket; throws if not found
    /** @param sock the socket whose operation should be canceled

        @return @ref True "True" (always, since an exception is thrown if no operation is found)

        @throw CONTROLLER-CANCEL-ERROR no operation is in progress for the given socket
    */
    bool cancelEx(Socket sock) {
        if (!cancel(sock)) {
            throw "CONTROLLER-CANCEL-ERROR", sprintf("Socket %y has no operation in progress",
                sock.uniqueHash());
        }
        return True;
    }

    #! Cancels the operation with the given custom key
    /** @param key the custom key assigned to the operation via \c SocketPollOperationInfo.key
        @return @ref True "True" if an operation was found and canceled
    */
    bool cancelByKey(string key) {
        return Qore::AsyncIoController::cancelByKey(key);
    }

    #! Cancels all operations belonging to a specific owner
    /** @param owner the owner identifier used when the operations were submitted
        @return the number of operations that were canceled
    */
    int cancelByOwner(string owner) {
        return Qore::AsyncIoController::cancelByOwner(owner);
    }

    #! Waits for all pending onComplete/abort callbacks to be processed
    /** Call after cancelByOwner() to ensure all cancelled operation callbacks have been
        delivered before stopping thread pools or destroying resources that the callbacks
        may reference.
    */
    flushCallbacks() {
        Qore::AsyncIoController::flushCallbacks();
    }

    #! Waits for in-flight callbacks tagged with @a owner to drain
    /** Narrowed-scope companion to @ref flushCallbacks() that only waits for callbacks
        whose operations were submitted with the matching @a owner.  Safe to call from
        a per-owner teardown path (e.g. a connection manager's destructor) where the
        global flush would deadlock on unrelated long-running work on the shared
        controller.

        Marks the owner as shutting down for the duration of the wait: any callback
        picked up by a worker while the mark is set is silently discarded.  The mark
        is cleared before return so later submits by a new object reusing the owner
        string are unaffected.

        @param owner the owner identifier to wait for
    */
    flushCallbacksByOwner(string owner) {
        Qore::AsyncIoController::flushCallbacksByOwner(owner);
    }

    #! Sets the logger for the controller
    /** @param logger the logger to use for diagnostic output, or @ref nothing to disable logging
    */
    setLogger(*LoggerInterface logger) {
        LoggerWrapper::setLogger(logger);
        Qore::AsyncIoController::setLogger(logger);
    }

    #! Submits a task to the controller's callback thread pool for execution
    /** This is a convenience method for running arbitrary code on the thread pool without
        associating it with a socket poll operation.

        @param task the closure or call reference to execute on a thread pool worker
        @param cancel an optional closure or call reference to call if the task is canceled
        (e.g., during controller shutdown)
    */
    submitTask(code<nothing()> task, *code<nothing()> cancel) {
        Qore::AsyncIoController::submitTask(task, cancel);
    }

    #! Adds a timer that fires at the given deadline
    /** When the deadline is reached, the runtime automatically dispatches \a callback to
        a worker thread in the async-I/O controller's dispatcher pool — the closure is
        @b not invoked on the I/O thread, so it is free to block, acquire locks, or perform
        Qore-level I/O.

        For non-callable \c udata (numbers, strings, hashes, …) see
        @ref Qore::AsyncIoController::setTimerCallback() "AsyncIoController::setTimerCallback()";
        when installed, that single callback receives a @ref Qore::TimerEventInfo
        "hash<TimerEventInfo>" for each fire.  With no callback installed, non-callable
        \c udata is silently dropped.

        @param deadline the absolute time when the timer should fire
        @param callback the closure to call when the timer fires

        @return the unique timer ID that can be passed to cancelTimer() to cancel the timer
    */
    int addTimer(date deadline, code<nothing()> callback) {
        return Qore::AsyncIoController::addTimer(deadline, callback);
    }

    #! Cancels a pending timer
    /** @param timer_id the timer ID returned by addTimer()
        @return @ref True "True" if the timer was found and canceled; @ref False "False" if
        the timer had already fired or was not found
    */
    bool cancelTimer(int timer_id) {
        return Qore::AsyncIoController::cancelTimer(timer_id);
    }

}

#! Returns the global async I/O controller singleton
/** The Qore wrapper is created lazily on first access; the underlying C++ singleton
    is created and managed by libqore with \c autostop=True.

    All async I/O components should use this singleton rather than creating their
    own controller instances. This single-controller architecture eliminates
    cross-controller deadlocks.

    @return the process-wide AsyncSocketIoController singleton

    @see AsyncSocketIoController
*/
public synchronized AsyncSocketIoController sub getGlobalAsyncIoController() {
    return globalController ?? (globalController = new AsyncSocketIoController(True));
}

our *AsyncSocketIoController globalController;
}
