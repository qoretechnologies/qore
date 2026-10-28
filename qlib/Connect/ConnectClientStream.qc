# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectClientStream.qc Connect client-side stream handle

/*  ConnectClientStream.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Connect call result hashdecl
public hashdecl ConnectCallResult {
    #! Response body (for unary and client-streaming final responses)
    *auto body;
    #! Trailing metadata from the server
    *hash<auto> trailing_metadata;
    #! Error info (if the call failed)
    *hash<ConnectErrorInfo> error;
}

} # namespace Connect

public namespace Connect {

#! Client-side Connect stream handle for streaming RPC calls
/** Wraps an HTTP/2 or HTTP/3 stream handle for Connect streaming
    (server-streaming, client-streaming, and bidirectional).

    @par Server-streaming example:
    @code{.py}
ConnectClientStream stream = client.serverStream("Events", "Subscribe",
    {"topic": "news"});
while (*auto msg = stream.read()) {
    printf("Event: %y\n", msg);
}
hash<ConnectCallResult> result = stream.finish();
    @endcode

    @par Client-streaming example:
    @code{.py}
ConnectClientStream stream = client.clientStream("Upload", "Send");
stream.write({"chunk": "data1"});
stream.write({"chunk": "data2"});
stream.writesDone();
hash<ConnectCallResult> result = stream.finish();
    @endcode
*/
public class ConnectClientStream {
    private {
        #! The HTTP stream handle (HTTP/2 or HTTP/3)
        HttpClientIo::HttpClientStreamHandle m_stream;
        #! The codec for serialization/deserialization
        AbstractConnectCodec m_codec;
        #! Read buffer for accumulating partial frames
        binary m_read_buf = binary();
        #! Whether reads are done (END_STREAM received or end-stream frame parsed)
        bool m_reads_done = False;
        #! Whether writesDone() has been called
        bool m_writes_done = False;
        #! Whether the HTTP status of the response has been checked
        bool m_status_checked = False;
        #! Trailing metadata from EndStreamResponse
        *hash<auto> m_trailing_metadata;
        #! Error from EndStreamResponse
        *hash<ConnectErrorInfo> m_error;
        #! Last response body decoded
        *auto m_last_body;
        #! Maximum message size in bytes (0 = no limit)
        int m_max_message_size;
        #! Compression encoding for outgoing messages (client→server)
        *string m_send_encoding;
        #! Compression encoding for incoming messages (server→client)
        *string m_recv_encoding;
    }

    #! Create a client stream
    /** @param stream the HTTP stream handle (already in streaming mode)
        @param codec the codec for message serialization
        @param writes_done if True, writes are already complete (server-streaming)
        @param max_message_size maximum message size in bytes (0 = no limit);
        defaults to @ref CONNECT_DEFAULT_MAX_MESSAGE_SIZE
        @param send_encoding optional compression encoding for outgoing messages
        (client→server); when set, all written messages are compressed
        @param recv_encoding optional compression encoding for incoming messages
        (server→client); when set, compressed frames from the server are decompressed
    */
    constructor(HttpClientIo::HttpClientStreamHandle stream,
            AbstractConnectCodec codec, bool writes_done = False,
            int max_message_size = CONNECT_DEFAULT_MAX_MESSAGE_SIZE,
            *string send_encoding, *string recv_encoding) {
        m_stream = stream;
        m_codec = codec;
        m_writes_done = writes_done;
        m_max_message_size = max_message_size;
        m_send_encoding = send_encoding;
        m_recv_encoding = recv_encoding;
    }

