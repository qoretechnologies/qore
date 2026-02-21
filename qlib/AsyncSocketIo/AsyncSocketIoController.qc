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

    @since AsyncSocketIo 3.0 (thin wrapper; previously a ~1400-line Qore implementation)

    @see Qore::AsyncIoController
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

    private {
        #! C++ async I/O controller
        Qore::AsyncIoController ctrl;
    }

    #! Creates the object
    constructor(bool autostop, *LoggerInterface logger) : LoggerWrapper(logger) {
        ctrl = new Qore::AsyncIoController(autostop);
        if (logger) {
            ctrl.setLogger(logger);
        }
    }

    #! Creates the object
    constructor(*LoggerInterface logger) : LoggerWrapper(logger) {
        ctrl = new Qore::AsyncIoController(True);
        if (logger) {
            ctrl.setLogger(logger);
        }
    }

    #! Stops the monitoring thread and destroys the object
    destructor() {
        ctrl.stop();
    }

    #! Returns information about the object
    hash<auto> getInfo() {
        return ctrl.getInfo();
    }

    #! Returns the number of operations in the cache (useful for testing cleanup)
    int getCacheSize() {
        return ctrl.getCacheSize();
    }

    #! Set autostop flag
    setAutostop(bool autostop = True) {
        ctrl.setAutostop(autostop);
    }

    #! Get autostop flag
    bool getAutostop() {
        return ctrl.getAutostop();
    }

    #! Starts monitoring
    /** This method starts the I/O thread and waits for it to signal that it's ready
        before returning. This ensures deterministic behavior where callers can
        immediately submit operations after start() returns.

        @throw CONTROLLER-START-ERROR if the I/O thread is already running
    */
    start() {
        ctrl.start();
    }

    #! Stops monitoring
    stop() {
        ctrl.stop();
    }

    #! Stops monitoring and clears all connections
    stopClear() {
        ctrl.stopClear();
    }

    #! Waits for the monitoring thread to stop if it's running
    waitStop() {
        ctrl.waitStop();
    }

    #! Returns @ref True if the I/O thread is running
    bool running() {
        return ctrl.running();
    }

    #! Waits for the I/O thread to be ready
    /** This method blocks until the I/O thread has started and is ready to process operations.
        Use this after submitting operations to ensure the I/O thread is polling before
        attempting to connect to the socket.

        @param to maximum time to wait; use @ref NOTHING for no timeout

        @return @ref True if the I/O thread is ready, @ref False if the timeout expired
    */
    bool waitReady(*timeout to) {
        return ctrl.waitReady(to);
    }

    #! Submits a socket operation to be performed and returns a Queue where the results will be returned
    /** @param info a hash with the socket operation and supporting info
        @param replace if True, then any operation currently in place for the given socket will be replaced

        @return a Queue that will have the result of the operation pushed to it; if \a info.resultQueue is
        provided, that Queue is returned; returns @ref NOTHING when \a info.callback is provided

        @throw CONTROLLER-ADD-ERROR invalid or incomplete data in \a info; a different operation on this socket is
        already in progress
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
        return ctrl.submit(info, replace);
    }

    #! Executes a socket operation asynchronously and waits for the results
    /** @param info a hash with the socket operation and supporting info
        @param replace if True, then any operation currently in place for the given socket will be replaced

        @return The result of the asynchronous operation

        @throw CONTROLLER-ADD-ERROR invalid or incomplete data in \a info; a different operation on this socket is
        already in progress
    */
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
        return ctrl.exec(info, replace);
    }

    #! Cancels the operation with the given Socket's unique hash as the key
    /** @param sock the socket to cancel any operation in progress for

        @note This method only cancels operations keyed by the socket's unique hash (the default).
        Operations submitted with a custom \c key will not be found by this method; use
        cancelByKey() or cancelByOwner() for those.

        Stops the I/O thread if the last operation is canceled and the \a autostop flag is enabled.

        @see cancelByKey()
        @see cancelByOwner()
        @see cancelEx()
    */
    bool cancel(Socket sock) {
        return ctrl.cancel(sock);
    }

    #! Wakes the I/O thread to re-evaluate poll state
    wake() {
        ctrl.wake();
    }

    #! Cancels any operation in progress for the given Socket
    /** @param sock the socket to cancel any operation in progress for

        @return @ref True if an operation for the socket was canceled, @ref False if not

        @note Stops the I/O thread if the last operation is canceled and the \a autostop flag is enabled

        @throw CONTROLLER-CANCEL-ERROR thrown if the Socket has no operation in progress

        @see cancel()
    */
    bool cancelEx(Socket sock) {
        if (!cancel(sock)) {
            throw "CONTROLLER-CANCEL-ERROR", sprintf("Socket %y has no operation in progress",
                sock.uniqueHash());
        }
        return True;
    }

    #! Cancels the operation with the given custom key
    /** @param key the custom key used when submitting the operation

        @return True if an operation was found and canceled, False otherwise

        @note This method is needed for operations submitted with a custom \c key field,
        since cancel(Socket) only looks up the socket's unique hash.

        @since %AsyncSocketIo 2.0

        @see cancel()
        @see cancelByOwner()
    */
    bool cancelByKey(string key) {
        return ctrl.cancelByKey(key);
    }

    #! Cancels all operations belonging to a specific owner
    /** @param owner the owner identifier (typically from uniqueHash()) to cancel operations for

        Operations are matched by checking the \c owner field in the @ref SocketPollOperationInfo hash.

        @return the number of operations canceled

        @note This method blocks until all matching operations have been canceled.
        The I/O thread continues running and other owners' operations are not affected.

        @since %AsyncSocketIo 1.1
    */
    int cancelByOwner(string owner) {
        return ctrl.cancelByOwner(owner);
    }

    #! Sets the logger for the controller
    setLogger(*LoggerInterface logger) {
        LoggerWrapper::setLogger(logger);
        ctrl.setLogger(logger);
    }
}

#! Returns the global async I/O controller singleton
/** The controller is created during module initialization with autostop enabled.
*/
public AsyncSocketIoController sub getGlobalAsyncIoController() {
    return globalController;
}

our *AsyncSocketIoController globalController;
}
