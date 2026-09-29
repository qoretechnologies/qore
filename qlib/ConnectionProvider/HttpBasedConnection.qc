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

    The request is made with @ref HttpClientIo::HttpClientRequestPollOperation "HttpClientRequestPollOperation",
    which follows redirects and uses the cookie jar of the connection manager like
    @ref HttpClientIo::HttpClientConnectionManager::request() "HttpClientConnectionManager::request()".

    @since ConnectionProvider 3.1
*/
public class HttpClientIoPollOperation inherits HttpClientIo::HttpClientRequestPollOperation {
    private {
        #! An optional logger
        *Logger::LoggerInterface logger;
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
            *hash<auto> headers, *data body, *hash<HttpClientIoPollOptions> opts, *Logger::LoggerInterface logger)
            : HttpClientIo::HttpClientRequestPollOperation(mgr, HttpClientIoPollOperation::checkUrl(url), method,
                headers, body, HttpClientIoPollOperation::getRequestOptions(opts ?? <HttpClientIoPollOptions>{})) {
        self.logger = logger;
    }

    #! Returns the state: \c "connecting", \c "receiving", \c "completed", or \c "aborted"
    string getState() {
        string state = HttpClientRequestPollOperation::getState();
        return state == HttpClientRequestPollOperation::ST_COMPLETE ? "completed" : state;
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
        return HttpUtil::HttpRedirectFollower::encodeTarget(target);
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

    #! Returns the URL of a request after checking that it has a host and an \c http or \c https scheme
    /** @throw HTTP-CLIENT-URL-ERROR the URL has no host or an unsupported scheme
    */
    private static string checkUrl(string url) {
        HttpClientIoPollOperation::splitUrl(url);
        return url;
    }

    #! Returns the options of the HttpClientIo request for the given options
    private static hash<HttpClientRequestPollOptions> getRequestOptions(hash<HttpClientIoPollOptions> opts) {
        hash<HttpClientRequestPollOptions> rv = <HttpClientRequestPollOptions>{
            "goal": opts.goal,
            "connect_only": opts.connect_only,
            "headers_only": opts.headers_only,
            "body_streaming": opts.body_streaming,
            "max_redirects": opts.max_redirects,
            "redirect_passthru": opts.redirect_passthru,
            "timeout": opts.timeout_ms,
            "assume_encoding": opts.assume_encoding,
        };
        if (exists opts.max_connect_retries) {
            rv.max_connect_retries = opts.max_connect_retries;
        }
        return rv;
    }

    #! Reports errors with the error codes documented for HTTP clients; see @ref rethrowError()
    private hash<ExceptionInfo> translateError(hash<ExceptionInfo> ex) {
        if (ex.err == "HTTPCLIENT-TIMEOUT") {
            ex.err = "SOCKET-TIMEOUT";
            return ex;
        }
        if (ex.err == "HTTPCLIENT-CONNECT-ERROR" && ex.arg.typeCode() == NT_HASH && ex.arg.err.typeCode() == NT_STRING
            && ex.arg.err =~ /^SOCKET-/) {
            return <ExceptionInfo>{
                "err": ex.arg.err,
                "desc": ex.arg.desc ?? ex.desc,
                "arg": ex.arg.arg,
            };
        }
        return ex;
    }

    #! Returns the output of a connect-only operation after validating it
    private auto buildConnectOutput(string url, *string protocol) {
        hash<auto> rv = {
            "url": url,
        };
        if (protocol) {
            rv.protocol = protocol;
        }
        validateResponse(rv);
        return rv;
    }

    #! Returns the output for a response in the format of @ref Qore::HTTPClient "HTTPClient" poll operations
    private auto buildOutput(hash<HttpClientResponseInfo> resp) {
        int code = resp.status_code;
        string http_version = resp.http_version ?? HttpClientIoPollOperation::getHttpVersion(resp.protocol);
        string status_message = resp.status_message ?? "";
        hash<auto> hdrs = resp.headers;
        if (logger && resp.redirects) {
            foreach hash<HttpRedirectInfo> r in (resp.redirects) {
                logger.debug("%s: followed redirect %d from %y to %y", opts.goal, r.status_code, r.request_url,
                    r.target_url);
            }
        }
        hash<auto> info = {
            "response-headers": hdrs + {
                "status_code": code,
                "status_message": status_message,
                "http_version": http_version,
            },
            "response-headers-raw": resp.headers_raw ?? hdrs,
            "response-uri": trim(sprintf("HTTP/%s %d %s", http_version, code, status_message)),
            "request-uri": sprintf("%s %s HTTP/%s", rf.getMethod(), rf.getPath(), http_version),
            "effective-url": resp.effective_url,
        };
        if (resp.content_type) {
            info."body-content-type" = mime_get_base_type(resp.content_type);
            *string charset = (resp.content_type =~ x/charset=["']?([^\s;,"']+)/i)[0];
            if (charset) {
                info.charset = charset;
            }
        }
        if (resp.redirects) {
            info.redirects = resp.redirects;
        }
        hash<auto> rv = {
            "code": code,
            "status_code": code,
            "status_message": status_message,
            "http_version": http_version,
            "protocol": resp.protocol,
            "headers": hdrs,
            "effective_url": resp.effective_url,
            "redirects": resp.redirects ?? (),
            "info": info,
            "request-headers": rf.getHeaders(),
        };
        if (exists resp.body) {
            rv."response-body" = resp.body;
            rv.info."response-body" = resp.body;
        }
        validateResponse(rv);
        return rv;
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
        return state == "completed" ? "connected" : state;
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
