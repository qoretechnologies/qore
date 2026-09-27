# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectHandler.qc Connect protocol server handler

/*  ConnectHandler.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Connect server method info hashdecl
public hashdecl ConnectServerMethodInfo {
    #! Service name
    string service;
    #! Method name
    string method;
    #! Full path: /{service}/{method}
    string path;
    #! Handler callback
    code handler;
    #! RPC type (one of CONNECT_RPC_* constants)
    int rpc_type;
    #! Optional request type descriptor for reflection
    *AbstractDataProviderType request_type;
    #! Optional response type descriptor for reflection
    *AbstractDataProviderType response_type;
}

#! Connect RPC server handler for HttpServer
/** Integrates with HttpServer as an @ref AbstractHttpRequestHandler.
    Routes requests by URL path (<tt>/{service}/{method}</tt>) and dispatches
    to registered handler closures.

    Supports multiple codecs simultaneously via a codec registry keyed by name.
    Content-Type header determines which codec is used for each request.

    @par Example:
    @code{.py}
ConnectHandler handler();
handler.registerUnary("Greeter", "SayHello",
    auto sub (auto request, hash<auto> metadata) {
        return {"message": "Hello, " + request.name};
    });

# Add to HttpServer
http.setHandler("connect", "", NOTHING, handler);
    @endcode
*/
public class ConnectHandler inherits HttpServer::AbstractHttpRequestHandler {
    private {
        #! Codec registry: codec name -> codec instance
        hash<string, AbstractConnectCodec> m_codecs;
        #! Default codec (used when no specific codec is requested)
        AbstractConnectCodec m_default_codec;
        #! Method registry: "service/method" -> method info
        hash<string, hash<ConnectServerMethodInfo>> m_methods;
        #! Enable debug logging
        bool m_debug = False;
        #! Maximum message size in bytes (0 = no limit)
        int m_max_message_size = CONNECT_DEFAULT_MAX_MESSAGE_SIZE;
    }

    #! Create a Connect handler
    /** @param auth optional authenticator (use new AbstractAuthenticator() for no auth)
        @param codec optional default codec (defaults to YamlConnectCodec)
    */
    constructor(*HttpServer::AbstractAuthenticator auth, *AbstractConnectCodec codec)
            : HttpServer::AbstractHttpRequestHandler(auth) {
        if (!codec) {
            codec = new YamlConnectCodec();
        }
        m_default_codec = codec;
        m_codecs{codec.name()} = codec;
        registerReflectionService();
    }

    #! Register a unary RPC handler
    /** Handler signature: \c auto \c sub(auto \c request, hash<auto> \c metadata)

        @param service the service name (e.g., \c "Greeter")
        @param method the method name (e.g., \c "SayHello")
        @param handler the handler callback
    */
    registerUnary(string service, string method, code handler) {
        string key = service + "/" + method;
        m_methods{key} = <ConnectServerMethodInfo>{
            "service": service,
            "method": method,
            "path": "/" + service + "/" + method,
            "handler": handler,
            "rpc_type": CONNECT_RPC_UNARY,
        };
    }

    #! Register a server-streaming RPC handler
    /** Handler signature: \c nothing \c sub(auto \c request,
        ConnectServerStream \c stream, hash<auto> \c metadata)

        @param service the service name
        @param method the method name
        @param handler the handler callback
    */
    registerServerStream(string service, string method, code handler) {
        string key = service + "/" + method;
        m_methods{key} = <ConnectServerMethodInfo>{
            "service": service,
            "method": method,
            "path": "/" + service + "/" + method,
            "handler": handler,
            "rpc_type": CONNECT_RPC_SERVER_STREAM,
        };
    }

    #! Register a client-streaming RPC handler
    /** Handler signature: \c auto \c sub(ConnectServerStream \c stream,
        hash<auto> \c metadata)

        @param service the service name
        @param method the method name
        @param handler the handler callback
    */
    registerClientStream(string service, string method, code handler) {
        string key = service + "/" + method;
        m_methods{key} = <ConnectServerMethodInfo>{
            "service": service,
            "method": method,
            "path": "/" + service + "/" + method,
            "handler": handler,
            "rpc_type": CONNECT_RPC_CLIENT_STREAM,
        };
    }

    #! Register a bidirectional streaming RPC handler
    /** Handler signature: \c nothing \c sub(ConnectServerStream \c stream,
        hash<auto> \c metadata)

        @param service the service name
        @param method the method name
        @param handler the handler callback
    */
    registerBidiStream(string service, string method, code handler) {
        string key = service + "/" + method;
        m_methods{key} = <ConnectServerMethodInfo>{
            "service": service,
            "method": method,
            "path": "/" + service + "/" + method,
            "handler": handler,
            "rpc_type": CONNECT_RPC_BIDI,
        };
    }