    #! Read the next message from the stream
    /** @param timeout_ms optional read timeout
        @return a deserialized message, or NOTHING if the stream has ended

        @throw CONNECT-STREAM-ERROR if a stream error occurs
        @throw CONNECT-ERROR the server answered the request with an HTTP error status (\c arg is the
        @ref ConnectErrorInfo)
    */
    *auto read(*timeout timeout_ms) {
        if (m_reads_done) {
            return NOTHING;
        }
        if (!timeout_ms) {
            timeout_ms = 30s;
        }

        while (True) {
            # Try to parse a complete Connect frame from buffer
            *auto msg = tryParseFrame();
            if (exists msg) {
                m_last_body = msg;
                return msg;
            }
            # Check if tryParseFrame set reads_done (end-stream frame)
            if (m_reads_done) {
                return NOTHING;
            }

            # Need more data
            auto data = m_stream.readData(timeout_ms);
            if (!exists data) {
                # Timeout
                return NOTHING;
            }
            if (data.typeCode() == NT_HASH) {
                if (exists data.status_code && !m_status_checked) {
                    # the response headers precede the response body
                    m_status_checked = True;
                    if (data.status_code >= 300) {
                        m_reads_done = True;
                        throwHttpError(data.status_code, timeout_ms, NOTHING, data.end_stream ?? False);
                    }
                }
                if (data.end_stream) {
                    m_reads_done = True;
                    # Try to parse remaining buffer
                    *auto remaining = tryParseFrame();
                    if (exists remaining) {
                        m_last_body = remaining;
                    }
                    return remaining;
                }
                if (data.error) {
                    throw "CONNECT-STREAM-ERROR", sprintf("stream error: %y", data.error);
                }
                if (exists data.status_code) {
                    continue;
                }
                return NOTHING;
            }

            if (!m_status_checked) {
                # the response headers can be delivered with the first body data (HTTP/3), in which case only the
                # response hash of the stream has the status
                m_status_checked = True;
                *int status = m_stream.getResponse().status_code;
                if (status >= 300) {
                    m_reads_done = True;
                    throwHttpError(status, timeout_ms, data, False);
                }
            }

            # Append data to buffer
            if (data.typeCode() == NT_BINARY) {
                m_read_buf += data;
            } else {
                m_read_buf += binary(data);
            }
            # Validate buffer size to prevent unbounded accumulation
            if (m_max_message_size > 0 && m_read_buf.size() > 5 + m_max_message_size) {
                throw "CONNECT-MESSAGE-SIZE-ERROR",
                    sprintf("read buffer size %d exceeds maximum message size %d bytes",
                        m_read_buf.size(), m_max_message_size);
            }
        }
    }

    #! Write a message to the stream
    /** @param message the message to send

        @throw CONNECT-STREAM-ERROR if the stream is not writable
    */
    write(auto message) {
        if (m_writes_done) {
            throw "CONNECT-STREAM-ERROR", "writes already done";
        }
        binary encoded = m_codec.serialize(message);
        int flags = 0;
        if (m_send_encoding) {
            encoded = ConnectCompression::compress(encoded, m_send_encoding);
            flags = CONNECT_FLAG_COMPRESSED;
        }
        binary framed = ConnectHelper::frameMessage(encoded, flags);
        m_stream.sendData(framed, False);
    }

    #! Signal that no more messages will be written
    /** Sends END_STREAM on the client side. Required before the server
        can send its response for client-streaming calls.

        Safe to call multiple times; subsequent calls are no-ops.
    */
    writesDone() {
        if (!m_writes_done) {
            m_writes_done = True;
            if (!m_stream.isDone()) {
                m_stream.sendData(binary(), True);
            }
        }
    }

    #! Get the final call result
    /** Automatically calls writesDone() if not already called.
        Reads any remaining messages and extracts the result from the
        EndStreamResponse.

        @return a ConnectCallResult with response body, metadata, and error info
    */
    hash<ConnectCallResult> finish() {
        writesDone();

        # Drain remaining messages
        try {
            while (!m_reads_done) {
                read();
            }
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "THREAD-CANCELLED" || ex.err == "PROGRAM-INTERRUPTED") {
                rethrow;
            }
            m_reads_done = True;
            if (!m_error) {
                m_error = <ConnectErrorInfo>{
                    "code": CONNECT_ERROR_INTERNAL,
                    "message": sprintf("%s: %s", ex.err, ex.desc),
                };
            }
        }

