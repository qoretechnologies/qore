# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpWebSocketPollOperation.qc Poll operation for WebSocket connections

/** HttpWebSocketPollOperation.qc Copyright 2024 Qore Technologies, s.r.o.

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

    WebSocket frames are handled according to RFC 6455.

    @since HttpServerAsyncIo 1.0
*/
public class HttpWebSocketPollOperation inherits AbstractPollOperation {
    public {
        #! State constants
        const STATE_CONNECTED = "connected";
        const STATE_CLOSING = "closing";
        const STATE_CLOSED = "closed";
        const STATE_ERROR = "error";
    }

    private {
        #! The WebSocket socket
        Socket sock;

        #! Current state
        string state = STATE_CONNECTED;

        #! Queue of frames to send
        Queue send_queue();

        #! Queue of received frames for the application
        Queue recv_queue();

        #! Pending outgoing data
        *binary pending_send;
        int pending_send_offset = 0;

        #! Buffer for incoming data
        binary recv_buffer = binary();

        #! Close code from close frame
        int close_code = 1000;

        #! Close reason from close frame
        string close_reason = "";

        #! True if we initiated close
        bool close_initiated = False;

        #! Maximum frame size (16MB default)
        int max_frame_size = 16 * 1024 * 1024;

        #! Error info
        *hash<ExceptionInfo> error_info;
    }

    #! Creates the poll operation
    /** @param sock the WebSocket socket (after upgrade handshake complete)
    */
    constructor(Socket sock) {
        self.sock = sock;
    }

    #! Returns the goal of the operation
    string getGoal() {
        return "websocket";
    }

    #! Returns the current state
    string getState() {
        return state;
    }

    #! Returns True when the operation is complete (closed or error)
    bool goalReached() {
        return state == STATE_CLOSED || state == STATE_ERROR;
    }

    #! Continues the poll operation
    /** @return poll info for the next poll() call, or NOTHING if complete
    */
    *hash<SocketPollInfo> continuePoll() {
        if (state == STATE_CLOSED || state == STATE_ERROR) {
            return NOTHING;
        }

        # Check if socket is still open
        if (!sock.isOpen()) {
            state = STATE_CLOSED;
            close_code = 1006;  # Abnormal closure
            close_reason = "Connection lost";
            return NOTHING;
        }

        int poll_events = 0;

        # Try to read available data
        try {
            if (sock.isDataAvailable(0ms)) {
                binary data = sock.recvBinary(-1, 0ms);
                if (!data || data.size() == 0) {
                    # Connection closed
                    state = STATE_CLOSED;
                    close_code = 1006;
                    close_reason = "Connection closed";
                    return NOTHING;
                }
                recv_buffer += data;

                # Process any complete frames in the buffer
                processReceivedData();
            }
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err !~ /TIMEOUT/) {
                state = STATE_ERROR;
                error_info = ex;
                return NOTHING;
            }
        }

        # Handle pending sends
        if (pending_send) {
            try {
                int sent = sock.send(pending_send.substr(pending_send_offset), 0ms);
                if (sent > 0) {
                    pending_send_offset += sent;
                    if (pending_send_offset >= pending_send.size()) {
                        remove pending_send;
                        pending_send_offset = 0;
                    }
                }
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err !~ /TIMEOUT/) {
                    state = STATE_ERROR;
                    error_info = ex;
                    return NOTHING;
                }
            }

            if (pending_send) {
                poll_events |= SOCK_POLLOUT;
            }
        }

        # Check for queued frames to send
        if (!pending_send) {
            try {
                *hash<auto> frame = send_queue.get(0ms);
                if (frame) {
                    pending_send = encodeFrame(frame);
                    pending_send_offset = 0;
                    poll_events |= SOCK_POLLOUT;
                }
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err != "QUEUE-TIMEOUT") {
                    rethrow;
                }
            }
        }

        # Always poll for incoming data
        poll_events |= SOCK_POLLIN;

        return <SocketPollInfo>{
            "events": poll_events,
            "socket": sock,
        };
    }

    #! Processes received data buffer for complete frames
    private processReceivedData() {
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
            binary payload;
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

            # Handle frame based on opcode
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

    #! Handles a received frame
    private handleFrame(int opcode, binary payload, bool fin) {
        switch (opcode) {
            case WS_OP_TEXT: {
                string text = payload.toString("UTF-8");
                recv_queue.push(<WebSocketFrameInfo>{
                    "type": "text",
                    "data": text,
                });
                break;
            }

            case WS_OP_BINARY: {
                recv_queue.push(<WebSocketFrameInfo>{
                    "type": "binary",
                    "data": payload,
                });
                break;
            }

            case WS_OP_PING: {
                # Send pong with same payload
                queueFrame(WS_OP_PONG, payload);
                recv_queue.push(<WebSocketFrameInfo>{
                    "type": "ping",
                    "data": payload,
                });
                break;
            }

            case WS_OP_PONG: {
                recv_queue.push(<WebSocketFrameInfo>{
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

                recv_queue.push(<WebSocketFrameInfo>{
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
                # Continuation frames - simplified handling
                recv_queue.push(<WebSocketFrameInfo>{
                    "type": "binary",
                    "data": payload,
                });
                break;
            }
        }
    }

    #! Encodes a frame for sending
    private binary encodeFrame(hash<auto> frame) {
        int opcode = frame.opcode;
        binary payload = frame.data ?? binary();

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

    #! Queues a frame for sending
    private queueFrame(int opcode, auto data) {
        binary payload;
        if (data.typeCode() == NT_STRING) {
            payload = binary(data);
        } else if (data.typeCode() == NT_BINARY) {
            payload = data;
        } else if (data) {
            payload = binary(string(data));
        }

        send_queue.push({
            "opcode": opcode,
            "data": payload,
        });
    }

    #! Sends a text message
    sendText(string text) {
        queueFrame(WS_OP_TEXT, text);
    }

    #! Sends a binary message
    sendBinary(binary data) {
        queueFrame(WS_OP_BINARY, data);
    }

    #! Sends a ping frame
    sendPing(*binary data) {
        queueFrame(WS_OP_PING, data ?? binary());
    }

    #! Initiates close handshake
    sendClose(int code = 1000, string reason = "") {
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

    #! Gets the next received frame (non-blocking)
    /** @return frame info hash or NOTHING if no frames available
    */
    *hash<WebSocketFrameInfo> getReceivedFrame() {
        try {
            return recv_queue.get(0ms);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "QUEUE-TIMEOUT") {
                return NOTHING;
            }
            rethrow;
        }
    }

    #! Returns True if there are received frames waiting
    bool hasReceivedFrames() {
        return recv_queue.size() > 0;
    }

    #! Returns the socket
    Socket getSocket() {
        return sock;
    }

    #! Returns the output of the operation
    auto getOutput() {
        return {
            "sock": sock,
            "state": state,
            "close_code": close_code,
            "close_reason": close_reason,
            "error_info": error_info,
        };
    }

    #! Returns the close code
    int getCloseCode() {
        return close_code;
    }

    #! Returns the close reason
    string getCloseReason() {
        return close_reason;
    }

    #! Aborts the operation
    abort() {
        state = STATE_CLOSED;
        try {
            sock.close();
        } catch () {
            # Ignore close errors
        }
    }
}

} # namespace HttpServerAsyncIo
