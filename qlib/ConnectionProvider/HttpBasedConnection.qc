# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore HttpBasedConnection class definition

/*  HttpBasedConnection.qc Copyright 2016 - 2026 Qore Technologies, s.r.o.

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

#! The ConnectionProvider namespace
/** All public declarations in the %ConnectionProvider module are defined in this namespace
*/
public namespace ConnectionProvider {
#! Options for @ref HttpClientIoPollOperation
/** @since ConnectionProvider 3.1
*/
public hashdecl HttpClientIoPollOptions {
    #! The goal of the operation as returned by @ref HttpClientIoPollOperation::getGoal()
    string goal = "http-request";

    #! If @ref True, only a connection is established and no request is sent
    /** The output is then a hash with the \c url and \c protocol keys
    */
    bool connect_only = False;

    #! If @ref True, the operation completes once the final response headers have been received
    /** The response body is not read and the stream is canceled; use for long-lived streaming responses such as
        Server-Sent Events or protocol upgrades
    */
    bool headers_only = False;

    #! If @ref True, the request body stays open for bidirectional streaming
    /** Required for an extended \c CONNECT request (RFC 8441 / RFC 9220) with \c headers_only
    */
    bool body_streaming = False;

    #! The maximum number of redirects to follow
    int max_redirects = 5;

    #! If @ref True, redirect responses are returned instead of followed
    bool redirect_passthru = False;

    #! The time in milliseconds to wait for the response; 0 means the connection manager's request timeout
    int timeout_ms = 0;

    #! The number of failed connection attempts retried; if not set, the connection manager's
    #! \c max_request_retries value is used
    *int max_connect_retries;

    #! The character encoding of text responses without a \c charset
    /** ISO-8859-1 is the default character set of text media types in HTTP/1.1 (RFC 2616), as assumed by
        @ref Qore::HTTPClient "HTTPClient"
    */
    string assume_encoding = "ISO-8859-1";
}

#! A non-blocking HTTP request made with an HttpClientIo connection manager
/** Supports HTTP/1.1, HTTP/2, and HTTP/3 as negotiated by the
    @ref HttpClientIo::HttpClientConnectionManager "HttpClientConnectionManager".

    No blocking I/O is performed on the calling thread: connection readiness is waited for with the connection
    manager's event notifiers, and the response is read from the stream as it arrives, so the operation can be
    driven by @ref Qore::Socket::poll() "Socket::poll()" or submitted to an async I/O controller.

    The output is a hash with the following keys, compatible with the output of the
    @ref Qore::HTTPClient::startPollSendRecv() "HTTPClient::startPollSendRecv()" operation:
    - \c code: the HTTP status code
    - \c status_code: the HTTP status code
    - \c status_message: the HTTP status message
    - \c http_version: the HTTP version of the response
    - \c protocol: the protocol of the connection (\c "HTTP/1.1", \c "HTTP/2", or \c "HTTP/3")
    - \c headers: the response headers with lower-case names
    - \c effective_url: the URL of the request that produced the response
    - \c redirects: a list of hashes, one for each redirect followed
    - \c info: a hash with the \c response-headers (lower-case names plus \c status_code, \c status_message,
      and \c http_version), \c response-headers-raw, \c response-uri, \c request-uri, \c effective-url, and
      optionally \c redirects, \c body-content-type, \c charset, and \c response-body keys
    - \c request-headers: the headers sent with the request
    - \c response-body: the response body, if any: a string for text responses, otherwise a binary; not present
      in \c headers_only mode

    With the \c connect_only option the output is a hash with the \c url (the origin) and \c protocol keys once the
    connection is ready.

    Subclasses can validate the response by reimplementing @ref validateResponse().

    @since ConnectionProvider 3.1
*/
public class HttpClientIoPollOperation inherits Qore::AbstractPollOperation<auto> {
    public {
        #! Waiting for a connection
        const ST_CONNECTING = "connecting";
        #! Waiting for the response
        const ST_RECEIVING = "receiving";
        #! Complete
        const ST_COMPLETE = "completed";
        #! Aborted or failed
        const ST_ABORTED = "aborted";

        #! The maximum time in milliseconds between two wakes of the operation while waiting for an event
        const PollTimeoutMs = 1000;

        #! Redirect status codes that are followed
        const FollowedRedirects = {
            "301": True,
            "302": True,
            "303": True,
            "307": True,
            "308": True,
        };

        #! Headers sent only to the origin of the original request
        const OriginBoundHeaders = {
            "authorization": True,
            "cookie": True,
            "host": True,
        };
    }