    #! Register a unary RPC handler with type information
    /** Handler signature: \c auto \c sub(auto \c request, hash<auto> \c metadata)

        @param service the service name (e.g., \c "Greeter")
        @param method the method name (e.g., \c "SayHello")
        @param handler the handler callback
        @param request_type optional request type for reflection
        @param response_type optional response type for reflection
    */
    registerUnary(string service, string method, code handler,
            *AbstractDataProviderType request_type, *AbstractDataProviderType response_type) {
        string key = service + "/" + method;
        m_methods{key} = <ConnectServerMethodInfo>{
            "service": service,
            "method": method,
            "path": "/" + service + "/" + method,
            "handler": handler,
            "rpc_type": CONNECT_RPC_UNARY,
            "request_type": request_type,
            "response_type": response_type,
        };
    }

    #! Register a server-streaming RPC handler with type information
    /** Handler signature: \c nothing \c sub(auto \c request,
        ConnectServerStream \c stream, hash<auto> \c metadata)

        @param service the service name
        @param method the method name
        @param handler the handler callback
        @param request_type optional request type for reflection
        @param response_type optional response type for reflection
    */
    registerServerStream(string service, string method, code handler,
            *AbstractDataProviderType request_type, *AbstractDataProviderType response_type) {
        string key = service + "/" + method;
        m_methods{key} = <ConnectServerMethodInfo>{
            "service": service,
            "method": method,
            "path": "/" + service + "/" + method,
            "handler": handler,
            "rpc_type": CONNECT_RPC_SERVER_STREAM,
            "request_type": request_type,
            "response_type": response_type,
        };
    }

    #! Register a client-streaming RPC handler with type information
    /** Handler signature: \c auto \c sub(ConnectServerStream \c stream,
        hash<auto> \c metadata)

        @param service the service name
        @param method the method name
        @param handler the handler callback
        @param request_type optional request type for reflection
        @param response_type optional response type for reflection
    */
    registerClientStream(string service, string method, code handler,
            *AbstractDataProviderType request_type, *AbstractDataProviderType response_type) {
        string key = service + "/" + method;
        m_methods{key} = <ConnectServerMethodInfo>{
            "service": service,
            "method": method,
            "path": "/" + service + "/" + method,
            "handler": handler,
            "rpc_type": CONNECT_RPC_CLIENT_STREAM,
            "request_type": request_type,
            "response_type": response_type,
        };
    }

    #! Register a bidirectional streaming RPC handler with type information
    /** Handler signature: \c nothing \c sub(ConnectServerStream \c stream,
        hash<auto> \c metadata)

        @param service the service name
        @param method the method name
        @param handler the handler callback
        @param request_type optional request type for reflection
        @param response_type optional response type for reflection
    */
    registerBidiStream(string service, string method, code handler,
            *AbstractDataProviderType request_type, *AbstractDataProviderType response_type) {
        string key = service + "/" + method;
        m_methods{key} = <ConnectServerMethodInfo>{
            "service": service,
            "method": method,
            "path": "/" + service + "/" + method,
            "handler": handler,
            "rpc_type": CONNECT_RPC_BIDI,
            "request_type": request_type,
            "response_type": response_type,
        };
    }

    #! Register a datagram fire-and-forget RPC handler
    /** Handler signature: \c nothing \c sub(auto \c message, hash<auto> \c metadata)

        Fire-and-forget: client sends a single unreliable datagram message;
        no response is expected.

        @par Example:
        @code{.py}
handler.registerDatagramFireAndForget("Telemetry", "Report",
    sub (auto message, hash<auto> metadata) {
        logger.info("received telemetry: cpu=%y mem=%y",
            message.cpu, message.mem);
    });
        @endcode

        @param service the service name
        @param method the method name
        @param handler the handler callback
    */
    registerDatagramFireAndForget(string service, string method, code handler) {
        string key = service + "/" + method;
        m_methods{key} = <ConnectServerMethodInfo>{
            "service": service,
            "method": method,
            "path": "/" + service + "/" + method,
            "handler": handler,
            "rpc_type": CONNECT_RPC_DATAGRAM_FIRE_AND_FORGET,
        };
    }

    #! Register a datagram fire-and-forget RPC handler with type information
    /** Handler signature: \c nothing \c sub(auto \c message, hash<auto> \c metadata)

        @param service the service name
        @param method the method name
        @param handler the handler callback
        @param request_type optional request type for reflection
    */
    registerDatagramFireAndForget(string service, string method, code handler,
            *AbstractDataProviderType request_type) {
        string key = service + "/" + method;
        m_methods{key} = <ConnectServerMethodInfo>{
            "service": service,
            "method": method,
            "path": "/" + service + "/" + method,
            "handler": handler,
            "rpc_type": CONNECT_RPC_DATAGRAM_FIRE_AND_FORGET,
            "request_type": request_type,
        };
    }

