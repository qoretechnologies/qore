# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AbstractStreamContext.qc Protocol-agnostic stream I/O interface

/*  AbstractStreamContext.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Protocol-agnostic stream I/O interface for extended CONNECT tunnels
/** Abstract base class for stream contexts used by WebSocket over HTTP/2 (RFC 8441)
    and HTTP/3 (RFC 9220). Provides a unified API for bidirectional stream I/O,
    enabling protocol-independent WebSocket frame handling.

    Subclasses:
    - @ref Http2StreamContext — HTTP/2 stream I/O via nghttp2
    - @ref Http3StreamContext — HTTP/3 stream I/O via ngtcp2/nghttp3
*/
public class AbstractStreamContext {
    private {
        #! True once this context sent the close frame of a WebSocket connection (@ref sendWebSocketClose())
        bool ws_close_sent = False;

        #! The record of the close frame of the stream's WebSocket connection, set by transports whose streams have
        #! a WebSocket frame state (HTTP/2 and HTTP/3); it outlives the stream and the session
        /** Held here and read inline by @ref isWebSocketCloseSent() and the data-frame check of every send, so that
            the check calls no method that a subclass overrides: such a self call is dispatched by name at run time
            on every send.
        */
        *WebSocketCloseRecord ws_close_record;
    }

    public {
        #! The opcode of a WebSocket close frame; frames with a lower opcode are data frames (RFC 6455 section 5.2)
        const WsOpcodeClose = 8;
    }

    #! Returns the protocol identifier
    /** @return \c "http2" or \c "http3"
    */
    abstract string getProtocol();

    #! Returns the stream ID
    /** @return the protocol-specific stream identifier
    */
    abstract int getStreamId();

    #! Returns the underlying socket
    /** @return the socket used for stream I/O
    */
    abstract Socket getSocket();

    #! Returns whether the stream is still open
    /** @return @ref True if the underlying socket is open
    */
    abstract bool isOpen();

    #! Send data on the stream
    /** @param data the binary payload to send
        @param end_stream if @ref True, signals end of stream after this data

        @throw WEBSOCKET-CLOSING-ERROR \a data is a WebSocket data frame and a close frame was sent on the stream
        before (see @ref sendWebSocketClose()); also raised by @ref sendDataWithTimeout() and @ref trySendData()
    */
    abstract sendData(binary data, bool end_stream = False);

    #! Sends data on the stream, waiting at most the given time for a full stream send buffer to drain
    /** Transports with a bounded stream send buffer (HTTP/2 and HTTP/3) override this to wait for the buffer to
        drain when it is full, which happens when the peer does not read as fast as data is sent, instead of
        failing; the default implementation calls @ref sendData().

        @param data the binary payload to send
        @param timeout_ms the maximum time to wait for the stream send buffer to drain; a value less than or equal to
        zero waits without a limit
        @param end_stream if @ref True, signals end of stream after this data

        @throw STREAM-SEND-TIMEOUT the stream send buffer did not drain in time; the data was not sent

        @since AsyncSocketIo 1.0
    */
    sendDataWithTimeout(binary data, timeout timeout_ms, bool end_stream = False) {
        sendData(data, end_stream);
    }

    #! Attempts to send data without blocking on outbound backpressure
    /** Transports that can detect a full stream send buffer without blocking (HTTP/2 and HTTP/3) override this to
        return @ref False when the buffer is full instead of blocking or waiting for it to drain.  This lets a
        caller (e.g. a WebSocket broadcast loop) drop a slow or non-reading client immediately rather than stalling.

        Dropping individual frames on a live connection is never safe for an ordered protocol like WebSocket, so on
        backpressure the caller must close the whole connection — the un-sent frame is intentionally discarded
        because the peer receives a connection close rather than a gap in the stream.

        The default implementation performs a normal (possibly blocking) @ref sendData() and returns @ref True.

        @param data the binary payload to send
        @param end_stream if @ref True, signals end of stream after this data

        @return @ref True if the data was accepted for sending; @ref False if the stream send buffer is full
        (backpressure) and the caller should drop the connection

        @since AsyncSocketIo 1.0
    */
    bool trySendData(binary data, bool end_stream = False) {
        sendData(data, end_stream);
        return True;
    }

