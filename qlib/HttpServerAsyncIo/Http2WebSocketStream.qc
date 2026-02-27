# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http2WebSocketStream.qc WebSocket over HTTP/2 (RFC 8441)

/** Http2WebSocketStream.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! WebSocket connection over HTTP/2 stream (RFC 8441)
/** This class implements WebSocket over HTTP/2 as defined in RFC 8441
    "Bootstrapping WebSockets with HTTP/2".

    The WebSocket is established via an extended CONNECT request with
    :protocol pseudo-header set to "websocket". The HTTP/2 stream then
    becomes a bidirectional tunnel for WebSocket frames.

    Key differences from HTTP/1.x WebSocket:
    - No HTTP Upgrade mechanism - uses CONNECT with :protocol
    - WebSocket frames are carried in HTTP/2 DATA frames
    - Multiplexing: multiple WebSocket connections can share one TCP connection
    - Header compression via HPACK

*/
public class Http2WebSocketStream inherits AbstractWebSocketConnection {
    public {
        #! State constants
        const STATE_CONNECTED = "connected";
        const STATE_CLOSING = "closing";
        const STATE_CLOSED = "closed";
        const STATE_ERROR = "error";
    }

    private {
        #! The HTTP/2 poll operation managing this connection
        Http2PollOperation poll_op;

        #! The stream ID for this WebSocket
        int stream_id;

        #! Current state
        string state = STATE_CONNECTED;

        #! Channel of frames to send
        Channel send_channel(-1);

        #! Channel of received frames
        Channel recv_channel(-1);

        #! Buffer for incoming WebSocket frame data
        binary recv_buffer = binary();

        #! Close code
        int close_code = 1000;

        #! Close reason
        string close_reason = "";

        #! True if we initiated close
        bool close_initiated = False;

        #! Maximum frame size (16MB default)
        int max_frame_size = 16 * 1024 * 1024;
    }

    #! Creates a WebSocket stream over HTTP/2
    /** @param poll_op the HTTP/2 poll operation for the connection
        @param stream_id the HTTP/2 stream ID for this WebSocket (from CONNECT request)

        @note The CONNECT response should already have been sent with status 200
    */
    constructor(Http2PollOperation poll_op, int stream_id) {
        self.poll_op = poll_op;
        self.stream_id = stream_id;
    }

    string getProtocol() {
        return "http2";
    }

    sendText(string text) {
        queueFrame(WS_OP_TEXT, binary(text));
    }

    sendBinary(binary data) {
        queueFrame(WS_OP_BINARY, data);
    }

    sendPing(*binary data) {
        queueFrame(WS_OP_PING, data ?? binary());
    }

    sendClose(int code = 1000, string reason = "") {
        if (state != STATE_CONNECTED) {
            return;
        }

        close_initiated = True;
        close_code = code;
        close_reason = reason;

        binary payload = binary();
        payload += binary(chr((code >> 8) & 0xFF));
        payload += binary(code & 0xFF);
        if (reason) {
            payload += binary(reason);
        }

        queueFrame(WS_OP_CLOSE, payload);
        state = STATE_CLOSING;
    }

    *hash<WebSocketFrameInfo> getReceivedFrame() {
        auto result = recv_channel.tryRecv();
        return exists result ? result.value : NOTHING;
    }

    bool hasReceivedFrames() {
        return !recv_channel.empty();
    }

    bool isOpen() {
        return state == STATE_CONNECTED && poll_op.getSocket().isOpen();
    }

    string getState() {
        return state;
    }

    int getCloseCode() {
        return close_code;
    }

    string getCloseReason() {
        return close_reason;
    }

    abort() {
        state = STATE_CLOSED;
        try {
            poll_op.abort();
        } catch () {
            # Ignore errors on abort
        }
    }

    #! Returns the HTTP/2 stream ID
    int getStreamId() {
        return stream_id;
    }

    #! Returns the underlying poll operation
    Http2PollOperation getPollOperation() {
        return poll_op;
    }

    #! Queues a WebSocket frame for sending
    private queueFrame(int opcode, binary data) {
        if (state == STATE_CLOSED || state == STATE_ERROR) {
            throw "WEBSOCKET-CLOSED", "WebSocket connection is closed";
        }
        send_channel.send({
            "opcode": opcode,
            "data": data,
        });
    }

    #! Gets the next frame to send (for poll operation use)
    /** @return frame hash with opcode and data, or NOTHING if queue is empty
    */
    *hash<auto> getNextSendFrame() {
        auto result = send_channel.tryRecv();
        return exists result ? result.value : NOTHING;
    }

    #! Processes received data from HTTP/2 DATA frames
    /** Called by the poll operation when data is received on this stream.
        Parses WebSocket frames from the data and adds them to the receive queue.
    */
    processReceivedData(binary data) {
        recv_buffer += data;
        parseFrames();
    }