    #! Register a bidirectional datagram RPC handler
    /** Handler signature: \c nothing \c sub(ConnectServerDatagramStream \c stream,
        hash<auto> \c metadata)

        Bidirectional datagrams: both client and server exchange unreliable
        messages until the anchor stream is closed.

        @par Example:
        @code{.py}
handler.registerDatagramBidi("Sensor", "Exchange",
    sub (ConnectServerDatagramStream stream, hash<auto> metadata) {
        while (*auto msg = stream.read(30s)) {
            # Echo each message back to the client
            stream.send({"echo": msg});
        }
    });
        @endcode

        @param service the service name
        @param method the method name
        @param handler the handler callback
    */
    registerDatagramBidi(string service, string method, code handler) {
        string key = service + "/" + method;
        m_methods{key} = <ConnectServerMethodInfo>{
            "service": service,
            "method": method,
            "path": "/" + service + "/" + method,
            "handler": handler,
            "rpc_type": CONNECT_RPC_DATAGRAM_BIDI,
        };
    }

    #! Register a bidirectional datagram RPC handler with type information
    /** Handler signature: \c nothing \c sub(ConnectServerDatagramStream \c stream,
        hash<auto> \c metadata)

        @param service the service name
        @param method the method name
        @param handler the handler callback
        @param request_type optional request type for reflection
        @param response_type optional response type for reflection
    */
    registerDatagramBidi(string service, string method, code handler,
            *AbstractDataProviderType request_type, *AbstractDataProviderType response_type) {
        string key = service + "/" + method;
        m_methods{key} = <ConnectServerMethodInfo>{
            "service": service,
            "method": method,
            "path": "/" + service + "/" + method,
            "handler": handler,
            "rpc_type": CONNECT_RPC_DATAGRAM_BIDI,
            "request_type": request_type,
            "response_type": response_type,
        };
    }

    #! Returns the method registry (for reflection service access)
    /** @return the method registry hash
    */
    hash<string, hash<ConnectServerMethodInfo>> getMethodRegistry() {
        return m_methods;
    }

    #! Add a codec to the registry
    /** @param codec the codec to add
    */
    addCodec(AbstractConnectCodec codec) {
        m_codecs{codec.name()} = codec;
    }

    #! Set the default codec
    /** @param codec the default codec (also added to registry if not present)
    */
    setDefaultCodec(AbstractConnectCodec codec) {
        m_default_codec = codec;
        if (!m_codecs{codec.name()}) {
            m_codecs{codec.name()} = codec;
        }
    }

    #! Enable or disable debug logging
    /** @param dbg True to enable debug logging
    */
    setDebug(bool dbg = True) {
        m_debug = dbg;
    }

    #! Set the maximum message size
    /** @param size maximum message size in bytes; 0 means no limit
    */
    setMaxMessageSize(int size) {
        m_max_message_size = size;
    }

    #! Get the maximum message size
    /** @return the maximum message size in bytes; 0 means no limit
    */
    int getMaxMessageSize() {
        return m_max_message_size;
    }

