# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectServerStream.qc Connect server-side stream handle

/*  ConnectServerStream.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Server-side Connect stream handle for streaming RPC patterns
/** Provides framed message reading and writing on the server side.
    Used for server-streaming, client-streaming, and bidirectional-streaming
    handler implementations.

    Pre-parsed client messages are returned by @ref read() in the order they
    were received; the stream is constructed only after the request body has
    been drained, so @ref read() never blocks.

    @par Example (server-streaming handler):
    @code{.py}
handler.registerServerStream("MyService", "ServerStream",
    sub(auto request, ConnectServerStream stream, hash<auto> metadata) {
        for (int i = 0; i < 5; ++i) {
            stream.write({"message": sprintf("msg %d", i)});
        }
    });
    @endcode

    @par Example (client-streaming handler):
    @code{.py}
handler.registerClientStream("MyService", "ClientStream",
    auto sub(ConnectServerStream stream, hash<auto> metadata) {
        list<string> names;
        while (*auto msg = stream.read()) {
            names += msg.name;
        }
        return {"names": names};
    });
    @endcode
*/
public class ConnectServerStream {
    private {
        #! The underlying Socket
        Socket m_socket;
        #! The codec for serialization/deserialization
        AbstractConnectCodec m_codec;
        #! The HTTP/2 stream ID
        int m_stream_id;
        #! Whether this is an HTTP/3 connection
        bool m_http3;
        #! HTTP/3 session ID (HTTP/3 only)
        *int m_session_id;
        #! Whether response headers have been sent
        bool m_headers_sent = False;
        #! Pre-parsed messages
        list<auto> m_messages = ();
        #! Current read position in pre-parsed messages
        int m_read_pos = 0;
        #! Custom trailing metadata set by the handler
        hash<auto> m_trailing_metadata();
        #! Maximum message size in bytes (0 = no limit)
        int m_max_message_size;
        #! Compression encoding for outgoing messages (server→client)
        *string m_send_encoding;
        #! Compression encoding for incoming messages (client→server)
        *string m_recv_encoding;

        #! Optional I/O-controller wake closure
        /** Forwarded by @ref ConnectHandler from @c cx."header-info".io_wake.
            When set, @ref sendStreamData() / @ref ensureHeadersSent() call
            the @c *Async Socket methods and then invoke this closure so the
            I/O thread flushes pending frames promptly.  The only HTTP/2
            code path on the server side once the sync H2 APIs are removed.
        */
        *code<nothing()> m_io_wake;
    }

    #! Create a server stream
    /** @param socket the client Socket
        @param codec the codec for message serialization
        @param stream_id the HTTP/2 stream ID
        @param http3 whether this is an HTTP/3 connection
        @param session_id the HTTP/3 session ID (only used for HTTP/3)
        @param messages pre-parsed list of client messages
        @param max_message_size maximum message size in bytes (0 = no limit);
        defaults to @ref CONNECT_DEFAULT_MAX_MESSAGE_SIZE
        @param send_encoding optional compression encoding for outgoing messages
        (server→client); when set, all written messages are compressed
        @param recv_encoding optional compression encoding for incoming messages
        (client→server); when set, compressed frames from the client are decompressed
        @param io_wake optional async I/O-thread wake closure; when set, write
        paths invoke it after enqueuing HTTP/2 frames so the I/O thread flushes
        promptly
    */
    constructor(Socket socket, AbstractConnectCodec codec, int stream_id,
            bool http3, *int session_id, list<auto> messages = (),
            int max_message_size = CONNECT_DEFAULT_MAX_MESSAGE_SIZE,
            *string send_encoding, *string recv_encoding,
            *code<nothing()> io_wake) {
        m_socket = socket;
        m_codec = codec;
        m_stream_id = stream_id;
        m_http3 = http3;
        m_session_id = session_id;
        m_messages = messages;
        m_max_message_size = max_message_size;
        m_send_encoding = send_encoding;
        m_recv_encoding = recv_encoding;
        m_io_wake = io_wake;
    }

    #! Read the next client message
    /** Returns pre-parsed messages one at a time.

        @param timeout_ms ignored; retained for API compatibility

        @return the next deserialized message, or NOTHING when all messages
        have been read
    */
    *auto read(*timeout timeout_ms) {
        if (m_read_pos >= m_messages.size()) {
            return NOTHING;
        }
        return m_messages[m_read_pos++];
    }

    #! Write a response message to the client
    /** For server-streaming and bidirectional-streaming handlers.
        Serializes, frames, and sends a Connect message.

        @param message the message data to send
    */
    write(auto message) {
        ensureHeadersSent();
        binary encoded = m_codec.serialize(message);
        int flags = 0;
        if (m_send_encoding) {
            encoded = ConnectCompression::compress(encoded, m_send_encoding);
            flags = CONNECT_FLAG_COMPRESSED;
        }
        binary framed = ConnectHelper::frameMessage(encoded, flags);
        sendStreamData(framed, False);
    }

    #! Check if all client messages have been consumed
    /** @return True if all messages have been read
    */
    bool readsDone() {
        return m_read_pos >= m_messages.size();
    }

    #! Set custom trailing metadata to send with the response
    /** @param metadata the trailing metadata key-value pairs
    */
    setTrailingMetadata(hash<auto> metadata) {
        map m_trailing_metadata{$1.key} = $1.value, metadata.pairIterator();
    }

    #! Get the custom trailing metadata set by the handler
    /** @return the custom trailing metadata
    */
    hash<auto> getTrailingMetadata() {
        return m_trailing_metadata;
    }

    #! Ensure response headers have been sent
    /** Called automatically by write(), but also called by the handler
        before sending the EndStreamResponse in case the handler wrote nothing.
    */
    ensureHeadersSent() {
        if (!m_headers_sent) {
            hash<auto> headers = {
                "content-type": ConnectStreamContentTypePrefix + m_codec.name(),
            };
            if (m_send_encoding) {
                headers{ConnectContentEncodingHeader} = m_send_encoding;
            }
            if (m_http3) {
                m_socket.submitQuicResponseStreaming(m_session_id, m_stream_id, 200, headers);
            } else {
                m_socket.submitHttp2StreamingResponseHeaders(m_stream_id, 200, headers);
                if (m_io_wake) {
                    m_io_wake();
                }
            }
            m_headers_sent = True;
        }
    }

    #! Send data on the transport stream
    private sendStreamData(binary data, bool end_stream) {
        if (m_http3) {
            while (m_socket.submitQuicStreamData(m_session_id, m_stream_id, data,
                    end_stream) == 1) {
                m_socket.waitForQuicStreamDrain(m_session_id, m_stream_id, 5s);
            }
        } else {
            # Async send — the I/O thread flushes on the next tick, or
            # immediately if m_io_wake nudges it.
            m_socket.sendHttp2StreamData(m_stream_id, data, end_stream);
            if (m_io_wake) {
                m_io_wake();
            }
        }
    }
}

} # namespace Connect