    private {
        #! The connection manager
        HttpClientIo::HttpClientConnectionManager mgr;
        #! The options
        hash<HttpClientIoPollOptions> opts;
        #! An optional logger
        *Logger::LoggerInterface logger;

        #! The origin of the current request (scheme, host, and port)
        string origin;
        #! The request path of the current request
        string path;
        #! The method of the current request
        string method;
        #! The headers of the current request
        hash<auto> headers = {};
        #! The body of the current request
        *data body;

        #! The origin of the original request
        string original_origin;
        #! Origin-bound headers of the original request
        hash<auto> origin_headers = {};
        #! The redirects followed
        list<hash<auto>> redirects = ();

        #! The deadline for the response
        date deadline;

        #! The current state
        string state = ST_CONNECTING;
        #! The connection handed back by the last acquisition step
        *HttpClientIo::HttpClientConnection tracked;
        #! Failed connection attempts
        int failures = 0;
        #! The connection readiness notifier
        *EventNotifier conn_notifier;
        #! True if @ref conn_notifier was signaled by this object
        bool self_signaled = False;
        #! The response notifier
        EventNotifier io_notifier();
        #! The current stream
        *HttpClientIo::HttpClientStreamHandle stream;
        #! The response headers of the current request
        *hash<auto> response;
        #! The body received for the current request
        list<binary> body_chunks = ();
        #! True if the current response is a redirect to follow
        bool redirecting = False;

        #! The output
        *hash<auto> output;

        #! Serializes the caller and an async I/O controller worker
        Mutex m();
    }

    #! Creates the operation; no I/O is performed until @ref continuePoll() is called
    /** @param mgr the connection manager to use; the operation does not close it
        @param url the absolute URL of the request, including any path and query
        @param method the HTTP method
        @param headers request headers
        @param body the request body, if any
        @param opts options for the operation
        @param logger an optional logger

        @throw HTTP-CLIENT-URL-ERROR the URL has no host or an unsupported scheme
    */
    constructor(HttpClientIo::HttpClientConnectionManager mgr, string url, string method = "GET",
            *hash<auto> headers, *data body, *hash<HttpClientIoPollOptions> opts, *Logger::LoggerInterface logger) {
        self.mgr = mgr;
        self.opts = opts ?? <HttpClientIoPollOptions>{};
        self.logger = logger;
        self.method = method;
        self.body = body;
        hash<auto> target = HttpClientIoPollOperation::splitUrl(url);
        origin = original_origin = target.origin;
        path = target.path;
        if (headers) {
            self.headers = headers;
        }
        origin_headers = map {$1.key: $1.value}, self.headers.pairIterator(), OriginBoundHeaders{$1.key.lwr()};
        deadline = now_us() + milliseconds(self.opts.timeout_ms > 0 ? self.opts.timeout_ms
            : mgr.getRequestTimeout());
    }

    #! Returns the goal
    string getGoal() {
        return opts.goal;
    }

    #! Returns the state: \c "connecting", \c "receiving", \c "completed", or \c "aborted"
    string getState() {
        AutoLock al(m);
        return state;
    }

    #! Returns @ref True once the output is available
    bool goalReached() {
        AutoLock al(m);
        return exists output;
    }

    #! Returns the output, or @ref nothing if the operation is not complete
    auto getOutput() {
        AutoLock al(m);
        return output;
    }

    #! Returns the connection manager
    HttpClientIo::HttpClientConnectionManager getConnectionManager() {
        return mgr;
    }

    #! Continues the operation
    /** @return a poll hash while the operation needs more I/O, @ref nothing once the output is available

        @throw SOCKET-CONNECT-ERROR the connection could not be established
        @throw SOCKET-TIMEOUT the response was not received in time
        @throw HTTP-CLIENT-MAXIMUM-REDIRECTS-EXCEEDED the maximum number of redirects was exceeded
        @throw HTTP-CLIENT-REDIRECT-ERROR a redirect cannot be followed
        @throw HTTP-CLIENT-UNKNOWN-PROTOCOL a redirect location has an unsupported scheme

        @note Other errors of the transport are raised as well, for example \c HTTPCLIENT-CONNECT-ERROR, and
        subclasses raise their own errors from @ref validateResponse()
    */
    *hash<SocketPollInfo> continuePoll() {
        AutoLock al(m);
        try {
            return continuePollIntern();
        } catch (hash<ExceptionInfo> ex) {
            cancelStream();
            state = ST_ABORTED;
            HttpClientIoPollOperation::rethrowError(ex);
        }
    }

    #! Aborts the operation
    abort() {
        AutoLock al(m);
        cancelStream();
        if (!exists output) {
            state = ST_ABORTED;
        }
    }

    #! Aborts the operation if it was canceled or failed in an async I/O controller
    nothing onComplete(hash<SocketPollResultInfo> result) {
        if (result.canceled || result.ex) {
            abort();
        }
    }

    #! Validates the output before it is made available
    /** The base class implementation does nothing; called with the operation's lock held

        @param output the output of the operation

        @note an exception raised here aborts the operation and is raised by @ref continuePoll()
    */
    private validateResponse(hash<auto> output) {
    }

    #! Returns operation options for the given HTTP connection options
    /** @param goal the goal of the operation
        @param http_opts @ref HttpConnection options; the \c max_redirects, \c redirect_passthru, \c timeout, and
        \c assume_encoding options are used
        @param max_connect_retries the number of failed connection attempts retried; if not set, the connection
        manager's \c max_request_retries value is used

        @return the options for the operation
    */
    static hash<HttpClientIoPollOptions> getOptions(string goal, *hash<auto> http_opts, *int max_connect_retries) {
        hash<HttpClientIoPollOptions> rv = <HttpClientIoPollOptions>{
            "goal": goal,
            "redirect_passthru": http_opts.redirect_passthru.toBool(),
        };
        if (exists http_opts.max_redirects) {
            rv.max_redirects = http_opts.max_redirects;
        }
        if (http_opts.timeout) {
            # the option may be given as a relative date or as an integer number of milliseconds
            timeout timeout_ms = http_opts.timeout;
            rv.timeout_ms = timeout_ms;
        }
        if (http_opts.assume_encoding) {
            rv.assume_encoding = http_opts.assume_encoding;
        }
        if (exists max_connect_retries) {
            rv.max_connect_retries = max_connect_retries;
        }
        return rv;
    }

