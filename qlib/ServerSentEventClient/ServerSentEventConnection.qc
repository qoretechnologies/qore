# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ServerSentEventConnection.qc SSE connection client

/*  ServerSentEventConnection.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! the ServerSentEventClient namespace contains all the objects in the ServerSentEventClient module
public namespace ServerSentEventClient {
#! class for SSE connections
/** Returns an object of class @ref ServerSentEventClient for receiving sse events from a sse server; supports
    all @ref ConnectionProvider::HttpConnection "HttpConnection" options.

    Also supports the following runtime options in getImpl():
    - \c "logger": a @ref Logger::LoggerInterface "LoggerInterface" object for internal logging
    - \c "start_thread": a closure to start a new thread (signature:
      \c "int sub start_thread(code callable) {}")
    - \c "yield": a closure to yield current thread execution
*/
public class ServerSentEventConnection inherits ConnectionProvider::AbstractConnectionWithInfo {
    public {
        #! Connection entry info
        const ConnectionScheme = <ConnectionSchemeInfo>{
            "display_name": "ServerSentEvent Connection",
            "short_desc": "A connection to a ServerSentEvent server service",
            "desc": "A connection to a ServerSentEvent server service",
            "cls": Class::forName("ServerSentEventConnection"),
            "schemes": {
                "sses": True,
                "sse": True,
            },
            "options": HttpConnection::ConnectionScheme.options + {
                "data": <ConnectionOptionInfo>{
                    "display_name": "Data Serialization",
                    "short_desc": "Sets the data serialization option",
                    "type": "string",
                    "desc": "Sets the data serialization option",
                    "allowed_values": (
                        <AllowedValueInfo>{
                            "value": "json",
                            "desc": "use JSON serialization",
                        },
                        <AllowedValueInfo>{
                            "value": "none",
                            "desc": "no serialization is used",
                        },
                    ),
                    "default_value": "none",
                },
                "last_event_id": <ConnectionOptionInfo>{
                    "display_name": "Last Event ID",
                    "short_desc": "Request the server restart sending events from this ID",
                    "type": "softstring",
                    "desc": "Request the server restart sending events from this ID. This will be send in the "
                        "`Last-Event-ID` header in the connection request to the SSE server",
                },
                "reconnect": <ConnectionOptionInfo>{
                    "display_name": "Reconnect?",
                    "short_desc": "Try to reconnect when the connection is closed or lost",
                    "type": "bool",
                    "desc": "Try to reconnect when the connection is closed or lost",
                    "default_value": False,
                },
                "token": <ConnectionOptionInfo>{
                    "display_name": "Authorization Token",
                    "short_desc": "Set an authorization token",
                    "type": "string",
                    "desc": "Any token to use for the initial HTTP request to authenticate with the server",
                    "sensitive": True,
                },
                "token_type": <ConnectionOptionInfo>{
                    "display_name": "Authorization Token Type",
                    "short_desc": "The type of authorization token",
                    "type": "string",
                    "desc": "The type of `token` to use for the `Authorization` header; ignored if no `token` "
                        "option is set",
                    "default_value": "Bearer",
                },
                "version": <ConnectionOptionInfo>{
                    "display_name": "ServerSentEvent Version",
                    "short_desc": "Sets the ServerSentEvent protocol version",
                    "type": "int",
                    "desc": "The ServerSentEvent protocol version to use in the client",
                    "allowed_values": (
                        <AllowedValueInfo>{
                            "value": 8,
                            "desc": "Version 8",
                        },
                        <AllowedValueInfo>{
                            "value": 13,
                            "desc": "Version 13",
                        },
                    ),
                },
                "method": <ConnectionOptionInfo>{
                    "display_name": "HTTP Method",
                    "short_desc": "HTTP method for the SSE connection request",
                    "type": "string",
                    "desc": "The HTTP method to use for the SSE connection request; default: "
                        "GET. Use POST for APIs that require a request body (e.g., OpenAI, "
                        "Anthropic streaming APIs)",
                    "default_value": "GET",
                },
                "body": <ConnectionOptionInfo>{
                    "display_name": "Request Body",
                    "short_desc": "Request body for non-GET SSE requests",
                    "type": "string",
                    "desc": "The request body to send with the SSE connection request; only "
                        "valid with non-GET methods. The body is resent on each reconnection "
                        "attempt",
                },
                "asyncapi": <ConnectionOptionInfo>{
                    "display_name": "AsyncAPI Schema",
                    "short_desc": "AsyncAPI schema URL or file path",
                    "type": "string",
                    "desc": "AsyncAPI schema URL or file path used to validate received SSE event data",
                },
                "asyncapi_channel": <ConnectionOptionInfo>{
                    "display_name": "AsyncAPI Channel",
                    "short_desc": "AsyncAPI channel for validation",
                    "type": "string",
                    "desc": "AsyncAPI channel name to use when `asyncapi` is configured",
                },
            },
        };
    }

