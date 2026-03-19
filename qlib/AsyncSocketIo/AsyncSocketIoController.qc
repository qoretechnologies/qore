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
#! I/O thread commands
public enum IoCommand : string {
    #! Add a new operation
    Add = "add",
    #! Cancel a specific operation
    Cancel = "cancel",
    #! Cancel all operations for an owner
    CancelOwner = "cancel_owner",
    #! Quit the I/O thread
    Quit = "quit",
    #! Wake the I/O thread to force re-poll
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

    @since AsyncSocketIo 1.0

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

    #! Creates the object
    constructor(bool autostop, *LoggerInterface logger) : LoggerWrapper(logger) {
        Qore::AsyncIoController::setAutostop(autostop);
        if (logger) {
            Qore::AsyncIoController::setLogger(logger);
        }
    }

    #! Creates the object
    constructor(*LoggerInterface logger) : LoggerWrapper(logger) {
        if (logger) {
            Qore::AsyncIoController::setLogger(logger);
        }
    }

    #! Returns information about the object
    hash<auto> getInfo() {
        return Qore::AsyncIoController::getInfo();
    }

    #! Returns the number of operations in the cache (useful for testing cleanup)
    int getCacheSize() {
        return Qore::AsyncIoController::getCacheSize();
    }

    #! Set autostop flag
    setAutostop(bool autostop = True) {
        Qore::AsyncIoController::setAutostop(autostop);
    }

    #! Get autostop flag
    bool getAutostop() {
        return Qore::AsyncIoController::getAutostop();
    }

    #! No-op — the I/O thread is automatically started on first submit
    start() {
    }

    #! No-op — stopping the global singleton is unsafe; use cancelByOwner() instead
    stop() {
    }

    #! No-op — stopping the global singleton is unsafe; use cancelByOwner() instead
    stopClear() {
    }

    #! No-op
    waitStop() {
    }

    #! Returns @ref True if the I/O thread is running
    bool running() {
        return Qore::AsyncIoController::running();
    }

    #! Waits for the I/O thread to be ready
    bool waitReady(*timeout to) {
        return Qore::AsyncIoController::waitReady(to);
    }

    #! Waits until the I/O thread has processed all currently submitted operations
    bool waitForProcessing(*timeout to) {
        return Qore::AsyncIoController::waitForProcessing(to);
    }

    #! Submits a socket operation
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

    #! Executes a socket operation synchronously
    hash<SocketPollResultInfo> exec(hash<SocketPollOperationInfo> info, *bool replace) {
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
        return Qore::AsyncIoController::exec(info, replace);
    }

    #! Cancels the operation with the given Socket's unique hash as the key
    bool cancel(Socket sock) {
        return Qore::AsyncIoController::cancel(sock);
    }

    #! Wakes the I/O thread to re-evaluate poll state
    wake() {
        Qore::AsyncIoController::wake();
    }

    #! Cancels any operation in progress for the given Socket; throws if not found
    bool cancelEx(Socket sock) {
        if (!cancel(sock)) {
            throw "CONTROLLER-CANCEL-ERROR", sprintf("Socket %y has no operation in progress",
                sock.uniqueHash());
        }
        return True;
    }

    #! Cancels the operation with the given custom key
    bool cancelByKey(string key) {
        return Qore::AsyncIoController::cancelByKey(key);
    }

    #! Cancels all operations belonging to a specific owner
    int cancelByOwner(string owner) {
        return Qore::AsyncIoController::cancelByOwner(owner);
    }

    #! Sets the logger for the controller
    setLogger(*LoggerInterface logger) {
        LoggerWrapper::setLogger(logger);
        Qore::AsyncIoController::setLogger(logger);
    }

    #! Submits a task to the controller's thread pool
    submitTask(code<nothing()> task, *code<nothing()> cancel) {
        Qore::AsyncIoController::submitTask(task, cancel);
    }

}

#! Returns the global async I/O controller singleton
/** The controller is created during module initialization with \c autostop=True.

    All async I/O components should use this singleton rather than creating their
    own controller instances. This single-controller architecture eliminates
    cross-controller deadlocks.

    @return the process-wide AsyncSocketIoController singleton

    @see AsyncSocketIoController
*/
public AsyncSocketIoController sub getGlobalAsyncIoController() {
    return globalController;
}

our *AsyncSocketIoController globalController;
}