    #! HttpServer handler entry point
    /** Routes the request based on URL path and Content-Type, then dispatches
        to the appropriate registered handler.

        @param listener the HTTP listener interface
        @param s the client socket
        @param cx the connection context hash
        @param hdr the HTTP request headers
        @param body the request body

        @return an HttpResponseInfo hash
    */
    hash<HttpResponseInfo> handleRequest(HttpListenerInterface listener, Socket s,
            hash<auto> cx, hash<auto> hdr, *data body) {
        @debug {
            if (m_debug) {
                printf("ConnectHandler::handleRequest() method=%y path=%y ct=%y body_size=%d\n",
                    hdr.method, hdr.path, hdr."content-type", body.size());
            }
        }

        # Only POST is valid for Connect RPC
        if (hdr.method != "POST") {
            return makeErrorResponse(405, CONNECT_ERROR_UNIMPLEMENTED,
                sprintf("method %y not allowed; POST required", hdr.method));
        }

        # Extract service/method from path
        string path = hdr.path;
        # Remove query string if present
        int qpos = path.find("?");
        if (qpos >= 0) {
            path = path.substr(0, qpos);
        }
        # Remove leading slash
        if (path[0] == "/") {
            path = path.substr(1);
        }
        # Split into service/method
        int slash = path.find("/");
        if (slash < 0) {
            return makeErrorResponse(404, CONNECT_ERROR_UNIMPLEMENTED,
                sprintf("invalid path: %y; expected /{service}/{method}", hdr.path));
        }
        string service = path.substr(0, slash);
        string method_name = path.substr(slash + 1);
        # Remove any trailing slashes or path components after the method
        int slash2 = method_name.find("/");
        if (slash2 >= 0) {
            method_name = method_name.substr(0, slash2);
        }

        string handler_key = service + "/" + method_name;
        *hash<ConnectServerMethodInfo> method_info = m_methods{handler_key};
        if (!method_info) {
            return makeErrorResponse(404, CONNECT_ERROR_UNIMPLEMENTED,
                sprintf("unknown method: %s/%s", service, method_name));
        }

        # Parse Content-Type to determine codec and streaming mode
        string content_type = hdr."content-type" ?? "";
        *hash<auto> ct_info = ConnectHelper::parseContentType(content_type);

        # Resolve codec
        AbstractConnectCodec codec;
        if (ct_info) {
            *AbstractConnectCodec found = m_codecs{ct_info.codec_name};
            if (!found) {
                return <HttpResponseInfo>{
                    "code": 415,
                    "body": sprintf("unsupported codec: %y", ct_info.codec_name),
                    "hdr": {"Content-Type": "text/plain"},
                };
            }
            codec = found;
        } else {
            # No recognized content type — use default codec
            codec = m_default_codec;
        }

        # Determine if request is streaming or datagram based on content type
        bool streaming = ct_info ? ct_info.streaming : False;
        bool datagram = ct_info ? (ct_info.datagram ?? False) : False;

        # Extract metadata from request headers
        hash<auto> metadata = extractMetadata(hdr);

        # Parse deadline from Connect-Timeout-Ms
        *date deadline;
        *string timeout_str = hdr{ConnectTimeoutHeader};
        if (timeout_str.val()) {
            deadline = now_us() + milliseconds(int(timeout_str));
        }

        # Dispatch based on RPC type and streaming/datagram mode
        try {
            if (datagram) {
                # Datagram request — requires HTTP/3
                bool http3 = cx."header-info".http3 ?? False;
                if (!http3) {
                    return makeErrorResponse(400, CONNECT_ERROR_INVALID_ARGUMENT,
                        "datagram RPC requires HTTP/3 (QUIC)");
                }
                switch (method_info.rpc_type) {
                    case CONNECT_RPC_DATAGRAM_FIRE_AND_FORGET:
                        return handleDatagramFireAndForget(s, cx, hdr, body, method_info,
                            codec, metadata);
                    case CONNECT_RPC_DATAGRAM_BIDI:
                        return handleDatagramBidi(s, cx, hdr, body, method_info,
                            codec, metadata);
                    default:
                        return makeErrorResponse(400, CONNECT_ERROR_INVALID_ARGUMENT,
                            sprintf("method %s/%s does not support datagram RPC",
                                method_info.service, method_info.method));
                }
            }
            if (!streaming) {
                # Unary request (plain HTTP POST)
                return handleUnary(s, cx, hdr, body, method_info, codec, metadata, deadline);
            }
            # Streaming request
            switch (method_info.rpc_type) {
                case CONNECT_RPC_SERVER_STREAM:
                    return handleServerStream(s, cx, hdr, body, method_info, codec, metadata,
                        deadline);
                case CONNECT_RPC_CLIENT_STREAM:
                    return handleClientStream(s, cx, hdr, body, method_info, codec, metadata,
                        deadline);
                case CONNECT_RPC_BIDI:
                    return handleBidiStream(s, cx, hdr, body, method_info, codec, metadata,
                        deadline);
                default:
                    # Unary via streaming content type is allowed
                    return handleUnary(s, cx, hdr, body, method_info, codec, metadata, deadline);
            }
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "THREAD-CANCELLED" || ex.err == "PROGRAM-INTERRUPTED") {
                rethrow;
            }
            return makeErrorResponse(500, CONNECT_ERROR_INTERNAL,
                sprintf("%s: %s", ex.err, ex.desc));
        }
    }

    #! Handle a unary RPC request
    private hash<HttpResponseInfo> handleUnary(Socket s, hash<auto> cx, hash<auto> hdr,
            *data body, hash<ConnectServerMethodInfo> method_info,
            AbstractConnectCodec codec, hash<auto> metadata, *date deadline) {
        # Deserialize request body
        auto request;
        if (body) {
            binary body_bin;
            if (body.typeCode() == NT_BINARY) {
                body_bin = body;
            } else {
                body_bin = binary(body);
            }
            if (body_bin.size() > 0) {
                if (m_max_message_size > 0 && body_bin.size() > m_max_message_size) {
                    return makeErrorResponse(413, CONNECT_ERROR_RESOURCE_EXHAUSTED,
                        sprintf("request body size %d exceeds maximum %d bytes",
                            body_bin.size(), m_max_message_size));
                }
                request = codec.deserialize(body_bin);
            }
        }

        # Call handler
        auto response;
        try {
            response = method_info.handler(request, metadata);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "THREAD-CANCELLED" || ex.err == "PROGRAM-INTERRUPTED") {
                rethrow;
            }
            return makeErrorResponse(500, CONNECT_ERROR_INTERNAL,
                sprintf("%s: %s", ex.err, ex.desc));
        }

        # Check deadline after handler returns
        if (deadline && now_us() > deadline) {
            return makeErrorResponse(408, CONNECT_ERROR_DEADLINE_EXCEEDED, "deadline exceeded");
        }

        # Serialize response
        binary response_body = codec.serialize(response);
        return <HttpResponseInfo>{
            "code": 200,
            "body": response_body,
            "hdr": {
                "Content-Type": ConnectContentTypePrefix + codec.name(),
            },
        };
    }

    #! Handle a server-streaming RPC request
    private hash<HttpResponseInfo> handleServerStream(Socket s, hash<auto> cx,
            hash<auto> hdr, *data body, hash<ConnectServerMethodInfo> method_info,
            AbstractConnectCodec codec, hash<auto> metadata, *date deadline) {
        # Determine transport
        bool http3 = cx."header-info".http3 ?? False;
        int stream_id = cx."header-info".stream_id ?? 0;
        *int session_id = cx."header-info".session_id;

        # Negotiate compression
        *string recv_encoding = hdr{ConnectContentEncodingHeader};
        if (recv_encoding && !ConnectCompression::isSupported(recv_encoding)) {
            return makeErrorResponse(400, CONNECT_ERROR_INVALID_ARGUMENT,
                sprintf("unsupported connect-content-encoding: %y", recv_encoding));
        }
        *string send_encoding =
            ConnectCompression::negotiateEncoding(hdr{ConnectAcceptEncodingHeader});

        # Deserialize request from streaming frames
        auto request;
        if (body) {
            binary body_bin;
            if (body.typeCode() == NT_BINARY) {
                body_bin = body;
            } else {
                body_bin = binary(body);
            }
            if (body_bin.size() >= 5) {
                hash<ConnectFrameInfo> frame = ConnectHelper::unframeMessage(body_bin,
                    m_max_message_size);
                if (!(frame.flags & CONNECT_FLAG_END_STREAM)) {
                    binary frame_data = frame.data;
                    if ((frame.flags & CONNECT_FLAG_COMPRESSED) && recv_encoding) {
                        frame_data = ConnectCompression::decompress(frame_data,
                            recv_encoding);
                    }
                    request = codec.deserialize(frame_data);
                }
            }
        }

        # Create server stream for handler to write responses.  Forward
        # io_wake (from HttpServer's async-controller bridge) so the stream
        # can use the *Async Socket methods without a handler-thread flush.
        *code<nothing()> io_wake = cx."header-info".io_wake;
        ConnectServerStream stream(s, codec, stream_id, http3, session_id, (),
            m_max_message_size, send_encoding, recv_encoding, io_wake);

        *hash<ConnectErrorInfo> error;
        try {
            method_info.handler(request, stream, metadata);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "THREAD-CANCELLED" || ex.err == "PROGRAM-INTERRUPTED") {
                rethrow;
            }
            error = <ConnectErrorInfo>{
                "code": CONNECT_ERROR_INTERNAL,
                "message": sprintf("%s: %s", ex.err, ex.desc),
            };
        }

        # Check deadline
        if (!error && deadline && now_us() > deadline) {
            error = <ConnectErrorInfo>{
                "code": CONNECT_ERROR_DEADLINE_EXCEEDED,
                "message": "deadline exceeded",
            };
        }

        # Ensure response headers are sent
        stream.ensureHeadersSent();

        # Send EndStreamResponse frame
        binary end_frame = ConnectHelper::frameEndStream(
            stream.getTrailingMetadata().size() > 0 ? stream.getTrailingMetadata() : NOTHING,
            error);
        if (http3) {
            while (s.submitQuicStreamData(session_id, stream_id, end_frame, True) == 1) {
                s.waitForQuicStreamDrain(session_id, stream_id, 5s);
            }
        } else {
            # Async H2 end-of-stream write + io_wake to flush promptly.
            # Replaces the former sendHttp2StreamData + flushHttp2 sync pair.
            s.sendHttp2StreamData(stream_id, end_frame, True);
            *code<nothing()> io_wake = cx."header-info".io_wake;
            if (io_wake) {
                io_wake();
            }
        }

        bool is_multiplexed = http3 || (cx."header-info".http2 ?? False);
        return <HttpResponseInfo>{
            "reply_sent": True,
            "streaming": is_multiplexed,
        };
    }

    #! Handle a client-streaming RPC request
    private hash<HttpResponseInfo> handleClientStream(Socket s, hash<auto> cx,
            hash<auto> hdr, *data body, hash<ConnectServerMethodInfo> method_info,
            AbstractConnectCodec codec, hash<auto> metadata, *date deadline) {
        # Determine transport
        bool http3 = cx."header-info".http3 ?? False;
        int stream_id = cx."header-info".stream_id ?? 0;
        *int session_id = cx."header-info".session_id;

        # Negotiate compression
        *string recv_encoding = hdr{ConnectContentEncodingHeader};
        if (recv_encoding && !ConnectCompression::isSupported(recv_encoding)) {
            return makeErrorResponse(400, CONNECT_ERROR_INVALID_ARGUMENT,
                sprintf("unsupported connect-content-encoding: %y", recv_encoding));
        }
        *string send_encoding =
            ConnectCompression::negotiateEncoding(hdr{ConnectAcceptEncodingHeader});

        # Parse all client messages from the buffered body
        list<auto> messages = parseStreamMessages(body, codec, recv_encoding);

        # Create server stream with pre-parsed messages
        *code<nothing()> io_wake = cx."header-info".io_wake;
        ConnectServerStream stream(s, codec, stream_id, http3, session_id, messages,
            m_max_message_size, send_encoding, recv_encoding, io_wake);

        auto response;
        *hash<ConnectErrorInfo> error;
        try {
            response = method_info.handler(stream, metadata);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "THREAD-CANCELLED" || ex.err == "PROGRAM-INTERRUPTED") {
                rethrow;
            }
            error = <ConnectErrorInfo>{
                "code": CONNECT_ERROR_INTERNAL,
                "message": sprintf("%s: %s", ex.err, ex.desc),
            };
        }

        # Check deadline
        if (!error && deadline && now_us() > deadline) {
            error = <ConnectErrorInfo>{
                "code": CONNECT_ERROR_DEADLINE_EXCEEDED,
                "message": "deadline exceeded",
            };
        }

        # Ensure headers sent
        stream.ensureHeadersSent();

        if (!error && exists response) {
            # Send the response as a framed message
            binary encoded = codec.serialize(response);
            binary framed = ConnectHelper::frameMessage(encoded);
            if (http3) {
                while (s.submitQuicStreamData(session_id, stream_id, framed, False) == 1) {
                    s.waitForQuicStreamDrain(session_id, stream_id, 5s);
                }
            } else {
                s.sendHttp2StreamData(stream_id, framed, False);
            }
        }

        # Send EndStreamResponse
        binary end_frame = ConnectHelper::frameEndStream(
            stream.getTrailingMetadata().size() > 0 ? stream.getTrailingMetadata() : NOTHING,
            error);
        if (http3) {
            while (s.submitQuicStreamData(session_id, stream_id, end_frame, True) == 1) {
                s.waitForQuicStreamDrain(session_id, stream_id, 5s);
            }
        } else {
            # Async H2 end-of-stream write + io_wake to flush promptly.
            # Replaces the former sendHttp2StreamData + flushHttp2 sync pair.
            s.sendHttp2StreamData(stream_id, end_frame, True);
            *code<nothing()> io_wake = cx."header-info".io_wake;
            if (io_wake) {
                io_wake();
            }
        }

        bool is_multiplexed = http3 || (cx."header-info".http2 ?? False);
        return <HttpResponseInfo>{
            "reply_sent": True,
            "streaming": is_multiplexed,
        };
    }

    #! Handle a bidirectional-streaming RPC request
    private hash<HttpResponseInfo> handleBidiStream(Socket s, hash<auto> cx,
            hash<auto> hdr, *data body, hash<ConnectServerMethodInfo> method_info,
            AbstractConnectCodec codec, hash<auto> metadata, *date deadline) {
        # Determine transport
        bool http3 = cx."header-info".http3 ?? False;
        int stream_id = cx."header-info".stream_id ?? 0;
        *int session_id = cx."header-info".session_id;

        # Negotiate compression
        *string recv_encoding = hdr{ConnectContentEncodingHeader};
        if (recv_encoding && !ConnectCompression::isSupported(recv_encoding)) {
            return makeErrorResponse(400, CONNECT_ERROR_INVALID_ARGUMENT,
                sprintf("unsupported connect-content-encoding: %y", recv_encoding));
        }
        *string send_encoding =
            ConnectCompression::negotiateEncoding(hdr{ConnectAcceptEncodingHeader});

        # Parse all client messages from the buffered body
        list<auto> messages = parseStreamMessages(body, codec, recv_encoding);

        # Create server stream with pre-parsed messages
        *code<nothing()> io_wake = cx."header-info".io_wake;
        ConnectServerStream stream(s, codec, stream_id, http3, session_id, messages,
            m_max_message_size, send_encoding, recv_encoding, io_wake);

        *hash<ConnectErrorInfo> error;
        try {
            method_info.handler(stream, metadata);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "THREAD-CANCELLED" || ex.err == "PROGRAM-INTERRUPTED") {
                rethrow;
            }
            error = <ConnectErrorInfo>{
                "code": CONNECT_ERROR_INTERNAL,
                "message": sprintf("%s: %s", ex.err, ex.desc),
            };
        }

        # Check deadline
        if (!error && deadline && now_us() > deadline) {
            error = <ConnectErrorInfo>{
                "code": CONNECT_ERROR_DEADLINE_EXCEEDED,
                "message": "deadline exceeded",
            };
        }

        # Ensure headers sent
        stream.ensureHeadersSent();

        # Send EndStreamResponse
        binary end_frame = ConnectHelper::frameEndStream(
            stream.getTrailingMetadata().size() > 0 ? stream.getTrailingMetadata() : NOTHING,
            error);
        if (http3) {
            while (s.submitQuicStreamData(session_id, stream_id, end_frame, True) == 1) {
                s.waitForQuicStreamDrain(session_id, stream_id, 5s);
            }
        } else {
            # Async H2 end-of-stream write + io_wake to flush promptly.
            # Replaces the former sendHttp2StreamData + flushHttp2 sync pair.
            s.sendHttp2StreamData(stream_id, end_frame, True);
            *code<nothing()> io_wake = cx."header-info".io_wake;
            if (io_wake) {
                io_wake();
            }
        }

        bool is_multiplexed = http3 || (cx."header-info".http2 ?? False);
        return <HttpResponseInfo>{
            "reply_sent": True,
            "streaming": is_multiplexed,
        };
    }

    #! Handle a datagram fire-and-forget RPC request
    /** The client sends a single datagram and expects no response.
        The anchor stream stays open for datagram delivery, then closes.
    */
    private hash<HttpResponseInfo> handleDatagramFireAndForget(Socket s, hash<auto> cx,
            hash<auto> hdr, *data body, hash<ConnectServerMethodInfo> method_info,
            AbstractConnectCodec codec, hash<auto> metadata) {
        int stream_id = cx."header-info".stream_id ?? 0;
        *int session_id = cx."header-info".session_id;
        code<nothing(*binary, bool)> send_stream_data = cx."header-info".send_stream_data;
        code<nothing()> io_wake = cx."header-info".io_wake;

        # Send 200 OK with Capsule-Protocol: ?1 to establish datagram channel
        hash<auto> resp_headers = {
            "content-type": ConnectDatagramContentTypePrefix + codec.name(),
            ConnectCapsuleProtocolHeader: "?1",
        };
        s.submitQuicResponseStreaming(session_id, stream_id, 200, resp_headers);
        io_wake();

        # Create server datagram stream for reading the client's datagram
        ConnectServerDatagramStream dg_stream(s, codec, stream_id, session_id,
            m_max_message_size);

        # Read one datagram from the client
        *auto message;
        try {
            message = dg_stream.read(30s);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "THREAD-CANCELLED" || ex.err == "PROGRAM-INTERRUPTED") {
                rethrow;
            }
        }

        # Dispatch to handler if we got a message
        if (exists message) {
            try {
                method_info.handler(message, metadata);
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == "THREAD-CANCELLED" || ex.err == "PROGRAM-INTERRUPTED") {
                    rethrow;
                }
                @debug {
                    if (m_debug) {
                        printf("ConnectHandler: fire-and-forget handler error: %s: %s\n",
                            ex.err, ex.desc);
                    }
                }
            }
        }

        # Close the anchor stream (uses send_stream_data for proper I/O wake)
        send_stream_data(binary(), True);

        return <HttpResponseInfo>{
            "reply_sent": True,
            "streaming": True,
        };
    }

    #! Handle a bidirectional datagram RPC request
    /** Both sides exchange unreliable datagrams until the anchor stream closes.
    */
    private hash<HttpResponseInfo> handleDatagramBidi(Socket s, hash<auto> cx,
            hash<auto> hdr, *data body, hash<ConnectServerMethodInfo> method_info,
            AbstractConnectCodec codec, hash<auto> metadata) {
        int stream_id = cx."header-info".stream_id ?? 0;
        *int session_id = cx."header-info".session_id;
        code<nothing(*binary, bool)> send_stream_data = cx."header-info".send_stream_data;
        code<nothing()> io_wake = cx."header-info".io_wake;

        # Send 200 OK with Capsule-Protocol: ?1 to establish datagram channel
        hash<auto> resp_headers = {
            "content-type": ConnectDatagramContentTypePrefix + codec.name(),
            ConnectCapsuleProtocolHeader: "?1",
        };
        s.submitQuicResponseStreaming(session_id, stream_id, 200, resp_headers);
        io_wake();

        # Create server datagram stream with I/O wake callback for flushing datagrams
        ConnectServerDatagramStream dg_stream(s, codec, stream_id, session_id,
            m_max_message_size, io_wake);

        # Dispatch to handler — handler is responsible for the read/write loop
        try {
            method_info.handler(dg_stream, metadata);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "THREAD-CANCELLED" || ex.err == "PROGRAM-INTERRUPTED") {
                rethrow;
            }
            # Send error datagram before closing
            try {
                dg_stream.sendError(<ConnectErrorInfo>{
                    "code": CONNECT_ERROR_INTERNAL,
                    "message": sprintf("%s: %s", ex.err, ex.desc),
                });
            } catch (hash<ExceptionInfo> ex2) {
                if (ex2.err == "THREAD-CANCELLED" || ex2.err == "PROGRAM-INTERRUPTED") {
                    rethrow;
                }
                # Ignore send errors during cleanup
            }
        }

        # Close the anchor stream (uses send_stream_data for proper I/O wake)
        send_stream_data(binary(), True);

        return <HttpResponseInfo>{
            "reply_sent": True,
            "streaming": True,
        };
    }

    #! Create an error response for unary requests
    private hash<HttpResponseInfo> makeErrorResponse(int http_status, string error_code,
            string message) {
        string error_json = ConnectError::serializeError(<ConnectErrorInfo>{
            "code": error_code,
            "message": message,
        });
        return <HttpResponseInfo>{
            "code": http_status,
            "body": error_json,
            "hdr": {"Content-Type": "application/json"},
        };
    }

    #! Extract metadata from request headers
    /** Extracts custom headers (non-standard, non-protocol headers) as metadata
        to pass to handlers.
    */
    private hash<auto> extractMetadata(hash<auto> hdr) {
        hash<auto> metadata();
        map metadata{$1.key} = $1.value,
            hdr.pairIterator(),
            $1.key != "content-type"
                && $1.key != "content-length"
                && $1.key != "host"
                && $1.key != "method"
                && $1.key != "path"
                && $1.key != "http_version"
                && $1.key != "user-agent"
                && $1.key != "accept"
                && $1.key != "accept-encoding"
                && $1.key != "connection"
                && $1.key !~ /^:/
                && $1.key != ConnectProtocolVersionHeader
                && $1.key != ConnectTimeoutHeader;
        return metadata;
    }

    #! Registers the built-in reflection service (qore.connect.reflection)
    /** Registers three unary methods for service discovery:
        - \c ListServices: returns all registered services
        - \c GetServiceDescriptor: returns full descriptor for a named service
        - \c GetMethodDescriptor: returns descriptor for a named method

        Handlers read \c m_methods at call time (late binding) so methods registered
        after handler construction are discoverable.
    */
    private registerReflectionService() {
        # ListServices — returns {"services": [{"name": "Greeter"}, ...]}
        registerUnary("qore.connect.reflection", "ListServices",
            auto sub (auto request, hash<auto> metadata) {
                hash<string, bool> seen;
                list<hash<auto>> services = ();
                map (seen{$1.value.service} = True), m_methods.pairIterator(),
                    !seen{$1.value.service} && $1.value.service != "qore.connect.reflection";
                map services += {"name": $1}, keys seen;
                return {"services": services};
            });

        # GetServiceDescriptor — takes {"service": "Greeter"}, returns full info
        registerUnary("qore.connect.reflection", "GetServiceDescriptor",
            auto sub (auto request, hash<auto> metadata) {
                string svc_name = request.service;
                if (!svc_name.val()) {
                    throw "CONNECT-ERROR", "missing 'service' field in request";
                }
                list<hash<auto>> methods = ();
                map methods += ConnectTypeHelper::methodInfoToDescriptor($1.value),
                    m_methods.pairIterator(),
                    $1.value.service == svc_name;
                if (!methods) {
                    throw "CONNECT-ERROR", sprintf("unknown service: %y", svc_name);
                }
                return {
                    "service": svc_name,
                    "methods": methods,
                };
            });

        # GetMethodDescriptor — takes {"service": "Greeter", "method": "SayHello"}
        registerUnary("qore.connect.reflection", "GetMethodDescriptor",
            auto sub (auto request, hash<auto> metadata) {
                string svc_name = request.service;
                string meth_name = request.method;
                if (!svc_name.val() || !meth_name.val()) {
                    throw "CONNECT-ERROR",
                        "missing 'service' and/or 'method' fields in request";
                }
                string key = svc_name + "/" + meth_name;
                *hash<ConnectServerMethodInfo> info = m_methods{key};
                if (!info) {
                    throw "CONNECT-ERROR",
                        sprintf("unknown method: %y/%y", svc_name, meth_name);
                }
                return ConnectTypeHelper::methodInfoToDescriptor(info);
            });
    }

    #! Parse streaming messages from a request body
    /** @param body the raw request body
        @param codec the codec for deserialization
        @param recv_encoding optional compression encoding for decompressing
        compressed frames
    */
    private list<auto> parseStreamMessages(*data body, AbstractConnectCodec codec,
            *string recv_encoding) {
        list<auto> messages = ();
        if (!body) {
            return messages;
        }
        binary body_bin;
        if (body.typeCode() == NT_BINARY) {
            body_bin = body;
        } else {
            body_bin = binary(body);
        }
        if (body_bin.size() < 5) {
            return messages;
        }
        list<hash<ConnectFrameInfo>> frames = ConnectHelper::unframeMessages(body_bin,
            m_max_message_size);
        for (int i = 0; i < frames.size(); ++i) {
            if (frames[i].flags & CONNECT_FLAG_END_STREAM) {
                # End-of-stream frame — skip (it's the client's end marker)
                continue;
            }
            binary frame_data = frames[i].data;
            if ((frames[i].flags & CONNECT_FLAG_COMPRESSED) && recv_encoding) {
                frame_data = ConnectCompression::decompress(frame_data, recv_encoding);
            }
            messages += codec.deserialize(frame_data);
        }
        return messages;
    }
}

} # namespace Connect