    #! creates the ServerSentEventConnection connection object
    /** @param name the name of the connection
        @param description connection description
        @param url connection URL (potentially with username and password info for HTTP basic authentication)
        @param attributes various attributes. See below
        @param options connection options

        See @ref ConnectionProvider::AbstractConnection::constructor() for \c attributes and \c options reference.

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option
    */
    constructor(string name, string description, string url, hash<auto> attributes = {}, hash<auto> options = {})
            : AbstractConnectionWithInfo(name, description, url, attributes, options) {
    }

    #! creates the ServerSentEventConnection connection object
    /** @param config with the following keys:
        - name (required string): the connection name
        - display_name (optional string): the display name
        - short_desc (optional string): a short description in plain text
        - desc (optional string): a long description with markdown formatting
        - url (required string): the connection URL
        - opts (optional hash): connection options
        - logger (optional LoggerInterface object): logger for the connection
        @param attr optional connection attributes
        - monitor (optional bool): should the connection be monitored? Default: True
        - enabled (optional bool): is the connection enabled? Default: True
        - locked (optional bool): is the connection locked? Default: False
        - debug_data (optional bool): debug data? Default: False
        - tags (optional hash): tags for the connection; no default value

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option or attribute
    */
    constructor(hash<auto> config, *hash<auto> attr) : AbstractConnectionWithInfo(config, attr) {
    }

    #! returns \c "sse"
    string getType() {
        return "sse";
    }

    #! returns runtime options
    /** @return a hash with the following keys reflecting support for the corresponding runtime options in getImpl():
        - \c "logger": a @ref Logger::LoggerInterface "LoggerInterface" object for internal logging
        - \c "start_thread": a closure to start a new thread (signature:
          \c "int sub start_thread(code callable) {}")
        - \c "yield": a closure to yield current thread execution
    */
    *hash<auto> getRuntimeOptions() {
        return {
            "logger": True,
            "start_thread": True,
            "yield": True,
        };
    }

    #! Checks the connection to the SSE server
    /** Sends the SSE request with the operation returned by @ref startPollConnect() and waits for the response
        headers on the calling thread; the event stream is closed once the response has been validated.

        @throw SSE-CONNECT-ERROR the response status code is not 200, or the response \c Content-Type is not
        \c text/event-stream
        @throw SOCKET-TIMEOUT the response headers were not received in time
    */
    private hash<PingInfo> pingIntern(hash<PingInfo> rv) {
        date start = now_us();
        AbstractPollOperation op = startPollConnect(logger);
        on_error op.abort();
        while (!op.goalReached()) {
            *hash<SocketPollInfo> info = op.continuePoll();
            if (info) {
                # the operation raises SOCKET-TIMEOUT when its deadline passes; it wakes at least once a second
                Socket::poll((info,), info.poll_timeout_ms ?? 1000);
            }
        }
        return rv += {
            "ok": True,
            "time": now_us() - start,
            "info": "OK",
        };
    }

    #! returns a @ref ServerSentEventClient object
    /** @param connect if @ref True "True", then @ref ServerSentEventClient::connect() is called
        @param rtopts supports the following runtime options in getImpl():
        - \c "logger": a @ref Logger::LoggerInterface "LoggerInterface" object for internal logging
        - \c "reconnect": try to reconnect when the connection is closed or lost (default @ref False)
        - \c "start_thread": a closure to start a new thread (signature:
          \c "int sub start_thread(code callable) {}")
        - \c "yield": a closure to yield current thread execution

        @return a @ref ServerSentEventClient object
    */
    private ServerSentEventClient getImpl(bool connect = True, *hash<auto> rtopts) {
        # to avoid type issues with rtopts
        if (rtopts) {
            rtopts = {} + rtopts;
        }
        hash<auto> opts = {"url": url} + rtopts{"logger", "reconnect", "yield", "start_thread"} + self.opts;
        if (!opts.logger && logger) {
            opts.logger = logger;
        }

        ServerSentEventClient sse(opts);
        if (connect) {
            sse.connect();
        }
        return sse;
    }

