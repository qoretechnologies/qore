# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GrpcProtocolAdapter.qc gRPC wire protocol adapter for Connect handler

/*  GrpcProtocolAdapter.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see GrpcProtocolAdapter
#! @see GrpcDetection
public namespace Connect {

#! gRPC protocol detection utilities (always available, no grpc module dependency)
/** Provides static methods for detecting gRPC and gRPC-Web requests from HTTP
    headers and parsing the \c grpc-timeout header format.

    These methods are pure string/regex operations with zero dependency on the
    \c grpc binary module.  They are extracted here so that tests and application
    code can use them even when protobuf support is not compiled in.
*/
public class GrpcDetection {
    #! Detect if request uses gRPC wire protocol
    /** @param hdr the HTTP request headers
        @return True if the Content-Type indicates gRPC (but not gRPC-Web)
    */
    static bool isGrpcRequest(hash<auto> hdr) {
        string ct = hdr."content-type" ?? "";
        return ct =~ /^application\/grpc/ && ct !~ /^application\/grpc-web/;
    }

    #! Detect if request uses gRPC-Web wire protocol
    /** @param hdr the HTTP request headers
        @return True if the Content-Type indicates gRPC-Web
    */
    static bool isGrpcWebRequest(hash<auto> hdr) {
        string ct = hdr."content-type" ?? "";
        return ct =~ /^application\/grpc-web/;
    }

    #! Parse a gRPC timeout header value
    /** @param value the timeout string (e.g., \c "5000m", \c "5S", \c "100u")
        @return the timeout in milliseconds, or NOTHING if the format is invalid
    */
    static *int parseGrpcTimeout(string value) {
        if (value.empty()) {
            return NOTHING;
        }
        string unit = value[-1];
        *string num_str = value.substr(0, value.size() - 1);
        if (!num_str.val()) {
            return NOTHING;
        }
        int num = int(num_str);
        switch (unit) {
            case "H": return num * 3600000;
            case "M": return num * 60000;
            case "S": return num * 1000;
            case "m": return num;
            case "u": return num / 1000;
            case "n": return num / 1000000;
            default: return NOTHING;
        }
    }
}

} # namespace Connect

%ifndef NoGrpc

public namespace Connect {

#! gRPC wire protocol adapter
/** Handles gRPC and gRPC-Web wire format differences when a ConnectHandler
    receives requests with gRPC Content-Types.

    gRPC wire protocol differences from Connect:
    - <b>Always envelope-framed</b> (even unary — 5-byte header + payload)
    - <b>HTTP/2 trailers</b> carry \c grpc-status and \c grpc-message (not JSON error body)
    - <b>No HTTP error status codes</b> — always HTTP 200, errors in trailers
    - <b>grpc-timeout</b> header format: \c "5S", \c "100m", \c "200u" (not milliseconds)
    - <b>gRPC-Web</b>: trailers are base64-encoded and appended to response body
*/
public class GrpcProtocolAdapter {
    #! Handle a gRPC request and produce a gRPC-format response
    /** Translates between gRPC wire conventions and the internal handler
        dispatch.

        @param s the client socket
        @param cx the connection context
        @param hdr the HTTP request headers
        @param body the request body
        @param method_info the registered method info
        @param codec the codec to use for serialization

        @return an HttpResponseInfo hash
    */
    static hash<HttpResponseInfo> handleGrpcRequest(Socket s, hash<auto> cx,
            hash<auto> hdr, *data body, hash<ConnectServerMethodInfo> method_info,
            AbstractConnectCodec codec) {
        # Parse gRPC timeout header
        *date deadline;
        if (hdr."grpc-timeout".val()) {
            *int timeout_ms = GrpcDetection::parseGrpcTimeout(hdr."grpc-timeout");
            if (timeout_ms) {
                deadline = now_us() + milliseconds(timeout_ms);
            }
        }

        # Extract metadata
        hash<auto> metadata();
        map metadata{$1.key} = $1.value,
            hdr.pairIterator(),
            $1.key !~ /^:/ && $1.key != "content-type" && $1.key != "te"
                && $1.key != "grpc-timeout";

        # Unframe the gRPC request (always envelope-framed)
        auto request;
        if (body) {
            binary body_bin;
            if (body.typeCode() == NT_BINARY) {
                body_bin = body;
            } else {
                body_bin = binary(body);
            }
            if (body_bin.size() >= 5) {
                hash<ConnectFrameInfo> frame = ConnectHelper::unframeMessage(body_bin);
                request = codec.deserialize(frame.data);
            }
        }

        # Call handler
        auto response;
        int grpc_status = 0;
        string grpc_message = "OK";
        try {
            response = method_info.handler(request, metadata);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "THREAD-CANCELLED" || ex.err == "PROGRAM-INTERRUPTED") {
                rethrow;
            }
            grpc_status = 13;  # INTERNAL
            grpc_message = sprintf("%s: %s", ex.err, ex.desc);
        }

