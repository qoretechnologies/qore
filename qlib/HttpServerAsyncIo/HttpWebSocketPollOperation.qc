# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpWebSocketPollOperation.qc Poll operation for WebSocket connections

/** HttpWebSocketPollOperation.qc Copyright 2024 - 2026 Qore Technologies, s.r.o.

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

#! Main namespace
public namespace HttpServerAsyncIo {

#! Poll operation for WebSocket bidirectional communication
/** This poll operation manages a WebSocket connection with:
    - Frame reading (text, binary, ping, pong, close)
    - Frame writing via queue
    - Automatic ping/pong handling
    - Close handshake
    - Heartbeat via poll_timeout_ms

    The C++ base class (HttpWebSocketPollOperationBase) handles the hot path:
    - Frame decoding/encoding via WebSocketFrameCodec
    - Non-blocking socket I/O via inner poll operations
    - Auto pong, close handshake, fragment reassembly
    - Heartbeat ping when no recv activity

    This Qore wrapper adds:
    - Completion context for the HttpAsyncSocketIoController
    - Frame dispatch to WebSocketConnection via onPollComplete (worker thread)
    - Extension transform support
    - Connection lifecycle signaling via Counter

    WebSocket frames are handled according to RFC 6455.
*/
public class HttpWebSocketPollOperation inherits HttpWebSocketPollOperationBase,
        HttpConnectionPollOperationBase {
    public {
        #! State constants (for backward compatibility)
        const STATE_CONNECTED = "connected";
        const STATE_CLOSING = "closing";
        const STATE_CLOSED = "closed";
        const STATE_ERROR = "error";
    }

    private {
        #! The WebSocketConnection for frame dispatch (untyped to avoid circular dependency)
        auto wsc;

        #! The WebSocketHandler for logging (untyped to avoid circular dependency)
        auto handler;

        #! Counter to signal when the connection closes
        *Counter done;

        #! Mutex to serialize frame-dispatch state (dispatching flag, onComplete,
        #! and the inline-drain fallback)
        Mutex dispatch_mutex();

        #! Counter tracking handler-pool drainers in flight
        /** onComplete() blocks on @ref Counter::waitForZero() so that no
            handler-pool drainer is still running when it finally clears
            @ref wsc/@ref handler.
        */
        Counter active_dispatches();

        #! True when onComplete has run — prevents new onPollComplete dispatches
        bool completed = False;

        #! True while a handler-pool drainer is running for this connection
        /** Protected by @ref dispatch_mutex.  When True, additional
            @ref onPollComplete() calls observe the flag and return without
            submitting a duplicate handler-pool task; the running drainer
            picks up any newly arrived frames before it clears the flag.
        */
        bool dispatching = False;
    }

    #! Creates the poll operation
    /** @param sock the WebSocket socket (after upgrade handshake complete)
    */
    constructor(Socket sock) : HttpWebSocketPollOperationBase(sock) {
    }

    #! Sets the connection and handler references for frame dispatch
    /** Called from startImpl() after the WebSocketConnection is created.

        @param wsc the WebSocket connection object
        @param handler the WebSocket handler for logging and extension access
    */
    public setConnection(auto wsc, auto handler) {
        self.wsc = wsc;
        self.handler = handler;

        # Process any frames that arrived before the connection was set.
        # The I/O thread may dispatch onPollComplete before setConnection
        # is called; those dispatches return immediately (!wsc check).
        # Drain the queue here to handle any frames that were dropped.
        # drainInlineLocked() also wakes the controller for any echoes
        # processServerFrame() pushed to send_queue.
        AutoLock al(dispatch_mutex);
        drainInlineLocked();
    }

    #! Sets the done counter for lifecycle signaling
    /** @param done counter to decrement when the connection closes.
        If onComplete() has already run (race between I/O completion and
        handler setup), signals immediately.
    */
    public setDone(Counter done) {
        AutoLock al(dispatch_mutex);
        self.done = done;
        if (completed) {
            # onComplete() already ran before we set the counter — signal now
            done.dec();
        }
    }

    #! Called by the AsyncSocketIo controller when frames have been pushed
    /** Hops frame processing from the AsyncSocketIo callback dispatcher worker
        onto a handler-pool thread via
        @ref HttpAsyncSocketIoController::submitForConnection() so that user
        code (\c WebSocketConnection::gotMessage(),
        \c WebSocketConnection::gotRawMessage(), and any per-dispatch hooks
        installed by the target) never runs on the AsyncSocketIo callback
        dispatcher worker — that thread is reserved for short, non-blocking
        callbacks and running user code there risks deadlocks and bypasses
        the @ref HttpAsyncTarget::saveDispatchContext() /
        @ref HttpAsyncTarget::restoreDispatchContext() hooks.

        Re-entrant onPollComplete() calls while a drainer is running observe
        the @ref dispatching flag and return; the in-flight drainer drains
        any newly arrived frames before clearing the flag.

        If the completion context has not been wired yet (the narrow window
        between op construction and @c registerH1WebSocketConnection), this
        method falls back to an inline drain that matches the legacy
        @ref setConnection() behavior.
    */
    public onPollComplete() {
        HttpWebSocketPollOperationBase::onPollComplete();

        *HttpAsyncSocketIoController c;
        *HttpAsyncTarget t;
        {
            AutoLock al(dispatch_mutex);
            if (completed || !wsc) {
                return;
            }
            if (dispatching) {
                # An already-running handlerPoolDrain() will pick up new
                # frames before it clears the dispatching flag.
                return;
            }
            if (!completion_controller || !completion_target) {
                # Pre-completionContext fallback: drain inline under the
                # mutex.  Mirrors @ref setConnection()'s upgrade-window drain.
                drainInlineLocked();
                return;
            }
            c = completion_controller;
            t = completion_target;
            dispatching = True;
            active_dispatches.inc();
        }
        try {
            c.submitToHandlerPool(t, \handlerPoolDrain());
        } catch (hash<ExceptionInfo> ex) {
            # Pool stopping or otherwise rejected the submission — clear the
            # flags so that future onPollComplete() calls (or onComplete()'s
            # final drain) can make progress.  A stopping pool means the
            # connection is going down anyway.
            {
                AutoLock al(dispatch_mutex);
                dispatching = False;
            }
            active_dispatches.dec();
        }
    }

    #! Drains queued frames on a handler-pool thread, serialized by dispatch_mutex
    /** Runs as the body submitted by @ref onPollComplete() to the controller.
        Must clear the @ref dispatching flag and decrement
        @ref active_dispatches before returning, on every code path, so
        @ref onComplete() can proceed.
    */
    private handlerPoolDrain() {
        bool wsc_alive = False;
        bool dispatch_cleared = False;
        on_exit {
            if (!dispatch_cleared) {
                AutoLock al(dispatch_mutex);
                dispatching = False;
            }
            active_dispatches.dec();
        }
        {
            AutoLock al(dispatch_mutex);
            if (!completed && wsc) {
                while (hasReceivedFrames()) {
                    *hash<auto> frame = getReceivedFrame();
                    if (!frame) {
                        break;
                    }
                    try {
                        processServerFrame(frame);
                    } catch (hash<ExceptionInfo> ex) {
                        if (ex.err == "OBJECT-ALREADY-DELETED") {
                            # Connection torn down mid-dispatch; skip the
                            # notifyIo() and exit cleanly.
                            dispatching = False;
                            dispatch_cleared = True;
                            return;
                        }
                        if (handler) {
                            try {
                                handler.logError("frame processing error: %s",
                                    get_exception_string(ex));
                            } catch () {}
                        }
                    }
                }
                wsc_alive = exists wsc;
            }
            dispatching = False;
            dispatch_cleared = True;
        }
        # Wake the controller to send any queued data (echo responses,
        # close echoes) that processServerFrame() pushed to send_queue.
        if (wsc_alive) {
            wsc.notifyIo();
        }
    }

    #! Drains queued frames inline under dispatch_mutex; caller must hold the mutex
    /** Used by @ref setConnection() during the upgrade-handshake window and by
        @ref onPollComplete()'s pre-completionContext fallback path.  Does not
        touch @ref dispatching or @ref active_dispatches — those are reserved
        for handler-pool drainers and for @ref onComplete() coordination.
    */
    private drainInlineLocked() {
        while (hasReceivedFrames()) {
            *hash<auto> frame = getReceivedFrame();
            if (!frame) {
                break;
            }
            try {
                processServerFrame(frame);
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == "OBJECT-ALREADY-DELETED") {
                    return;
                }
                if (handler) {
                    try {
                        handler.logError("frame processing error: %s",
                            get_exception_string(ex));
                    } catch () {}
                }
            }
        }
        if (wsc) {
            wsc.notifyIo();
        }
    }

    #! Called when the poll operation completes (goal reached, error, or timeout)
    /** Signals the handler thread's done counter so it can run cleanup.
        Sets the completed flag to prevent new onPollComplete dispatches,
        then waits for any running ones to finish.
    */
    nothing onComplete(hash<SocketPollResultInfo> result) {
        # Set completed flag under the mutex to prevent new dispatches
        {
            AutoLock al(dispatch_mutex);
            completed = True;
        }
        # Now wait for any in-progress dispatches to finish
        active_dispatches.waitForZero();
        {
            AutoLock al(dispatch_mutex);
            # Drain any remaining frames
            if (wsc) {
                while (hasReceivedFrames()) {
                    *hash<auto> frame = getReceivedFrame();
                    if (!frame) {
                        break;
                    }
                    try {
                        processServerFrame(frame);
                    } catch () {}
                }
            }
            # Clear references to prevent further dispatch
            wsc = NOTHING;
            handler = NOTHING;
        }
        # Signal the handler thread to wake up and run cleanup
        if (done) {
            done.dec();
        }
        # Call base class to trigger handleConnectionResult() → handleWebSocketClose(),
        # which removes the stale connections{uh} entry and cancels the I/O operation.
        # Without this, asyncIo.stop() finds stale entries and sends unnecessary cancel
        # commands that can race with I/O thread auto-stop on ARM.
        HttpConnectionPollOperationBase::onComplete(result);
    }

    #! gotRawMessage return values (from WebSocketHandler, duplicated to avoid circular dep)
    private {
        const WSC_Process = 0;
        const WSC_Continue = 1;
        const WSC_Break = 2;
    }

    #! Processes a single frame from the C++ recv_queue
    private processServerFrame(hash<auto> frame) {
        string frame_type = frame.type;

        # Close — queue the close echo from the Qore side (after all data
        # frames have been processed) so echo responses are delivered to the
        # peer before the close frame.  The C++ side does NOT echo
        # peer-initiated close frames — that's deferred to here.
        if (frame_type == "close") {
            sendClose(frame.code ?? WSCC_NormalClosure);
            return;
        }

        # Ping/pong — C++ auto-pongs; just log
        if (frame_type == "ping" || frame_type == "pong") {
            return;
        }

        # Data frames (text/binary) — apply extensions and dispatch
        if (frame_type == "text" || frame_type == "binary") {
            int op = frame_type == "text" ? WSOP_Text : WSOP_Binary;

            # Build WsMsgInfo for gotRawMessage / extension processing
            hash<WsMsgInfo> h = cast<hash<WsMsgInfo>>({
                "op": op,
                "msg": frame.data,
                "masked": True,
                "fin": True,
                "rsv1": frame.rsv1 ? True : False,
                "rsv2": frame.rsv2 ? True : False,
                "rsv3": frame.rsv3 ? True : False,
            });

            # Validate RSV bits against active extensions
            if (!wsc.validateRsvBits(h.rsv1, h.rsv2, h.rsv3)) {
                if (handler) {
                    handler.logError("invalid RSV bits: rsv1=%y rsv2=%y rsv3=%y",
                        h.rsv1, h.rsv2, h.rsv3);
                }
                sendClose(WSCC_ProtocolError);
                return;
            }

            # gotRawMessage callback — allows subclasses to inspect/filter raw frames
            int state = wsc.gotRawMessage(h);
            if (state == WSC_Continue) {
                return;
            }
            if (state == WSC_Break) {
                sendClose(WSCC_NormalClosure);
                return;
            }

            # Apply incoming extension transforms
            auto msg = h.msg;
            hash<auto> ext_ctx = {
                "op": h.op,
                "rsv1": h.rsv1,
                "rsv2": h.rsv2,
                "rsv3": h.rsv3,
                "fin": True,
            };
            call_object_method(wsc, "applyIncomingExtensions", \msg, ext_ctx);
            call_object_method(wsc, "validateIncomingMessage", \msg);
            # Wrap user-code dispatch in the handler's thread-local save/restore
            # so any framework context (e.g. Qorus service-context tld) is
            # initialized on this worker-pool thread before gotMessage() runs.
            # Mirrors WebSocketHandler::WebSocketStreamProcessor::notifyIo (H2/H3 path).
            # The handler-pool worker starts with cleared tld (ThreadPool calls
            # clear_all_program_thread_local_data() between tasks); without this
            # wrap, gotMessage() runs with no service context and code paths
            # that need it (e.g. UserApi::getConfigItemValue() returning NOTHING)
            # fail silently.
            *hash<auto> tld_save;
            if (exists handler) {
                tld_save = call_object_method(handler, "saveThreadLocalData");
            }
            on_exit if (exists handler) {
                call_object_method(handler, "restoreThreadLocalData", tld_save);
            }
            wsc.gotMessage(msg);
        }
    }
}

} # namespace HttpServerAsyncIo