    #! Returns the absolute URL of a request
    /** @param base_url the URL of the connection
        @param path the request path; a relative path is appended to any path in \a base_url, an absolute path
        replaces it, and an absolute URL is returned unchanged

        @return the absolute URL of the request
    */
    static string getRequestUrl(string base_url, string path) {
        if (path =~ /^https?:\/\//i) {
            return path;
        }
        hash<auto> target = HttpClientIoPollOperation::splitUrl(base_url);
        if (path[0] != "/") {
            string base_path = parse_url(base_url).path ?? "/";
            if (base_path[-1] != "/") {
                base_path += "/";
            }
            path = base_path + path;
        }
        return target.origin + path;
    }

    #! Returns request headers for the given HTTP connection options
    /** @param url the URL of the connection; any user information is used for HTTP basic authentication
        @param http_opts @ref HttpConnection options; the \c headers option gives headers sent with the request, and
        the \c username and \c password options are used for HTTP basic authentication if \a url has no user
        information
        @param headers additional headers, overriding headers from \a http_opts

        @return the request headers
    */
    static hash<auto> getRequestHeaders(string url, *hash<auto> http_opts, *hash<auto> headers) {
        hash<auto> rv = {};
        if (http_opts.headers.typeCode() == NT_HASH) {
            rv += http_opts.headers;
        }
        rv += headers;
        if (!(map 1, keys rv, $1.lwr() == "authorization")) {
            hash<UrlInfo> u = parse_url(url);
            *string username = u.username;
            *string password = u.password;
            if (!username.val() && !password.val()) {
                username = http_opts.username;
                password = http_opts.password;
            }
            if (username.val() || password.val()) {
                rv.Authorization = "Basic " + make_base64_string(sprintf("%s:%s", username ?? "", password ?? ""));
            }
        }
        return rv;
    }

    #! Returns the request body
    /** @param method the HTTP method
        @param body the body

        @return the body, or @ref nothing for \c GET requests

        @throw HTTP-CLIENT-BODY-ERROR the body is not a string or binary value
    */
    static *data getRequestBody(string method, auto body) {
        if (method == "GET") {
            return;
        }
        switch (body.typeCode()) {
            case NT_NOTHING:
            case NT_STRING:
            case NT_BINARY:
                return body;
        }
        throw "HTTP-CLIENT-BODY-ERROR", sprintf("cannot send a request body of type %y; expecting \"string\" or "
            "\"binary\"", body.fullType());
    }