    #! Parses WebSocket frames from the receive buffer
    private parseFrames() {
        while (recv_buffer.size() >= 2) {
            # Parse frame header
            int b0 = recv_buffer[0];
            int b1 = recv_buffer[1];

            bool fin = (b0 & 0x80) != 0;
            int opcode = b0 & 0x0F;
            bool masked = (b1 & 0x80) != 0;
            int payload_len = b1 & 0x7F;

            int header_size = 2;
            if (payload_len == 126) {
                if (recv_buffer.size() < 4) {
                    return;  # Need more data
                }
                payload_len = (recv_buffer[2] << 8) | recv_buffer[3];
                header_size = 4;
            } else if (payload_len == 127) {
                if (recv_buffer.size() < 10) {
                    return;  # Need more data
                }
                payload_len = 0;
                for (int i = 0; i < 8; ++i) {
                    payload_len = (payload_len << 8) | recv_buffer[2 + i];
                }
                header_size = 10;
            }

            if (masked) {
                header_size += 4;  # Masking key
            }

            # Check frame size limit
            if (payload_len > max_frame_size) {
                sendClose(1009, "Message too big");
                return;
            }

            int total_frame_size = header_size + payload_len;
            if (recv_buffer.size() < total_frame_size) {
                return;  # Need more data
            }

            # Extract payload
            binary payload = binary();
            if (payload_len > 0) {
                payload = recv_buffer.substr(header_size, payload_len);

                # Unmask if needed (client frames are always masked)
                if (masked) {
                    binary mask = recv_buffer.substr(header_size - 4, 4);
                    payload = unmaskData(payload, mask);
                }
            }

            # Remove processed frame from buffer
            recv_buffer = recv_buffer.substr(total_frame_size);

            # Handle frame
            handleFrame(opcode, payload, fin);
        }
    }

    #! Unmasks WebSocket data
    private binary unmaskData(binary data, binary mask) {
        binary result = binary();
        for (int i = 0; i < data.size(); ++i) {
            result += binary(chr(data[i] ^ mask[i % 4]));
        }
        return result;
    }

    #! Handles a received WebSocket frame
    private handleFrame(int opcode, binary payload, bool fin) {
        switch (opcode) {
            case WS_OP_TEXT: {
                string text = payload.toString("UTF-8");
                recv_channel.send(<WebSocketFrameInfo>{
                    "type": "text",
                    "data": text,
                });
                break;
            }

            case WS_OP_BINARY: {
                recv_channel.send(<WebSocketFrameInfo>{
                    "type": "binary",
                    "data": payload,
                });
                break;
            }

            case WS_OP_PING: {
                # Send pong with same payload
                queueFrame(WS_OP_PONG, payload);
                recv_channel.send(<WebSocketFrameInfo>{
                    "type": "ping",
                    "data": payload,
                });
                break;
            }

            case WS_OP_PONG: {
                recv_channel.send(<WebSocketFrameInfo>{
                    "type": "pong",
                    "data": payload,
                });
                break;
            }

            case WS_OP_CLOSE: {
                if (payload.size() >= 2) {
                    close_code = (payload[0] << 8) | payload[1];
                    if (payload.size() > 2) {
                        close_reason = payload.substr(2).toString("UTF-8");
                    }
                }

                recv_channel.send(<WebSocketFrameInfo>{
                    "type": "close",
                    "code": close_code,
                    "reason": close_reason,
                });

                if (!close_initiated) {
                    # Echo the close frame
                    queueFrame(WS_OP_CLOSE, payload);
                }
                state = STATE_CLOSING;
                break;
            }

            case WS_OP_CONTINUATION: {
                recv_channel.send(<WebSocketFrameInfo>{
                    "type": "binary",
                    "data": payload,
                });
                break;
            }
        }
    }

    #! Encodes a WebSocket frame for sending
    /** @param opcode the WebSocket opcode
        @param payload the frame payload
        @return encoded frame as binary
    */
    static binary encodeFrame(int opcode, binary payload) {
        binary header = binary();
        # FIN bit + opcode
        header += binary(chr(0x80 | opcode));

        # Payload length (server frames are not masked)
        if (payload.size() < 126) {
            header += binary(chr(payload.size()));
        } else if (payload.size() < 65536) {
            header += binary(chr(126));
            header += binary(chr((payload.size() >> 8) & 0xFF));
            header += binary(chr(payload.size() & 0xFF));
        } else {
            header += binary(chr(127));
            for (int i = 7; i >= 0; --i) {
                header += binary(chr((payload.size() >> (i * 8)) & 0xFF));
            }
        }

        return header + payload;
    }
}

} # namespace HttpServerAsyncIo
