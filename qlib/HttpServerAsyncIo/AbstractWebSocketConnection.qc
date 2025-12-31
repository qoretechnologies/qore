# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AbstractWebSocketConnection.qc Abstract WebSocket connection interface

/** AbstractWebSocketConnection.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Abstract interface for WebSocket connections
/** This abstract class provides a protocol-agnostic interface for WebSocket connections,
    supporting both HTTP/1.x (RFC 6455) and HTTP/2 (RFC 8441) transports transparently.

    Implementations:
    - @ref HttpWebSocketPollOperation for HTTP/1.x
    - @ref Http2WebSocketStream for HTTP/2

    @since HttpServerAsyncIo 1.0
*/
public class AbstractWebSocketConnection {
    #! Returns the underlying transport protocol
    /** @return "http1" for HTTP/1.x upgraded connection, "http2" for HTTP/2 stream (RFC 8441)
    */
    abstract string getProtocol();

    #! Sends a text message
    /** @param text the text message to send
    */
    abstract sendText(string text);

    #! Sends a binary message
    /** @param data the binary data to send
    */
    abstract sendBinary(binary data);

    #! Sends a ping frame
    /** @param data optional payload data for the ping
    */
    abstract sendPing(*binary data);

    #! Initiates close handshake
    /** @param code the close code (default 1000 for normal closure)
        @param reason the close reason string
    */
    abstract sendClose(int code = 1000, string reason = "");

    #! Gets the next received frame (non-blocking)
    /** @return frame info hash or NOTHING if no frames available
    */
    abstract *hash<WebSocketFrameInfo> getReceivedFrame();

    #! Returns True if there are received frames waiting
    abstract bool hasReceivedFrames();

    #! Returns True if the connection is still open
    abstract bool isOpen();

    #! Returns the current connection state
    /** @return one of: "connected", "closing", "closed", "error"
    */
    abstract string getState();

    #! Returns the close code (valid after close)
    abstract int getCloseCode();

    #! Returns the close reason (valid after close)
    abstract string getCloseReason();

    #! Aborts the connection immediately
    abstract abort();
}

#! HTTP/1.x WebSocket connection wrapper
/** Wraps HttpWebSocketPollOperation to implement AbstractWebSocketConnection interface.
    This allows existing HTTP/1.x WebSocket code to work with the unified interface.

    @since HttpServerAsyncIo 1.0
*/
public class Http1WebSocketConnection inherits AbstractWebSocketConnection {
    private {
        HttpWebSocketPollOperation poll_op;
    }

    #! Creates wrapper around an HTTP/1.x WebSocket poll operation
    constructor(HttpWebSocketPollOperation poll_op) {
        self.poll_op = poll_op;
    }

    string getProtocol() {
        return "http1";
    }

    sendText(string text) {
        poll_op.sendText(text);
    }

    sendBinary(binary data) {
        poll_op.sendBinary(data);
    }

    sendPing(*binary data) {
        poll_op.sendPing(data);
    }

    sendClose(int code = 1000, string reason = "") {
        poll_op.sendClose(code, reason);
    }

    *hash<WebSocketFrameInfo> getReceivedFrame() {
        return poll_op.getReceivedFrame();
    }

    bool hasReceivedFrames() {
        return poll_op.hasReceivedFrames();
    }

    bool isOpen() {
        string state = poll_op.getState();
        return state == HttpWebSocketPollOperation::STATE_CONNECTED;
    }

    string getState() {
        return poll_op.getState();
    }

    int getCloseCode() {
        return poll_op.getCloseCode();
    }

    string getCloseReason() {
        return poll_op.getCloseReason();
    }

    abort() {
        poll_op.abort();
    }

    #! Returns the underlying poll operation for direct access if needed
    HttpWebSocketPollOperation getPollOperation() {
        return poll_op;
    }
}

} # namespace HttpServerAsyncIo
