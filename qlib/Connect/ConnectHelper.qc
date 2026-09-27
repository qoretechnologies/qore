# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectHelper.qc Connect protocol framing and utilities

/*  ConnectHelper.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Connect datagram frame info hashdecl
public hashdecl ConnectDatagramFrameInfo {
    #! Frame flags (combination of CONNECT_FLAG_* constants)
    int flags;
    #! Sequence number (0-255, wrapping application-level ordering hint)
    int seq;
    #! Frame payload data
    binary data;
}

#! Connect frame info hashdecl
public hashdecl ConnectFrameInfo {
    #! Frame flags (combination of CONNECT_FLAG_* constants)
    int flags;
    #! Frame payload data
    binary data;
}

#! Connect end-of-stream info hashdecl
public hashdecl ConnectEndStreamInfo {
    #! Optional trailing metadata
    *hash<auto> metadata;
    #! Optional error (present if the stream ended with an error)
    *hash<ConnectErrorInfo> error;
}

#! Connect call options hashdecl
public hashdecl ConnectCallOptions {
    #! Request timeout in milliseconds
    *int timeout_ms;
    #! Request metadata (sent as HTTP headers)
    *hash<string, string> metadata;
    #! Content encoding (e.g., \c "gzip")
    *string encoding;
}

#! RPC type constants
public const CONNECT_RPC_UNARY = 0;

#! Server-streaming RPC type
public const CONNECT_RPC_SERVER_STREAM = 1;

#! Client-streaming RPC type
public const CONNECT_RPC_CLIENT_STREAM = 2;

#! Bidirectional streaming RPC type
public const CONNECT_RPC_BIDI = 3;

#! Datagram fire-and-forget RPC type (client→server, no response)
public const CONNECT_RPC_DATAGRAM_FIRE_AND_FORGET = 4;

#! Datagram bidirectional RPC type (unreliable both directions)
public const CONNECT_RPC_DATAGRAM_BIDI = 5;

#! Content-Type prefix for unary requests: application/{codec}
public const ConnectContentTypePrefix = "application/";

#! Content-Type prefix for streaming requests: application/connect+{codec}
public const ConnectStreamContentTypePrefix = "application/connect+";

#! Content-Type prefix for datagram requests: application/connect+datagram+{codec}
public const ConnectDatagramContentTypePrefix = "application/connect+datagram+";

#! Connect protocol version header name
public const ConnectProtocolVersionHeader = "connect-protocol-version";

#! Connect protocol version value
public const ConnectProtocolVersion = "1";

#! Connect timeout header name
public const ConnectTimeoutHeader = "connect-timeout-ms";

#! Connect content encoding header for streaming
public const ConnectContentEncodingHeader = "connect-content-encoding";

#! Connect accept encoding header for streaming
public const ConnectAcceptEncodingHeader = "connect-accept-encoding";

#! Envelope flag: payload is compressed (bit 0)
public const CONNECT_FLAG_COMPRESSED = 0x01;

#! Envelope flag: end-of-stream marker (bit 1)
public const CONNECT_FLAG_END_STREAM = 0x02;

#! Datagram envelope flag: request application-level ACK (bit 2)
public const CONNECT_FLAG_DATAGRAM_ACK_REQ = 0x04;

#! Datagram envelope flag: payload is an error (bit 3)
public const CONNECT_FLAG_DATAGRAM_ERROR = 0x08;

#! Capsule-Protocol header name (RFC 9297)
public const ConnectCapsuleProtocolHeader = "capsule-protocol";

#! Default maximum message size: 4 MB (matches gRPC default)
/** Used to prevent unbounded memory allocation from malicious or malformed
    frames that declare very large payload sizes.
*/
public const CONNECT_DEFAULT_MAX_MESSAGE_SIZE = 4 * 1024 * 1024;