        return <ConnectCallResult>{
            "body": m_last_body,
            "trailing_metadata": m_trailing_metadata,
            "error": m_error,
        };
    }

    #! Returns True if the stream has received all server messages
    /** @return True if reads are done
    */
    bool readsDone() {
        return m_reads_done;
    }

    #! Cancel the stream
    /** Sends RST_STREAM to abort. Any unread messages are discarded.
    */
    cancel() {
        m_reads_done = True;
        m_stream.cancel();
    }

    #! Reads the error body of a response with an HTTP error status and throws it as a Connect error
    /** The server answers a request that it rejects before the stream starts (for example, an unknown method, or a
        request body over its size limit) with an HTTP error status and a JSON Connect error body instead of a
        streaming response.

        @param status the HTTP status code of the response
        @param timeout_ms the read timeout
        @param initial_body response body data already read, if any
        @param ended @ref True if the end of the response has already been read

        @throw CONNECT-ERROR always; \c arg is the @ref ConnectErrorInfo
    */
    private throwHttpError(int status, timeout timeout_ms, *data initial_body, bool ended) {
        binary body = exists initial_body ? binary(initial_body) : binary();
        while (!ended) {
            auto data = m_stream.readData(timeout_ms);
            if (!exists data || (data.typeCode() == NT_HASH && (data.end_stream || data.error))) {
                break;
            }
            if (data.typeCode() == NT_HASH) {
                continue;
            }
            body += data.typeCode() == NT_BINARY ? data : binary(data);
            # an error body is small; do not accumulate an arbitrarily large one, also when messages have no size
            # limit
            if (body.size() > (m_max_message_size > 0 ? m_max_message_size : CONNECT_DEFAULT_MAX_MESSAGE_SIZE)) {
                body = binary();
                break;
            }
        }

        *hash<ConnectErrorInfo> err;
        if (body) {
            try {
                err = ConnectError::parseError(body.toString("utf-8"));
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == "THREAD-CANCELLED" || ex.err == "PROGRAM-INTERRUPTED") {
                    rethrow;
                }
                # not a Connect error body: the error is built from the HTTP status
            }
        }
        if (!err) {
            err = <ConnectErrorInfo>{
                "code": ConnectError::errorForHttpStatus(status),
                "message": sprintf("HTTP %d", status),
            };
        }
        m_error = err;
        throw "CONNECT-ERROR", err.message ?? err.code, err;
    }

    #! Try to parse a complete Connect frame from the read buffer
    /** Handles both regular data frames and end-of-stream frames.
        For end-of-stream frames, parses the EndStreamResponse JSON
        and extracts trailing metadata and error.

        @return the deserialized message from a data frame, or NOTHING
    */
    private *auto tryParseFrame() {
        if (m_read_buf.size() < 5) {
            return NOTHING;
        }
        int flags = m_read_buf[0];
        int len = (m_read_buf[1] << 24) | (m_read_buf[2] << 16)
            | (m_read_buf[3] << 8) | m_read_buf[4];
        if (m_max_message_size > 0 && len > m_max_message_size) {
            throw "CONNECT-MESSAGE-SIZE-ERROR",
                sprintf("Connect frame payload size %d exceeds maximum %d bytes",
                    len, m_max_message_size);
        }
        if (m_read_buf.size() < 5 + len) {
            return NOTHING;
        }
        # Extract the frame payload
        binary payload = m_read_buf.substr(5, len);
        # Remove consumed bytes from buffer
        if (m_read_buf.size() > 5 + len) {
            m_read_buf = m_read_buf.substr(5 + len);
        } else {
            m_read_buf = binary();
        }

        # Check for end-of-stream flag
        if (flags & CONNECT_FLAG_END_STREAM) {
            # Parse EndStreamResponse JSON
            hash<ConnectEndStreamInfo> end_info = ConnectHelper::parseEndStream(payload);
            m_trailing_metadata = end_info.metadata;
            m_error = end_info.error;
            m_reads_done = True;
            return NOTHING;
        }

        # Decompress if needed
        if ((flags & CONNECT_FLAG_COMPRESSED) && m_recv_encoding) {
            payload = ConnectCompression::decompress(payload, m_recv_encoding);
            if (m_max_message_size > 0 && payload.size() > m_max_message_size) {
                throw "CONNECT-MESSAGE-SIZE-ERROR",
                    sprintf("decompressed payload size %d exceeds maximum %d bytes",
                        payload.size(), m_max_message_size);
            }
        }

        # Regular data frame — deserialize with codec
        return m_codec.deserialize(payload);
    }
}

} # namespace Connect
