# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpClientRequestPollOperation.qc a non-blocking HTTP request that follows redirects

/** HttpClientRequestPollOperation.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Main namespace
public namespace HttpClientIo {
#! Options for @ref HttpClientRequestPollOperation
/** @since HttpClientIo 1.0
*/
public hashdecl HttpClientRequestPollOptions {
    #! The goal of the operation as returned by @ref HttpClientRequestPollOperation::getGoal()
    string goal = "http-request";

    #! If @ref True, only a connection is established and no request is sent
    /** The output is then a hash with the \c url (the origin) and \c protocol keys
    */
    bool connect_only = False;

    #! If @ref True, the operation completes once the final response headers have been received
    /** The response body is not read and the stream is canceled; use for responses whose body is not needed
    */
    bool headers_only = False;

    #! If @ref True, the operation completes once the final response headers have been received, and the stream is
    #! handed over to the caller
    /** The output has no body; the body received with the headers is returned by
        @ref HttpClientRequestPollOperation::getInitialBody(), and the rest of the body is read from the stream
        returned by @ref HttpClientRequestPollOperation::getStream()
    */
    bool keep_stream = False;

    #! If @ref True, the request body stays open for bidirectional streaming
    /** Required for an extended \c CONNECT request (RFC 8441 / RFC 9220) with \c headers_only
    */
    bool body_streaming = False;

    #! The time to wait for the response, including all redirects; 0 means the \c request_timeout option of the
    #! connection manager
    /** The \c io_timeout option of the connection manager applies as well
    */
    timeout timeout = 0;

    #! The number of failed connection attempts retried; the \c max_request_retries option of the connection
    #! manager if not set
    *int max_connect_retries;

    #! The maximum number of redirects to follow; the \c max_redirects option of the connection manager if not set
    *int max_redirects;

    #! If @ref True, redirect responses are returned instead of followed; the \c redirect_passthru option of the
    #! connection manager if not set
    *bool redirect_passthru;

    #! If @ref False, a body with a content coding (\c Content-Encoding) is returned as received; if not set, the
    #! body is decoded unless the \c encoding_passthru option of the connection manager is set
    *bool decode_content;

    #! If @ref False, a text body is returned as binary data after its content encoding is decoded
    bool decode_text = True;

    #! The encoding assumed for text response bodies whose encoding is not determined otherwise; the
    #! \c assume_encoding option of the connection manager if not set
    *string assume_encoding;

    #! Credentials for HTTP authentication; if not set, any user information in the URL
    /** The credentials are sent only to the origin of the URL; a 401 challenge is answered once for each request
        sent.  See @ref HttpUtil::HttpAuthenticator "HttpAuthenticator".
    */
    *hash<HttpClientCredentials> credentials;

    #! The cookie jar that supplies and stores the cookies of the request; if not set, the cookie jar of the
    #! connection manager
    /** @since HttpClientIo 1.0
    */
    *CookieJar cookie_jar;

    #! If @ref False, no cookies are sent or stored for the request
    /** @since HttpClientIo 1.0
    */
    bool cookies = True;

    #! The maximum size in bytes of the response body, as received and after its content encoding is decoded; if
    #! not set, the \c max_response_body_size option of the connection manager; 0 means no limit
    /** @since HttpClientIo 1.0
    */
    *int max_response_body_size;
}