    #! Returns an SSE client backed by HttpClientIo
    /** @param connect if \c True, then the asynchronous event loop is started before returning
        @param rtopts supports the same runtime options as @ref getImpl()

        @return an async-I/O-capable @ref ServerSentEventClientIo object
    */
    private ServerSentEventClientIo getAsyncImpl(bool connect = True, *hash<auto> rtopts) {
        if (rtopts) {
            rtopts = {} + rtopts;
        }
        hash<auto> async_opts = {"url": url} + rtopts{"logger", "reconnect", "yield", "start_thread"}
            + self.opts;
        if (!async_opts.logger && logger) {
            async_opts.logger = logger;
        }

        ServerSentEventClientIo sse(async_opts);
        if (connect) {
            sse.connect();
        }
        return sse;
    }

    #! returns an ServerSentEventClientDataProvider object for this connection
    /** @param constructor_options any additional constructor options for the data provider

        @return an ServerSentEventClientDataProvider object for this connection

        @see hasDataProvider()
    */
    DataProvider::AbstractDataProvider getDataProvider(*hash<auto> constructor_options) {
        return new ServerSentEventClientDataProvider({
            "url": url,
        } + opts{"token", "token_type", "username", "password", "method", "body", "asyncapi",
            "asyncapi_channel"});
    }

    #! returns @ref True, as the connection returns a data provider with the \c getDataProvider() method
    /** @return @ref True, as the connection returns a data provider with the \c getDataProvider() method

        @see @ref getDataProvider()
    */
    bool hasDataProvider() {
        return True;
    }

    #! Sets child data provider capabilities
    private setChildCapabilities() {
        children_can_support_observers = True;
    }

    #! Returns @ref True since SSE connections support the polling API
    /** @return @ref True

        @since ServerSentEventClient 1.1
    */
    bool supportsPollingApi() {
        return True;
    }

    #! Starts an async SSE connection check via polling
    /** @param logger an optional logger for the poll operation

        @return a poll operation that validates the SSE server is reachable and responds correctly

        @since ServerSentEventClient 1.1
    */
    Qore::AbstractPollOperation startPollConnect(*Logger::LoggerInterface logger) {
        # Build HTTP options from connection options
        hash<auto> http_opts = {"url": url} + self.opts;
        # Replace SSE scheme with HTTP scheme
        hash<UrlInfo> uh = parse_url(http_opts.url);
        if (uh.protocol) {
            hash<auto> sh = SseSchemes;
            *hash<auto> ph = sh{uh.protocol};
            if (ph) {
                splice http_opts.url, 0, uh.protocol.size(), "http" + (ph.ssl ? "s" : "");
            }
        }
        # Extract SSE-specific options before removing them from HTTP opts
        string sse_method = remove http_opts.method ?? "GET";
        *data sse_body = remove http_opts.body;

        # Remove SSE-only options
        remove http_opts{"logger", "last_event_id", "data", "reconnect", "yield", "start_thread",
            "asyncapi", "asyncapi_channel", "validator"};

        # Handle authorization token
        if ((*string token = remove http_opts.token) && !http_opts.headers.Authorization) {
            http_opts.headers.Authorization = sprintf("%s %s", http_opts.token_type ?? "Bearer", token);
        }
        remove http_opts.token_type;

        return SseConnectPollOperation::create(http_opts.url, http_opts, NOTHING, logger,
            sse_method, sse_body);
    }

    #! Returns the ConnectionSchemeInfo hash for this object
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }
}

