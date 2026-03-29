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

        #! Mutex to serialize onPollComplete and onComplete
        Mutex dispatch_mutex();

        #! Counter tracking active onPollComplete dispatches
        Counter active_dispatches();

        #! True when onComplete has run — prevents new onPollComplete dispatches
        bool completed = False;
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
    }

    #! Sets the done counter for lifecycle signaling
    /** @param done counter to decrement when the connection closes
    */
    public setDone(Counter done) {
        self.done = done;
    }

    #! Called by the I/O controller on a worker thread when frames have been pushed
    /** Drains the C++ recv_queue, applies extension transforms, and dispatches
        to the WebSocketConnection's gotMessage() callback.

        Runs on a WORKER thread (never on the I/O thread).
    */
    public onPollComplete() {
        {
            AutoLock al(dispatch_mutex);
            if (completed || !wsc) {
                return;
            }
            active_dispatches.inc();
        }
        on_exit active_dispatches.dec();
        {
            AutoLock al(dispatch_mutex);
            if (!wsc) {
                return;
            }
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
                            handler.logError("frame processing error: %s: %s", ex.err, ex.desc);
                        } catch () {}
                    }
                }
            }
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

        # Close — C++ already handled the close handshake
        if (frame_type == "close") {
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
            wsc.gotMessage(msg);
        }
    }
}

} # namespace HttpServerAsyncIo