#! A non-blocking HTTP request with an @ref HttpClientConnectionManager that follows redirects
/** Supports HTTP/1.1, HTTP/2, and HTTP/3 as negotiated by the connection manager.  No blocking I/O is performed on
    the calling thread: connection readiness is waited for with the connection manager's event notifiers, and the
    response is read from the stream as it arrives, so the operation can be driven by
    @ref Qore::Socket::poll() "Socket::poll()" in a loop (see @ref Qore::AbstractPollOperation), or started in the
    background with @ref startAsync(), which returns a Future for the response.

    The request follows redirects like @ref HttpClientConnectionManager::request(), with the rules of
    @ref HttpUtil::HttpRedirectFollower "HttpRedirectFollower": the cookie jar of the connection manager supplies the
    cookies for each target and stores the cookies of each response, and a redirect to another origin is sent on a
    connection to that origin.

    The output is a @ref HttpClientResponseInfo hash with the \c effective_url and \c redirects keys set; the body is
    decoded according to its content encoding and character set by the rules of
    @ref HttpClientStreamHandle::normalizeResponseBody().  A body larger than the \c max_response_body_size option of
    the connection manager fails the request with \c HTTP-CLIENT-RESPONSE-BODY-TOO-LARGE as soon as the limit is
    exceeded.

    Subclasses can build another output from the response by reimplementing @ref buildOutput(), and report errors
    with other error codes by reimplementing @ref translateError(); a subclass can change the \c method,
    \c headers, \c body, and \c opts members in its constructor, before the operation is started.

    @par Example:
    @code{.py}
HttpClientConnectionManager mgr();
HttpClientRequestPollOperation op(mgr, "https://example.com/docs");
while (!op.goalReached()) {
    *hash<SocketPollInfo> info = op.continuePoll();
    if (info) {
        Socket::poll((info,), 30000);
    }
}
hash<HttpClientResponseInfo> resp = op.getOutput();
    @endcode

    @since HttpClientIo 1.0
*/
public class HttpClientRequestPollOperation inherits Qore::AbstractPollOperation<auto> {
    public {
        #! Waiting for a connection
        const ST_CONNECTING = "connecting";
        #! Waiting for the response
        const ST_RECEIVING = "receiving";
        #! Complete
        const ST_COMPLETE = "complete";
        #! Aborted or failed
        const ST_ABORTED = "aborted";

        #! The maximum time in milliseconds between two wakes of the operation while waiting for an event
        const PollTimeoutMs = 1000;
    }

    private {
        #! The connection manager
        HttpClientConnectionManager mgr;
        #! The options
        hash<HttpClientRequestPollOptions> opts;
        #! The URL of the request
        string url;
        #! The method of the first request; a subclass can change it before the operation is started
        string method;
        #! The request target of the first request
        string path;
        #! The headers of the first request; a subclass can change them before the operation is started
        hash<auto> headers = {};
        #! The body of the first request; a subclass can change it before the operation is started
        *data body;
        #! Follows redirects and holds the next request; created with the first request when the operation starts
        *HttpRedirectFollower rf;
        #! Answers the authentication challenges of the origin of the request, if the request has credentials
        *HttpAuthenticator auth;
        #! The authentication state of the current request; see HttpClientConnectionManager::prepareRequestHeaders()
        hash<auto> auth_state;
        #! True if the current response is an authentication challenge that is answered
        bool auth_retry = False;
        #! True once a proxy authentication challenge for a CONNECT request was answered
        bool proxy_connect_answered = False;
        #! The deadline for the response, if any
        *date deadline;
        #! The time the current request was sent, for the inactivity timeout
        *date sent;

        #! The current state
        string state = ST_CONNECTING;
        #! The connection handed back by the last acquisition step
        *HttpClientConnection tracked;
        #! Failed connection attempts
        int failures = 0;
        #! The connection readiness notifier
        *EventNotifier conn_notifier;
        #! True if @ref conn_notifier was signaled by this object
        bool self_signaled = False;
        #! The response notifier
        EventNotifier io_notifier();
        #! The current stream
        *HttpClientStreamHandle stream;
        #! The response headers of the current request
        *hash<auto> response;
        #! The body received for the current request
        list<binary> body_chunks = ();
        #! The size of the body received for the current request
        int body_size = 0;
        #! True if the current response is a redirect to follow
        bool redirecting = False;
        #! True if the whole response was received when the stream was handed over
        bool stream_complete = False;

        #! The output
        auto output;
        #! True once the output is available
        bool done = False;

        #! The Promise resolved with the output when the operation was started with @ref startAsync()
        *Promise promise;
        #! True once @ref promise was resolved or rejected
        bool promise_done = False;

        #! Serializes the caller, async I/O controller workers, and cancellation
        Mutex m();
    }