#! Poll operation for non-blocking SSE connection verification
/** Connects to an SSE server and validates the response (status 200, \c Content-Type: \c text/event-stream).

    The request is made with an
    @ref HttpClientIo::HttpClientConnectionManager "HttpClientConnectionManager" (HTTP/1.1, HTTP/2, or HTTP/3 as
    negotiated or configured with the \c http_version option) using only non-blocking I/O; the operation completes
    once the response headers have been received, and the event stream is then closed.

    The goal is \c "sse-connect"; the output is the response without a body as described for
    @ref ConnectionProvider::HttpClientIoPollOperation "HttpClientIoPollOperation".

    Errors raised by @ref continuePoll() in addition to the errors of the transport (for example
    \c SOCKET-CONNECT-ERROR if the connection is refused and \c SOCKET-TIMEOUT if the response headers are not
    received in time):
    - \c SSE-CONNECT-ERROR: the response status code is not 200, or the response \c Content-Type is missing or not
      \c text/event-stream

    @since ServerSentEventClient 1.1
*/
public class SseConnectPollOperation inherits ConnectionProvider::HttpClientIoPollOperation {
    #! Creates the poll operation
    /** @param base_url the SSE URL with an \c http or \c https scheme
        @param http_opts @ref ConnectionProvider::HttpConnection "HttpConnection" options; see
        @ref ConnectionProvider::HttpConnection::getAsyncConnectionManager() for the options used for the connection;
        the \c headers, \c username, \c password, \c max_redirects, \c redirect_passthru, and \c timeout options
        are used for the request
        @param headers additional headers for the request
        @param logger optional logger
        @param sse_method the HTTP method for the request
        @param sse_body the request body; a binary value is sent serialized with JSON

        @throw HTTP-CLIENT-URL-ERROR the URL has no host or an unsupported scheme

        @since ServerSentEventClient 1.1
    */
    constructor(string base_url, *hash<auto> http_opts, *hash<auto> headers, *Logger::LoggerInterface logger,
            string sse_method = "GET", *data sse_body)
            : HttpClientIoPollOperation(HttpConnection::getAsyncConnectionManager(http_opts, logger), base_url,
                sse_method,
                HttpClientIoPollOperation::getRequestHeaders(base_url, http_opts,
                    SseConnectPollOperation::getSseHeaders(headers, sse_body)),
                SseConnectPollOperation::getSseBody(sse_body),
                HttpClientIoPollOperation::getOptions("sse-connect", http_opts, 0), logger) {
        opts.headers_only = True;
        if (logger) {
            logger.logArgs("INFO", "SSE async connect start: url=%y method=%y", (base_url, sse_method));
        }
    }

    #! Creates an SSE connect poll operation
    /** @param base_url the SSE URL with an \c http or \c https scheme
        @param http_opts @ref ConnectionProvider::HttpConnection "HttpConnection" options
        @param headers additional headers for the request
        @param logger optional logger
        @param sse_method the HTTP method for the request
        @param sse_body the request body; a binary value is sent serialized with JSON

        @return a new SseConnectPollOperation

        @throw HTTP-CLIENT-URL-ERROR the URL has no host or an unsupported scheme

        @since ServerSentEventClient 1.1
    */
    static SseConnectPollOperation create(string base_url, *hash<auto> http_opts,
            *hash<auto> headers, *Logger::LoggerInterface logger,
            string sse_method = "GET", *data sse_body) {
        return new SseConnectPollOperation(base_url, http_opts, headers, logger, sse_method, sse_body);
    }

    #! Validates the SSE response
    /** @throw SSE-CONNECT-ERROR the response status code is not 200, or the response \c Content-Type is missing or
        not \c text/event-stream
    */
    private validateResponse(hash<auto> output) {
        if (output.code != 200) {
            throw "SSE-CONNECT-ERROR", sprintf("expected status 200, got %d %s", output.code,
                output.status_message ?? ""), output - "info";
        }
        auto ct = output.headers."content-type";
        if (ct.typeCode() != NT_STRING) {
            throw "SSE-CONNECT-ERROR", "missing required response header 'content-type'", output - "info";
        }
        if (ct.lwr().find(MimeTypeSse) != 0) {
            throw "SSE-CONNECT-ERROR", sprintf("expected header 'content-type' to start with '%s', got '%s'",
                MimeTypeSse, ct), output - "info";
        }
    }

    #! Returns the request headers for an SSE request
    private static hash<auto> getSseHeaders(*hash<auto> headers, *data sse_body) {
        hash<auto> sse_headers = {
            "Accept": MimeTypeSse,
            "Cache-Control": "no-cache",
        };
        sse_headers += headers;
        if (exists sse_body && sse_body.typeCode() != NT_STRING && !sse_headers."Content-Type") {
            sse_headers."Content-Type" = MimeTypeJson;
        }
        return sse_headers;
    }

    #! Returns the request body for an SSE request
    private static *data getSseBody(*data sse_body) {
        if (exists sse_body && sse_body.typeCode() != NT_STRING) {
            return make_json(sse_body);
        }
        return sse_body;
    }
}
}
