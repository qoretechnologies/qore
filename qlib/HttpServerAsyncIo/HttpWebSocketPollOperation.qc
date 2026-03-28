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

    The C++ base class (HttpWebSocketPollOperationBase) handles the hot path:
    - Frame decoding/encoding via WebSocketFrameCodec
    - Non-blocking socket I/O via inner poll operations
    - Auto pong, close handshake, fragment reassembly

    This Qore wrapper adds:
    - Completion context for the HttpAsyncSocketIoController
    - High-level Channel-based API (if needed in future)

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

    #! Creates the poll operation
    /** @param sock the WebSocket socket (after upgrade handshake complete)
    */
    constructor(Socket sock) : HttpWebSocketPollOperationBase(sock) {
    }
}

} # namespace HttpServerAsyncIo
