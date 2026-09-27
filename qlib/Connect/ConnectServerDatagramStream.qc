# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectServerDatagramStream.qc Connect server-side datagram stream

/*  ConnectServerDatagramStream.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace Connect {

#! Server-side datagram stream for unreliable message exchange over QUIC datagrams
/** Provides the server-side API for datagram-based RPC communication.
    Used by ConnectHandler to service datagram fire-and-forget and bidirectional
    datagram RPC methods.

    The underlying HTTP/3 anchor stream stays open for the lifetime of the
    datagram channel. All payloads flow as QUIC datagrams via the quarter-stream-ID.

    @par Example (bidi datagram handler):
    @code{.py}
handler.registerDatagramBidi("Sensor", "Exchange",
    sub (ConnectServerDatagramStream stream, hash<auto> metadata) {
        while (*auto msg = stream.read(30s)) {
            stream.send({"echo": msg});
        }
    });
    @endcode

    @note Datagram support requires HTTP/3 (QUIC).
*/
public class ConnectServerDatagramStream {
    private {
        #! The underlying Socket
        Socket m_socket;
        #! The codec for serialization/deserialization
        AbstractConnectCodec m_codec;
        #! The HTTP/3 stream ID (anchor stream)
        int m_stream_id;
        #! The HTTP/3 session ID
        *int m_session_id;
        #! Maximum datagram message size in bytes
        int m_max_message_size;
        #! Wrapping sequence number (0-255)
        int m_seq = 0;
        #! Optional I/O wake callback (wakes the I/O controller to flush writes)
        *code<nothing()> m_io_wake;
        #! Cached max payload size (negotiated at connection setup, doesn't change)
        int m_cached_max_payload_size;
    }

    #! Create a server-side datagram stream
    /** @param socket the client Socket
        @param codec the codec for serialization
        @param stream_id the HTTP/3 anchor stream ID
        @param session_id the HTTP/3 session ID
        @param max_message_size maximum message size in bytes (0 = no limit);
        defaults to @ref CONNECT_DEFAULT_MAX_MESSAGE_SIZE
        @param io_wake optional callback to wake the I/O controller after writes
    */
    constructor(Socket socket, AbstractConnectCodec codec, int stream_id,
            *int session_id,
            int max_message_size = CONNECT_DEFAULT_MAX_MESSAGE_SIZE,
            *code<nothing()> io_wake) {
        m_socket = socket;
        m_codec = codec;
        m_stream_id = stream_id;
        m_session_id = session_id;
        m_max_message_size = max_message_size;
        m_io_wake = io_wake;
        # Cache max payload size — it's negotiated at connection setup and won't change
        m_cached_max_payload_size = m_socket.isQuicDatagramSupported(m_session_id)
            ? m_socket.getQuicMaxDatagramSize(m_session_id, m_stream_id) : 0;
    }

    #! Send an unreliable datagram message
    /** @param message the message data to send

        @throw CONNECT-DATAGRAM-SIZE-ERROR if the serialized message exceeds
        the maximum datagram size
    */
    nothing send(auto message) {
        binary encoded = m_codec.serialize(message);
        binary framed = ConnectHelper::frameDatagramMessage(encoded, 0, m_seq);
        m_seq = (m_seq + 1) & 0xFF;

        # Check size against cached max datagram size
        if (m_cached_max_payload_size > 0 && framed.size() > m_cached_max_payload_size) {
            throw "CONNECT-DATAGRAM-SIZE-ERROR",
                sprintf("datagram size %d exceeds maximum %d bytes",
                    framed.size(), m_cached_max_payload_size);
        }

        m_socket.submitQuicDatagram(m_session_id, m_stream_id, framed);
        if (m_io_wake) {
            m_io_wake();
        }
    }

    #! Send an error datagram
    /** @param error the error info to send
    */
    nothing sendError(hash<ConnectErrorInfo> error) {
        string err_json = ConnectError::serializeError(error);
        binary err_payload = binary(err_json);
        binary framed = ConnectHelper::frameDatagramMessage(err_payload,
            CONNECT_FLAG_DATAGRAM_ERROR, m_seq);
        m_seq = (m_seq + 1) & 0xFF;
        m_socket.submitQuicDatagram(m_session_id, m_stream_id, framed);
        if (m_io_wake) {
            m_io_wake();
        }
    }

    #! Read the next incoming datagram
    /** @param timeout_ms read timeout (default 30s)
        @return the deserialized message, or NOTHING on timeout

        @throw CONNECT-DATAGRAM-ERROR if the remote side sent an error datagram
        @throw CONNECT-DATAGRAM-SIZE-ERROR if the received payload exceeds the
        maximum message size
    */
    *auto read(*timeout timeout_ms) {
        if (!timeout_ms) {
            timeout_ms = 30s;
        }

        *binary raw = m_socket.readQuicDatagram(m_session_id, m_stream_id, timeout_ms);
        if (!exists raw) {
            return NOTHING;
        }

        hash<ConnectDatagramFrameInfo> frame = ConnectHelper::unframeDatagramMessage(raw);

        # Check for error flag
        if (frame.flags & CONNECT_FLAG_DATAGRAM_ERROR) {
            string err_json = frame.data.toString("utf-8");
            *hash<ConnectErrorInfo> err;
            try {
                err = ConnectError::parseError(err_json);
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == "THREAD-CANCELLED" || ex.err == "PROGRAM-INTERRUPTED") {
                    rethrow;
                }
            }
            if (!err) {
                err = <ConnectErrorInfo>{
                    "code": CONNECT_ERROR_INTERNAL,
                    "message": "unknown datagram error",
                };
            }
            throw "CONNECT-DATAGRAM-ERROR", err.message ?? err.code, err;
        }

        # Decompress if needed
        binary payload = frame.data;
        if (frame.flags & CONNECT_FLAG_COMPRESSED) {
            payload = ConnectCompression::decompress(payload, "gzip");
        }

        if (m_max_message_size > 0 && payload.size() > m_max_message_size) {
            throw "CONNECT-DATAGRAM-SIZE-ERROR",
                sprintf("datagram payload size %d exceeds maximum %d bytes",
                    payload.size(), m_max_message_size);
        }

        return m_codec.deserialize(payload);
    }

    #! Get the maximum payload size for a single datagram
    /** @return the maximum payload size in bytes, or 0 if unknown
    */
    int getMaxPayloadSize() {
        return m_cached_max_payload_size;
    }
}

} # namespace Connect