    #! Sends the close frame of a WebSocket connection on the stream, unless one was sent on it before
    /** A WebSocket connection has one close frame, after which no data frame is sent (RFC 6455 section 5.5.1).
        Transports whose streams have a WebSocket frame state that can send a close frame itself (HTTP/2 and HTTP/3)
        override this to record the close frame with the frame state's: if a close frame was sent before, this one is
        not sent, and data sent after it is refused with \c WEBSOCKET-CLOSING-ERROR; the close frame is never
        refused for a full stream send buffer.  The default implementation calls @ref sendData() and returns
        @ref True.

        @param frame the encoded close frame
        @param end_stream if @ref True, ends the stream after the frame (also if the frame is not sent)

        @return @ref True if the frame was sent; @ref False if a close frame was sent on the stream before

        @since AsyncSocketIo 1.0
    */
    bool sendWebSocketClose(binary frame, bool end_stream = False) {
        if (isWebSocketCloseSent()) {
            return False;
        }
        sendData(frame, end_stream);
        ws_close_sent = True;
        return True;
    }

    #! Returns @ref True if the close frame of a WebSocket connection was sent on the stream
    /** A WebSocket connection has one close frame, after which no data frame is sent (RFC 6455 section 5.5.1).  The
        connection's record of its close frame decides before any state of the stream or of its session: it holds
        also after the stream or the session has ended.  The result is @ref True once this context has sent a close
        frame (@ref sendWebSocketClose()), or, on transports whose streams have a WebSocket frame state that can send
        a close frame itself (HTTP/2 and HTTP/3), once the stream's record says that a close frame was sent (see
        @ref Qore::WebSocketCloseRecord "WebSocketCloseRecord"), which includes the frame state's close frame.

        @return @ref True if a close frame was sent on the stream

        @note this method is final: the transports set the record (@ref ws_close_record) instead of overriding it, so
        that the check of each send is not dispatched by name

        @since AsyncSocketIo 1.0
    */
    final bool isWebSocketCloseSent() {
        # with a record, the session records this context's close frame in it too (every close frame of the stream is
        # recorded there), so one member is read
        return ws_close_record ? ws_close_record.isCloseSent() : ws_close_sent;
    }

    #! Raises @c WEBSOCKET-CLOSING-ERROR if \a data is a WebSocket data frame and a close frame was sent on the stream
    /** Called by the transports before they hand data to the socket, so that the connection's record decides before
        any state of the stream or of its session.
    */
    final private checkWebSocketDataFrame(binary data) {
        # the same test as isWebSocketCloseSent(), inline: this runs for every data frame sent
        if (data && (data[0] & 0x0f) < WsOpcodeClose
                && (ws_close_record ? ws_close_record.isCloseSent() : ws_close_sent)) {
            throw "WEBSOCKET-CLOSING-ERROR", sprintf("cannot send data on %s stream %d: its WebSocket connection sent "
                "a close frame; no data frame may follow a close frame (RFC 6455 section 5.5.1)", getProtocol(),
                getStreamId());
        }
    }

    #! Flushes pending transport data if the protocol has an explicit flush path
    /** Default implementation is a no-op. HTTP/3 overrides this to force a
        QUIC write cycle after startup data is enqueued from a handler thread.
    */
    flushPendingData() {
    }

    #! Read data from the stream
    /** @return binary data if available, or @ref nothing if no data is buffered
    */
    abstract *binary readData(int max_bytes = 0);

    #! Returns True when the transport feeds pre-decoded WebSocket frames
    /** Subclasses that support WebSocket frame-level delivery (HTTP/3 with
        C++ <tt>WebSocketStreamFrameState</tt> in the loop) override this.  Default
        is False, meaning @ref readData() returns raw bytes.
    */
    bool usesFrameState() {
        return False;
    }

    #! Drains one pre-decoded WebSocket frame hash (only valid when usesFrameState())
    /** Default implementation throws — concrete classes that return True from
        @ref usesFrameState() must override.
    */
    *hash<auto> readFrame() {
        throw "STREAM-CONTEXT-ERROR",
            "readFrame() not supported on this stream context";
    }

    #! Close the stream
    /** Default implementation is a no-op. HTTP/2 streams die with the TCP connection;
        HTTP/3 streams require explicit END_STREAM signaling.
    */
    close() {
        # no-op by default; Http3StreamContext overrides
    }
}

} # namespace AsyncSocketIo