#! Connect protocol framing and header utility class
/** Provides static methods for Connect message framing, header construction,
    and EndStreamResponse serialization.

    Connect message framing format (same 5-byte envelope as gRPC):
    - 1 byte: flags (bit 0 = compressed, bit 1 = end-of-stream)
    - 4 bytes: big-endian payload length
    - N bytes: payload data
*/
public class ConnectHelper {
    #! Frame a binary message with the Connect 5-byte envelope header
    /** @param payload the message payload
        @param flags envelope flags (combination of \c CONNECT_FLAG_* constants)
        @return the framed message (5-byte header + payload)
    */
    static binary frameMessage(binary payload, int flags = 0) {
        int len = payload.size();
        string header_str = chr(flags & 0xFF);
        header_str += chr((len >> 24) & 0xFF);
        header_str += chr((len >> 16) & 0xFF);
        header_str += chr((len >> 8) & 0xFF);
        header_str += chr(len & 0xFF);
        return binary(header_str) + payload;
    }

    #! Parse a Connect-framed message
    /** @param data the framed message data (must be >= 5 bytes)
        @param max_message_size optional maximum payload size in bytes; 0 means no limit;
        if not provided, no size validation is performed (caller is responsible)

        @return the frame info with flags and payload

        @throw CONNECT-FRAMING-ERROR if the data is too short or declares more bytes
        than available
        @throw CONNECT-MESSAGE-SIZE-ERROR if the declared payload exceeds max_message_size
    */
    static hash<ConnectFrameInfo> unframeMessage(binary data, *int max_message_size) {
        if (data.size() < 5) {
            throw "CONNECT-FRAMING-ERROR",
                sprintf("Connect frame too short: %d bytes (minimum 5)", data.size());
        }
        int flags = data[0];
        int len = (data[1] << 24) | (data[2] << 16) | (data[3] << 8) | data[4];
        if (max_message_size > 0 && len > max_message_size) {
            throw "CONNECT-MESSAGE-SIZE-ERROR",
                sprintf("Connect frame payload size %d exceeds maximum %d bytes",
                    len, max_message_size);
        }
        if (data.size() < 5 + len) {
            throw "CONNECT-FRAMING-ERROR",
                sprintf("Connect frame declares %d bytes but only %d available",
                    len, data.size() - 5);
        }
        return <ConnectFrameInfo>{
            "flags": flags,
            "data": data.substr(5, len),
        };
    }

    #! Parse multiple concatenated Connect-framed messages
    /** @param data the buffer containing one or more framed messages
        @param max_message_size optional maximum payload size in bytes per frame; 0 means no limit

        @return a list of frame info hashes

        @throw CONNECT-FRAMING-ERROR if any frame is malformed
        @throw CONNECT-MESSAGE-SIZE-ERROR if any frame payload exceeds max_message_size
    */
    static list<hash<ConnectFrameInfo>> unframeMessages(binary data, *int max_message_size) {
        list<hash<ConnectFrameInfo>> messages = ();
        int offset = 0;
        while (offset < data.size()) {
            if (data.size() - offset < 5) {
                throw "CONNECT-FRAMING-ERROR",
                    sprintf("incomplete Connect frame header at offset %d: %d bytes remaining",
                        offset, data.size() - offset);
            }
            int flags = data[offset];
            int len = (data[offset + 1] << 24) | (data[offset + 2] << 16)
                | (data[offset + 3] << 8) | data[offset + 4];
            if (max_message_size > 0 && len > max_message_size) {
                throw "CONNECT-MESSAGE-SIZE-ERROR",
                    sprintf("Connect frame payload size %d at offset %d exceeds maximum %d bytes",
                        len, offset, max_message_size);
            }
            if (data.size() - offset - 5 < len) {
                throw "CONNECT-FRAMING-ERROR",
                    sprintf("incomplete Connect frame at offset %d: declares %d bytes but only "
                        "%d available", offset, len, data.size() - offset - 5);
            }
            messages += <ConnectFrameInfo>{
                "flags": flags,
                "data": data.substr(offset + 5, len),
            };
            offset += 5 + len;
        }
        return messages;
    }