    #! Returns the origin (scheme, host, and port) and the request path of the given URL
    /** @param url an absolute URL

        @return a hash with the \c origin and \c path keys

        @throw HTTP-CLIENT-URL-ERROR the URL has no host or an unsupported scheme
    */
    static hash<auto> splitUrl(string url) {
        *list<*string> parts = (url =~ x/^([A-Za-z][A-Za-z0-9+.-]*):\/\/([^\/?#]*)([^#]*)/s);
        if (!parts || !parts[1].val()) {
            throw "HTTP-CLIENT-URL-ERROR", sprintf("URL %y has no host", url);
        }
        string scheme = parts[0].lwr();
        if (scheme != "http" && scheme != "https") {
            throw "HTTP-CLIENT-URL-ERROR", sprintf("URL %y has unsupported scheme %y; expecting \"http\" or "
                "\"https\"", url, scheme);
        }
        # user information is never part of the origin
        string authority = parts[1];
        authority =~ s/^.*@//s;
        string path = HttpClientIoPollOperation::encodeRequestTarget(parts[2] ?? "");
        if (path !~ /^\//) {
            path = "/" + path;
        }
        return {
            "origin": scheme + "://" + authority,
            "path": path,
        };
    }

    #! Percent-encodes characters that are not valid in a request target
    /** @param target the request target

        @return the request target with invalid characters percent-encoded
    */
    static string encodeRequestTarget(string target) {
        if (target !~ /[^\x21-\x7e]|["<>\\^`{|}]/) {
            return target;
        }
        binary octets = target.toBinary();
        string rv;
        for (int i = 0; i < octets.size(); ++i) {
            int c = octets[i];
            if (c <= 0x20 || c >= 0x7f || c == 0x22 || c == 0x3c || c == 0x3e || c == 0x5c || c == 0x5e
                    || c == 0x60 || c == 0x7b || c == 0x7c || c == 0x7d) {
                rv += sprintf("%%%02X", c);
            } else {
                rv += chr(c);
            }
        }
        return rv;
    }

    #! Raises an error of the transport with the error code documented for HTTP clients
    /** @param ex the error

        - \c HTTPCLIENT-TIMEOUT is raised as \c SOCKET-TIMEOUT
        - \c HTTPCLIENT-CONNECT-ERROR caused by a connection that failed is raised with the error of the
          connection, for example \c SOCKET-CONNECT-ERROR
        - other errors are raised unchanged
    */
    static rethrowError(hash<ExceptionInfo> ex) {
        if (ex.err == "HTTPCLIENT-TIMEOUT") {
            throw "SOCKET-TIMEOUT", ex.desc, ex.arg;
        }
        if (ex.err == "HTTPCLIENT-CONNECT-ERROR" && ex.arg.typeCode() == NT_HASH && ex.arg.err.typeCode() == NT_STRING
            && ex.arg.err =~ /^SOCKET-/) {
            throw ex.arg.err, ex.arg.desc ?? ex.desc, ex.arg.arg;
        }
        throw ex.err, ex.desc, ex.arg;
    }

    #! Continues the operation; called with the lock held
    private *hash<SocketPollInfo> continuePollIntern() {
        while (True) {
            switch (state) {
                case ST_COMPLETE:
                case ST_ABORTED:
                    return;

                case ST_CONNECTING: {
                    if (conn_notifier) {
                        # one acquisition step per readiness notification; see the HttpClientIo design
                        if (!self_signaled && connectionPending()) {
                            checkDeadline();
                            return getPollInfo(conn_notifier);
                        }
                        conn_notifier.acknowledge();
                        remove conn_notifier;
                    }
                    checkConnectionFailure();
                    checkDeadline();
                    self_signaled = False;
                    *EventNotifier notifier = mgr.acquireConnectionAsync(origin, \tracked);
                    if (!notifier && tracked && HttpClientIoPollOperation::failed(tracked)
                        && !mgr.isUnresolvedRaceLeg(tracked)) {
                        # the connection closed synchronously; the next step counts the failure
                        notifier = new EventNotifier();
                        notifier.notify();
                    }
                    if (notifier) {
                        conn_notifier = notifier;
                        return getPollInfo(notifier);
                    }
                    if (opts.connect_only) {
                        completeConnect();
                        return;
                    }
                    try {
                        stream = mgr.acquireStream(origin);
                        stream.startStreaming(method, path, headers, opts.body_streaming, body, io_notifier);
                    } catch (hash<ExceptionInfo> ex) {
                        if (ex.err == "HTTPCLIENT-NOT-READY") {
                            # the connection reported itself ready and refused the request; retry the acquisition
                            remove stream;
                            conn_notifier = new EventNotifier();
                            conn_notifier.notify();
                            self_signaled = True;
                            return getPollInfo(conn_notifier);
                        }
                        rethrow;
                    }
                    # a used connection that closes later is not a failed connection attempt
                    remove tracked;
                    state = ST_RECEIVING;
                    continue;
                }

                case ST_RECEIVING: {
                    io_notifier.acknowledge();
                    while (True) {
                        auto data = stream.readData(0);
                        if (!exists data) {
                            if (!response) {
                                response = stream.getResponse();
                            }
                            if (response && !processHeaders()) {
                                return;
                            }
                            if (stream.isDone()) {
                                # the end of the stream was delivered together with the last body chunk
                                if (processEnd()) {
                                    return;
                                }
                                break;
                            }
                            checkDeadline();
                            return getPollInfo(io_notifier);
                        }
                        bool end;
                        if (data.typeCode() == NT_HASH) {
                            if (data.error) {
                                throw data.error.err ?? "HTTP-IO-ERROR", data.error.desc ?? sprintf("%y", data.error),
                                    data.error;
                            }
                            if (!response && (data.status_code || data.headers)) {
                                response = data;
                            }
                            if (exists data.body) {
                                body_chunks += binary(data.body);
                            }
                            end = data.end_stream.toBool();
                        } else {
                            body_chunks += binary(data);
                        }
                        if (!response) {
                            response = stream.getResponse();
                        }
                        if (response && !processHeaders()) {
                            return;
                        }
                        if (end) {
                            if (processEnd()) {
                                return;
                            }
                            # a redirect was followed
                            break;
                        }
                    }
                    continue;
                }
            }
        }
    }

    #! Raises a \c SOCKET-TIMEOUT error if the deadline has passed
    private checkDeadline() {
        if (now_us() >= deadline) {
            throw "SOCKET-TIMEOUT", sprintf("timed out waiting for the %s from %s%s", state == ST_CONNECTING
                ? "connection" : "response", origin, opts.connect_only ? "" : path);
        }
    }

    #! Completes a connect-only operation
    private completeConnect() {
        hash<auto> rv = {
            "url": origin,
        };
        if (tracked) {
            rv.protocol = tracked.getProtocol();
        }
        validateResponse(rv);
        output = rv;
        remove tracked;
        state = ST_COMPLETE;
    }

    #! Processes the response headers once; returns @ref False if the output is available
    private bool processHeaders() {
        if (redirecting || response.processed) {
            return True;
        }
        response.processed = True;
        if (!opts.redirect_passthru && FollowedRedirects{response.status_code}) {
            redirecting = True;
            return True;
        }
        if (opts.headers_only) {
            hash<auto> rv = getResponseOutput();
            validateResponse(rv);
            output = rv;
            state = ST_COMPLETE;
            # the response body is not read
            cancelStream(True);
            return False;
        }
        return True;
    }

    #! Processes the end of a response; returns @ref True if the output is available, @ref False if a redirect is
    #! followed
    private bool processEnd() {
        if (redirecting) {
            followRedirect();
            return False;
        }
        hash<auto> rv = getResponseOutput();
        if (!opts.headers_only) {
            binary bin;
            map bin += $1, body_chunks;
            if (bin) {
                auto response_body = getBody(bin);
                rv."response-body" = response_body;
                rv.info."response-body" = response_body;
            }
        }
        validateResponse(rv);
        output = rv;
        state = ST_COMPLETE;
        return True;
    }

    #! Returns the response body decoded according to its content encoding and character set
    private auto getBody(binary bin) {
        hash<auto> decoded = HttpClientIo::HttpClientStreamHandle::normalizeResponseBody({
            "headers": response.headers ?? {},
            "body": bin,
        }, stream.getMaxResponseBodySize());
        auto rv = decoded.body;
        if (rv.typeCode() == NT_STRING) {
            *string ct = response.headers."content-type";
            if (ct !~ /charset=/i && ct =~ /^text\//i) {
                # the text was assumed to be UTF-8; reinterpret its octets
                rv = rv.toBinary().toString(opts.assume_encoding);
            }
        }
        return rv;
    }

    #! Returns the output without the body
    private hash<auto> getResponseOutput() {
        int code = response.status_code ?? 0;
        string http_version = response.http_version ?? HttpClientIoPollOperation::getHttpVersion(response.protocol);
        string status_message = response.status_message ?? "";
        hash<auto> hdrs = response.headers ?? {};
        hash<auto> info = {
            "response-headers": hdrs + {
                "status_code": code,
                "status_message": status_message,
                "http_version": http_version,
            },
            "response-headers-raw": response.headers_raw ?? hdrs,
            "response-uri": trim(sprintf("HTTP/%s %d %s", http_version, code, status_message)),
            "request-uri": sprintf("%s %s HTTP/%s", method, path, http_version),
            "effective-url": origin + path,
        };
        *string ct = hdrs."content-type";
        if (ct) {
            info."body-content-type" = mime_get_base_type(ct);
            *string charset = (ct =~ x/charset=["']?([^\s;,"']+)/i)[0];
            if (charset) {
                info.charset = charset;
            }
        }
        if (redirects) {
            info.redirects = redirects;
        }
        return {
            "code": code,
            "status_code": code,
            "status_message": status_message,
            "http_version": http_version,
            "protocol": response.protocol,
            "headers": hdrs,
            "effective_url": origin + path,
            "redirects": redirects,
            "info": info,
            "request-headers": headers,
        };
    }

    #! Returns the HTTP version for the given protocol
    private static string getHttpVersion(*string protocol) {
        if (protocol =~ /3/) {
            return "3";
        }
        if (protocol =~ /2/) {
            return "2";
        }
        return "1.1";
    }

    #! Follows a redirect
    /** @throw HTTP-CLIENT-MAXIMUM-REDIRECTS-EXCEEDED the maximum number of redirects was exceeded
        @throw HTTP-CLIENT-REDIRECT-ERROR the redirect cannot be followed
        @throw HTTP-CLIENT-UNKNOWN-PROTOCOL the redirect location has an unsupported scheme
    */
    private followRedirect() {
        int code = response.status_code;
        string request_url = origin + path;
        auto location = response.headers.location;
        if (location.typeCode() == NT_LIST) {
            throw "HTTP-CLIENT-REDIRECT-ERROR", sprintf("redirect response %d from %y has %d Location headers; "
                "expecting one", code, request_url, location.size());
        }
        if (!location.val()) {
            throw "HTTP-CLIENT-REDIRECT-ERROR", sprintf("no redirect location given for status code %d from %y",
                code, request_url);
        }
        if (redirects.size() >= opts.max_redirects) {
            throw "HTTP-CLIENT-MAXIMUM-REDIRECTS-EXCEEDED", sprintf("maximum redirects (%d) exceeded; redirect %d "
                "from %y to %y ignored", opts.max_redirects, code, request_url, location);
        }

        # resolve the location against the URL of the request that received it (RFC 3986 section 5.2) and remove
        # any fragment, which is never sent
        string target = resolve_url(request_url, location);
        target =~ s/#.*//s;
        *list<*string> parts = (target =~ x/^([A-Za-z][A-Za-z0-9+.-]*):\/\/([^\/?#]*)(.*)$/s);
        if (!parts) {
            throw "HTTP-CLIENT-REDIRECT-ERROR", sprintf("redirect location %y from %y (code %d) has no host",
                location, request_url, code);
        }
        string scheme = parts[0].lwr();
        if (scheme != "http" && scheme != "https") {
            throw "HTTP-CLIENT-UNKNOWN-PROTOCOL", sprintf("redirect location %y from %y (code %d) has unsupported "
                "scheme %y", location, request_url, code, scheme);
        }
        # user information in a redirect location is never used as credentials
        string authority = parts[1];
        authority =~ s/^.*@//s;
        if (!authority.val() || authority =~ /[\x00-\x20\x7f"<>\\^`{|}]|%(?![0-9a-fA-F]{2})/) {
            throw "HTTP-CLIENT-REDIRECT-ERROR", sprintf("redirect location %y from %y (code %d) has an invalid "
                "host", location, request_url, code);
        }
        # a network server must never be able to send the request to a local UNIX domain socket
        if (authority =~ /^socket=/i && request_url !~ /^[a-z]+:\/\/socket=/i) {
            throw "HTTP-CLIENT-REDIRECT-ERROR", sprintf("redirect location %y from %y (code %d) names a UNIX "
                "domain socket; only a request on a UNIX domain socket can be redirected to one", location,
                request_url, code);
        }
        string next_origin = scheme + "://" + authority;
        string next_path = HttpClientIoPollOperation::encodeRequestTarget(parts[2] ?? "");
        if (next_path !~ /^\//) {
            next_path = "/" + next_path;
        }

        # RFC 9110 sections 15.4.2 - 15.4.4: 303 is retrieved with GET, and a POST redirected with 301 or 302 is
        # changed to GET; 307 and 308 repeat the request
        bool drop_body;
        if (code == 303) {
            if (method != "HEAD") {
                method = "GET";
            }
            drop_body = True;
        } else if ((code == 301 || code == 302) && method == "POST") {
            method = "GET";
            drop_body = True;
        }
        if (drop_body) {
            remove body;
            headers -= (map $1, keys headers, $1 =~ /^(content-|transfer-encoding$)/i);
        }

        # origin-bound headers are sent only to the origin of the original request
        headers -= (map $1, keys headers, OriginBoundHeaders{$1.lwr()});
        if (next_origin.lwr() == original_origin.lwr()) {
            headers += origin_headers;
        }

        redirects += {
            "request_url": request_url,
            "status_code": code,
            "status_message": response.status_message,
            "location": location,
            "target_url": next_origin + next_path,
            "headers": response.headers_raw ?? response.headers,
        };
        if (logger) {
            logger.debug("%s: following redirect %d from %y to %y", opts.goal, code, request_url,
                next_origin + next_path);
        }

        origin = next_origin;
        path = next_path;
        cancelStream();
        remove stream;
        remove response;
        body_chunks = ();
        redirecting = False;
        failures = 0;
        state = ST_CONNECTING;
    }

    #! Returns @ref True while the tracked connection is still connecting
    private bool connectionPending() {
        return exists tracked && !HttpClientIoPollOperation::failed(tracked) && !tracked.isReady()
            && !mgr.isUnresolvedRaceLeg(tracked);
    }

    #! Counts the tracked connection as a failed attempt if it failed before it could be used
    /** @throw HTTPCLIENT-CONNECT-ERROR the connection attempts failed more often than allowed
    */
    private checkConnectionFailure() {
        *HttpClientIo::HttpClientConnection conn = remove tracked;
        if (!conn || !HttpClientIoPollOperation::failed(conn) || mgr.isUnresolvedRaceLeg(conn)) {
            return;
        }
        if (++failures <= (opts.max_connect_retries ?? mgr.getMaxRequestRetries())) {
            return;
        }
        *hash<auto> err = conn.getPollOperation().getError();
        throw "HTTPCLIENT-CONNECT-ERROR", sprintf("connection closed: %s: %s", err.err ?? "UNKNOWN",
            err.desc ?? "unknown"), err;
    }

    #! Returns @ref True if a pending connection failed
    private static bool failed(HttpClientIo::HttpClientConnection conn) {
        return conn.isClosed() || conn.getPollOperation().hasError();
    }

    #! Cancels the current stream, unless it was completed
    /** @param force if @ref True, the stream is canceled even if the output is available
    */
    private cancelStream(*bool force) {
        if (stream && (force || !exists output)) {
            try {
                stream.cancel();
            } catch (hash<ExceptionInfo> ex) {
                if (logger) {
                    logger.debug("%s: error canceling the stream: %s: %s", opts.goal, ex.err, ex.desc);
                }
            }
        }
    }

    #! Returns poll information for the given notifier
    private hash<SocketPollInfo> getPollInfo(EventNotifier notifier) {
        int remaining = (deadline - now_us()).durationMilliseconds();
        return <SocketPollInfo>{
            "events": SOCK_POLLIN,
            "socket": notifier,
            "poll_timeout_ms": max(1, min(PollTimeoutMs, remaining)),
        };
    }
}

#! Poll operation for non-blocking HTTP connection checks
/** Establishes a connection with an
    @ref HttpClientIo::HttpClientConnectionManager "HttpClientConnectionManager" (HTTP/1.1, HTTP/2, or HTTP/3 as
    negotiated or configured) without sending a request.

    The goal is \c "connect", and the state is \c "connected" once the connection is ready; the output is then a
    hash with the \c url (the origin) and \c protocol keys.

    @since ConnectionProvider 3.1
*/
public class HttpConnectPollOperation inherits HttpClientIoPollOperation {
    #! Creates the poll operation for a connection check
    /** @param url the HTTP URL
        @param http_opts @ref HttpConnection options; see
        @ref HttpConnection::getAsyncConnectionManager() for the options used
        @param logger optional logger

        @throw HTTP-CLIENT-URL-ERROR the URL has no host or an unsupported scheme
    */
    constructor(string url, *hash<auto> http_opts, *Logger::LoggerInterface logger)
            : HttpClientIoPollOperation(HttpConnection::getAsyncConnectionManager(http_opts, logger), url, "GET",
                NOTHING, NOTHING, <HttpClientIoPollOptions>{
                    "goal": "connect",
                    "connect_only": True,
                    "max_connect_retries": 0,
                }, logger) {
        if (http_opts.connect_timeout) {
            # the option may be given as a relative date or as an integer number of milliseconds
            timeout timeout_ms = http_opts.connect_timeout;
            deadline = now_us() + milliseconds(timeout_ms);
        }
    }

    #! Returns the state: \c "connecting", \c "connected", or \c "aborted"
    string getState() {
        string state = HttpClientIoPollOperation::getState();
        return state == ST_COMPLETE ? "connected" : state;
    }
}

#! Poll operation for async HTTP ping requests
/** Uses only non-blocking I/O for HTTP ping requests; the request is made with an
    @ref HttpClientIo::HttpClientConnectionManager "HttpClientConnectionManager", so HTTP/1.1, HTTP/2, and HTTP/3 are
    supported as negotiated or as configured with the \c http_version option.

    The goal is \c "http-ping"; the output is the response as described for @ref HttpClientIoPollOperation; a
    response with a status code outside the range 200 - 299 raises an error with the error code given to the
    constructor.

    @since ConnectionProvider 2.2
*/
public class HttpPingPollOperation inherits HttpClientIoPollOperation {
    private {
        #! The error code for responses with a status code outside the range 200 - 299
        string err_code;
    }

    #! Creates the poll operation for an async HTTP ping
    /** @param base_url the HTTP URL
        @param method the HTTP method
        @param path the request path; a relative path is appended to any path in \a base_url, and an absolute URL is
        used as the request URL
        @param http_opts @ref HttpConnection options; see
        @ref HttpConnection::getAsyncConnectionManager() for the options used for the connection; the \c headers,
        \c username, \c password, \c max_redirects, \c redirect_passthru, \c timeout, and \c assume_encoding options
        are used for the request
        @param headers additional headers
        @param body the request body; ignored for \c GET requests
        @param logger optional logger
        @param err_code the error code for responses with a status code outside the range 200 - 299
        @param log_prefix the prefix of the log message logged when the ping is started

        @throw HTTP-CLIENT-URL-ERROR the URL has no host or an unsupported scheme
        @throw HTTP-CLIENT-BODY-ERROR the body is not a string or binary value

        @note Since ConnectionProvider 3.1 the request is made with \c HttpClientIo instead of
        @ref Qore::HTTPClient "HTTPClient"
    */
    constructor(string base_url, string method, string path, *hash<auto> http_opts, *hash<auto> headers, auto body,
            *Logger::LoggerInterface logger, string err_code = "HTTP-PING-ERROR",
            string log_prefix = "HttpConnection")
            : HttpClientIoPollOperation(HttpConnection::getAsyncConnectionManager(http_opts, logger),
                HttpClientIoPollOperation::getRequestUrl(base_url, path ?? "/"), method ?? "GET",
                HttpClientIoPollOperation::getRequestHeaders(base_url, http_opts, headers),
                HttpClientIoPollOperation::getRequestBody(method ?? "GET", body),
                HttpClientIoPollOperation::getOptions("http-ping", http_opts, 0), logger) {
        self.err_code = err_code;
        if (logger) {
            logger.logArgs("INFO", sprintf("%s async ping start: method=%s path=%y url=%y",
                log_prefix, self.method, self.path, base_url));
        }
    }

    #! Raises an error if the status code of the response is not in the range 200 - 299
    private validateResponse(hash<auto> output) {
        if (output.code < 200 || output.code > 299) {
            throw err_code, sprintf("request failed with HTTP code %d (expected 200-299)", output.code), output;
        }
    }
}

#! Poll operation for async HTTP requests
/** Uses non-blocking I/O for arbitrary HTTP requests, returning the full response; the request is made with an
    @ref HttpClientIo::HttpClientConnectionManager "HttpClientConnectionManager", so HTTP/1.1, HTTP/2, and HTTP/3 are
    supported as negotiated or as configured with the \c http_version option.

    Unlike @ref HttpPingPollOperation which validates that the response is 2xx, this class
    returns the full response hash (code, headers, body) for any HTTP status code, allowing
    the caller to handle the response; see @ref HttpClientIoPollOperation for the output.

    The goal is \c "http-request".

    @since ConnectionProvider 3.0
*/
public class HttpRequestPollOperation inherits HttpClientIoPollOperation {
    #! Creates the poll operation for an async HTTP request
    /** @param base_url the HTTP URL
        @param method the HTTP method (GET, POST, etc.)
        @param path the request path; a relative path is appended to any path in \a base_url, and an absolute URL is
        used as the request URL
        @param body the request body; ignored for \c GET requests
        @param http_opts @ref HttpConnection options; see
        @ref HttpConnection::getAsyncConnectionManager() for the options used for the connection; the \c headers,
        \c username, \c password, \c max_redirects, \c redirect_passthru, \c timeout, and \c assume_encoding options
        are used for the request
        @param headers additional headers
        @param logger optional logger

        @throw HTTP-CLIENT-URL-ERROR the URL has no host or an unsupported scheme
        @throw HTTP-CLIENT-BODY-ERROR the body is not a string or binary value

        @note Since ConnectionProvider 3.1 the request is made with \c HttpClientIo instead of
        @ref Qore::HTTPClient "HTTPClient"
    */
    constructor(string base_url, string method, string path, auto body, *hash<auto> http_opts,
            *hash<auto> headers, *Logger::LoggerInterface logger)
            : HttpClientIoPollOperation(HttpConnection::getAsyncConnectionManager(http_opts, logger),
                HttpClientIoPollOperation::getRequestUrl(base_url, path), method,
                HttpClientIoPollOperation::getRequestHeaders(base_url, http_opts, headers),
                HttpClientIoPollOperation::getRequestBody(method, body),
                HttpClientIoPollOperation::getOptions("http-request", http_opts), logger) {
        if (logger) {
            logger.logArgs("INFO", sprintf("HttpRequest async start: method=%s path=%y url=%y",
                self.method, self.path, base_url));
        }
    }
}

#! base class for HTTP-based connections that need their URLs rewritten to create the real connection object
/** Example of the usage is in \c RestConnection where the scheme \c rest:// is internally used as \c http://
*/
public class HttpBasedConnection inherits AbstractConnectionWithInfo {
    #! Creates the connection object
    /** @param name the name of the connection
        @param desc connection description
        @param url connection URL (potentially with password info)
        @param attributes various attributes. See below
        @param options connection options
        @param logger optional logger interface for the connection

        See @ref ConnectionProvider::AbstractConnection::constructor() for \c attributes and \c options reference.

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option
    */
    constructor(string name, string desc, string url, hash<auto> attributes, hash<auto> options, *Logger logger)
            : AbstractConnectionWithInfo(name, desc, url, attributes, options, logger) {
    }

    #! Creates the connection object
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

    #! returns @ref True, since this connection type supports the %Qore @ref Qore::Socket "Socket-based" polling API
    /** @return @ref True

        @since ConnectionProvider 1.7
    */
    bool supportsPollingApi() {
        return True;
    }

    #! Called to start the connection if the connection supports the polling API
    /** If the \c ping_method and \c ping_path options are set, an @ref HttpPingPollOperation is returned that sends
        a ping request with non-blocking I/O using \c HttpClientIo (supporting HTTP/1.1, HTTP/2, and HTTP/3).

        Otherwise the connection is checked by the \c startPollConnect() method of the object returned by
        @ref getPollImpl() (for example @ref Qore::HTTPClient::startPollConnect() "HTTPClient::startPollConnect()"),
        or, if the object has no such method, with an @ref HttpConnectPollOperation, which establishes a connection
        with \c HttpClientIo.

        @param logger an optional logger for the poll operation

        @return a socket poll operation object that will allow the connection goal to be reached with polling

        @see supportsPollingApi()

        @since ConnectionProvider 1.7

        @note Before ConnectionProvider 3.1, connections whose client object has no \c startPollConnect() method
        raised an error here
    */
    Qore::AbstractPollOperation startPollConnect(*Logger::LoggerInterface logger) {
        if (opts.ping_method && opts.ping_path) {
            return new HttpPingPollOperation(getPingBaseUrl(), opts.ping_method, opts.ping_path, getPingHttpOpts(),
                getPingHeaders(), getPingBody(opts.ping_method), logger);
        }
        object client = getPollImpl();
        if (client.hasCallableMethod("startPollConnect")) {
            return client.startPollConnect();
        }
        # the connection's client object does not support the polling API
        return new HttpConnectPollOperation(getPingBaseUrl(), getPingHttpOpts(), logger);
    }

    #! Returns @ref True since HTTP-based connections support the async request API
    /** @return @ref True

        @since ConnectionProvider 3.0
    */
    bool supportsAsyncRequestApi() {
        return True;
    }

    #! Starts a non-blocking HTTP request on the connection
    /** @param method the HTTP method (GET, POST, PUT, DELETE, PATCH, etc.)
        @param path the request path
        @param body the request body
        @param headers optional request headers
        @param logger an optional logger interface

        @return a poll operation for the async request

        @since ConnectionProvider 3.0
    */
    Qore::AbstractPollOperation startPollRequest(string method, string path, auto body,
            *hash<auto> headers, *Logger::LoggerInterface logger) {
        return new HttpRequestPollOperation(real_url, method, path, body, getPingHttpOpts(),
            getPingHeaders() + headers, logger);
    }

    #! Called by the constructor to set the real URL
    /** @param url the new URL

        sets the real HTTP URL for HTTP-based connections
    */
    private setRealUrl(*string url) {
        if (exists url) {
            real_url = getRealUrl(url, getConnectionSchemeInfo(), getType());
        }
    }

    #! Returns the base URL for ping operations
    private string getPingBaseUrl() {
        return real_url;
    }

    #! Returns HTTP options for ping operations
    private hash<auto> getPingHttpOpts() {
        return opts{keys HttpConnection::ConnectionScheme.options} + {"url": getPingBaseUrl()};
    }

    #! Returns HTTP headers for ping operations
    private hash<auto> getPingHeaders() {
        hash<auto> headers;
        headers += opts.headers;
        headers += opts.ping_headers;
        return headers;
    }

    #! Returns the ping body based on the HTTP method
    private auto getPingBody(string method) {
        return method == "GET" ? NOTHING : opts.ping_body;
    }
}
} # ConnectionProvider namespace
