# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectDatagramStream.qc Connect client-side datagram stream

/*  ConnectDatagramStream.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Client-side datagram stream for unreliable message exchange over QUIC datagrams
/** Wraps an HTTP/3 stream handle for datagram-based RPC communication.

    Per RFC 9297, QUIC datagrams are associated with an HTTP/3 request via
    the quarter-stream-ID. The HTTP/3 stream serves as an anchor — no body
    data flows on the stream itself; all payloads are sent as QUIC datagrams.

    Datagram messages use a minimal 2-byte envelope:
    - Byte 0: flags (\c CONNECT_FLAG_COMPRESSED, \c CONNECT_FLAG_DATAGRAM_ACK_REQ,
      \c CONNECT_FLAG_DATAGRAM_ERROR)
    - Byte 1: sequence number (0-255, wrapping)
    - Bytes 2+: codec-encoded payload

    @par Example (fire-and-forget):
    @code{.py}
ConnectClient client("https://localhost:8080", NOTHING,
    <ConnectClientOptions>{"protocol": "h3"});
client.sendDatagram("Telemetry", "Report", {"cpu": 42.5, "mem": 1024});
    @endcode

    @par Example (bidirectional datagrams):
    @code{.py}
ConnectDatagramStream dg = client.datagramBidi("Sensor", "Exchange");
dg.send({"reading": 99.1});
*auto reply = dg.read(5s);
dg.close();
    @endcode

    @note Datagram support requires HTTP/3 (QUIC). Attempting to use datagrams
    over HTTP/2 will throw \c CONNECT-DATAGRAM-NOT-SUPPORTED.
*/
public class ConnectDatagramStream {
    private {
        #! The HTTP/3 stream handle (anchor stream for datagrams)
        HttpClientIo::HttpClientStreamHandle m_stream;
        #! The codec for serialization/deserialization
        AbstractConnectCodec m_codec;
        #! Maximum datagram message size in bytes
        int m_max_message_size;
        #! Wrapping sequence number (0-255)
        int m_seq = 0;
        #! Optional compression encoding for outgoing messages
        *string m_send_encoding;
        #! Whether the channel is open
        bool m_open = True;
        #! Cached max payload size (negotiated at connection setup, doesn't change)
        int m_cached_max_payload_size;
    }

    #! Create a client-side datagram stream
    /** @param stream the HTTP/3 stream handle (anchor stream for datagrams)
        @param codec the codec for serialization
        @param max_message_size maximum datagram payload size in bytes
        @param send_encoding optional compression encoding for outgoing messages
    */
    constructor(HttpClientIo::HttpClientStreamHandle stream, AbstractConnectCodec codec,
            int max_message_size, *string send_encoding) {
        m_stream = stream;
        m_codec = codec;
        m_max_message_size = max_message_size;
        m_send_encoding = send_encoding;
        # Cache max payload size — it's negotiated at connection setup and won't change
        m_cached_max_payload_size = m_stream.isDatagramSupported()
            ? m_stream.getMaxDatagramSize() : 0;
    }

    #! Send an unreliable datagram message
    /** @param message the message data to send

        @throw CONNECT-DATAGRAM-ERROR if the channel is closed
        @throw CONNECT-DATAGRAM-SIZE-ERROR if the serialized message exceeds
        the maximum datagram payload size
    */
    nothing send(auto message) {
        if (!m_open) {
            throw "CONNECT-DATAGRAM-ERROR", "datagram channel is closed";
        }
        binary encoded = m_codec.serialize(message);
        int flags = 0;
        if (m_send_encoding) {
            encoded = ConnectCompression::compress(encoded, m_send_encoding);
            flags = CONNECT_FLAG_COMPRESSED;
        }
        binary framed = ConnectHelper::frameDatagramMessage(encoded, flags, m_seq);
        m_seq = (m_seq + 1) & 0xFF;

        # Check size against cached max datagram size
        if (m_cached_max_payload_size > 0 && framed.size() > m_cached_max_payload_size) {
            throw "CONNECT-DATAGRAM-SIZE-ERROR",
                sprintf("datagram size %d exceeds maximum %d bytes",
                    framed.size(), m_cached_max_payload_size);
        }

        m_stream.sendDatagram(framed);
    }

    #! Read the next incoming datagram
    /** @param timeout_ms read timeout (default 30s)
        @return the deserialized message, or NOTHING on timeout or channel close

        @throw CONNECT-DATAGRAM-ERROR if the remote side sent an error datagram
        @throw CONNECT-DATAGRAM-SIZE-ERROR if the received payload exceeds the
        maximum message size
    */
    *auto read(*timeout timeout_ms) {
        if (!m_open) {
            return NOTHING;
        }
        if (!timeout_ms) {
            timeout_ms = 30s;
        }

        *binary raw = m_stream.readDatagram(timeout_ms);
        if (!exists raw) {
            return NOTHING;
        }

        hash<ConnectDatagramFrameInfo> frame = ConnectHelper::unframeDatagramMessage(raw);

        # Check for error flag
        if (frame.flags & CONNECT_FLAG_DATAGRAM_ERROR) {
            # Error payload is JSON-encoded ConnectErrorInfo
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
            # Use gzip as default decompression if no encoding specified
            payload = ConnectCompression::decompress(payload,
                m_send_encoding ?? "gzip");
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

    #! Check if the datagram channel is still open
    /** @return True if the channel is open
    */
    bool isOpen() {
        return m_open;
    }

    #! Close the datagram channel
    /** Cancels the underlying HTTP/3 anchor stream, which resets the QUIC
        stream and releases the connection resource. The server handler will
        receive a read error or timeout, causing it to exit its message loop.

        This must be called when the datagram channel is no longer needed to
        avoid leaking HTTP/3 streams on persistent connections.
    */
    close() {
        if (m_open) {
            m_open = False;
            m_stream.cancel();
        }
    }

    #! Destructor — ensures the channel is closed
    destructor() {
        close();
    }
}

} # namespace Connect
