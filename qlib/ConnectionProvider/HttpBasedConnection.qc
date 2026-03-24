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
#! Poll operation for async HTTP ping requests
/** Uses only non-blocking I/O for HTTP ping requests

    @since ConnectionProvider 2.2
*/
public class HttpPingPollOperation inherits Qore::AbstractPollOperation {
    private {
        *AbstractPollOperation poller;
        *Logger::LoggerInterface logger;
        string err_code;
        string log_prefix;
        bool goal_reached = False;
        auto output;
        string state = "ping";
    }

    constructor(string base_url, string method, string path, *hash<auto> http_opts, *hash<auto> headers, auto body,
            *Logger::LoggerInterface logger, string err_code = "HTTP-PING-ERROR",
            string log_prefix = "HttpConnection") {
        self.logger = logger;
        self.err_code = err_code;
        self.log_prefix = log_prefix;

        string request_method = method ?? "GET";
        string request_path = path ?? "/";
        auto request_body = request_method == "GET" ? NOTHING : body;
        hash<auto> req_headers;
        req_headers += headers;

        HTTPClient hc((http_opts ?? {}) + {"url": base_url});
        if (request_path =~ /^http/) {
            hc.setURL(request_path);
            request_path = "";
        } else {
            hash<UrlInfo> u = parse_url(base_url);
            string base_path = u.path ?? "/";
            if (request_path[0] != "/") {
                if (base_path[-1] != "/") {
                    base_path += "/";
                }
                request_path = base_path + request_path;
            }
        }

        log("INFO", "%s async ping start: method=%s path=%y url=%y", log_prefix, request_method, request_path,
            base_url);
        poller = hc.startPollSendRecv(request_method, request_path, request_body, req_headers);
    }

    string getGoal() {
        return "ping";
    }

    string getState() {
        return state + (poller ? ("-" + poller.getState()) : NOTHING);
    }

    bool goalReached() {
        return goal_reached;
    }

    *hash<SocketPollInfo> continuePoll() {
        if (!poller) {
            return;
        }
        *hash<SocketPollInfo> info = poller.continuePoll();
        if (info) {
            return info;
        }
        hash<auto> resp = poller.getOutput();
        if (!exists resp.code || (resp.code / 100) != 2) {
            log("ERROR", "%s async ping failed: code=%y", log_prefix, resp.code);
            throw err_code, sprintf("ping failed with HTTP code %y", resp.code);
        }
        output = resp;
        log("INFO", "%s async ping complete: code=%d", log_prefix, resp.code);
        goal_reached = True;
    }

    auto getOutput() {
        return output;
    }

    abort() {
        if (poller) {
            poller.abort();
        }
    }

    private log(string level, string msg, ...) {
        if (logger) {
            logger.logArgs(level, msg, argv);
        }
    }
}

#! Poll operation for async HTTP requests
/** Uses non-blocking I/O for arbitrary HTTP requests, returning the full response.

    Unlike @ref HttpPingPollOperation which validates that the response is 2xx, this class
    returns the full response hash (code, headers, body) for any HTTP status code, allowing
    the caller to handle the response.

    @since ConnectionProvider 2.3
*/
public class HttpRequestPollOperation inherits Qore::AbstractPollOperation {
    private {
        #! The underlying poll operation
        *AbstractPollOperation poller;
        #! Optional logger
        *Logger::LoggerInterface logger;
        #! Goal reached flag
        bool goal_reached = False;
        #! Output from the operation
        auto output;
        #! Current state
        string state = "send-request";
    }

    #! Creates the poll operation for an async HTTP request
    /** @param base_url the HTTP URL
        @param method the HTTP method (GET, POST, etc.)
        @param path the request path
        @param body the request body
        @param http_opts HTTP client options
        @param headers additional headers
        @param logger optional logger
    */
    constructor(string base_url, string method, string path, auto body, *hash<auto> http_opts,
            *hash<auto> headers, *Logger::LoggerInterface logger) {
        self.logger = logger;

        hash<auto> req_headers;
        req_headers += headers;

        HTTPClient hc((http_opts ?? {}) + {"url": base_url});
        if (path =~ /^http/) {
            hc.setURL(path);
            path = "";
        } else {
            hash<UrlInfo> u = parse_url(base_url);
            string base_path = u.path ?? "/";
            if (path[0] != "/") {
                if (base_path[-1] != "/") {
                    base_path += "/";
                }
                path = base_path + path;
            }
        }

        log("INFO", "HttpRequest async start: method=%s path=%y url=%y", method, path, base_url);
        poller = hc.startPollSendRecv(method, path, method == "GET" ? NOTHING : body, req_headers);
    }

    string getGoal() {
        return "http-request";
    }

    string getState() {
        if (goal_reached) {
            return "complete";
        }
        return state + (poller ? ("-" + poller.getState()) : "");
    }

    bool goalReached() {
        return goal_reached;
    }

    *hash<SocketPollInfo> continuePoll() {
        if (!poller) {
            return;
        }
        *hash<SocketPollInfo> info = poller.continuePoll();
        if (info) {
            return info;
        }
        output = poller.getOutput();
        log("INFO", "HttpRequest async complete: code=%d", output.code);
        goal_reached = True;
    }

    auto getOutput() {
        return output;
    }

    abort() {
        if (poller) {
            poller.abort();
        }
    }

    private log(string level, string msg, ...) {
        if (logger) {
            logger.logArgs(level, msg, argv);
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
    /**
        @return a socket poll operation object that will allow the connection goal to be reached with polling

        @see supportsPollingApi()

        @since ConnectionProvider 1.7
    */
    Qore::AbstractPollOperation startPollConnect(*Logger::LoggerInterface logger) {
        if (opts.ping_method && opts.ping_path) {
            return new HttpPingPollOperation(getPingBaseUrl(), opts.ping_method, opts.ping_path, getPingHttpOpts(),
                getPingHeaders(), getPingBody(opts.ping_method), logger);
        }
        return getPollImpl().startPollConnect();
    }

    #! Returns @ref True since HTTP-based connections support the async request API
    /** @return @ref True

        @since ConnectionProvider 2.3
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

        @since ConnectionProvider 2.3
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