    #! Creates the operation; no I/O is performed until @ref continuePoll() or @ref startAsync() is called
    /** @param mgr the connection manager to use; the operation does not close it
        @param url the absolute URL of the request, including any path and query; user information is not used
        @param method the HTTP method
        @param headers request headers
        @param body the request body, if any
        @param opts options for the operation

        @throw HTTPCLIENT-URL-ERROR the URL has no host
        @throw HTTP-CLIENT-UNKNOWN-PROTOCOL the URL has a scheme other than \c http or \c https
        @throw HTTPCLIENT-REQUEST-BODY-ERROR a GET request specifies a body or enables request-body streaming
    */
    constructor(HttpClientConnectionManager mgr, string url, string method = "GET", *hash<auto> headers,
            *data body, *hash<HttpClientRequestPollOptions> opts) {
        self.mgr = mgr;
        self.opts = opts ?? <HttpClientRequestPollOptions>{};
        if (!mgr.getAllowGetBody()) {
            check_http_request_body(method, headers, body, self.opts.body_streaming);
        }
        *list<*string> parts = (url =~ x/^([A-Za-z][A-Za-z0-9+.-]*):\/\/([^\/?#]*)([^#]*)/s);
        if (!parts || !parts[1].val()) {
            throw "HTTPCLIENT-URL-ERROR", sprintf("URL %y has no host", HttpClientRequestPollOperation::safeUrl(url));
        }
        string scheme = parts[0].lwr();
        if (scheme != "http" && scheme != "https") {
            throw "HTTP-CLIENT-UNKNOWN-PROTOCOL", sprintf("URL %y has unsupported scheme %y; expecting \"http\" or "
                "\"https\"", HttpClientRequestPollOperation::safeUrl(url), scheme);
        }
        try {
            HttpRedirectFollower::parseOrigin(url);
        } catch (hash<ExceptionInfo> ex) {
            throw "HTTPCLIENT-URL-ERROR", ex.desc, ex.arg;
        }
        self.url = url;
        self.method = method;
        path = HttpRedirectFollower::encodeTarget(parts[2] ?? "");
        if (path !~ /^\//) {
            path = "/" + path;
        }
        if (headers) {
            self.headers = headers;
        }
        self.body = body;
        # 0 means no overall deadline; see the io_timeout option of the connection manager
        timeout to = self.opts.timeout > 0 ? self.opts.timeout : mgr.getRequestTimeout();
        if (to > 0) {
            deadline = now_us() + milliseconds(to);
        }
    }

    #! Returns the goal
    string getGoal() {
        return opts.goal;
    }

    #! Returns the state: \c "connecting", \c "receiving", \c "complete", or \c "aborted"
    string getState() {
        AutoLock al(m);
        return state;
    }

    #! Returns @ref True once the output is available
    bool goalReached() {
        AutoLock al(m);
        return done;
    }

    #! Returns the output, or @ref nothing if the operation is not complete
    /** @return a @ref HttpClientResponseInfo hash, unless a subclass builds another output; with the
        \c connect_only option, a hash with the \c url and \c protocol keys
    */
    auto getOutput() {
        AutoLock al(m);
        return output;
    }

    #! Returns the connection manager
    HttpClientConnectionManager getConnectionManager() {
        return mgr;
    }

    #! Returns the stream of the response with the \c keep_stream option, once the output is available
    /** The caller reads the rest of the response body from the stream, and cancels it if the body is not read
    */
    *HttpClientStreamHandle getStream() {
        AutoLock al(m);
        return opts.keep_stream && done ? stream : NOTHING;
    }

    #! Returns the body received with the response headers with the \c keep_stream option
    list<binary> getInitialBody() {
        AutoLock al(m);
        return opts.keep_stream ? body_chunks : ();
    }

    #! Returns @ref True if the whole response was received when the stream was handed over with the
    #! \c keep_stream option
    bool isStreamComplete() {
        AutoLock al(m);
        return stream_complete;
    }

    #! Returns the absolute URL of the current request, without user information
    string getRequestUrl() {
        AutoLock al(m);
        return getFollower().getRequestUrl();
    }

    #! Continues the operation
    /** @return a poll hash while the operation needs more I/O, @ref nothing once the output is available

        @throw HTTPCLIENT-TIMEOUT the response was not received in time
        @throw HTTPCLIENT-CONNECT-ERROR the connection could not be established
        @throw HTTP-CLIENT-MAXIMUM-REDIRECTS-EXCEEDED the maximum number of redirects was exceeded
        @throw HTTP-CLIENT-REDIRECT-ERROR a redirect cannot be followed
        @throw HTTP-CLIENT-UNKNOWN-PROTOCOL a redirect location has an unsupported scheme
        @throw HTTP-CLIENT-RESPONSE-BODY-TOO-LARGE the response body exceeds the \c max_response_body_size option
        of the connection manager

        @note Other errors of the transport are raised as well; subclasses can translate errors with
        @ref translateError().  When the operation was started with @ref startAsync(), errors reject the Future
        instead of being raised.
    */
    *hash<SocketPollInfo> continuePoll() {
        AutoLock al(m);
        try {
            return continuePollIntern();
        } catch (hash<ExceptionInfo> ex) {
            cancelStream();
            state = ST_ABORTED;
            if (promise) {
                rejectIntern(translateError(ex));
                return;
            }
            raiseError(ex);
        }
    }

    #! Starts the operation in the background with the async I/O controller of the connection manager
    /** @return a Future for the output of the operation; canceling the Future cancels the request

        @par Example:
        @code{.py}
HttpCancellableFuture f = new HttpClientRequestPollOperation(mgr, "https://example.com/data.json").startAsync();
hash<HttpClientResponseInfo> resp = f.get(30s);
        @endcode
    */
    HttpCancellableFuture startAsync() {
        Future f;
        {
            AutoLock al(m);
            if (promise) {
                throw "HTTPCLIENT-STATE-ERROR", sprintf("operation %y was already started", opts.goal);
            }
            promise = new Promise();
            f = promise.getFuture();
        }
        *hash<SocketPollInfo> info = continuePoll();
        if (info) {
            mgr.getController().submit(<SocketPollOperationInfo>{
                "sock": info.socket,
                "spop": self,
                # the operation raises its own timeout errors first; a negative value means no timeout
                "to": deadline ? max(1, (deadline - now_us()).durationMilliseconds()) + PollTimeoutMs : -1,
                "owner": "httpclient-request:" + self.uniqueHash(),
                "has_on_complete": True,
            });
        }
        return new HttpCancellableFuture(f, \cancel());
    }

    #! Cancels the operation
    /** A Future returned by @ref startAsync() is rejected with \c HTTPCLIENT-CANCELLED

        @return @ref True if the operation was canceled, @ref False if it was already complete
    */
    bool cancel() {
        AutoLock al(m);
        if (done || state == ST_ABORTED) {
            return False;
        }
        cancelStream();
        state = ST_ABORTED;
        rejectIntern(<ExceptionInfo>{
            "err": "HTTPCLIENT-CANCELLED",
            "desc": sprintf("request %y was canceled", opts.goal),
        });
        return True;
    }

    #! Aborts the operation
    abort() {
        AutoLock al(m);
        cancelStream();
        if (!done) {
            state = ST_ABORTED;
        }
    }

    #! Called by an async I/O controller when the operation completes, fails, times out, or is canceled
    nothing onComplete(hash<SocketPollResultInfo> result) {
        AutoLock al(m);
        if (done) {
            return;
        }
        cancelStream();
        state = ST_ABORTED;
        if (result.canceled) {
            rejectIntern(<ExceptionInfo>{
                "err": "HTTPCLIENT-CANCELLED",
                "desc": sprintf("request %y was canceled", opts.goal),
            });
        } else if (result.ex) {
            rejectIntern(result.ex);
        } else {
            rejectIntern(translateError(<ExceptionInfo>{
                "err": "HTTPCLIENT-TIMEOUT",
                "desc": sprintf("no response received for %s %s within the timeout", getFollower().getMethod(),
                    getFollower().getRequestUrl()),
            }));
        }
    }

    #! Returns the output of the operation for a response; called with the operation's lock held
    /** The base class implementation returns \a resp

        @param resp the response, with the body decoded unless the \c headers_only or \c keep_stream option is set

        @return the output of the operation

        @note an exception raised here fails the operation
    */
    private auto buildOutput(hash<HttpClientResponseInfo> resp) {
        return resp;
    }

    #! Returns the output of a \c connect_only operation; called with the operation's lock held
    /** The base class implementation returns a hash with the \c url and \c protocol keys

        @param url the origin of the connection
        @param protocol the protocol of the connection

        @return the output of the operation
    */
    private auto buildConnectOutput(string url, *string protocol) {
        return {
            "url": url,
            "protocol": protocol,
        };
    }

    #! Raises an error of the operation
    /** The base class implementation raises the error returned by @ref translateError()

        @param ex the error
    */
    private raiseError(hash<ExceptionInfo> ex) {
        hash<ExceptionInfo> err = translateError(ex);
        throw err.err, err.desc, err.arg;
    }

    #! Returns the error reported for an error of the operation
    /** The base class implementation returns the error unchanged; subclasses can report errors with other error
        codes

        @param ex the error

        @return the error to report
    */
    private hash<ExceptionInfo> translateError(hash<ExceptionInfo> ex) {
        return ex;
    }

    #! Returns the redirect follower, creating it with the first request if necessary
    private HttpRedirectFollower getFollower() {
        if (!rf) {
            auth = mgr.getRequestAuthenticator(url, opts.credentials);
            auth_state = mgr.newRequestState(<HttpClientRequestOptions>{
                "cookie_jar": opts.cookie_jar,
                "cookies": opts.cookies,
            });
            rf = new HttpRedirectFollower(url, path, method, headers, body,
                opts.max_redirects ?? mgr.getMaxRedirects(), opts.redirect_passthru ?? mgr.getRedirectPassthru());
        }
        return rf;
    }

    #! Continues the operation; called with the lock held
    private *hash<SocketPollInfo> continuePollIntern() {
        getFollower();
        while (True) {
            switch (state) {
                case ST_COMPLETE:
                case ST_ABORTED:
                    return;

                case ST_CONNECTING: {
                    if (conn_notifier) {
                        # one acquisition step per readiness notification: this method is also entered when no
                        # notification arrived, and a step taken then would open another connection while the
                        # first one is still connecting
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
                    string origin = rf.getOrigin();
                    *EventNotifier notifier = mgr.acquireConnectionAsync(origin, \tracked);
                    if (!notifier && tracked && HttpClientRequestPollOperation::failed(tracked)
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
                        complete(buildConnectOutput(origin, tracked ? tracked.getProtocol() : NOTHING));
                        remove tracked;
                        return;
                    }
                    string target = rf.getPath();
                    try {
                        stream = mgr.acquireStream(origin);
                        # the cookie jar supplies the cookies for each target
                        # the body is decoded unless the stream is handed over
                        stream.startStreaming(rf.getMethod(), target, mgr.prepareRequestHeaders(rf, auth,
                            \auth_state, !opts.keep_stream), opts.body_streaming, rf.getBody(), io_notifier);
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
                    sent = now_us();
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
                            bool end = stream.isDone();
                            if (response && !processHeaders(end)) {
                                return;
                            }
                            if (end) {
                                # the end of the stream was delivered together with the last body chunk
                                if (processEnd()) {
                                    return;
                                }
                                break;
                            }
                            checkDeadline();
                            return getPollInfo(io_notifier);
                        }
                        bool end = False;
                        if (data.typeCode() == NT_HASH) {
                            if (data.error) {
                                throw data.error.err ?? "HTTPCLIENT-RECEIVE-ERROR",
                                    data.error.desc ?? sprintf("%y", data.error), data.error;
                            }
                            if (!response && (data.status_code || data.headers)) {
                                response = data;
                            }
                            if (exists data.body) {
                                addBody(data.body);
                            }
                            end = data.end_stream.toBool();
                        } else {
                            addBody(data);
                        }
                        if (!response) {
                            response = stream.getResponse();
                        }
                        if (response && !processHeaders(end)) {
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

    #! Adds a chunk of the response body
    /** The body of a redirect response is discarded

        @throw HTTP-CLIENT-RESPONSE-BODY-TOO-LARGE the body exceeds the \c max_response_body_size option of the
        connection manager
    */
    private addBody(data chunk) {
        if (redirecting) {
            return;
        }
        binary bin = chunk.typeCode() == NT_BINARY ? chunk : binary(chunk);
        body_size += bin.size();
        # a body that is kept in memory whole is limited as it arrives
        int max_size = opts.max_response_body_size ?? mgr.getMaxResponseBodySize();
        if (max_size > 0 && !opts.keep_stream && !opts.headers_only && body_size > max_size) {
            throw "HTTP-CLIENT-RESPONSE-BODY-TOO-LARGE", sprintf("the response body from %y exceeds the maximum "
                "response body size of %d bytes", rf.getRequestUrl(), max_size);
        }
        body_chunks += bin;
    }

    #! Processes the response headers once; returns @ref False if the output is available
    /** @param end @ref True if the whole response was received
    */
    private bool processHeaders(bool end) {
        if (redirecting || response.processed) {
            return True;
        }
        response.processed = True;
        # cookies set by a response are stored before a redirect is followed
        mgr.storeStateCookies(auth_state, rf, response.headers);
        # an authentication challenge is answered once for each request; the body of the response is discarded
        if (mgr.processAuthChallenge(response.status_code, response.headers, auth, \auth_state)) {
            redirecting = True;
            auth_retry = True;
            body_chunks = ();
            body_size = 0;
            return True;
        }
        if (rf.isFollowed(response.status_code)) {
            # the body of the redirect response is discarded
            redirecting = True;
            body_chunks = ();
            body_size = 0;
            return True;
        }
        if (opts.headers_only) {
            complete(buildOutput(getResponseInfo()));
            # the response body is not read
            cancelStream(True);
            return False;
        }
        if (opts.keep_stream) {
            stream_complete = end;
            complete(buildOutput(getResponseInfo()));
            return False;
        }
        return True;
    }

    #! Processes the end of a response; returns @ref True if the output is available, @ref False if a redirect is
    #! followed
    private bool processEnd() {
        if (redirecting) {
            if (auth_retry) {
                resendRequest();
            } else {
                followRedirect();
            }
            return False;
        }
        hash<HttpClientResponseInfo> resp = getResponseInfo();
        binary bin;
        map bin += $1, body_chunks;
        if (bin) {
            if (opts.decode_content ?? !mgr.getEncodingPassthru()) {
                resp.body = HttpClientStreamHandle::normalizeResponseBody({
                    "headers": resp.headers,
                    "body": bin,
                }, opts.max_response_body_size ?? mgr.getMaxResponseBodySize(),
                    opts.assume_encoding ?? mgr.getAssumeEncoding(),
                    opts.decode_text).body;
            } else {
                resp.body = bin;
            }
        }
        complete(buildOutput(resp));
        return True;
    }

    #! Makes the output available
    private complete(auto output) {
        self.output = output;
        done = True;
        state = ST_COMPLETE;
        if (promise && !promise_done) {
            promise_done = True;
            promise.set(output);
        }
    }

    #! Rejects the Future of an operation started with @ref startAsync()
    private rejectIntern(hash<ExceptionInfo> ex) {
        if (promise && !promise_done) {
            promise_done = True;
            promise.setError(ex.err, ex.desc, ex.arg);
        }
    }

    #! Returns the response information without the body
    private hash<HttpClientResponseInfo> getResponseInfo() {
        hash<auto> hdrs = response.headers ?? {};
        auto ct = hdrs."content-type";
        hash<HttpClientResponseInfo> rv({
            "status_code": response.status_code ?? 0,
            "status_message": response.status_message,
            "http_version": response.http_version,
            "headers": hdrs,
            "headers_raw": response.headers_raw,
            "content_type": ct.typeCode() == NT_LIST ? ct[0] : ct,
            "stream_id": stream.getStreamId() ?? 0,
            "protocol": response.protocol ?? stream.getConnection().getProtocol(),
            "effective_url": rf.getRequestUrl(),
        });
        list<hash<HttpRedirectInfo>> redirects = rf.getRedirects();
        if (redirects) {
            rv.redirects = redirects;
        }
        return rv;
    }

    #! Follows a redirect
    /** @throw HTTP-CLIENT-MAXIMUM-REDIRECTS-EXCEEDED the maximum number of redirects was exceeded
        @throw HTTP-CLIENT-REDIRECT-ERROR the redirect cannot be followed
        @throw HTTP-CLIENT-UNKNOWN-PROTOCOL the redirect location has an unsupported scheme
    */
    private followRedirect() {
        rf.follow(response.status_code, response.status_message, response.headers ?? {});
        resendRequest();
    }

    #! Sends the current request of the redirect state again
    private resendRequest() {
        remove stream;
        remove response;
        body_chunks = ();
        body_size = 0;
        redirecting = False;
        auth_retry = False;
        failures = 0;
        state = ST_CONNECTING;
    }

    #! Raises \c HTTPCLIENT-TIMEOUT if the deadline has passed, or if no data was received for the request for the
    #! \c io_timeout option of the connection manager
    private checkDeadline() {
        if (deadline && now_us() >= deadline) {
            throw "HTTPCLIENT-TIMEOUT", sprintf("timed out waiting for the %s from %s", state == ST_CONNECTING
                ? "connection" : "response", opts.connect_only ? rf.getOrigin() : rf.getRequestUrl());
        }
        timeout io_timeout = mgr.getIoTimeout();
        if (io_timeout > 0 && state == ST_RECEIVING && stream
                && stream.getInactiveTime(sent) >= io_timeout * 1000) {
            throw "HTTPCLIENT-TIMEOUT", sprintf("no data received for %d ms from %s (io_timeout)", io_timeout,
                rf.getRequestUrl());
        }
    }

    #! Returns @ref True while the tracked connection is still connecting
    private bool connectionPending() {
        return exists tracked && !HttpClientRequestPollOperation::failed(tracked) && !tracked.isReady()
            && !mgr.isUnresolvedRaceLeg(tracked);
    }

    #! Counts the tracked connection as a failed attempt if it failed before it could be used
    /** @throw HTTPCLIENT-CONNECT-ERROR the connection attempts failed more often than allowed
    */
    private checkConnectionFailure() {
        *HttpClientConnection conn = remove tracked;
        if (!conn || !HttpClientRequestPollOperation::failed(conn) || mgr.isUnresolvedRaceLeg(conn)) {
            return;
        }
        # a proxy authentication challenge of a CONNECT request is answered once with a new connection
        if (!proxy_connect_answered
                && mgr.processProxyConnectFailure(rf.getOrigin(), conn.getPollOperation().getError())) {
            proxy_connect_answered = True;
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
    private static bool failed(HttpClientConnection conn) {
        return conn.isClosed() || conn.getPollOperation().hasError();
    }

    #! Cancels the current stream, unless it was handed over in the output
    /** @param force if @ref True, the stream is canceled even if the output is available
    */
    private cancelStream(*bool force) {
        if (stream && (force || !done)) {
            try {
                stream.cancel();
            } catch (hash<ExceptionInfo> ex) {
            }
        }
    }

    #! Returns poll information for the given notifier
    private hash<SocketPollInfo> getPollInfo(EventNotifier notifier) {
        int wait_ms = PollTimeoutMs;
        if (deadline) {
            wait_ms = min(wait_ms, (deadline - now_us()).durationMilliseconds());
        }
        return <SocketPollInfo>{
            "events": SOCK_POLLIN,
            "socket": notifier,
            "poll_timeout_ms": max(1, wait_ms),
        };
    }

    #! Returns a URL without user information, for error messages
    private static string safeUrl(string url) {
        return regex_subst(url, "^([^:/?#]+://)[^/?#]*@", "$1");
    }
}
}