    #! Get the size of the next complete Connect frame in a buffer
    /** @param buf the buffer to check
        @return the total frame size (5 + payload), or -1 if the buffer does not
        contain a complete frame
    */
    static int nextFrameSize(binary buf) {
        if (buf.size() < 5) {
            return -1;
        }
        int len = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];
        int total = 5 + len;
        if (buf.size() < total) {
            return -1;
        }
        return total;
    }

    #! Frame an EndStreamResponse message
    /** The EndStreamResponse is always JSON per the Connect spec, regardless of
        the codec used for messages. It is sent as the final frame in a streaming
        response with the \c CONNECT_FLAG_END_STREAM flag set.

        @param metadata optional trailing metadata
        @param error optional error info

        @return the framed EndStreamResponse (5-byte header + JSON payload)
    */
    static binary frameEndStream(*hash<auto> metadata, *hash<ConnectErrorInfo> error) {
        hash<auto> obj();
        if (metadata) {
            obj.metadata = metadata;
        }
        if (error) {
            hash<auto> err_obj = {"code": error.code};
            if (error.message.val()) {
                err_obj.message = error.message;
            }
            obj.error = err_obj;
        }
%ifdef NoJson
        string json = ConnectError::simpleJsonEncode(obj);
%else
        string json = make_json(obj);
%endif
        return ConnectHelper::frameMessage(binary(json), CONNECT_FLAG_END_STREAM);
    }

    #! Parse an EndStreamResponse message from a frame payload
    /** @param data the frame payload (JSON-encoded EndStreamResponse)
        @return the parsed end-of-stream info
    */
    static hash<ConnectEndStreamInfo> parseEndStream(binary data) {
        string json = data.toString("utf-8");
        hash<auto> obj;
%ifdef NoJson
        obj = ConnectError::simpleJsonDecode(json);
%else
        obj = parse_json(json);
%endif
        hash<ConnectEndStreamInfo> result();
        if (obj.metadata) {
            result.metadata = obj.metadata;
        }
        if (obj.error) {
            result.error = <ConnectErrorInfo>{
                "code": obj.error.code,
                "message": obj.error.message,
            };
        }
        return result;
    }

    #! Frame a datagram message with the 2-byte envelope (flags + seq)
    /** Datagram envelope format (minimal for low-latency):
        - Byte 0: flags (\c CONNECT_FLAG_COMPRESSED, \c CONNECT_FLAG_DATAGRAM_ACK_REQ,
          \c CONNECT_FLAG_DATAGRAM_ERROR)
        - Byte 1: sequence number (0-255, wrapping)
        - Bytes 2+: codec-encoded payload

        No length field — QUIC datagrams are self-delimiting.

        @param payload the message payload
        @param flags envelope flags (combination of \c CONNECT_FLAG_* constants)
        @param seq application-level sequence number (0-255)
        @return the framed datagram (2-byte header + payload)
    */
    static binary frameDatagramMessage(binary payload, int flags = 0, int seq = 0) {
        string header_str = chr(flags & 0xFF);
        header_str += chr(seq & 0xFF);
        return binary(header_str) + payload;
    }

    #! Parse a datagram-framed message
    /** @param data the framed datagram data (must be >= 2 bytes)
        @return the datagram frame info with flags, sequence number, and payload

        @throw CONNECT-FRAMING-ERROR if the data is too short
    */
    static hash<ConnectDatagramFrameInfo> unframeDatagramMessage(binary data) {
        if (data.size() < 2) {
            throw "CONNECT-FRAMING-ERROR",
                sprintf("Connect datagram frame too short: %d bytes (minimum 2)", data.size());
        }
        return <ConnectDatagramFrameInfo>{
            "flags": data[0],
            "seq": data[1],
            "data": data.size() > 2 ? data.substr(2) : binary(),
        };
    }

    #! Build HTTP headers for a datagram channel setup request
    /** @param codec_name the codec name (e.g., \c "yaml")
        @param opts optional call options
        @return the HTTP headers hash
    */
    static hash<string, string> buildDatagramRequestHeaders(string codec_name,
            *hash<ConnectCallOptions> opts) {
        hash<string, string> headers = {
            "content-type": ConnectDatagramContentTypePrefix + codec_name,
            ConnectProtocolVersionHeader: ConnectProtocolVersion,
            ConnectCapsuleProtocolHeader: "?1",
        };
        if (opts.timeout_ms) {
            headers{ConnectTimeoutHeader} = string(opts.timeout_ms);
        }
        if (opts.metadata) {
            map headers{$1.key} = $1.value, opts.metadata.pairIterator();
        }
        return headers;
    }

    #! Build HTTP headers for a unary Connect request
    /** @param codec_name the codec name (e.g., \c "yaml")
        @param opts optional call options

        @return the HTTP headers hash
    */
    static hash<string, string> buildUnaryRequestHeaders(string codec_name,
            *hash<ConnectCallOptions> opts) {
        hash<string, string> headers = {
            "content-type": ConnectContentTypePrefix + codec_name,
            ConnectProtocolVersionHeader: ConnectProtocolVersion,
        };
        if (opts.timeout_ms) {
            headers{ConnectTimeoutHeader} = string(opts.timeout_ms);
        }
        if (opts.encoding) {
            headers."content-encoding" = opts.encoding;
        }
        if (opts.metadata) {
            map headers{$1.key} = $1.value, opts.metadata.pairIterator();
        }
        return headers;
    }

    #! Build HTTP headers for a streaming Connect request
    /** @param codec_name the codec name (e.g., \c "yaml")
        @param opts optional call options

        @return the HTTP headers hash
    */
    static hash<string, string> buildStreamRequestHeaders(string codec_name,
            *hash<ConnectCallOptions> opts) {
        hash<string, string> headers = {
            "content-type": ConnectStreamContentTypePrefix + codec_name,
            ConnectProtocolVersionHeader: ConnectProtocolVersion,
        };
        if (opts.timeout_ms) {
            headers{ConnectTimeoutHeader} = string(opts.timeout_ms);
        }
        if (opts.encoding) {
            headers{ConnectContentEncodingHeader} = opts.encoding;
            # Also advertise that we accept compressed responses
            headers{ConnectAcceptEncodingHeader} = ConnectCompression::acceptEncodingValue();
        }
        if (opts.metadata) {
            map headers{$1.key} = $1.value, opts.metadata.pairIterator();
        }
        return headers;
    }

    #! Extract the codec name from a Content-Type header value
    /** @param content_type the Content-Type header value
        @return a hash with \c codec_name and \c streaming keys, or NOTHING
        if the content type is not a valid Connect content type
    */
    static *hash<auto> parseContentType(string content_type) {
        # Strip parameters (;charset=... etc)
        int semi = content_type.find(";");
        if (semi >= 0) {
            content_type = trim(content_type.substr(0, semi));
        }
        content_type = content_type.lwr();

        # Check datagram prefix first (longest match)
        if (content_type =~ /^application\/connect\+datagram\+/) {
            string codec_name = content_type.substr(ConnectDatagramContentTypePrefix.size());
            if (codec_name.val()) {
                return {
                    "codec_name": codec_name,
                    "streaming": False,
                    "datagram": True,
                };
            }
        }
        # Check streaming prefix (longer match than unary)
        if (content_type =~ /^application\/connect\+/) {
            string codec_name = content_type.substr(ConnectStreamContentTypePrefix.size());
            if (codec_name.val()) {
                return {
                    "codec_name": codec_name,
                    "streaming": True,
                };
            }
        }
        # Check unary prefix
        if (content_type =~ /^application\//) {
            string codec_name = content_type.substr(ConnectContentTypePrefix.size());
            # Filter out non-codec content types
            if (codec_name.val() && codec_name != "octet-stream"
                    && codec_name !~ /^grpc/) {
                return {
                    "codec_name": codec_name,
                    "streaming": False,
                };
            }
        }
        return NOTHING;
    }
}

} # namespace Connect