        if (grpc_status == 0 && deadline && now_us() > deadline) {
            grpc_status = 4;  # DEADLINE_EXCEEDED
            grpc_message = "deadline exceeded";
        }

        # Build gRPC response — route through the *Async Socket methods so
        # this handler thread never holds the socket wrapper lock during
        # HTTP/2 I/O.  io_wake (forwarded by HttpServer.qm from the async
        # controller's conn_info.header_info) nudges the I/O thread to flush
        # the pending frames promptly; without it, frames sit in nghttp2's
        # outbound buffer until the next controller tick.  Replaces the old
        # sync submit/send/trailer + startPollHttp2Flush + Socket::poll loop.
        int stream_id = cx."header-info".stream_id ?? 0;
        *code<nothing()> io_wake = cx."header-info".io_wake;

        if (grpc_status == 0) {
            binary encoded = codec.serialize(response);
            binary framed = ConnectHelper::frameMessage(encoded);

            s.submitHttp2StreamingResponseHeaders(stream_id, 200,
                {"content-type": "application/grpc+proto"});
            s.sendHttp2StreamData(stream_id, framed, False);
        } else {
            s.submitHttp2StreamingResponseHeaders(stream_id, 200,
                {"content-type": "application/grpc+proto"});
        }

        s.sendHttp2Trailers(stream_id, {
            "grpc-status": string(grpc_status),
            "grpc-message": grpc_message,
        });

        if (io_wake) {
            io_wake();
        }

        return <HttpResponseInfo>{
            "reply_sent": True,
        };
    }

    #! Handle a gRPC-Web request
    /** gRPC-Web encodes trailers as base64 in the response body instead of
        using HTTP/2 trailers.

        @param s the client socket
        @param cx the connection context
        @param hdr the HTTP request headers
        @param body the request body
        @param method_info the registered method info
        @param codec the codec to use for serialization

        @return an HttpResponseInfo hash
    */
    static hash<HttpResponseInfo> handleGrpcWebRequest(Socket s, hash<auto> cx,
            hash<auto> hdr, *data body, hash<ConnectServerMethodInfo> method_info,
            AbstractConnectCodec codec) {
        # Extract metadata
        hash<auto> metadata();
        map metadata{$1.key} = $1.value,
            hdr.pairIterator(),
            $1.key !~ /^:/ && $1.key != "content-type" && $1.key != "te";

        # Unframe the gRPC request
        auto request;
        if (body) {
            binary body_bin;
            if (body.typeCode() == NT_BINARY) {
                body_bin = body;
            } else {
                body_bin = binary(body);
            }
            if (body_bin.size() >= 5) {
                hash<ConnectFrameInfo> frame = ConnectHelper::unframeMessage(body_bin);
                request = codec.deserialize(frame.data);
            }
        }

        # Call handler
        auto response;
        int grpc_status = 0;
        string grpc_message = "OK";
        try {
            response = method_info.handler(request, metadata);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "THREAD-CANCELLED" || ex.err == "PROGRAM-INTERRUPTED") {
                rethrow;
            }
            grpc_status = 13;  # INTERNAL
            grpc_message = sprintf("%s: %s", ex.err, ex.desc);
        }

        # Build gRPC-Web response body
        binary response_body = binary();
        if (grpc_status == 0 && exists response) {
            binary encoded = codec.serialize(response);
            response_body = ConnectHelper::frameMessage(encoded);
        }

        # Append trailers as a base64-encoded trailer frame
        string trailers_str = sprintf("grpc-status: %d\r\ngrpc-message: %s\r\n",
            grpc_status, grpc_message);
        binary trailer_frame = ConnectHelper::frameMessage(binary(trailers_str),
            0x80);  # trailer flag for gRPC-Web

        return <HttpResponseInfo>{
            "code": 200,
            "body": response_body + trailer_frame,
            "hdr": {"Content-Type": "application/grpc-web+proto"},
        };
    }
}

} # namespace Connect

%endif
